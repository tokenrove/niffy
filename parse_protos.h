#pragma once

#include "ast.h"
#include "lex.h"

extern void *ParseAlloc(void *(*allocProc)(size_t));
extern void Parse(void *, int, struct token, void (*)(struct statement *));
extern void ParseFree(void *, void(*freeProc)(void *));
