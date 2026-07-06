// viewline.h

#pragma once

// Returns a string containing a single line of input from the terminal with no terminating newline.
// Allows the user to scroll through command history using the up and down arrow keys.
// The caller must free the returned string.
char *viewline_get();

// Releases any resources used by viewline and clears the command history.
// This should be called before program exit.
// May be called to clear command history before calling viewline_get().
void viewline_end();
