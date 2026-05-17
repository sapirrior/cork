#ifndef LEXER_H
#define LEXER_H

#include "cork.h"

Token lex_line(const char *raw, int lineno);

#endif /* LEXER_H */
