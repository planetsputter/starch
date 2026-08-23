// expr.c

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "expr.h"
#include "lits.h"
#include "stmsg.h"

// Returns whether the given token begins a group
static bool begins_group(const bchar *token)
{
	return token[0] == '(' || token[0] == '[' || token[0] == '{';
}

// Returns whether the given token ends a group
static bool ends_group(const bchar *token)
{
	return token[0] == ')' || token[0] == ']' || token[0] == '}';
}

enum { // Unary status values
	NOT_UNARY,
	MAY_BE_UNARY,
	MUST_BE_UNARY,
};

// Returns whether the given token is an operator.
// Sets *unary to one of the enumerated unary status values.
static bool is_op(const bchar *token, int *unary)
{
	*unary = NOT_UNARY;

	// We rely on the input string being encoded with UTF-8 and check the first byte.
	// Quoted strings are not operators.
	char fb = token[0];
	if (fb >= 0x7f || fb <= ' ' || fb == '"' || fb == '\'') return false;

	// Check for unary operators
	if ((fb == '!' && token[1] == '\0') || fb == '~' ||
		begins_group(token) || ends_group(token)) {
		*unary = MUST_BE_UNARY;
		return true;
	}

	// Check for sign at beginning of integer literal
	if (fb == '+' || fb == '-') {
		if (isdigit(token[1])) { // Integer literal
			return false;
		}
		if (token[1] == fb) { // "++" or "--"
			*unary = MUST_BE_UNARY;
		}
		else {
			*unary = MAY_BE_UNARY;
		}
		return true;
	}

	// Any token beginning with a printable character that is non-alphanumeric
	// and not an underscore, dollar sign, colon, or whitespace is considered an operator.
	// This approach works because the tokenizer recognizes operators in a similar way.
	return !isalnum(fb) && fb != '_' && fb != '$' && fb != ':';
}

struct expr *expr_new(struct token *op_val)
{
	struct expr *e = (struct expr*)malloc(sizeof(struct expr));
	e->lhs = NULL;
	e->rhs = NULL;
	e->op_val = op_val;
	return e;
}

struct expr *expr_dup(struct expr *src)
{
	struct expr *dup = expr_new(token_dup(src->op_val));
	dup->lhs = src->lhs ? expr_dup(src->lhs) : NULL;
	dup->rhs = src->rhs ? expr_dup(src->rhs) : NULL;
	return dup;
}

void expr_destroy(struct expr *e)
{
	if (e->op_val) {
		token_free(e->op_val);
		e->op_val = NULL;
	}
	if (e->lhs) {
		expr_delete(e->lhs);
		e->lhs = NULL;
	}
	if (e->rhs) {
		expr_delete(e->rhs);
		e->rhs = NULL;
	}
}

void expr_delete(struct expr *e)
{
	expr_destroy(e);
	free(e);
}

