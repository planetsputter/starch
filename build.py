#!/usr/bin/env python3

import glob, hashlib, multiprocessing, pathlib, re, shlex, subprocess, sys

# Returns a shell line representing the given list of arguments, or single string argument
def sh_esc(args):
	if isinstance(args, str): args = [args]
	return ' '.join(shlex.quote(x) for x in args)

# Returns the list of arguments described by the given shell line
def sh_unesc(line):
	return shlex.split(line)

# Returns the basename of the given path, optionally including the extension
def basename(path, withext=True):
	dotpos = path.rfind('.')
	slashpos = path.rfind('/')
	if slashpos >= dotpos or withext: dotpos = len(path)
	return path[slashpos + 1:dotpos]

# Changes the extension of the given path
def change_ext(path, ext):
	dotpos = path.rfind('.')
	slashpos = path.rfind('/')
	if slashpos >= dotpos: dotpos = len(path)
	return path[0:dotpos] + '.' + ext

# Returns the name of the library at the given path, such as would be passed with '-l' to the linker
def get_lib_name(path):
	dotpos = path.rfind('.')
	slashpos = path.rfind('/')
	if slashpos >= dotpos: dotpos = len(path)
	if path[slashpos + 1:].startswith('lib'): slashpos += 3
	return path[slashpos + 1:dotpos]

# Returns the string representing a single argument for a makefile rule
def mk_esc_rule(rule):
	return rule.replace('$', '$$').replace(' ', '\\ ')

# Returns the string representing a single argument escaped for the shell and then for a makefile recipe
def mk_esc_recipe(rec):
	return sh_esc(rec).replace('$', '$$')

# Writes a dependency rule to the given makefile, escaping the target and each dependency
def mf_write_rule(mf, target, deps):
	mf.write('%s:%s\n' % (mk_esc_rule(target), ' '.join([mk_esc_rule(d) for d in deps])))

