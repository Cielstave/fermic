# Execution ledger — docs/superpowers/plans/2026-10-21-native-compiler.md

- Authorization: user explicitly requested design and implementation together; proceed inline without another plan approval.
- Workspace: no Git metadata exists; work in the supplied directory. No branch, worktree, or commit operations are applicable.
- Pre-flight 1→2: token spans and arena-owned token strings feed parser; consistent.
- Pre-flight 2→3: typed AST with resolved binding IDs feeds ordered C lowering; consistent.
- Pre-flight 3→4: one Make test entry point feeds local validation and CI; consistent.
- Scope: implement M0–M3 of the strategic plan as an experimental release; future records/ownership remain explicit roadmap work.

## 2026-10-23 — research and redesign

- User requested a consistent syntax (including parenthesized for/if/while), a defensible purpose, and deep research before redesign.
- Reviewed primary surveys, adoption reports, language/compiler documentation, and resource-analysis research. Published source-linked findings with explicit evidence limits.
- Replaced the v0.1 direction with the v0.2 bounded native component proposal. The old implementation plan is superseded; its lexer test is retained as early test material.
- Syntax now uses C-style parenthesized for loops, parenthesized conditions, suffix array types, explicit types and let/var, plus call-scoped read/edit views.
- No compiler implementation or successful compiler test is claimed. The last observed lexer test was RED because the frontend is absent.
- Native resource contracts are a product hypothesis, not an assertion of global novelty. Metering and static proof are explicitly separate; stack remains unknown unless backend evidence exists.
- Added research comparison, future scenarios, acceptance experiments, and stop/pivot criteria. No interviews or benchmarks have been performed.

## 2026-10-23 — compiler pilot

- Implemented the approved binary-parser pilot in C11 with native C lowering,
  explicit buffer access, checked faults, static work contracts, and C exports.
- Added grammar, work-model, ABI, and architecture documents. The typed structured
  tree is the pilot IR; a separate SSA graph is not claimed.
- Telemetry parser validates the entire frame before output writes. Exhaustive
  i16 decoding and differential malformed-packet tests exercise the generated code.
- Added sanitizer builds, frontend/lowering and packet fuzz targets, CLI tests,
  SHA-256 artifact identity, and a local C baseline benchmark.
- Researched LLVM, CPython C, and Linux style guidance after the user's readability
  request. Reworked lexer helpers, analysis names, binding states, loop recognition,
  and split CLI/build/toolchain responsibilities; formatter enforces braces.
- Review found and covered regressions in loop-bound invalidation, temporary-array
  effects, conditional Result handling, and consuming loop conditions.
- Initial benchmark does not meet a near-C performance target. See validation.md;
  the compiler is an experimental pilot, not a production or speed claim.

## Referensi sensor hub — 23 Oktober 2026

Ditambahkan `examples/sensor_hub/hub.fe`: 235 baris Fermi yang dapat dikompilasi,
meliputi parsing paket, kalibrasi integer, statistik, hysteresis, dan laporan.
`make simulate` menjalankan tiga kanal melalui host C; I/O tetap di host.
Tes integrasi baru masuk suite normal dan sanitizer pada O0/O2, dengan referensi
numerik independen, ekstrem kapasitas/kalibrasi, ambang alarm, dan preservasi output
pada domain error. Tidak ada perubahan sintaks compiler atau `AGENTS.md`.

Catatan riset `docs/research/sensor-hub-reference.md` menghubungkan pemisahan
pengolahan/transport dengan dokumentasi Zephyr dan mencatat hambatan yang muncul:
ABI record, modul, kontrak rentang helper, serta borrow record. Fitur tersebut
belum diimplementasikan. Contoh ini tidak membawa klaim benchmark baru.

## Fondasi aplikasi native dan modul

Ditambahkan diagnostik source/JSON dengan kode dan lokasi terkait, integer i32,
serta executable `main() -> i32`. Build executable dipublikasikan setelah native
backend berhasil dan mempertahankan output lama pada kegagalan build.

Loader proyek kini mendukung import lokal, pub, identitas tipe bersama, pemeriksaan
cycle dan batas sumber/memori bersama. Error tetap menunjuk source unit asal;
manifest library mencatat hash snapshot dependensi. Contoh yang dapat dijalankan:
`examples/cli/modules/main.fe` dengan modul statistik dan kalibrasi.

Suite normal, sanitizer, fuzzing satu-unit, formatting, dan static analysis
terpilih lulus. Benchmark menambahkan referensi C ber-ABI checked dan masih jauh
dari target performa. Status serta pekerjaan yang belum selesai tercatat dalam
`docs/PLAN.md`: resource ownership, library standar, dua aplikasi penerimaan,
optimasi, dan validasi Linux belum selesai. `AGENTS.md` tetap dipertahankan.

## 2026-10-24 — native Hello World

- Added immutable byte-string expressions and the bootstrap `std.io.write` module.
  The `.fe` interface is loaded from the build-configured library directory;
  application modules cannot declare native implementations.
- Added POSIX output lowering with short-write handling, EINTR retry, and explicit
  `Result` errors. I/O effects propagate through calls and reject budget contracts.
- Compiled and executed `examples/cli/hello.fe` on Termux: `Hello, world!` followed
  by a newline, exit status 0. Normal tests and formatting checks passed.
- Added output, access, error, and runtime fault-injection tests. Stdout failure is
  tested with a read-only descriptor; closing descriptor 1 before exec is not a
  reliable test on Android because startup may replace missing standard descriptors.
- This adds stdout only. Files, stdin, dynamic buffers, relocatable installation,
  resource ownership, and Linux validation remain unfinished.
- `make sanitize` also passed, including the instrumented compiler, generated
  executables, and I/O runtime tests under AddressSanitizer/UndefinedBehaviorSanitizer.

## 2026-10-24 — standard input and stream copying

- Added `std.io.read_into(edit bytes)` with a fallible byte count, EINTR retry,
  short-read support, and a bounded syscall request. Empty views return zero
  without accessing the descriptor; nonblocking errors remain domain errors.
- Replaced the single I/O flag with read/write effect bits. Fixed-point propagation
  retains both effects through recursive helpers, and budget rejection remains
  conservative. Manifest and explain output share the same effect renderer.
- Added `examples/cli/copy_stdin.fe`, with explicit input/output error statuses
  and no application-specific C host. The API name avoids a grammar exception
  for the existing `read` access keyword.
- Normal tests and formatting checks passed on Termux, including binary streams,
  pipes, EOF, unreadable stdin, immutable/uninitialized buffers, discarded
  Results, native read fault injection, and recursive effect propagation.
- File handles, arguments, dynamic allocation, and resource cleanup remain future
  work. Read/write error variants preserve the existing write-failure ordinal.
- `make sanitize` passed with ASan/UBSan on the compiler and generated native
  programs, including the new input tests at both optimization levels.

## 2026-10-24 — performance gate instrumentation

- Extended `make benchmark` to cover decoding, filtering at three thresholds, and
  signed-sample summation with independent ABI-checked C references.
- Added three runtime trials, raw batch measurements, build wall times, object
  sizes, host/toolchain metadata, and vectorization remarks for both languages.
- Added 512-case differential validation with five filtering thresholds, output
  preservation, extrema, input rejection, and O0/O2 sanitizer coverage.
- All normal, sanitizer, formatting, and benchmark commands passed. Local results
  and primary-source research are recorded in the kernel performance report.
- The 1.5x target remains unmet for decoding and summation. No compiler checks
  were removed; the next optimization experiment requires audited range proofs.

## 2026-10-24 — bounded i16 reduction optimization

- Added an independent, deliberately narrow prefix-sum proof pass. It annotates
  a single i64 accumulator update only when the complete loop pattern and a safe
  immutable length bound are established; budget numbers are not proof evidence.
- Lowering preserves overflow checks outside that pattern and retains index,
  counter, and ABI checks. Explain output reports proven update sites.
- New unit/native tests cover proof boundaries, mutation/control-flow rejection,
  extrema, SIMD tails, and retained overflow/bounds faults at O0/O2. Normal tests,
  sanitizer runs, 10000 fuzz inputs, formatting, and static analysis passed.
- Clang now vectorizes the sum. Alternating preserved/new benchmark runs at 4096
  samples measured approximately 3428 ns before and 721–728 ns after, near the
  checked C reference. Object text grows from 168 to 432 bytes. The research
  follow-up records variability and scope; the overall performance gate is unmet.

## 2026-10-24 — backend focus approved

- Updated the active plan to target backend services, workers, daemons, and
  infrastructure. The primary acceptance application is an HTTP API with a
  bounded background-work queue, explicit overload behavior, and graceful shutdown.
- Ordered milestones around resource ownership, service foundations, TCP, safe
  concurrency, and HTTP/worker operation. Async scheduling remains a design
  decision to validate, not an implemented feature or frozen syntax.
- Retained kernel benchmarks and optional work budgets as supporting capabilities.
  Linux service validation remains outstanding; current execution evidence is Termux.
- Updated README positioning and marked the older product proposal as historical.
  Documentation-only change; compiler behavior and AGENTS.md are unchanged.

## 2026-10-24 — scalar buffer runtime foundation

- Added an internal C runtime for fallible scalar-buffer creation, exact reserve,
  resize, move into vacant slots, and drop. Capacity-byte limits and size arithmetic
  are checked before allocation; failed growth preserves owner state and live data.
- Allocator identity follows moves. Newly exposed elements are zeroed, including
  after shrink/regrow. Growth temporarily retains old and new allocations; the
  per-buffer limit does not claim to bound process RSS or aggregate live memory.
- Added guarded allocator tests, deterministic failures at each allocation site,
  exact-release accounting, and 10000 operations checked against a byte model.
  `make test`, `make sanitize-runtime`, `make check-format`, and Clang static
  analysis of the runtime passed. No new external dependency was introduced.
- Documented owner-slot transitions, cleanup edges, Result payload transfer,
  borrow invalidation, memory effects, and required compiler integration tests.
- This is a runtime prerequisite only. The compiler still has no dynamic buffer
  type, ownership cleanup IR, or automatic resource cleanup in generated programs.

## 2026-10-24 — shared compiler function exits

- Extracted function-body emission into `compiler/backend/c/function.c`. Internal
  functions now publish return values through a function-scope slot and converge
  on one epilogue, including native stdin/stdout paths and void fallthrough.
- Local initialization precedes the entry fault check. Fault exits preserve the
  fault code and reset the result before the common return; C ABI wrappers retain
  their existing status and argument-validation behavior.
- Added generated-program epilogue instrumentation at O0/O2. Tests verify exact
  exit counts, aggregate Result payloads, nested/loop returns, bounds/overflow/
  division faults, left-to-right argument evaluation, suppression of later
  arguments and callee entry after a fault, and native I/O success/error exits.
- `make test`, `make sanitize`, and `make check-format` passed on Termux AArch64.
  The sanitizer run covers both the compiler and generated C with ASan/UBSan.
- This is a compiler prerequisite for resource cleanup, not buffer integration.
  Stable temporary owner slots, ownership transfer, scope drops, memory effects,
  and the source-level dynamic buffer API remain outstanding. AGENTS.md is unchanged.

