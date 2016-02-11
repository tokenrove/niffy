
#include <stdio.h>

#include "map.h"
#include "str.h"
#include "variable.h"


static struct atom_ptr_map map;


bool variable_assign(atom k, term v)
{
    return map_insert(&map, k, (void *)v);
}


term variable_lookup(atom k)
{
    return (term)map_lookup(&map, k);
}
