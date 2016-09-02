/*
 * Lexer for Erlang terms.
 *
 * Primary reference: OTP's lib/stdlib/src/erl_scan.erl
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex.h"

int int_of_digit(char c, int radix)
{
    int v = -1;
    if (c >= '0' && c <= '9')
        v = c-'0';
    else if ((c|0x20) >= 'a' && (c|0x20) <= 'z')
        v = 10+(c|0x20)-'a';
    if (v < 0 || v >= radix) {
        fprintf(stderr, "bad digit %d for radix %d\n", v, radix);
        exit(1);
    }
    return v;
}

%%{

machine erlang_term;
access state->;
variable p state->p;
variable pe state->pe;
variable eof state->eof;

action i_val_digit {
  state->i.value = (state->radix*state->i.value) + int_of_digit(fc, state->radix);
}

primitive_escapes =
    'n' @{ state->i.value = '\n'; }
  | 'r' @{ state->i.value = '\r'; }
  | 't' @{ state->i.value = '\t'; }
  | 'v' @{ state->i.value = 11; }
  | 'b' @{ state->i.value = 8; }
  | 'f' @{ state->i.value = 12; }
  | 'e' @{ state->i.value = 27; }
  | 's' @{ state->i.value = ' '; }
  | 'd' @{ state->i.value = 127; }
  | [0-7]{1,3} >{ state->i.value = 0; state->radix = 8; }
               $i_val_digit
  | 'x' >{state->i.value = 0; state->radix = 16;}
    ('{' xdigit++ $i_val_digit '}'
    | xdigit{2} $i_val_digit
    )
  | '^' [@A-Z\[\]\\^_] ${ state->i.value = fc-'@'; }
  ;

escaped_char = '\\'
  ( primitive_escapes
  | (any - primitive_escapes) ${ state->i.value = fc; }
  );

char_literal =
    '$' (
    escaped_char
  | [^\\] @{ state->i.value = fc; }
    );

float_literal =
  '-'? [0-9]+ '.' [0-9]+ ([eE] [+\-]? [0-9]+)?
  # XXX should probably convert character-by-character here
  %{ char *s = strndup(state->ts, state->te-state->ts), *e; token->float_value = strtod(s, &e); free(s); };

int_literal =
  (([2-9] | [1-2][0-9] | '3'[0-6])
   '#' @{ state->radix = state->i.value; state->i.negate_p = false; state->i.value = 0; }
   '-'? ${ state->i.negate_p = true; }
   [0-9a-zA-Z]+ $i_val_digit
  |
   '-'? >{ state->radix = 10; state->i.negate_p = false; state->i.value = 0; }
        ${ state->i.negate_p = true; }
   [0-9]+ $i_val_digit
  )
  %{ if (state->i.negate_p) state->i.value = -state->i.value; }
  ;

# There is a leak here, on erroneous input; we could use a local error
# state to free these strings here and below under those conditions.
string_literal =
  '"' >{ token->string_value = str_new(0); }
  (( (escaped_char $1 %0)
   | [^"\\] ${ state->i.value = fc; }
   ) %{ str_appendch(&token->string_value, state->i.value); })*
  '"' ;

quoted_atom =
  '\'' >{ token->string_value = str_new(0); }
  (( (escaped_char $1 %0)
   | [^'\\] ${ state->i.value = fc; }
   ) %{ str_appendch(&token->string_value, state->i.value); })*
  '\''
    %{ token->atom_value = intern(token->string_value); }
  ;

main := |*
  # skip comments and whitespace
  '%' [^\r\n]*;
  [ \t]+;
  ('\r\n' | '\n') => { ++state->location.line_num; };

  char_literal => { token->type = TOK_CHAR; token->char_value = state->i.value; fbreak; };
  float_literal => { token->type = TOK_FLOAT; fbreak; };
  int_literal => { token->type = TOK_INTEGER; token->int64_value = state->i.value; fbreak; };
  string_literal => { token->type = TOK_STRING; fbreak; };

  # atom
  [a-z@][0-9a-zA-Z_@]* => {
    token->type = TOK_ATOM;
    struct str *name = str_new(state->te - state->ts);
    name->len = state->te - state->ts;
    memcpy(name->data, state->ts, name->len);
    token->atom_value = intern(name);
    fbreak;
  };
  quoted_atom => {
    token->type = TOK_ATOM;
    fbreak;
  };
  [A-Z_][0-9a-zA-Z_]* => {
    token->type = TOK_VARIABLE;
    struct str *name = str_new(state->te - state->ts);
    name->len = state->te - state->ts;
    memcpy(name->data, state->ts, name->len);
    token->atom_value = intern(name);
    fbreak;
  };

  # line noise
  '(' => { token->type = TOK_LPAREN; fbreak; };
  ')' => { token->type = TOK_RPAREN; fbreak; };
  ',' => { token->type = TOK_COMMA; fbreak; };
  '{' => { token->type = TOK_LBRACE; fbreak; };
  '}' => { token->type = TOK_RBRACE; fbreak; };
  '[' => { token->type = TOK_LBRACKET; fbreak; };
  ']' => { token->type = TOK_RBRACKET; fbreak; };
  '|' => { token->type = TOK_PIPE; fbreak; };
  '/' => { token->type = TOK_SLASH; fbreak; };
  '-' => { token->type = TOK_HYPHEN; fbreak; };
  ':' => { token->type = TOK_COLON; fbreak; };
  '.' => { token->type = TOK_DOT; fbreak; };
  '=' => { token->type = TOK_EQUALS; fbreak; };
  '<<' => { token->type = TOK_LBIN; fbreak; };
  '>>' => { token->type = TOK_RBIN; fbreak; };
*|;

}%%

%% write data;

void lex_init(struct lexer *state)
{
    /* Compromise to suppress unused variable warnings. */
    (void)erlang_term_first_final;
    (void)erlang_term_en_main;

    *state = (struct lexer){
        .location = {.line_num = 1}
    };
    %% write init;
}


