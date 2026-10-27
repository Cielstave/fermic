#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/buffers
mkdir -p "$work/std"
printf 'invalid project shadow\n' > "$work/std/mem.fe"
cp examples/cli/buffer.fe "$work/example.fe"
printf 'Fermi\n' > "$work/expected"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/buffers.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/buffer_ownership.c -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/buffer_ownership.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" build --library tests/cases/buffers_second.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/buffer_abi.c "$work/buffers.o" "$work/buffers_second.o" -o "$work/abi"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
            tests/integration/buffer_abi.c "$work/buffers.o" "$work/buffers_second.o" -o "$work/abi"
    fi
    "$work/abi"
    "$fermi" build "$work/example.fe" -o "$work/example" "-$mode"
    "$work/example" > "$work/actual"
    cmp "$work/expected" "$work/actual"
    "$fermi" build examples/cli/owned_bytes.fe -o "$work/owned_bytes" "-$mode"
    "$work/owned_bytes" > "$work/actual"
    cmp "$work/expected" "$work/actual"
    "$fermi" build examples/cli/buffer_growth.fe -o "$work/growth" "-$mode"
    "$work/growth" > "$work/actual"
    cmp "$work/expected" "$work/actual"
done
"$fermi" explain tests/cases/buffers.fe > "$work/explain.json"
grep -q '"memory"' "$work/explain.json"
grep -q '"heap_bytes": null' "$work/buffers.contract.json"
reject() {
    if "$fermi" check "$work/rejected.fe" > "$work/rejected.out" 2> "$work/rejected.err"; then
        echo 'FAIL: invalid resource program accepted' >&2
        exit 1
    fi
    grep -q "$1" "$work/rejected.err"
}
cat > "$work/rejected.fe" <<'SOURCE'
use std.mem;
func main() -> i32 { mem.bytes(8, 8); return 0; }
SOURCE
reject E_RESULT
cat > "$work/rejected.fe" <<'SOURCE'
use std.mem;
@budget(work = 1000, heap = 0)
func main() -> i32 { return helper(); }
func helper() -> i32 {
    match (mem.bytes(8, 8)) {
        case Result.ok(let bytes: Buffer<u8>) { return 0; }
        case Result.err(let error: mem.Error) { return 1; }
    }
}
SOURCE
reject E_EFFECT
cat > "$work/rejected.fe" <<'SOURCE'
use std.mem;
func main() -> i32 {
    match (mem.bytes(8, 8)) {
        case Result.ok(let bytes: Buffer<u8>) { return 0; }
        case Result.err(let error: mem.Error) { return 1; }
    }
}
SOURCE
"$fermi" explain "$work/rejected.fe" > "$work/effects.txt"
grep -q 'heap: unknown; effects: \["memory"\]' "$work/effects.txt"
cat > "$work/rejected.fe" <<'SOURCE'
use Buffer;
func main() -> i32 { return 0; }
SOURCE
reject 'module alias conflicts with a prelude name'

cat > "$work/rejected.fe" <<'SOURCE'
use std.mem;
func main() -> i32 { mem.from_bytes(read "data", 4); return 0; }
SOURCE
reject E_RESULT
cat > "$work/rejected.fe" <<'SOURCE'
use std.mem;
@budget(work = 1000, heap = 0)
func main() -> i32 {
    match (mem.from_bytes(read "data", 4)) {
        case Result.ok(let buffer: Buffer<u8>) { return 0; }
        case Result.err(let error: mem.Error) { return 1; }
    }
}
SOURCE
reject E_EFFECT

cp tests/cases/borrow_bounds_invalid.fe "$work/rejected.fe"
reject E_ALIAS
for mode in O0 O2; do
    if "$fermi" build "$work/rejected.fe" -o "$work/borrow_bounds" "-$mode" \
        > "$work/rejected.out" 2> "$work/rejected.err"; then
        echo 'FAIL: build accepted an owner consumed by another view argument' >&2
        exit 1
    fi
    grep -q 'E_ALIAS' "$work/rejected.err"
done

cat > "$work/rejected.fe" <<'SOURCE'
use std.mem;
@budget(work = 1000, heap = 0)
func grow(buffer: edit Buffer<u8>) -> Result<usize, mem.Error> {
    return mem.resize(edit buffer, 8);
}
func main() -> i32 { return 0; }
SOURCE
reject E_EFFECT
