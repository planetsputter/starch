#!/usr/bin/env python3

import argparse, glob, multiprocessing, pathlib, re, shlex, shutil, subprocess, sys

# Returns a shell line representing the given list of arguments, or single string argument
def sh_esc(args):
	if isinstance(args, str): args = [args]
	return ' '.join(shlex.quote(x) for x in args)

# Returns the list of arguments described by the given shell line
def sh_unesc(line):
	return shlex.split(line)

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
	# Make the build directory
	pathlib.Path('.build').mkdir(exist_ok=True)

	# Open the makefile
	mf = open('.build/makefile', 'w')

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
		if not target: raise Exception('no target specified')
		if not compiler: raise Exception('no compiler specified for target %s' % target)
		if not src: raise Exception('no src specified for target %s' % target)
		if not target_type: raise Exception('no type specified for target %s' % target)
		if not target_type in ('bin', 'lib', 'so'):
			raise Exception('invalid target type %s for target %s' % (target_type, target))
		if cflags == None: cflags = ''
		if lflags == None: lflags = ''

		cflist = sh_unesc(cflags) # Escape cflags to list
		lflist = sh_unesc(lflags) # Escape lflags to list

		# Add inc directories to cflags list
		if inc:
			incdirs = []
			for d in sh_unesc(inc): incdirs += glob.glob(d, recursive=True) # Allow globs
			cflist += ['-I%s' % d for d in incdirs] # Include directory in header search path

		# Generate dependencies for all source files
		sources = []
		for s in sh_unesc(src): sources += glob.glob(s, recursive=True) # Allow globs
		objs = []
		for source in sources:
			# Make the directory in which to build the object file
			# @todo: Need to prevent path traversal
			obj = change_ext('.build/obj/%s' % source, 'o')
			objs.append(obj)
			pathlib.Path(obj).parents[0].mkdir(parents=True, exist_ok=True)
			# Have the compiler generate the dependency rules
			args = (compiler, '-c', source, '-M', '-MM', '-MF', '-', '-MQ', obj, *cflist)
			result = subprocess.run(args, capture_output=True)
			if result.returncode:
				raise Exception('unable to generate dependency list for %s:\n%s\n%s'\
					% (sh_esc(source), sh_esc(args), result.stderr.decode('utf-8')))
			# Write the dependencies to the makefile
			mf.write(result.stdout.decode('utf-8'))
			# Write the build recipe to the makefile
			mf.write('\t%s -c %s -o %s %s\n' % (mk_esc_recipe(compiler), \
				mk_esc_recipe(source),
				mk_esc_recipe(obj),
				mk_esc_recipe(cflist)))

		# Listed libs are dependencies which also generate extra linker flags
		if libs:
			libraries = []
			for l in sh_unesc(libs): libraries += glob.glob(l, recursive=True) # Allow globs
			# Generate extra linker flags
			lflist += ['-L%s' % str(pathlib.Path(l).parents[0]) for l in libraries]
			lflist += ['-l%s' % get_lib_name(pathlib.Path(l).name) for l in libraries]
			# Write extra dependency rule
			mf_write_rule(mf, target, libraries)

		# Write the target object dependencies rule to the makefile
		mf_write_rule(mf, target, objs)

		# Write the recipe to create the target
		if target_type == 'bin':
			mf.write('\t%s -o %s %s %s\n' % (mk_esc_recipe(compiler), \
				mk_esc_recipe(target), \
				mk_esc_recipe(objs), \
				mk_esc_recipe(lflist)))
		elif target_type == 'lib':
			mf.write('\trm %s\n' % mk_esc_recipe(target))
			mf.write('\tar -crs %s %s\n' % (mk_esc_recipe(target), mk_esc_recipe(objs)))
		elif target_type == 'so':
			raise Exception('unimplemented')
		targets.append(target)

	# Parse each line of the file
	comment_regex = '^\\s*#'
	empty_regex = '^\\s*$'
	file = open(filename)
	for line in file:
		# Strip trailing newline, if any
		if len(line) > 0 and line[-1] == '\n': line = line[:-1]

		# Skip comments and empty lines
		if re.match(comment_regex, line) or re.match(empty_regex, line): continue

		# Process line as a key-value pair
		colpos = line.find(':')
		if colpos <= 0: raise Exception('invalid line: %s' % line)
		key = line[0:colpos]
		value = line[colpos + 1:]
		if not key in ctx: raise Exception('invalid key "%s" in line: %s' % (key, line))
		if key == 'target':
			if ctx['target']:
				process_target()
				for e in ctx: ctx[e] = None # Clear context
		elif not ctx['target']:
			raise Exception('key without target: %s' % key)
		ctx[key] = value
	if ctx['target']: process_target()
	return targets

if __name__ == '__main__':
	# Parse command-line arguments
	parser = argparse.ArgumentParser(prog='build.py', description='Python build script')
	parser.add_argument('targets', default='all', nargs='*', help='targets to build, \'all\', or \'clean\'')
	parser.add_argument('-f', '--file', default='build.cfg', help='build config file')
	parser.add_argument('-j', '--jobs', default=multiprocessing.cpu_count(), help='number of processors')
	args = parser.parse_args()

	# Ensure args.targets is an array
	if type(args.targets) != type([]):
		args.targets = [args.targets]

	# "clean" is a virtual target
	if 'clean' in args.targets:
		args.targets.remove('clean')
		if pathlib.Path('.build').exists():
			shutil.rmtree('.build')
	if len(args.targets) == 0:
		exit(0)

	# Process the config file, generating dependencies
	cfg_targets = None
	try:
		cfg_targets = process_cfg(args.file)
	except Exception as e:
		print('%s: an error occurred processing %s: %s' % (parser.prog, args.file, e))
		exit(1)

	# Build each specified target
	try:
		if 'all' in args.targets:
			args.targets.remove('all')
			for cfg_target in cfg_targets:
				if cfg_target not in args.targets: args.targets.append(cfg_target)
		if args.targets:
			completed = subprocess.run(('make', '-j', str(args.jobs), '-f', '.build/makefile', *args.targets))
			completed.check_returncode();
	except Exception as e:
		print('%s: error: %s' % (parser.prog, str(e)))
		exit(1)