## 2026-10-25 — owned byte buffers in compiled Fermi programs

- Added intrinsic `Buffer<u8>` and trusted `std.mem.bytes(length, limit)` returning
  `Result<Buffer<u8>, mem.Error>`. Allocation is fallible and zero-initialized;
  length remains fixed. Empty owners allocate no storage. The limit bounds the
  individual payload, not process memory.
- Added an owner-slot planning pass over the typed tree. Resource temporaries live
  at function scope; moves, Result construction/matching, argument handoff, and
  return transfer invalidate the source obligation. Scope and fault cleanup release
  active temporaries and locals, with reverse-order local cleanup and vacant-slot
  checks. Replacement evaluates the new value before releasing the old owner.
- Added call-scoped byte views, resource copy/export/container restrictions, and
  the transitive `memory` effect. Budget annotations reaching allocation or release
  are rejected. Resource move diagnostics no longer recommend copying buffers.
- Closed an indexing lifetime hole: computing an index or slice bound cannot
  consume the storage owner whose address was already selected for access. Added
  rejection tests for index and both slice-bound positions.
- Generated C embeds the trusted runtime with internal linkage. Two independently
  generated resource objects link successfully using only their scalar C ABI.
  The compiler still requires its configured library/runtime source directories.
- Added `examples/cli/buffer.fe`; the compiled executable prints `Fermi` and a
  newline. Generated-program tests at O0/O2 cover allocation errors and limits,
  empty owners, zeroing, moves, Result forwarding, nested/loop scopes, conditional
  ownership, replacement, argument faults, callee faults, and exact release counts
  and order. A guarded allocator fails every allocation in a four-buffer chain.
- `make test`, `make sanitize`, `make check-format`, and 10000 frontend/lowering
  fuzz inputs passed on Termux AArch64. Clang static analysis reported no findings
  in the new ownership, memory-emission, and function-emission modules. No external
  dependency was added; AGENTS.md is unchanged.
- Reserve/resize, descriptor borrowing, wider buffer elements, nested resource
  containers, and stable resource ABI remain unsupported. The broader stage-1
  resource gate remains open, particularly generated-program growth-failure tests.

## 2026-10-25 — owned copies of borrowed bytes

Added `std.mem.from_bytes(source: read u8[], limit: usize)` returning
`Result<Buffer<u8>, mem.Error>`. The implementation is ordinary Fermi: allocate,
copy through checked indices, and transfer the resulting owner. It introduces
no native function or compiler intrinsic. The source remains unchanged on both
success and allocation failure; the resulting storage has an independent lifetime.

Generated-program tests cover lengths 0 through 256, every byte value, empty
null views, destination isolation, limit rejection, allocator failure, cleanup
on a subsequent bounds fault, and returning a copy after its source is released.
Both allocation sites in the lifetime scenario support deterministic failure
injection. Allocation/release counters verify cleanup. Additional rejection tests
cover discarded Results and unsupported zero-heap budget declarations.

Validation: `make test`, `make check-format`, and the buffer integration suite
with ASan/UBSan at O0/O2 passed. `examples/cli/owned_bytes.fe` compiled and printed
`Fermi` with exit status 0. Logs are in `build/from-bytes-{tests,format,sanitize}.log`.

Buffer length remains fixed. Descriptor borrowing, reserve and resize remain the
next ownership milestone; this operation does not expose any of those features.

## 2026-10-25 — product requirements and implementation gates

Added a documentation entry point, product requirements, implemented-specification
index, runtime architecture, decision register, descriptor-borrow proposal and
primary-source research review. Updated the existing compiler architecture and
roadmap to distinguish current typed-tree lowering from proposed future stages.

Product requirements identify acceptance evidence for ownership, failure handling,
bounded service work, shutdown, native integration and performance. The first
service remains a single-process in-memory worker. Foundry remains a speculative
longer-term workload, not an implemented language contract or initial release gate.

The next buffer proposal defines a distinct exclusive descriptor borrow,
non-escaping reborrows, fallible reserve/resize signatures, lowering obligations
and a generated-program acceptance matrix. No proposed syntax was enabled.

Validation: checked 70 local document links and balanced code fences across the
11 reviewed documents. Cross-checked pipeline entry points, runtime growth behavior
and current parser restrictions against source. No compiler/runtime code changed;
execution suites were not rerun for this documentation-only change. No new runtime
safety, platform support or performance result is claimed.

## 2026-10-25 — borrowed argument evaluation correction

Audited call-scoped views against ownership transfer during argument evaluation.
The previous checker accepted a borrowed buffer followed by a different view whose
slice bound consumed that buffer. Running the accepted reproducer with ASan
confirmed heap-use-after-free. The checker now examines nested uses in every
other argument, including view bounds, and handles same-owner view bounds separately
from the storage path. Argument-list links are no longer modified during inspection.

Added positive/negative semantic cases, an executable-source rejection fixture,
O0/O2 CLI build rejection and a frontend fuzz seed. Updated the pilot and buffer
contracts. The audit record describes the exact finding and remaining scope.

Validation: `make test`, `make sanitize`, `make check-format`, and `make fuzz`
all passed. Fuzzing completed 10,000 runs without a new finding. The unsafe source
now fails with E_ALIAS; its reverse-order variant fails with E_INIT. Shared reads,
disjoint literal edit slices, precomputed bounds and unrelated-owner operations
remain accepted. Logs: `build/borrow-audit-{tests,sanitize,format,fuzz}.log` and
`build/fuzz/frontend.log`. The original ASan report is `build/borrow-order.asan`.

This is a correction to existing ownership semantics, not a new syntax feature.
Descriptor borrowing and source-level growth remain unimplemented. These local
Termux results do not establish production readiness or Linux platform coverage.

## 2026-10-25 — Acode language plugin

Added tools/acode-fermi with separate Ace and CodeMirror adapters, a shared Fermi
lexer, theme-aware syntax tokens, keyword/type completion, twelve snippets and
debounced lexical/delimiter diagnostics. Editor checks use UTF-16 positions and
have explicit input/diagnostic limits. They do not claim parser, import, type or
ownership validation; authoritative checking remains the native compiler.

Lifecycle tests cover annotation preservation, mode restoration after rename,
cancelled pending work and removal of plugin listeners/completers. Snippet defaults
compile in their documented contexts. A Fermi demonstration program compiles and
prints Fermi with exit status 0. The CodeMirror builtin token uses the documented
dotted modifier form. No third-party package installation or runtime dependency
is introduced.

Validation: twelve Node tests passed, including deterministic ZIP generation and
bundle lifecycle registration. node --check and unzip -t passed on the generated
package. The archive contains plugin.json, main.js, icon.png, readme.md and an
example .fe file; its SHA-256 is emitted beside it. Installation instructions and
source compatibility targets are documented in tools/acode-fermi/README.md.

Direct testing inside the Android Acode application has not been performed in
this workspace. Ace v1.11.0 and upstream CodeMirror APIs were reviewed as source
compatibility targets; this is not a verified claim for every Acode release.

## 2026-10-25 — exclusive descriptors and byte-buffer growth

Implemented edit Buffer<u8> parameters with distinct TY_BUFFER_EDIT type/access
identity. Source calls can borrow a mutable owner or reborrow a descriptor;
returns, locals, fields, arrays, Result payloads, copies/moves, replacement and
C ABI exposure are rejected. Lowering passes a pointer to the caller descriptor.
Borrow arguments do not reserve owning temporaries and never acquire cleanup
obligations. Scalar views project the current allocation after a growth call.

Added std.mem.reserve and std.mem.resize, both returning Result<usize, mem.Error>,
and read-only .capacity metadata. Reserve returns capacity; resize returns length.
The runtime preserves the owner on failure and zeroes newly exposed bytes.
The checker also rejects descriptor access in index/slice bounds and assignment
values that could relocate the target after preparing its address. Descriptor
metadata is conservatively excluded from immutable length-bound refinement.

Generated-program tests exercise empty/no-op growth, size/limit failures,
shrink/regrow zeroing, nested reborrows, allocation failures, subsequent faults,
late argument faults, return transfer and 64 growth iterations. Guarded allocator
accounting checks releases and transient old-plus-new payload bytes. Each allocation
site in the growth loop and argument/return scenarios is rerun with deterministic
failure injection. Separate generated objects retain private runtime linkage.

Validation on Termux AArch64: make test, make sanitize, make check-format and
make fuzz passed; fuzzing completed 10,000 inputs. Twelve Acode tests also passed
against the updated library and snippets, and the refreshed ZIP passed unzip -t.
The buffer_growth.fe example compiled at O2, printed Fermi and exited with status 0.
Logs: build/buffer-growth-{tests,focused,sanitize,format,fuzz,acode}.log.
Documentation link validation checked 82 local targets.

This completes the byte-buffer growth integration subset. General object borrows,
wider source Buffer types, resource nesting, file/socket resources, task runtime
and network services remain unavailable. Linux deployment and production readiness
are not established by these local results.

## 2026-10-25 — bounded startup configuration

Added std.env.read_into with portable key validation, bounded value copying and
failure-preserving output. Missing and empty values remain distinct; oversized
values are rejected without truncation. The compiler recognizes env.read as a
transitive external effect and rejects environment access in budgeted functions.
Standard module resolution prevents project files from shadowing these modules.

Added strict decimal parsing and inclusive range validation in std.num, written
in Fermi. Inputs are limited to 20 ASCII digits. Overflow is checked before
arithmetic. The compiler reports work bounds of 1181 and 1219 respectively under
fermi-work-1, with zero heap use and no external effects. These are compiler-model
bounds, not execution-time guarantees. std.config composes environment lookup
and numeric validation using fixed local storage. std.io.write_stderr provides
checked diagnostic output through the existing write implementation.

The startup_config.fe example validates worker count, per-job bytes and their
combined active-byte allowance before accepting startup configuration. Invalid
configuration returns status 78 without printing raw values. This example does
not launch workers or enforce process memory limits.

Validation on Termux AArch64: make test, make sanitize, make check-format and
make fuzz passed. The configuration harness includes 5000 deterministic numeric
differential cases, output-preservation checks, module-shadow rejection and
compiler rejection tests. Frontend fuzzing completed 10,000 inputs. The example
compiled at O2, printed "startup configuration accepted" and exited with status 0.
Logs: build/configuration-{tests,focused,sanitize,format,fuzz}.log.
Updated documentation links were checked for existing local targets.

Environment lookup retains libc lifetime and synchronization requirements;
its work and heap costs are not certified. File/socket resources, task execution
and network services remain future work. These checks do not establish production
readiness or portability beyond the tested host.

## 2026-10-25 — unsigned bits and binary fields

Implemented unsigned &, |, ^, ~, << and >> for u8, u32 and usize. Shift counts
have type usize and must be below the Fermi operand width. Invalid literal counts
are rejected; dynamic violations report fault code 4 through the shared epilogue.
Lowering uses uint64_t operands and unsigned result conversion, avoiding signed
C promotions. High bits discarded by left shift do not alter checked arithmetic.
Evaluation remains eager and left to right. New precedence levels preserve the
relative ordering of existing operators. Type-argument closing delimiters remain
parseable beside shift tokens. Signed and mixed-type operations are rejected.

