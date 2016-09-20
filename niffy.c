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
static atom default_module;

struct fptr {
    unsigned arity;
    term (*fptr)(ErlNifEnv *env, int argc, const term argv[]);
    struct fptr *next;
};


static struct enif_environment_t *find_module_or_die(atom module)
{
    struct enif_environment_t *m = map_lookup(&modules, module);
    if (NULL == m) {
        fputs("no such module: ", stderr);
        pretty_print_atom(stderr, module);
        fputc('\n', stderr);
        exit(1);
    }
    return m;
}


static term bif_load_nif(ErlNifEnv *env, int argc, const term argv[])
{
    if (2 != argc)
        return enif_make_badarg(env);

    struct enif_environment_t *m = find_module_or_die(atom_untagged(argv[0]));
    assert(NULL != m);
    return enif_make_int(NULL, m->entry->load(m, &m->priv_data, argv[1]));
}


static term bif_assert_eq(ErlNifEnv *env, int UNUSED, const term argv[])
{
    if (enif_is_identical(argv[0], argv[1]))
        return enif_make_atom(env, "true");
    fputs("assertion failed: ", stderr);
    pretty_print_term(stderr, &argv[0]);
    fputs(" =:= ", stderr);
    pretty_print_term(stderr, &argv[1]);
    fputc('\n', stderr);
    abort();
}


static term bif_assert_ne(ErlNifEnv *env, int UNUSED, const term argv[])
{
    if (!enif_is_identical(argv[0], argv[1]))
        return enif_make_atom(env, "true");
    fputs("assertion failed: ", stderr);
    pretty_print_term(stderr, &argv[0]);
    fputs(" =/= ", stderr);
    pretty_print_term(stderr, &argv[1]);
    fputc('\n', stderr);
    abort();
}


static term bif_halt(ErlNifEnv *UNUSED, int UNUSED, const term *UNUSED)
{
    exit(0);
}


static struct fptr *find_fn_or_die(struct enif_environment_t *m, atom fn, unsigned arity)
{
    struct fptr *f = map_lookup(&m->fns, fn);
    while (f) {
        if (arity == f->arity)
            return f;
        f = f->next;
    }
    fprintf(stderr, "no match for function %s:", m->entry->name);
    pretty_print_atom(stderr, fn);
    fprintf(stderr, "/%u\n", arity);
    exit(1);
    return NULL;                /* unreachable */
}


static term call(struct function_call *call)
{
    atom module = call->module ? call->module : default_module;
    struct enif_environment_t *m = find_module_or_die(module);
    assert(NULL != m);
    term tuple = tuple_of_list(NULL, call->args);
    unsigned arity;
    const term *p;
    assert(enif_get_tuple(NULL, tuple, (int *)&arity, &p));
    struct fptr *f = find_fn_or_die(m, call->function, arity);
    term result = f->fptr(m, arity, p);
    if (m->exception) {
        fprintf(stderr, "raised exception ");
        pretty_print_term(stderr, &m->exception);
        fputc('\n', stderr);
        /* continuing cowardly */
        m->exception = 0;
    }
    return result;
}


static bool add_fn(struct atom_ptr_map *fm, const char *s, struct fptr fn)
{
    atom sym = intern_cstr(s);
    struct fptr *f = malloc(sizeof(*f));
    fn.next = map_lookup(fm, sym);
    *f = fn;
    return map_insert(fm, sym, f);
}


static struct enif_entry_t internal_env_entry = {
    .name = "niffy"
};

/* such bifs.  wow. */
void niffy_construct_erlang_env(void)
{
    struct enif_environment_t *e = calloc(1, sizeof(*e));
    assert(e);
    *e = (struct enif_environment_t){
        .entry = &internal_env_entry
    };
    assert(map_insert(&modules, intern_cstr(e->entry->name), e));

    assert(add_fn(&e->fns, "load_nif", (struct fptr){.arity = 2, .fptr = bif_load_nif}));
    assert(add_fn(&e->fns, "assert_eq", (struct fptr){.arity = 2, .fptr = bif_assert_eq}));
    assert(add_fn(&e->fns, "assert_ne", (struct fptr){.arity = 2, .fptr = bif_assert_ne}));
    assert(add_fn(&e->fns, "halt", (struct fptr){.arity = 0, .fptr = bif_halt}));
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
    atom module_atom = intern_cstr(s->entry->name);
    assert(map_insert(&modules, module_atom, s));
    if (!default_module)
        default_module = module_atom;

    if (verbosity > 0)
        printf("%s: %s %d.%d\n", s->path, s->entry->name, s->entry->major, s->entry->minor);
    for (int i = 0; i < s->entry->num_of_funcs; ++i) {
        if (verbosity > 1)
            printf("  %s/%d\n", s->entry->funcs[i].name, s->entry->funcs[i].arity);
        atom sym = intern_cstr(s->entry->funcs[i].name);
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
    void free_fn_v(struct atom_ptr_pair p) {
        for (struct fptr *f = p.v, *g; f; f = g) {
            g = f->next;
            free(f);
        }
    }
    void free_mp_v(struct atom_ptr_pair p) {
        struct enif_environment_t *e = p.v;
        map_iter(&e->fns, free_fn_v);
        map_destroy(&e->fns);
        if (e->dl_handle)
            dlclose(e->dl_handle);
        enif_free_env(e);
    }
    map_iter(&modules, free_mp_v);
    map_destroy(&modules);
    enif_free_env(NULL);
}
