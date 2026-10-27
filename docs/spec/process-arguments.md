# Process arguments

Status: implemented for native executables through std.args. The Fermi entry
signature remains func main() -> i32. No new keyword or nested view type is added.

## Public API

```fermi
use std.args;

pub enum Error {
    unavailable,
    out_of_range,
    output_too_small,
    invalid_argument,
}
pub native func count() -> Result<usize, Error>;
pub native func read_into(index: usize, output: edit u8[]) -> Result<usize, Error>;
```

count returns the number of user arguments, excluding argv[0]. Index zero names
the first user argument. The executable name is deliberately not exposed as an
argument or used as trusted program identity. Zero user arguments is successful
with count zero; it is distinct from unavailable process context.

read_into copies the complete argument into caller storage and returns its byte
length, without a terminator. An empty argument succeeds with length zero, even
with a zero-length output. An index at least count returns out_of_range. A short
output returns output_too_small without changing any destination byte. Successful
copies leave bytes after the returned length untouched. There is no prefix copy,
implicit allocation, Unicode validation, escaping or shell interpretation.

Arguments are raw terminated host byte strings; embedded NUL cannot be represented
by this API. Non-UTF-8 bytes are retained. The caller chooses its buffer capacity;
there is no additional language-wide argument length limit. Scanning inspects at
most capacity + 1 bytes before copying, not the full length of an oversized value.
OS command-line limits remain separate from the chosen output capacity.

## Startup and embedding

The executable backend uses C main(argc, argv) only when the standard argument
module is present. Before entering Fermi, it initializes a private runtime context
once. The runtime accepts argc zero, validates basic startup shape and excludes
the program-name slot. No mutable argument pointer escapes into Fermi. read/edit,
initialization checks and Result handling apply normally at calls.

Generated libraries have private, initially unavailable contexts. Calling their
exports does not discover the host's arguments through constructors, /proc or
platform-specific linker hooks. Their argument calls return unavailable. There
is currently no public context initialization ABI for embedded Fermi libraries;
C hosts can pass explicit byte views to exported application functions instead.
Separate generated objects do not share argument state or public runtime symbols.

The internal C runtime requires live argument vectors and terminated strings for
all reads. Context initialization precedes publication; native hosts must serialize
mutation with readers and must not supply argument storage as output. Invalid C
runtime destinations, missing length slots or missing vector entries can return
invalid_argument. Safe Fermi source cannot construct these malformed pointers;
C ABI wrappers validate null/nonempty views before entering the function.

Both APIs carry the transitive process.args effect. Work and external heap bounds
remain unknown in compiler metadata; @budget functions reaching them receive
E_EFFECT. The adapter itself performs no allocation. This distinction is
conservative effect accounting, not a claim that count performs an expensive scan.
Pure parsing and [formatting](formatting.md) retain independent certificates.

## Executable example

[bytecount](../../examples/tools/bytecount/README.md) composes arguments, decimal
validation, bounded buffers, stdin, numeric formatting and stderr reporting:

```sh
make
./build/fermi build examples/tools/bytecount/main.fe -o build/bytecount -O2
printf 'Fermi\n' | ./build/bytecount 100
# 6
```

Run sh tests/arguments.sh for generated-library checks, native argument fidelity,
CLI failure paths and the streaming example. make test-runtime covers independent
C argument contexts and destination preservation over varying capacities.