Added std.binary as an isolated standard module implemented in Fermi. It reads
and writes u32 fields in explicit big/little-endian order. Short writes preserve
the entire destination; successful writes touch four bytes. The compiler computes
work bounds of 46 for reads and 102 for writes, with heap zero and no external
effects under fermi-work-1. The binary_frame.fe example compiled at O2, printed
"binary frame validated" and exited with status 0.

Validation on Termux AArch64: make test, make sanitize, make check-format and
make fuzz passed. Tests cover all 65,536 byte operand pairs; every valid byte
shift; 10,000 deterministic word pairs across valid shift counts; invalid counts
including SIZE_MAX; evaluation order; precedence; and type/constant rejection.
Binary tests cover each byte in each field position and 5000 deterministic values
against independent byte vectors. Short and oversized destinations use sentinels.
Allocation tracking verifies exactly-once cleanup on shift faults and precedence
of allocation failure. Both O0 and O2 are exercised, including sanitized generated
code. Frontend fuzzing completed 10,000 inputs. Logs:
build/bitwise-{tests,focused,sanitize,format,fuzz}.log.
Documentation validation checked 69 local targets before the decision-register
update; the added decision links reference the same checked specification/research.

This adds reusable foundations for flags, formats and application protocols.
It does not implement signed bit operations, bitwise compound assignment,
additional integer widths, networking or general serialization. No performance
comparison or deployment portability claim follows from these checks.

## 2026-10-25 — process arguments and executable stream accounting

Added std.args.count/read_into with an explicit internal startup context. Generated
executables initialize that context from C main before entering Fermi. User indices
exclude argv[0]; argument copies preserve raw bytes without allocation, termination
or truncation. Domain errors preserve output. The compiler validates native
signatures, protects standard-module resolution and propagates process.args.
Budgeted functions reaching argument access are rejected. Generated libraries
report unavailable rather than guessing host process state; a public embedding
initialization ABI remains future work.

Added runtime/process/arguments.c with independent caller-supplied contexts and
private linkage in generated objects. A shared runtime snapshot copier now embeds
both argument and buffer runtimes, retaining existing object independence.
std.fmt.usize_into implements canonical decimal output in Fermi, validating output
capacity before any write. Its current work certificate is 1322 under fermi-work-1,
with heap zero and no effects; no CPU-time or stack guarantee is implied.

Added examples/tools/bytecount as a multi-file executable application. It validates
one numeric CLI limit before allocation or input, reads stdin through a fixed
4096-byte owned buffer, checks remaining allowance before addition and writes a
canonical count at EOF. Over-limit input, argument errors, allocation failure and
I/O failure use distinct exit statuses and fixed English diagnostics. The tool
can read one extra chunk before detecting excess and does not time out a stalled
producer. The Fermi application needs no application-specific C host.

Validation on Termux AArch64: make test, make sanitize, make check-format,
make test-runtime and focused tests/arguments.sh passed. New tests exercise empty,
non-UTF-8 and shell-sensitive literal arguments, unavailable library contexts,
independent runtime contexts, bounded copying, every capacity around short values,
5000 differential numeric-format cases, decimal boundaries and sentinel integrity.
Executable tests cover multiple chunks, exact/exceeded limits, unreadable stdin,
unwritable stdout/stderr, invalid CLI input and injected allocation failure.
Both O0 and O2 run under the generated-program checks and sanitizer suite.

The initial closed-stdin failure test observed EOF on this Android startup path;
using a write-only input descriptor correctly exercises read failure. The corrected
suite passed. Direct execution counted "Fermi\n" as 6 bytes and returned status 65
when the allowance was 5. Documentation links checked 77 existing local targets.
Logs: build/arguments-{build,runtime,focused,tests,sanitize,format,contracts}.log.
The startup/encoding research and contracts document byte lifetime, embedding,
effect accounting and platform limits. No networking, scheduler, file resources,
monotonic deadlines or new deployment-target validation is claimed.

## 2026-10-26 — monotonic time and pure deadlines

Added std.time.monotonic_ns with a clock.read effect and checked POSIX timespec
conversion. Host failure, invalid readings and representation overflow produce
domain errors. Generated C requests POSIX clock declarations before system
headers and uses CLOCK_MONOTONIC without a calendar-clock fallback. Native
signatures and error variants are checked, and project shadows remain excluded.
Budgeted callers reaching clock access receive E_EFFECT.

Added deadline_after and remaining_ns in Fermi. Deadline addition rejects overflow;
remaining allowance saturates at zero on expiration. The helpers read no clock,
allocate no heap and have work bounds of 22 and 12 respectively under fermi-work-1.
Raw usize nanoseconds remain a unit/domain safety limitation. This API does not
sleep, cancel work or interrupt blocking I/O, and Linux monotonic time excludes
system suspension. The specification records those limits explicitly.

Validation on Termux AArch64: make test, make sanitize, make check-format and
focused tests/time.sh passed. O0/O2 tests inject host failures, negative/invalid
fields, exact SIZE_MAX conversion and one-step overflow. Boundary pairs exercise
pure addition and saturated subtraction without querying the clock. Each native
run samples the real clock 1000 times and accepts equal but never backward
readings. The deadline.fe example compiled at O2, printed "monotonic deadline
initialized" and exited with status 0. Documentation validation checked 81 local
targets. Logs: build/time-{build,focused,tests,sanitize,format}.log.

The integration harness uses -iquote for generated headers, keeping an artifact
named time.h from shadowing the system <time.h>. Research and the decision register
record startup/header requirements, clock semantics and future strong-time-type
questions. No networking, deadline enforcement or new target verification is
claimed by this milestone.

## 2026-10-26 — programmer-needs review, file lifecycle and unit Results

Reviewed primary Rust survey, Android safety, Go error-composition, Linux close
and LLVM frontend materials. The research distinguishes evidence from Fermi
inferences and prioritizes resource correctness, meaningful Result APIs, explicit
compiler invariants and measured developer costs. It does not infer universal
programmer demand or Fermi superiority from another project's survey or results.

Implemented a read-only regular-file C runtime with bounded paths, atomic
close-on-exec, descriptor inspection, move, chunked read, consuming explicit close
and fallback drop. FIFO acquisition is nonblocking before rejection. Linux close
errors, including EINTR, consume the owner without retry; drop preserves errno.
Real-I/O and injected-syscall tests cover descriptor zero, interrupted acquisition
and reads, partial reads, failed inspection/close, move, EOF and cleanup. The
runtime is not embedded into generated programs and no std.fs/File source type
is available yet. The file-resource design records the compiler integration gate.

Implemented Result<void, E>, Result.ok() and case Result.ok() without payloads.
Constructor and pattern checks reject invented payloads; mandatory Result handling
and explicit forwarding remain unchanged. Unit cases create no binding. Internal
C Results and exported status/error envelopes omit the success value field.
The work model assigns zero cells to void payloads; existing supported Result
shapes retain their costs. Added generated tests for nested unit Results, branch
initialization, forwarding, C ABI shape, faults and rejection. Buffer integration
checks exactly-once cleanup on unit return/fault and allocation failure.

Validation on Termux AArch64: make test-runtime, make sanitize-runtime, make test,
make sanitize, make check-format and make fuzz passed. Normal and generated
sanitizer suites exercise O0/O2; fuzzing completed 10,000 inputs with a unit-Result
seed. The unit_result.fe example compiled at O2, printed "unit Result validated"
and exited with status 0. Documentation checks resolved 107 local targets.
Logs: build/file-{runtime,sanitize,format}.log and
build/file-unit-{tests,sanitize,format,fuzz}.log.

The next implementation gate is opaque file owner identity, exclusive borrowing,
resource-specific effects/drop dispatch and generated cleanup under failures.
Current checker/cleanup assumptions about buffers must be revised before files
enter Fermi source. Read-only runtime support does not imply write durability,
path sandboxing, deadline enforcement or validated non-Linux close behavior.

## 2026-10-26 — file ownership integrated into Fermi source

Integrated std.fs.File as a trusted opaque native owner with open_read, exclusive
read_into and consuming close returning Result<void, fs.Error>. Native resource
declarations are accepted only for the standard file interface. User construction,
field access, copy, borrow escape, resource nesting and owner C ABI exports remain
rejected. Stable mutable owners and borrowed parameters support exclusive reborrow;
conflicting argument uses and use after close are rejected.

Generalized owner-borrow classification while retaining buffer projection rules.
Cleanup dispatch now distinguishes file release from buffer release, including
resource Result payloads. Native owned parameters are included in fallback cleanup.
Effects distinguish fs.open, fs.read and fs.close from memory allocation/release;
file access and cleanup remain outside work/heap certificates. Runtime snapshots
retain private linkage across independently generated objects.

Added the executable filecat tool. It validates a CLI path, opens a regular file,
streams through one fixed 4096-byte buffer, and explicitly closes on success and
streaming failure. Failed close is reported without replacing an earlier I/O
failure. Direct O2 execution copied library/std/fs.fe byte-for-byte with status 0.

Validation on Termux AArch64: make test, make sanitize, make check-format and
make fuzz passed. Focused tests/files.sh also passed after adding pending-Result
and application close-error cases. Generated O0/O2 tests check Result forwarding,
reborrows, replacement, nested scopes/loops, late argument and callee faults,
return transfer, mixed buffer/file owners and exactly-once cleanup. Failure
injection covers every acquisition in a 16-open nested loop, allocation failure,
read failure and close EINTR/EIO. Close failures preserve the original fault.
Separate objects link through scalar/view exports; real filecat tests compare
multi-chunk and empty inputs. Frontend fuzzing completed 10,000 inputs; it does
not fuzz trusted module loading or native syscalls.

Logs: build/file-owner-{build,focused,tests,sanitize,format,fuzz,effects}.log.
The specification, runtime design and roadmap now identify std.fs as implemented.
General native owners, resource fields, writing/sync, networking and cancellation
remain unavailable. This does not establish non-Linux close semantics or production
readiness.

## 2026-10-26 — core control flow and arithmetic consistency

Completed the integer compound assignment family with `/=` and `%=` across the
lexer, parser, semantic checker, budget analysis, and native backend. Both reuse
the checked division/remainder implementation. Literal zero divisors are rejected
at compile time. Budget analysis conservatively retains the old unsigned bound
and full signed type bound rather than treating the divisor as the new value.

Integer literals in noninteger contexts now report `E_TYPE`; integer range
violations retain `E_RANGE`. Added semantic regressions for all six integer types,
immutable and uninitialized targets, incompatible operands, boolean conditions,
branch initialization, loop scope, return coverage, required parentheses, and
missing or duplicate Result branches.

Added native control-flow regressions for `-O0` and `-O2`: nested `for`/`while`,
`else if`, Result branch order and initialization, short-circuit behavior, signed
division/remainder, zero and overflow faults, and single evaluation of an indexed
compound-assignment target. The suite is part of `make test` and `make sanitize`.

