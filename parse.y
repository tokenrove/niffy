/* -*- lemon -*-
 *
 * This is based on lib/stdlib/src/erl_parse.yrl from OTP, circa late
 * 2015.  Beware!  That grammar is right-recursive, and lemon's stack
 * is limited.  This grammar has been converted to use left-recursion.
 *
 * (In fact, that grammar should not be right-recursive; it's easy to
 * construct a relatively small file that causes erl_parse to consume
 * all memory and die.)
 */

%include {
  #include <assert.h>
  #include "ast.h"
  #include "lex.h"
  #include "nif_stubs.h"
  #include "variable.h"

  typedef void (*callback)(struct statement *);
}

%extra_argument {callback cb}

%default_type {term}

%token_type {struct token}
%token_prefix TOK_
%token_destructor { destroy_token(&$$); }

top ::= statements.

statements ::= statements statement.
statements ::= .

%type statement {struct statement}
statement(S) ::= VARIABLE(V) EQUALS function_call(C) DOT.
  {
    S.type = AST_ST_V_OF_MFA;
    S.variable = V.atom_value;
    S.call = C;
    cb(&S);
  }
statement(S) ::= VARIABLE(V) EQUALS terms(T) DOT.
  {
    S.type = AST_ST_V_OF_TERM;
    S.variable = V.atom_value;
    S.call = (struct function_call){
        .args = nreverse_list(T)
    };
    cb(&S);
  }
statement(S) ::= function_call(C) DOT.
  {
    S.type = AST_ST_MFA;
    S.call = C;
    cb(&S);
  }
statement(S) ::= VARIABLE(V) DOT.
  {
    S.type = AST_ST_VAR;
    S.variable = V.atom_value;
    cb(&S);
  }

%type function_call {struct function_call}
function_call(C) ::= ATOM(M) COLON ATOM(F) argument_list(A).
  {
    C.module = M.atom_value;
    C.function = F.atom_value;
    C.args = A;
  }
function_call(C) ::= ATOM(F) argument_list(A).
  {
    C.function = F.atom_value;
    C.args = A;
  }

argument_list(A) ::= LPAREN RPAREN. { A = enif_make_list(NULL, 0); }
argument_list(A) ::= LPAREN terms(T) RPAREN. { A = nreverse_list(T); }

terms(T) ::= terms(A) COMMA term(D). { T = enif_make_list_cell(NULL, D, A); }
terms(T) ::= term(A). { T = enif_make_list_cell(NULL, A, nil); }

list(L) ::= LBRACKET RBRACKET. { L = nil; }
list(L) ::= LBRACKET terms(A) RBRACKET. { L = nreverse_list(A); }
list(L) ::= LBRACKET terms(A) PIPE term(T) RBRACKET. { L = nreverse_list(enif_make_list_cell(NULL, A, T)); }

binary(B) ::= LBIN RBIN. {
    B = enif_make_binary(NULL, &(ErlNifBinary){.size = 0, .data = NULL});
}
binary(B) ::= LBIN bin_elts(Es) RBIN. { B = iolist_to_binary(nreverse_list(Es)); }

bin_elts(E) ::= bin_elts(Hs) COMMA bin_elt(T). { E = enif_make_list_cell(NULL, T, Hs); }
bin_elts(E) ::= bin_elt(H). { E = enif_make_list(NULL, 1, H); }

/* we could read a specific subset of terms here, and have a better
* system for concatenating them. */
bin_elt(B) ::= term(T) opt_bit_size_expr opt_bit_type_list. { B = T; }

/* We don't support fancy bitsyntax stuff yet. */
/* erl_parse.yrl has bit_size_expr being an expression, but since we
 * only support constant terms, integer is probably enough here. */
opt_bit_size_expr ::= COLON INTEGER.
opt_bit_size_expr ::= .

opt_bit_type_list ::= SLASH bit_type_list.
opt_bit_type_list ::= .

bit_type_list ::= bit_type HYPHEN bit_type_list.
bit_type_list ::= bit_type.

/* atom can be one of integer, float, binary, signed, unsigned, big,
 * little, native */
bit_type ::= ATOM.
bit_type ::= ATOM COLON INTEGER.

tuple(T) ::= LBRACE RBRACE. { T = enif_make_tuple(NULL, 0); }
tuple(T) ::= LBRACE terms(L) RBRACE. { T = tuple_of_list(NULL, nreverse_list(L)); }

term(T) ::= atomic(A). { T = A; }
term(T) ::= tuple(A). { T = A; }
term(T) ::= list(L). { T = L; }
term(T) ::= binary(B). { T = B; }

atomic(A) ::= CHAR(C). { A = enif_make_int(NULL, C.char_value); }
atomic(A) ::= INTEGER(I). { A = enif_make_int(NULL, I.int64_value); }
atomic(A) ::= FLOAT(F). { A = enif_make_double(NULL, F.float_value); }
atomic(A) ::= ATOM(T). { A = tagged_atom(T.atom_value); }
atomic(A) ::= VARIABLE(V). { A = variable_lookup(V.atom_value); }
atomic(A) ::= strings(S). { A = S; }

strings(S) ::= STRING(T). {
    S = enif_make_string_len(NULL, T.string_value->data, T.string_value->len, ERL_NIF_LATIN1);
    destroy_token(&T);
}
strings(S) ::= strings(T) STRING(H). {
    S = T;
    T = enif_make_string_len(NULL, H.string_value->data, H.string_value->len, ERL_NIF_LATIN1);
    destroy_token(&H);
    assert(nconc(S, T));
}

%parse_failure {
    fprintf(stderr, "parse failure\n");
}

%syntax_error {
    fprintf(stderr, "%d: syntax error (", TOKEN.location.line_num);
    pretty_print_token(stderr, &TOKEN);
    fprintf(stderr, ")\n");
}

%stack_overflow {
    fprintf(stderr, "parser stack overflow\n");
}
