# Verifikasi pilot — 23 Oktober 2026

Lingkungan: AArch64 Android/Termux, Clang 21.1.8,
`aarch64-unknown-linux-android24`, C11, Make. Hasil lokal ini bukan bukti
kesiapan produksi, audit independen, atau klaim performa universal.

## Pemeriksaan yang dijalankan

| Pemeriksaan | Hasil |
| --- | --- |
| `make test` | Lulus: lexer, tipe/flow/view, budget, SHA-256, CLI, host native |
| Integrasi `-O0` dan `-O2` | Lulus: telemetri, filter, aggregate, Result, checked faults |
| Exhaustive decoding | Semua 65.536 pola bit i16 sesuai referensi C |
| Differential packets | 4.000 paket per konfigurasi, termasuk input rusak dan kapasitas kurang |
| `make sanitize` | ASan/UBSan pada compiler dan generated C, termasuk kasus di atas |
| `make fuzz` | 10.000 input frontend/checker/analyzer/lowering, tanpa temuan sanitizer |
| `make fuzz-telemetry` | 10.000 input packet, tanpa fault internal atau pelanggaran output atomicity |
| `make check-format` | Lulus konfigurasi clang-format repository |
| Manifest | JSON terbaca; SHA-256 object/source cocok dengan implementasi independen |
| Dokumentasi | Contoh README dan filter lolos compiler; tautan lokal diperiksa |

Regresi semantik mencakup penggunaan setelah move, initialization pada cabang,
Result yang hanya ditangani sebagian jalur, pemindahan owner dalam kondisi loop,
overlap direct/nested slice, bound outer variable yang berubah dalam loop,
transitive nested-loop budget, rekursi, dan rantai 128 helper. Batas native stack
program pengguna tetap tidak diklaim.

Clang Static Analyzer juga dijalankan pada source compiler. Temuan pembacaan
stream setelah partial error/EOF pada hashing diperbaiki dengan pemeriksaan state
sebelum iterasi berikutnya. Static analysis dan fuzzing terbatas tidak menggantikan
pembuktian menyeluruh atau audit pengguna nyata.

## Biaya model

| Fungsi | Bound `fermi-work-1` | Budget |
| --- | ---: | ---: |
| `decode_sample` | 32 | tidak dianotasi |
| `parse_samples` | 249.977 | 500.000 |
| `filter_above` | 114.733 | 150.000 |

Angka adalah upper bound operasi abstrak, bukan nanodetik. Contoh filter awal
meminta 100.000 dan memang ditolak oleh analyzer; budget contoh diubah menjadi
150.000 secara terbuka, bukan mengubah model agar permintaan lama terlihat lolos.

## Benchmark awal

Jalankan ulang dengan `make benchmark`. Kedua implementasi dikompilasi Clang `-O2`
tanpa LTO. Pemanggilan berada pada translation unit terpisah; hasil digunakan oleh
checksum volatile. Ada warmup dan 101 batch × 128 call untuk setiap ukuran.
Urutan pengukuran C/Fermi bergantian. Median/p95/p99 berikut merupakan kuantil
**rata-rata biaya call per batch**, bukan distribusi latency individual packet.

| Sampel/paket | Fermi median ns | C median ns | Rasio Fermi/C | Fermi p95/p99 ns | C p95/p99 ns |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 64 | 91,9 | 21,0 | 4,372 | 93,1 / 93,2 | 21,6 / 21,6 |
| 1024 | 1311,9 | 206,7 | 6,346 | 1326,3 / 3851,6 | 208,5 / 406,9 |
| 4096 | 5183,3 | 807,7 | 6,417 | 5252,4 / 5346,8 | 810,1 / 860,6 |

Referensi C memeriksa format dan kapasitas, tetapi mengasumsikan argumen host valid;
wrapper Fermi juga memeriksa null, alignment, overflow ukuran, dan overlap. Input
benchmark semuanya valid. CPU tidak dipin, frekuensi tidak dikunci, dan hasil hanya
satu lingkungan lokal. Tidak ada kesimpulan lintas arsitektur atau pembandingan
Rust/Wuffs/EverParse dari pengukuran ini.