Validation: `make test`, `make sanitize` (AddressSanitizer and
UndefinedBehaviorSanitizer), and `make check-format` passed on Termux Android
AArch64. The new native control-flow suite passed at both optimization levels
with and without sanitizers. Logs: `build/core-tests.log`,
`build/core-sanitize.log`, and `build/core-format.log`.

## 2026-10-26 — contextual unary integer literals

Fixed asymmetric operand typing: `value > -1` accepted an `i32` value, while
`-1 < value` incorrectly required the value to be `i64`. The semantic checker now
recognizes integer literals wrapped in unary `+`, `-`, or `~` as contextual
literals. Recognition is iterative and excludes calls, bindings, and ownership
operations. Typed values still require explicit conversions. Shift counts retain
the independent `usize` requirement.

Semantic regressions cover signed minima, unary nesting, unsigned complements,
out-of-range literals, invalid negation/complement domains, and mixed typed
operands. Native regressions cover i16/i32/i64 boundaries, a function call in the
right operand evaluated once, checked nested negation overflow, and u8 complement
at both optimization levels.

Validation passed on Termux Android AArch64: `make test`, `make sanitize`, and
`make check-format`. Native regressions passed at `-O0`/`-O2`, including under
AddressSanitizer and UndefinedBehaviorSanitizer. Logs are available in
`build/literal-tests.log`, `build/literal-sanitize.log`, and
`build/literal-format.log`.

## 2026-10-26 — exhaustive matches for fieldless enums

Extended the core match statement from Result-only dispatch to existing fieldless
enums. Cases use `case State.variant { ... }` or imported
`case module.State.variant { ... }`. The checker resolves nominal type identity,
rejects unknown/duplicate/missing variants, and shares branch initialization,
return coverage, and ownership joins with Result matching. Enums remain scalar
values and are not consumed by a match. The backend evaluates the subject once
and emits ordinal comparisons; payloads, wildcard patterns, and guards are not
introduced. Work bounds include a conservative comparison charge per enum case.

Fixed an existing null dereference in budget bound inference for declarations
initialized from enum variants: a qualifier naming a type has no value-expression
type. Such bounds now remain conservative instead of dereferencing the qualifier.

Added semantic coverage for malformed and incomplete matches, nominal mismatch,
branch initialization, return paths, nested Result error matching, and enums at
128 variants with rejection at 129. Native tests cover case order, all variants,
subject evaluation once, subject faults, nested loops, and imported public enums
with same-named local types. The frontend fuzz corpus now includes enum matches.

Input review: source text remains untrusted. Enum coverage indexing uses only
variant indices created by the parser's existing 128-variant limit. Case names
resolve to fields before lowering; source names are not inserted into C dispatch
expressions. Existing source, token, allocation, and nesting limits remain active.

Validation passed on Termux Android AArch64: `make test`, `make sanitize`,
`make check-format`, and `make fuzz` (10,000 frontend runs). Native control-flow
and enum suites passed at `-O0` and `-O2`, including AddressSanitizer and
UndefinedBehaviorSanitizer runs. Missing-variant diagnostics were also checked
from the CLI and identify the unhandled variant. Logs are in
`build/enum-match-{tests,sanitize,format,fuzz}.log`.

## 2026-10-26 — complete fixed-width integer families

Added `i8`, `u16`, and `u64` to complete signed/unsigned 8/16/32/64-bit scalars.
Updated primitive registration, signedness and bit widths, literal range checks,
checked arithmetic, signed minima, conversions, work bounds, C representations,
and prelude alias restrictions. Primitive name tables now use designated entries
to keep type indices aligned. `u64` and `usize` remain distinct source types.
`Buffer` storage remains byte-only; floating point is outside this change.

Native tests exercise every i8 operand pair for five arithmetic operations,
negation, u16/u64 boundaries, shift faults, narrowing and widening, scalar C ABI,
struct/array values, mutable u16 views, and a budgeted u16 counting loop. Semantic
tests cover all nine integer types and invalid ranges, conversions, and shifts.
The module suite checks the three new reserved prelude aliases. The frontend
fuzz seed set includes the new integer fixture.

Validation passed on Termux Android AArch64: `make test`, `make sanitize`,
`make check-format`, and `make fuzz` (10,000 frontend runs). All native integer
regressions passed at `-O0` and `-O2`, both normally and under AddressSanitizer /
UndefinedBehaviorSanitizer. Logs: `build/integer-family-{tests,sanitize,format,fuzz}.log`.

## 2026-10-26 — break and continue with scoped cleanup

Implemented semicolon-terminated break/continue statements targeting the nearest
for/while loop. Semantic loop frames accumulate break and continue states,
continue states join before checking the for step, and post-loop states include
break exits. Jumps record exact resource cleanup lists for the scopes they leave.
The C backend emits loop-specific labels after the body or loop, retaining for
initializer owners across continue and releasing them on exit.

Closed a Result back-edge gap: newly assigned outer Results must be handled
before another iteration can overwrite them. Rejected declarations in for steps,
which could otherwise repeatedly initialize a resource binding without a clear
per-iteration scope. Work analysis counts jump statements and invalidates outer
written bounds after loops where jumps can bypass restoring assignments.

Added native for/while/nested/match tests and semantic rejection for invalid
jumps, unreachable code, moved owners, unhandled Results, and bypassed budget
bounds. Extended the file suite with nested file/buffer jump cleanup, reverse
close order, retained outer owners, resource-valued initializers, injected open
and allocation failures, close EINTR, and fault cleanup. Added a frontend fuzz seed.

Validation passed on Termux Android AArch64: `make test`, `make sanitize`,
`make check-format`, and `make fuzz` (10,000 frontend runs). Native loop-control
and mixed-resource cleanup tests passed at `-O0` and `-O2`, both normally and
with AddressSanitizer/UndefinedBehaviorSanitizer. Logs are available in
`build/loop-control-{tests,sanitize,format,fuzz}.log`.

## 2026-10-26 — native run command and structured CLI help

Added `run`/`r` to build and execute source in one command, defaulting to `main.fe`.
Options parsing and command help now live in `compiler/driver/options.c`; process
execution and temporary output ownership live in `compiler/driver/run.c`.
No-argument invocation prints the command overview; help/version have short
aliases and `help COMMAND` supports command-specific usage.

Run shares executable generation and atomic output publication with build.
Default output uses a private temporary directory; `-o` retains the executable.
Execution uses an absolute path and argv through execv, with inherited streams,
environment and working directory. An exec-status pipe distinguishes launch
failure from child exit 127. Exit status and signal status are collected without
capturing program output. Terminal interrupt handling retains the parent long
enough to clean temporary output. Compiler option scanning stops at `--`.

Added driver integration coverage for default entry, help, aliases, literal and
empty arguments, stdin/stderr, relative file access, statuses, signals, temporary
cleanup, failed-build preservation, missing backend, permission failures, and
executable-format failures without a shell fallback. The suite is included in
normal and sanitizer targets. Updated CLI, module, diagnostic and example docs.

Validation passed on Termux Android AArch64: `make test`, `make sanitize`, and
`make check-format`. The dedicated runner suite passed normally and with the
sanitized compiler/backend, including default-entry runs from another working
directory. Actual queue and piped bytecount programs were also launched using
`fermi run` with the checkout build prepended to PATH. The separately installed
compiler was left untouched; README documents selecting `build/fermi` in the
current shell. Logs: `build/run-command-{tests,sanitize,format}.log`.

## 2026-10-26 — avoid repeated native builds in run

Measured the reported Hello World run cost separately from frontend checking and
native execution. Native backend work dominated the approximately 300–400 ms run
and approximately 101 MiB peak. Added a generated-C-addressed native run cache in
`build/.fermi-run`, retaining frontend checking on every invocation. Key identity
includes native flags, optimization, default compiler path/stat, host, PATH and
loader environment metadata. Explicit backend wrappers and custom build search
paths bypass reuse; `--no-cache` retains the temporary build-and-run path.

Factored executable emission and native compilation into shared functions so a
cache miss compiles exactly the source snapshot used for its key. Added streaming
SHA-256 to avoid buffering the generated source. Entry publication uses private
staging and directory rename; hits verify owned regular artifacts and the binary
checksum. Tests cover stale modules, invalid source, corruption/symlinks, parallel
publication, runtime arguments and bypass paths. Complete system-header/linker
tracking is explicitly outside this cache contract.

Local hyperfine sample (2 warmups, 10 runs): warm run 36.4 ms / 3.3 MiB; uncached
run 371.2 ms / 100.7 MiB; direct executable 12.1 ms / 3.1 MiB. Device load caused
substantial uncached variance. This improves repeated build-and-run latency;
initial native compilation and application execution speed are unchanged.

Validation: `make test`, `make sanitize`, and `make check-format` passed. A further
loader-path regression first demonstrated that splitting LD_LIBRARY_PATH on spaces
missed directory metadata changes. The separator logic now keeps spaces inside
library directory names. That added regression and the runner suite passed with
the rebuilt normal and AddressSanitizer/UndefinedBehaviorSanitizer compiler.
Logs: `build/run-cache-{tests,sanitize,format}.log`,
`build/run-cache-loader-{regression,fixed,sanitize}.log`, and
`build/run-cache-runner-sanitize.log`. No global compiler installation was changed.

## 2026-10-26 — concise output and entry functions

Added allocation-free `std.io.print` and `println`, implicit immutable access for
string literals passed to `read u8[]`, and executable `main() -> void` with normal
exit status zero. Explicit access remains required for bindings and mutable
views. Recoverable `io.write` Results remain mandatory to handle. Printer failures
propagate runtime fault 5 and use existing resource cleanup paths.

The Hello World example now uses five lines. Added native I/O regressions for
short writes, EINTR, zero progress, newline failure, transitive fault propagation,
and owned buffer cleanup, plus semantic rejection cases. Feature priorities from
the contributor discussion are recorded in `docs/design/feature-priorities.md`;
they are design proposals, not claims of implementation.

Validation: `make test`, `make sanitize` (AddressSanitizer/UndefinedBehaviorSanitizer),
and `make check-format` passed. The focused printing suite also verifies void entry
with process arguments. Logs: `build/concise-{tests,sanitize,format}.log` and
`build/concise-printing-focused.log`. No global compiler installation was changed.

## 2026-10-26 — explicit Result propagation

Implemented `try(expression)` through lexer, parser, type/ownership checking,
work analysis, and native C lowering. It evaluates once, consumes a Result,
produces the success payload or forwards the exact same nominal error type.
Unit success, nested Results, successful owner transfers, transitive effects,
and the normal resource cleanup epilogue remain explicit parts of the contract.
Other pending Result bindings prevent propagation, as they prevent explicit return.

Refactored `std.num.usize_range` and `std.mem.from_bytes` to use try. Added a
bounded worker-limit example and O0/O2 regressions for every u8 input, ordered
calls, short circuits, loops, assignment preservation, arithmetic/bounds faults,
buffer failure injection, pending owner temporaries, file cleanup, and preservation
of a primary read error when automatic close also fails. Semantic tests cover
invalid syntax, type mismatches, moves, loop back edges and mandatory handling.

