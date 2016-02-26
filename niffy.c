#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "erl_nif.h"

#include "ast.h"
#include "lex.h"
#include "macrology.h"
#include "niffy.h"
#include "nif_stubs.h"
#include "parse_protos.h"
#include "variable.h"


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
void niffy_construct_erlang_env(void)
{
    struct enif_environment_t *e = calloc(1, sizeof(*e));
    assert(e);
    struct enif_entry_t *entry = calloc(1, sizeof(*entry));
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
    term tuple = tuple_of_list(NULL, call->args);
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


void niffy_handle_statement(struct statement *st)
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

    case AST_ST_VAR:
        result = variable_lookup(st->variable);
        pretty_print_term(stdout, &result);
        putchar('\n');
        break;
    }
}


bool niffy_load_so(const char *path, int rtld_mode, int verbosity)
{
    struct enif_environment_t *s = calloc(1, sizeof(*s));
    s->path = path;

    s->dl_handle = dlopen(s->path, rtld_mode);
    if (s->dl_handle == NULL) {
        fprintf(stderr, "dlopen(%s): %s\n", s->path, dlerror());
        return false;
    }
    ErlNifEntry *(*init)(void);
    *(void **)&init = dlsym(s->dl_handle, "nif_init");
    if (init == NULL && verbosity > 0) {
        printf("%s does not have a nif_init symbol\n", s->path);
        return true;
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
    return true;
}


void niffy_destroy_environments(void)
{
    void free_fn_v(struct atom_ptr_pair p) { free(p.v); }
    void free_mp_v(struct atom_ptr_pair p) {
        struct enif_environment_t *e = p.v;
        map_iter(&e->fns, free_fn_v);
        map_destroy(&e->fns);
        if (e->dl_handle)
            dlclose(e->dl_handle);
        /* enif_free_env(e); */
    }
    map_iter(&modules, free_mp_v);
    map_destroy(&modules);
    enif_free_env(NULL);
}
