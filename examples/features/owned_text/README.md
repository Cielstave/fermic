# Owned UTF-8 text

This executable copies an argument into owned text, appends a prefix and suffix,
and writes a greeting. The argument is runtime input; malformed UTF-8 is rejected
without publishing a partial greeting. Message storage is limited to 256 bytes.

```sh
./build/fermi run examples/features/owned_text/main.fe -- 'café'
sh tests/owned-text.sh
```

Expected output:

```text
Hello, café!
```

`std.text.Text` owns its buffer. Move it into another binding, struct, or payload
enum when transferring ownership; copying owners is rejected. Its private field
prevents external byte mutation or construction that bypasses UTF-8 validation.
`text.append` reports growth errors and preserves the previous text on failure.
`text.copy_into` copies bytes into caller-owned output; it does not expose an
escaping borrow. Static `str` remains a separate type.

The integration suite checks all encoded values from U+0000 through U+10FFFF
(including rejection of surrogate encodings), truncated encodings, all one- and
two-byte inputs, and 10,000 seeded byte sequences against an independent decoder.
It also checks destination sentinels, memory allocation failures, preservation
after rejected append, ownership cleanup, private fields through aliases and
nested projections, and the executable's invalid-argument handling at O0/O2.

## Reusing a text buffer

```fermi
func reset() -> Result<usize, text.Error> {
    var value: text.Text = try(text.from_utf8(read "previous", 64));
    let cleared: usize = try(text.clear(edit value));
    return text.replace(edit value, read "ready");
}
```

`equal_text(read left, read right)` and `compare_text(read left, read right)`
compare owned values directly. Replacement and clear retain storage for reuse;
failed replacement preserves the previous content. The suite includes growth
failure injection, shrinking, output sentinels, capacity reuse without another
allocation, and 1,000 seeded comparisons against C byte ordering.