The accepted contract is `docs/spec/error-propagation.md`. `try` is now reserved;
there is no implicit error conversion or rollback of earlier edits. No exported
C ABI layout changed. The runtime/export symbol collision found while preparing
the file fixture is recorded in the foundation audit as separate open work.

Validation: `make test`, `make sanitize`, `make check-format`, and `make fuzz`
passed, including 10,000 frontend mutation runs. The focused O0/O2 propagation
suite and runnable worker-limit example also passed. Logs:
`build/try-{tests,sanitize,format,fuzz}.log`, `build/try-integration-final.log`,
and `build/try-example.log`. No global compiler installation was changed.

## 2026-10-26 — typed payload enums

Added single-payload enum variants, typed constructors, and exhaustive `match`
branches. Fieldless enums retain their scalar representation. Payload enums can
carry values, earlier payload enums, buffers, or files; the active payload moves
into a match binding. Resource cleanup runs on scope exit, replacement, propagation,
and faults. Payload enum layouts remain internal and cannot cross the exported C
ABI; Result error enums remain fieldless.

Cached abstract type cells and resource effects when each type is completed. This
keeps analysis linear across shared nested type graphs. Generated C uses one
cleanup helper per resource enum, avoiding repeated expansion of shared branches.
The focused regression compiles 31 nested two-way resource enums into under 64 KiB
of generated C. Added native O0/O2 value and owner tests, semantic rejection cases,
module import coverage, a runnable event-dispatch example, and a frontend fuzz seed.
The accepted syntax and limits are in `docs/spec/payload-enums.md`.

Validation passed on Termux Android AArch64: `make test`, `make sanitize`
(AddressSanitizer/UndefinedBehaviorSanitizer), `make check-format`, and `make fuzz`
(10,000 runs). The focused payload suite passed at O0/O2, including the imported
module example and deep shared-type regression. Logs:
`build/payload-{tests,sanitize,format,fuzz}-current.log`. The installed compiler
was not changed.

## 2026-10-26 — compiler input and ABI hardening

The compiler now rejects exported C names that overlap embedded runtime functions
or another export's generated result typedef. Previously `file_read` with std.fs
and `inspect` paired with `inspect_result` passed `fermi check`, then failed in
Clang with E_TOOLCHAIN. Both now fail in semantic checking with E_ABI.

Entrypoint and imported source files now open with O_NONBLOCK and must pass fstat
as regular files before reading. This prevents an unwritten FIFO supplied as a
source path from blocking the compiler. The project import boundary is still not
a filesystem sandbox against concurrent directory mutation.

Validation: the full normal and ASan/UBSan suites, formatting checks, and 10,000
frontend fuzz runs passed. Clang static analysis of source opening and semantic
checking reported no findings. A final diagnostic-only correction uses the current
module's export span for cross-module collisions; semantic and CLI tests passed
afterward. The sanitized compiler was rebuilt and the final driver regressions
passed again, including the exact cross-module source location and both entrypoint
and imported FIFO rejection. Logs: `build/hardening-{tests,sanitize,fuzz}.log`,
`build/hardening-format-final.log`, `build/hardening-analysis.log`, and
`build/hardening-driver{,-sanitize}-final.log`.

## 2026-10-26 — resource-owning structs

Struct fields can now own buffers, files, earlier structs and payload enums.
Construction requires every named field in declaration order; whole-value moves,
field replacement, and call-scoped buffer/file descriptor borrows are supported.
Scalar reads and copies of resource-free fields preserve the enclosing owner.
Cleanup releases fields in reverse declaration order, including completed fields
when a later constructor argument fails. Generated aggregate cleanup helpers are
shared instead of expanding nested resource graphs at every exit.

The initial contract deliberately excludes partial field moves, field-by-field
initialization, resource arrays, general struct borrows and exported C layouts.
Borrow alias checks use the whole root binding, including distinct resource fields.
Struct field type nesting is limited to 32 levels. The accepted behavior and
remaining limits are documented in `docs/spec/resource-structs.md`.

Added O0/O2 native allocation and file failure injection, exact cleanup-order
checks, replacement and loop-exit coverage, nested enum/struct cleanup, semantic
rejections, nesting boundary tests and a frontend fuzz seed. The imported example
in `examples/cli/owned_packet/` constructs, transfers and prints an owned payload.

Validation passed: `make test`, `make sanitize` (ASan/UBSan), `make check-format`,
and `make fuzz` (10,000 runs). The final nesting-boundary regression passed in
both normal and sanitized semantic suites. Native resource tests passed at O0/O2,
and `./build/fermi run examples/cli/owned_packet/main.fe` printed `Worker ready.`.
Logs: `build/resource-struct-{tests,sanitize,format,fuzz}.log`. No global compiler
installation was changed.

## 2026-10-26 — file output and native tests

Added exclusive create_new, partial write and explicit sync to std.fs, with a
write_all loop implemented in Fermi. Creation uses O_CREAT/O_EXCL and owner-only
permissions, preserving existing paths. Writes accept short progress, retry EINTR,
and fail on zero progress for nonempty input. The runtime preserves live owners
and reports write/sync failures; earlier output is not rolled back. New fs.write
and fs.sync effects propagate through helpers and reject budgeted file access.
Existing fs.Error discriminants are retained with two variants appended.

Added fermi test (alias t), discovering entry-module test_* functions with no
parameters and bool results. It generates a native harness, reports individual
outcomes and totals, and continues after checked language faults using fresh
fault slots. Imported tests and an application main are not run. Test executables
use the existing shell-free build/launch pipeline and are temporary, without run
cache reuse. Tests share one process and have no timeout or filesystem rollback.

Added a runnable filecopy tool with a bounded 4096-byte transfer buffer and an
8 MiB limit, plus pure-Fermi codec tests. Regressions cover syscall failures,
short writes, no-progress writes, sync errors, primary-error preservation,
resource fields, language faults, cleanup, exact size limits, existing paths,
symlinks, test discovery, defaults, CLI rejection and unavailable native compilers.
Contracts: docs/spec/files.md and docs/spec/test-runner.md.

Validation passed on Android/Termux AArch64: the full normal suite, full ASan/UBSan
suite, formatting checks, and 10,000 frontend fuzz runs. Static analysis of the
file runtime and test-harness emitter reported no findings. A default-directory
test initially exposed a relative FERMI_CC wrapper path in the test script; the
script now resolves that explicit path before changing directory, and the full
sanitizer suite passed afterward. The final regular-file/symlink test also passed
normally and under ASan/UBSan with a controlled, restored umask.

The codec example passed all three native tests. The existing job-queue application
still completed 13 jobs within its 60-unit application budget. Documentation links
were checked. Logs: build/foundation-apps-tests.log,
build/foundation-apps-sanitize-final.log, build/foundation-apps-format-final.log,
build/foundation-apps-fuzz.log, and build/test-command-sanitize-final.log.
No global compiler installation was changed.

## 2026-10-26 — explicit function effect ceilings

Added optional @effects(...) contracts over the existing transitive effect
inference. Empty lists reject every currently tracked effect without demanding a
static work bound. Resource ownership cleanup still contributes; permitted memory
or file effects cannot be hidden behind imported or recursive helper calls.
Unannotated functions retain inference without an explicit ceiling, and unused
permissions do not add inferred effects. @budget retains its stricter rules.

The parser accepts @effects and @budget in either order, rejects duplicates and
unknown effects, and preserves attribute-before-pub syntax. A shared effect-name
registry serves parsing, diagnostics and reporting. Violations use E_EFFECT at
the contract and identify a contributing call when one exists. Export manifests
add allowed_effects, distinct from inferred effects; explain reports both.
No runtime code, capability token, request quota or general purity proof is added.

Added semantic checks for empty/unused ceilings, resources, unknown work,
recursion, attribute failures and budget composition. Native O0/O2 tests exercise
checksum/copy functions and metadata; integration tests reject each tracked effect,
recursive imported helper changes and violations within imported declarations.
The accepted contract is docs/spec/effect-contracts.md; the runnable example is
examples/cli/effect_contracts/main.fe. Generics and function values remain the next
abstraction work rather than being treated as already implemented.

Source review also applied const-qualified AST queries, explicit traversal names,
and multiline Fermi examples. The repository coding guide records these rules
for subsequent changes. Normal semantic and focused O0/O2 integration tests and
formatting passed after the review. Clang static analysis of the final effect
checker reported no findings.

Validation passed: make test, make sanitize, make check-format, and make fuzz
(10,000 frontend runs including the new annotation seed). After the final source
style review, the sanitized compiler and semantic suite were rebuilt; semantic
and focused O0/O2 effect-contract tests passed again with ASan/UBSan. The standalone
example passed both native tests. Logs: build/effect-contracts-tests.log,
build/effect-contracts-sanitize.log, build/effect-contracts-format-final.log,
build/effect-contracts-fuzz.log, and build/effect-contracts-sanitize-style-tests.log.
No global compiler installation or AGENTS.md was changed.

## 2026-10-26 — runnable feature examples

Added examples/features with three multi-module native applications: a five-byte
frame-header validator with an empty tracked-effect ceiling, a sequential job
example transferring an owned buffer through a struct and payload enum, and a
CLI report writer with explicit filesystem effect permissions. The first two
include four and three native test functions respectively. Two intentionally
invalid sources demonstrate hidden helper output and prohibited owner cleanup.
All source and comments use formal English and multiline declarations/branches.

The example guide documents exact commands, output, limits and expected compiler
rejections. No networking, concurrency or proposed syntax is simulated as working.
The report writer handles diagnostics on stderr, refuses an existing destination,
and documents possible partial output on failure.

Validation: sh tests/feature-examples.sh passed at O0/O2, then passed with the
sanitized compiler and ASan/UBSan generated executables. Checks cover native tests,
stdout/stderr, exact file contents, overwrite refusal, argument errors and both
E_EFFECT rejections. make check-format, shell syntax and eight documentation links
passed. The integration script is included in make test and make sanitize.
Logs: build/feature-examples-{tests,sanitize,format}.log. Compiler/runtime semantics,
AGENTS.md and the global compiler installation were not changed.

## 2026-10-26 — finite integer function generics

Added one constrained type parameter, T: integer, and explicit calls such as
larger<u32>(left, right). Each declaration creates nine independent concrete
function bodies and checks every supported integer type, including unused bodies.
Views, local types, arrays and Result success payloads retain their concrete types.
Public generic helpers respect module visibility; C exports and native/test entry
functions use concrete wrappers. Generic resource types and inference are excluded.

The parser binds the parameter while parsing each specialization. Nested >> type
closers now use parser-local pending-token state instead of mutating shared tokens,
so repeated parsing retains the original token stream. Normal call resolution
keys both module/name and the explicit type argument; ordinary calls cannot silently
select a generic instance. Lowering reuses the existing concrete typed pipeline.
Effects, work budgets, ownership and fault checks apply to every specialization.
Explain output labels the concrete type. The existing function limit counts all
nine instances; eager expansion costs and constraints are documented explicitly.

