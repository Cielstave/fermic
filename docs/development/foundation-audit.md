# Foundation audit: borrowed argument evaluation

Status: checker correction implemented; verification results are recorded in the
progress log. Scope: call-scoped byte views, argument evaluation and owner cleanup.
This review does not certify the entire compiler or runtime as memory-safe.

## Finding

A view argument could retain a pointer to an owner that was consumed while
constructing a different argument's slice. The checker compared direct storage
owners and inspected non-view arguments, but skipped nested uses inside a view
with a different storage owner.

The reproducer in `tests/cases/borrow_bounds_invalid.fe` borrowed a Buffer in the
first argument and passed `move(buffer)` to a helper in the second view's lower
bound. The helper released the owner before the receiving function read its view.
The original compiler accepted the program; ASan reported heap-use-after-free.

## Correction

Inspect each other argument's expression independently of its sibling list.
Different storage owners do not exempt nested expressions from the borrow rule.
For views of the same owner, inspect bound expressions separately from the storage
path, allowing ordinary shared reads without allowing hidden nested owner use.
The analysis no longer temporarily detaches argument-list nodes.

The reproducer now fails with E_ALIAS. Reverse evaluation that reads an already
moved owner fails with E_INIT. Conservative rejection also covers owner-dependent
bounds that only read: evaluate those bounds into scalar locals before the call.
No new language syntax or runtime operation is introduced.

## Regression coverage

- Lower/upper bounds, nested slices, and field-based storage paths.
- Read and edit view arguments, same-owner nested mutation, and both argument orders.
- Accepted repeated read views, literal disjoint edits, precomputed bounds, and
  consuming an unrelated owner while constructing a view.
- CLI rejection in check/build modes at O0/O2 and an additional frontend fuzz seed.

Existing suites continue to exercise allocation failure, exact cleanup, byte
copy lifetime, arithmetic/bounds faults and the native ABI. Compiler fuzzing does
not execute generated C; it complements, rather than replaces, runtime tests.

## Remaining foundation work

Descriptor borrowing and growth are now integrated with dedicated relocation
checks: index/slice bounds and assignment values cannot reborrow the target
descriptor. The [integration design](../design/buffer-borrows.md) records the
additional checker and generated-program tests. Preserve these guards as resource
forms expand; this earlier argument-evaluation correction remains a regression. Source-mapped
runtime faults, native-stack bounds and relocatable installation remain open work;
this correction does not establish those properties.

## Generated-name collision

An export named `file_read` with `std.fs` loaded once reached Clang and collided
with the embedded `fermi_file_read` runtime function. Exports named `inspect`
and `inspect_result` likewise collided with the generated result typedef.
The checker now rejects both with `E_ABI` before invoking Clang. Known embedded
runtime function names are reserved across builds, even when their module is not
loaded. The generated C ABI still uses its experimental `fermi_` wrapper names;
future runtime symbols must be added to this collision check.
