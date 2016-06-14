#!/usr/bin/env bash

set -eu

echo 1..1
for i in t/iolist-*.in; do
    ./niffy -q ./t/clean_nif.so <$i 2>/dev/null | diff -u - $i.out | while read line; do
        echo "# $line"
    done
    if (( (PIPESTATUS[0] | PIPESTATUS[1]) == 0 )); then
        echo ok
    else
        echo not ok
    fi
done
