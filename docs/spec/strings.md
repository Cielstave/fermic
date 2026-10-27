# UTF-8 strings

`str` is a two-word, immutable UTF-8 value containing a pointer and byte length. String literals are checked when their expected type is `str`; invalid UTF-8, overlong encodings, surrogate values, truncated sequences, and values above U+10FFFF are rejected with `E_STRING`. A literal without a `str` context retains its existing `read u8[]` meaning for byte-oriented APIs.

```fermi
use std.io;
use std.text;

func service_name() -> str { return "Fermi"; }

func main() -> void {
    var message: str = service_name();
    message = "café"; // Rebinds the value; its bytes remain immutable.
    io.println(read message);
    if (text.scalar_count(message) != 4) { return; }
}
```

Bindings follow ordinary Fermi mutability rules: `let` cannot be reassigned and `var` can be rebound. `str` is copyable, carries no allocation or cleanup effect, and can be used in parameters, results, struct fields, and `Result` payloads. Values currently originate from static literals or other `str` values, so returning or storing them does not create a dangling view. Converting arbitrary byte buffers into `str` is not implicit.

`str.len` reports UTF-8 bytes. `std.text.scalar_count` counts Unicode scalar values by counting non-continuation bytes; it does not count grapheme clusters or display columns. `std.text.equal` compares encoded bytes. Byte indexing is supported; byte editing, character indexing, normalization, and locale-aware operations are not part of static `str`. Dynamically owned text is available separately as [std.text.Text](owned-text.md). Use `read str` with byte-oriented APIs such as `std.io`.

The C export ABI does not yet define a string representation, so exported functions cannot accept or return `str`. Internal generated C represents a string as `const uint8_t *data` and `size_t len`.

## Text queries and ordering

| API | Behavior |
| --- | --- |
| `text.starts_with(value, prefix)` | Tests an exact UTF-8 byte prefix. |
| `text.ends_with(value, suffix)` | Tests an exact UTF-8 byte suffix. |
| `text.compare(left, right)` | Returns `text.Order.less`, `.equal`, or `.greater` in lexicographic byte order. |

An empty prefix or suffix matches every string, including an empty string. A pattern longer than the value does not match. Comparison examines unsigned bytes until a difference; when one value is a prefix of the other, the shorter value sorts first. Equal encodings compare equal regardless of storage location. These functions perform no allocation and have an empty effect ceiling. Case folding, Unicode normalization, and locale collation are not applied.

The [strings example](../../examples/features/strings/main.fe) uses a relative import and a route classifier built from prefix and suffix queries. `tests/strings.sh` checks the library and example at O0/O2.

## Checked byte indexing

`value[index]` reads one unsigned UTF-8 byte and has type `u8`. The index must
have type `usize`, following existing array/view rules. Evaluation computes the
string value before the index, each once. An index greater than or equal to the
byte length raises bounds fault 1; empty strings have no valid index. Constant
indices on str follow the same runtime check; no new static bound proof is claimed.

```fermi
func initial(value: str) -> u8 { return value[0]; }
func test_encoded_byte() -> bool {
    let name: str = "café";
    return name[3] == 195 && name[4] == 169;
}
```

Indexing a continuation byte is allowed: this operation inspects encoded bytes,
not Unicode scalars. It allocates nothing, does not modify the source, and works
with string-valued calls, struct fields, array elements, and transparent aliases.
Generated addresses to the bytes remain const. Assignment and compound assignment
to string bytes report `E_MUT`, including bytes reached through mutable struct
fields or arrays. Mutable str bindings and fields may still be rebound as a whole.

String slices do not produce new str values. Use call-scoped byte views for raw
byte ranges, or the checked owned-text range APIs when scalar boundaries matter.
The C export restriction on str values remains unchanged; exported functions can
read internal strings and return scalar bytes through the established scalar ABI.
