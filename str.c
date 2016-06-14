/* Naive dynamically-allocated strings
 *
 * No attempts to be clever; hopefully your allocator will do an
 * acceptable job.  Not intended for high-performance or high-safety
 * applications.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

struct str *str_new(size_t len)
{
    struct str *p = malloc(sizeof(*p) + len);
    p->avail = len;
    p->len = 0;
    return p;
}


struct str *str_dup_cstr(const char *s)
{
    size_t len = strlen(s);
    struct str *p = str_new(len);
    memcpy(p->data, s, len);
    p->len = len;
    return p;
}


void str_free(struct str **p)
{
    if (NULL == *p) return;
    (*p)->avail = (*p)->len = 0;
    free(*p);
    *p = NULL;
}


static bool str_grow(struct str **p)
{
    size_t next = 16;
    enum { BIG_STRING_LEN = 2048 };
    if ((*p)->avail >= BIG_STRING_LEN)
        next = (*p)->avail + ((*p)->avail >> 1);
    else if ((*p)->avail >= next)
        next = (*p)->avail * 2;
    assert(next > (*p)->avail);
    size_t total;
    assert(!__builtin_add_overflow(next, sizeof(struct str), &total));
    struct str *q = realloc(*p, total);
    if (q == NULL)
        return false;
    *p = q;
    (*p)->avail = next;
    return true;
}


bool str_appendch(struct str **p, char c)
{
    if (NULL == *p)
        *p = str_new(16);
    if ((*p)->len+1 >= (*p)->avail && !str_grow(p))
        return false;
    (*p)->data[(*p)->len++] = c;
    return true;
}


bool str_eq(const struct str *a, const struct str *b)
{
    if (a->len != b->len)
        return false;
    return 0 == memcmp(a->data, b->data, a->len);
}


void str_print(FILE *out, const struct str *s)
{
    fprintf(out, "%.*s", (int)s->len, s->data);
}
