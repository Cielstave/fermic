# Inventory journal processor

A native batch CLI written entirely in Fermi. It maintains up to 64 named items,
applies stock and price changes, and saves a deterministic, reloadable snapshot.
Prices are integers in caller-chosen minor units; no floating-point calculation is
required. Commands and UTF-8 names can cross file-read boundaries.

## Run

From the repository root, choose an output path that does not already exist:

```sh
make
./build/fermi run examples/inventory/main.fe -- examples/inventory/sample.journal build/inventory.snapshot
cat build/inventory.snapshot
./build/fermi run examples/inventory/main.fe -- build/inventory.snapshot build/inventory.reloaded.snapshot
cmp build/inventory.snapshot build/inventory.reloaded.snapshot
```

The sample produces:

```text
ITEM 10 6 250 Steel bolts
ITEM 20 7 1500 Buku café
```

A snapshot is itself a valid journal. To continue, concatenate the last snapshot
and subsequent commands into a new journal, then write a new snapshot. Items are
serialized by increasing ID, independent of internal slot reuse. Successful runs
write no stdout/stderr output.

## Commands and limits

| Command | Meaning |
| --- | --- |
| `ITEM ID QUANTITY PRICE NAME` | Create a new item; duplicate IDs are rejected. |
| `IN ID QUANTITY` | Receive stock without exceeding the quantity bound. |
| `OUT ID QUANTITY` | Issue available stock; underflow is rejected. |
| `PRICE ID PRICE` | Replace the integer unit price. |
| `DROP ID` | Remove an item only when its quantity is zero. |

IDs are 1–1,000,000; quantities/prices are 0–1,000,000. Names contain 1–48 UTF-8
bytes, with no C0 controls, DEL, or leading/trailing ASCII spaces. Internal spaces
are retained. Numbers use unsigned ASCII decimal, at most seven digits; leading
zeroes are accepted. Fields before the name use exactly one ASCII space.
Commands use uppercase words and a final LF. Blank lines are accepted; CRLF is
rejected. Limits: 128 bytes per physical line excluding LF, 4,096 nonblank commands,
and 1,048,576 total input bytes. Empty input produces an empty snapshot.

Domain and syntax errors abort the batch before creating output. They do not
change an existing snapshot or the input journal. Exit codes are 64 for usage,
65 for input/domain rejection, 70 for internal invariant failure, 71 for startup
allocation/argument failures, and 74 for I/O failures. Processing diagnostics include
the physical line number and never echo untrusted item names.

Output is exclusively created, written, synchronized and explicitly closed.
An output failure may leave a partial newly created file. Atomic replacement,
locking, transaction recovery and concurrent writers are not implemented. This
application does not provide database durability guarantees.

## Organization and validation

- `src/model.fe`: records, name validation, stock transitions and invariants.
- `src/parser.fe`: bounded line decoding into typed commands.
- `src/runner.fe`: streaming admission, error boundaries and lifecycle.
- `src/snapshot.fe`: canonical serialization and exclusive file publication.
- `src/errors.fe`, `src/diagnostics.fe`: typed application errors and stderr output.

```sh
./build/fermi test examples/inventory/tests.fe
sh tests/inventory.sh
```

Four native tests check lifecycle, unchanged state after rejection, parser edges
and full-capacity slot reuse. The shell suite tests O0/O2 against a separate
JavaScript Map reference for 6,144 commands per mode, snapshot replay, all 64 read
boundary offsets, exact name/command limits, malformed UTF-8, NUL, overflow,
truncation, input limits and overwrite refusal. Reference code only generates
fixtures; it is not part of the application executable.

The application exposed and now regresses a compiler bug in read-only struct
array slices. This application retains fixed capacity for its bounded batch contract.
Nonresource dynamic records are now supported through std.buffer; resource-owning
elements still require further lifecycle work. Runtime names here use inline byte
arrays, independently of the static-str record-buffer example.
