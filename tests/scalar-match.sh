#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/scalar-match
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/scalar_match.fe "-$mode" > "$work/results"
    grep -q '^test result: ok. 8 passed; 0 failed$' "$work/results"
    "$fermi" test examples/features/scalar_match/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 2 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/scalar_match/main.fe --no-cache "-$mode" > "$work/out"
    printf 'scalar-match: checks passed\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
    cat > "$work/fault.fe" <<'SOURCE'
use std.io;
func divide(value: usize) -> usize { return 1 / value; }
func main() -> void {
    match (divide(0)) {
        case 1 { io.println(read "incorrect branch"); }
        case _ { io.println(read "incorrect fallback"); }
    }
}
SOURCE
    status=0
    "$fermi" run "$work/fault.fe" --no-cache "-$mode" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 1
    test ! -s "$work/out"
    grep -q 'runtime fault\[3\]: division by zero' "$work/err"
done
node - "$work" <<'JS'
const fs = require('fs');
const directory = process.argv[2];
for (const count of [127,128]) {
    let source = 'func classify(value: u16) -> u8 { match (value) {\n';
    for (let value = 0; value < count; ++value) {
        source += `case ${value} { return 1; }\n`;
    }
    source += 'case _ { return 2; } } }\n';
    fs.writeFileSync(`${directory}/cases-${count}.fe`, source);
}
JS
"$fermi" check "$work/cases-127.fe" > "$work/out"
if "$fermi" check "$work/cases-128.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'match exceeds 128 cases' "$work/err"
printf 'func f(value: u8) -> void { match (value) { case 0 {} } }\n' > "$work/rejected.fe"
status=0
"$fermi" check "$work/rejected.fe" --diagnostic-format=json > "$work/out" 2> "$work/err" || status=$?
test "$status" -eq 1
node - "$work/err" <<'JS'
const fs = require('fs');
const diagnostic = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
if (diagnostic.code !== 'E_MATCH' || !diagnostic.message.includes('wildcard')) throw Error('exhaustiveness diagnostic');
JS
"$fermi" explain tests/cases/scalar_match.fe > "$work/explain"
grep -q '^signed_code: static upper bound.*requested = 100' "$work/explain"
echo 'PASS: scalar patterns, extrema, generics, single evaluation, joins, resources, faults and limits'