Pilot belum mencapai target eksploratif overhead dekat C. Langkah berikutnya adalah
profiling generated C dan optimasi yang mempertahankan checks atau menghapusnya
hanya dengan bukti yang diuji. Belum ada wawancara calon pengguna maupun audit
keamanan eksternal.

## Ekstensi aplikasi native dan modul

Verifikasi lokal berikut dilakukan setelah penambahan diagnostik, i32, executable,
dan modul. Target tetap Termux AArch64; hasil Linux belum tersedia.

- `make test`: lulus, termasuk seluruh suite pilot, i32 O0/O2, executable, dan modul.
- `make sanitize`: lulus pada compiler dan generated C library. Pengujian executable
  serta modul juga dijalankan dengan backend ASan/UBSan melalui
  `tests/sanitized-cc.sh`; wrapper ini sekarang dipakai target sanitizer.
- `make fuzz`: 10.000 input tambahan tanpa temuan. Target ini menguji frontend
  satu unit dan emisi C, belum filesystem loader atau graph import acak.
- `make check-format`: lulus.
- Clang Static Analyzer pada driver modul/executable/main, renderer diagnostik,
  resolusi tipe, dan checker: tidak menghasilkan diagnostic.
- Pemeriksaan independen JSON memastikan diagnostik dapat diparse serta hash
  snapshot setiap modul dan object sesuai dengan SHA-256 file terkait.

Kasus modul meliputi diamond dependency dengan satu identitas nominal, private
function, benturan alias, error di file impor, cycle dengan rantai dependensi,
file hilang, namespace std yang belum tersedia, symlink keluar root, budget
antarmodul, dan duplicate C export. Kegagalan backend mempertahankan executable lama.

### Baseline C dengan validasi ABI setara

Benchmark kini mempertahankan baseline C lama dan menambahkan C dengan pemeriksaan
null, alignment, ukuran/address overflow, dan overlap sebelum decoder independen.
Urutan ketiga implementasi dirotasi antarbatch. Keduanya menggunakan compiler dan
flags yang sama dengan Fermi; kontrak alokasi hidup tetap kewajiban caller.

Satu pengukuran lokal menghasilkan:

| Sampel | Fermi median ns | C biasa median ns | C checked median ns | Fermi/C checked |
| ---: | ---: | ---: | ---: | ---: |
| 64 | 119,6 | 19,8 | 24,0 | 4,975 |
| 1024 | 1726,0 | 212,1 | 226,0 | 7,638 |
| 4096 | 6851,6 | 807,7 | 866,0 | 7,912 |

Angka tersebut belum memenuhi target 1,5×. CPU/frekuensi tetap tidak dikunci;
perubahan metode dan variasi kondisi tidak memungkinkan kesimpulan regresi dari
perbandingan langsung dengan tabel historis. Tidak ada optimasi yang diklaim dari
perubahan ini. Benchmark filtering/statistik dan pengukuran compile time/object
size untuk gerbang rilis masih harus dilengkapi.

## 2026-10-27 — signed decimal input/output

On Termux AArch64, the complete `make test` suite passed after adding ordinary
Fermi i32/i64 parsing and formatting. This includes the capability lab and
1,369-line library-system workload. `make check-format` and affected local
Markdown links passed. Focused O0/O2 checks also passed using the existing
ASan/UBSan compiler, generated native executables, and instrumented C integration
harness. The compiler and runtime implementation were unchanged; a complete
`make sanitize` run was not performed for this library-only change.

The independent harness checks 65,536 small signed values, 10,000 seeded values,
integer extrema, decimal-power neighbors, invalid ASCII/non-ASCII bytes, error
precedence, and guarded output capacities. Parsing/formatting do not allocate;
all four exported wrapper contracts report static bounds, zero heap, and no
external effects. Two native example tests and expected output `-1250` passed.

Evidence: `build/signed-decimal-regression.log`,
`build/signed-decimal-sanitize.log`, `build/signed-decimal-format.log`.

## 2026-10-27 — repeated fixed-array seeds

