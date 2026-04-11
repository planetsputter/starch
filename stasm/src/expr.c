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
		e->lhs = NULL;
	}
	if (e->rhs) {
		expr_destroy(e->rhs);
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
		p->expr = NULL;
	}
}

// @todo: Consolidate with tokenizer.
// These characters are operators or begin operators.
// The list must be in numeric order.
static const char ops[] = "\n!#%&'()*+,-./;<=>?@[\\]^`{|}~";

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
		// @todo: avoid assignment operator
		ret = 4;
		break;
	case '&':
		if (op[1] == '&') ret = 8;
		else ret = 5;
		break;
	case '^':
		ret = 6;
		break;
	case '|':
		if (op[1] == '|') ret = 8;
		else ret = 7;
		break;
	case '!':
		if (op[1] == '=') ret = 4;
		else ret = -1; // @todo: unary
		break;
	default:
		ret = -1;
	}
	return ret;
}

int expr_parser_parse(struct expr_parser *p, bchar *token)
{
	// Note: Here we rely on the fact that all operator tokens begin with characters
	// that have single-byte representation in UTF-8, and that no other tokens will
	// begin with these bytes.
	bool is_op = isop(token[0]);

	int ret = 0;
	if (p->afterval) { // Expect operator
		if (!is_op) {
			ret = 1;
		}
		else {
			// Get operator precedence value
			int pval = prec_val(token);
			assert(pval >= 0);

			// Create expression to store token
			struct expr *e = (struct expr*)malloc(sizeof(struct expr));
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
	// Expect value
	else if (is_op) {
		// @todo: Don't fail for unary operators that modify a single value like '!'
		ret = 1;
	}
	else {
		// Create expression to store token
		struct expr *e = (struct expr*)malloc(sizeof(struct expr));
		expr_init(e, token);
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
