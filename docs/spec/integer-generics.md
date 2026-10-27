# Finite integer function generics

Status: implemented for one integer type parameter and explicit type arguments.
This document covers integer constraints. The additional nonresource
[data constraint](data-generics.md) admits records and other owning data.

## Declaration and calls

```fermi
@effects()
pub func larger<T: integer>(left: T, right: T) -> T {
    if (left > right) {
        return left;
    }
    return right;
}
```

Call it with `larger<u32>(7, 9)` or `numeric.larger<i32>(-7, 9)` after importing
its module. The parameter name is local to the declaration and cannot shadow a
visible type, Result or Buffer. One parameter may use one of these finite constraints:

| Constraint | Permitted concrete arguments | Admitted types |
| --- | --- | --- |
| integer | u8, i8, u16, i16, u32, i32, u64, i64, usize | 9 |
| signed_integer | i8, i16, i32, i64 | 4 |
| unsigned_integer | u8, u16, u32, u64, usize | 5 |

The chosen constraint applies to every use of T in that declaration. Type arguments
are explicit scalar type names; inference,
multiple arguments, boolean/resource arguments and generic nominal types are not
supported. Existing copy/move and constructors do not accept type arguments.

T may appear in parameter/result types, local types, fixed arrays, scalar views,
Result success payloads and explicit calls to another integer generic function.
Examples include `read T[]`, `edit T[]`, `Result<T, Error>` and `helper<T>(value)`.
The usual restrictions on views, arrays and Result remain in force. Integer
literals must fit their expected concrete types and mixed integer types require
explicit conversion.

## Checking and specialization

Ordinary generic declarations are stored as templates. The parser checks their
syntax once using an admitted integer type; that provisional tree is not checked
semantically or emitted. Each explicit call requests a concrete body. Repeated
calls to the same declaration and canonical type reuse one instance, including
recursive calls. Calls inside a newly requested body may request further bodies.
Each body is parsed from immutable tokens in its defining module, with its own
symbols, control flow, ownership, effects, work analysis, and generated C identity.
No source-text substitution, runtime type dispatch, or boxing is used.

The caller's constraint must admit its requested argument. Typing, literal range,
name resolution, ownership, effects, and budget checks apply to every requested
body. A constraint describes admissible arguments; it does not prove the body
valid for all of them. For example, negation in an integer template can compile
when called with i32 and fail when called with u32. Unrequested bodies may contain
semantic errors, but malformed syntax and invalid declared types still fail when
the declaration is parsed. This replaces the former eager full-domain check.

All non-generic functions are still checked, including otherwise unused helpers;
their calls request generic bodies. This is specialization on static call sites,
not whole-program dead-code elimination or runtime reachability analysis. Calls
in branches remain subject to the usual checker rules. Modules preserve normal
privacy and alias boundaries; function overloading is not introduced.

There are separate project limits of 128 ordinary generic declarations and 128
concrete functions. Unrequested ordinary templates consume no concrete function
slot. Every non-generic function and requested instance consumes one; type/source,
AST, arena, nesting, and local limits also remain active. The integer native allocate/resize
adapters retain eager finite validation (18 functions); the data adapters follow
[their separate checked contract](record-buffers.md). Resource type parameters
and generic structs are not implemented.

## Ownership, effects and ABI

T cannot represent a resource. A generic may still accept an explicit concrete
resource parameter such as Buffer<u8>; ordinary move, borrow, cleanup and alias
checks apply independently to every specialization. No resource becomes copyable
through a generic call.

`@effects` and `@budget` are checked for every requested specialization. An
unrequested template is not certified as meeting either contract. Recursive calls
such as `countdown<T>(value - 1)` resolve to the corresponding specialization;
recursion still has unknown work bound and no termination guarantee.

Generic declarations cannot be native, exported, named main or start with test_.
Expose a concrete non-generic wrapper for a C ABI or a native test entrypoint.
`fermi explain` includes the concrete argument, for example `larger<u32>`.
Invalid/missing explicit arguments use E_GENERIC. A rejected integer type is
named in the diagnostic when it falls outside the declared constraint; ordinary
typing, literal range,
visibility, effect and budget violations retain their existing diagnostic codes.

## Working library use cases and evidence

The [example library](../../examples/features/integer_generics/numeric.fe) shares
one scalar comparison helper across signed/unsigned widths and one typed-view
search helper across slice element types. Neither allocates or performs I/O.

```sh
./build/fermi run examples/features/integer_generics/main.fe
./build/fermi test examples/features/integer_generics/main.fe
sh tests/integer-generics.sh
```

Tests cover all widths, every u8 value, view reads/mutation, empty views, nested
Results, recursion, resource parameters, module isolation/privacy, invalid requested
bodies, declaration/instance limits, and checked overflow. The native suites run at O0/O2 and
under ASan/UBSan; the frontend fuzz corpus includes generic nested Results.

The signed/unsigned domains allow helpers that are not valid over all integers:
negation uses signed_integer, and bit operations use unsigned_integer. Calling a
narrower-domain helper from an integer generic must remain valid for every caller
specialization; an unsigned instantiation cannot call a signed-only helper. Signed
minimum negation still raises checked overflow; the constraint does not remove
arithmetic checks. See [std.bits](bits.md) for an implemented library use case.

## Trusted native integer buffers

The [std.buffer extension](record-buffers.md) has a closed native signature set.
Integer allocate/resize remain eager; nonresource data adapters and ordinary
collection functions instantiate on request. User-defined native/exported generic
functions remain rejected.

`sh tests/demand-generics.sh` checks instance reuse, nested/private helpers,
module-local aliases, recursion and source-context diagnostics at O0/O2.
