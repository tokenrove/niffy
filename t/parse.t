#!/usr/bin/env bash

set -eu

echo 1..6
for i in t/parse-*.in; do
    ./parse_test < $i | diff -u - $i.out | while read line; do
        echo "# $line"
    done
    if (( (PIPESTATUS[0] | PIPESTATUS[1]) == 0 )); then
        echo ok
    else
        echo not ok
    fi
done
