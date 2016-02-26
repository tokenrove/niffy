#pragma once

#include <stdbool.h>

#include "atom.h"
#include "str.h"
#include "parse.h"

#define TOK_NOTHING 0

struct location {
    int line_num;
};

struct token {
    int type;
    union {
        char char_value;
        int64_t int64_value;
        double float_value;
        struct str *string_value;
        atom atom_value;
    };
    struct location location;
};

struct lexer {                  /* private */
    int cs, act;
    char *p, *pe, *eof;
    char *ts, *te;
    struct location location;
    int radix;
    struct {
        bool negate_p;
        int64_t value;
    } i;
};

#include <stdbool.h>

extern void lex_init(struct lexer *);
extern void lex_setup_next_line(struct lexer *, char *, bool);
extern bool lex(struct lexer *, struct token *);
extern void destroy_token(struct token *);
extern bool pretty_print_token(void *, struct token *);