Added the integer_generics example library for scalar ordering and typed-view
search. Native regressions cover every integer width and every u8 value, mutable
and empty views, nested Results, recursive calls, resource parameters, private and
same-named module functions, invalid unused bodies, checked overflow, limits and
malformed declarations/calls. Contracts and decisions are recorded in
spec/integer-generics.md and design/decisions.md.

Validation passed on Android/Termux AArch64: full make test and make sanitize,
O0/O2 native generic suites, formatting, and 10,000 frontend fuzz runs. After the
final naming review, normal semantic and generic integration checks passed again;
the full sanitizer suite compiled and exercised the final parser/checker source.
The example passed three native tests and the larger fixture passed eight. Seventeen
local documentation links were checked. Logs: build/integer-generics-tests.log,
build/integer-generics-sanitize.log, build/integer-generics-format-final.log,
build/integer-generics-fuzz.log, and build/integer-generics-integration-final.log.

Clang static analysis reported no findings in parsing or semantic checking. A scan
of types.c retained three null-precondition warnings in the existing fe_type
constructor's base/error cell calculation; those constructor lines were unchanged
by this work. This validation does not establish general generic ownership,
production readiness or Linux x86-64 acceptance. No global installation or
AGENTS.md was changed.

## 2026-10-26 — signed and unsigned generic domains

Added signed_integer and unsigned_integer constraints alongside integer. They
produce four and five checked specializations rather than nine. Specialization
selection now retains the original declaration for the first admitted type even
when the domain skips u8. Explicit calls outside the domain report E_GENERIC with
the function and rejected concrete type, plus guidance to inspect the constraint.

Added std.bits as a Fermi library with has_all, has_any, set, clear and toggle.
All five helpers preserve their unsigned input width, carry an empty effect
ceiling and a static work budget. Zero-mask behavior is explicit. The module uses
25 function slots under the existing eager specialization strategy; it requires
no native extension or new operator syntax.

The integer_domains example combines signed negation/sign helpers with u16 flags
and high-u64-bit operations. Native tests cover every integer width, unsigned
complements, all u8 values, zero masks, and controlled overflow for negation of
each signed minimum. Source regressions cover invalid domains, incompatible generic
composition, effect preservation and the exact per-domain function-count limits.

Validation passed: the full normal and ASan/UBSan suites, O0/O2 domain tests,
formatting, 10,000 frontend fuzz runs, shell syntax and thirteen documentation
links. A final false-result has_all assertion passed at both optimization levels
and was included in the full sanitizer run. Clang static analysis reported no
findings in the modified parser and checker. The new example passed three native
tests and the domain fixture passed four. Logs:
build/integer-domains-{tests,sanitize,format,fuzz}.log and
build/integer-domains-integration.log. No global compiler installation or
AGENTS.md was changed.


String UTF-8 awal tersedia sebagai tipe str immutable dengan representasi pointer dan panjang byte. Literal bertipe str divalidasi terhadap Unicode scalar encoding; nilai dapat menjadi parameter, hasil fungsi, field struct, dan payload Result tanpa alokasi atau cleanup. Binding var dapat diganti nilainya, sedangkan byte selalu immutable. std.text menambahkan perbandingan isi dan penghitungan Unicode scalar values. Dokumentasi menjelaskan batasnya: tidak ada string dinamis, mutasi byte, normalisasi, atau ekspor ABI string. Contoh examples/features/strings dan fixture semantic menguji literal ASCII, Unicode, string kosong, validasi UTF-8 tepi, Result, field struct, dan aturan mutabilitas. Validasi build, semantic suite, serta integrasi strings O0/O2 lulus.


`use` kini menerima path file lokal dalam tanda kutip, misalnya `use "app/start.fe";`, dengan alias dari stem file atau alias eksplisit `as`. Path diselesaikan relatif terhadap file yang berisi import, sehingga modul bertingkat dapat memakai `../lib/number.fe`. Dotted import lama tetap memakai root entrypoint. Path absolut dan hasil yang keluar dari root proyek setelah normalisasi symlink ditolak. Uji modul mencakup import bertingkat, kedua bentuk alias, batas proyek, dan tetap melewati kasus modul lama.

## 2026-10-27 — string queries and typed ordering

Added std.text.starts_with, ends_with, and compare as Fermi library functions.
Comparison returns the public Order enum rather than an untyped integer.
All three APIs preserve immutable UTF-8 values, perform no allocation, and declare
an empty effect ceiling. Prefix/suffix matching explicitly handles empty patterns
and rejects longer patterns. Comparison uses unsigned UTF-8 byte order without
normalization or locale processing.

The shared matching helper validates its offset using subtraction before indexing,
avoiding overflow in the length guard. Suffix offsets are computed before creating
call-scoped views, following the existing alias rules. The strings example now
imports a local route classifier which demonstrates prefix/suffix dispatch.

Validation passed on Termux AArch64: ten string fixture tests and two example tests
at O0/O2, expected program output, and the same integration suite with generated
executables instrumented by ASan/UBSan. Compiler C sources and runtime interfaces
were unchanged. This validation does not add dynamically owned strings.

## 2026-10-27 — built-in transparent type aliases

Added top-level `type Name = Existing;` and `pub type` declarations to the C
frontend. Alias records are separate from canonical types and resolve immediately
to their target. Alias chains therefore preserve type identity without introducing
runtime representation, recursive lookup, allocation, or cleanup operations.
Module lookup checks alias visibility; declaration checks reject conflicts with
prelude names, nominal types, functions, generic functions, and import aliases.
Public aliases can deliberately expose an existing private nominal type.

Aliases work with scalars, strings, arrays, views, structs, enums, Results,
resources, and concrete generic type arguments. Existing range, ownership,
mutability, effect, and ABI checks operate on the canonical target. The initial
syntax excludes forward, recursive, and generic alias declarations. The project
limit is 1024 aliases, and `type` is now a reserved keyword.

Added a runnable multi-file example, six native feature tests, visibility and
namespace rejections, exact alias-limit checks, semantic rejection cases, and a
frontend fuzz seed. Validation passed on Termux AArch64: full `make test`, full
`make sanitize` with ASan/UBSan, O0/O2 alias integration, formatting, documentation
links, and 10,000 frontend fuzz inputs. The final sanitizer suite includes the
1024/1025 alias-limit regression and generic alias argument case. Logs are
`build/type-alias-{tests,sanitize,format,fuzz}.log`. No global binary was installed
and AGENTS.md was not changed.

## 2026-10-27 — consistency gates for further foundations

Reconciled the candidate feature sequence with the existing native backend/service
focus and foundation-maturity phase. Feature priorities now state concrete workload
requirements, semantic/resource admission rules, compiler/runtime/library boundaries,
and dependency gates for dynamic text, collections, function values, floats, and
native integration. The roadmap and PRD link to these rules; decision D21 records
the policy. This does not implement or approve new syntax. Compiler/runtime code
was unchanged, so no execution suite was repeated for this documentation update.

## 2026-10-27 — call-scoped struct borrows and borrowed-array rejection

Added read/edit struct parameters to the parser, canonical type system, checker,
and C lowering. Shared borrows admit immutable access and repeated readers;
exclusive borrows admit field updates and reborrowing. Stable bindings, nested
fields, and fixed-array elements are accepted. Borrowed descriptors cannot escape
through locals, returns, aggregates, Results, or the public C ABI. Const-qualified
C pointers preserve shared access through nested addresses. Resource fields retain
caller ownership while supporting existing buffer/file borrows and replacement.
Root-based alias analysis remains conservative about disjoint fields and indices.

The array type parser now rejects aliases of views and borrowed descriptors as
element types. Alias wrapping had exposed a lifetime hole in the parameter-only
borrow rule. Semantic regressions cover read/edit views and resource descriptor
aliases as well as the new struct borrows. Editing a root during assignment-value
or index evaluation is also rejected. This extension requires no escaping-lifetime
inference or runtime allocation.

Added a multi-file grouped-record example, seven native borrow tests, resource
replacement/growth and borrowed-fault cleanup checks, negative semantic cases,
and a frontend fuzz seed. The PRD now records concrete program requirements before
claiming general foundation flexibility; dynamic text, generic collections,
function values, and floats remain gaps.

Validation passed on Termux AArch64: full make test, full ASan/UBSan make sanitize,
O0/O2 borrow integration, formatting, 10,000 frontend fuzz inputs, and 91 local
documentation links. Logs are build/struct-borrow-{tests,sanitize,format,fuzz}.log.
No global compiler installation or AGENTS.md was changed.

## 2026-10-27 — capability applications and concrete gaps

Added four runnable native Fermi applications under examples/capability_lab:
number algorithms, breadth-first grid search, a tagged-instruction stack evaluator,
and a streaming stock ledger. The largest application comprises 262 Fermi lines
across five files; all accepted application source totals 539 lines, including a
shared decimal-output helper. The ledger parses runtime file input in 32-byte
chunks and writes a new synchronized report only after input validation succeeds.
Its eight typed inventory slots and 64 KiB input limit are explicit bounds.

Added twelve native application test functions, including 1024 GCD pairs, 576
open-grid paths, and 400 expression evaluations. An independent C fixture generator
models six repeatable random sequences totaling 6144 stock commands. The reports
match byte-for-byte at O0/O2. Negative runtime cases cover truncated syntax, number
limits, invalid bytes, exact/excess input size, missing files, bad argument counts,
and existing-output preservation. Test fixtures use exclusive creation; generated
artifacts are confined to the suite's build directory.

The focused application suite passed with the normal compiler and with the
existing sanitized compiler plus ASan/UBSan-generated executables at both
optimization levels. Formatting and application documentation links passed.
The suite is registered in make test and make sanitize. Compiler/runtime sources
were unchanged; their full suites were not repeated for this application-only work.
Logs: build/capability-lab-{tests,sanitize,format}.log.

The experiment encountered the entrypoint import-root restriction and conservative
constant-true loop return checking. Accepted programs also needed fixed-array
wrappers because views exclude user-defined elements. Six intentional rejection
probes document those flow/view limits plus absent dynamic text retention,
floating-point types, general generics, and non-byte dynamic buffers. The report
in examples/capability_lab/README.md separates absent mechanisms from intentional
safety restrictions. No wrong-answer mismatch or unexpected compiler crash was
observed; the general foundation flexibility gate is still incomplete.

## 2026-10-27 — signed decimal library foundations

Added std.num.parse_i32/parse_i64 and std.fmt.i32_into/i64_into as ordinary Fermi
library functions. No keyword, compiler intrinsic, runtime adapter, implicit cast,
or allocation was introduced. Parsing consumes strict whole-input decimal ASCII,
accepts an optional minus and at most 20 digits, and checks overflow before the
arithmetic operation. Negative accumulation represents the minimum i64 safely.
Formatting determines capacity before writing; insufficient space preserves the
entire destination. Unsigned entry points and existing error enum identities
remain unchanged.

Both operations have static work/zero-heap certificates. Current computed bounds
are 1459/1483 for parsing i64/i32 and 1339/1347 for formatting i64/i32, under
fermi-work-1. These are abstract work estimates, not native execution deadlines.
The new signed_numbers example prints -1250 and tests a minimum-i64 round trip.