The complete `make test` suite passed on Termux AArch64. This includes twelve
new native array tests at O0/O2, two example tests, tracked owner cleanup and
allocation-failure cases, effect and diagnostic checks, existing library and
compiler regressions, and the capability lab. The migrated library system still
matches four independent C catalog journals (16384 commands) at both optimization
levels, with input-limit and failure checks unchanged.

A newly built ASan/UBSan compiler and semantic executable passed the focused
suite; generated native programs and the ownership C harness were instrumented
at O0/O2. Frontend fuzzing completed 10000 runs with the new seed and no detected
crash. `make check-format` and affected local documentation links passed. This
was a full normal regression and focused sanitizer run, not a full make sanitize.

The current library application is 1300 Fermi lines across 13 files (1227
nonblank/noncomment lines), excluding tests/documentation. Repeated initializers
removed 69 lines without replacing domain logic. Native stack remains uncertified;
fixed-array initialization adds neither dynamic collections nor dynamic strings.

Evidence: `build/array-repeat-regression.log`,
`build/array-repeat-sanitize-focused.log`, `build/array-repeat-fuzz.log`,
`build/array-repeat-format.log`. See `tests/array-repeat.sh` to reproduce the
feature checks; the suite is registered in both test and sanitizer targets.

## 2026-10-27 — owned runtime text

Full `make test` passed on Termux AArch64 after private-field checks and the
ordinary Fermi owned-text library were added. Existing strings, resource owners,
module visibility, aliases, arrays, effect contracts, CLI, capability applications,
and library-system differential checks remain accepted.

`tests/owned-text.sh` passed at O0/O2 with the normal compiler and with a newly
built ASan/UBSan compiler and instrumented generated programs/C harness. It covers
five native text tests, one module-privacy test, ten invalid-source cases,
valid/invalid UTF-8 CLI arguments, exhaustive one/two-byte input, every scalar
encoding and surrogate encoding through U+10FFFF, truncated encodings, 10000
seeded byte sequences, allocation failure, growth preservation, and output bounds.
Tracked allocations are released on success and domain-error paths.
The existing semantic suite also passed with sanitizers.

Frontend fuzzing completed 10000 runs without a detected crash. C formatting and
affected local documentation links passed. This is a full normal regression and
focused sanitizer run, not a full make sanitize. Read-only Text field projections
still conservatively carry memory effects; escaping text views, generic record
collections, Unicode normalization, and a public Text C ABI are not introduced.

Evidence: build/owned-text-regression.log, build/owned-text-sanitize.log,
build/owned-text-fuzz.log, build/owned-text-format.log.

## 2026-10-27 — text reuse and dynamic integer buffers

Owned-text replacement, clear and direct comparison passed six native tests and
an expanded C harness at O0/O2, normally and under ASan/UBSan. Tests include 1000
seeded comparison pairs, Unicode ordering, retained capacity, unchanged old bytes
after invalid input or failed growth, output sentinels, and allocation cleanup.
The existing UTF-8 decoder checks remain in the suite. The full normal regression
passed before integer storage work. Logs: build/text-editing-{regression,sanitize,format}.log.

The final `make test` suite passed after integer Buffer support, strict native
signatures, and ordinary push/pop were finalized. It includes the capability lab
and the 1300-line library application's independent C reference reports at O0/O2.
The newly built semantic executable also exercises native signature mismatch and
legacy byte-only declarations. No runtime implementation change was required.

Focused typed-buffer tests passed at O0/O2 using the normal compiler and a newly
built ASan/UBSan compiler, instrumented generated C, and tracked allocation/release.
They check nine integer widths/signed extrema, exact byte limits, reserve/resize,
zeroing after regrowth, overflow before allocation, allocator failures, retained
contents after failed growth, bounds-fault cleanup, resource structs, geometric
push/fallback, reverse-order pop for every width, empty pop, retained capacity,
readonly/type/effect rejection, and standard-module shadowing. The executable
series example is checked against an independent sum formula for four runtime
counts, plus rejected input. Native and generic declarations remain restricted
as documented; record/resource elements and general generics remain unavailable.

Frontend fuzzing completed 10000 runs with the typed buffer seed and no detected
crash. C formatting and affected local documentation links passed. The sanitizer
checks were focused rather than a full make sanitize. Memory effects and abstract
work/heap bounds remain unknown for dynamic storage; byte payload limits do not
prove process RSS, stack, or wall-clock bounds.

