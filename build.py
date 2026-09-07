#!/usr/bin/env python3

"""Generic build script which invokes a compiler to accurately generate dependencies, then functions as a front-end for make."""

import concurrent.futures, copy, datetime, glob, hashlib, multiprocessing, os, pathlib, re, shlex, subprocess, sys, threading

# When generating makefile contents and output messages, try not to use more than this many columns per line
wrap_cols = 120

# Returns the number of columns in the given line
def count_cols(line, tabstop=8):
	i = 0
	c = 0
	while i < len(line):
		if line[i] == '\n': break
		if line[i] == '\t': c = (c + tabstop) // tabstop * tabstop
		else: c = c + 1
		i = i + 1
	return c

# Returns the string containing the given words separated by the given separator string
# and wrapped to the given number of columns with a backslash escaping the newlines
# and a single space at the beginning of each wrapped line. Optionally prepends a prefix.
def wordwrap(words, prefix='', sep=' ', wrap=0):
	if wrap:
		result = prefix
		first = True
		words = copy.copy(words) # Create a copy we can modify
		while len(words) > 0: # Wrap all words
			word = words[0]
			if first:
				result += word
				cols = count_cols(result)
				first = False
			elif cols + len(sep) + len(word) <= wrap - 2:
				result += sep + word
				cols += len(sep) + len(word)
			else:
				result += ' \\\n ' + word
				cols = 1 + count_cols(word)
			words.pop(0) # Consume first argument
	else:
		result = prefix + sep.join(words)
	return result


# Returns a shell line representing the given list of arguments, or single string argument.
# Optionally prepends a prefix. Optionally wraps to the given number of columns.
def sh_esc(args, prefix='', wrap=0):
	if isinstance(args, str): args = [args]
	return wordwrap([shlex.quote(x) for x in args], prefix=prefix, wrap=wrap)

# Returns the list of arguments described by the given shell line
def sh_unesc(line):
	return shlex.split(line)

# Returns a string containing the given string in quotes in a mainly-unambiguous way.
# Used for messages to the user.
def msg_quote(s):
	return f'"{s.replace('"', '\\"').replace('\n', '\\n')}"'

# Returns the string escaped for use in a makefile rule
def mk_esc_rule(rule):
	return rule.replace('$', '$$').replace(' ', '\\ ')

# Returns the string representing a list of arguments or single string argument
# as a makefile recipe. Prepends a tab and appends a newline.
def mk_esc_recipe(args):
	escaped = [arg.replace('$', '$$') for arg in args]
	return sh_esc(escaped, prefix='\t', wrap=wrap_cols) + '\n'

# Writes a dependency rule to the given makefile, escaping the target and each dependency.
# deps may be a string representing a single dependency or a list of dependencies.
def mf_write_rule(mf, target, deps):
	if isinstance(deps, str): deps = [deps]
	mf.write(wordwrap([mk_esc_rule(dep) for dep in deps], prefix=mk_esc_rule(target) + ':', wrap=wrap_cols) + '\n')

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

# Inserts values from the parent dictionary into the child dictionary
def inherit(child, parent):
	for e in parent:
		if child[e] == None: child[e] = copy.copy(parent[e])

# Returns whether the given dictionary only contains the given keys as non-None values
def only_contains(d, keys):
	for key in d:
		if d[key] != None and not key in keys: return False
	return True

# Runs the command described by the given arguments, intended to generate dependencies
# for the given source. Throws an exception if the command returns non-zero. Otherwise
# returns a string of the process's captured stdout (the dependencies).
def gen_deps(source, args):
	result = subprocess.run(args, capture_output=True)
	if result.returncode:
		raise Exception(f'unable to generate dependency list for {msg_quote(source)}:\n' +
			f'{sh_esc(args, wrap=wrap_cols)}\n' +
			f'{result.stderr.decode('utf-8')}')
	return result.stdout.decode('utf-8')

# Experimentation indicates that this script spends most of its time waiting for the compiler
# to generate dependencies. We spawn multiple threads to reduce execution time. These threads
# are believed to be IO-bound, not CPU-bound, so we make more than we have CPUs.
executor = concurrent.futures.ThreadPoolExecutor(max_workers=multiprocessing.cpu_count() + 4)

