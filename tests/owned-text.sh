#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/owned-text
if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then work=build/tests/owned-text-sanitize; fi
mkdir -p "$work"
cat > "$work/model.fe" <<'SOURCE'
pub struct Record { private value: usize; public_value: usize; }
pub type Alias = Record;
pub func make() -> Record { return Record(value = 7, public_value = 9); }
pub func value(record: read Record) -> usize { return record.value; }
pub func change(record: edit Record) -> void { record.value += 1; }
SOURCE
cat > "$work/accepted.fe" <<'SOURCE'
use "model.fe";
struct Compatible { private: usize; }
func test_private_fields() -> bool {
    var record: model.Alias = model.make();
    model.change(edit record);
    let compatible: Compatible = Compatible(private = 3);
    return model.value(read record) == 8 && record.public_value == 9 && compatible.private == 3;
}
SOURCE
for mode in O0 O2; do
    "$fermi" test tests/cases/owned_text.fe "-$mode" > "$work/tests"
    grep -q '^test result: ok. 9 passed; 0 failed$' "$work/tests"
    "$fermi" test "$work/accepted.fe" "-$mode" > "$work/private-tests"
    grep -q '^test result: ok. 1 passed; 0 failed$' "$work/private-tests"
    "$fermi" build --library tests/cases/owned_text.fe --out-dir "$work" "-$mode"
    if [ "${FERMI_TEST_SANITIZE:-0}" = 1 ]; then
        "${CC:-clang}" -std=c11 "-$mode" -fsanitize=address,undefined -I"$work" tests/integration/owned_text_input.c -o "$work/run"
    else
        "${CC:-clang}" -std=c11 "-$mode" -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable -I"$work" tests/integration/owned_text_input.c -o "$work/run"
    fi
    "$work/run"
    "$fermi" build examples/features/owned_text/main.fe "-$mode" -o "$work/greeting"
    "$work/greeting" 'café' > "$work/out" 2> "$work/error"
    printf 'Hello, café!\n' > "$work/expected"
    cmp "$work/expected" "$work/out"
    test ! -s "$work/error"
    status=0
    raw=$(printf '\377')
    "$work/greeting" "$raw" > "$work/out" 2> "$work/error" || status=$?
    test "$status" -eq 65
    test ! -s "$work/out"
    grep -q '^greeting: name is not valid UTF-8$' "$work/error"
done
reject() {
    expected=$1
    source=$2
    printf '%s\n' "$source" > "$work/rejected.fe"
    status=0
    "$fermi" check "$work/rejected.fe" > "$work/out" 2> "$work/error" || status=$?
    test "$status" -eq 1
    grep -q "error\[$expected\]" "$work/error"
}
reject E_ACCESS 'use "model.fe"; func f() -> usize { let value: model.Record = model.make(); return value.value; }'
reject E_ACCESS 'use "model.fe"; func f() -> void { var value: model.Alias = model.make(); value.value = 4; }'
reject E_ACCESS 'use "model.fe"; func f() -> void { let value: model.Alias = model.Alias(value = 3, public_value = 4); }'
reject E_ACCESS 'use "model.fe"; func f(value: read model.Record) -> usize { return value.value; }'
reject E_ACCESS 'use "model.fe"; struct Outer { record: model.Record; } func f() -> usize { let value: Outer = Outer(record = model.make()); return value.record.value; }'
reject E_ACCESS 'use std.text; func f(value: read text.Text) -> usize { return value.bytes.len; }'
reject E_ACCESS 'use std.text; use std.mem; func f() -> Result<text.Text, mem.Error> { return Result.ok(text.Text(bytes = try(mem.bytes(1, 8)))); }'
reject E_MOVE 'use std.text; func f(value: text.Text) -> void { let other: text.Text = copy(value); }'
reject E_PARSE 'use std.text; type Texts = text.Text[2];'
reject E_PARSE 'use std.text; func f(value: read text.Text) -> read text.Text {}'
printf 'PASS: owned text O0/O2, private fields, resource rules and failure preservation\n'
