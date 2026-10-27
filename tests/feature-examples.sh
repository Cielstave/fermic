#!/bin/sh
set -eu
fermi=${FERMI_TEST_COMPILER:-./build/fermi}
work=build/tests/feature-examples
mkdir -p "$work"
for mode in O0 O2; do
    "$fermi" run examples/features/buffer_batches/main.fe --no-cache "-$mode" > "$work/batch-out" 2> "$work/batch-err"
    printf '25\n' > "$work/batch-expected"
    cmp "$work/batch-expected" "$work/batch-out"
    test ! -s "$work/batch-err"
    "$fermi" run examples/features/text_ranges/main.fe --no-cache "-$mode" > "$work/text-out" 2> "$work/text-err"
    printf 'café\nworker: café\n' > "$work/text-expected"
    cmp "$work/text-expected" "$work/text-out"
    test ! -s "$work/text-err"
    "$fermi" test examples/features/frame_check/main.fe "-$mode" > "$work/frame-tests"
    grep -q '^test result: ok. 4 passed; 0 failed$' "$work/frame-tests"
    "$fermi" test examples/features/owned_jobs/main.fe "-$mode" > "$work/job-tests"
    grep -q '^test result: ok. 3 passed; 0 failed$' "$work/job-tests"
    "$fermi" run examples/features/frame_check/main.fe --no-cache "-$mode" > "$work/frame-out"
    printf 'frame-check: all header checks passed\n' > "$work/frame-expected"
    cmp "$work/frame-expected" "$work/frame-out"
    "$fermi" run examples/features/owned_jobs/main.fe --no-cache "-$mode" > "$work/job-out"
    printf 'owned-jobs: ownership checks passed\n' > "$work/job-expected"
    cmp "$work/job-expected" "$work/job-out"
    "$fermi" build examples/features/file_report/main.fe -o "$work/file-report" "-$mode"
    directory=$(mktemp -d "$work/report-$mode-XXXXXX")
    "$work/file-report" "$directory/status" > "$work/report-out" 2> "$work/report-err"
    test ! -s "$work/report-err"
    printf 'file-report: report created\n' > "$work/report-expected"
    cmp "$work/report-expected" "$work/report-out"
    printf 'status=ready\nqueued=0\n' > "$work/expected"
    cmp "$work/expected" "$directory/status"
    status=0
    "$work/file-report" "$directory/status" > "$work/report-out" 2> "$work/report-err" || status=$?
    test "$status" -eq 74
    test ! -s "$work/report-out"
    grep -q '^file-report: cannot save report; output may be incomplete$' "$work/report-err"
    cmp "$work/expected" "$directory/status"
    status=0
    "$work/file-report" > "$work/report-out" 2> "$work/report-err" || status=$?
    test "$status" -eq 64
    test ! -s "$work/report-out"
    grep -q '^usage: file-report OUTPUT$' "$work/report-err"
done
for source in hidden_output implicit_cleanup; do
    status=0
    "$fermi" check "examples/features/rejected/$source.fe" > "$work/out" 2> "$work/err" || status=$?
    test "$status" -eq 1
    grep -q 'error\[E_EFFECT\]' "$work/err"
    case "$source" in
        hidden_output) grep -q "effect 'io.write'" "$work/err" ;;
        implicit_cleanup) grep -q "effect 'memory'" "$work/err" ;;
    esac
done
echo 'PASS: feature examples O0/O2, native tests, file output, overwrite refusal and effect rejections'
