# Success without a payload

Status: Result<void, E> is implemented. E must be an enum as for other Results.
This supports validation and operations whose only meaningful outcome is success
or failure, without inventing an integer/boolean success value.

```fermi
enum Error { invalid_limit, }

func validate(limit: usize) -> Result<void, Error> {
    if (limit == 0) { return Result.err(Error.invalid_limit); }
    return Result.ok();
}

func run() -> i32 {
    match (validate(1024)) {
        case Result.ok() { return 0; }
        case Result.err(let error: Error) { return 64; }
    }
}
```

## Constructors and handling

The success constructor takes no arguments. Its case has empty parentheses and
creates no binding. Supplying a success value or binding let value: void is an
error. The error constructor and case still require one typed enum payload.
A non-void Result success still requires its normal value and typed case binding.
There is no implicit conversion from a void function call to Result.ok().

All existing mandatory-handling rules remain in force. The Result must be matched, propagated with `try(...)`,
or explicitly forwarded; it cannot be discarded, silently overwritten or left
unhandled at scope exit. Matching consumes a named Result. Passing/returning a
named aggregate still requires the existing explicit transfer rules. Success does
not imply that a side effect or resource lifetime has been completed automatically.

void is not a general value: local void bindings, void parameters and void array
members remain rejected. Result<void, E> itself is an owning Result value and can
be passed to functions or nested in a non-resource Result. Results remain forbidden
in struct fields/arrays under the current handling restrictions. This extension
does not enable nested resource Results or new resource types.

## Lowering, effects and ABI

The internal Result representation stores its success tag and error enum, with
no value field for unit success. No dummy C value or void object is emitted. Unit
cases allocate no payload binding. Constructor, case transfer, forwarding and
fault cleanup use the existing checked control-flow paths.

Unit Results add no external effect. Effects still propagate from the operations
performed by a function, including active buffer cleanup. Under fermi-work-1,
void contributes zero payload cells; the success tag and error enum remain counted.
This extends the model to the new shape without changing bounds for existing
accepted programs. Native stack and timing are not certified.

Exports use the existing C ABI status/code envelope with no value member. A
successful Result<void, E> returns status zero; a domain error returns status one
and the error enum code. Fault and invalid-argument statuses are unchanged.
Existing exported function layouts do not change. Consumers inspect the generated
header and Result<void,E> manifest type rather than assuming every Result has a
value member.

[unit_result.fe](../../examples/cli/unit_result.fe) is executable. Run
sh tests/unit-result.sh for O0/O2 ABI, forwarding, nested matching, branch-state,
fault and rejection cases. Buffer integration tests additionally verify cleanup
on unit-success return, allocation failure and bounds faults.
