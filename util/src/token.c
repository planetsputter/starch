// token.c

#include <stdlib.h>

#include "token.h"

struct token *token_alloc(bchar *str, int lineno, int charno)
{
	struct token *t = (struct token*)malloc(sizeof(struct token));
	t->str = str;
	t->lineno = lineno;
	t->charno = charno;
	return t;
}

void token_free(struct token *t)
{
	bfree(t->str);
	free(t);
}

struct token *token_dup(struct token *t)
{
	return token_alloc(bstrdupb(t->str), t->lineno, t->charno);
}
