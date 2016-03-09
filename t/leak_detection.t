#!/usr/bin/env bash

set -eu

check_clean_nif() {
    echo "# clean NIF should have no leaks"
    echo 'clean_nif:return_ok().' | valgrind --leak-check=full --error-exitcode=42 ./niffy ./t/clean_nif.so >/dev/null 2>/dev/null
}

check_leaky_nif() {
    echo "# leaky NIF should have leaks"
    echo 'leaky_nif:alloc_resource_without_make().' | valgrind --leak-check=full --error-exitcode=42 ./niffy ./t/leaky_nif.so >/dev/null 2>/dev/null
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
