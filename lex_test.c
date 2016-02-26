#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lex.h"


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
            pretty_print_token(stdout, &token);
            fputc('\n', stdout);
            destroy_token(&token);
        }
    }
    free(line);
}

