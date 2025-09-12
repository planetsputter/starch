#!/usr/bin/env python3

import argparse, glob, multiprocessing, pathlib, re, shlex, shutil, subprocess, sys

# Returns a shell line representing the given list of arguments
def shell_escape(args):
	return ' '.join(shlex.quote(x) for x in args)

# Returns the list of arguments described by the given shell line
def shell_unescape(line):
	return shlex.split(line)

# Changes the extension of the given path
def change_ext(path, ext):
	dotpos = path.rfind('.')
	slashpos = path.rfind('/')
	if slashpos > dotpos or dotpos < 0: return path + '.' + ext
	return path[0:dotpos] + '.' + ext

# Returns the name of the library at the given path, such as would be passed with '-l' to the linker
def get_lib_name(path):
	path = path[path.rfind('/') + 1:] # Strip parent directory
	dotpos = path.rfind('.')
	if dotpos < 0: dotpos = len(path)
	path = path[0:dotpos] # Strip extension
	if path[0:3] == 'lib': path = path[3:] # Strip leading 'lib'
	return path

# Process a build configuration file
def process_cfg(filename):
	# Make the build directory
	build_dir = pathlib.Path('.build')
	build_dir.mkdir(exist_ok=True)

	# Open the makefile
	mf = open('.build/makefile', 'w')

	# Parameters from file
	ctx = {
		'target': None,
		'compiler': None,
		'src': None,
		'inc': None,
		'deps': None,
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
		deps = ctx['deps']
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

		# Add inc directories to cflags
		if inc:
			incdirs = []
			for d in shell_unescape(inc): incdirs += glob.glob(d, recursive=True) # Allow globs
			for d in incdirs: cflags += ' -I%s' % d # Include directory in header search path

		# Generate dependencies for all source files
		sources = []
		for s in shell_unescape(src): sources += glob.glob(s, recursive=True) # Allow globs
		objs = []
		for source in sources:
			# Make the directory in which to build the object file
			obj = change_ext('.build/obj/%s' % source, 'o')
			objs.append(obj)
			objpath = pathlib.Path(obj)
			objpath.parents[0].mkdir(parents=True,exist_ok=True)
			# Have the compiler generate the dependencies
			args = (compiler, '-c', source, '-M', '-MM', '-MF', '-', '-MQ', obj, *shell_unescape(cflags))
			result = subprocess.run(args, capture_output=True)
			if result.returncode:
				raise Exception('unable to generate dependency list for %s:\n%s\n%s'\
					% (source, shell_escape(args), result.stderr.decode('utf-8')))
			# Write the dependencies to the makefile
			mf.write(result.stdout.decode('utf-8'))
			# Write the build rule to the makefile
			mf.write('\t%s -c $< -o $@ %s\n' % (compiler, cflags))

		# Listed libs are dependencies which also generate extra linker flags
		if libs:
			libraries = []
			for l in shell_unescape(libs): libraries += glob.glob(l, recursive=True) # Allow globs
			for lib in libraries:
				libpath = pathlib.Path(lib)
				# Include parent directory in library search path
				lflags += ' -L%s -l%s' % (str(libpath.parents[0]), get_lib_name(libpath.name))
			mf.write('%s:%s\n' % (target, libs))

		# List user-provided dependencies
		if deps: mf.write('%s:%s\n' % (target, deps))

		# Write the target rule to the makefile
		obj_esc = shell_escape(objs)
		mf.write('%s: %s\n' % (target, obj_esc))
		if target_type == 'bin':
			mf.write('\t%s -o $@ %s %s\n' % (compiler, obj_esc, lflags))
		elif target_type == 'lib':
			mf.write('\trm $@\n')
			mf.write('\tar -crs $@ %s\n' % obj_esc)
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
		colidx = line.find(':')
		if colidx <= 0: raise Exception('invalid line: %s' % line)
		key = line[0:colidx]
		value = line[colidx + 1:]
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