int expr_eval(const struct expr *e, int64_t *val, bool print_err, void *userptr, expr_lookup_func f)
{
	int ret;
	if (!e->op_val) { // Null expression
		if (print_err) stmsgf(SMT_ERROR, "cannot evaluate null expression");
		ret = 1;
	}
	else if (e->lhs) { // This must be an operator
		const bchar *op = e->op_val->str;
		int unary;
		if (!is_op(op, &unary)) {
			if (print_err) stmsgtf(SMT_ERROR, e->op_val->lineno, e->op_val->charno,
				"expected operator, not \"%s\"", op);
			ret = 1;
		}
		else if (unary == NOT_UNARY && !e->rhs) {
			if (print_err) stmsgtf(SMT_ERROR, e->op_val->lineno, e->op_val->charno,
				"expected rhs when evaluating operator \"%s\"", op);
			ret = 1;
		}
		else if (unary == MUST_BE_UNARY && e->rhs) {
			if (print_err) stmsgtf(SMT_ERROR, e->op_val->lineno, e->op_val->charno,
				"unexpected rhs when evaluating unary operator \"%s\"", op);
			ret = 1;
		}
		else {
			// Evaluate lhs and rhs
			int64_t lv, rv = 0;
			ret = expr_eval(e->lhs, &lv, print_err, userptr, f);
			if (!ret && e->rhs) {
				ret = expr_eval(e->rhs, &rv, print_err, userptr, f);
			}
			// Combine lhs and rhs using operator
			if (!ret) {
				bool eval = can_eval_op(op);
				switch (*(op++)) {
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
					if (*op == '+') {
						op++;
					}
					else {
						*val = lv + rv;
					}
					break;
				case '-':
					if (*op == '-') {
						op++;
					}
					else if (!e->rhs) { // Unary "-"
						*val = -lv;
					}
					else {
						*val = lv - rv;
					}
					break;
				case '<':
					if (*op == '<') {
						op++;
						*val = lv << rv;
					}
					else if (*op == '=') {
						op++;
						*val = lv <= rv;
					}
					else {
						*val = lv < rv;
					}
					break;
				case '>':
					if (*op == '>') {
						op++;
						*val = lv >> rv;
					}
					else if (*op == '=') {
						op++;
						*val = lv >= rv;
					}
					else {
						*val = lv > rv;
					}
					break;
				case '=':
					if (*op == '=') {
						op++;
						*val = lv == rv;
					}
					break;
				case '&':
					if (*op == '&') {
						op++;
						*val = lv && rv;
					}
					else {
						*val = lv & rv;
					}
					break;
				case '^':
					*val = lv ^ rv;
					break;
				case '|':
					if (*op == '|') {
						op++;
						*val = lv || rv;
					}
					else {
						*val = lv | rv;
					}
					break;
				case '!':
					if (*op == '=') {
						op++;
						*val = lv != rv;
					}
					else {
						*val = !lv;
					}
					break;
				case '~':
					*val = ~lv;
					break;
				case '(': // Parenthetical group
					*val = lv;
					break;
					// We deliberately don't evaluate "," because it is likely intended
					// to separate arguments rather than to act as an operator.
					// Fall-through.
				default:
					assert(!eval);
				}
				assert(*op == '\0');
				if (!eval) {
					if (print_err) stmsgtf(SMT_ERROR, e->op_val->lineno, e->op_val->charno,
						"unrecognized operator \"%s\"", e->op_val->str);
					ret = 1;
				}
			}
		}
	}
	else { // This must be a value
		if (e->rhs) {
			if (print_err) stmsgtf(SMT_ERROR, e->op_val->lineno, e->op_val->charno,
				"unexpected rhs when evaluating expression");
			ret = 1;
		}
		else if (!parse_int(e->op_val->str, val) && (!f || !f(userptr, e->op_val, val))) {
			if (print_err) stmsgtf(SMT_ERROR, e->op_val->lineno, e->op_val->charno,
				"failed to parse term \"%s\"", e->op_val->str);
			ret = 1;
		}
		else {
			ret = 0;
		}
	}
	return ret;
}

void *expr_iter(struct expr *e, void *user_ptr, expr_iter_func f)
{
	void *ret;
	if (!e) {
		ret = NULL;
	}
	else if ((ret = f(user_ptr, e->op_val))) { }
	else if ((ret = expr_iter(e->lhs, user_ptr, f))) { }
	else if ((ret = expr_iter(e->rhs, user_ptr, f))) { }
	else {
		ret = NULL;
	}
	return ret;
}

bool can_eval_op(const bchar *op)
{
	bool eval;
	switch (*(op++)) {
	case '*':
	case '/':
	case '%':
		eval = true;
		break;
	case '+':
		if (*op == '+') {
			eval = false; // Don't know how to handle "++"
		}
		else {
			eval = true;
		}
		break;
	case '-':
		if (*op == '-') {
			eval = false; // Don't know how to handle "--"
		}
		else {
			eval = true;
		}
		break;
	case '<':
	case '>':
		eval = true;
		break;
	case '=':
		if (*op == '=') {
			eval = true;
		}
		else {
			eval = false; // Don't know how to handle assignment operator
		}
		break;
	case '&':
	case '^':
	case '|':
	case '!':
	case '~':
	case '(':
		eval = true;
		break;
	default:
		eval = false;
	}
	return eval;
}

void expr_parser_init(struct expr_parser *p, struct token *group_char)
{
	p->expr = NULL;
	p->afterval = false;
	p->subp = NULL;
	p->group_char = group_char;
}

void expr_parser_destroy(struct expr_parser *p)
{
	if (p->expr) {
		expr_delete(p->expr);
		p->expr = NULL;
	}
	if (p->subp) {
		expr_parser_destroy(p->subp);
		free(p->subp);
		p->subp = NULL;
	}
	if (p->group_char) {
		token_free(p->group_char);
		p->group_char = NULL;
	}
}

// Return the precedence of the given operator if it occurs after a value.
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
	case ',':
		ret = 11;
		break;
	case '!':
		if (op[1] == '=') {
			ret = 4;
			break;
		}
		// Fall-through
	case '~':
	default:
		assert(false); // Unary should not occur after value
	}
	return ret;
}

