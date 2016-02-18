#pragma once

#include <stdio.h>

#include "erl_nif.h"
#include "map.h"
#include "str.h"

typedef ERL_NIF_TERM term;

struct alloc {
    void *p;
    struct alloc *next;
};

/* This should be called struct shared_object, but the way erl_nif.h
 * is written forces me to use this dumb name (which should not have a
 * trailing _t, and should be a typedef). */
struct enif_environment_t {
    void *dl_handle;
    char *path;
    /* The following items are non-NULL only if this SO is a NIF. */
    ErlNifEntry *entry;
    void *priv_data;
    term exception;
    struct atom_ptr_map fns;
    struct alloc *allocations;
};

typedef enum {
    TERM_THE_NON_VALUE,
    TERM_SMALL,
    TERM_ATOM,
    TERM_NIL,
    TERM_IMMEDIATE,
    TERM_TUPLE,
    TERM_FLOAT,
    TERM_BIN,
    TERM_EXTREF,
    TERM_BOXED,
    TERM_CONS,
    TERM_THING
} term_type;

/* helpers */
extern void pretty_print_term(FILE *, const term *);
void pretty_print_argument_list(FILE *, const term *);
term tuple_of_list(ErlNifEnv *, term);
extern bool nconc(term, term);
extern term iolist_to_binary(term);
extern term_type type_of_term(const term);
extern term tagged_atom(atom);
extern atom atom_untagged(term);

extern const term nil;
extern const unsigned max_atom_index;
