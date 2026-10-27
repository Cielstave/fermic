#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/record-buffers
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/record-buffers-sanitize; fi
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" test tests/cases/record_buffers.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 3 passed; 0 failed$' "$work/tests"
    "$fermi" build --library tests/cases/record_buffers.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" tests/integration/record_buffers_input.c -o "$work/run"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable -I"$work" tests/integration/record_buffers_input.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" run examples/features/record_buffers/main.fe --no-cache "-$mode" > "$work/example-out"
    printf '2096128\n' > "$work/example-expected"
    cmp "$work/example-expected" "$work/example-out"
done
reject() {
    expected=$1
    source=$2
    printf '%s\n' "$source" > "$work/rejected.fe"
    if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then exit 1; fi
    grep -q "error\[$expected\]" "$work/err"
}
reject E_PARSE 'struct Owner { bytes: Buffer<u8>; } func f(value: Buffer<Owner>) -> void {}'
reject E_GENERIC 'use std.buffer; struct Row { id: usize; } func f() -> Result<Buffer<Row>, buffer.Error> { return buffer.allocate<Row>(1, 100); }'
reject E_GENERIC 'use std.buffer; struct Row { id: usize; } func f(value: edit Buffer<Row>) -> Result<usize, buffer.Error> { return buffer.resize<Row>(edit value, 1); }'
reject E_MOVE 'struct Row { id: usize; } func f(value: Buffer<Row>) -> Buffer<Row> { return copy(value); }'
reject E_ALIAS 'use std.buffer; struct Row { id: usize; } func f(value: edit Buffer<Row>) -> Result<usize, buffer.Error> { return buffer.push<Row>(edit value, copy(value[0])); }'
reject E_ABI 'struct Row { id: usize; } export func f(value: Buffer<Row>) -> void {}'
cat > "$work/model.fe" <<'SOURCE'
pub struct Sealed { private value: usize; }
pub func make() -> Sealed { return Sealed(value = 17); }
pub func value(item: read Sealed) -> usize { return item.value; }
SOURCE
cat > "$work/private.fe" <<'SOURCE'
use "model.fe";
use std.buffer;
func check() -> Result<bool, buffer.Error> {
    var items: Buffer<model.Sealed> = try(buffer.empty<model.Sealed>(1024));
    let length: usize = try(buffer.push<model.Sealed>(edit items, model.make()));
    let value: model.Sealed = try(buffer.pop<model.Sealed>(edit items));
    return Result.ok(length == 1 && model.value(read value) == 17);
}
func test_private_storage() -> bool {
    match (check()) {
        case Result.ok(let valid: bool) { return valid; }
        case Result.err(let error: buffer.Error) { return false; }
    }
}
SOURCE
"$fermi" test "$work/private.fe" > "$work/tests"
grep -q '^test result: ok. 1 passed; 0 failed$' "$work/tests"
printf 'PASS: record buffers, seeded initialization, resource exclusion, privacy and fault cleanup\n'
