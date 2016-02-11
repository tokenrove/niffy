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
    struct str *q = realloc(*p, next + sizeof(struct str));
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
    if ((*p)->len+1 >= (*p)->avail)
        if (!str_grow(p))
            return false;
    (*p)->data[(*p)->len++] = c;
    return true;
}


bool str_eq(const struct str *a, const struct str *b)
{
    if (a->len != b->len)
        return false;
    for (size_t i = 0; i < a->len; ++i)
        if (a->data[i] != b->data[i])
            return false;
    return true;
}


void str_print(FILE *out, const struct str *s)
{
    fprintf(out, "%.*s", (int)s->len, s->data);
}
