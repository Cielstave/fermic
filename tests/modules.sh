#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/modules
mkdir -p "$work/pkg" "$work/other"
reject() {
    code=$1
    shift
    if "$fermi" "$@" --diagnostic-format=json >"$work/output" 2>"$work/error"; then
        echo "FAIL: expected module rejection $code" >&2
        exit 1
    fi
    grep -q "\"code\":\"$code\"" "$work/error"
    test ! -s "$work/output"
}
for mode in O0 O2; do
    "$fermi" build examples/cli/modules/main.fe -o "$work/example" "-$mode"
    "$work/example"
    "$fermi" run examples/cli/local_paths/main.fe --no-cache "-$mode" > "$work/local-output"
    printf 'local imports: 42\n' > "$work/local-expected"
    cmp "$work/local-expected" "$work/local-output"
done

cat >"$work/pkg/shared.fe" <<'SOURCE'
pub struct Value { number: i32; }
SOURCE
cat >"$work/pkg/left.fe" <<'SOURCE'
use pkg.shared;
pub func make() -> shared.Value { return shared.Value(number = 7); }
func private_value() -> i32 { return 4; }
SOURCE
cat >"$work/pkg/right.fe" <<'SOURCE'
use pkg.shared;
pub func check(value: shared.Value) -> i32 { return value.number; }
SOURCE
cat >"$work/main.fe" <<'SOURCE'
use pkg.left;
use pkg.right;
func main() -> i32 { return right.check(left.make()) - 7; }
SOURCE
"$fermi" build "$work/main.fe" -o "$work/diamond"
"$work/diamond"
cat >"$work/private.fe" <<'SOURCE'
use pkg.left;
func main() -> i32 { return left.private_value(); }
SOURCE
reject E_PRIVATE check "$work/private.fe"
cat >"$work/shadow.fe" <<'SOURCE'
use pkg.left;
func main() -> i32 { let left: i32 = 1; return left; }
SOURCE
reject E_NAME check "$work/shadow.fe"
cat >"$work/pkg/broken.fe" <<'SOURCE'
pub func value() -> i32 { return missing; }
SOURCE
cat >"$work/broken.fe" <<'SOURCE'
use pkg.broken;
func main() -> i32 { return broken.value(); }
SOURCE
reject E_NAME check "$work/broken.fe"
grep -q 'pkg/broken.fe' "$work/error"
cat >"$work/pkg/a.fe" <<'SOURCE'
use pkg.b;
pub func value() -> i32 { return b.value(); }
SOURCE
cat >"$work/pkg/b.fe" <<'SOURCE'
use pkg.a;
pub func value() -> i32 { return 0; }
SOURCE
cat >"$work/cycle.fe" <<'SOURCE'
use pkg.a;
func main() -> i32 { return a.value(); }
SOURCE
reject E_MODULE_CYCLE check "$work/cycle.fe"
grep -q '<entry> -> pkg.a -> pkg.b -> pkg.a' "$work/error"
cat >"$work/missing.fe" <<'SOURCE'
use absent;
func main() -> i32 { return 0; }
SOURCE
reject E_MODULE check "$work/missing.fe"
cat >"$work/standard.fe" <<'SOURCE'
use std.net;
func main() -> i32 { return 0; }
SOURCE
reject E_MODULE check "$work/standard.fe"
cp "$work/pkg/left.fe" "$work/other/left.fe"
cat >"$work/duplicate.fe" <<'SOURCE'
use pkg.left;
use other.left;
func main() -> i32 { return 0; }
SOURCE
reject E_MODULE check "$work/duplicate.fe"

# A symlink may not enlarge the import root, including when its target is valid source.
cp "$work/pkg/shared.fe" build/tests/outside-module.fe
ln -sf ../../outside-module.fe "$work/pkg/outside.fe"
cat >"$work/escape.fe" <<'SOURCE'
use pkg.outside;
func main() -> i32 { return 0; }
SOURCE
reject E_MODULE check "$work/escape.fe"
grep -q 'outside the project directory' "$work/error"

cat >"$work/library.fe" <<'SOURCE'
use pkg.left;
export func number() -> i32 { return left.make().number; }
SOURCE
"$fermi" build --library "$work/library.fe" --out-dir "$work/library"
grep -q '"name":"pkg.shared"' "$work/library/library.contract.json"
cat >"$work/pkg/bounded.fe" <<'SOURCE'
@budget(work = 1, heap = 0)
pub func value() -> i32 { return 5; }
SOURCE
cat >"$work/budget.fe" <<'SOURCE'
use pkg.bounded;
func main() -> i32 { return bounded.value(); }
SOURCE
reject E_BUDGET check "$work/budget.fe"
grep -q 'pkg/bounded.fe' "$work/error"
cat >"$work/pkg/exported.fe" <<'SOURCE'
export func value() -> i32 { return 0; }
SOURCE
cat >"$work/exports.fe" <<'SOURCE'
use pkg.exported;
export func value() -> i32 { return 0; }
SOURCE
reject E_MODULE check "$work/exports.fe"

echo 'PASS: modules, shared nominal identity, privacy, diagnostics, cycles, import boundaries, manifest'

# Fixed-width integer names belong to the prelude, including import aliases.
for name in i8 u16 u64; do
    printf 'use pkg.%s;\nfunc main() -> i32 { return 0; }\n' "$name" > "$work/prelude.fe"
    reject E_MODULE check "$work/prelude.fe"
    grep -q 'module alias conflicts with a prelude name' "$work/error"
done

# Quoted imports resolve from the importing source file and retain the project root.
mkdir -p "$work/path/app" "$work/path/lib"
cat >"$work/path/main.fe" <<'SOURCE'
use "app/start.fe";
func main() -> i32 { return start.value() - 42; }
SOURCE
cat >"$work/path/app/start.fe" <<'SOURCE'
use "../lib/number.fe";
pub func value() -> i32 { return number.answer(); }
SOURCE
cat >"$work/path/lib/number.fe" <<'SOURCE'
pub func answer() -> i32 { return 42; }
SOURCE
"$fermi" build "$work/path/main.fe" -o "$work/path/relative"
"$work/path/relative"
cat >"$work/path/alias.fe" <<'SOURCE'
use "lib/number.fe" as numeric;
func main() -> i32 { return numeric.answer() - 42; }
SOURCE
"$fermi" build "$work/path/alias.fe" -o "$work/path/explicit-alias"
"$work/path/explicit-alias"
cat >"$work/path/escape.fe" <<'SOURCE'
use "../../outside-module.fe";
func main() -> i32 { return 0; }
SOURCE
reject E_MODULE check "$work/path/escape.fe"
grep -q 'outside the project directory' "$work/error"
echo 'PASS: relative file imports, inferred and explicit aliases, nested resolution and project boundary'
