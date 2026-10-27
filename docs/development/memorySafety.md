# Memory safety boundaries

## Compiler-owned storage

Syntax and semantic objects live in bounded, aligned compiler arenas. String
copies reject oversized lengths before calculating space for a terminator or
reading source bytes. Empty copies accept a null source and construct a valid
terminated string. Other copies require caller-owned storage of the stated size.
These are internal C contracts; Fermi does not make arbitrary C pointers safe.

## Buffer ownership and initialization

A buffer owns one allocation and its allocator identity. Moving transfers the
whole owner; dropping releases it once. Allocation failures preserve an existing
owner. Capacity bytes are checked against multiplication overflow, PTRDIFF_MAX,
and the owner's byte limit. Growth may temporarily retain both allocations;
the capacity limit is not a process-memory limit.

Only initialized elements below the logical length are accessible. Raw integer
resize zeroes newly exposed elements. Record growth uses typed seed assignment;
zero bytes are not a constructor for arbitrary records. Resource elements remain
excluded until their lifecycle and cleanup semantics are implemented.

`clear` and `truncate` retain capacity for reuse without allocating. Removed
bytes remain in storage. Neither operation promises secret erasure. The checked native truncate adapter updates only the descriptor length. It has
no tracked external effects and one abstract native work unit. Borrowed helpers
can carry a checked zero-heap budget; allocation or owner cleanup still adds the
memory effect. This does not bound native stack usage or elapsed time.

## Verification

- `make check`: compiler unit and runtime lifecycle tests.
- `make sanitize-support sanitize-runtime`: focused C ASan/UBSan checks.
- `sh tests/bufferReuse.sh`: Fermi and tracked-allocation C tests at O0/O2.
- `FERMI_TEST_SANITIZE=1 FERMI_CC=./tests/sanitized-cc.sh sh tests/bufferReuse.sh`:
  instrumented generated code and executable example.
- `make test`: broader ownership, borrow, cleanup, bounds and language regressions.

C ABI callers still supply valid aligned storage and must avoid conflicting
concurrent access. Raw descriptor corruption, arbitrary shallow owner copies,
and invalid custom allocators are outside the safe-language contract.

Sanitizers cover executed paths, rather than proving all possible executions
safe. In particular, ordinary unsigned wraparound requires explicit size guards
and boundary tests. Instrumentation references: [Clang AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
and [Clang UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html).