# Process a build configuration file
def process_cfg(filename, buildcfg):
	# All configurations by name
	configs = {}

	# All non-phony targets in order of definition
	targets = []

	# List of objects for all targets
	allobjs = []

	# Processes the section with the given settings
	def process_section(sec):
		# Check context
		config = sec['config']
		if config != None: # This is a configuration section
			if config in configs: raise Exception(f'redefinition of config {msg_quote(config)}')
			configs[config] = copy.copy(sec)
			return
		# Convenience variable
		target = sec['target']
		# Check that current build configuration has been defined
		if buildcfg not in configs:
			raise Exception(f'config {msg_quote(buildcfg)} was not specified before target {msg_quote(target)}')
		# Check target type
		target_type = sec['type']
		if not target_type: raise Exception(f'no target type specified for target {msg_quote(target)}')
		if not target_type in ('bin', 'lib', 'so', 'phony'):
			raise Exception(f'invalid target type {target_type} for target {msg_quote(target)}')
		if target_type == 'phony':
			# Phony targets may only contain certain keys
			if not only_contains(sec, ('target', 'type', 'requires', 'required-by')):
				raise Exception(f'only "requires" and "required-by" may be specified for phony target {msg_quote(target)}')
		else: # Keep track of all non-phony targets
			targets.append(target)
		# Inherit values from the current build configuration
		inherit(sec, configs[buildcfg])

		# Emit comment at beginning of rules for this section
		mf.write(f'\n# Rules for target {sh_esc(target)}\n')

		# Document any explicit dependencies
		requires = sec['requires']
		required_by = sec['required-by']
		if required_by != None:
			for req in required_by:
				mf_write_rule(mf, req, target)
		if requires != None:
			mf_write_rule(mf, target, requires)
		if target_type == 'phony':
			mf_write_rule(mf, '.PHONY', target)
			return # Phony targets only have explicit dependencies

		# Check other keys
		inc = sec['inc']
		libs = sec['libs']
		cflags = sec['cflags']
		lflags = sec['lflags']
		compiler = sec['compiler']
		src = sec['src']
		if not compiler: raise Exception(f'no compiler specified for target {msg_quote(target)}')
		if not src: raise Exception(f'no src specified for target {msg_quote(target)}')
		if cflags == None: cflags = []
		if lflags == None: lflags = []

		# Add inc directories to cflags list
		if inc:
			incdirs = []
			for d in inc:
				globs = glob.glob(d, recursive=True) # Allow globs
				if globs: incdirs += globs
				else: incdirs += [d] # Globs were not used
			cflags += [f'-I{d}' for d in incdirs] # Include directory in header search path

		# Generate dependencies for all source files
		sources = []
		for s in src:
			globs = glob.glob(s, recursive=True) # Allow globs
			if globs: sources += globs
			else: sources += [s] # Globs were not used
		objs = []
		futures = []
		for source in sources:
			# Compute a hash that encodes the significant parameters for generating the object for this source
			h = hashlib.sha256(repr((compiler, source, cflags)).encode('utf-8')).hexdigest()[0:16]
			# Compute the name for the object file, which includes the hash
			obj = f'.build/obj/{basename(source, withext=False)}-{h}.o'
			objs.append(obj)
			if obj in allobjs: continue # This object is required by multiple targets, rule already generated
			allobjs.append(obj)
			# Have the compiler generate the dependency rules
			args = (compiler, '-c', source, '-M', '-MM', '-MF', '-', '-MQ', obj, *cflags)
			futures.append(executor.submit(gen_deps, source, args))
		i = 0
		while i < len(sources): # Wait for dependency generation to complete for all sources
			deps = futures[i].result()
			# Write the dependencies to the makefile
			mf.write(deps)
			# Write the build recipe to the makefile
			mf.write(mk_esc_recipe((compiler, '-c', sources[i], '-o', objs[i], *cflags)))
			i += 1

		# Listed libs are dependencies which also generate extra linker flags
		if libs:
			libraries = []
			for l in libs:
				globs = glob.glob(l, recursive=True) # Allow globs
				if globs: libraries += globs
				else: libraries += [l] # Globs were not used
			# Generate extra linker flags
			lflags += [f'-L{pathlib.Path(l).parents[0]}' for l in libraries]
			lflags += [f'-l{get_lib_name(pathlib.Path(l).name)}' for l in libraries]
			# Write extra dependency rule
			mf_write_rule(mf, target, libraries)

		# Write the target dependencies rule to the makefile.
		# The dependencies themselves depend on the build configuration.
		mf_write_rule(mf, target, objs + ['.build/lastcfg'])

		# Write the recipe to create the target
		if target_type == 'bin':
			mf.write(mk_esc_recipe((compiler, '-o', target, *objs, *lflags)))
		elif target_type == 'lib':
			mf.write(mk_esc_recipe(('rm', '-f', target)))
			mf.write(mk_esc_recipe(('ar', '-crs', target, *objs)))
		elif target_type == 'so':
			raise Exception('unimplemented')

	# Open the build configuration file
	file = open(filename)

	# Make the build directory, object directory, and rule directory
	pathlib.Path('.build/obj').mkdir(exist_ok=True, parents=True)

	# Open the makefile
	mf = open('.build/makefile', 'w')

	# Generate phony targets, specify 'all' as first target, and disable builtin rules.
	# Automatically running the 'clean' target when the makefile is parsed allows us
	# to avoid a race between 'clean' and other targets during a parallel build.
	mf.write(
		f'# Generated by {sh_esc(basename(sys.argv[0]))}\n' +
		f'# time: {datetime.datetime.now()}\n' +
		f'# config: {sh_esc(buildcfg)}\n' +
		'.PHONY:clean all every\n' +
		'all:\n' +
		'clean:\n' +
		'\t@echo rm -f .build/obj/*.o\n' +
		'ifneq ($(filter clean,$(MAKECMDGOALS)),)\n' +
		'    $(shell rm -f .build/obj/*.o)\n' +
		'endif\n' +
		'MAKEFLAGS+=-rR\n')

	# Parameters from section
	ctx = {
		'config': None, # Configuration name
		'target': None, # Target name
		'requires': None,
		'required-by': None,
		'compiler': None,
		'src': None,
		'inc': None,
		'libs': None,
		'type': None,
		'cflags': None,
		'lflags': None
	}
	# Parse each line of the file
	comment_regex = r'^\s*#'
	empty_regex = r'^\s*$'
	lineno = 0
	for line in file:
		try: # Attempt to parse the line
			lineno += 1
			# Strip trailing newline, if any
			if len(line) > 0 and line[-1] == '\n': line = line[:-1]

			# Skip comments and empty lines
			if re.match(comment_regex, line) or re.match(empty_regex, line):
				continue

			# Process line as a key-value pair
			colpos = line.find(':')
			if colpos <= 0: raise Exception('invalid line')
			# Parse key
			key = line[0:colpos].strip()
			# Parse value
			values = sh_unesc(line[colpos + 1:])

			# See if the key has a "key[config]" format
			config = None
			lbpos = key.find('[')
			rbpos = key.find(']')
			if lbpos > 0 and rbpos == len(key) - 1:
				config = key[lbpos + 1:rbpos].strip()
				key = key[:lbpos].strip()

			if key == 'target' or key == 'config':
				# 'target' or 'config' keys initiate a new section.
				# We must process the old section, if any.
				if ctx['target'] or ctx['config']:
					process_section(ctx)
					for e in ctx: ctx[e] = None # Clear context
			elif not key in ctx:
				raise Exception(f'invalid key {msg_quote(key)}')
			elif not ctx['target'] and not ctx['config']:
				raise Exception('key before target or config')

			if config != None and config != buildcfg:
				# This line is for a different configuration
				continue

			# Certain keys expect a single non-empty value
			if key in ('target', 'type', 'compiler', 'config'):
				if len(values) > 1: raise Exception(f'multiple {key} values')
				elif len(values) == 0 or not values[0]: raise Exception(f'empty {key}')
				ctx[key] = values[0]
			else: # Others expect a list of values
				ctx[key] = values

		except Exception as e: # Add detail to exception and re-throw
			raise Exception(f'line {lineno}: {e}')

	if ctx['target'] or ctx['config']: process_section(ctx) # Process final section

	# Update phony 'every' target
	mf.write('\n')
	mf_write_rule(mf, 'every', targets)

