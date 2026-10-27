#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/performance
mkdir -p "$work"
"$cc" -std=c11 -O2 -Wall -Wextra -Werror tests/performance/measure-build.c -o "$work/measure-build"
{
    "$fermi" --version
    "$cc" --version
    uname -a
    printf '%s\n' 'Backend: -std=c11 -O2; reference: -std=c11 -O2; no LTO.' \
        'Runtime: 101 batches of 128 calls, rotating variant order, warmed inputs.' \
        'CPU affinity, governor and thermal state are not controlled by this runner.' \
        'Build times are end-to-end wall time; object sizes are whole-file bytes.'
} >"$work/environment.txt"
printf 'build,wall_ms,object_bytes\n' >"$work/build.csv"
: >"$work/build.log"
for trial in 1 2 3; do
    for kernel in telemetry filter_above sum_samples; do
        case "$kernel" in
            telemetry) source=examples/telemetry/telemetry.fe ;;
            *) source="examples/design/$kernel.fe" ;;
        esac
        "$work/measure-build" "$kernel-$trial" "$work/$kernel.o" \
            env FERMI_CC="$cc" "$fermi" build --library "$source" --out-dir "$work" -O2 \
            >>"$work/build.csv" 2>>"$work/build.log"
    done
    "$work/measure-build" "reference-decode-$trial" "$work/reference-decode.o" \
        "$cc" -std=c11 -O2 -Wall -Wextra -Werror -I"$work" \
        -c tests/integration/reference.c -o "$work/reference-decode.o" \
        >>"$work/build.csv" 2>>"$work/build.log"
    "$work/measure-build" "reference-kernels-$trial" "$work/reference-kernels.o" \
        "$cc" -std=c11 -O2 -Wall -Wextra -Werror -I"$work" \
        -c tests/performance/reference.c -o "$work/reference-kernels.o" \
        >>"$work/build.csv" 2>>"$work/build.log"
done
"$cc" -std=c11 -O2 -Wall -Wextra -Werror -I"$work" tests/integration/benchmark.c \
    "$work/reference-decode.o" "$work/telemetry.o" -o "$work/decode"
"$cc" -std=c11 -O2 -Wall -Wextra -Werror -I"$work" tests/performance/kernels.c \
    tests/performance/validate.c "$work/reference-kernels.o" \
    "$work/filter_above.o" "$work/sum_samples.o" -o "$work/kernels"
for trial in 1 2 3; do
    "$work/decode" "$work/decode-raw-$trial.csv" >"$work/decode-$trial.csv"
    "$work/kernels" "$work/raw-$trial.csv" >"$work/kernels-$trial.csv"
done
for kernel in telemetry filter_above sum_samples; do
    "$cc" -std=c11 -O2 -Wno-unused-function -Wno-unused-but-set-variable \
        -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize \
        -c "$work/$kernel.c" -o "$work/$kernel.remarks.o" 2>"$work/$kernel.remarks.txt"
done
"$cc" -std=c11 -O2 -I"$work" -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
    -Rpass-analysis=loop-vectorize -c tests/performance/reference.c \
    -o "$work/reference-kernels.remarks.o" 2>"$work/reference-kernels.remarks.txt"
"$cc" -std=c11 -O2 -I"$work" -Rpass=loop-vectorize -Rpass-missed=loop-vectorize \
    -Rpass-analysis=loop-vectorize -c tests/integration/reference.c \
    -o "$work/reference-decode.remarks.o" 2>"$work/reference-decode.remarks.txt"
if command -v llvm-size >/dev/null 2>&1; then
    llvm-size -A "$work/telemetry.o" "$work/filter_above.o" "$work/sum_samples.o" \
        "$work/reference-decode.o" "$work/reference-kernels.o" >"$work/sections.txt"
fi
cat "$work/decode-1.csv" "$work/kernels-1.csv"
printf 'Three runtime trials, build timings and optimization remarks: %s\n' "$work"
