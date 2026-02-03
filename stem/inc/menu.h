// menu.h

#pragma once

#include <stdbool.h>

// Presents a text-based menu to the user.
// Returns negative if an error occurred.
// Returns zero to continue execution.
// Returns greater than zero to exit the application.
int do_menu();
