#include <assert.h>
#include <dlfcn.h>
#include <getopt.h>
#include <stdio.h>

#include "niffy.h"
#include "parse_protos.h"

#ifndef NIFFY_VERSION
#define NIFFY_VERSION "0"
#endif


static void print_usage(FILE *out)
{
    fprintf(out, "niffy [OPTION]... <NIF>\n");
    struct { const char *name, *description; } args[] = {
        {"--help", "display this help and exit"},
        {"--lazy", "resolve NIF symbols lazily"},
        {"--quiet", "print less information"},
        {"--verbose", "print more information"},
        {"--version", "output version and exit"},
        {NULL, NULL}
    }, *p;
    for (p = args; p->name; ++p)
        fprintf(out, "\t%-32s%s\n", p->name, p->description);
}


int main(int argc, char **argv)
{
    int option_index = 0, c;
    const struct option long_opts[] = {
        {"help", no_argument, 0, 'h'},
        {"lazy", no_argument, 0, 'l'},
        {"quiet", no_argument, 0, 'q'},
        {"verbose", no_argument, 0, 'v'},
        {"version", no_argument, 0, 'V'},
        {0,0,0,0}
    };
    int rtld_mode = RTLD_NOW;
    int verbosity = 1;

    while (-1 != (c = getopt_long(argc, argv, "hlqvV", long_opts, &option_index))) {
        switch (c) {
        case 'h':
            print_usage(stdout);
            return 0;
        case 'l':
            rtld_mode = RTLD_LAZY;
            break;
        case 'q':
            verbosity = -999;
            break;
        case 'v':
            ++verbosity;
            break;
        case 'V':
            printf("niffy %s\n", NIFFY_VERSION);
            return 0;
        default:
            print_usage(stdout);
            return 1;
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "no NIF specified\n");
        return 1;
    }

    int n_sos = argc-optind;
    assert(n_sos > 0);

    niffy_construct_erlang_env();
    niffy_construct_assert_env();

    for (int so_idx = 0; so_idx < n_sos && optind < argc; ++so_idx, ++optind) {
        if (!niffy_load_so(argv[optind], rtld_mode, verbosity))
            return 1;
    }

    FILE *in = stdin;
    char *line = NULL;
    size_t line_len = 0;
    struct lexer lexer;
    lex_init(&lexer);
    void *pParser;

    pParser = ParseAlloc(malloc);
    ssize_t nread = 0;
    while (-1 != (nread = getline(&line, &line_len, in))) {
        lex_setup_next_line(&lexer, line, nread, feof(in));
        struct token token;

        while (lex(&lexer, &token)) {
            Parse(pParser, token.type, token, niffy_handle_statement);
            /* Slight hack to allow nicer interactive sessions.  We
               don't have nested expressions, so if we see a dot, we
               encourage the parser to do its work eagerly. */
            if (token.type == TOK_DOT)
                Parse(pParser, 0, (struct token){.type = 0, .location = lexer.location}, niffy_handle_statement);
        }
    }

    Parse(pParser, 0, (struct token){.type = 0, .location = lexer.location}, niffy_handle_statement);
    free(line);
    ParseFree(pParser, free);

    niffy_destroy_environments();
    return 0;
}
