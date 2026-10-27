# Capability lab

Four native Fermi programs exercise different kinds of logic and increasing application structure. All application code is Fermi. The C helper under tests generates an independent reference for verification; applications do not link to it. Sources under limitations/ are intentionally rejected programs, not runnable examples.

## Programs

| Program | Fermi source lines, excluding shared output helper | What it exercises |
| --- | ---: | --- |
| [Numbers](numbers.fe) | 60 | Euclidean GCD, primality, integer division, loops and numeric output. |
| [Grid search](grid.fe) | 98 | Breadth-first search, bounded queue, mutable views, payload enum results and shortest paths. |
| [Stack machine](machine.fe) | 106 across two files | Tagged instructions, a checked arithmetic stack, explicit domain errors and shared struct access. |
| [Stock ledger](ledger.fe) | 262 across five files | Runtime file input, streaming state machine, mutable records, domain rejection, owned files, error mapping and report output. |

The shared decimal output helper is 13 lines. The largest application is a bounded CLI, not a large production system. These line counts exclude standard-library modules.

```sh
./build/fermi run examples/capability_lab/numbers.fe
./build/fermi run examples/capability_lab/grid.fe
./build/fermi run examples/capability_lab/machine.fe
```

Expected output:

```text
primes-below-100=25 gcd-84-30=6
grid-distance=8
stack-result=32
```

Entrypoints are in this directory so their relative imports stay within the current entrypoint-root boundary. Run any of their local test functions with `fermi test FILE.fe`.

## Stock ledger

```sh
mkdir -p build/capability-lab
./build/fermi run examples/capability_lab/ledger.fe -- \
  examples/capability_lab/stock_ledger/sample.commands \
  build/capability-lab/stock.report
cat build/capability-lab/stock.report
```

Choose a new output path on each run: existing destinations are refused. The sample retains item quantities 7 and 8, accepts three commands and rejects three. See [sample.expected](stock_ledger/sample.expected).

The input is ASCII, one command per LF-terminated line:

```text
+ ID AMOUNT
- ID AMOUNT
```

IDs 0–7 select fixed inventory slots. Amounts are decimal 0–1,000,000; item quantities cannot exceed 1,000,000. Unknown IDs within the parser's 0–65,535 numeric range, insufficient stock and capacity excess are domain rejections; they increment the rejected counter and processing continues. Leading zeroes and empty lines are accepted. Malformed syntax, invalid bytes, larger numbers and incomplete final commands fail the whole input.

The reader uses 32-byte chunks and admits at most 65,536 input bytes. Parser state survives chunk boundaries. No arbitrary-length text or record collection is retained. The report is created only after all input has been parsed successfully. It is written, synchronized and explicitly closed. A failure during output may leave an incomplete newly created report; there is no transactional rename or removal.

Exit status is 0 on success, 64 for invalid arguments, 65 for invalid/oversized input, 71 for unavailable startup resources, and 74 for file/output failure. Runtime arithmetic and bounds faults retain normal Fermi behavior. These examples add no networking, authentication, database, or external dependency.

## Evidence

Run the focused suite:

```sh
sh tests/capability-lab.sh
```

It builds and runs all four programs at O0/O2 and checks twelve native test functions. The tests include 1,024 GCD pairs, 576 open-grid source/target pairs, and 400 instruction expressions. A separate C reference generates six repeatable pseudo-random command sequences, totaling 6,144 ledger commands; every report is compared byte-for-byte.

Additional checks cover empty input, the exact 65,536-byte limit, a 65,537-byte refusal, truncated commands, numeric overflow, CRLF and invalid-byte refusal, a missing input file, wrong argument count, and preservation of an existing output. The same suite is included in make test and make sanitize. Normal and ASan/UBSan results were verified on Termux AArch64 on 27 October 2026. Logs: build/capability-lab-tests.log and build/capability-lab-sanitize.log.

## Concrete limitations found

| Limitation | Evidence and consequence | Workaround used |
| --- | --- | --- |
| Entrypoint directory is the import root | A nested entrypoint could not import ../common/output.fe. A package-root override is unavailable. | Put entrypoints at the shared project root. |
| Constant-true loop return analysis is conservative | [loop_return.fe](limitations/loop_return.fe) rejects a function that returns on its first iteration with E_RETURN. | Break at EOF and return after the loop. |
| Views exclude resource-owning element types | [record_view.fe](limitations/record_view.fe) rejects read Item[] when Item owns a buffer. Nonresource record views are supported. | Borrow individual resource structs; use data views for plain records. |
| Containers exclude resource-owning elements | [typed_buffer.fe](limitations/typed_buffer.fe) rejects Buffer<Item> when Item owns a buffer. Nonresource dynamic records are supported. | Buffer<T> for data; fixed resource-owning structs for resource lifecycles. |
| Runtime bytes cannot be retained as str | [dynamic_text.fe](limitations/dynamic_text.fe) rejects an unsafe conversion. This conversion remains unsafe even with owned text available. | Use std.text.from_utf8 to obtain an independent text.Text owner. |
| Generic parameters require explicit constraints | [general_generic.fe](limitations/general_generic.fe) omits its constraint and is rejected. | Use T: data for nonresource record algorithms, or an integer constraint. |
| Floating-point types are unavailable | [float_math.fe](limitations/float_math.fe) cannot declare f64. | The applications use integer quantities only. |

The original sources exposed repeated zero-filled array literals; `[value; count]` now addresses that syntax. Explicit mapping between unrelated error types remains necessary. The ledger is eight-slot rather than runtime-sized, and the stack program has seven instruction slots. Those bounds make the examples useful for bounded tasks but do not demonstrate general collection flexibility.

The rejection probes are checked against their expected diagnostic codes. They document absent mechanisms and conservative analysis, not a request to permit unsafe references or unchecked conversions. No wrong-answer mismatch or unexpected compiler crash was observed in this suite. The foundation flexibility gate remains incomplete, particularly for generic typed storage and reusable algorithms. Owned runtime text is now available separately.
