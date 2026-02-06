// menu.c

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "bpmap.h"
#include "menu.h"
#include "starch.h"
#include "stem.h"

static int do_help(size_t argc, const char *argv[], int *flags);

// Helper function to parse an initial address argument.
// Returns zero on success.
static int get_addr(size_t argc, const char *argv[], uint64_t *addr)
{
	if (argc < 1) {
		printf("error: expected address argument\n");
		return 1;
	}

	// Parse address
	char *end = NULL;
	*addr = strtoll(argv[0], &end, 0);
	if (end == NULL || end == argv[0] || *end != '\0') {
		printf("error: unable to parse address \"%s\"\n", argv[0]);
		return 1;
	}
	return 0;
}

// Helper function to parse a second value argument.
// Returns zero on success.
static int get_val(size_t argc, const char *argv[], int64_t *val, const char *valname)
{
	if (argc < 2) {
		printf("error: expected %s argument\n", valname);
		return 1;
	}

	// Parse address
	char *end = NULL;
	*val = strtoll(argv[1], &end, 0);
	if (end == NULL || end == argv[1] || *end != '\0') {
		printf("error: unable to parse %s \"%s\"\n", valname, argv[1]);
		return 1;
	}
	return 0;
}

// Set a breakpoint at address
static int do_break(size_t argc, const char *argv[], int *flags)
{
	(void)flags;
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	// Add new breakpoint
	bpmap = bpmap_insert(bpmap, addr, 1);
	printf("breakpoint set at address %#"PRIx64"\n", addr);
	return 0;
}

// Un-pause processor execution
static int do_continue(size_t argc, const char *argv[], int *flags)
{
	(void)argv;
	(void)argc;
	*flags |= SF_RUN; // Un-pauses processor
	return 0;
}

// Delete a breakpoint at address
static int do_delete(size_t argc, const char *argv[], int *flags)
{
	// @todo: implement
	(void)argv;
	(void)argc;
	(void)flags;
	printf("unimplemented\n");
	return 0;
}

// Dump memory from given address and size
static int do_dump(size_t argc, const char *argv[], int *flags)
{
	(void)flags;

	// Parse address
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	// Parse size
	int64_t size = 0;
	if (get_val(argc, argv, &size, "size")) return 0;

	// Dump memory to stdout
	return mem_dump_hex(&main_mem, addr, size, stdout);
}

// List all breakpoints
static int do_list(size_t argc, const char *argv[], int *flags)
{
	// @todo: implement
	(void)argv;
	(void)argc;
	(void)flags;
	printf("unimplemented\n");
	return 0;
}

// Quit application
static int do_quit(size_t argc, const char *argv[], int *flags)
{
	(void)argv;
	(void)argc;
	*flags |= SF_EXIT; // Exits menu loop and emulation loop
	return 0;
}

// Read 8 bits from address
static int do_r8(size_t argc, const char *argv[], int *flags)
{
	(void)flags;
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	uint8_t data = 0;
	int ret = mem_read8(&main_mem, addr, &data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}

	printf("%#x\n", data);
	return 0;
}

// Read 16 bits from address
static int do_r16(size_t argc, const char *argv[], int *flags)
{
	(void)flags;
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	uint16_t data = 0;
	int ret = mem_read16(&main_mem, addr, &data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}

	printf("%#x\n", data);
	return 0;
}

// Read 32 bits from address
static int do_r32(size_t argc, const char *argv[], int *flags)
{
	(void)flags;
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	uint32_t data = 0;
	int ret = mem_read32(&main_mem, addr, &data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}

	printf("%#x\n", data);
	return 0;
}

// Read 64 bits from address
static int do_r64(size_t argc, const char *argv[], int *flags)
{
	(void)flags;
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	uint64_t data = 0;
	int ret = mem_read64(&main_mem, addr, &data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}

	printf("%#"PRIx64"\n", data);
	return 0;
}

// Print all register values
static int do_reg(size_t argc, const char *argv[], int *flags)
{
	(void)argv;
	(void)argc;
	(void)flags;
	for (int i = 0; i < STEM_NUM_CORES; i++) {
		printf("core %d:\n", i);
		printf("pc:  0x%016"PRIx64"\n", cores[i].pc);
		printf("sbp: 0x%016"PRIx64"\n", cores[i].sbp);
		printf("sfp: 0x%016"PRIx64"\n", cores[i].sfp);
		printf("sp:  0x%016"PRIx64"\n", cores[i].sp);
		printf("slp: 0x%016"PRIx64"\n", cores[i].slp);
	}
	return 0;
}

// Take a single step
static int do_step(size_t argc, const char *argv[], int *flags)
{
	(void)argv;
	(void)argc;
	*flags |= SF_STEP;
	return 0;
}