Focused validation passed on Termux AArch64 at O0/O2 with normal and existing
ASan/UBSan compilers and instrumented generated C. The independent C harness
checks every i16 value, 10,000 seeded signed values, decimal-power transitions,
all 256 candidate bytes inside numeric input, boundary/error precedence, and
0–24-byte output capacities with sentinel preservation. Two native example tests
and executable output checks pass at both optimization levels. The suite is
registered in make test and make sanitize. Logs are
build/signed-decimal-{tests,sanitize,format}.log. The complete make test suite
also passed, including the capability lab and library-system differential runs;
its log is build/signed-decimal-regression.log. Dynamic text and record collections remain
separate foundation gaps; signed decimal conversion does not resolve them.

## 2026-10-27 — repeated fixed-array initialization

Added `[value; count]` with an explicit expected array type and a literal count.
The count must match the array length; existing 1..65536-cell limits and resource,
Result, and borrow element exclusions remain enforced through aliases. A seed
expression is evaluated once, then copied into independent elements. Named
aggregate seeds retain explicit copy/move requirements. No implicit default
construction, dynamic length, heap allocation, or runtime intrinsic was added.

The typed tree records one seed and a count; C lowering emits a bounded copy
loop. Effect inference visits the seed once, and work analysis charges its cost
once plus the array result cells. Existing comma arrays keep left-to-right
per-element evaluation. Mismatched-length diagnostics report both counts.
The semantic contract is docs/spec/array-initializers.md (decision D23).

Twelve native tests cover scalar extrema, structs with nested arrays, aliases,
static text, payload enums, generic integer specialization, move consumption,
try propagation, 65536 elements, work certificates, and comma-array ordering.
The C ownership harness tracks allocation/release counts for temporary resource
fields, bounds faults, propagated errors, and injected allocation failures.
Additional integration checks validate single I/O effects, forbidden effects,
length diagnostics, and two executable-example tests at O0/O2.

The library-system application now uses repeated empty-record seeds. Removing
69 lines of repeated constructors reduces its current source count to 1300
(1227 nonblank/noncomment lines), still above the 1000-line workload requirement.
A distinct-byte example uses `[false; 256]` and a `[0; 20]` output buffer.

Focused checks passed with a newly built ASan/UBSan compiler, instrumented
semantic tests, and instrumented generated programs at O0/O2. Frontend fuzzing
completed 10000 runs without a detected crash. C formatting and affected local
documentation links passed. The complete make test suite passed, including
both optimization levels of the capability lab and migrated library system. Logs: build/array-repeat-{regression,sanitize-focused,fuzz,format}.log.

## 2026-10-27 — owned runtime UTF-8 and private fields

Implemented contextual `private` struct fields with module-scoped field access
and construction checks. Existing fields remain public and a field literally
named private remains accepted. Imported aliases, nested projections, assignment
targets, and read/edit borrows preserve the defining module boundary. Whole-value
ownership and cleanup are unchanged. Generated C remains a trusted boundary.

Added ordinary Fermi std.text.Text, wrapping a private Buffer<u8>. from_utf8
validates and copies caller bytes, append validates before fallible growth,
length/scalar_length inspect content, and copy_into copies into caller-owned
storage. Resource copying, resource arrays, and escaping borrows remain rejected.
There is no Text-to-static-str conversion or mutable byte exposure. Memory errors
are mapped explicitly; failed growth preserves text and insufficient output
preserves the destination. Existing static str APIs remain available.

The UTF-8 validator has an empty effect ceiling. Read-only Text wrappers do not
allocate, but conservatively carry memory effects due to existing resource-field
projection analysis; no pure/zero-heap certificate is claimed for those wrappers.
The runtime operations add no compiler intrinsic, C runtime adapter, or I/O.
The executable example reads a process argument and prints Hello, café! for the
corresponding input, with formal diagnostics for malformed or oversized names.

Full make test passed on Termux AArch64, including the capability lab and
1300-line library system. Focused O0/O2 checks passed with a newly built
ASan/UBSan compiler, instrumented semantic executable, generated native examples,
and tracked-allocation C harness. Five native text tests, a module-privacy test,
ten rejection cases, and the CLI run are covered. An independent decoder agrees
on all one/two-byte inputs, all encodings U+0000..U+10FFFF (including surrogate
rejection), truncated multibyte encodings, and 10000 seeded byte sequences.
Allocation failure, growth failure, cleanup, output sentinels, and unchanged old
content after rejected append are checked. Frontend fuzzing completed 10000 runs
without a detected crash; formatting and affected local documentation links pass.
Logs: build/owned-text-{regression,sanitize,fuzz,format}.log.

## 2026-10-27 — complete basic owned-text editing

Added ordinary library replace/clear/equal_text/compare_text operations. Replacement
validates before resize and preserves previous text on invalid UTF-8, limit failure,
or allocator refusal. Clear retains capacity and the original byte limit. Comparison
uses the same unsigned byte order as static str without scratch allocation. Existing
error types, static string signatures, resource ownership and private fields remain
unchanged; resource projection effects remain conservative.

Six native text tests and the expanded C harness passed at O0/O2, including 1000
seeded comparison pairs, Unicode ordering, clear/reuse without another allocation,
failed replacement growth, output sentinels, and existing exhaustive UTF-8 checks.
Focused checks passed with the existing matching ASan/UBSan compiler and instrumented
generated programs. The full make test suite passed before typed storage work began.
Evidence: build/text-editing-regression.log, build/text-editing-sanitize.log,
build/text-editing-format.log.

## 2026-10-27 — dynamic integer storage

Extended Buffer elements to the nine existing integer types. std.buffer exposes
finite native allocate/reserve/resize specializations through strictly checked
trusted-module signatures. Parser/type formatting, native signature checks,
standard-module registration and C allocation width selection were updated. The
existing width-aware runtime was reused without changes. std.mem remains byte-only,
including explicit native growth-signature checks after broadening Buffer parsing.

The ordinary generic push helper supplies geometric reservation and an exact-size
fallback when byte/object limits prevent speculative growth. Allocator refusal
preserves the owner without another allocation attempt. Existing moves, descriptor
borrows, scalar views, bounds faults, memory effects and caller cleanup apply.
Record, bool, enum, string and resource elements remain excluded. This does not
implement general generics, arbitrary typed lists, callbacks, or escaping references.
The contract is docs/spec/typed-buffers.md and decision D25.

An executable integer-series workload accepts runtime counts, pushes i64 samples,
and prints their sum. Tests cover all nine scalar widths and extrema, zeroed new
and regrown elements, exact byte limits, overflow before allocation, failed growth,
tracked allocation/release, bounds-fault cleanup, resource structs, source rejection,
module shadowing and formal CLI diagnostics. Native signature tests exercise valid
specializations and reject mismatched return/growth types and nonbyte std.mem APIs.
The former Buffer<u32> limitation probe now checks unsupported record elements.

Focused checks passed at O0/O2 with normal and newly built ASan/UBSan compilers,
including instrumented generated code and semantic checks. Frontend fuzzing passed
10000 runs with the typed seed. Formatting and affected local Markdown links passed.
The complete make test suite passed after the native/type changes and push
policy, including the capability lab and library-system differentials. Pop was
then added as ordinary Fermi, retaining capacity and returning invalid_argument
on empty input. Focused tests passed at O0/O2 with normal/sanitized compilers,
including all-width reverse-order pops, empty input and reuse without allocation.
The final full make test regression, including pop, also passed; its log is
build/typed-buffer-final-regression.log.
Evidence: build/typed-buffer-{regression,sanitize,semantics,fuzz,format}.log.

## 2026-10-27 — UTF-8 search and checked text ranges

Added ordinary Fermi library operations `text.find`, `text.slice_into`, and
`text.truncate`. Byte ranges require Unicode scalar boundaries; rejected ranges
and insufficient output preserve caller storage. Truncation retains capacity
and the owner limit. Search returns the first exact match, handles empty patterns,
and reports absence explicitly. New Error variants are appended to preserve
existing variant values; the greeting example's exhaustive match is updated.

Nine native text tests now include all 169 endpoint pairs around a mixed
one/two/three/four-byte string, output sentinels, missing/empty/repeated patterns,
embedded NUL, invalid truncation, and reuse after shrinking. Existing independent
UTF-8 decoder and allocation-failure checks remain in the suite. The executable
text_ranges example demonstrates extraction and truncation. General record
buffers remain unavailable: safe element initialization, layout and resource
cleanup require a separate design and implementation.

Validation passed: full `make test`, focused O0/O2 text suite with ASan/UBSan,
the executable range example under sanitizers, and `make check-format`.
Evidence is recorded in docs/development/validation.md and
build/text-ranges-{regression,tests,sanitize,format}.log.

## 2026-10-27 — Integer buffer batch operations

Added `buffer.extend<T>` and `buffer.from_slice<T>` as ordinary Fermi library
functions. Extend checks length addition before one exact resize, then appends
all elements. Domain and allocator errors preserve the original buffer.
From-slice creates independent storage with the caller's payload-byte limit;
empty input needs no payload allocation. Existing call-scoped alias rules reject
borrowing the destination as the input to extend.

All nine integer widths exercise batch copying, empty appends, retained capacity,
and source independence through the C harness. Additional tracked-allocation
cases cover exact byte limits, growth failure preserving the old owner, failed
construction, and empty construction without allocation. Native tests cover
source mutation after copying and the zero-limit empty-owner case. The executable
buffer_batches example computes 25 from two batches and is included in the
feature-example suite. The standard module now expands to 63 functions under the
unchanged 128-function cap; general lazy generics are not implemented here.

Validation passed: full `make test`, focused O0/O2 typed-buffer ASan/UBSan suite,
sanitized executable batch example, formatting, and local documentation links.
Logs: build/buffer-batches-{regression,sanitize,format}.log.

## 2026-10-27 — Checked static-string byte indexing

Closed a consistency gap between static str and its existing immutable byte view.
The checker now accepts str[index] as u8, with usize indices and existing bounds
faults. Native lowering preserves const byte addresses while still allowing whole
mutable string fields to be rebound. Assignment to bytes is explicitly rejected
through direct bindings, struct fields, and string arrays. UTF-8 character indexing
and str-producing slices are not introduced.

Added native tests for multibyte values, function results, mutable-field rebinding,
and nested array indexing. A C harness tests every byte and exact/end/maximum
out-of-range indices, including empty text. Unit semantics test accepted access and
three mutation rejections. The string shell suite now instruments its C harness
under sanitizer mode; the new fuzz seed has no standard-module dependency.

Validation passed: complete make test regression, fresh sanitized compiler and
semantics, O0/O2 sanitized string suite, 10,000 fuzz inputs, format and doc links.
Logs: build/string-index-{regression,tests,sanitize,fuzz,format}.log.

## 2026-10-27 — Requested ordinary integer generic instances

Replaced eager ordinary integer expansion with bounded template declarations and
on-request concrete bodies. Templates retain tokens and defining source/module;
each requested body is parsed independently and checked with ordinary symbols,
ownership, effects and work analysis. Requests reuse module/type identity, and
recursion resolves to an already registered concrete function. Duplicate names and
aliases cannot bypass the new template namespace checks. Trusted native buffer
signatures retain their eager finite validation.

