# Reusable record batches

```
./build/fermi run examples/features/bufferReuse/main.fe --no-cache
```

Processes 128 records in sixteen batches using capacity reserved for eight
records. `buffer.clear` resets the logical length between batches; the owner and
capacity remain live. Subsequent pushes initialize each new record with a typed
value. Shrinking retains old bytes and provides no confidential-data erasure.

`buffer.truncate(edit value, length)` also preserves capacity but rejects lengths
larger than the current length. Both helpers return `Result<usize, buffer.Error>`
and support resource-free data elements, including records and strings.

`resetBatch` carries `@effects()` and `@budget(work = 32, heap = 0)`.
The compiler verifies this borrowed operation without including the caller's
allocation and final owner cleanup in that helper's budget. Work units are
abstract language operations, not CPU cycles. Explicit `edit` access still
permits mutation even with an empty effect set.
