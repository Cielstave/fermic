#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/data-generics
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/data-generics-sanitize; fi
mkdir -p "$work/std"
printf 'invalid standard module shadow\n' > "$work/std/slice.fe"
for mode in O0 O2; do
    "$fermi" test tests/cases/data_generics.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 5 passed; 0 failed$' "$work/tests"
    "$fermi" build --library tests/cases/data_generics.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" tests/integration/data_generics_input.c -o "$work/run"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable -I"$work" tests/integration/data_generics_input.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" test examples/features/data_generics/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 1 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/data_generics/main.fe --no-cache "-$mode" > "$work/example-out"
    printf 'data-generics: inventory and token checks passed\n' > "$work/example-expected"
    cmp "$work/example-expected" "$work/example-out"
done
reject() {
    expected=$1
    source=$2
    printf '%s\n' "$source" > "$work/rejected.fe"
    status=0
    "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 1
    grep -q "error\[$expected\]" "$work/err"
}
reject E_GENERIC 'type Owner = Buffer<u8>; func f<T: data>(value: T) -> void {} func g(value: Owner) -> void { f<Owner>(move(value)); }'
reject E_GENERIC 'struct Owner { bytes: Buffer<u8>; } func f<T: data>(value: T) -> void {} func g(value: Owner) -> void { f<Owner>(move(value)); }'
reject E_GENERIC 'enum Owner { none, bytes(Buffer<u8>), } func f<T: data>(value: T) -> void {} func g(value: Owner) -> void { f<Owner>(move(value)); }'
reject E_GENERIC 'enum Error { bad, } type Outcome = Result<u8, Error>; func f<T: data>(value: T) -> void {} func g(value: Outcome) -> void { f<Outcome>(move(value)); }'
reject E_PARSE 'struct Owner { bytes: Buffer<u8>; } func f(values: read Owner[]) -> void {}'
reject E_PARSE 'enum Owner { none, bytes(Buffer<u8>), } func f(values: edit Owner[]) -> void {}'
reject E_MUT 'struct Row { id: usize; } func f(values: read Row[]) -> void { values[0].id = 1; }'
reject E_MUT 'use std.slice; struct Row { id: usize; } func f(values: read Row[]) -> Result<void, slice.Error> { return slice.swap<Row>(edit values, 0, 1); }'
reject E_MOVE 'func f<T: data>(value: T) -> T { return value; } struct Row { id: usize; } func g() -> Row { return f<Row>(Row(id = 1)); }'
reject E_ABI 'struct Row { id: usize; } export func f(values: read Row[]) -> void {}'
reject E_ABI 'export func f(values: read str[]) -> void {}'
reject E_ALIAS 'use std.slice; struct Row { id: usize; } func f(values: edit Row[]) -> Result<usize, slice.Error> { return slice.copy_into<Row>(read values, edit values); }'
reject E_TYPE 'use std.slice; struct First { id: usize; } struct Second { id: usize; } func f(input: read First[], output: edit Second[]) -> Result<usize, slice.Error> { return slice.copy_into<First>(read input, edit output); }'
cat > "$work/left.fe" <<'SOURCE'
pub struct Row { id: usize; }
pub struct Sealed { private value: usize; }
pub func sealed() -> Sealed { return Sealed(value = 17); }
pub func value(item: read Sealed) -> usize { return item.value; }
SOURCE
printf 'pub struct Row { id: usize; }\n' > "$work/right.fe"
cat > "$work/modules.fe" <<'SOURCE'
use "left.fe";
use "right.fe";
use std.slice;
func duplicate<T: data>(value: T) -> T { return copy(value); }
func test_modules() -> bool {
    var flags: bool[1] = [false];
    slice.fill<bool>(edit flags, true);
    let a: left.Row = duplicate<left.Row>(left.Row(id = 1));
    let b: right.Row = duplicate<right.Row>(right.Row(id = 2));
    let secret: left.Sealed = duplicate<left.Sealed>(left.sealed());
    return flags[0] && a.id == 1 && b.id == 2 && left.value(read secret) == 17;
}
SOURCE
"$fermi" test "$work/modules.fe" > "$work/tests"
grep -q '^test result: ok. 1 passed; 0 failed$' "$work/tests"
"$fermi" explain "$work/modules.fe" > "$work/explain"
grep -q 'duplicate<local[.]left[.]fe[.]Row>:' "$work/explain"
grep -q 'duplicate<local[.]right[.]fe[.]Row>:' "$work/explain"
reject E_ACCESS 'use "left.fe"; func leak<T: data>(value: T) -> usize { return value.value; } func f() -> usize { return leak<left.Sealed>(left.sealed()); }'
printf 'PASS: data generics, record views, privacy, nominal identity, ownership and scalar ABI boundary\n'