Evidence: build/typed-buffer-final-regression.log,
build/typed-buffer-stack-tests.log, build/typed-buffer-stack-sanitize.log,
build/typed-buffer-sanitize.log, build/typed-buffer-semantics.log,
build/typed-buffer-fuzz.log, build/typed-buffer-format.log.

## 2026-10-27 — UTF-8 text ranges

- `sh tests/owned-text.sh`: passed at O0/O2, including nine native tests,
  independent C UTF-8 checks, allocation failure, privacy and ownership cases.
- The same suite passed with the current dedicated sanitized compiler and
  ASan/UBSan-instrumented generated programs (`FERMI_TEST_SANITIZE=1`).
- The text_ranges example ran normally and under ASan/UBSan; its two output
  lines matched `café` and `worker: café` exactly.
- `make check-format` and affected local Markdown links passed.
- Evidence: `build/text-ranges-tests.log`, `build/text-ranges-sanitize.log`,
  `build/text-ranges-format.log`, `build/text-ranges-example.out`.

This is focused sanitizer coverage, not a new complete `make sanitize` run.

The complete `make test` regression also passed, including the new text range
example at O0/O2, capability-lab differentials, and the large library-system
reference tests. Log: `build/text-ranges-regression.log`.

## 2026-10-27 — Integer buffer batches

- Focused typed-buffer tests passed at O0/O2 with the current dedicated sanitized
  compiler and ASan/UBSan-instrumented generated programs. The suite includes
  two native tests, all-width C comparisons, allocation tracking/failure injection,
  byte limits, empty input, and compile-time alias/type rejection.
- `make check-format` passed; affected local Markdown links were checked.
- Evidence: `build/buffer-batches-sanitize.log`,
  `build/buffer-batches-format.log`.

This is focused sanitizer coverage, not a complete new `make sanitize` run.

The full `make test` regression passed, including the batch example at O0/O2,
typed-buffer checks, capability-lab differentials, and library-system reference
commands. Log: `build/buffer-batches-regression.log`. The batch example also ran
under ASan/UBSan and produced exactly `25\n`; output is recorded in
`build/buffer-batches-example.out`.

## 2026-10-27 — Static-string byte indexing

- `sh tests/strings.sh` passed at O0/O2: twelve native string tests, existing
  executable examples, and the new scalar-ABI byte/bounds C harness.
- Fresh ASan/UBSan builds of the compiler and unit semantics passed; the string
  suite also passed with instrumented generated C and its C harness at O0/O2.
- `make fuzz` completed 10,000 frontend runs with the new import-free seed.
- `make check-format` passed.
- Evidence: `build/string-index-tests.log`, `build/string-index-sanitize.log`,
  `build/string-index-fuzz.log`, `build/string-index-format.log`.

This records focused sanitizer coverage rather than a full `make sanitize` run.

The complete `make test` regression passed, including ownership, text, alias,
borrow, capability-lab, and large library-system reference tests at O0/O2.
Log: `build/string-index-regression.log`. Affected local Markdown links passed.

## 2026-10-27 — Requested ordinary integer generic instances

- Full `make test` passed, including the updated integer-domain/generic suites,
  the new demand-generics suite, typed buffers, ownership and effect checks,
  capability-lab differentials and the large library-system reference tests.
- Fresh ASan/UBSan compiler and semantics binaries passed. Instrumented O0/O2
  suites passed for integer generics, demand instantiation, signed/unsigned
  domains, and typed-buffer allocation/cleanup/failure paths.
- `make fuzz` completed 10,000 frontend inputs with the new demand/recursive seed.
- `make check-format` and affected local Markdown links passed.
- The same example compiled to 18 generic instances with the prior compiler and
  three with this compiler, measured using explain output. This is a function
  count comparison, not a timing or binary-size benchmark.
- Evidence: `build/lazy-generics-regression.log`,
  `build/lazy-generics-sanitize.log`, `build/lazy-generics-fuzz.log`,
  `build/lazy-generics-format.log`, and
  `build/lazy-generics-{before,after}.explain`.

