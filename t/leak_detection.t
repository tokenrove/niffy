#!/usr/bin/env bash

set -eu -o pipefail

verbose=false
if [ "${1:-}" = '-v' ]; then
   verbose=true
fi

quiet() {
    if [ "$verbose" = true ]; then
        "$@"
    else
        "$@" 2>/dev/null >/dev/null
    fi
}

check_clean_nif() {
    echo "# clean NIF should have no leaks"
    echo 'clean_nif:return_ok().' | quiet valgrind --leak-check=full --error-exitcode=42 ./niffy ./t/clean_nif.so
    echo 'clean_nif:return_iolist_as_binary([<<1,2,3>>, 42]).' | quiet valgrind --leak-check=full --error-exitcode=42 ./niffy ./t/clean_nif.so
    echo 'clean_nif:alloc_and_make_binary().' | quiet valgrind --leak-check=full --error-exitcode=42 ./niffy ./t/clean_nif.so
}

check_leaky_nif() {
    echo "# leaky NIF should have leaks"
    echo 'leaky_nif:alloc_resource_without_make().' | quiet valgrind --leak-check=full --error-exitcode=42 ./niffy ./t/leaky_nif.so
}

echo 1..2

if check_clean_nif; then
    echo ok
else
    echo not ok
fi

if ! check_leaky_nif; then
    echo ok
else
    echo not ok
fi
