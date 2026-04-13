// expr.c

#include <assert.h>
#include <stdlib.h>

#include "expr.h"
#include "stmsg.h"

void expr_init(struct expr *e, bchar *op_val)
{
	e->lhs = NULL;
	e->rhs = NULL;
	e->op_val = op_val;
}

void expr_destroy(struct expr *e)
{
	if (e->op_val) {
		bfree(e->op_val);
		e->op_val = NULL;
	}
	if (e->lhs) {
		expr_destroy(e->lhs);
		free(e->lhs);
		e->lhs = NULL;
	}
	if (e->rhs) {
		expr_destroy(e->rhs);
		free(e->rhs);
		e->rhs = NULL;
	}
}

void expr_parser_init(struct expr_parser *p)
{
	p->expr = NULL;
	p->afterval = false;
	p->subp = NULL;
}

void expr_parser_destroy(struct expr_parser *p)
{
	if (p->expr) {
		expr_destroy(p->expr);
		free(p->expr);
		p->expr = NULL;
	}
	if (p->subp) {
		expr_parser_destroy(p->subp);
		free(p->subp);
		p->subp = NULL;
	}
}

// @todo: Consolidate with tokenizer.
// These characters are operators or begin operators.
// The list must be in numeric order.
static const char ops[] = "\n!%&()*+,-./;<=>?@[\\]^`{|}~";

// Returns whether the given character is in the above list
static bool isop(ucp c)
{
	// Use binary search for efficiency
	int low = 0, high = sizeof(ops) - 2, mid;
	while (low <= high) {
		mid = (low + high) / 2;
		int comp = c - ops[mid];
		if (comp < 0) {
			high = mid - 1;
		}
		else if (comp > 0) {
			low = mid + 1;
		}
		else {
			return true;
		}
	}
	return false;
}

// Return the precedence value of the given operator.
// Lower return values mean higher precedence.
// A non-operator has higher precedence than any operator.
static int prec_val(const bchar *op)
{
	int ret;
	switch (op[0]) {
	case '*':
	case '/':
	case '%':
		ret = 0;
		break;
	case '+':
	case '-':
		ret = 1;
		break;
	case '<':
		if (op[1] == '<') ret = 2;
		else ret = 3;
		break;
	case '>':
		if (op[1] == '>') ret = 2;
		else ret = 3;
		break;
	case '=':
		if (op[1] == '=') ret = 4;
		else ret = 10;
		break;
	case '&':
		if (op[1] == '&') ret = 8;
		else ret = 5;
		break;
	case '^':
		ret = 6;
		break;
	case '|':
		if (op[1] == '|') ret = 9;
		else ret = 7;
		break;
	case '!':
		if (op[1] == '=') ret = 4;
		else ret = -1; // @todo: unary
		break;
	case ',':
		ret = 11;
		break;
	default:
		ret = -1;
	}
	return ret;
}

int expr_parser_parse(struct expr_parser *p, bchar *token)
{
	// @todo: Use line and character numbers for error messages
	struct expr *e = NULL;
	int ret = 0;
	if (token[0] == ')') {
		if (!p->subp) {
			stmsgf(SMT_ERROR, "unmatched \")\"");
			ret = 1;
		}
		else if (!p->subp->subp) { // This finishes p->subp
			if (!expr_parser_complete(p->subp)) {
				stmsgf(SMT_ERROR, "incomplete expression");
				ret = 1;
			}
			else {
				// Insert p->subp->expr into p->expr
				// Create expression to store token
				e = (struct expr*)malloc(sizeof(struct expr));
				expr_init(e, NULL);
				e->lhs = p->subp->expr;
				p->subp->expr = NULL;
				expr_parser_destroy(p->subp);
				free(p->subp);
				p->subp = NULL;
			}
		}
	}
	if (ret) {
		bfree(token);
		return ret;
	}
	if (p->subp) { // Active subexpression
		return expr_parser_parse(p->subp, token);
	}

	// Note: Here we rely on the fact that all operator tokens begin with characters
	// that have single-byte representation in UTF-8, and that no other tokens will
	// begin with these bytes.
	bool is_op = isop(token[0]);

	if (p->afterval) { // Expect operator
		if (!is_op || token[0] == '(') {
			stmsgf(SMT_ERROR, "expected operator, not \"%s\"", token);
			ret = 1;
		}
		else {
			// Get operator precedence value
			int pval = prec_val(token);
			assert(pval >= 0);

			// Create expression to store token
			e = (struct expr*)malloc(sizeof(struct expr));
			expr_init(e, token);

			// Insert operator into tree according to precedence
			struct expr **root = &p->expr;
			assert(*root);
			while ((*root)->op_val && prec_val((*root)->op_val) > pval) {
				root = &(*root)->rhs;
				assert(*root);
			}
			e->lhs = *root;
			*root = e;
			p->afterval = false;
		}
	}
	else if (token[0] == '(') {
		p->subp = (struct expr_parser*)malloc(sizeof(struct expr_parser));
		expr_parser_init(p->subp);
		bfree(token);
	}
	// Expect value
	else if (is_op && token[0] != ')') {
		// @todo: Don't fail for unary operators that modify a single value like '!'
		stmsgf(SMT_ERROR, "expected value, not \"%s\"", token);
		ret = 1;
	}
	else {
		if (!e) { // Create expression to store token
			e = (struct expr*)malloc(sizeof(struct expr));
			expr_init(e, token);
		}
		else { // Expression already created, free token
			bfree(token);
		}
		if (!p->expr) { // First value in expression
			p->expr = e;
		}
		else {
			struct expr *rmost; // Find rightmost operator
			for (rmost = p->expr; rmost->rhs; rmost = rmost->rhs);
			rmost->rhs = e; // Append value to rightmost
		}
		p->afterval = true;
	}

	if (ret) {
		bfree(token);
	}
	return ret;
}

bool expr_parser_complete(struct expr_parser *p)
{
	// @todo: Check for unfinished sub-expressions
	bool complete = !p->subp && (!p->expr || p->afterval);
	if (!complete) {
		stmsgf(SMT_ERROR, "incomplete expression");
	}
	return complete;
}
