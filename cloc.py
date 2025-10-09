#!/usr/bin/env python3

import pathlib, re

# Dictionary of line count dictionaries by file extension
counts_by_ext = {}

# Prints a human-readable list of line counts with the given label
def print_counts(counts, lbl):
	print(f'{counts['files']} {lbl} files')
	print(f'  comment: {counts['comment']}')
	print(f'  code:    {counts['code']}')
	print(f'  blank:   {counts['blank']}')
	print(f'  total:   {counts['total']}')

# Prints a human-readable list of line counts by file extension with a total
def print_counts_by_ext():
	totals = {'files' : 0, 'comment' : 0, 'code' : 0, 'blank' : 0, 'total' : 0 }
	for ext in counts_by_ext:
		print_counts(counts_by_ext[ext], f'.{ext}')
		for field in totals:
			totals[field] = totals[field] + counts_by_ext[ext][field]
	print('----------')
	print_counts(totals, 'total source')

# Update counts and return new state for a single line of Python
def parse_python_line(state, line, counts):
	comment_regex = r'^\s*#'
	blank_regex = r'^\s*$'
	if re.match(comment_regex, line):
		counts['comment'] = counts['comment'] + 1
	elif re.match(blank_regex, line):
		counts['blank'] = counts['blank'] + 1
	else:
		counts['code'] = counts['code'] + 1
	counts['total'] = counts['total'] + 1
	return state

# Update counts and return new state for a single line of C
def parse_c_line(state, line, counts):
	if state == None: state = 0 # Set initial state

	contains_code = False

	i = 0
	while i < len(line):
		# Look ahead to next two characters
		char = line[i]
		if i + 1 < len(line): next_char = line[i + 1]
		else: next_char = '\0'

		if state == 0: # Default
			if char == '/':
				if next_char == '*': # Begin multi line comment
					state = 1
					i = i + 1
				elif next_char == '/': # Begin single line comment
					state = 2
				else:
					contains_code = True
			elif not char.isspace():
				contains_code = True
		elif state == 1: # Multi line comment
			if char == '*' and next_char == '/': # End multi line comment
				state = 0
		elif state == 2: # Single line comment
			if char == '\n': # End single line comment
				state = 0
		i = i + 1

	# Update line counts
	if contains_code:
		counts['code'] = counts['code'] + 1
	elif line.isspace():
		counts['blank'] = counts['blank'] + 1
	else:
		counts['comment'] = counts['comment'] + 1

	counts['total'] = counts['total'] + 1
	return state

# Dictionary of parser functions by file extension
parsers = {
	'py' : parse_python_line,
	'c' : parse_c_line,
	'h' : parse_c_line
}

# Returns the extension of the given path, or empty string if there is none
def file_ext(path):
	path = str(path)
	dotpos = path.rfind('.')
	slashpos = path.rfind('/')
	if slashpos >= dotpos: dotpos = len(path)
	return path[dotpos + 1:]

# Update line counts based on the file at the given path
def count_file(path):
	ext = file_ext(path)
	if ext not in parsers: return # Only count files with known extensions
	if ext not in counts_by_ext: # First time counting a file with this extension
		counts_by_ext[ext] = { 'files': 0, 'comment' : 0, 'code' : 0, 'blank' : 0, 'total' : 0 }
	counts_by_ext[ext]['files'] = counts_by_ext[ext]['files'] + 1

	state = None

	# Open file to count lines
	file = open(path)
	for line in file:
		# Parse each line of the file
		state = parsers[ext](state, line, counts_by_ext[ext])
	file.close()

# Update line counts based on the file or directory at the given path, recursively
def count_path(path):
	if path.is_dir():
		for child in path.iterdir():
			count_path(child)
	else:
		count_file(path)

if __name__ == '__main__':
	# Count all files in the current directory
	count_path(pathlib.Path('.'))

	print_counts_by_ext()
