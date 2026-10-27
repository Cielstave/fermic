#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/integer-domains
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/integer-domains-sanitize; fi
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/integer_domains.fe "-$mode" > "$work/results"
    grep -q '^test result: ok. 4 passed; 0 failed$' "$work/results"
    "$fermi" test examples/features/integer_domains/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 3 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/integer_domains/main.fe --no-cache "-$mode" > "$work/out"
    printf 'integer-domains: checks passed\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
    for pair in i8:-128 i16:-32768 i32:-2147483648 i64:-9223372036854775808; do
        type=${pair%%:*}
        minimum=${pair#*:}
        cat > "$work/fault.fe" <<SOURCE
func negate<T: signed_integer>(value: T) -> T { return -value; }
func main() -> i32 {
    let value: $type = negate<$type>($minimum);
    return 0;
}
SOURCE
        status=0
        "$fermi" run "$work/fault.fe" --no-cache "-$mode" > "$work/out" 2> "$work/err" || status=$?
        test "$status" -eq 1
        grep -q 'runtime fault\[2\]: integer overflow' "$work/err"
    done
done
cat > "$work/rejected.fe" <<'SOURCE'
use std.bits;
func main() -> i32 { return bits.set<i32>(1, 2); }
SOURCE
status=0
"$fermi" check "$work/rejected.fe" --diagnostic-format=json > "$work/out" 2> "$work/err" || status=$?
test "$status" -eq 1
node - "$work/err" <<'JS'
const fs = require('fs');
const diagnostic = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
if (diagnostic.code !== 'E_GENERIC' || !diagnostic.message.includes("'bits.set'") || !diagnostic.message.includes("'i32'")) throw Error('constraint diagnostic');
if (!diagnostic.help.includes('constraint')) throw Error('constraint guidance');
JS
"$fermi" explain tests/cases/integer_domains.fe > "$work/explain"
test "$(grep -c '^negate<' "$work/explain")" -eq 4
test "$(grep -c '^std.bits.set<' "$work/explain")" -eq 5
grep -q '^std.bits.clear<u8>: static upper bound.*effects: \[\]' "$work/explain"
echo 'PASS: signed/unsigned constraints, standard bits, widths, signed faults, diagnostics and limits'