# Process a build configuration file
def process_cfg(filename):
	# Make the build directory and object directory
	pathlib.Path('.build/obj').mkdir(exist_ok=True, parents=True)

	# Open the makefile
	mf = open('.build/makefile', 'w')

	# Generate phony targets, specify 'all' as first target.
	# Automatically running the 'clean' target when the makefile is parsed allows us
	# to avoid a race between 'clean' and other targets during a parallel build.
	mf.write(
		'.PHONY:clean all\n' +
		'all:\n' +
		'clean:\n' +
		'\t@echo rm -f .build/obj/*.o\n' +
		'ifneq ($(filter clean,$(MAKECMDGOALS)),)\n' +
		'    $(shell rm -f .build/obj/*.o)\n' +
		'endif\n')

	# Parameters from file
	ctx = {
		'target': None,
		'compiler': None,
		'src': None,
		'inc': None,
		'libs': None,
		'type': None,
		'cflags': None,
		'lflags': None
	}
	# Keep a list of targets
	targets = []

	def process_target():
		# Check context
		target = ctx['target']
		compiler = ctx['compiler']
		src = ctx['src']
		inc = ctx['inc']
		libs = ctx['libs']
		target_type = ctx['type']
		cflags = ctx['cflags']
		lflags = ctx['lflags']
		if not compiler: raise Exception('no compiler specified for target %s' % target)
		if not src: raise Exception('no src specified for target %s' % target)
		if not target_type: raise Exception('no target type specified for target %s' % target)
		if not target_type in ('bin', 'lib', 'so'):
			raise Exception('invalid target type %s for target %s' % (target_type, target))
		if cflags == None: cflags = []
		if lflags == None: lflags = []

		# Add inc directories to cflags list
		if inc:
			incdirs = []
			for d in inc:
				globs = glob.glob(d, recursive=True) # Allow globs
				if globs: incdirs += globs
				else: incdirs += [d] # Globs were not used
			cflags += ['-I%s' % d for d in incdirs] # Include directory in header search path

		# Generate dependencies for all source files
		sources = []
		for s in src:
			globs = glob.glob(s, recursive=True) # Allow globs
			if globs: sources += globs
			else: sources += [s] # Globs were not used
		objs = []
		for source in sources:
			# Compute the name for the object file
			obj = '.build/obj/%s-%s.o' % (basename(source, withext=False),
				hashlib.sha256(source.encode('utf-8')).hexdigest()[0:16])
			objs.append(obj)
			# Have the compiler generate the dependency rules
			args = (compiler, '-c', source, '-M', '-MM', '-MF', '-', '-MQ', obj, *cflags)
			result = subprocess.run(args, capture_output=True)
			if result.returncode:
				raise Exception('unable to generate dependency list for %s:\n%s\n%s'
					% (sh_esc(source), sh_esc(args), result.stderr.decode('utf-8')))
			# Write the dependencies to the makefile
			mf.write(result.stdout.decode('utf-8'))
			# Write the build recipe to the makefile
			mf.write('\t%s -c %s -o %s %s\n' % (mk_esc_recipe(compiler),
				mk_esc_recipe(source),
				mk_esc_recipe(obj),
				mk_esc_recipe(cflags)))

		# Listed libs are dependencies which also generate extra linker flags
		if libs:
			libraries = []
			for l in libs:
				globs = glob.glob(l, recursive=True) # Allow globs
				if globs: libraries += globs
				else: libraries += [l] # Globs were not used
			# Generate extra linker flags
			lflags += ['-L%s' % str(pathlib.Path(l).parents[0]) for l in libraries]
			lflags += ['-l%s' % get_lib_name(pathlib.Path(l).name) for l in libraries]
			# Write extra dependency rule
			mf_write_rule(mf, target, libraries)

		# Write the target object dependencies rule to the makefile
		mf_write_rule(mf, target, objs)

		# Write the recipe to create the target
		if target_type == 'bin':
			mf.write('\t%s -o %s %s %s\n' % (mk_esc_recipe(compiler),
				mk_esc_recipe(target),
				mk_esc_recipe(objs),
				mk_esc_recipe(lflags)))
		elif target_type == 'lib':
			mf.write('\trm -f %s\n\tar -crs %s %s\n' % (mk_esc_recipe(target),
				mk_esc_recipe(target),
				mk_esc_recipe(objs)))
		elif target_type == 'so':
			raise Exception('unimplemented')
		targets.append(target)

	# Parse each line of the file
	comment_regex = '^\\s*#'
	empty_regex = '^\\s*$'
	file = open(filename)
	lineno = 1
	for line in file:
		try: # Attempt to parse the line
			# Strip trailing newline, if any
			if len(line) > 0 and line[-1] == '\n': line = line[:-1]

			# Skip comments and empty lines
			if re.match(comment_regex, line) or re.match(empty_regex, line):
				lineno = lineno + 1
				continue

			# Process line as a key-value pair
			colpos = line.find(':')
			if colpos <= 0: raise Exception('invalid line')
			# Parse key
			key = line[0:colpos].strip()
			if not key in ctx: raise Exception('invalid key')
			# Parse value
			value = line[colpos + 1:]
			values = sh_unesc(value)

			# Each new 'target' key initiates processing of the previous target
			if key == 'target':
				if ctx['target']:
					process_target()
					for e in ctx: ctx[e] = None # Clear context
			elif not ctx['target']:
				raise Exception('key before target')

			# Certain keys expect a single non-empty value
			if key in ('target', 'type', 'compiler'):
				if len(values) > 1: raise Exception('multiple %s values' % key)
				elif len(values) == 0 or not values[0]: raise Exception('empty %s' % key)
				ctx[key] = values[0]
			else: # Others expect a list of values
				ctx[key] = values
			lineno = lineno + 1

		except Exception as e: # Add detail to exception and re-throw
			raise Exception('line %d: %s. line: %s' % (lineno, str(e), line))

	if ctx['target']: process_target() # Process final target

	# Update phony 'all' target
	mf_write_rule(mf, 'all', targets)

if __name__ == '__main__':
	try:
		# Parse command line arguments.
		# We pass all arguments to make except an optional initial -f <cfgfile> pair.
		buildcfg = 'build.cfg' # Default value
		args = sys.argv[1:] 
		if len(args) > 0: 
			if args[0] == '-f':
				if len(args) < 2: raise Exception('expected value for argument \'-f\'')
				buildcfg = args[1]
				# Remove the -f <cfgfile> pair from the arguments for make
				args = args[2:]

		# Specify '-j' argument to use all available processors unless '-j' is already specified
		if '-j' not in args: args = ['-j', str(multiprocessing.cpu_count())] + args

		# Process the config file, generating a makefile with dependencies
		try:
			process_cfg(buildcfg)
		except Exception as e:
			raise Exception('failed to parse %s: %s' % (sh_esc(buildcfg), str(e)))

		completed = subprocess.run(('make', '-f', '.build/makefile', *args))
		completed.check_returncode();

	except Exception as e:
		print('%s: error: %s' % (sys.argv[0], str(e)), file=sys.stderr)
		exit(1)
