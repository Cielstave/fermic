#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/resource-structs
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/resource_structs.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/resource_structs.c -o "$work/owners"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/resource_structs.c -o "$work/owners"
    fi
    "$work/owners"
done
"$fermi" explain tests/cases/resource_structs.fe > "$work/effects"
grep -q 'memory' "$work/effects"
grep -q 'fs.close' "$work/effects"
"$fermi" build examples/cli/owned_packet/main.fe -o "$work/example"
"$work/example" > "$work/actual"
printf 'Worker ready.\n' > "$work/expected"
cmp "$work/expected" "$work/actual"
echo 'PASS: imported owning structs, byte views and conservative resource effects'
