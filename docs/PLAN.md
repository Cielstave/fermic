# Pengembangan Fermi untuk backend dan layanan infrastruktur

Arahan terbaru: kerjakan seluruh fondasi pemrograman umum secara bertahap.
Status implementasi dan sisa pekerjaan dilacak di
[foundation program](development/foundation-program.md). Generics data dan
buffer record nonresource sudah diterapkan; container resource, callback,
error berpayload, float, filesystem lanjutan, dan FFI C masih pekerjaan berikutnya.

Arah aktif, disepakati 24 Oktober 2026:

> Fermi adalah bahasa native untuk membangun backend, background services, dan
> infrastruktur berperforma tinggi, dengan keamanan memori dan kontrol resource.

Sasaran awal: API server, background worker, daemon, dan layanan jaringan.
Bahasa tetap cukup umum untuk library serta alat pendukungnya. Compiler memakai
C11, sumber `.fe`, backend awal C/Clang, dan ownership tanpa GC sebagai arah
pengelolaan memori. Sintaks familiar dengan tipe eksplisit, `let`/`var`, serta
`read`/`edit` dipertahankan; perubahan membutuhkan catatan migrasi.

Linux x86-64 menjadi target penerimaan layanan pertama. Termux AArch64 tetap
lingkungan pengembangan dan pengujian. Linux belum diverifikasi dalam sesi ini.
Kode, komentar, dan diagnostik compiler menggunakan bahasa Inggris profesional.
`AGENTS.md` tidak diubah.

## Bootstrap frontend

Lexer, pembaca sumber, dan kolektor token awal sudah ditulis dalam Fermi.
Lihat [kontrak dan tahap berikutnya](../bootstrap/README.md). Parser AST subset
awal juga tersedia; lihat [kontrak parser](../bootstrap/PARSER.md).
Ini belum compiler self-hosting; fondasi yang masih pending tetap dilacak terpisah.

## Prioritas aktif: pematangan dasar

Arahan 26 Oktober 2026: matangkan `if`/`else`, `for`, `while`, `match`,
fungsi, dan tipe yang sudah tersedia sebelum memperluas fitur bahasa. Setiap
perubahan harus menjaga sintaks bertanda kurung, kondisi `bool`, tipe eksplisit,
evaluasi kiri ke kanan, dan aturan mutabilitas yang konsisten.

Gerbang penerimaan mencakup eksekusi native pada `-O0`/`-O2`, penolakan program
yang tidak valid, penggabungan status inisialisasi antarcabang, scope loop,
jalur return, short-circuit, dan kegagalan aritmetika tanpa undefined behavior.
`break`/`continue` tersedia dengan cleanup scope dan pemeriksaan back edge loop.
`match` mendukung Result, enum dengan/tanpa payload, integer, dan bool. Pola
skalar literal dan wildcard final bersifat exhaustif; lihat [scalar matches](spec/scalar-matches.md).
Range, guard, dan fallthrough belum tersedia untuk pola skalar.
Networking, concurrency, dan fitur lanjutan tidak menjadi pekerjaan aktif tahap ini.

## Dokumen pengarah

[PRD](product/requirements.md) menentukan kebutuhan dan aplikasi penerimaan;
[indeks spesifikasi](spec/README.md) menentukan kontrak yang tersedia. Lihat
[peta dokumen](README.md), [arsitektur runtime](architecture/runtime.md), dan
[register keputusan](design/decisions.md) sebelum memperluas fitur.
Proposal tidak mengubah status compiler sampai implementasi dan pengujiannya selesai.

## Status yang sudah tersedia

- Compiler native eksperimental, executable, modul lokal, visibilitas `pub`, C ABI.
- Integer signed/unsigned 8/16/32/64-bit, `usize`, array tetap, struct, enum tanpa payload,
  Result, pemeriksaan
  mutabilitas, move/copy, dan peminjaman view terbatas pada pemanggilan.
- Checked arithmetic dan bounds, diagnostik lokasi sumber serta JSON.
- Operasi bit unsigned dengan shift terperiksa serta `std.binary` untuk field u32
  big/little-endian, tanpa alokasi dan dengan work budget statis.
