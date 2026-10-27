#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/entryDiagnostics
mkdir -p "$work"
cat > "$work/helper.fe" <<'SOURCE'
pub func answer() -> i32 { return 42; }
SOURCE
cat > "$work/program.fe" <<'SOURCE'
use "helper.fe";
func main() -> i32 {
    if (helper.answer() == 42) { return 0; }
    return 1;
}
SOURCE
cat > "$work/suite.fe" <<'SOURCE'
use "helper.fe";
func testAnswer() -> bool { return helper.answer() == 42; }
SOURCE
cat > "$work/parameters.fe" <<'SOURCE'

func main(value: i32) -> i32 { return value; }
SOURCE
cat > "$work/return.fe" <<'SOURCE'

func main() -> bool { return true; }
SOURCE
cat > "$work/exported.fe" <<'SOURCE'

export func main() -> i32 { return 0; }
SOURCE
cat > "$work/foreign.fe" <<'SOURCE'
pub func main() -> i32 { return 0; }
SOURCE
cat > "$work/imported.fe" <<'SOURCE'
use "foreign.fe";
SOURCE
"$fermi" check "$work/helper.fe" > "$work/checked"
"$fermi" run --no-cache "$work/program.fe"
"$fermi" test "$work/suite.fe" > "$work/tests"
grep -q '1 passed; 0 failed' "$work/tests"
"$fermi" build --library "$work/exported.fe" --out-dir "$work/native" > "$work/library"
checkError() {
    code=$1
    file=$2
    line=$3
    hint=$4
    if "$fermi" run --no-cache "$work/$file.fe" --diagnostic-format=json > "$work/out" 2> "$work/error"; then
        echo 'FAIL: invalid executable entry was accepted' >&2
        exit 1
    fi
    node - "$work/error" "$code" "$line" "$hint" <<'JS'
const fs = require('fs');
const [path, code, line, hint] = process.argv.slice(2);
const diagnostic = JSON.parse(fs.readFileSync(path, 'utf8'));
if (diagnostic.code !== code || diagnostic.primary.line !== Number(line) ||
    !diagnostic.help.includes(hint) || !diagnostic.message) {
    throw new Error(`Unexpected entry diagnostic: ${JSON.stringify(diagnostic)}`);
}
JS
    test ! -s "$work/out"
}
checkError E_ENTRY_MISSING helper 0 'fermi check'
checkError E_ENTRY_MISSING imported 0 'root module'
checkError E_ENTRY_MISSING suite 0 'fermi test'
checkError E_ENTRY_PARAMETERS parameters 2 'std.args'
checkError E_ENTRY_RETURN_TYPE return 2 'exit status'
checkError E_ENTRY_EXPORTED exported 2 'build --library'
if "$fermi" run --no-cache "$work/suite.fe" > "$work/out" 2> "$work/error"; then exit 1; fi
grep -q '^help: .*fermi test' "$work/error"
if grep -q 'use "helper.fe"' "$work/error"; then
    echo 'FAIL: missing entry attributed to an unrelated import' >&2
    exit 1
fi
printf '%s\n' 'PASS: entryless modules, imports, tests, C libraries and precise entry diagnostics'
