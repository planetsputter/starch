// expr.c

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "expr.h"
#include "lits.h"
#include "stmsg.h"

// These characters are operators or begin operators.
// The list must be in numeric order.
static const char ops[] = "\n!%&()*,./;<=>?@[\\]^`{|}";

// Returns whether the given token is an operator.
// Sets *is_unary to whether the operator is a unary operator.
static bool is_op(const bchar *token, bool *is_unary)
{
	// Check for unary operators
	if ((token[0] == '!' && token[1] == '\0') || token[0] == '~') {
		*is_unary = true;
		return true;
	}
	*is_unary = false;

	// Check for sign at beginning of integer literal
	if (token[0] == '+' || token[0] == '-') {
		return !isdigit(token[1]);
	}

	// Use binary search for efficiency
	char c = token[0];
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

int expr_eval(struct expr *e, int64_t *val)
{
	int ret;
	if (!e->op_val) { // Null expression
		if (!e->lhs || e->rhs) {
			stmsgf(SMT_ERROR, "cannot evaluate null expression");
			ret = 1;
		}
		else {
			ret = expr_eval(e->lhs, val);
		}
	}
	else if (e->lhs) { // This must be an operator
		const bchar *op = e->op_val;
		bool is_unary;
		if (!is_op(op, &is_unary)) {
			stmsgf(SMT_ERROR, "expected operator, not \"%s\"", op);
			ret = 1;
		}
		else if (!is_unary && !e->rhs) {
			stmsgf(SMT_ERROR, "expected rhs when evaluating operator \"%s\"", op);
			ret = 1;
		}
		else if (is_unary && e->rhs) {
			stmsgf(SMT_ERROR, "unexpected rhs when evaluating unary operator \"%s\"", op);
			ret = 1;
		}
		else {
			// Evaluate lhs and rhs
			int64_t lv, rv;
			ret = expr_eval(e->lhs, &lv);
			if (!ret && e->rhs) {
				ret = expr_eval(e->rhs, &rv);
			}
			// Combine lhs and rhs using operator
			if (!ret) {
				bool unrec = false;
				switch (*op) {
				case '*':
					*val = lv * rv;
					break;
				case '/':
					*val = lv / rv;
					break;
				case '%':
					*val = lv % rv;
					break;
				case '+':
					*val = lv + rv;
					break;
				case '-':
					*val = lv - rv;
					break;
				case '<':
					op++;
					if (*op == '<') *val = lv << rv;
					else if (*op == '=') *val = lv <= rv;
					else {
						op--;
						*val = lv < rv;
					}
					break;
				case '>':
					op++;
					if (*op == '>') *val = lv >> rv;
					else if (*op == '=') *val = lv >= rv;
					else {
						op--;
						*val = lv > rv;
					}
					break;
				case '=':
					op++;
					if (*op == '=') *val = lv == rv;
					else {
						op--;
						unrec = true; // Don't know how to handle assignment operator
					}
					break;
				case '&':
					op++;
					if (*op == '&') *val = lv && rv;
					else {
						op--;
						*val = lv & rv;
					}
					break;
				case '^':
					*val = lv ^ rv;
					break;
				case '|':
					op++;
					if (*op == '|') *val = lv || rv;
					else {
						op--;
						*val = lv | rv;
					}
					break;
				case '!':
					op++;
					if (*op == '=') *val = lv != rv;
					else {
						op--;
						assert(is_unary);
						*val = !lv;
					}
					break;
				case '~':
					assert(is_unary);
					*val = ~lv;
					break;
				case ',':
					*val = rv;
					break;
				default:
					unrec = true;
				}
				assert(*(++op) == '\0');
				if (unrec) {
					stmsgf(SMT_ERROR, "unrecognized operator \"%s\"", op);
					ret = 1;
				}
			}
		}
	}
	else { // This must be a value
		if (e->rhs) {
			stmsgf(SMT_ERROR, "unexpected rhs when evaluating expression");
			ret = 1;
		}
		else if (!parse_int(e->op_val, val)) {
			stmsgf(SMT_ERROR, "failed to parse integer literal \"%s\"", e->op_val);
			ret = 1;
		}
		else {
			ret = 0;
		}
	}
	return ret;
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
		else ret = -1; // Unary
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
	bool is_unary;
	bool this_op = is_op(token, &is_unary);
	if (p->afterval) { // Expect operator
		if (!this_op || is_unary || token[0] == '(') {
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
	// Expect value or unary operator
	else if (this_op && !is_unary && token[0] != ')') {
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
			// See if rightmost is unary
			bool isrmostu;
			assert(is_op(rmost->op_val, &isrmostu));
			if (isrmostu) { // If it is, append to leftmost from rightmost
				struct expr *lmfr;
				for (lmfr = rmost; lmfr->lhs; lmfr = lmfr->lhs);
				lmfr->lhs = e;
			}
			else {
				rmost->rhs = e; // Append value to rightmost
			}
		}
		p->afterval = !is_unary;
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
