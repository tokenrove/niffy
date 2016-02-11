#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "atom.h"

struct atom_ptr_map {
    struct atom_ptr_pair {
        atom k;
        void *v;
    } *entries;
    size_t len, avail;
};

extern bool map_insert(struct atom_ptr_map *, atom, void *);
extern void *map_lookup(struct atom_ptr_map *, atom);