- Enum berpayload, match lengkap, move/copy, dan cleanup buffer/file per varian aktif.
- Integer/bool match dengan coverage, duplicate checking, typed literal, dan wildcard final.
- UTF-8 `str` statis serta helper teks; string dinamis masih direncanakan.
- Alias tipe transparan `type Name = Existing;` dengan visibilitas modul.
- Borrow `read Struct`/`edit Struct` selama pemanggilan tanpa transfer owner.
- Struct pemilik resource dengan konstruksi lengkap, move utuh, penggantian field,
  borrow buffer/file melalui field, dan cleanup saat konstruksi gagal.
- Propagasi error `try(expression)` dengan tipe error identik dan cleanup resource.
- Stdin/stdout byte melalui `std.io`, dengan efek I/O transitif.
- `Buffer<u8>` dengan alokasi/copy/growth fallible, descriptor borrow eksklusif,
  move, view, Result, cleanup otomatis, serta efek memori transitif.
- Analisis work budget dan pembuktian khusus reduksi `i16` ke `i64`.
- Kontrak `@effects(...)` sebagai batas efek transitif per fungsi, termasuk cleanup;
  independen dari kewajiban work bound statis pada `@budget`.
- Tes O0/O2, sanitizer, fuzzing, serta benchmark decoding/filtering/penjumlahan.

Lihat [spesifikasi pilot](spec/pilot.md), [I/O](spec/io.md), dan
[pembuktian rentang](spec/range-proofs.md). Networking, runtime tugas, dan
server HTTP belum tersedia. Status ini bukan klaim siap produksi.

## Prinsip pengembangan

Keamanan memori, penanganan kegagalan eksplisit, dan lifecycle resource mendahului
penambahan concurrency. API alokasi dan I/O harus fallible. Antrean, koneksi,
request, serta pekerjaan aktif harus bisa dibatasi; overload tidak boleh memicu
pertumbuhan memori tanpa batas.

`@budget` bersifat opsional untuk komponen yang dapat dianalisis. Bound unknown
pada layanan normal diperbolehkan; anotasi tetap ditolak jika kontraknya tidak
terbukti. Work abstrak tidak menjamin latensi nyata atau native stack.

HTTP, TLS, JSON, database, dan integrasi cloud menjadi library. Fondasi bahasa
menangani tipe, ownership, error, dan aturan concurrency. Mekanisme scheduling
serta I/O asinkron membutuhkan rancangan terpisah sebelum sintaks dibekukan.