int expr_parser_parse(struct expr_parser *p, struct token *token)
{
	const bchar *ts = token->str;
	struct expr *e = NULL;
	int ret = 0;
	if (ends_group(ts)) {
		if (!p->subp) {
			stmsgtf(SMT_ERROR, token->lineno, token->charno, "unmatched \"%c\"", ts[0]);
			ret = 1;
		}
		else if (!p->subp->subp) { // This finishes p->subp
			const bchar *group_name = p->subp->group_char->str;
			if ((group_name[0] == '(' && ts[0] != ')') ||
				(group_name[0] == '[' && ts[0] != ']') ||
				(group_name[0] == '{' && ts[0] != '}')) {
				stmsgtf(SMT_ERROR, token->lineno, token->charno, "unmatched \"%c\"", ts[0]);
				ret = 1;
			}
			else if (!expr_parser_complete(p->subp)) {
				stmsgtf(SMT_ERROR, token->lineno, token->charno, "incomplete expression");
				ret = 1;
			}
			else {
				// Insert p->subp->expr into p->expr
				// Create expression to store token
				assert(e == NULL);
				e = expr_new(NULL);
				e->lhs = p->subp->expr;
				p->subp->expr = NULL;
				e->op_val = p->subp->group_char;
				p->subp->group_char = NULL;
				expr_parser_destroy(p->subp);
				free(p->subp);
				p->subp = NULL;
			}
		}
	}
	if (ret) {
		token_free(token);
		return ret;
	}
	if (p->subp) { // Active subexpression
		return expr_parser_parse(p->subp, token);
	}

	// Note: Here we rely on the fact that all operator tokens begin with characters
	// that have single-byte representation in UTF-8, and that no other tokens will
	// begin with these bytes.
	int unary;
	bool this_op = is_op(ts, &unary);
	if (p->afterval) { // Expect operator
		if (!this_op || unary == MUST_BE_UNARY) {
			stmsgtf(SMT_ERROR, token->lineno, token->charno, "expected %soperator, not \"%s\"",
				!this_op ? "" : "binary ", ts);
			ret = 1;
		}
		else {
			// Get operator precedence value
			int pval = prec_val(ts);

			// Create expression to store token
			assert(e == NULL);
			e = expr_new(token);

			// Insert operator into tree according to precedence
			struct expr **root = &p->expr;
			assert(*root);
			while ((*root)->op_val && (*root)->rhs && prec_val((*root)->op_val->str) > pval) {
				root = &(*root)->rhs;
			}
			e->lhs = *root;
			*root = e;
			p->afterval = false;
		}
	}
	else if (begins_group(ts)) {
		p->subp = (struct expr_parser*)malloc(sizeof(struct expr_parser));
		expr_parser_init(p->subp, token);
	}
	// Expect value or unary operator
	else if (this_op && unary == NOT_UNARY) {
		stmsgtf(SMT_ERROR, token->lineno, token->charno, "expected value, not \"%s\"", ts);
		ret = 1;
	}
	else {
		p->afterval = !this_op || ends_group(ts);
		if (!e) { // Create expression to store token
			e = expr_new(token);
		}
		else { // Expression already created, free token
			token_free(token);
		}
		if (!p->expr) { // First value in expression
			p->expr = e;
		}
		else {
			struct expr *rmost; // Find rightmost operator
			for (rmost = p->expr; rmost->rhs; rmost = rmost->rhs);
			// See if rightmost is unary
			int rmostu;
			assert(is_op(rmost->op_val->str, &rmostu));
			if (rmostu == NOT_UNARY) { // Rightmost is not unary
				rmost->rhs = e; // Append value to rightmost
			}
			else {
				// Rightmost might be unary. It depends on leftmost from rightmost.
				struct expr *lmost;
				for (lmost = rmost; lmost->lhs; lmost = lmost->lhs);
				if (rmostu == MAY_BE_UNARY && !is_op(lmost->op_val->str, &rmostu)) {
					// Rightmost is not really unary
					rmost->rhs = e;
				}
				else { // Rightmost is unary, append to leftmost from rightmost
					lmost->lhs = e;
				}
			}
		}
	}

	if (ret) {
		token_free(token);
	}
	return ret;
}

bool expr_parser_complete(struct expr_parser *p)
{
	// Check that current expression is finished (after a value)
	// and that no subexpression is in progress.
	return !p->subp && (!p->expr || p->afterval);
}
