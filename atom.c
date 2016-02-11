/* Symbol table.
 *
 * Braindead -- simplest hash table possible.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "atom.h"

static const struct str **string_of_atom;
static atom symbol_counter;

typedef uint32_t hash;
static size_t allocated, n_entries;
static atom *table;

__attribute__((constructor))
static void init(void)
{
    allocated = 128;
    n_entries = 0;
    table = calloc(allocated, sizeof(*table));
    assert(table);
    string_of_atom = malloc((1+allocated) * sizeof(*string_of_atom));
    assert(string_of_atom);
}


static hash hash_of_string(const struct str *s)
{
    hash v = 0;
    for (size_t i = 0; i < s->len; ++i)
        v += s->data[i] * 31;
    return v;
}


static void grow(void)
{
    size_t original_size = allocated;
    allocated <<= 1;
    atom *new = calloc(allocated, sizeof(*table));
    for (size_t i = 0; i < original_size; ++i) {
        atom sym = table[i];
        if (sym) {
            assert(sym <= symbol_counter);
            hash h = hash_of_string(string_of_atom[sym]) % allocated, o = h;
            while (new[h] && ++h != o)
                h %= allocated;
            assert(!new[h]);
            new[h] = sym;
        }
    }
    free(table);
    table = new;
    typeof(string_of_atom) tmp =
        realloc(string_of_atom, (1+allocated) * sizeof(*string_of_atom));
    assert(tmp != NULL);
    string_of_atom = tmp;
    assert(string_of_atom);
}


static void insert(const struct str *s, hash h, atom sym)
{
    if (n_entries >= allocated)
        grow();
    while (table[h % allocated])
        ++h;
    table[h % allocated] = sym;
    ++n_entries;
    string_of_atom[sym] = s;
}


static atom lookup(const struct str *s, hash h_orig)
{
    atom sym = 0;
    h_orig %= allocated;
    hash h = h_orig;
    while ((sym = table[h])) {
        if (str_eq(s, string_of_atom[sym]))
            return sym;
        h = (h+1) % allocated;
        if (h == h_orig)
            return 0;
    }
    return 0;
}


atom intern(const struct str *name)
{
    hash h = hash_of_string(name);
    atom sym = lookup(name, h);
    if (sym != 0)
        return sym;
    sym = ++symbol_counter;
    insert(name, h, sym);
    return sym;
}


const struct str *symbol_name(atom sym)
{
    if (sym < 1 || sym > symbol_counter)
        return NULL;
    return string_of_atom[sym];
}


static bool atom_must_be_quoted_p(const struct str *name)
{
    if (name->len == 0)
        return true;

    /* [a-z@][0-9a-zA-Z_@] */
    if (name->data[0] != '@' &&
        !(islower(name->data[0]) &&
          isalpha(name->data[0])))
        return true;

    for (unsigned i = 1; i < name->len; ++i)
        if (name->data[i] != '@' &&
            name->data[i] != '_' &&
            !isalpha(name->data[i]))
            return true;

    return false;
}


static void pretty_print_quoted_atom(FILE *out, const struct str *name)
{
    fputc('\'', out);
    for (unsigned i = 0; i < name->len; ++i) {
        if (isgraph(name->data[i]))
            fputc(name->data[i], out);
        else
            fprintf(out, "\\x%02x", (uint8_t)name->data[i]);
    }
    fputc('\'', out);
}


void pretty_print_atom(FILE *out, atom a)
{
    const struct str *name = symbol_name(a);
    if (atom_must_be_quoted_p(name))
        pretty_print_quoted_atom(out, name);
    else
        str_print(out, name);
}