This is focused sanitizer coverage, not a complete new `make sanitize` run.
Unrequested ordinary templates are not semantically or resource certified;
trusted native buffer templates retain eager validation.

## 2026-10-27 — Inventory and shared-struct array slices

- Minimal shared-struct array-slice native reproduction failed before the fix with
  a C const-discard error; removing the slice or changing the borrow to edit
  passed. The same read reproduction passed after preserving shared view types.
- `sh tests/inventory.sh` passed at O0/O2: four native tests, 6,144 independently
  modeled commands per mode, canonical snapshot replay, all 64 read-boundary
  offsets, exact command/name limits, and malformed/domain/I/O failure cases.
- Fresh ASan/UBSan compiler and semantics passed. The struct-borrow suite (nine
  native tests) and complete inventory suite passed with instrumented generated
  programs at O0/O2. Read-to-edit rejection and valid edit behavior remain tested.
- `make fuzz` completed 10,000 frontend inputs including nested read-slice seed.
- `make check-format` and affected local Markdown links passed.
- Evidence: `build/read-array-slice-before.log`, `build/inventory-tests.log`,
  `build/inventory-sanitize.log`, `build/inventory-fuzz.log`,
  `build/inventory-format.log`.

Sanitizer coverage is focused; this is not a complete new `make sanitize` run.
The syntax candidates are design-only and have no executable validation claim.

The full `make test` regression also passed, including the new inventory suite,
shared/edit slice regressions, capability-lab and library-system differential
applications. Log: `build/inventory-regression.log`.

## 2026-10-27 — Data generics and initialized record buffers

- Full `make test` passed, including data generics, record buffers, integer-buffer
  compatibility, capability lab, library system and inventory regressions.
- Fresh ASan/UBSan compiler and semantics binaries passed. Focused O0/O2 suites
  passed for data generics, record buffers and existing integer buffers, with
  instrumented generated programs and allocation-failure/cleanup checks.
- `make sanitize-runtime` passed, including positive nonstandard element widths.
- `make fuzz` completed 10,000 frontend inputs with both new seeds.
- `make check-format` passed. Native `element_size` budget analysis now has a
  regression test for the previous null-body crash.
- Evidence: `build/record-buffers-regression.log`,
  `build/record-buffers-sanitize.log`, `build/record-runtime-sanitize.log`,
  `build/record-buffers-fuzz.log`, and `build/record-buffers-format.log`.

Sanitizer coverage is focused; this is not a complete new `make sanitize` run.
Resource-containing elements, general generic types, callbacks, payload errors,
floats and the remaining foundation stages are not implemented by this change.

## 2026-10-27 — Fermi-written bootstrap lexer

- Full `make test` passed, including the new bootstrap suite, capability lab,
  library system and inventory regressions.
- `sh tests/bootstrap.sh` passed at O0/O2: five native tests, 70,754 differential
  inputs per mode, every two-byte input, all token fields, diagnostic categories
  and byte locations, source/token/identifier limits and repository sources.
- Token collection passed injected failure at every allocation, with exact
  release accounting for successful, lexical-error and allocation-error paths.
- CLI checks passed for empty/regular/missing/nonregular files, exact/over limits,
  32 chunk-boundary placements, malformed input, argument errors and own-source
  scanning. The standalone O2 build reports 564 tokens for bootstrap/main.fe.
- The same suite passed with the existing source-matching ASan/UBSan compiler and
  instrumented generated programs/host lexer at O0/O2. Compiler C sources did not
  change in this milestone. This is focused coverage, not a new full make sanitize.
- `make check-format`, shell/fixture syntax checks and affected local Markdown
  links passed.
- Evidence: `build/bootstrap-tests.log`, `build/bootstrap-regression.log`,
  `build/bootstrap-sanitize.log`, `build/bootstrap-format.log`, and
  `build/bootstrap-build.log`; standalone executable: `build/fermi-bootstrap`.

Tokenizing compiler sources does not establish parsing or self-hosting. The
production compiler remains C; the Fermi frontend has no AST, semantic analyzer
or code generator yet. Bootstrap contracts are in bootstrap/README.md.

