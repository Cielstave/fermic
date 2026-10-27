# Runtime buffer tests

Run `make test-runtime` for the standalone C runtime tests and
`make sanitize-runtime` for AddressSanitizer/UndefinedBehaviorSanitizer coverage.
They are also dependencies of `make test` and `make sanitize` respectively.
The compiler uses creation, reserve, resize and drop for `Buffer<u8>`.
Generated-program ownership/growth tests run separately through `sh tests/buffers.sh`.

The test allocator records every acquisition/release, checks guard bytes, and can
fail a selected allocation attempt. Tests cover scalar widths, overflow and byte
limits, empty owners, failure-preserving growth, zeroing after shrink/regrow,
allocator identity across moves, occupied destinations, and duplicate cleanup of
vacant slots. A four-allocation ownership chain is rerun with each site failing.

A deterministic 10,000-operation sequence compares resize/reserve/write/move
behavior with an independent byte model, including injected allocation failures.
This is model-based testing, not fuzzing or a proof of compiler ownership safety.
Automatic scope/return/fault cleanup is covered by the generated-program suite;
descriptor borrows and source-level growth are covered there as well.
