#include <assert.h>
#include <stdint.h>
#include "erl_nif.h"
#include "../macrology.h"

static int load(ErlNifEnv *UNUSED, void **UNUSED, ERL_NIF_TERM UNUSED) { return 0; }


static ERL_NIF_TERM return_ok(ErlNifEnv *env, int argc, const ERL_NIF_TERM *UNUSED)
{
    assert(0 == argc);
    /* Create some garbage that would be collected. */
    enif_make_double(env, 42.);
    enif_make_binary(env, &(ErlNifBinary){.data = (uint8_t *)"foo", .size = 3});
    return enif_make_atom(env, "ok");
}


static ERL_NIF_TERM return_iolist_as_binary(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    assert(1 == argc);
    ErlNifBinary bin;
    if (!enif_inspect_iolist_as_binary(env, argv[0], &bin))
        return enif_make_badarg(env);
    return enif_make_binary(env, &bin);
}



static ErlNifFunc fns[] = {
    {"return_ok", 0, return_ok},
    {"return_iolist_as_binary", 1, return_iolist_as_binary}
};
ERL_NIF_INIT(clean_nif, fns, &load, NULL, NULL, NULL);