// Write 8 bits to address
static int do_w8(size_t argc, const char *argv[], int *flags)
{
	(void)flags;

	// Parse address
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	// Parse value
	int64_t data = 0;
	if (get_val(argc, argv, &data, "value")) return 0;
	if (!sdt_contains(SDT_A8, data)) { // Check bounds
		printf("error: value out of bounds\n");
		return 0;
	}

	int ret = mem_write8(&main_mem, addr, data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}
	return 0;
}

// Write 16 bits to address
static int do_w16(size_t argc, const char *argv[], int *flags)
{
	(void)flags;

	// Parse address
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	// Parse value
	int64_t data = 0;
	if (get_val(argc, argv, &data, "value")) return 0;
	if (sdt_contains(SDT_A16, data)) { // Check bounds
		printf("error: value out of bounds\n");
		return 0;
	}

	int ret = mem_write16(&main_mem, addr, data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}
	return 0;
}

// Write 32 bits to address
static int do_w32(size_t argc, const char *argv[], int *flags)
{
	(void)flags;

	// Parse address
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	// Parse value
	int64_t data = 0;
	if (get_val(argc, argv, &data, "value")) return 0;
	if (!sdt_contains(SDT_A32, data)) { // Check bounds
		printf("error: value out of bounds\n");
		return 0;
	}

	int ret = mem_write32(&main_mem, addr, data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}
	return 0;
}

// Write 64 bits to address
static int do_w64(size_t argc, const char *argv[], int *flags)
{
	(void)flags;

	// Parse address
	uint64_t addr = 0;
	if (get_addr(argc, argv, &addr)) return 0;

	// Parse value
	int64_t data = 0;
	if (get_val(argc, argv, &data, "value")) return 0;

	int ret = mem_write64(&main_mem, addr, data);
	if (ret) {
		printf("error: address out of bounds\n");
		return 0;
	}
	return 0;
}

static struct menu_item {
	const char *name; // Menu item name
	const char *helptext; // Help text
	int (*func)(size_t argc, const char *argv[], int *flags); // Menu item function
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
	{ "r16", "<addr> - read 16 bits at addr", do_r16 },
	{ "r32", "<addr> - read 32 bits at addr", do_r32 },
	{ "r64", "<addr> - read 64 bits at addr", do_r64 },
	{ "r8", "<addr> - read 8 bits at addr", do_r8 },
	{ "reg", "- show register values", do_reg },
	{ "step", "- execute a single instruction", do_step },
	{ "w16", "<addr> <val> - write 16 bits at addr", do_w16 },
	{ "w32", "<addr> <val> - write 32 bits at addr", do_w32 },
	{ "w64", "<addr> <val> - write 64 bits at addr", do_w64 },
	{ "w8", "<addr> <val> - write 8 bits at addr", do_w8 },
};

static void print_menu_help(const struct menu_item *items, size_t count)
{
	printf("commands:\n");
	for (size_t i = 0; i < count; i++) {
		printf("%s %s\n", items[i].name, items[i].helptext);
	}
	printf("unambiguous abbreviations of commands may be used\n");
}

static int do_help(size_t argc, const char *argv[], int *flags)
{
	// @todo: add ability to print more help on a certain topic
	(void)argv;
	(void)argc;
	(void)flags;
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
	if (low < count && strncmp(items[low].name, name, pfxlen) == 0) {
		// Next menu option starts with our prefix
		if (low + 1 >= count || strncmp(items[low + 1].name, name, pfxlen) != 0) {
			// The one after that does not. The reference is unambiguous.
			return items + low;
		}
		else {
			// So does the one after that. The reference is ambiguous.
			// Print list of matches.
			printf("error: ambiguous command \"%s\"\ndid you mean ", name);
			for (int i = low; i < count; i++) {
				if (strncmp(items[i].name, name, pfxlen) != 0) break;
				if (i > low) printf(", ");
				printf("%s", items[i].name);
			}
			printf("?\n");
		}
	}
	else {
		// No menu option starts with that prefix
		printf("error: unrecognized command \"%s\"\n", name);
	}
	return NULL;
}

int do_menu(int *flags)
{
	int ret = 0;
	if (!(*flags & (SF_RUN | SF_EXIT))) {
		if (*flags & SF_STEP) { // We got here by single-step
			*flags &= ~SF_STEP; // Clear single-step flag
		}
		else { // We got here by other means such as breakpoint
			printf("stem: menu: type \"?\" for help\n");
		}

		char *line = NULL;
		size_t linesize = 0;

		do {
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
				continue;
			}

			// Execute menu function
			ret = item->func(token_count - 1, tokens + 1, flags);
		} while (ret == 0 && !(*flags & (SF_RUN | SF_EXIT | SF_STEP)));

		free(line);
	}

	return ret;
}
