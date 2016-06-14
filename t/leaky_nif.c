#include "erl_nif.h"
#include "../macrology.h"

static int load(ErlNifEnv *UNUSED, void **UNUSED, ERL_NIF_TERM UNUSED) { return 0; }

static ERL_NIF_TERM alloc_resource_without_make(ErlNifEnv *env, int UNUSED, const ERL_NIF_TERM *UNUSED)
{
    enif_alloc(42);             /* This won't be collected. */
    return enif_make_atom(env, "ok");
}

static ErlNifFunc fns[] = {
    {"alloc_resource_without_make", 0, alloc_resource_without_make}
};
ERL_NIF_INIT(leaky_nif, fns, &load, NULL, NULL, NULL);
