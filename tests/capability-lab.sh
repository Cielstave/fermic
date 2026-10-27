#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/capability-lab
mkdir -p "$work"
"${CC:-clang}" -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic tests/integration/capability_reference.c -o "$work/reference"
index=0
while [ "$index" -lt 6 ]; do
    rm -f "$work/case-$index.input" "$work/case-$index.expected"
    index=$((index + 1))
done
"$work/reference" "$work"
printf 'primes-below-100=25 gcd-84-30=6\n' > "$work/numbers.expected"
printf 'grid-distance=8\n' > "$work/grid.expected"
printf 'stack-result=32\n' > "$work/machine.expected"
printf '+ 0 1' > "$work/truncated.input"
printf '+ 0 1000001\n' > "$work/overflow.input"
printf '+ 0 1\r\n' > "$work/crlf.input"
printf '+ 0 \377\n' > "$work/binary.input"
printf '+ 65536 1\n' > "$work/id-overflow.input"
awk 'BEGIN { for (i = 0; i < 65536; i++) printf "\n"; }' > "$work/exact.input"
cp "$work/exact.input" "$work/oversized.input"
printf '\n' >> "$work/oversized.input"
: > "$work/empty.input"
{
    index=0
    while [ "$index" -lt 8 ]; do
        printf 'item %s 0\n' "$index"
        index=$((index + 1))
    done
    printf 'accepted 0\nrejected 0\n'
} > "$work/empty.expected"
for mode in O0 O2; do
    for program in numbers grid machine ledger; do
        "$fermi" build "examples/capability_lab/$program.fe" "-$mode" -o "$work/$program-$mode"
        "$fermi" test "examples/capability_lab/$program.fe" "-$mode" > "$work/test-results"
        case "$program" in
            numbers|ledger) count=2 ;;
            grid|machine) count=4 ;;
        esac
        grep -q "^test result: ok. $count passed; 0 failed\$" "$work/test-results"
        if [ "$program" != ledger ]; then
            "$work/$program-$mode" > "$work/output"
            cmp "$work/$program.expected" "$work/output"
        fi
    done
    index=0
    while [ "$index" -lt 6 ]; do
        target="$work/report-$mode-$index"
        rm -f "$target"
        "$work/ledger-$mode" "$work/case-$index.input" "$target" > "$work/out" 2> "$work/err"
        test ! -s "$work/out"
        test ! -s "$work/err"
        cmp "$work/case-$index.expected" "$target"
        index=$((index + 1))
    done
    for name in empty exact; do
        target="$work/$name-$mode.report"
        rm -f "$target"
        "$work/ledger-$mode" "$work/$name.input" "$target" > "$work/out" 2> "$work/err"
        cmp "$work/empty.expected" "$target"
    done
    for name in truncated overflow crlf binary id-overflow oversized; do
        target="$work/$name-$mode.report"
        rm -f "$target"
        status=0
        "$work/ledger-$mode" "$work/$name.input" "$target" > "$work/out" 2> "$work/err" || status=$?
        test "$status" -eq 65
        test ! -e "$target"
        printf 'ledger: invalid or oversized input\n' > "$work/error.expected"
        cmp "$work/error.expected" "$work/err"
    done
    target="$work/preserved-$mode.report"
    printf 'preserve this file\n' > "$target"
    cp "$target" "$work/preserved.expected"
    status=0
    "$work/ledger-$mode" "$work/case-0.input" "$target" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 74
    cmp "$work/preserved.expected" "$target"
    status=0
    "$work/ledger-$mode" "$work/absent.input" "$target" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 74
    status=0
    "$work/ledger-$mode" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 64
    printf 'PASS: capability applications %s, 6144 reference commands and failure cases\n' "$mode"
done
reject() {
    file=$1
    code=$2
    if "$fermi" check "examples/capability_lab/limitations/$file.fe" --diagnostic-format=json > "$work/out" 2> "$work/err"; then
        echo "FAIL: expected limitation rejection: $file" >&2
        exit 1
    fi
    grep -q "\"code\":\"$code\"" "$work/err"
}
reject loop_return E_RETURN
reject record_view E_PARSE
reject dynamic_text E_TYPE
reject float_math E_PARSE
reject general_generic E_PARSE
reject typed_buffer E_PARSE
