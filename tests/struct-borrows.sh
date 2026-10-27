#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/struct-borrows
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/struct-borrows-sanitize; fi
mkdir -p "$work"
cat > "$work/fault.fe" <<'SOURCE'
use std.mem;
struct Packet { bytes: Buffer<u8>; }
func fail(packet: read Packet) -> u8 { return packet.bytes[99]; }
func main() -> i32 {
    match (mem.from_bytes(read "data", 16)) {
        case Result.ok(let bytes: Buffer<u8>) {
            let packet: Packet = Packet(bytes = move(bytes));
            let value: u8 = fail(read packet);
            return widen<i32>(value);
        }
        case Result.err(let error: mem.Error) { return 2; }
    }
}
SOURCE
for mode in O0 O2; do
    "$fermi" test tests/cases/struct_borrows.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 9 passed; 0 failed$' "$work/tests"
    "$fermi" test examples/features/struct_borrows/main.fe "-$mode" > "$work/example-tests"
    grep -q '^test result: ok. 1 passed; 0 failed$' "$work/example-tests"
    "$fermi" run examples/features/struct_borrows/main.fe --no-cache "-$mode" > "$work/output"
    printf 'struct-borrows: records processed\n' > "$work/expected"
    cmp "$work/expected" "$work/output"
    status=0
    "$fermi" run "$work/fault.fe" --no-cache "-$mode" > "$work/output" 2> "$work/error" || status=$?
    test "$status" -eq 1
    test ! -s "$work/output"
    grep -q 'runtime fault\[1\]' "$work/error"
done
echo 'PASS: shared/edit struct borrows, reborrows, fields, arrays, resources and faults'
