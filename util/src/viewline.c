// viewline.c

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "stmsg.h"
#include "viewline.h"

// Disable raw terminal mode
struct termios orig_termios;
static void viewline_disable_raw()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void viewline_enable_raw()
{
	// Store original terminal configuration once
	static int inited = 0;
	if (!inited) {
		tcgetattr(STDIN_FILENO, &orig_termios);
		atexit(viewline_disable_raw);
		inited = 1;
	}

	// Modify current terminal configuration
	struct termios raw = orig_termios;
	cfmakeraw(&raw);
    raw.c_cc[VMIN] = 1;  // read() blocks until at least 1 byte is ready
    raw.c_cc[VTIME] = 0; // No timeout

    // Apply new terminal configuration
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Structure to record a single line of user input
struct viewline {
	char *line;
	size_t bufsize;
	struct viewline *prev, *next;
};

// Allocates and returns a new input line record with the given line contents
// and previous line pointer.
static struct viewline *viewline_alloc(char *line, struct viewline *prev)
{
	struct viewline *vl = (struct viewline*)malloc(sizeof(struct viewline));
	vl->line = line;
	vl->bufsize = strlen(line) + 1;
	vl->next = NULL;
	vl->prev = prev;
	if (prev) {
		prev->next = vl;
	}
	return vl;
}

// Destroys and frees the given input line and all previous input lines
static void viewline_free_with_prev(struct viewline *vl)
{
	if (vl) {
		free(vl->line);
		struct viewline *prev = vl->prev;
		free(vl);
		viewline_free_with_prev(prev);
	}
}

// Inserts the given character at the given position, which must be less than
// or equal to the length of the string
static void viewline_insert(struct viewline *vl, char c, size_t pos)
{
	size_t len = strlen(vl->line);
	assert(pos <= len);
	if (len + 2 > vl->bufsize) {
		vl->line = realloc(vl->line, len + 16);
		vl->bufsize = len + 16;
	}
	// Move all characters from the insertion point to the end of the string to the right
	for (size_t i = len + 1; i > pos; i--) {
		vl->line[i] = vl->line[i - 1];
	}
	// Insert the character at the given position
	vl->line[pos] = c;
}

// Removes the character from the given position, which must be less than
// the length of the string
static void viewline_remove(struct viewline *vl, size_t pos)
{
	size_t len = strlen(vl->line);
	assert(pos < len);
	// Move all characters from the insertion point to the end of the string to the left
	for (size_t i = pos; i < len; i++) {
		vl->line[i] = vl->line[i + 1];
	}
}

// All input lines from the current user
static struct viewline *viewlines = NULL;

char *viewline_get()
{
	fflush(stdin); // @todo: necessary?
	fflush(stdout);

	if (!isatty(STDIN_FILENO)) { // Not a terminal
		char *line = NULL;
		size_t linesize = 0;
		ssize_t nread = getline(&line, &linesize, stdin);
		if (nread <= 0) {
			if (ferror(stdin)) { // error
				stmsgf(SMT_ERROR, "failed to read from stdin");
			}
			else { // EOF
				stmsgf(SMT_ERROR, "EOF while reading from stdin");
			}
		}
		viewlines = viewline_alloc(line, viewlines);
	}
	else { // Terminal
		viewline_enable_raw();

		// Create a new empty line record at the head of the list
		viewlines = viewline_alloc(strdup(""), viewlines);
		// Pointer to the user's place in line history
		struct viewline *current_line = viewlines;
		// Maintain a shadow copy of what the user is currently editing.
		// This is more similar to Bash behavior.
		char *edit_line = NULL;

		size_t cursor_pos = 0;
		enum { // Input states
			INS_DEFAULT,
			INS_ESC_BRKT,
			INS_ESC_LTR,
		};
		int state = INS_DEFAULT;
		char c;
		bool done = false;
		int signal = 0;
		while (!done && read(STDIN_FILENO, &c, 1)) {
			switch (state) {
			case INS_DEFAULT:
				switch (c) {
				case 3: // Likely Ctrl-C
					signal = SIGINT;
					done = true;
					break;
				case '\r': // CR
					printf("\r\n");
					done = true;
					break;
				case 0x1b: // ESC
					state = INS_ESC_BRKT;
					break;
				case 0x7f: // DEL (backspace)
					if (cursor_pos > 0) {
						viewline_remove(viewlines, --cursor_pos);
						// Move cursor back, reprint rest of line, print space at end,
						// and move back again
						printf("\b%s \b", viewlines->line + cursor_pos);
						size_t linelen = strlen(viewlines->line);
						for (size_t i = 0; i < linelen - cursor_pos; i++) {
							printf("\b");
						}
					}
					break;
				default:
					if (isprint(c)) {
						viewline_insert(viewlines, c, cursor_pos);
						printf("%c", c);
						cursor_pos++;
						size_t linelen = strlen(viewlines->line);
						if (cursor_pos < linelen) { // Reprint line after insertion position
							printf("%s", viewlines->line + cursor_pos);
							for (size_t i = 0; i < linelen - cursor_pos; i++) {
								printf("\b");
							}
						}
					}
				}
				break;
			case INS_ESC_BRKT:
				state = c == '[' ? INS_ESC_LTR : INS_DEFAULT; // Expect '['
				break;
			case INS_ESC_LTR:
				switch (c) {
				case 'A': // ESC[A: Up arrow
				case 'B': // ESC[A: Down arrow
					struct viewline *toline = c == 'A' ? current_line->prev : current_line->next;
					if (!toline) break; // No line to move to
					// Record original line length
					size_t origlinelen = strlen(viewlines->line);
					// If moving from the most recent line, save what was edited
					if (current_line == viewlines) {
						assert(edit_line == NULL);
						edit_line = strdup(current_line->line);
					}
					// Change the current line
					current_line = toline;
					// If moving to the most recent line, restore what was being edited
					char *src_line;
					if (current_line == viewlines) {
						assert(edit_line != NULL);
						src_line = edit_line;
						edit_line = NULL;
					}
					else {
						src_line = strdup(current_line->line);
					}
					// Copy source line to head of list (editing line)
					size_t linelen = strlen(src_line);
					free(viewlines->line);
					viewlines->line = src_line;
					viewlines->bufsize = linelen + 1;

					// Go to beginning of line
					for (; cursor_pos > 0; cursor_pos--) {
						printf("\b");
					}
					// Print new line contents
					printf("%s", viewlines->line);
					cursor_pos = linelen;
					// Erase any remnants of original line
					for (; cursor_pos < origlinelen; cursor_pos++) {
						printf(" ");
					}
					// Go back to end of new line contents
					for (; cursor_pos > linelen; cursor_pos--) {
						printf("\b");
					}
					break;
				case 'C': // ESC[A: Right arrow
					linelen = strlen(viewlines->line);
					if (cursor_pos < linelen) {
						printf("\x1b[C");
						cursor_pos++;
					}
					break;
				case 'D': // ESC[A: Left arrow
					if (cursor_pos > 0) {
						printf("\b");
						cursor_pos--;
					}
					break;
				default: // Ignore unrecognized escape
				}
				state = INS_DEFAULT;
				break;
			default:
				assert(false);
			}
			fflush(stdout);
		}

		viewline_disable_raw();

		free(edit_line);

		if (signal) {
			kill(getpid(), signal);
		}
	}

	// Don't include empty lines in history
	char *line;
	if (!viewlines->line || viewlines->line[0] == '\0') {
		struct viewline *prev = viewlines->prev;
		viewlines->prev = NULL;
		viewline_free_with_prev(viewlines);
		viewlines = prev;
		line = strdup("");
	}
	else {
		line = strdup(viewlines->line);
	}

	return line;
}

// Releases any resources used by viewline
void viewline_end()
{
	viewline_free_with_prev(viewlines);
	viewlines = NULL;
}
