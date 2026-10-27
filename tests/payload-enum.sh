#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/payload-enum
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/payload_enum.fe --out-dir "$work" "-$mode"
    "$fermi" build --library tests/cases/payload_owners.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/payload_enum.c "$work/payload_enum.o" -o "$work/values"
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/payload_ownership.c -o "$work/owners"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
            tests/integration/payload_enum.c "$work/payload_enum.o" -o "$work/values"
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/payload_ownership.c -o "$work/owners"
    fi
    "$work/values"
    "$work/owners"
    "$fermi" build examples/cli/event_dispatch/main.fe -o "$work/example" "-$mode"
    "$work/example" > "$work/actual"
    printf 'Job accepted.\nWorker stopped.\n' > "$work/expected"
    cmp "$work/expected" "$work/actual"
done
# Shared nested variants must generate one cleanup routine per type.
depth_source="$work/payload_depth.fe"
printf 'enum E0 { left(Buffer<u8>), right(Buffer<u8>), }\n' > "$depth_source"
level=1
while [ "$level" -lt 31 ]; do
    previous=$((level - 1))
    printf 'enum E%s { left(E%s), right(E%s), }\n' "$level" "$previous" "$previous" >> "$depth_source"
    level=$((level + 1))
done
printf 'export func smoke() -> void { var value: E30; }\n' >> "$depth_source"
"$fermi" build --library "$depth_source" --out-dir "$work" -O0
source_bytes=$(wc -c < "$work/payload_depth.c")
[ "$source_bytes" -lt 65536 ]
"$fermi" explain tests/cases/payload_enum.fe > "$work/explain"
grep -q 'dispatch: static upper bound' "$work/explain"
"$fermi" explain tests/cases/payload_owners.fe > "$work/effects"
grep -q 'memory' "$work/effects"
grep -q 'fs.close' "$work/effects"
echo 'PASS: imported payload types, constructors, patterns and conservative effects'
