#pragma once

#include <stdint.h>
#include <stdio.h>

#include "str.h"

typedef uint32_t atom;

extern atom intern(const struct str *);
extern atom intern_cstr(const char *);
extern const struct str *symbol_name(atom);
extern void pretty_print_atom(FILE *, atom);
