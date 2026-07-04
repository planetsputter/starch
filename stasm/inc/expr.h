// expr.h

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "token.h"

//
// Expression
//
struct expr {
	// Left hand side and right hand side.
	struct expr *lhs, *rhs;

	// Value token if lhs is null.
	// Operator token if lhs is non-null.
	// null for a null expression.
	struct token *op_val;
};

// Initializes the given expression, taking ownership of the given op/val token
void expr_init(struct expr*, struct token *op_val);

// Destroys the given expression and all sub-expressions
void expr_destroy(struct expr*);

// Type of function that returns whether the given non-literal string value
// could be converted to an integer value, and if so sets *intval.
typedef bool expr_lookup_func(void *userptr, bchar *strval, int64_t *intval);

// Evaluates the given expression. Calls the given function if not NULL for
// terms that are not integer literals.
// Returns zero on success.
int expr_eval(struct expr*, int64_t *val, void *userptr, expr_lookup_func f);

//
// Expression parser
//
struct expr_parser {
	struct expr *expr; // Current expression
	bool afterval; // Whether we just parsed a value as opposed to an operator
	struct expr_parser *subp; // Parser for parenthetical sub-expression
	struct token *group_char; // "(", "[", or "{" for grouped, or NULL for ungrouped
};

// Initializes the given expression parser with the given group character
void expr_parser_init(struct expr_parser*, struct token *group_char);

// Destroys the given expression parser and releases its resources
void expr_parser_destroy(struct expr_parser*);

// Takes ownership of and parses the given token. Returns 0 on success.
// Takes ownership of the token whether it succeeds or not.
int expr_parser_parse(struct expr_parser*, struct token *token);

// Returns whether the expression is complete.
// A null expression is complete.
bool expr_parser_complete(struct expr_parser*);
