#include "erl_nif.h"

static int load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) { return 0; }

static ERL_NIF_TERM alloc_resource_without_make(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    enif_alloc(42);             /* This won't be collected. */
    return enif_make_atom(env, "ok");
}

static ErlNifFunc fns[] = {
    {"alloc_resource_without_make", 0, alloc_resource_without_make}
};
ERL_NIF_INIT(leaky_nif, fns, &load, NULL, NULL, NULL);
