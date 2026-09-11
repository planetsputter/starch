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

# Returns whether the line can wrap. Line must be non-empty.
def line_can_wrap(line):
	# Find first character not space or tab
	i = 0
	while i < len(line) - 1:
		if line[i] in (' ', '\t'): i += 1
		else: break
	# Find last character not space, tab, or newline
	j = len(line) - 1
	while j > 0:
		if line[j] in (' ', '\t', '\n'): j -= 1
		else: break
	if line[i:i+3] == '```': return False # Used to open and close code blocks in markdown
	if line[i] == '#': return False # Used to denote headers in markdown
	if line[i] == '=': return False # Used to denote headers in markdown
	if line[i] == '|' and line[j] == '|': return False # Used to define tables in markdown
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

# Returns the line justified to the given number of columns
def justify_line(line, *, width, tabstop):
	# Split the line after prefix into words on spaces, attempting to preserve quoted strings
	prefix = get_prefix(line)
	aftpfx = line[len(prefix):] # Line after prefix
	words = []

	# Find whether the first quotation mark is preceded by something other than a space'
	# If so, we assume the first part of the line is part of a quoted string.
	qi = aftpfx.find('\'')
	if qi < 0: qi = aftpfx.find('"')
	if qi < 0: qi = aftpfx.find('`')
	if qi > 0 and aftpfx[qi - 1] != ' ': # All prior is part of a quoted string
		qi += 1
		word = aftpfx[0:qi]
	else:
		word = ''
		qi = 0

	quoted = ''
	while qi < len(aftpfx):
		c = aftpfx[qi]
		if quoted:
			word += c
			if c == quoted: quoted = ''
		elif c == '\'' or c == '"' or c == '`':
			word += c
			quoted = c
		elif c == ' ':
			if word:
				words += [word]
				word = ''
		else:
			word += c
		qi += 1
	if word: words += [word] # Append last word

	if len(words) == 0: # All whitespace after prefix
		line = prefix
	elif len(words) == 1: # Only one word after prefix
		line = prefix + words[0]
	else: # Multiple words after prefix
		# Prepend prefix to first word
		words[0] = prefix + words[0]

		# If spaces must be inserted, insert them after periods first, as these
		# probably indicate the end of a sentence.
		i = 0
		line = ' '.join(words)
		while i < len(words) - 1 and count_cols(line, tabstop) < width:
			if words[i][-1] == '.':
				words[i] += ' '
				line = ' '.join(words)
			i += 1

		# Construct list specifying in which positions to insert spaces into the line
		spl = []
		l = [(0, len(words) - 1)]
		while len(l) > 0:
			low = l[0][0]
			high = l[0][1]
			mid = (low + high) // 2
			spl += [mid]
			if low < mid: l += [(low, mid)]
			if mid + 1 < high: l += [(mid + 1, high)]
			l.pop(0)

		i = 0
		while True: # Keep inserting spaces until width is met
			line = ' '.join(words)
			if count_cols(line, tabstop) >= width: break
			words[spl[i]] += ' '
			i += 1
			if i >= len(spl): i = 0
	return line

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
			prefix = rem_prefix
	# Split the line on a space if allowed and necessary to maintain width
	if not inpre and len(line) > 1 and line_can_wrap(line):
		while True: # Keep writing until all wrapped lines are written
			si = len(line)
			c = count_cols(line, tabstop)
			while c > width:
				tsi = line[0:si].rfind(' ')
				if tsi < 0: break # No more spaces
				c = count_cols(line[0:tsi], tabstop)
				if c <= count_cols(prefix, tabstop): break # Space in prefix
				si = tsi
			if si >= len(line): break # Whole line fits, use as remnant
			if justify:
				justified = justify_line(line[0:si], width=width, tabstop=tabstop)
				line = justified + line[si:]
				si = len(justified)
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
		parser.add_argument('-j','--justify', dest='justify', metavar='justify', action='store_const', const=True, help='If specified, output is right- and left-justified. Implies \'-f\'.')

		# Parse arguments
		args = parser.parse_args()

		# Check for argument conflicts
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
