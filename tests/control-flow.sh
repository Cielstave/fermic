#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/control-flow
mkdir -p "$work"
for mode in O0 O2; do
    for component in control_flow enum_match integer_types loop_control; do
        "$fermi" build --library "tests/cases/$component.fe" --out-dir "$work" "-$mode"
        if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
            "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
                "tests/integration/$component.c" "$work/$component.o" -o "$work/run"
        else
            "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
                "tests/integration/$component.c" "$work/$component.o" -o "$work/run"
        fi
        "$work/run"
    done
    "$fermi" build tests/cases/enum_module.fe -o "$work/enum-module" "-$mode"
    "$work/enum-module"
done
