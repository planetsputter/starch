#!/usr/bin/env python3

import argparse, pathlib, re

# Dictionary of line count dictionaries by file extension
all_counts = {}

# Inserts or numerically adds all fields of d1 to the matching fields in d2
def add_dict(d1, d2):
	for key in d1:
		if key in d2: d2[key] = d2[key] + d1[key]
		else: d2[key] = d1[key]

# Prints a human-readable list of line counts with the given label
def print_counts(counts, ext):
	print(f'{counts['files']} {ext} files')
	total = 0
	for key in counts:
		if key == 'files': continue
		print(f'  {key + ':':8} {counts[key]:6d}')
		total = total + int(counts[key])
	print(f'  total:   {total:6d}')

# Prints a human-readable list of all line counts by file extension with a total
def print_all_counts():
	totals = {}
	for ext in all_counts:
		print_counts(all_counts[ext], f'.{ext}')
		add_dict(all_counts[ext], totals)
	print('-----------------')
	print_counts(totals, 'total source')

# Update counts and return new state for a single line of Python
def parse_python_line(state, line, counts):
	if state == None:
		state = 0 # Set initial state
		counts['code'] = 0
		counts['comment'] = 0
		counts['blank'] = 0

	comment_regex = r'^\s*#'
	blank_regex = r'^\s*$'
	if re.match(comment_regex, line):
		counts['comment'] = counts['comment'] + 1
	elif re.match(blank_regex, line):
		counts['blank'] = counts['blank'] + 1
	else:
		counts['code'] = counts['code'] + 1
	return state

# Update counts and return new state for a single line of C
def parse_c_line(state, line, counts):
	if state == None:
		state = 0 # Set initial state
		counts['code'] = 0
		counts['comment'] = 0
		counts['blank'] = 0

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
					i = i + 1
				else:
					contains_code = True
			elif not char.isspace():
				contains_code = True
		elif state == 1: # Multi line comment
			if char == '*' and next_char == '/': # End multi line comment
				state = 0
				i = i + 1
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

	return state

# Update counts and return new state for a single line of markdown
def parse_md_line(state, line, counts):
	if state == None:
		state = 0 # Set initial state
		counts['doc'] = 0
		counts['blank'] = 0
	if line.isspace(): counts['blank'] = counts['blank'] + 1
	else: counts['doc'] = counts['doc'] + 1
	return state

# Dictionary of parser functions by file extension
parsers = {
	'py' : parse_python_line,
	'c' : parse_c_line,
	'h' : parse_c_line,
	'md' :  parse_md_line,
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
	if ext not in all_counts: # First time counting a file with this extension
		all_counts[ext] = { 'files': 0 }
	all_counts[ext]['files'] = all_counts[ext]['files'] + 1

	state = None
	counts = {}

	# Open file to count lines
	file = open(path)
	for line in file:
		# Parse each line of the file
		state = parsers[ext](state, line, counts)
	add_dict(counts, all_counts[ext])
	file.close()

# Update line counts based on the file or directory at the given path, recursively
def count_path(path):
	if path.is_dir():
		for child in path.iterdir():
			count_path(child)
	else:
		count_file(path)

if __name__ == '__main__':
	argp = argparse.ArgumentParser(description='Count lines of code')
	argp.add_argument('target', help='file or directory in which to count lines (default current directory)',
		nargs='?', default='.')
	args = argp.parse_args()

	# Count all source files in the target directory
	count_path(pathlib.Path(args.target))

	print_all_counts()
