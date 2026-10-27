#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
mkdir -p build/tests
for mode in O0 O2; do
    output="build/tests/$mode"
    "$fermi" build --library examples/telemetry/telemetry.fe --out-dir "$output" "-$mode"
    "$fermi" build --library tests/cases/native.fe --out-dir "$output" "-$mode"

    "$fermi" build --library examples/design/filter_above.fe --out-dir "$output" "-$mode"

    "$fermi" build --library examples/sensor_hub/hub.fe --out-dir "$output" "-$mode"

    "$fermi" build --library tests/cases/integer32.fe --out-dir "$output" "-$mode"

    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 -g "-$mode" -fsanitize=address,undefined -I"$output" \
            tests/integration/telemetry.c tests/integration/reference.c \
            "$output/telemetry.c" -o "$output/telemetry-test"
        "$cc" -std=c11 -g "-$mode" -fsanitize=address,undefined -I"$output" \
            tests/integration/native.c "$output/native.c" -o "$output/native-test"
        "$cc" -std=c11 -g "-$mode" -fsanitize=address,undefined -I"$output" \
            tests/integration/filter.c "$output/filter_above.c" -o "$output/filter-test"
        "$cc" -std=c11 -g "-$mode" -fsanitize=address,undefined -I"$output" \
            tests/integration/hub.c "$output/hub.c" -o "$output/hub-test"
        "$cc" -std=c11 -g "-$mode" -fsanitize=address,undefined -I"$output" \
            tests/integration/integer32.c "$output/integer32.c" -o "$output/integer32-test"
    else
        "$cc" -std=c11 -Wall -Wextra -Werror "-$mode" -I"$output" \
            tests/integration/telemetry.c tests/integration/reference.c \
            "$output/telemetry.o" -o "$output/telemetry-test"
        "$cc" -std=c11 -Wall -Wextra -Werror "-$mode" -I"$output" \
            tests/integration/native.c "$output/native.o" -o "$output/native-test"
        "$cc" -std=c11 -Wall -Wextra -Werror "-$mode" -I"$output" \
            tests/integration/filter.c "$output/filter_above.o" -o "$output/filter-test"
        "$cc" -std=c11 -Wall -Wextra -Werror "-$mode" -I"$output" \
            tests/integration/hub.c "$output/hub.o" -o "$output/hub-test"
        "$cc" -std=c11 -Wall -Wextra -Werror "-$mode" -I"$output" \
            tests/integration/integer32.c "$output/integer32.o" -o "$output/integer32-test"
    fi
    "$output/telemetry-test"
    "$output/native-test"
    "$output/filter-test"
    "$output/hub-test"
    "$output/integer32-test"
done
