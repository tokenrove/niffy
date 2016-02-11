#!/usr/bin/env bash

set -eu

echo 1..5
for i in t/term_lex-*.in; do
    ./lex_test < $i | diff -u - $i.out | while read line; do
        echo "# $line"
    done
    if (( ${PIPESTATUS[1]} == 0 )); then
        echo ok
    else
        echo not ok
    fi
done
