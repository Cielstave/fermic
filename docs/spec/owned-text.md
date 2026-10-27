# Owned UTF-8 text

`std.text.Text` owns dynamically allocated, validated UTF-8 bytes. It is an ordinary
resource-owning Fermi struct with a private buffer field. Construction, append,
and copying are library operations; there is no new string keyword or implicit
conversion to static `str`.

- `from_utf8(read bytes, limit)` validates and copies input into independent storage.
- `append(edit value, read bytes)` validates the entire suffix before growing.
- `length(read value)` reports bytes; `scalar_length(read value)` reports Unicode scalars.
- `copy_into(read value, edit output)` copies bytes and returns their count.

Errors are explicit. Constructors reject input exceeding the payload limit before
UTF-8 validation. Append reports invalid UTF-8 before allocation errors. Neither
operation publishes an invalid Text. Allocation/growth failures preserve existing
text. Insufficient output preserves the entire destination. Bytes after a successful
copy remain unchanged. No terminator is written; embedded U+0000 is valid text.

Text moves and cleanup follow existing resource rules. Copying an owner, storing
it in a fixed array, and escaping a borrow remain rejected. It can be moved into
structs or payload enums. Shared/read and exclusive/edit borrows are call-scoped.
There is no byte-edit view or conversion to static `str`; callers may copy bytes
into storage they own. Scalar counts are not grapheme or display-column counts.
Normalization, case folding, locale collation, replacement of malformed input,
and code-point indexing are outside this implementation.

## Private fields

`private` is a contextual modifier before a struct field name:

```fermi
pub struct Text { private bytes: Buffer<u8>; }
```

Access and construction naming private fields are permitted only in the defining
module. Imported aliases, nested field projections, read/edit borrows, constructors,
and assignment targets must preserve that rule. Existing fields stay public by
default for compatibility. A field literally named `private` remains valid as
`private: usize;`. Whole-value moves and resource cleanup need no field access.
Privacy applies to Fermi source; generated C is a trusted backend, not a sandbox.

UTF-8 validation accepts Unicode scalar encodings only: overlong forms, isolated
continuations, surrogates, truncation, and code points above U+10FFFF are errors.
Validation and copying are linear in input length. Text ownership carries memory
effects and has unknown static work/heap bounds under the current model. A buffer
payload limit is not a process RSS or latency bound.

Read-only length, scalar counting, and copying do not allocate at runtime, but
the current effect analysis conservatively includes `memory` when projecting
the resource buffer field. Their effect ceilings therefore permit `memory`;
this release does not claim a pure/zero-heap certificate for those wrappers.
The standalone `valid_utf8(read bytes)` helper has an empty effect ceiling.

## API

```fermi
pub func valid_utf8(bytes: read u8[]) -> bool;
pub func from_utf8(bytes: read u8[], limit: usize) -> Result<Text, Error>;
pub func append(value: edit Text, bytes: read u8[]) -> Result<usize, Error>;
pub func length(value: read Text) -> usize;
pub func scalar_length(value: read Text) -> usize;
pub func copy_into(value: read Text, output: edit u8[]) -> Result<usize, Error>;
```

`append` returns the resulting total byte length. Error variants are
`invalid_utf8`, `no_memory`, `size_overflow`, `limit`, `output_too_small`,
`invalid_state`, `invalid_range`, and `not_found`. No text API writes logs or performs I/O. Validation/length
operations read bytes without normalization; append does not repair a sequence
split across two invalid input chunks. Streaming byte chunks must be assembled
into complete UTF-8 before construction or append.

The [executable example](../../examples/features/owned_text/README.md) reads a name
from process arguments. `sh tests/owned-text.sh` runs native, independent decoder,
tracked allocation, field privacy, resource rejection, and CLI checks at O0/O2.

## Replacement, reuse, and comparison

```fermi
pub func replace(value: edit Text, bytes: read u8[]) -> Result<usize, Error>;
pub func clear(value: edit Text) -> Result<usize, Error>;
pub func equal_text(left: read Text, right: read Text) -> bool;
pub func compare_text(left: read Text, right: read Text) -> Order;
```

`replace` replaces the complete contents and returns the new byte length. It
validates input before changing storage, matching append's invalid-UTF-8 error
precedence. Invalid input, limit errors, and failed growth preserve the original
bytes and length. Replacing with empty input succeeds and retains capacity.
Shorter replacement reuses existing capacity. Subsequent growth remains subject
to the original owner limit; shrinking does not reset it.

`clear` sets the logical length to zero and returns zero. It retains the existing
allocation and payload limit for reuse, and does not promise secure erasure.
Neither clear nor replacement permits malformed UTF-8 or exposes mutable bytes.
The Result follows the existing fallible buffer API; no hidden allocation or
implicit error conversion is introduced.

`equal_text` and `compare_text` inspect two borrowed owners without allocation or
copying either text into a scratch buffer. Repeated shared borrows of the same
owner are accepted. Comparison matches the existing static-string byte ordering:
empty text sorts first, a prefix sorts before a longer value, and embedded NUL
is an ordinary byte. Normalization, locale collation, and case folding are not
applied. The conservative resource-projection memory effect remains explicit.
Existing `equal(str, str)` and `compare(str, str)` keep their original signatures.

## Search and byte ranges

```fermi
pub func find(value: read Text, pattern: str) -> Result<usize, Error>;
pub func slice_into(value: read Text, start: usize, end: usize, output: edit u8[]) -> Result<usize, Error>;
pub func truncate(value: edit Text, length: usize) -> Result<usize, Error>;
```

`find` returns the first exact UTF-8 byte match. Empty patterns match at zero,
including empty text. Absence returns `Error.not_found`; it is not an invalid
UTF-8 error. Search performs no allocation and uses a bounded candidate scan,
with worst-case O(text bytes × pattern bytes) work. It does not normalize text.
The pattern is a static `str`; searching with an owned pattern is not yet exposed.

`slice_into` copies the half-open byte range `[start, end)` into caller storage.
Both offsets must be within the text, ordered, and on Unicode scalar boundaries.
Empty ranges at a valid boundary are accepted. Invalid ranges return
`Error.invalid_range` before checking output capacity. Too-small output returns
`Error.output_too_small`. Either error leaves all output bytes unchanged; success
leaves bytes after the copied range unchanged. No terminator is appended.

`truncate` returns the resulting byte length, retaining capacity and the original
payload limit. Growth and offsets inside multibyte encodings return
`Error.invalid_range` without modifying the owner. Zero clears text; truncating
at the current length succeeds. Truncation does not securely erase retained data.
No operation creates an escaping view or exposes editable UTF-8 storage.

These APIs add `invalid_range` and `not_found` to `text.Error`. Exhaustive matches
on that enum must handle both new variants. The
[text range example](../../examples/features/text_ranges/README.md) demonstrates
search, extraction, and truncation.
