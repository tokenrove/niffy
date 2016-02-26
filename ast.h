
#pragma once

#include "lex.h"
#include "nif_stubs.h"

struct function_call {
    atom module;
    atom function;
    term args;
};

struct statement {
    enum {
        AST_ST_NOP,
        AST_ST_V_OF_TERM,
        AST_ST_V_OF_MFA,
        AST_ST_MFA,
        AST_ST_VAR
    } type;
    atom variable;
    struct function_call call;
};
