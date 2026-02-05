// menu.c

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "bpmap.h"
#include "menu.h"
#include "stem.h"

static int do_help(const char *argv[], size_t argc);

static int do_break(const char *argv[], size_t argc)
{
	if (argc == 0) {
		printf("error: expected address argument\n");
		return 0;
	}

	// Parse BP address
	char *end = NULL;
	long long addr = strtoll(argv[0], &end, 0);
	if (end == NULL || end == argv[0] || *end != '\0') {
		printf("error: unable to parse BP address \"%s\"\n", argv[0]);
	}
	else {
		// Add new breakpoint
		bpmap = bpmap_insert(bpmap, addr, 1);
		printf("breakpoint set at address %#llx\n", addr);
	}
	return 0;
}

static int do_continue(const char *argv[], size_t argc)
{
	(void)argv;
	(void)argc;
	return 256; // Exits menu loop
}

static int do_delete(const char *argv[], size_t argc)
{
	// @todo: implement
	(void)argv;
	(void)argc;
	printf("unimplemented\n");
	return 0;
}

static int do_dump(const char *argv[], size_t argc)
{
	if (argc < 2) {
		printf("error: expected <addr> <size>\n");
		return 0;
	}

	// Parse address
	char *end = NULL;
	long long addr = strtoll(argv[0], &end, 0);
	if (end == NULL || end == argv[0] || *end != '\0') {
		printf("error: unable to parse address \"%s\"\n", argv[0]);
		return 0;
	}
	end = NULL;
	long long size = strtoll(argv[1], &end, 0);
	if (end == NULL || end == argv[1] || *end != '\0') {
		printf("error: unable to parse size \"%s\"\n", argv[1]);
		return 0;
	}

	// Dump memory to stdout
	return mem_dump_hex(&main_mem, addr, size, stdout);
}

static int do_list(const char *argv[], size_t argc)
{
	// @todo: implement
	(void)argv;
	(void)argc;
	printf("unimplemented\n");
	return 0;
}

static int do_quit(const char *argv[], size_t argc)
{
	(void)argv;
	(void)argc;
	return 512; // Exits menu loop and emulation loop
}

static struct menu_item {
	const char *name; // Menu item name
	const char *helptext; // Help text
	int (*func)(const char *argv[], size_t argc); // Menu item function
} menu_items[] = {
	// List main menu items in alphabetical order
	{ "?", "- print help", do_help },
	{ "break", "<addr> - set breakpoint at addr", do_break },
	{ "continue", "- continue program execution", do_continue },
	{ "delete", "<addr> - delete breakpoint at addr", do_delete },
	{ "dump", "<addr> <size> - dump size bytes of memory at addr", do_dump },
	{ "help", "- print help", do_help },
	{ "list", "- list source code", do_list },
	{ "quit", "- terminate program", do_quit },
};

static void print_menu_help(const struct menu_item *items, size_t count)
{
	printf("commands:\n");
	for (size_t i = 0; i < count; i++) {
		printf("%s %s\n", items[i].name, items[i].helptext);
	}
	printf("unambiguous abbreviations of commands can also be used\n");
}

static int do_help(const char *argv[], size_t argc)
{
	// @todo: add ability to print more help on a certain topic
	(void)argv;
	(void)argc;
	print_menu_help(menu_items, sizeof(menu_items) / sizeof(*menu_items));
	return 0;
}

// Looks up a menu item from the list of menu items by name or unambiguous prefix.
// Returns a pointer to the menu item or NULL.
static struct menu_item *menu_item_lookup(struct menu_item *items, int count, const char *name)
{
	if (count == 0) return NULL;

	// Use binary search for efficiency
	int low = 0, high = count - 1, mid;
	while (low <= high) {
		mid = (low + high) / 2;
		int comp = strcmp(name, items[mid].name);
		if (comp < 0) {
			high = mid - 1;
		}
		else if (comp > 0) {
			low = mid + 1;
		}
		else {
			return items + mid;
		}
	}

	// No exact match found. See if this is an unambiguous prefix.
	// The prefix will have sorted before the first possible match.
	size_t pfxlen = strlen(name);
	if (low < count && strncmp(items[low].name, name, pfxlen) == 0 &&
		(low + 1 >= count || strncmp(items[low + 1].name, name, pfxlen) != 0)) {
		return items + low;
	}
	return NULL;
}

int do_menu()
{
	char *line = NULL;
	size_t linesize = 0;

	printf("stem: menu: type \"?\" for help\n");

	int ret = 0;
	while (ret == 0) {
		// Print prompt
		printf("> ");

		// Read line from user
		// @todo: Handle common terminal escape sequences such as those for arrow keys
		ssize_t nread = getline(&line, &linesize, stdin);
		if (nread <= 0) {
			if (ferror(stdin)) { // error
				fprintf(stderr, "error: failed to read from stdin\n");
			}
			else { // EOF
				fprintf(stderr, "error: EOF while reading from stdin\n");
			}
			ret = 1;
			break;
		}

		// Get tokens from line
		enum { MAX_TOKENS = 3 };
		const char *ws = " \t\n";
		const char *tokens[MAX_TOKENS] = {};
		char *tokptr = line;
		size_t token_count;
		for (token_count = 0; token_count < MAX_TOKENS; token_count++) {
			const char *token = strtok(tokptr, ws);
			if (!token) break;
			tokens[token_count] = token;
			tokptr = NULL;
		}

		if (token_count == 0) { // Empty or whitespace line
			continue;
		}

		// Look up menu item for first token
		struct menu_item *item = menu_item_lookup(menu_items, sizeof(menu_items) / sizeof(*menu_items), tokens[0]);
		if (!item) {
			printf("error: unrecognized command \"%s\"\n", tokens[0]);
			continue;
		}

		// Execute menu function
		ret = item->func(tokens + 1, token_count - 1);
	}
	if (ret >= 256) ret -= 256;

	free(line);
	return ret;
}
