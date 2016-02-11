#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lex.h"

static bool print_token(void *data, struct token *tk)
{
    FILE *out = data;
    const struct str *name;

    switch (tk->type) {
    case TOK_NOTHING: fprintf(out, "NOTHING"); break;
    case TOK_CHAR: fprintf(out, "CHAR(%c)", tk->char_value); break;
    case TOK_FLOAT: fprintf(out, "FLOAT(%g)", tk->float_value); break;
    case TOK_INTEGER: fprintf(out, "INTEGER(%ld)", tk->int64_value); break;
    case TOK_STRING: fprintf(out, "STRING(%.*s)",
                             (int)tk->string_value->len, tk->string_value->data); break;
    case TOK_ATOM:
        name = symbol_name(tk->atom_value);
        fprintf(out, "ATOM(%u :%.*s)", tk->atom_value, (int)name->len, name->data);
        break;
    case TOK_VARIABLE:
        name = symbol_name(tk->atom_value);
        fprintf(out, "VARIABLE(%.*s)", (int)name->len, name->data);
        break;
    case TOK_LPAREN: fprintf(out, "LPAREN"); break;
    case TOK_RPAREN: fprintf(out, "RPAREN"); break;
    case TOK_LBRACE: fprintf(out, "LBRACE"); break;
    case TOK_RBRACE: fprintf(out, "RBRACE"); break;
    case TOK_LBRACKET: fprintf(out, "LBRACKET"); break;
    case TOK_RBRACKET: fprintf(out, "RBRACKET"); break;
    case TOK_LBIN: fprintf(out, "LBIN"); break;
    case TOK_RBIN: fprintf(out, "RBIN"); break;
    case TOK_COMMA: fprintf(out, "COMMA"); break;
    case TOK_COLON: fprintf(out, "COLON"); break;
    case TOK_DOT: fprintf(out, "DOT"); break;
    case TOK_PIPE: fprintf(out, "PIPE"); break;
    case TOK_SLASH: fprintf(out, "SLASH"); break;
    case TOK_HYPHEN: fprintf(out, "HYPHEN"); break;
    case TOK_EQUALS: fprintf(out, "EQUALS"); break;
    default: fprintf(out, "unknown"); break;
    }
    fputc('\n', out);
    return true;
}

/* read terms from stdin, print token information to stdout */
int main(void)
{
    FILE *in = stdin;
    char *line = NULL;
    size_t line_len = 0;

    struct lexer lexer;
    lex_init(&lexer);

    while (-1 != getline(&line, &line_len, in)) {
        lex_setup_next_line(&lexer, line, feof(in));
        struct token token;

        while (lex(&lexer, &token)) {
            print_token(stdout, &token);
        }
    }
    free(line);
}

