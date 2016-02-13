
#include <assert.h>
#include <dlfcn.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "erl_nif.h"

#include "ast.h"
#include "lex.h"
#include "macrology.h"
#include "nif_stubs.h"
#include "parse_protos.h"
#include "variable.h"

#ifndef NIFFY_VERSION
#define NIFFY_VERSION "0"
#endif


static struct atom_ptr_map modules;

struct fptr {
    unsigned arity;
    term (*fptr)(ErlNifEnv *env, int argc, const term argv[]);
    struct fptr *next;
};


static term load_nif(ErlNifEnv *env, int argc, const term argv[])
{
    if (2 != argc)
        return enif_make_badarg(env);

    struct enif_environment_t *m = map_lookup(&modules, atom_untagged(argv[0]));
    assert(NULL != m);
    return enif_make_int(NULL, m->entry->load(m, &m->priv_data, argv[1]));
}


/* such bifs.  wow. */
static void construct_erlang_env(void)
{
    struct enif_environment_t *e = malloc(sizeof(*e));
    assert(e);
    struct enif_entry_t *entry = malloc(sizeof(*entry));
    assert(entry);
    *entry = (struct enif_entry_t){
        .name = "erlang"
    };
    *e = (struct enif_environment_t){
        .entry = entry
    };
    assert(map_insert(&modules, intern(str_dup_cstr(e->entry->name)), e));

    atom sym = intern(str_dup_cstr("load_nif"));
    struct fptr *f = malloc(sizeof(*f));
    *f = (struct fptr){.arity = 2, .fptr = load_nif};
    map_insert(&e->fns, sym, f);
}


static term call(struct function_call *call)
{
    struct enif_environment_t *m = map_lookup(&modules, call->module);
    assert(NULL != m);
    term tuple = tuple_of_list(call->args);
    unsigned arity;
    const term *p;
    assert(enif_get_tuple(NULL, tuple, (int *)&arity, &p));
    struct fptr *f = map_lookup(&m->fns, call->function);
    while (f) {
        if (arity == f->arity) {
            term result = f->fptr(m, arity, p);
            if (m->exception) {
                fprintf(stderr, "raised exception ");
                pretty_print_term(stderr, &m->exception);
                fputs("", stderr);
                /* continuing cowardly */
            }
            return result;
        }
        f = f->next;
    }
    fprintf(stderr, "no match for function ");
    pretty_print_atom(stderr, call->function);
    fprintf(stderr, "/%u\n", arity);
    abort();
}


static void handle_statement(struct statement *st)
{
    term result;

    /* for each statement, execute it and check the env for
       exceptions thrown. */
    switch (st->type) {
    default:
    case AST_ST_NOP:
        break;
    case AST_ST_V_OF_TERM:
        enif_get_list_cell(NULL, st->call.args, &result, NULL);
        assert(variable_assign(st->variable, result));
        break;

    case AST_ST_V_OF_MFA:
        result = call(&st->call);
        assert(variable_assign(st->variable, result));
        break;
    case AST_ST_MFA:
        result = call(&st->call);
        pretty_print_term(stdout, &result);
        putchar('\n');
        break;
    }
}


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
    struct enif_environment_t so[n_sos];
    memset(so, 0, n_sos * sizeof(*so));

    construct_erlang_env();

    for (int so_idx = 0; so_idx < n_sos && optind < argc; ++so_idx, ++optind) {
        struct enif_environment_t *s = &so[so_idx];
        s->path = argv[optind];

        s->dl_handle = dlopen(s->path, rtld_mode);
        if (s->dl_handle == NULL) {
            fprintf(stderr, "dlopen(%s): %s\n", s->path, dlerror());
            return 1;
        }
        ErlNifEntry *(*init)(void);
        *(void **)&init = dlsym(s->dl_handle, "nif_init");
        if (init == NULL && verbosity > 0) {
            printf("%s does not have a nif_init symbol\n", s->path);
            continue;
        }
        s->entry = init();
        assert(map_insert(&modules, intern(str_dup_cstr(s->entry->name)), s));

        if (verbosity > 0)
            printf("%s: %s %d.%d\n", s->path, s->entry->name, s->entry->major, s->entry->minor);
        for (int i = 0; i < s->entry->num_of_funcs; ++i) {
            if (verbosity > 1)
                printf("  %s/%d\n", s->entry->funcs[i].name, s->entry->funcs[i].arity);
            atom sym = intern(str_dup_cstr(s->entry->funcs[i].name));
            struct fptr *f = malloc(sizeof(*f));
            *f = (struct fptr){.arity = s->entry->funcs[i].arity,
                               .fptr = s->entry->funcs[i].fptr,
                               .next = map_lookup(&s->fns, sym)};
            map_insert(&s->fns, sym, f);
        }
    }

    FILE *in = stdin;
    char *line = NULL;
    size_t line_len = 0;
    struct lexer lexer;
    lex_init(&lexer);
    void *pParser;

    pParser = ParseAlloc(malloc);
    while (-1 != getline(&line, &line_len, in)) {
        lex_setup_next_line(&lexer, line, feof(in));
        struct token token;

        while (lex(&lexer, &token)) {
            Parse(pParser, token.type, token, handle_statement);
            /* Slight hack to allow nicer interactive sessions.  We
               don't have nested expressions, so if we see a dot, we
               encourage the parser to do its work eagerly. */
            if (token.type == TOK_DOT)
                Parse(pParser, 0, (struct token){0}, handle_statement);
        }
    }

    Parse(pParser, 0, (struct token){0}, handle_statement);
    free(line);
    ParseFree(pParser, free);

    for (int so_idx = 0; so_idx < n_sos; ++so_idx)
        dlclose(so[so_idx].dl_handle);
    return 0;
}
