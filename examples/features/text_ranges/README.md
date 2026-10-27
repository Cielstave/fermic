# UTF-8 text ranges

Run from the repository root:

```sh
./build/fermi run examples/features/text_ranges/main.fe
```

Expected output:

```text
café
worker: café
```

The example searches owned text, copies a byte range into caller-owned storage,
and truncates the owner without splitting a Unicode scalar. Offsets count bytes,
not characters. Fallible operations propagate explicit `text.Error` values.
