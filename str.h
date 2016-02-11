#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct str {
    size_t len, avail;
    char data[];
};

extern struct str *str_new(size_t);
extern struct str *str_dup_cstr(const char *);
extern void str_free(struct str **);
extern bool str_appendch(struct str **, char);
extern bool str_eq(const struct str *, const struct str *);
extern void str_print(FILE *, const struct str *);
