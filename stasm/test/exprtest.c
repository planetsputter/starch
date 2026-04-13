// exprtest.c

#include <assert.h>
#include <stdlib.h>

#include "expr.h"
#include "tokenizer.h"

// Parses the given expression string, generating an expression struct.
// Returns zero on success.
// Cannot handle non-ASCII characters.
// The caller is responsible for destroying and deallocating the emitted expression struct.
static int parse_expr(const char *expr, struct expr **e)
{
	// Initialize expression parser
	struct expr_parser p;
	expr_parser_init(&p);

	// Initialize tokenizer
	struct tokenizer tz;
	tokenizer_init(&tz);

	// Parse tokens
	int ret = 0;
	for (int i = 0; ;) {
		bchar *token = NULL;
		do {
			tokenizer_emit(&tz, &token);
			if (token) {
				ret = expr_parser_parse(&p, token);
				if (ret) {
					break;
				}
			}
		} while (token);
		if (ret) {
			break;
		}

		if (expr[i] == '\0') {
			break;
		}
		// Note: We don't handle non-ASCII characters
		char c = expr[i++];
		tokenizer_parse(&tz, c);
		if (expr[i] == '\0') {
			assert(tokenizer_finish(&tz));
		}
	}
	if (!expr_parser_complete(&p)) {
		ret = 1;
	}

	// Destroy tokenizer
	tokenizer_destroy(&tz);

	// Emit any generated expression
	*e = p.expr;
	p.expr = NULL;
	expr_parser_destroy(&p);
	return ret;
}

int main()
{
	// Test acceptance of empty expression
	struct expr *e = NULL;
	int ret = parse_expr("", &e);
	assert(ret == 0 && e == NULL);

	// Test acceptance of a single value
	ret = parse_expr("A", &e);
	assert(ret == 0 && e && e->op_val && bstrcmpc(e->op_val, "A") == 0);
	expr_destroy(e);
	free(e);

	// Test rejection of single operator expression
	ret = parse_expr("*", &e);
	assert(ret != 0 && !e);

	// Test rejection of expression beginning with operator
	ret = parse_expr("*A", &e);
	assert(ret != 0 && !e);

	// Test rejection of incomplete expression
	ret = parse_expr("A*", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		!e->rhs);
	expr_destroy(e);
	free(e);

	// Test rejection of paired values
	ret = parse_expr("A B", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val, "A") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);

	// Test rejection of paired operators
	ret = parse_expr("A*/", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs && !e->rhs);
	expr_destroy(e);
	free(e);

	// Test acceptance of complete expression
	ret = parse_expr("A*B", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		bstrcmpc(e->rhs->op_val, "B") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	// Test rejection of unmatched parenthesis
	ret = parse_expr(")", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("(", &e);
	assert(ret != 0 && !e);

	// Test rejection of parenthesis where operator expected
	ret = parse_expr("A()", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val, "A") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);

	// Test acceptance of expressions with empty parenthesis
	ret = parse_expr("()", &e);
	assert(ret == 0 && e && e->op_val == NULL && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);

	ret = parse_expr("A*()", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		e->rhs && !e->rhs->op_val && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	ret = parse_expr("A*()-B", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val, "-") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val, "*") == 0 && e->lhs->lhs &&
		bstrcmpc(e->lhs->lhs->op_val, "A") == 0 && !e->lhs->lhs->lhs && !e->lhs->lhs->rhs &&
		e->lhs->rhs && !e->lhs->rhs->op_val && !e->lhs->rhs->lhs && !e->lhs->rhs->rhs &&
		e->rhs && bstrcmpc(e->rhs->op_val, "B") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	return 0;
}
