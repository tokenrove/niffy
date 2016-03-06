/* Trivial two-choice hash table for int32 -> void*
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "map.h"

typedef uint32_t hash;


/* per Bob Jenkins */
static hash hash_1(atom a, uint32_t m)
{
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    assert(m > 0);
    return a % m;
}

/* per Knuth */
static hash hash_2(atom a, uint32_t m)
{
    assert(m > 0);
    return (a * 2654435761) % m;
}


static bool insert(struct atom_ptr_map *m, struct atom_ptr_pair p)
{
    hash h1 = hash_1(p.k, m->avail), h2 = hash_2(p.k, m->avail), h;
    if (m->entries[h1].k == p.k)
        h = h1;
    else if (m->entries[h2].k == p.k)
        h = h2;
    else if (m->entries[h1].k == 0)
        h = h1;
    else if (m->entries[h2].k == 0)
        h = h2;
    else
        return false;
    m->entries[h] = p;
    return true;
}


static bool grow(struct atom_ptr_map *m)
{
    size_t original_size = m->avail;
    size_t next = 16;
    enum { BIG_MAP_LEN = 2048 };
    if (m->avail >= BIG_MAP_LEN)
        next = m->avail + (m->avail >> 1);
    else if (m->avail >= next)
        next = m->avail * 2;
    void *old = m->entries;
    m->entries = calloc(next, sizeof(*m->entries));
    if (NULL == m->entries) {
        m->entries = old;
        return false;
    }
    m->avail = next;
    for (size_t i = 0; i < original_size; ++i) {
        struct atom_ptr_pair p = m->entries[i];
        if (p.k) {
            if (!insert(m, p)) {
                free(m->entries);
                m->entries = old;
                m->avail = original_size;
                return false;
            }
            /*  maybe grow again */
        }
    }
    free(old);
    return true;
}


bool map_insert(struct atom_ptr_map *m, atom k, void *v)
{
    if ((m->avail == 0 || m->len == m->avail) &&
        !grow(m))
        return false;
    if (insert(m, (struct atom_ptr_pair){.k = k, .v = v})) {
        ++m->len;
        return true;
    }
    if (!grow(m))
        return false;
    return map_insert(m, k, v) && ++m->len;
}


void *map_lookup(struct atom_ptr_map *m, atom k)
{
    if (m->avail == 0)
        return NULL;

    hash h = hash_1(k, m->avail);
    if (m->entries[h].k == k)
        return m->entries[h].v;
    h = hash_2(k, m->avail);
    if (m->entries[h].k == k)
        return m->entries[h].v;
    return NULL;
}


void map_destroy(struct atom_ptr_map *m)
{
    free(m->entries);
    m->entries = NULL;
    m->avail = 0;
}


void map_iter(struct atom_ptr_map *m, void (*f)(struct atom_ptr_pair))
{
    for (size_t i = 0; i < m->avail; ++i)
        if (m->entries[i].k)
            f(m->entries[i]);
}
