/* -*- lemon -*- */

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

statements ::= statement.

%type statement {struct statement}
statement(S) ::= statement VARIABLE(V) EQUALS function_call(C) DOT.
  {
    S.type = AST_ST_V_OF_MFA;
    S.variable = V.atom_value;
    S.call = C;
    cb(&S);
  }
statement(S) ::= statement VARIABLE(V) EQUALS terms(T) DOT.
  {
    S.type = AST_ST_V_OF_TERM;
    S.variable = V.atom_value;
    S.call = (struct function_call){
        .args = T
    };
    cb(&S);
  }
statement(S) ::= statement function_call(C) DOT.
  {
    S.type = AST_ST_MFA;
    S.call = C;
    cb(&S);
  }
statement ::= .

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
argument_list(A) ::= LPAREN terms(T) RPAREN. { A = T; }

terms(T) ::= term(A) COMMA terms(D). { T = enif_make_list_cell(NULL, A, D); }
terms(T) ::= term(A). { T = enif_make_list_cell(NULL, A, nil); }

list(L) ::= LBRACKET RBRACKET. { L = nil; }
list(L) ::= LBRACKET term(A) tail(D). { L = enif_make_list_cell(NULL, A, D); }
tail(T) ::= RBRACKET. { T = nil; }
tail(T) ::= PIPE term(A) RBRACKET. { T = A; }
tail(T) ::= COMMA term(A) tail(D). { T = enif_make_list_cell(NULL, A, D); }

binary(B) ::= LBIN RBIN. {
    B = enif_make_binary(NULL, &(ErlNifBinary){.size = 0, .data = NULL});
}
binary(B) ::= LBIN bin_elts(Es) RBIN. { B = iolist_to_binary(Es); }

bin_elts(E) ::= bin_elt(H). { E = enif_make_list(NULL, 1, H); }
bin_elts(E) ::= bin_elt(H) COMMA bin_elts(T). { E = enif_make_list_cell(NULL, H, T); }

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
tuple(T) ::= LBRACE terms(L) RBRACE. { T = tuple_of_list(L); }

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
strings(S) ::= STRING(H) strings(T). {
    S = enif_make_string_len(NULL, H.string_value->data, H.string_value->len, ERL_NIF_LATIN1);
    destroy_token(&H);
    assert(nconc(S, T));
}

%parse_failure {
    fprintf(stderr, "parse failure\n");
}

%syntax_error {
    fprintf(stderr, "%d: syntax error\n", TOKEN.location.line_num);
}
