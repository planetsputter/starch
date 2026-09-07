#!/usr/bin/env python3

"""Program to format text files, intelligently wrapping lines at a given number of columns while preserving overall document structure."""

import argparse, shutil, sys, tempfile

# String of all characters which may appear in a line prefix
__prefix_chars__ = '\t #*-/=`|'

# Returns the prefix of the given line
def get_prefix(line):
	i = 0
	while i < len(line):
		if line[i] in __prefix_chars__: i = i + 1
		else: break
	return line[0:i]

# Returns whether the prefix can wrap
def prefix_can_wrap(prefix):
	# Find first character not space or tab
	i = 0
	while i < len(prefix):
		if prefix[i] == ' ' or prefix[i] == '\t': i = i + 1
		else: break
	if prefix[i:i+1] == '`': return False # Used to open and close code blocks in markdown
	if prefix[i:i+1] == '#': return False # Used to denote headers in markdown
	if prefix[i:i+1] == '=': return False # Used to denote headers in markdown
	if prefix[i:i+1] == '|': return False # Used to define tables in markdown
	return True

# Returns the expected prefix of the next line after a line with the given prefix
def next_prefix(prefix):
	# Find first character not space or tab
	i = 0
	while i < len(prefix):
		if prefix[i] == ' ' or prefix[i] == '\t': i = i + 1
		else: break
	if prefix[i:i+1] == '*' or prefix[i:i+1] == '-': return prefix.replace('*', ' ') # Bulleted list
	return prefix # Unmodified

# Returns the number of columns in the given line
def count_cols(line, tabstop):
	i = 0
	c = 0
	while i < len(line):
		if line[i] == '\n': break
		if line[i] == '\t': c = (c + tabstop) // tabstop * tabstop
		else: c = c + 1
		i = i + 1
	return c

# Processes a single line of input from a file and an optional remnant from the previous line
# using the given parameters. Returns a remnant, if any.
def process_line(line, remnant, outfile, *, inpre, flow, justify, width, tabstop):
	prefix = get_prefix(line)
	if remnant: # Handle any remnant from the previous line
		rem_prefix = get_prefix(remnant)
		if inpre or not flow or next_prefix(rem_prefix) != prefix or len(line) <= 1: # Can't flow these lines together
			outfile.write(remnant)
		else: # Can flow these lines together
			line = remnant[0:-1] + ' ' + line[len(prefix):]
	# Split the line on a space if allowed and necessary to maintain width
	if not inpre and prefix_can_wrap(prefix) and len(line) > 1:
		while True: # Keep writing until all wrapped lines are written
			si = len(line)
			c = count_cols(line, tabstop)
			while c > width:
				tsi = line[0:si].rfind(' ')
				if tsi < 0: break
				si = tsi
				c = count_cols(line[0:si], tabstop)
			if si >= len(line): break
			outfile.write(line[0:si] + '\n')
			prefix = next_prefix(prefix)
			line = prefix + line[si + 1:]
	else: # Can't wrap or flow this line
		outfile.write(line)
		line = ''
	return line # Remnant

# Processes the input file, generating the output file, using the given parameters
def process_file(infile, outfile, *, flow, justify, width, tabstop):
	remnant = ''
	inpre = False # Whether we are in a preformatted block
	line = infile.readline()
	while True:
		remnant = process_line(line, remnant, outfile, inpre=inpre, flow=flow, justify=justify, width=width, tabstop=tabstop)
		if not line: break # EOF
		# Keep track of whether we are in a preformatted block to avoid modifying there
		i = 0
		while i < len(line):
			if line[i] == ' ' or line[i] == '\t': i = i + 1
			break
		if line[i:i+3] == '```': inpre = not inpre
		line = infile.readline()

if __name__ == '__main__':
	try:
		# Parse command line arguments
		parser = argparse.ArgumentParser('ifmt.py', description=__doc__)
		parser.add_argument('-w','--width', dest='width', metavar='width', type=int, default=80, help='Maximum number of columns (default: %(default)s)')
		parser.add_argument('-t', '--tabstop', dest='tabstop', metavar='tabstop', type=int, default=8, help='Number of columns between tabstops (default: %(default)s)')
		parser.add_argument('inputs', metavar='input', type=argparse.FileType('r'), nargs='*', default=[sys.stdin], help='Input file. If "-", STDIN is read.')
		parser.add_argument('-o','--output', dest='outfile', metavar='outfile', type=argparse.FileType('w'), help='Output file. If unspecified, output is written to STDOUT.')
		parser.add_argument('-O','--overwrite', dest='overwrite', metavar='overwrite', action='store_const', const=True, help='If specified, input files are overwritten in place. Cannot be specified in tandem with -o (--output).')
		parser.add_argument('-f','--flow', dest='flow', metavar='flow', action='store_const', const=True, help='If specified, consecutive lines with the same prefix are presumed to be part of the same block of text. Newlines are not preserved.')
		parser.add_argument('-j','--justify', dest='justify', metavar='justify', action='store_const', const=True, help='If specified, output is right- and left-justified. Implies \'-f\'. Neither tabs nor newlines are preserved.')

		# Parse arguments
		args = parser.parse_args()

		# Check for argument conflicts
		if args.justify:
			raise Exception('Unimplemented')
		if args.justify and args.flow:
			sys.stderr.write('Warning: Justification (\'-j\' or \'--justify\') implies flow (\'-f\' or \'--flow\'). Both are specified.\n')
		if args.outfile and args.overwrite:
			raise Exception('Cannot specify an output file (\'-o\' or \'--output\') and overwrite (\'-O\') in tandem.')
		if args.tabstop <= 0:
			raise Exception('Invalid tabstop value.')
		if args.width <= 0:
			raise Exception('Invalid width value.')

		# Justification implies flow
		if args.justify: args.flow = True

		# Process each input file
		for infile in args.inputs:
			# Determine the output file
			if args.overwrite:
				outfile = tempfile.NamedTemporaryFile('r+')
			else:
				if args.outfile: outfile = args.outfile
				else: outfile = sys.stdout

			# Process each line of the input file
			process_file(infile, outfile, flow=args.flow, justify=args.justify, width=args.width, tabstop=args.tabstop)
			# Close the input file
			infile.close()

			if args.overwrite:
				# Close the temporary file and move it to the input file
				outfile.flush()
				shutil.move(outfile.name, infile.name)
				outfile.close()

	except Exception as e:
		print(f'{sys.argv[0]}: error: {e}', file=sys.stderr)
		exit(1)