This intentionally changes the old full-domain promise: unrequested ordinary
bodies receive syntax/declaration checks, not semantic or effect/work certification.
Added separate tests for 128 templates, 128 concrete functions, the next rejected
instance, deduplication, transitive/private helpers, source context, and recursive
calls. Existing negative tests now instantiate the invalid bodies they exercise.
The same integer-generics example has 18 generic instances with the prior compiler
and three with this compiler; these are function counts, not timing measurements.
Evidence: build/lazy-generics-{before,after}.explain. General non-integer generics,
record collections and lazy native instantiation remain unavailable.

Validation passed: full make test, fresh ASan/UBSan compiler and semantics,
O0/O2 sanitized generic/domain/demand/buffer suites, 10,000 fuzz inputs,
formatting and local documentation links. See docs/development/validation.md and
build/lazy-generics-{regression,sanitize,fuzz,format}.log.

## 2026-10-27 — Inventory workload and shared array slices

Implemented a 430-line Fermi inventory application across seven files, plus 111
lines of native tests. The application decodes bounded journals, validates UTF-8
names, applies stock/price changes to 64 fixed records, checks invariants after
every command, and saves sorted snapshots that can be read back as journals.
Separate fixtures use a JavaScript Map model for 6,144 reference commands per
optimization mode; reference code does not run in the application executable.

The workload exposed a real compiler composition bug: array slices under shared
struct borrows were typed as edit views, causing generated C to discard const.
A six-line minimal reproduction failed before the fix; replacing read with edit,
or removing the slice, passed. The checker now preserves shared access through
field/index/slice projections. Native regressions cover direct, nested and
array-element fields, and retained edit behavior. Unit tests preserve read-to-edit
rejection. The original inventory workload then compiled and matched its snapshot.

The user's syntax-design steering is captured in docs/design/syntax-candidates.md:
limit, access and acquire remain proposals, with admission gates and explicit
non-claims. No new grammar or global novelty claim was introduced. Fixed record
capacity, fixed-size names and lack of atomic snapshot replacement remain actual
application/language boundaries rather than being hidden by the example.

Validation completed: full make test; fresh sanitized compiler/semantics and
O0/O2 struct-borrow/inventory suites; 10,000 fuzz inputs; format and local links.
Evidence is recorded in docs/development/validation.md and
build/inventory-{regression,tests,sanitize,fuzz,format}.log.

## 2026-10-27 — Data generics and dynamic record foundations

Implemented T: data for nonresource integer/bool/str/record/enum/fixed-array values,
call-scoped data views and scalar copy places. Ordinary instance checking preserves
privacy, explicit aggregate transfers, canonical nominal identity and alias rules.
The std.slice library supplies copy_into/fill/swap with failure-before-mutation
checks. Data views stay outside the exported C ABI. Explain output uses qualified
argument type names and array shapes.

Implemented Buffer<data> storage through typed construction: empty, element_size,
resize_filled and data reserve, with generalized push/pop/extend/from_slice.
Integer allocate/resize retain zero initialization. Record construction never
publishes raw zeroed bytes. Runtime positive widths, alignment, multiplication,
PTRDIFF_MAX and byte limits are tested, including odd-sized records and payload
enums. Buffer owners retain normal cleanup; resource-containing elements remain
rejected. The new executable record example pushes/pops 2,048 records and sums
their IDs to 2,096,128.

The new constant native element_size exposed a budget-analyzer null-body crash.
ASan/UBSan localized it to statement_cost called on a native declaration. Native
cost analysis now recognizes the checked constant size primitive as one abstract
unit and treats other native calls as unknown, instead of analyzing absent bodies.
Native signature tests and the scalar query exercise this regression.

An initial full regression reached the capability lab and failed because its old
record-view limitation became supported. The probes now check resource-containing
views/containers, and the final full make test passed. The earlier failed log is
build/data-generics-regression.log; final evidence is
build/record-buffers-regression.log. All remaining foundation stages are tracked
in docs/development/foundation-program.md and are not claimed complete.

## 2026-10-27 — Fermi-written bootstrap lexer

Added bootstrap/frontend lexer and offset-based token records, a chunked regular
file reader, an owning token collector and a diagnostic CLI, all written in Fermi.
The lexer follows the host C lexer's token codes, byte locations, decimal u64,
comments, escape validation and resource limits. Private state is terminal after
failure; the owning collector retains a lexical error's valid prefix and releases
partial storage on allocation failure. Strings retain raw spans and decoded byte
lengths; literal decoding/UTF-8 semantics remain a future parser responsibility.

The native CLI tokenizes its own source. This is a lexer workload, not a parser,
code generator or self-hosting compiler. The production compiler remains C.
Bootstrap contracts and the next flat-AST parser gate are in bootstrap/README.md.
Tests are wired into make test and make sanitize; the differential harness also
injects every token allocation failure and verifies cleanup. General foundation
stages 3–8 remain pending and are not superseded by this workload.

Validation passed: full make test; focused ASan/UBSan O0/O2 bootstrap suite;
format and affected local links. Each mode compared 70,754 inputs against the
host lexer. Logs and limits are recorded in docs/development/validation.md.

## 2026-10-27 — Bootstrap AST parser and camelCase names

Implemented a Fermi-written parser for plain functions, bare type references,
blocks, typed local bindings, assignments, returns and core expressions/calls.
The AST uses one-based typed IDs, private owning node storage and checked reads;
source spans and terminal EOF are validated before recursive parsing. Syntax
failures retain partial nodes and an exact error token; allocation failures
release the full partial owner. Explicit depth, chain, node, parameter and
function limits have runnable rejection cases. The arithmetic example builds a
35-node tree and compiles natively with the production compiler to exit status 39.
Control flow, imports/nominal declarations and semantic analysis are not ported.

A native reproduction exposed the host parser's unary threshold of 7: -a + b
incorrectly grouped as -(a + b). The pre-fix test failed, and threshold 10 now
passes six native precedence regressions. Parentheses preserve the former
explicit grouping. The bootstrap parser follows the corrected host rule.

Applied the requested concise camelCase convention to bootstrap functions,
fields, variants and multiword files. AST children now use lhs/rhs/body instead
of single-letter fields. Existing standard-library interfaces remain compatible.
Native test discovery recognizes testName and test_* using a shared predicate;
generic test rejection, imported-test exclusion and fault continuation are tested.
Naming rules are recorded in docs/development/naming.md. Existing AGENTS.md is
unchanged. The parser contract and remaining gates are in bootstrap/PARSER.md.

Validation passed: full make test; fresh ASan/UBSan compiler/semantics and O0/O2
parser/lexer/test-command suites; 10,000 frontend fuzz inputs; format, scripts and
local links. The parser compared 1,446 trees and the lexer 70,760 inputs per mode.
Evidence and the unary migration note are in docs/development/validation.md.

## 2026-10-27 — Bootstrap control-flow grammar

Extended the Fermi-written parser with parenthesized if/else-if/else, while,
three-part for headers, break and continue. Simple statements share one parser
between ordinary statements and for headers. A for step cannot declare a binding.
The AST has distinct other/init/step IDs, preserving sibling links and owner
stability during buffer growth. Enclosing-loop and condition-type validation
remain semantic work; the bootstrap parser does not claim to perform those checks.

The normalized host-tree harness now covers statements and all control-flow
links, including malformed headers, else branches, 64 nesting configurations and
partial cleanup. Existing expression comparisons are retained. The native
controlFlow.fe example parses to 49 nodes and exercises branch selection,
continue, break and while/for; its expected native exit status is 19.
No production compiler semantics or existing AGENTS.md were changed.

Validation passed: full make test, O0/O2 parser references and native examples,
focused ASan/UBSan parser suite, formatting, script syntax and local links.
Evidence and scope are recorded in docs/development/validation.md.

## 2026-10-27 — Dispatch backend workload

Added dispatch/, an executable Fermi job-broker simulation with separate domain,
protocol, storage and runner modules. Dynamic record buffers store tenants,
workers and jobs; an audit ring retains 256 events. Implements quota/backlog
admission, priority/age/ID ordering, fenced leases, exponential retries,
dead-letter reopening, TTL precedence, dependency cancellation, pause/drain,
idempotent submission, scenario assertions and queue gauges. State is checked
after every accepted command. Parent indices are cached and validated against
immutable IDs, avoiding repeated dependency-history searches during validation.

The CLI reads a bounded ASCII journal and publishes a byte-preserving replay
checkpoint and deterministic report using exclusive creation, sync and close.
Reports/checkpoints are separate publications with documented partial-I/O
semantics. Workers and time are simulated by commands; no socket or payload
executor is claimed. All application logic is Fermi; Node is test-only.

There are 14 Fermi files and 1,225 lines including native tests. The independent
Map/sort-based reference checks full reports for three 1,024-job workloads,
checkpoint replay, all 64 selected input boundaries and domain/I/O failures.
O0/O2 and instrumented generated programs passed. The demo runs with
make -C dispatch demo and writes fresh artifacts under dispatch/build/demo-*.
Compiler C sources and existing AGENTS.md were unchanged.

### Portable build layout

- Split regression/sanitizer and installation rules into `mk/tests.mk` and
  `mk/install.mk`; retained existing development commands and output paths.
- Added configurable host flags, incremental configuration tracking, unit-check
  and help targets, prefix-bound installation, and DESTDIR package staging.
- Added Linux x86-64/AArch64 Clang/GCC CI and an experimental macOS compiler job.
  Hosted results are pending; Android AArch64 remains the locally verified host.
- Adapted Darwin stat timestamp fields. Kept Linux-only file descriptor semantics
  and the 64-bit ABI restriction explicit instead of declaring untested support.
- Removed historical generated build output and the empty initial `src/` scaffold.

### String allocation hardening and reusable buffers

- Guarded compiler string lengths before adding a terminator; pathological
  lengths now produce E_LIMIT before allocation or source access. Empty copies
  avoid a null-pointer zero-byte memcpy.
- Added generic `std.buffer.truncate` and `clear` helpers for resource-free data.
  Both retain capacity; truncation rejects growth without modifying storage.
- Added scalar, record, empty-buffer, reinitialization, acquisition-failure and
  allocation-count tests at O0/O2, plus a sixteen-batch executable example.
- Included support-unit sanitizer coverage in `make sanitize` and buffer reuse
  checks in both ordinary and sanitizer regression targets.

### Native buffer truncation and bounded reuse

- Added a checked runtime truncate operation that only updates logical length.
  It rejects growth and vacant owners without reading payload storage, copying a
  seed, or calling an allocator. Capacity and retained bytes stay unchanged.
- Promoted `std.buffer.truncate<T: data>` to a validated standard native; `clear`
  delegates to it, and `pop` uses it after copying the returned element.
- Classified truncate as an effect-free metadata primitive costing one abstract
  native work unit. Borrowed clear/pop helpers now admit checked zero-heap budgets;
  creating/dropping owners remains a memory effect.
- Added runtime lifecycle and invalid-signature cases, budgeted reuse tests, and
  a budgeted batch reset example. Existing Result signatures remain compatible.
