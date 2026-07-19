// exprtest.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

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
	expr_parser_init(&p, NULL);

	// Initialize tokenizer
	struct tokenizer tz;
	tokenizer_init(&tz);

	// Parse tokens
	int ret = 0;
	for (int i = 0; ;) {
		struct token *token;
		do {
			token = tokenizer_emit(&tz);
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
		tokenizer_parse(&tz, c, 1, i);
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

int64_t A = 0, B = 0, C = 0, D = 0, E = 0;

// Lookup function for evaluation of non-literal values
bool nonlit_lookup(void *userptr, bchar *strval, int64_t *intval)
{
	assert(userptr == NULL);
	if (bstrcmpc(strval, "A") == 0) {
		*intval = A;
	}
	else if (bstrcmpc(strval, "B") == 0) {
		*intval = B;
	}
	else if (bstrcmpc(strval, "C") == 0) {
		*intval = C;
	}
	else if (bstrcmpc(strval, "D") == 0) {
		*intval = D;
	}
	else if (bstrcmpc(strval, "E") == 0) {
		*intval = E;
	}
	else {
		return false;
	}
	return true;
}

// Asserts that the expression evaluates to the given value
void test_expr(const char *expr, int64_t val)
{
	struct expr *e = NULL;
	int ret = parse_expr(expr, &e);
	assert(ret == 0);
	int64_t test_val;
	ret = expr_eval(e, &test_val, NULL, nonlit_lookup);
	assert(ret == 0 && test_val == val);
	expr_destroy(e);
	free(e);
}

// Return a random 64-bit integer, weighted toward lower absolute values
int64_t lograndi64()
{
	int64_t i = ((int64_t)random() << 32) ^ random();
	int s = random() & 63;
	return i >> s;
}

int main()
{
	// Test acceptance of empty expression
	struct expr *e = NULL;
	int ret = parse_expr("", &e);
	assert(ret == 0 && e == NULL);

	// Test acceptance of a single value
	ret = parse_expr("A", &e);
	assert(ret == 0 && e && e->op_val && bstrcmpc(e->op_val->str, "A") == 0);
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
	assert(ret != 0 && e && bstrcmpc(e->op_val->str, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		!e->rhs);
	expr_destroy(e);
	free(e);

	// Test rejection of paired values
	ret = parse_expr("A B", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val->str, "A") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);

	// Test rejection of paired operators
	ret = parse_expr("A*/", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val->str, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs && !e->rhs);
	expr_destroy(e);
	free(e);

	// Test acceptance of complete expression
	ret = parse_expr("A*B", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		bstrcmpc(e->rhs->op_val->str, "B") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	// Test rejection of unmatched grouping characters
	ret = parse_expr(")", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("(", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("[", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("]", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("{", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("}", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("{]", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("[)", &e);
	assert(ret != 0 && !e);
	ret = parse_expr("(}", &e);
	assert(ret != 0 && !e);

	// Test rejection of groups where operator expected
	ret = parse_expr("A()", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val->str, "A") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);
	ret = parse_expr("A[]", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val->str, "A") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);
	ret = parse_expr("A{}", &e);
	assert(ret != 0 && e && bstrcmpc(e->op_val->str, "A") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);

	// Test acceptance of expressions with empty groups
	ret = parse_expr("()", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "(") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);
	ret = parse_expr("[]", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "[") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);
	ret = parse_expr("{}", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "{") == 0 && !e->lhs && !e->rhs);
	expr_destroy(e);
	free(e);

	ret = parse_expr("A*()", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "A") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		e->rhs && bstrcmpc(e->rhs->op_val->str, "(") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	ret = parse_expr("A*()-B", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "-") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "*") == 0 && e->lhs->lhs &&
		bstrcmpc(e->lhs->lhs->op_val->str, "A") == 0 && !e->lhs->lhs->lhs && !e->lhs->lhs->rhs &&
		e->lhs->rhs && bstrcmpc(e->lhs->rhs->op_val->str, "(") == 0 && !e->lhs->rhs->lhs && !e->lhs->rhs->rhs &&
		e->rhs && bstrcmpc(e->rhs->op_val->str, "B") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	// Test treatment of quoted strings as values
	ret = parse_expr("\"hello\"+ 1", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "+") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "\"hello\"") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		e->rhs && bstrcmpc(e->rhs->op_val->str, "1") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	ret = parse_expr("'h'+ 1", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "+") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "'h'") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		e->rhs && bstrcmpc(e->rhs->op_val->str, "1") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	// Test treatment of symbols as values
	ret = parse_expr("$sym*2", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "*") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, "$sym") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		e->rhs && bstrcmpc(e->rhs->op_val->str, "2") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	// Test treatment of labels as values
	ret = parse_expr(":lbl/3", &e);
	assert(ret == 0 && e && bstrcmpc(e->op_val->str, "/") == 0 && e->lhs &&
		bstrcmpc(e->lhs->op_val->str, ":lbl") == 0 && !e->lhs->lhs && !e->lhs->rhs &&
		e->rhs && bstrcmpc(e->rhs->op_val->str, "3") == 0 && !e->rhs->lhs && !e->rhs->rhs);
	expr_destroy(e);
	free(e);

	// Get current time
	struct timeval tv;
	ret = gettimeofday(&tv, NULL);
	if (ret != 0) {
		fprintf(stderr, "error: failed to get current time\n");
		return 1;
	}

	// Seed random number generator with current time in microseconds
	srandom(tv.tv_usec + tv.tv_sec * 1000000);

	enum { TEST_ITER = 500 };
	for (int i = 0; i < TEST_ITER; i++) {
		// Generate random numbers
		A = lograndi64();
		B = lograndi64();
		C = lograndi64();
		D = random() & 63; // For bit shifts
		E = random() & 63; // For bit shifts

		// Test evaluation of expressions
		// Test ! to * precedence
		test_expr("!A * B", !A * B);
		test_expr("A * !B", A * !B);
		// Test * to / precedence
		if (C != 0) { // Avoid division by zero
			test_expr("A * B / C", A * B / C);
		}
		if (B != 0) { // Avoid division by zero
			test_expr("A / B * C", A / B * C);
		}
		// Test / associativity
		if (B != 0 && C != 0) {
			test_expr("A / B / C", A / B / C);
		}
		// Test * to % precedence
		if (C != 0) { // Avoid division by zero
			test_expr("A * B % C", A * B % C);
		}
		if (B != 0) { // Avoid division by zero
			test_expr("A % B * C", A % B * C);
		}
		// Test % associativity
		if (B != 0 && C != 0) {
			test_expr("A % B % C", A % B % C);
		}
		// Test * to + precedence
		test_expr("A * B + C", A * B + C);
		test_expr("A + B * C", A + B * C);
		// Test + to - precedence
		test_expr("A + B - C", A + B - C);
		test_expr("A - B + C", A - B + C);
		// Test - associativity
		test_expr("A - B - C", A - B - C);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
		// Test + to << precedence
		test_expr("A + B << D", A + B << D);
		test_expr("A << D + E", A << D + E);
		// Test << associativity
		test_expr("A << D << E", A << D << E);
		// Test << to >> precedence
		test_expr("A << D >> E", A << D >> E);
		test_expr("A >> D << E", A >> D << E);
		// Test >> associativity
		test_expr("A >> D >> E", A >> D >> E);
		// Test << to < precedence
		test_expr("A << B < C", A << B < C);
		test_expr("A < B << D", A < B << D);
		// Test < associativity
		test_expr("A < B < C", A < B < C);
		// Test < to > precedence
		test_expr("A < B > C", A < B > C);
		test_expr("A > B < C", A > B < C);
		// Test > associativity
		test_expr("A > B > C", A > B > C);
		// Test < to & precedence
		test_expr("A < B & C", A < B & C);
		test_expr("A & B < C", A & B < C);
		// Test & to ^ precedence
		test_expr("A & B ^ C", A & B ^ C);
		test_expr("A ^ B & C", A ^ B & C);
		// Test ^ to | precedence
		test_expr("A ^ B | C", A ^ B | C);
		test_expr("A | B ^ C", A | B ^ C);
		// Test | to && precedence
		test_expr("A | B && C", A | B && C);
		test_expr("A && B | C", A && B | C);
		// Test && to || precedence
		test_expr("A && B || C", A && B || C);
		test_expr("A || B && C", A || B && C);
		// Test || to , precedence
#pragma GCC diagnostic ignored "-Wunused-value"
		test_expr("A || B , C", (A || B , C));
		test_expr("A , B || C", (A , B || C));
#pragma GCC diagnostic pop
	}

	return 0;
}
