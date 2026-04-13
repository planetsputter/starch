// expr.h

#pragma once

#include <stdbool.h>

#include "bstr.h"

//
// Expression
//
struct expr {
	// Left hand side and right hand side.
	struct expr *lhs, *rhs;

	// Value if lhs is null.
	// Operator if lhs is non-null.
	// null for a null expression.
	bchar *op_val;
};

// Initializes the given expression, taking ownership of the given op/val
void expr_init(struct expr*, bchar *op_val);

// Destroys the given expression and all sub-expressions
void expr_destroy(struct expr*);

//
// Expression parser
//
struct expr_parser {
	struct expr *expr; // Current expression
	bool afterval; // Whether we just parsed a value as opposed to an operator
	struct expr_parser *subp; // Parser for parenthetical sub-expression
};

// Initializes the given expression parser
void expr_parser_init(struct expr_parser*);

// Destroys the given expression parser and releases its resources
void expr_parser_destroy(struct expr_parser*);

// Takes ownership of and parses the given token.
// Returns 0 on success.
// Takes ownership of the token whether it succeeds or not.
int expr_parser_parse(struct expr_parser*, bchar *token);

// Returns whether the expression is complete.
// A null expression is complete.
bool expr_parser_complete(struct expr_parser*);
