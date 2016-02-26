#pragma once

#include <stdbool.h>
#include "ast.h"

extern void niffy_construct_erlang_env(void);
extern bool niffy_load_so(const char *, int, int);
extern void niffy_handle_statement(struct statement *);
extern void niffy_destroy_environments(void);
