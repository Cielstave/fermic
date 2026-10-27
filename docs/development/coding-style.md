# Gaya kode C Fermi

Kode Fermi harus mudah diperiksa oleh orang yang baru masuk ke compiler ini.
Keterbacaan dinilai dari maksud nama, alur data, dan kemudahan memeriksa invariant,
bukan dari banyaknya komentar atau kemiripan dengan satu proyek besar.

## Dasar keputusan

- [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html) menekankan nama
  yang sesuai peran dan early return untuk mengurangi konteks yang harus diingat
  pembaca. Fermi menerapkannya pada pemeriksaan tipe dan pengenalan loop terbatas.
- [PEP 7 untuk implementasi C CPython](https://peps.python.org/pep-0007/) memakai
  indentasi empat spasi, prototype lengkap, dan braces pada kode baru. Fermi tetap
  menggunakan C11 dan mempertahankan batas 100 kolom yang sudah ada di repository.
- [Linux kernel coding style](https://docs.kernel.org/process/coding-style.html)
  membedakan nama lokal pendek dari nama antarmuka deskriptif, serta menyarankan
  fungsi fokus dan komentar yang memberi alasan. Fermi tidak mengambil aturan tab
  atau seluruh kebijakan typedef kernel karena konteks proyeknya berbeda.

Pembacaan sumber juga mencakup [state lexer CPython](https://github.com/python/cpython/blob/main/Parser/lexer/state.c)
dan [parser statement Clang](https://github.com/llvm/llvm-project/blob/main/clang/lib/Parse/ParseStmt.cpp):
state dan kegagalan dibaca dekat dengan operasi terkait; cabang grammar besar
boleh tetap berbentuk dispatch ketika pemecahan tambahan justru mengaburkan alur.
Tidak ada implementasi dari sumber tersebut yang disalin ke Fermi.

## Aturan repository

- Gunakan `snake_case` untuk fungsi/variabel, `FeType` untuk tipe lintas modul,
  dan prefix `fe_` untuk fungsi yang diekspor compiler. Helper satu file `static`.
- Pilih nama domain: `scan_integer`, `parse_expression`, `storage_owner`,
  `counting_loop_counter`. Hindari nama seperti `process_data` tanpa objek jelas.
- `i`, `left`, `right`, dan `count` sesuai untuk lingkup pendek. State panjang
  memakai `parser`, `checker`, `context`, atau `emitter`, bukan huruf tunggal.
- Satu statement per baris. Semua kontrol memakai braces. Pisahkan tahap logis
  dengan baris kosong; jangan menyembunyikan percabangan dalam ternary bertingkat.
- Pisahkan helper ketika ada konsep yang bisa diberi nama, bukan sekadar mengejar
  jumlah baris. Tidak membuat interface generik untuk satu kebutuhan lokal.
- Komentar menjelaskan invariant, alasan penolakan, atau keterbatasan pembuktian.
  Hindari komentar yang hanya membacakan assignment berikutnya.
- Diagnostic harus menyebut pelanggaran yang bisa diperbaiki pengguna. Tes memakai
  perilaku nyata dan regresi; jangan hanya menguji bentuk implementasi internal.
- Gunakan `make format` dan `make check-format`. Perubahan semantik tetap harus
  melewati `make test`; formatter tidak menilai desain atau membuktikan keamanan.

Contoh nyata: pengenal loop membedakan pemeriksaan initializer, condition, step,
dan perubahan variabel dalam helper `counting_loop_counter`. Pembaca bisa menilai
mengapa loop diterima tanpa membongkar satu ekspresi boolean yang panjang.

## Bahasa source code dan diagnostik

Identifier, komentar source, dan pesan yang dikeluarkan compiler menggunakan
bahasa Inggris teknis yang formal. Komunikasi pengembangan dan dokumentasi yang
sudah ada tidak perlu diterjemahkan hanya untuk memenuhi aturan source code ini.
Nama menyatakan tanggung jawab; komentar menjelaskan invariant dan alasan desain.
Hindari bahasa percakapan, humor, klaim pemasaran, atau pernyataan kesiapan produksi
yang tidak memiliki bukti pengujian.

Untuk pesan diagnostik baru atau yang diubah:

- Nyatakan pelanggaran spesifik dan identifikasi binding atau tipe terkait.
- Pada ketidakcocokan tipe, tampilkan expected dan actual, termasuk panjang array.
- Bedakan transfer ownership yang pasti terjadi dari kemungkinan pada suatu jalur.
- Pisahkan pesan utama, lokasi penyebab, dan petunjuk perbaikan. Jangan menyarankan
  konversi numerik untuk kesalahan yang tidak melibatkan tipe integer.
- Pertahankan kode diagnostik untuk integrasi tooling. Gunakan huruf kecil di awal
  pesan dan tanpa titik penutup; identifier mempertahankan kapitalisasi aslinya.
- Uji pesan pada kegagalan program yang nyata, bukan hanya pemanggilan renderer.

Dasar riset: [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html#error-and-warning-messages)
menekankan bahasa Inggris yang ringkas dan benar;
[GCC Guidelines for Diagnostics](https://gcc.gnu.org/onlinedocs/gccint/Guidelines-for-Diagnostics.html)
menekankan konteks kode pengguna dan tindakan perbaikan;
[Rust Compiler Development Guide](https://rustc-dev-guide.rust-lang.org/diagnostics.html)
memisahkan pesan utama, lokasi sumber, dan subdiagnostik. Aturan di atas merupakan
kebijakan Fermi; tidak mengadopsi seluruh konvensi atau implementasi proyek tersebut.


## Repository-wide source policy

This policy applies to compiler and runtime C, public headers, tests, scripts,
and Fermi library/application examples. Existing Indonesian documentation may
remain Indonesian. Source identifiers, comments, diagnostic messages, and log
fields use formal technical English.

Fermi's style is defined by explicit ownership, bounded operations, local failure
handling, and domain-specific names. Keep those properties visible in code.
Do not introduce arbitrary stylistic variation to imply human authorship.

- Name helpers after the operation and representation: write_json_string,
  write_source_excerpt, reserve_capacity, or check_assignment. Short names remain
  appropriate for small loop counters and established compiler terminology.
- Keep validation near the operation it protects. Make ownership transfer and
  cleanup visible; document whether failure preserves or consumes an input.
- Use comments for invariants, ordering constraints, exceptional cases, and
  external contracts. Do not narrate assignments or add decorative section banners.
- Keep errors factual. State the failed operation or violated requirement, identify
  the relevant object, and include expected/actual values when available. Separate
  corrective help from the primary message. Avoid apologies, jokes, exclamation
  marks, conversational prompts, and unsupported claims.
- Keep diagnostic codes and machine-readable fields stable. Preserve source spans
  and byte escaping. Never expose credentials or untrusted terminal control bytes.
- Name log events by operation and outcome. Attach bounded structured fields;
  do not serialize entire request objects or use private errors as public messages.
- Preserve established formatting through .clang-format. Do not reformat unrelated
  code or add abstraction solely to make a patch appear more elaborate.
- Mark design-only .fe sources explicitly. Proposed library contracts must not be
  described as compiler guarantees or validated runtime behavior.

The referenced [CPython Python-ast.c](https://github.com/python/cpython/blob/main/Python/Python-ast.c)
is generated by Parser/asdl_c.py, as its header states. Its regular structure is
useful to study, but generated repetition is not a template for handwritten Fermi
modules. Retain Fermi's own names, interfaces, formatting, and error conventions.

Review style changes separately from semantic changes where practical. For a
helper rename, compile the affected module and run its existing behavioral tests;
for diagnostic wording changes, verify the actual failing source and both text
and JSON output. Formatting alone is not evidence of correctness.

## Penulisan perubahan compiler berikutnya

- Helper yang hanya membaca AST menerima pointer `const`; mutasi state harus
  terlihat dari antarmukanya. Gunakan nama seperti `function`, `statement`, dan
  `expression` pada traversal yang panjang.
- Nama helper pencarian menjelaskan hasil dan ruang pencariannya, misalnya
  `find_effect_call`. Jangan menambah singkatan atau lapisan wrapper tanpa alasan.
- Registry nama publik, flag, dan representasi laporan memakai satu sumber data
  agar parser, diagnostik, serta metadata tidak berbeda penafsiran.
- Tulis badan fungsi dan cabang contoh Fermi dalam beberapa baris. Pisahkan tahap
  validasi, analisis, dan pelaporan dengan blok yang mudah diperiksa.
- Komentar dan diagnostik tetap berbahasa Inggris formal. Komentar menjelaskan
  invariant atau alasan implementasi; detail kontrak publik ditempatkan dalam
  spesifikasi. Jangan menyebut sifat keamanan yang belum dibuktikan.