if __name__ == '__main__':
	try:
		# Parse command line arguments.
		# We pass all arguments to make except an optional initial -f <cfgfile> pair.
		cfgfile = 'build.cfg' # Default value
		args = sys.argv[1:] 
		if len(args) > 0: 
			if args[0] == '-f':
				if len(args) < 2: raise Exception('expected value for argument \'-f\'')
				cfgfile = args[1]
				# Remove the -f <cfgfile> pair from the arguments for make
				args = args[2:]

		# Determine the current build configuration
		buildcfg = os.getenv('BUILDCFG')

		# Scan for a command line argument that assigns the BUILDCFG variable
		# such as "BUILDCFG=debug". Make parses these and they will override
		# an environment variable.
		for arg in args:
			equpos = arg.find('=')
			if equpos >= 0 and arg[:equpos] == 'BUILDCFG': # An assignment to BUILDCFG
				buildcfg = arg[equpos + 1:]

		# A build configuration must be specified
		if not buildcfg:
			raise Exception('a build configuration must be specified with the BUILDCFG environment variable ' +
				'or command line argument')
		# Set our environment variable BUILDCFG so the make process will inherit it
		os.putenv('BUILDCFG', buildcfg)

		# Specify '-j' argument to use all available processors unless '-j' is already specified
		if '-j' not in args: args = ['-j', str(multiprocessing.cpu_count())] + args

		# Process the config file, generating a makefile with dependencies
		try:
			process_cfg(cfgfile, buildcfg)
		except Exception as e:
			raise Exception(f'failed to process {msg_quote(cfgfile)}: {e}')

		# Check what the last build configuration was
		lastcfg = None
		try:
			with open('.build/lastcfg') as file:
				lastcfg = file.readline().strip()
		except FileNotFoundError as e:
			pass

		if lastcfg != buildcfg:
			# Record current build configuration if different from last
			with open('.build/lastcfg', 'w') as file:
				file.write(buildcfg)

		result = subprocess.run(('make', '-f', '.build/makefile', *args))
		result.check_returncode();

	except Exception as e:
		print(f'{basename(sys.argv[0])}: error: {e}', file=sys.stderr)
		exit(1)
