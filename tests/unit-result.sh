#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
cc=${CC:-clang}
work=build/tests/unit-result
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" build --library tests/cases/unit_result.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "$cc" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" \
            tests/integration/unit_result.c "$work/unit_result.o" -o "$work/run"
    else
        "$cc" -std=c11 "-$mode" -Wall -Wextra -Werror -I"$work" \
            tests/integration/unit_result.c "$work/unit_result.o" -o "$work/run"
    fi
    "$work/run"
    "$fermi" build examples/cli/unit_result.fe -o "$work/example" "-$mode"
    "$work/example" > "$work/actual"
    printf 'unit Result validated\n' > "$work/expected"
    cmp "$work/expected" "$work/actual"
done
reject() {
    printf 'enum Error { rejected, }\n%s\n' "$2" > "$work/rejected.fe"
    if "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/err"; then
        echo 'FAIL: invalid unit Result accepted' >&2
        exit 1
    fi
    grep -q "$1" "$work/err"
}
reject E_CALL 'func f() -> Result<void, Error> { return Result.ok(0); }'
reject E_CALL 'func f() -> Result<void, Error> { return Result.err(); }'
reject E_CALL 'func f() -> Result<usize, Error> { return Result.ok(); }'
reject E_MATCH 'func f(result: Result<void, Error>) -> void {
    match (result) { case Result.ok(let value: void) {} case Result.err(let error: Error) {} }
}'
reject E_MATCH 'func f(result: Result<usize, Error>) -> void {
    match (result) { case Result.ok() {} case Result.err(let error: Error) {} }
}'
reject E_MATCH 'func f(result: Result<void, Error>) -> void {
    match (result) { case Result.ok() {} case Result.err() {} }
}'
reject E_MATCH 'func f(result: Result<void, Error>) -> void { match (result) { case Result.ok() {} } }'
reject E_RESULT 'func checked() -> Result<void, Error> { return Result.ok(); }
func f() -> void { checked(); }'
reject E_RESULT 'func f() -> void { let result: Result<void, Error> = Result.ok(); }'
reject E_INIT 'func f(result: Result<void, Error>) -> Result<void, Error> {
    match (result) { case Result.ok() {} case Result.err(let error: Error) {} }
    return move(result);
}'
reject E_TYPE 'func f() -> void { var value: void; }'
grep -q '"result": "Result<void,Error>"' "$work/unit_result.contract.json"
echo 'PASS: unit Result constructor, matching and mandatory-handling rejection'