## 2026-10-27 — Bootstrap parser, unary binding and camelCase tests

- Full `make test` passed, including bootstrap lexer/parser, test discovery,
  capability lab, library system and inventory regressions.
- A minimal native unary test failed before correction; six precedence tests now
  pass at O0/O2. Prefix operators bind above all binary operators.
- `sh tests/bootstrapParser.sh` passed at O0/O2: four native AST/state tests,
  1,446 normalized host-expression-tree comparisons per mode, malformed syntax
  locations, all operator pairs/prefixes, calls, associativity and depth/chain
  boundaries. Node/parameter/function limits and unsupported syntax were rejected.
- Every token/node allocation in a representative parser workload was failed in
  turn; allocation, lexical and syntax errors released all tracked owners.
- A fresh ASan/UBSan compiler and semantics binary passed. Instrumented O0/O2
  parser, lexer and test-command suites passed, including 70,760 lexer differential
  inputs per mode. This is focused coverage, not a new complete make sanitize.
- Mixed testName/test_* discovery passed; bare test/testhelper and imported tests
  stayed excluded, invalid signatures/generic tests were rejected, and camelCase
  tests continued after a checked fault.
- `make fuzz` completed 10,000 frontend inputs. `make check-format`, script syntax
  and affected local Markdown links passed.
- Standalone O2 `build/fermi-parse bootstrap/examples/arithmetic.fe` reports
  `AST nodes: 35`; its production-compiled example returns status 39. The updated
  lexer executable reports 564 tokens for bootstrap/main.fe.
- Evidence: `build/bootstrap-unary-before.log`, `build/bootstrap-unary-after.log`,
  `build/bootstrap-parser-tests.log`, `build/bootstrap-parser-test-names.log`,
  `build/bootstrap-parser-regression.log`, `build/bootstrap-parser-sanitize.log`,
  `build/bootstrap-parser-fuzz.log`, and `build/bootstrap-parser-format.log`.

The bootstrap parser is syntactic and accepts only its documented subset. Name,
type and ownership analysis, remaining grammar and code emission are not ported.
It cannot parse or compile its own implementation yet. Existing code that relied
on the former unary grouping must add parentheses, as documented in decision D29.

## 2026-10-27 — Bootstrap control-flow parser

- Full `make test` passed, including bootstrap parser/lexer, capability lab,
  library system and inventory regressions.
- O0/O2 parser tests passed: five native AST tests and 1,530 normalized host-tree
  comparisons per mode. Added statement/branch/header links, malformed control
  syntax, else-if chains and 64 nesting configurations to the existing expression
  reference tests. For-step declarations were rejected at the host error location.
- Injected failure at every allocation in a workload containing a for/if header
  and a large declaration body; all successful and partial owners were released.
- The complete parser suite passed with the source-matching ASan/UBSan compiler,
  instrumented generated programs and instrumented host reference parser at O0/O2.
  This is focused sanitizer coverage, not a new complete make sanitize run.
- The controlFlow.fe example produced 49 AST nodes and native exit status 19 at
  O0/O2. The earlier arithmetic example still produced 35 nodes and status 39.
- `make check-format`, fixture/shell syntax and affected local Markdown links
  passed. Standalone build/fermi-parse was rebuilt and reports 49 nodes.
- Evidence: build/bootstrap-control-tests.log,
  build/bootstrap-control-regression.log, build/bootstrap-control-sanitize.log,
  and build/bootstrap-control-format.log.

This extends the Fermi-written syntactic parser only. Type checking, enclosing-loop
checks and ownership analysis remain in the production C compiler. Match, module
and nominal-declaration grammar still block parsing the bootstrap's own sources.

## 2026-10-27 — Dispatch executable project

- sh dispatch/tests/run.sh passed at O0/O2: 12 native tests, three independently
  modeled workloads of 1,024 retained jobs and 4,612 commands each, full state/
  metrics/audit/queue reports, and byte-identical checkpoint replay.
- Tested 64 positions around a 512-byte input boundary, CRLF, empty/exact 4 MiB
  sources, malformed/overflow input, stale/replaced leases, key/ID conflicts,
  dependency/TTL policy, backlog and tenant/worker/job/command limits. Rejected
  inputs created no output artifacts; existing output sentinels were preserved.
