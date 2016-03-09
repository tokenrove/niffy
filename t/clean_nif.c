#include "erl_nif.h"

static int load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info) { return 0; }

static ERL_NIF_TERM return_ok(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    /* Create some garbage that would be collected. */
    enif_make_double(env, 42.);
    enif_make_binary(env, &(ErlNifBinary){.data = "foo", .size = 3});
    return enif_make_atom(env, "ok");
}

static ErlNifFunc fns[] = {
    {"return_ok", 0, return_ok}
};
ERL_NIF_INIT(clean_nif, fns, &load, NULL, NULL, NULL);
