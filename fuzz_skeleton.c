#include <assert.h>
#include <dlfcn.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "niffy.h"
#include "lex.h"
#include "parse_protos.h"


static void process(struct lexer *lexer, void *parser, char *input)
{
    lex_setup_next_line(lexer, input, strlen(input), true);
    struct token token;
    while (lex(lexer, &token))
        Parse(parser, token.type, token, niffy_handle_statement);
}


int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage:\n  %s <NIF .so> <term file>\n\n", argv[0]);
        fprintf(stderr, "The variable Input will be bound to a binary "
                "constructed from stdin before the term file is read.\n");
        return 1;
    }
    niffy_construct_erlang_env();
    /* Beware!  In a (well-meaning) attempt to speed up fuzzing,
     * afl-fuzz by default sets LD_BIND_NOW which will override this
     * RTLD_LAZY; if your NIF uses any functions that aren't implement
     * yet, afl-fuzz will complain that your program always crashes.
     * (It's actually SIGABRT'ing but you can't easily see that.)
     *
     * Set LD_BIND_LAZY in your environment before running afl-fuzz,
     * unless you know niffy implements everything your NIF
     * references. */
    assert(niffy_load_so(argv[1], RTLD_LAZY, 0));
    FILE *in = fopen(argv[2], "r");
    assert(in);

    char *line = NULL;
    size_t line_len = 0;
    struct lexer lexer;
    lex_init(&lexer);
    void *pParser;

    pParser = ParseAlloc(malloc);

    process(&lexer, pParser, "Input = <<");

    ssize_t cnt;
    bool is_first = true;
    while (-1 != (cnt = getline(&line, &line_len, stdin))) {
        char *tmp = malloc(cnt*4+1), *p = tmp;
        if (is_first) {
            p += snprintf(p, 4, "%hhu", (uint8_t)line[0]);
            is_first = false;
        }
        for (ssize_t i = 1; i < cnt; ++i)
            p += snprintf(p, 5, ",%hhu", (uint8_t)line[i]);
        *p = 0;
        process(&lexer, pParser, tmp);
        free(tmp);
    }

    process(&lexer, pParser, ">>.\n");

    /* Read the rest from the supplied file */
    while (-1 != getline(&line, &line_len, in))
        process(&lexer, pParser, line);
    Parse(pParser, 0, (struct token){.type = 0, .location = lexer.location},
          niffy_handle_statement);

    ParseFree(pParser, free);
    niffy_destroy_environments();
    return 0;
}
