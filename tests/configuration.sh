#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/configuration
mkdir -p "$work/std"
for module in env num config; do
    printf 'invalid standard-library shadow\n' > "$work/std/$module.fe"
done
cp examples/cli/startup_config.fe "$work/startup.fe"
printf 'startup configuration accepted\n' > "$work/expected"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/configuration.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/configuration_input.c -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function \
            -Wno-unused-but-set-variable -I"$work" tests/integration/configuration_input.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" build "$work/startup.fe" -o "$work/startup" "-$mode"
    FERMI_WORKERS=4 FERMI_JOB_BYTES=1024 FERMI_ACTIVE_BYTES=4096 \
        "$work/startup" > "$work/out" 2> "$work/err"
    cmp "$work/expected" "$work/out"
    test ! -s "$work/err"
    for value in '' 0 33 -1 secret-value 18446744073709551616 000000000000000000001; do
        status=0
        FERMI_WORKERS="$value" FERMI_JOB_BYTES=1024 FERMI_ACTIVE_BYTES=4096 \
            "$work/startup" > "$work/out" 2> "$work/err" || status=$?
        test "$status" -eq 78
        test ! -s "$work/out"
        grep -q 'FERMI_WORKERS must be a decimal integer' "$work/err"
        if grep -q 'secret-value' "$work/err"; then
            echo 'FAIL: raw configuration value entered diagnostics' >&2
            exit 1
        fi
    done
    status=0
    FERMI_WORKERS=4 FERMI_JOB_BYTES=1024 FERMI_ACTIVE_BYTES=4095 \
        "$work/startup" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 78
    grep -q 'active job payloads exceed' "$work/err"
    status=0
    (unset FERMI_WORKERS; "$work/startup") > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 78
    status=0
    FERMI_WORKERS=invalid "$work/startup" > "$work/out" 2< "$work/expected" || status=$?
    test "$status" -eq 74
    test ! -s "$work/out"
done
"$fermi" explain tests/cases/configuration.fe > "$work/effects.txt"
grep -q '"env.read"' "$work/effects.txt"
grep -q '"heap_bytes": 0, "effects": \[\]' "$work/configuration.contract.json"
reject() {
    if "$fermi" check "$work/rejected.fe" > "$work/rejected.out" 2> "$work/rejected.err"; then
        echo 'FAIL: invalid configuration access accepted' >&2
        exit 1
    fi
    grep -q "$1" "$work/rejected.err"
}
cat > "$work/rejected.fe" <<'SOURCE'
use std.env;
func main() -> i32 { let out: u8[1] = [0]; env.read_into(read "KEY", edit out); return 0; }
SOURCE
reject E_MUT
cat > "$work/rejected.fe" <<'SOURCE'
use std.env;
func main() -> i32 { var out: u8[1] = [0]; env.read_into(read "KEY", edit out); return 0; }
SOURCE
reject E_RESULT
cat > "$work/rejected.fe" <<'SOURCE'
use std.config;
@budget(work = 10000, heap = 0)
func main() -> i32 {
    match (config.usize_env(read "FERMI_WORKERS", 1, 32)) {
        case Result.ok(let value: usize) { return 0; }
        case Result.err(let error: config.Error) { return 1; }
    }
}
SOURCE
reject E_EFFECT
grep -q 'environment access' "$work/rejected.err"

cat > "$work/rejected.fe" <<'SOURCE'
use std.env;
func main() -> i32 { var out: u8[1]; env.read_into(read "KEY", edit out); return 0; }
SOURCE
reject E_INIT
cat > "$work/rejected.fe" <<'SOURCE'
use std.env;
func main() -> i32 {
    var bytes: u8[2] = [65, 0];
    match (env.read_into(read bytes, edit bytes)) {
        case Result.ok(let count: usize) { return 0; }
        case Result.err(let error: env.Error) { return 1; }
    }
}
SOURCE
reject E_ALIAS
"$fermi" explain "$work/startup.fe" > "$work/startup-effects.txt"
grep -q 'effects: \["io.write", "env.read"\]' "$work/startup-effects.txt"
echo 'PASS: startup validation, standard-library isolation, stderr failures and environment effects'
