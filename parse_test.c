
#include <assert.h>

#include "lex.h"
#include "parse_protos.h"
#include "str.h"
#include "variable.h"


static void pretty_print_mfa(struct function_call *call)
{
    if (call->module) {
        pretty_print_atom(stdout, call->module);
        fputc(':', stdout);
    }
    pretty_print_atom(stdout, call->function);
    pretty_print_argument_list(stdout, &call->args);
}


static void print(struct statement *st)
{
    switch (st->type) {
    default:
    case AST_ST_NOP:
        printf("nop.\n");
        break;

    case AST_ST_V_OF_TERM:
        str_print(stdout, symbol_name(st->variable));
        fputs(" = ", stdout);
        term head;
        enif_get_list_cell(NULL, st->call.args, &head, NULL);
        pretty_print_term(stdout, &head);
        puts(".");
        assert(variable_assign(st->variable, head));
        break;

    case AST_ST_V_OF_MFA:
        str_print(stdout, symbol_name(st->variable));
        fputs(" = ", stdout);
        pretty_print_mfa(&st->call);
        puts(".");
        break;

    case AST_ST_MFA:
        pretty_print_mfa(&st->call);
        puts(".");
        break;
    }
}


int main()
{
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

        while (lex(&lexer, &token))
            Parse(pParser, token.type, token, print);
    }
    Parse(pParser, 0, (struct token){.type = 0, .location = lexer.location}, print);
    free(line);
    ParseFree(pParser, free);
}
