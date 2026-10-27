# Explicit Result propagation

Status: implemented in the native compiler.

`try(expression)` consumes a `Result<T, E>`. Success produces its `T` payload;
error returns `Result.err(error)` from the enclosing function. That function must
return `Result<U, E>` with the exact same nominal fieldless enum error type. `T` and `U` may
differ. There is no implicit conversion, panic, allocation, or exception handler.

```fermi
use std.io;

func write_all(bytes: read u8[]) -> Result<void, io.Error> {
    let written: usize = try(io.write(read bytes));
    return Result.ok();
}
```

## Evaluation and ownership

The operand is evaluated once, at its ordinary left-to-right expression position.
An error skips the rest of that expression and function. Short-circuit operands
remain conditional. Faults retain their separate runtime fault path.

Like `match`, `try(result)` consumes an existing Result binding. It cannot be used
again unless reinitialized. A successful resource payload transfers to its new
owner. Existing resource temporaries and locals are cleaned on error, success
scope exits, and faults through the normal function epilogue.

Every other live Result binding must already be handled at the propagation point,
just as at an explicit return. A possible early error must not silently discard
an unrelated Result. Branch and loop ownership checks remain unchanged.

The operand must have an independently known Result type, normally from a function
call or typed binding. Constructors such as `Result.ok(...)` require an explicit
Result context; `try` does not invent one. `try` of a unit-success Result produces
void and can be an expression statement. Non-void success values cannot be ignored.

## Analysis and diagnostics

`E_TRY` reports a non-Result operand, non-Result enclosing function, or incompatible
error type. Existing `E_RESULT`, `E_INIT`, and `E_TYPE` checks still apply.

Effects include the operand and resource payload. Work analysis includes the
operand, tag check, success payload transfer, and enclosing Result transfer as a
conservative bound. Early returns do not subtract later source work. No elapsed
time, stack, or fault-freedom guarantee follows from this syntax.

This addition reserves `try` as a keyword. Parentheses are required.

## Native interface and verification

The generated C ABI and Result layout are unchanged. Exported wrappers still
distinguish success, domain error, fault, and invalid host arguments. Partial
mutations made before propagation remain visible; try does not provide rollback.

Run `sh tests/try-result.sh` for native O0/O2 evaluation and cleanup regressions.
[try_result.fe](../../examples/cli/try_result.fe) demonstrates bounded configuration
validation. The compiler's semantic suite covers invalid operands, incompatible
errors, pending Results, consumed bindings, loop back edges, and malformed syntax.