void lex_setup_next_line(struct lexer *state, char *line, size_t len, bool is_eof)
{
    state->p = line;
    state->pe = state->p + len;
    state->eof = is_eof ? state->pe : NULL;
}


bool lex(struct lexer *state, struct token *token)
{
    token->type = TOK_NOTHING;
    token->location = state->location;

    if (state->p >= state->pe)
        return false;

    %% write exec;

    /* check for error */
    if (erlang_term_error == state->cs) {
        fprintf(stderr, "%d: oops!\n", state->location.line_num);
        return false;
    }

    /* p == pe when we need more data; we could be in the middle of a
     * token, though. */
    return (state->p < state->pe || state->pe == state->eof);
}


void destroy_token(struct token *token)
{
    if (TOK_STRING == token->type && token->string_value)
        str_free(&token->string_value);
}


bool pretty_print_token(void *data, struct token *tk)
{
    FILE *out = data;
    const struct str *name;

    switch (tk->type) {
    case TOK_NOTHING: fprintf(out, "NOTHING"); break;
    case TOK_CHAR: fprintf(out, "CHAR(%c)", tk->char_value); break;
    case TOK_FLOAT: fprintf(out, "FLOAT(%g)", tk->float_value); break;
    case TOK_INTEGER: fprintf(out, "INTEGER(%ld)", tk->int64_value); break;
    case TOK_STRING: fprintf(out, "STRING(%.*s)",
                             (int)tk->string_value->len, tk->string_value->data); break;
    case TOK_ATOM:
        name = symbol_name(tk->atom_value);
        fprintf(out, "ATOM(%u :%.*s)", tk->atom_value, (int)name->len, name->data);
        break;
    case TOK_VARIABLE:
        name = symbol_name(tk->atom_value);
        fprintf(out, "VARIABLE(%.*s)", (int)name->len, name->data);
        break;
    case TOK_LPAREN: fprintf(out, "LPAREN"); break;
    case TOK_RPAREN: fprintf(out, "RPAREN"); break;
    case TOK_LBRACE: fprintf(out, "LBRACE"); break;
    case TOK_RBRACE: fprintf(out, "RBRACE"); break;
    case TOK_LBRACKET: fprintf(out, "LBRACKET"); break;
    case TOK_RBRACKET: fprintf(out, "RBRACKET"); break;
    case TOK_LBIN: fprintf(out, "LBIN"); break;
    case TOK_RBIN: fprintf(out, "RBIN"); break;
    case TOK_COMMA: fprintf(out, "COMMA"); break;
    case TOK_COLON: fprintf(out, "COLON"); break;
    case TOK_DOT: fprintf(out, "DOT"); break;
    case TOK_PIPE: fprintf(out, "PIPE"); break;
    case TOK_SLASH: fprintf(out, "SLASH"); break;
    case TOK_HYPHEN: fprintf(out, "HYPHEN"); break;
    case TOK_EQUALS: fprintf(out, "EQUALS"); break;
    default: fprintf(out, "unknown"); break;
    }
    return true;
}
