# Function exit lowering

Implemented: 24 October 2026; extended with byte-buffer cleanup on 25 October 2026.
See [owned byte buffers](../spec/buffers.md) for the source-level API.

## Backend contract

Every generated internal function has one C return statement, in `fe_exit`.
The public C ABI wrappers retain their separate validation and status handling.

- Non-void functions declare a zero-initialized, function-scope `fe_return` slot.
- Local storage is initialized before checking an incoming fault. An entry fault
  therefore cannot bypass initialization of storage that future cleanup will use.
- A source return evaluates its expression completely, saves the value in
  `fe_return`, and branches to `fe_exit`. This includes aggregate and Result values
  returned from nested blocks or match arms.
- Checked operations and failed callees branch to `fe_fault_exit`, which resets
  the return slot to a typed zero value and then enters `fe_exit`. The fault code
  remains authoritative; a zeroed Result is not exposed as a domain error.
- Void fallthrough and explicit void returns use the same epilogue.
- Native stdin/stdout functions also use this path, including empty operations
  and domain-error returns.

`compiler/backend/c/function.c` owns function storage and epilogue emission.
Statement and native I/O lowering use `fe_emit_return`; they must not emit direct
C returns. No heap allocation or runtime dependency is introduced by this change.

## Evaluation and fault behavior

Arguments retain left-to-right evaluation. If an argument faults, later arguments
and the callee are not evaluated. Prior side effects remain visible. A fault in a
callee visits its epilogue and then the caller's epilogue. Each activated internal
function visits its epilogue exactly once.

Resource returns transfer into the return slot and clear the source temporary
before entering the epilogue. Owner temporaries are initialized at function scope,
and the epilogue releases live temporaries followed by locals. Normal nested-scope
exits release their own bindings earlier. Argument handoff and Result matching
similarly invalidate the source cleanup obligation. A pointer to a block-local C
value must never be registered for use by the function epilogue.

## Verification

`sh tests/function-exits.sh` compiles a Fermi fixture at O0 and O2 and inserts a
counter at every generated internal epilogue. The C harness checks both results
and observed exit counts for nested returns, Result array payloads, loops, void
fallthrough, bounds/overflow/division faults, failed argument evaluation, and
native read/write success and errors. It replaces native I/O with deterministic
mocks. The suite runs under `make test` and `make sanitize` (ASan/UBSan).

## Design reference

The [Rust compiler's drop-elaboration guide](https://rustc-dev-guide.rust-lang.org/mir/drop-elaboration.html)
explains why cleanup must distinguish initialized, moved, and conditionally
initialized values. Fermi's shared epilogue is a local backend design choice;
it does not replace the ownership-state analysis described in that reference.
