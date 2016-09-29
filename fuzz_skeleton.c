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
    if (argc < 3) {
        fprintf(stderr, "Usage:\n  %s <NIF .so>... <term file>\n\n", argv[0]);
        fprintf(stderr, "The variable Input will be bound to a binary "
                "constructed from stdin before the term file is read.\n");
        return 1;
    }
    niffy_construct_erlang_env();
    niffy_construct_assert_env();
    /* Beware!  In a (well-meaning) attempt to speed up fuzzing,
     * afl-fuzz by default sets LD_BIND_NOW which will override this
     * RTLD_LAZY; if your NIF uses any functions that aren't implement
     * yet, afl-fuzz will complain that your program always crashes.
     * (It's actually SIGABRT'ing but you can't easily see that.)
     *
     * Set LD_BIND_LAZY in your environment before running afl-fuzz,
     * unless you know niffy implements everything your NIF
     * references. */
    for (int i = 1; i < (argc - 1); ++i)
        assert(niffy_load_so(argv[i], RTLD_LAZY, 0));
    FILE *in = fopen(argv[argc - 1], "r");
    assert(in);

    struct lexer lexer;
    lex_init(&lexer);
    void *pParser;

    pParser = ParseAlloc(malloc);

    process(&lexer, pParser, "Input = <<");

    bool is_first = true;
    char buf_in[4096], buf_out[5*sizeof(buf_in)+1];
    size_t len;
    while ((len = fread(buf_in, 1, sizeof(buf_in), stdin))) {
        char *p = buf_in, *q = buf_out;
        if (is_first) {
            q += snprintf(q, 4, "%hhu", (uint8_t)*p++);
            is_first = false;
        }
        while (p < buf_in+len)
            q += snprintf(q, 5, ",%hhu", (uint8_t)*p++);
        *q = 0;
        assert(q < buf_out+sizeof(buf_out));
        process(&lexer, pParser, buf_out);
    }

    process(&lexer, pParser, ">>.\n");

    /* Read the rest from the supplied file */
    char *line = NULL;
    size_t line_len = 0;
    while (-1 != getline(&line, &line_len, in))
        process(&lexer, pParser, line);
    Parse(pParser, 0, (struct token){.type = 0, .location = lexer.location},
          niffy_handle_statement);

    ParseFree(pParser, free);
    niffy_destroy_environments();
    return 0;
}
