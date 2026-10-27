#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/effect-contracts
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/effect_contracts.fe "-$mode" > "$work/results"
    grep -q '^test result: ok. 2 passed; 0 failed$' "$work/results"
    "$fermi" build --library tests/cases/effect_contracts.fe --out-dir "$work" "-$mode"
    "$fermi" run examples/cli/effect_contracts/main.fe --no-cache "-$mode" > "$work/out"
    printf 'Effect contracts passed.\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
done
node - "$work/effect_contracts.contract.json" <<'JS'
const fs = require('fs');
const manifest = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const lookup = name => manifest.exports.find(entry => entry.name === name);
const sum = lookup('checksum'), copied = lookup('copied_checksum'), increment = lookup('increment');
if (sum.proof !== 'unknown' || JSON.stringify(sum.allowed_effects) !== '[]') throw Error('unbounded contract');
if (JSON.stringify(copied.effects) !== '["memory"]' || JSON.stringify(copied.allowed_effects) !== '["memory"]') throw Error('memory contract');
if (increment.proof !== 'static' || increment.requested_work !== 100 || increment.allowed_effects.length) throw Error('budget contract');
JS
cat > "$work/helper.fe" <<'SOURCE'
use std.io;
pub func cycle(flag: bool) -> void {
    if (flag) { cycle(false); }
    io.print(read "dependency output");
}
SOURCE
cat > "$work/rejected.fe" <<'SOURCE'
use helper;
@effects()
func boundary() -> void { helper.cycle(false); }
SOURCE
status=0
"$fermi" check "$work/rejected.fe" --diagnostic-format=json > "$work/out" 2> "$work/diagnostic" || status=$?
test "$status" -eq 1
node - "$work/diagnostic" <<'JS'
const fs = require('fs');
const diagnostic = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
if (diagnostic.code !== 'E_EFFECT' || !diagnostic.path.endsWith('/rejected.fe') || diagnostic.primary.line !== 2) throw Error('contract location');
if (!diagnostic.message.includes("'io.write'") || !diagnostic.help.includes('helpers')) throw Error('contract explanation');
if (!diagnostic.related || diagnostic.related.span.line !== 3 || !diagnostic.related.message.includes('cycle')) throw Error('call witness');
JS
reject() {
    printf 'use std.fs; use std.mem; use std.io; use std.time; use std.env; use std.args;\n%s\n' "$2" > "$work/rejected.fe"
    if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then
        echo "FAIL: effect contract accepted: $2" >&2
        exit 1
    fi
    grep -q 'error\[E_EFFECT\]' "$work/err"
    grep -q "effect '$1'" "$work/err"
}
reject io.read '@effects() func f(output: edit u8[]) -> Result<usize, io.Error> { return io.read_into(edit output); }'
reject io.write '@effects() func f() -> void { io.println(read "x"); }'
reject memory '@effects() func f() -> Result<Buffer<u8>, mem.Error> { return mem.bytes(8, 8); }'
reject env.read '@effects() func f(output: edit u8[]) -> Result<usize, env.Error> { return env.read_into(read "FERMI_TEST", edit output); }'
reject process.args '@effects() func f() -> Result<usize, args.Error> { return args.count(); }'
reject clock.read '@effects() func f() -> Result<usize, time.Error> { return time.monotonic_ns(); }'
reject fs.open '@effects(fs.close) func f(path: read u8[]) -> Result<fs.File, fs.Error> { return fs.open_read(read path); }'
reject fs.read '@effects() func f(file: edit fs.File, output: edit u8[]) -> Result<usize, fs.Error> { return fs.read_into(edit file, edit output); }'
reject fs.close '@effects() func f(file: fs.File) -> void {}'
reject fs.write '@effects() func f(file: edit fs.File) -> Result<usize, fs.Error> { return fs.write(edit file, read "x"); }'
reject fs.sync '@effects() func f(file: edit fs.File) -> Result<void, fs.Error> { return fs.sync(edit file); }'
# Allowing effects is not a demand to perform them, nor permission to bypass budget.
printf '@effects(memory) func f() -> void {}\n' > "$work/allowed.fe"
"$fermi" check "$work/allowed.fe" > "$work/out"
cat > "$work/helper.fe" <<'SOURCE'
use std.io;
@effects()
pub func bad() -> void { io.print(read "x"); }
SOURCE
printf 'use helper; func main() -> void {}\n' > "$work/imported.fe"
if "$fermi" check "$work/imported.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
grep -q 'helper.fe:2:1: error\[E_EFFECT\]' "$work/err"
"$fermi" explain tests/cases/effect_contracts.fe > "$work/explain"
grep -q '^checksum:.*effects: \[\]; permitted effects: \[\]' "$work/explain"
grep -q '^copied_checksum:.*effects: \["memory"\]; permitted effects: \["memory"\]' "$work/explain"
echo 'PASS: effect ceilings, all tracked effects, recursive imports, cleanup, native code and metadata'