- The same complete project suite passed at O0/O2 using the source-matching
  ASan/UBSan compiler and instrumented generated executables. This is project
  sanitizer coverage, not a new full repository make sanitize run.
- make -C dispatch and make -C dispatch demo passed. Demo final state has five
  jobs: three succeeded, one cancelled, one expired; audit sequence 36 and global
  fencing sequence 7. Shell/JS syntax and local Markdown links passed.
- Evidence: build/dispatch-tests.log, build/dispatch-sanitize.log,
  build/dispatch-build.log and build/dispatch-demo.log.

The application uses current Fermi syntax and existing libraries. No compiler
implementation changed in this milestone, so the repository-wide regression was
not rerun. Persistence is replay-based, sequential and not atomic across both
output files; all scope and capacity contracts are in dispatch/README.md.

### Portable build and installation

Locally verified on Android/Termux AArch64 after deleting generated output:

- `make -j4 all check`: compiler, five C unit suites, four runtime suites passed.
- `make all` repeated: no compiler or linker commands executed.
- `make test-install`: installed Hello World launched outside the source directory;
  `/usr` layout staged under DESTDIR without writing to system directories.
- `tests/driver.sh`, `tests/executable.sh`, `tests/run-command.sh`, and
  `tests/run-cache.sh`: passed.
- `make check-format`: passed.

Linux Clang/GCC and experimental macOS jobs are configured, not locally executed.
The local `gcc` executable is a Clang symlink; this is not independent GCC coverage.
The full language regression and sanitizer suites were not repeated for this build
layout change. Historical generated logs were removed with the old build tree;
new focused logs are under `build/platform-*.log`.

### String allocation guards and reusable buffers

- `make test`: full repository regression suite passed, including bootstrap lexer,
  1,530 host/parser AST comparisons per optimization mode, ownership/fault cleanup,
  record buffers, capability applications, library system and inventory workloads.
- `make -j4 all check`: compiler and C unit/runtime suites passed.
- `make sanitize-support sanitize-runtime`: focused ASan/UBSan suites passed.
- `tests/bufferReuse.sh`: three native Fermi tests at O0/O2, tracked-allocation C
  integration, allocation failure, and the executable batch example passed.
- The same buffer tests and example passed with `FERMI_TEST_SANITIZE=1` and the
  sanitized backend wrapper. Successful record/scalar reuse required exactly one
  initial allocation each; empty reuse required none, and failed acquisitions
  left no live allocations. This is allocation-count evidence, not a speed benchmark.
- `make check-format` and shell syntax validation passed.

Evidence: `build/memory-regression.log`, `build/memory-check.log`,
`build/memory-sanitize.log`, `build/memory-reuse.log`,
`build/memory-reuse-sanitize.log`, and `build/memory-format.log`.
The complete `make sanitize` suite was not run for this change.

### Native truncation and zero-heap reuse contracts

- `make -j4 all check` passed, including growth rejection, retained capacity/bytes,
  zero-capacity and vacant descriptors, reinitialization, and invalid native
  signatures. The read-view negative case uses valid slice syntax to reach the
  native signature validator.
- `make test` passed in full after the runtime, native checker, emitter and work
  analysis changes; this includes bootstrap and application workload regressions.
- Runtime/support sanitizer targets and a focused ASan/UBSan semantics binary
  passed. Generated reuse tests and the budgeted batch example passed with
  ASan/UBSan at O0/O2. The complete sanitizer suite was not rerun.
- `fermi explain tests/cases/bufferReuse.fe` reports heap zero and empty effects
  for resetEntries (work bound 10), takeLast (48), and truncate (native work 1).
  Counts use fermi-work-1 and do not measure CPU time or native stack usage.
- `make check-format` passed.

Evidence: `build/truncate-check.log`, `build/truncate-regression.log`,
`build/truncate-sanitize.log`, `build/truncate-semantics-sanitize.log`,
`build/truncate-reuse-sanitize.log`, `build/truncate-contracts.log`, and
`build/truncate-format.log`.
