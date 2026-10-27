#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/bitwise
mkdir -p "$work/std"
printf 'invalid standard-library shadow\n' > "$work/std/binary.fe"
cp tests/cases/binary.fe "$work/binary.fe"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/bitwise.fe --out-dir "$work" "-$mode"
    "$fermi" build --library "$work/binary.fe" --out-dir "$work" "-$mode"
    for component in bitwise binary; do
        if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
            "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
                "tests/integration/$component.c" "$work/$component.o" -o "$work/run"
        else
            "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
                "tests/integration/$component.c" "$work/$component.o" -o "$work/run"
        fi
        "$work/run"
    done
    "$fermi" build examples/cli/binary_frame.fe -o "$work/frame" "-$mode"
    "$work/frame" > "$work/actual"
    printf 'binary frame validated\n' > "$work/expected"
    cmp "$work/expected" "$work/actual"
    cat > "$work/fault.fe" <<'SOURCE'
func main() -> i32 { let byte: u8 = 1; let count: usize = 8; let value: u8 = byte << count; return 0; }
SOURCE
    "$fermi" build "$work/fault.fe" -o "$work/fault" "-$mode"
    status=0
    "$work/fault" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 1
    test ! -s "$work/out"
    grep -q 'runtime fault\[4\]: shift count outside operand width' "$work/err"
done
# All binary exports must retain a finite work certificate and no external effects.
test "$(grep -c '"proof": "static".*"work_bound": [0-9][0-9]*.*"heap_bytes": 0, "effects": \[\]' "$work/binary.contract.json")" -eq 4

echo 'PASS: binary library isolation, certificates and executable shift diagnostics'
