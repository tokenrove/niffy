#pragma once

#include "atom.h"
#include "nif_stubs.h"

extern bool variable_assign(atom, term);
extern term variable_lookup(atom);
