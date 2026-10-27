# Native test runner

Status: implemented for one entry source and its local imports.

```sh
fermi test                         # tests.fe in the current directory
fermi test examples/testing/codec/tests.fe
fermi test tests.fe -O0
fermi t tests.fe -O2
```

## Test functions

The runner discovers entry-module functions named `testName` (`test` followed by
an ASCII uppercase letter) or beginning with `test_`. A bare `test` and
`testhelper` remain ordinary helpers. Generic tests are rejected in both forms.
Each must take no arguments and return `bool`. Names follow ordinary identifier
rules. Helpers and imports are checked normally; imported test functions are not
executed. An existing `main` function is compiled but is not called by the runner.
No test keyword, annotation, or special assertion syntax is required.

```fermi
func testAddition() -> bool {
    let result: u32 = 7 + 5;
    return result == 12;
}
```

A true result passes. False or a checked language fault fails. Each invocation
receives a fresh fault slot, so later tests can run after a bounds, arithmetic,
shift or I/O fault. Normal language cleanup still runs before returning from a
faulting test. A fault is reported by its runtime code. Invalid test signatures
and a source with no test functions fail with `E_TEST` before invoking Clang.

## Execution contract

Tests run in declaration order, sequentially in one native process. There is no
process isolation, timeout, rollback, mocking, or automatic cleanup of filesystem
changes. An OS signal, abort, or infinite loop may stop the entire run. Tests share
process state and inherit the caller's environment, working directory and streams.
The process-argument library is initialized with zero user arguments.

Exit status is 0 when all tests pass, 1 for a failed test/build, and 2 for invalid
CLI usage; signal termination follows the existing run-command convention.
Reports list each test and the passed/failed totals. The default optimization is
O2; O0 is also supported. A temporary executable is removed after execution or
build failure. Test builds do not reuse the run cache. `-o`, `--library`,
`--no-cache`, and program arguments after `--` are not accepted.

This first version does not discover directories or packages. Generators, test
filters, coverage, benchmarks and parallel execution need separate contracts.
The [codec example](../../examples/testing/codec/tests.fe) tests endian encoding,
short-input errors and owned buffer copies without a C harness.

Run `sh tests/test-command.sh` for discovery, failure continuation, CLI, default
source, imports and temporary-file regression checks. The suite also runs under
`make sanitize` with a sanitized compiler and generated test executables.