Urutan kandidat fitur tunduk pada [aturan penerimaan dan dependensi fondasi](design/feature-priorities.md#feature-admission-rules). Daftar fitur untuk fleksibilitas bukan komitmen urutan rilis. String dinamis, koleksi, callback, float, dan FFI harus memiliki kontrak tipe, ownership, kegagalan, efek, dan lowering sebelum diimplementasikan.

## Urutan implementasi dan gerbang penerimaan

### 1. Ownership resource dan alokasi

- Tambahkan operasi IR eksplisit untuk transfer dan cleanup, dengan owner tunggal.
- Implementasikan buffer scalar dengan alokasi/pertumbuhan fallible; nama
  `Buffer<T>` adalah arah API resource; dukungan fungsi generik integer tidak
  membuka elemen Buffer selain u8.
- Pelepasan tepat sekali pada scope exit, return, dan fault. Pertumbuhan gagal
  mempertahankan owner/data lama; view tidak boleh bertahan melewati realokasi.
- Larang copy resource implisit, use-after-move, double-free, dan dangling borrow.

**Gerbang:** fault injection setiap titik alokasi, tes alias/lifetime, dan
ASan/UBSan lulus sebelum resource file atau socket diperkenalkan.
**Kemajuan:** [aturan lifecycle dan rancangan IR](design/resource-ownership.md)
sudah ditulis. Runtime buffer scalar C terpisah tersedia dengan create, reserve,
resize, move, drop, batas kapasitas, dan tes fault injection. Runtime create/drop
kini dipakai compiler untuk [Buffer<u8>](spec/buffers.md),
dengan alokasi fallible, slot owner temporer stabil, transfer Result/argumen,
cleanup scope/return/fault, serta tes allocator failure pada generated-program.
Reserve/resize kini tersedia melalui parameter `edit Buffer<u8>` yang meminjam
descriptor secara eksklusif. Scalar selain u8 belum dibuka pada API `.fe`.
Backend kini memiliki [epilogue tunggal](design/function-exits.md) untuk return,
fault, dan native I/O, dengan slot return pada scope fungsi. Tes generated-program
memeriksa setiap fungsi aktif melewati epilogue tepat sekali. Integrasi buffer
kini menambahkan pelepasan resource pada jalur tersebut.
`std.mem.from_bytes` kini menyalin read view ke buffer independen melalui implementasi
Fermi. Tes mencakup seluruh nilai byte, input kosong, limit, kegagalan alokasi,
masa hidup hasil setelah sumber dilepas, dan cleanup setelah fault.

Audit fondasi memperbaiki celah penggunaan owner di batas slice argumen lain;
[catatan audit](development/foundation-audit.md) menjelaskan reproduksi dan
regresinya. Suite normal, sanitizer, format, serta 10.000 input fuzz lulus.

**Kemajuan growth:** [integrasi descriptor](design/buffer-borrows.md) membedakan
pointer descriptor dari view elemen, mendukung reborrow tanpa transfer owner,
dan menolak escape, konflik alias, serta realokasi dalam indeks/assignment. Tes
generated-program mencakup pertumbuhan gagal, shrink/regrow, fault, loop, transfer
hasil dan peak payload lama+baru. Gate byte-buffer sudah diimplementasikan;
resource nesting dan tipe scalar lebih luas tetap dibatasi.

**Kemajuan tahap 2:** [konfigurasi startup](spec/configuration.md) kini menyediakan
environment read berbatas, parser decimal/range bersertifikat, dan diagnostik stderr.
Contoh startup memvalidasi batas payload aktif tanpa mencetak nilai mentah.

**Kemajuan CLI:** [argumen proses](spec/process-arguments.md) dan
[formatting decimal](spec/formatting.md) kini tersedia.
[Bytecount](../examples/tools/bytecount/README.md) adalah aplikasi native Fermi
yang memvalidasi argumen, membaca input beberapa chunk dalam buffer tetap,
menolak pelampauan limit, dan mencetak hasil numerik. Library mode belum memiliki
ABI untuk inisialisasi konteks argumen host.

**Kemajuan waktu:** [std.time](spec/time.md) menyediakan clock monotonic dengan
konversi nanosecond terperiksa serta helper deadline murni. Deadline belum dapat
membatalkan blocking I/O; penegakannya membutuhkan integrasi runtime berikutnya.

**Kemajuan runtime file:** [fondasi read-only](design/file-resources.md) tersedia
di C dan compiler dengan move, read berbatas, consuming close dan fallback drop.
`std.fs.File` kini tersedia melalui alias `fs.File`.
[Filecat](../examples/tools/filecat/README.md) sudah menjadi aplikasi native Fermi;
tes generated-program mencakup fault, close gagal dan campuran owner buffer/file.

**Kemajuan unit Result:** `Result<void, E>`, `Result.ok()` dan success case tanpa
payload kini didukung, termasuk ABI dan pemeriksaan wajib-handle.

**Kemajuan struct:** [struct pemilik resource](spec/resource-structs.md) kini
mendukung konstruksi lengkap, move utuh, penggantian field, borrow buffer/file
melalui field, dan cleanup konstruksi parsial yang gagal.
[Borrow struct](spec/struct-borrows.md) mendukung akses read/edit selama pemanggilan,
termasuk field bersarang dan elemen array tetap. Partial move dan referensi yang
bisa disimpan atau dikembalikan belum tersedia.

**Kemajuan output dan tes:** create_new, write parsial, write_all dan sync tersedia
melalui [std.fs](spec/files.md), dengan contoh filecopy berbatas. [fermi test](spec/test-runner.md)
mengompilasi fungsi test_* lokal ke executable native sementara. Tes tidak
terisolasi per proses dan perubahan filesystem tidak di-rollback.

**Kemajuan generics:** [fungsi generik integer](spec/integer-generics.md) mendukung
satu parameter integer dan pemanggilan eksplisit. Constraint `integer`,
`signed_integer`, dan `unsigned_integer` memeriksa domain masing-masing. `std.bits`
menyediakan helper flag unsigned dengan budget statis; generics struct, tipe resource sebagai T dan inferensi belum tersedia.

**Pekerjaan berikutnya:** perluasan generics berdasarkan kebutuhan koleksi bertipe
dan rancangan callback dengan kontrak efek yang jelas.
Socket memerlukan kontrak
listener/connection, partial I/O dan shutdown tersendiri; integrasi file bukan
bukti bahwa lifecycle resource jaringan sudah tersedia.

### 2. Fondasi aplikasi layanan

- Argumen CLI, environment, konfigurasi tervalidasi, dan error startup yang jelas.
- Resource file dengan cleanup, byte/text UTF-8, parsing angka, formatting dasar.
- Waktu monotonic untuk deadline; waktu kalender untuk log. Logging terstruktur
  ke stderr dengan kontrol level dan tanpa menyalin rahasia konfigurasi ke log.
- Tentukan kebutuhan generics, koleksi, dan interface dari API buffer/error yang
  nyata; tambahkan bertahap setelah aturan ownership dapat diuji.

**Gerbang:** daemon satu proses membaca konfigurasi dan memproses pekerjaan
terbatas, dengan penanganan kegagalan I/O dan penghentian yang tidak bocor resource.

### 3. Networking dasar

- Resource TCP listener/connection, partial read/write, disconnect, EOF, dan error.
- Batas koneksi, ukuran buffer, ukuran pesan, serta deadline operasi.
- Mulai dari layanan TCP sederhana sebelum menambahkan runtime I/O asinkron.

**Gerbang:** layanan TCP Fermi lulus tes input terpotong/rusak, peer lambat,
disconnect di tengah operasi, kegagalan descriptor, dan pelepasan koneksi.

### 4. Concurrency dan runtime tugas

- Rancang ownership lintas tugas, aturan data sharing/synchronization, serta
  task lifecycle. Jangan mengandalkan pengguna untuk menghindari data race.
- Implementasikan concurrency terbatas, antrean berkapasitas, backpressure,
  deadline, cancellation, dan propagasi error.
- Putuskan model async/executor berdasarkan prototipe dan pengukuran; jangan
  menetapkan keyword atau scheduler permanen sebelum lifecycle teruji.

**Gerbang:** tes race/stress dan cancellation saat menunggu I/O/antrean;
shutdown menolak pekerjaan baru, menyelesaikan atau membatalkan pekerjaan aktif,
dan membebaskan resource tepat sekali dalam batas waktu.

### 5. HTTP API dan background worker

- Library HTTP dengan batas header/body, parsing defensif, dan timeout.
- JSON serta respons error terstruktur; TLS melalui library terpelihara yang
  dipilih setelah evaluasi integrasi dan lifecycle, tanpa kriptografi buatan sendiri.
- Worker dengan antrean terbatas, status pekerjaan, cancellation, dan shutdown.
- Health/readiness, metrik, serta konfigurasi deployment Linux/container.

**Gerbang:** aplikasi penerimaan utama di bawah berjalan tanpa host C khusus
aplikasi, dengan tes overload dan interoperabilitas klien HTTP.

## Dependensi dan status milestone

| Milestone | Dependensi | Status | Bukti untuk menutup tahap |
| --- | --- | --- | --- |
| M1: ownership/alokasi | compiler pilot | Byte-buffer terintegrasi; resource lebih luas dibatasi | Matriks growth/borrow O0/O2, fault injection, sanitizer |
| M2: fondasi layanan | M1 | Konfigurasi, CLI, numeric, monotonic/deadline dan file read/write dan test runner tersedia | CLI/file/time, input invalid, close failure, cleanup |
| M3: TCP | M2 | Belum tersedia | Partial I/O, timeout, peer lambat, disconnect |
| M4: task terbatas | M1 dan prototipe I/O M3 | Belum tersedia | Race/stress, cancellation, join dan bounded admission |
| M5: HTTP worker | M2–M4 | Belum tersedia | Aplikasi penerimaan dan laporan Linux |

Dokumentasi, diagnostik, packaging dan pengukuran mengikuti tahap terkait.
Sebelum M5, tambahkan lokasi sumber fault runtime, konfigurasi tervalidasi dan
instalasi relocatable sebagai pekerjaan eksplisit, bukan asumsi deployment.
Generic/resource fields memerlukan proposal berdasarkan kebutuhan M2–M4;
Foundry tidak otomatis menetapkan modelnya. Tidak ada tanggal rilis yang dijanjikan.

Pemetaan ke PRD: M1 memenuhi bagian ownership P02/P03; M2 memperluas P01/P05/P07;
M3–M5 memenuhi P04/P06. P08 harus diukur pada workload layanan dan P09 berlaku
pada setiap perubahan. Catat bukti aktual di progress/validation, bukan di kolom
status sebagai klaim tanpa hasil tes.

## Aplikasi penerimaan utama

Profil batas dan skenario penerimaan ditetapkan dalam [PRD](product/requirements.md).
[Foundry](../examples/design/foundry/README.md) adalah workload desain jangka panjang,
bukan syarat database/multi-tenant untuk tahap pertama.

Bangun layanan pemrosesan pekerjaan dengan endpoint `POST /jobs`, `GET /jobs/{id}`,
health/readiness, dan background worker. Pekerjaan awal memvalidasi atau mengolah
payload data berbatas. Kapasitas antrean, ukuran payload, jumlah worker, koneksi,
deadline, dan retensi hasil wajib dikonfigurasi serta memiliki default terbatas.

Versi awal menggunakan penyimpanan in-memory: pekerjaan tidak dijanjikan bertahan
setelah restart. Persistence, retry otomatis, dan jaminan delivery membutuhkan
kontrak tambahan sebelum diterapkan. Saat penuh, layanan mengembalikan respons
penolakan yang terdokumentasi, bukan terus menambah antrean.

Logika aplikasi ditulis dalam Fermi; runtime/library C umum tetap diperbolehkan.
Parser telemetri, filter, dan CSV menjadi workload uji pendukung, bukan batas
cakupan bahasa atau syarat yang harus selesai sebelum layanan pertama.

## Pengukuran dan verifikasi rilis

- Ukur throughput, latensi request p50/p95/p99, RSS puncak, alokasi, startup,
  ukuran executable, dan waktu shutdown pada beban normal, overload, dan soak.
- Laporkan metode load generation, concurrency, ukuran payload, durasi, error,
  warmup, dan kondisi host; pisahkan waktu antrean dari waktu pemrosesan.
- Tetapkan ambang kapasitas/latensi setelah baseline layanan tersedia. Belum ada
  janji angka throughput atau p99. Bandingkan implementasi dengan kontrak setara.
- Target microbenchmark 1,5× terhadap C tetap sasaran kernel, bukan penghalang
  semua pekerjaan backend. Target keseluruhan belum tercapai; lihat
  [hasil optimasi sum](research/2026-10-24-bounded-sum-optimization.md).
- Jalankan tes normal, sanitizer, fuzz parser/protokol, differential testing,
  fault injection, stress, serta soak. Verifikasi Linux secara nyata sebelum
  menyatakan layanan didukung pada target itu; catat hasil Termux terpisah.

## Di luar fokus awal dan batas yang tetap terlihat

GUI, game engine, kernel OS, bare-metal, distributed scheduler, registry paket,
backend LLVM langsung, serta metaprogramming umum tidak menjadi prioritas awal.
Database driver dan integrasi cloud ditambahkan berdasarkan kebutuhan aplikasi.

Saat ini IR masih berupa typed structured tree dengan metadata owner dan cleanup
untuk byte buffer dan file regular dengan read/write. Runtime concurrency belum diimplementasikan. Fault runtime belum memiliki lokasi sumber,
native stack belum dibatasi, dan distribusi compiler/library belum relocatable.
Rilis backend pertama tetap eksperimental sampai ada bukti penggunaan dan audit.

Plan ini menggantikan prioritas aplikasi native sebelumnya. Proposal v0.1/v0.2
serta dokumen 21–23 Oktober adalah konteks historis; spesifikasi fitur yang telah
diimplementasikan tetap berlaku sampai migrasinya dinyatakan secara eksplisit.

## Bootstrap acceptance gates

Lihat [riset dan gate bootstrap](development/bootstrapResearch.md) untuk urutan
manifest sumber, grammar, analisis semantik, backend C, dan pembangunan F1/F2/F3.
Status saat ini masih frontend awal; belum ada compiler Fermi-written yang dapat
mengompilasi sumber dirinya sendiri.
