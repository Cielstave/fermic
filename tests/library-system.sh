#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/library-system
mkdir -p "$work"
"${CC:-clang}" -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic tests/integration/library_reference.c -o "$work/reference"
index=0
while [ "$index" -lt 4 ]; do
    rm -f "$work/catalog-$index.journal" "$work/catalog-$index.expected"
    index=$((index + 1))
done
"$work/reference" "$work"
: > "$work/empty.journal"
printf 'STATE 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n' > "$work/empty.expected"
awk 'BEGIN { for (i=0; i<1048576; i++) printf "\n"; }' > "$work/exact.journal"
cp "$work/exact.journal" "$work/oversized.journal"
printf '\n' >> "$work/oversized.journal"
awk 'BEGIN { for (i=0; i<16384; i++) print "TICK 0"; }' > "$work/commands.journal"
cp "$work/commands.journal" "$work/excess.journal"
printf 'TICK 0\n' >> "$work/excess.journal"
printf 'BOOK 1 1' > "$work/truncated.journal"
printf 'BOOK  1 1\n' > "$work/spaces.journal"
printf 'BOOK 1 1000001\n' > "$work/number.journal"
printf 'BOOK 1 1\r\n' > "$work/crlf.journal"
printf 'UNKNOWN 1\n' > "$work/unknown.journal"
printf 'BOOK 1 \000\n' > "$work/binary.journal"
for mode in O0 O2; do
    "$fermi" test examples/library_system/tests.fe "-$mode" > "$work/test-results"
    grep -q '^test result: ok. 11 passed; 0 failed$' "$work/test-results"
    "$fermi" build examples/library_system/main.fe "-$mode" -o "$work/library-$mode"
    index=0
    while [ "$index" -lt 4 ]; do
        target="$work/catalog-$index-$mode.report"
        rm -f "$target"
        "$work/library-$mode" "$work/catalog-$index.journal" "$target" > "$work/out" 2> "$work/err"
        test ! -s "$work/out"
        test ! -s "$work/err"
        cmp "$work/catalog-$index.expected" "$target"
        index=$((index + 1))
    done
    target="$work/sample-$mode.report"
    rm -f "$target"
    "$work/library-$mode" examples/library_system/sample.journal "$target" > "$work/out" 2> "$work/err"
    test ! -s "$work/out"
    test ! -s "$work/err"
    grep -q '^STATE 6 28 2 26 2 3 2 1 1 2 1 1 102 102$' "$target"
    test "$(wc -l < "$target")" -eq 29
    for name in empty exact commands; do
        target="$work/$name-$mode.report"
        rm -f "$target"
        "$work/library-$mode" "$work/$name.journal" "$target"
        if [ "$name" != commands ]; then
            cmp "$work/empty.expected" "$target"
        else
            grep -q '^STATE 0 16384 0 0 16384 ' "$target"
        fi
    done
    for name in truncated spaces number crlf unknown binary oversized excess; do
        target="$work/$name-$mode.report"
        rm -f "$target"
        status=0
        "$work/library-$mode" "$work/$name.journal" "$target" > "$work/out" 2> "$work/err" || status=$?
        test "$status" -eq 65
        test ! -e "$target"
        test ! -s "$work/out"
        grep -q '^library: .*: line [0-9][0-9]*$' "$work/err"
    done
    target="$work/preserved-$mode.report"
    printf 'preserve this file\n' > "$target"
    cp "$target" "$work/preserved.expected"
    status=0
    "$work/library-$mode" "$work/empty.journal" "$target" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 74
    cmp "$work/preserved.expected" "$target"
    status=0
    "$work/library-$mode" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 64
    status=0
    "$work/library-$mode" "$work/absent.journal" "$target" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 74
    printf 'PASS: library system %s, 16384 reference commands, limits and failure cases\n' "$mode"
done
lines=$(awk 'NF && $1 !~ /^\/\// {count++} END {print count}' examples/library_system/main.fe examples/library_system/src/*.fe)
test "$lines" -gt 1000
