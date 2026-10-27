#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/typed-buffers
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/typed-buffers-sanitize; fi
mkdir -p "$work/std"
printf 'invalid project shadow\n' > "$work/std/buffer.fe"
cat > "$work/shadow.fe" <<'SOURCE'
use std.buffer;
func test_standard_module() -> bool {
    match (buffer.allocate<i32>(0, 0)) {
        case Result.ok(let values: Buffer<i32>) { return values.len == 0; }
        case Result.err(let error: buffer.Error) { return false; }
    }
}
SOURCE
for mode in O0 O2; do
    "$fermi" test tests/cases/typed_buffers.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 2 passed; 0 failed$' "$work/tests"
    "$fermi" test "$work/shadow.fe" "-$mode" > "$work/shadow-tests"
    grep -q '^test result: ok. 1 passed; 0 failed$' "$work/shadow-tests"
    "$fermi" build --library tests/cases/typed_buffers.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" tests/integration/typed_buffers_input.c -o "$work/run"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable -I"$work" tests/integration/typed_buffers_input.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" build examples/features/typed_buffers/main.fe "-$mode" -o "$work/series"
    for count in 1 8 9 128; do
        "$work/series" "$count" > "$work/out" 2> "$work/error"
        test ! -s "$work/error"
        expected=$((count * (count - 1) / 2 - 4 * count))
        printf '%s\n' "$expected" > "$work/expected"
        cmp "$work/expected" "$work/out"
    done
    status=0
    "$work/series" 129 > "$work/out" 2> "$work/error" || status=$?
    test "$status" -eq 65
    test ! -s "$work/out"
    grep -q '^series: count must be a decimal integer in 1..128$' "$work/error"
done
reject() {
    expected=$1
    source=$2
    printf '%s\n' "$source" > "$work/rejected.fe"
    status=0
    "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/error" || status=$?
    test "$status" -eq 1
    grep -q "error\[$expected\]" "$work/error"
}
reject E_PARSE 'enum Error { bad, } type Outcome = Result<u8, Error>; func f(value: Buffer<Outcome>) -> void {}'
reject E_PARSE 'struct S { value: Buffer<u8>; } func f(value: Buffer<S>) -> void {}'
reject E_PARSE 'type Owner = Buffer<u8>; func f(value: Buffer<Owner>) -> void {}'
reject E_PARSE 'type View = read u8[]; func f(value: Buffer<View>) -> void {}'
reject E_MOVE 'func f(value: Buffer<i32>) -> void { let copied: Buffer<i32> = copy(value); }'
reject E_TYPE 'func f(value: Buffer<i32>) -> void { let moved: Buffer<u32> = move(value); }'
reject E_TYPE 'use std.buffer; func f() -> void {} func view(value: read u8[]) -> void {} func bad(value: Buffer<u32>) -> void { view(read value); }'
reject E_ACCESS 'use std.buffer; func f(value: edit Buffer<u32>) -> void { let size: Result<usize, buffer.Error> = buffer.resize<i32>(edit value, 1); }'
reject E_MUT 'use std.buffer; func f() -> Result<usize, buffer.Error> { let values: Buffer<i32> = try(buffer.allocate<i32>(0, 32)); return buffer.push<i32>(edit values, 1); }'
reject E_PARSE 'pub native func allocate<T: integer>(length: usize, limit: usize) -> T;'
reject E_EFFECT 'use std.buffer; @effects() func f() -> void { match (buffer.allocate<i32>(0, 0)) { case Result.ok(let value: Buffer<i32>) {} case Result.err(let error: buffer.Error) {} } }'
reject E_ALIAS 'use std.buffer; func f(value: edit Buffer<i32>) -> Result<usize, buffer.Error> { return buffer.extend<i32>(edit value, read value); }'
reject E_TYPE 'use std.buffer; func f(value: edit Buffer<i32>, source: read u32[]) -> Result<usize, buffer.Error> { return buffer.extend<i32>(edit value, read source); }'
printf 'PASS: typed buffer O0/O2 values, ownership, effects and module isolation\n'
