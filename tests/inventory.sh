#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/inventory
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/inventory-sanitize; fi
mkdir -p "$work"
run=$(mktemp -d "$work/run-XXXXXX")
node tests/inventory-fixtures.js "$run"
for mode in O0 O2; do
    "$fermi" test examples/inventory/tests.fe "-$mode" > "$run/tests"
    grep -q '^test result: ok. 4 passed; 0 failed$' "$run/tests"
    "$fermi" build examples/inventory/main.fe "-$mode" -o "$run/inventory"
    "$run/inventory" examples/inventory/sample.journal "$run/sample-$mode.snapshot" > "$run/out" 2> "$run/err"
    test ! -s "$run/out"
    test ! -s "$run/err"
    cmp examples/inventory/sample.expected "$run/sample-$mode.snapshot"
    "$run/inventory" "$run/sample-$mode.snapshot" "$run/replay-$mode.snapshot"
    cmp "$run/sample-$mode.snapshot" "$run/replay-$mode.snapshot"
    for seed in 1 2 3; do
        "$run/inventory" "$run/reference-$seed.journal" "$run/reference-$seed-$mode.snapshot"
        cmp "$run/reference-$seed.expected" "$run/reference-$seed-$mode.snapshot"
        "$run/inventory" "$run/reference-$seed-$mode.snapshot" "$run/replay-$seed-$mode.snapshot"
        cmp "$run/reference-$seed.expected" "$run/replay-$seed-$mode.snapshot"
    done
    for offset in $(seq 0 63); do
        "$run/inventory" "$run/chunk-$offset.journal" "$run/chunk-$offset-$mode.snapshot"
        cmp "$run/chunk.expected" "$run/chunk-$offset-$mode.snapshot"
    done
    "$run/inventory" "$run/boundary.journal" "$run/boundary-$mode.snapshot"
    cmp "$run/boundary.expected" "$run/boundary-$mode.snapshot"
    "$run/inventory" "$run/empty.journal" "$run/empty-$mode.snapshot"
    test ! -s "$run/empty-$mode.snapshot"
    for name in duplicate missing underflow overflow nonempty zero_id number spacing trailing name truncated line input commands capacity utf8 nul; do
        status=0
        "$run/inventory" "$run/reject-$name.journal" "$run/reject-$name-$mode.snapshot" > "$run/out" 2> "$run/err" || status=$?
        test "$status" -eq 65
        test ! -e "$run/reject-$name-$mode.snapshot"
        test ! -s "$run/out"
        grep -q '^inventory: .*: line [0-9][0-9]*$' "$run/err"
    done
    status=0
    "$run/inventory" examples/inventory/sample.journal "$run/sample-$mode.snapshot" > "$run/out" 2> "$run/err" || status=$?
    test "$status" -eq 74
    cmp examples/inventory/sample.expected "$run/sample-$mode.snapshot"
    status=0
    "$run/inventory" > "$run/out" 2> "$run/err" || status=$?
    test "$status" -eq 64
    grep -q '^usage: inventory JOURNAL SNAPSHOT$' "$run/err"
    status=0
    "$run/inventory" "$run/absent" "$run/absent-$mode.snapshot" > "$run/out" 2> "$run/err" || status=$?
    test "$status" -eq 74
    test ! -e "$run/absent-$mode.snapshot"
    printf 'PASS: inventory %s, 6144 reference commands, snapshot replay, UTF-8 boundaries and failures\n' "$mode"
done
