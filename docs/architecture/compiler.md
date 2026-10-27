# Pipeline compiler pilot

Compiler menggunakan C11; driver memakai POSIX untuk proses dan file sementara.
Tidak ada interpreter atau dependency package-manager. Backend awal menghasilkan
C lalu memanggil Clang sebagai proses dengan argv terpisah.

```text
source .fe
  -> project/module loading (lex imports, resolve dependencies, parse units)
  -> lexer + source spans (per unit)
  -> syntax tree
  -> name/type/ownership checking
  -> typed structured tree + resource bounds + proven arithmetic ranges
  -> stable owner slots + sequenced C temporaries + cleanup/fault propagation
  -> native object + C header + contract manifest
```

## Tanggung jawab modul

- `compiler/support`: arena per kompilasi, diagnostic pertama, hash SHA-256.
- `compiler/frontend`: tokenisasi, recursive-descent parser, interning tipe.
- `compiler/analysis`: pemeriksaan tipe/flow/view, batas kerja konservatif, dan
  [pembuktian overflow untuk reduksi terbatas](../spec/range-proofs.md).
- `compiler/ir`: perencanaan slot owner dan lowering expression menjadi temporary
  berurutan, transfer owner, serta cleanup expression/scope/fungsi.
- `compiler/backend/c`: kontrol statement, tipe C, wrapper export, header, manifest.
- `compiler/driver`: CLI, staging artefak, dan eksekusi toolchain tanpa shell.

Node syntax memperoleh tipe dan binding/function ID selama checking. Pilot memakai
pohon terstruktur bertipe tersebut sebagai representasi antara; belum ada SSA atau
instruction graph terpisah. Nama sumber tidak ditempel menjadi expression C.
Temporary memastikan argument dan operasi efek samping mengikuti urutan Fermi.

Semua memori frontend berada dalam context arena. Parser/checker menghentikan
kompilasi pertama yang gagal melalui batas `setjmp` lokal; tidak ada objek heap
terpisah yang harus dilepas saat `longjmp`. Driver selalu menghancurkan context.
Tidak ada `longjmp` pada program Fermi hasil kompilasi.

State binding membedakan initialized, moved, uninitialized, dan mungkin berisi
nilai setelah join cabang. State terakhir penting: Result yang ditangani hanya
pada satu cabang tetap tidak boleh hilang ketika scope berakhir.

Fault internal dibawa sebagai parameter context dalam fungsi C privat. Setiap
caller memeriksa status setelah call; wrapper mengubahnya menjadi return struct.
Runtime buffer menyediakan alokasi fallible dan release tanpa mutable state global.
Tidak ada abort untuk fault bahasa. Buffer dan Result resource memakai slot stabil
pada scope fungsi; descriptor kosong dan tag Result menentukan kewajiban cleanup.
Pohon bertipe tetap menjadi IR, dengan metadata slot owner; belum ada CFG/SSA baru.
Nilai initial zero pada storage C bukan izin membaca binding Fermi yang belum
initialized; checker tetap menolak pembacaan tersebut.

## Batas kepercayaan

Sumber `.fe` tidak dipercaya: batas memori/nesting, literal overflow checks, dan
fuzz target melindungi compiler dari input rusak. Objek native bergantung pada
kebenaran compiler Fermi, compiler C, dan OS. Buffer host C memiliki kewajiban
allocation/lifetime/race yang tidak dapat dibuktikan hanya dari pointer dan length.

Model work dan ABI dijelaskan terpisah di `docs/spec/`. Mengubah biaya, layout,
atau hak akses membutuhkan pembaruan spesifikasi dan tes; perubahan formatter
saja tidak boleh mengubah kontrak.


## Entry points and stage invariants

The following maps the current implementation, not a proposed SSA pipeline.
Module loading invokes lexing/parsing per unit; it is not an independent complete
pass that precedes all lexing. The diagram above summarizes responsibilities.

| Stage | Current entry or module | Required output |
| --- | --- | --- |
| Project loading | fe_load_project, driver/modules.c | Bounded acyclic module graph and parsed declarations |
| Syntax | fe_lex, fe_parse_module | Tokens/nodes with source spans |
| Semantics | fe_check, analysis/check.c | Resolved types, bindings, ownership and access checks |
| Analysis | fe_analyze, analysis/budget.c | Effects, work bounds and supported range proofs |
| Owner planning | fe_plan_owners, ir/ownership.c | Stable resource temporary slots and cleanup obligations |
| C lowering | ir/lower.c, backend/c | Sequenced expressions, fault edges and resource cleanup |
| Native build | driver/build.c, executable.c, toolchain.c | Native artifacts or a diagnostic; prior output preserved on failure |

## Planned evolution

Descriptor borrowing uses explicit TY_BUFFER_EDIT type/access identity and pointer
lowering, distinct from scalar-view layout. The [integration design](../design/buffer-borrows.md)
records checking, conservative analysis, cleanup exclusions and ABI restrictions.
The current tree is retained for this bounded extension.

Before adding escaping borrows, suspension or partial resource moves, evaluate a
control-flow IR with explicit storage, transfer, borrow, call and cleanup operations.
Specify a verifier for operand types, initialized storage, borrow boundaries and
valid exits. Compare observable behavior against the existing backend on accepted
programs before migrating. No SSA conversion or LLVM backend is scheduled merely
to resemble another compiler repository.

The [research review](../research/2026-10-25-backend-design.md) separates upstream
observations from these Fermi-specific decisions. Native adapters and generated C
remain part of the trusted implementation, not independent proof of source safety.
