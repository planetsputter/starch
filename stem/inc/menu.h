// menu.h

#pragma once

// Presents a text-based debug menu to the user if indicated by *flags.
// May modify the given flags integer with SF_* flags.
// Menu will not be presented if SF_RUN or SF_EXIT flags are present.
// Returns zero on success, non-zero on failure.
int do_menu(int *flags);
