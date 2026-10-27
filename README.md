# Fermi

Fermi dikembangkan sebagai bahasa native untuk backend, background services, dan
infrastruktur berperforma tinggi, dengan keamanan memori dan kontrol resource.
Sumber memakai `.fe`; compiler ditulis dalam C11 dengan backend C/Clang.
[PRD dan peta dokumen](docs/README.md) menjelaskan kebutuhan, status fitur, dan
keputusan arsitektur. [Arah pengembangan aktif](docs/PLAN.md) menargetkan HTTP API dengan background
worker; networking dan concurrency belum tersedia pada compiler saat ini.

**Status: compiler pengembangan eksperimental.** Parser telemetri sudah dapat dikompilasi
dan dipanggil dari C. Belum ada klaim siap produksi atau lebih cepat daripada C.
Executable dan modul lokal sudah tersedia. Target yang diuji adalah AArch64
Android/Termux; Linux x86-64 menjadi target berikutnya. Host dan target wajib 64-bit.

Operasi bit unsigned dan [library biner](docs/spec/binary.md) juga tersedia untuk
flags, field protokol dan format file, dengan shift terperiksa serta byte order
eksplisit. Lihat [contoh frame biner](examples/cli/binary_frame.fe).

`match` mendukung Result, enum tanpa payload, serta nilai integer dan bool dengan
aturan kelengkapan pola. Tipe [str](docs/spec/strings.md) menyediakan nilai teks
UTF-8 immutable, pengukuran byte, dan helper Unicode dasar.
[Alias tipe](docs/spec/type-aliases.md) seperti `type Count = usize;` tersedia
langsung dalam bahasa dan mempertahankan identitas serta aturan tipe asal.
[Borrow struct](docs/spec/struct-borrows.md) menyediakan parameter `read Struct`
dan `edit Struct` untuk mengakses data caller tanpa transfer owner.

## Build dan coba

```sh
make
./build/fermi                         # Command overview
./build/fermi help run
./build/fermi run examples/tools/job_queue/main.fe
printf 'Fermi\n' | ./build/fermi run examples/tools/bytecount/main.fe -- 100
```

`run` membangun executable native lalu menjalankannya. Tanpa path sumber,
`main.fe` di direktori saat ini dipakai. Argumen program ditulis setelah `--`.
Hasil native yang belum berubah dipakai ulang dari `build/.fermi-run`, sehingga
run berikutnya tidak memanggil Clang lagi. Gunakan `--no-cache` untuk build
sementara tanpa reuse, atau `-o PATH` untuk menyimpan executable pada path tertentu. Lihat [CLI dan run](docs/spec/cli.md).

Untuk memakai binary hasil build sebagai perintah `fermi` pada sesi shell ini:

```sh
export PATH="$PWD/build:$PATH"
fermi help run
```

Prasyarat: Clang, Make, dan POSIX shell. `clang-format` diperlukan hanya untuk
formatting. Tidak ada dependency Python atau paket pihak ketiga.

```sh
make
./build/fermi check examples/telemetry/telemetry.fe
./build/fermi explain examples/telemetry/telemetry.fe
./build/fermi build --library examples/telemetry/telemetry.fe --out-dir build/telemetry
```

Hasil: `telemetry.o`, `telemetry.h`, `telemetry.contract.json`, dan `telemetry.c`
untuk inspeksi. Direktori induk `--out-dir` harus sudah ada. `FERMI_CC` dapat
menunjuk executable compiler C yang kompatibel; nilainya bukan string perintah
shell. Build mendukung `-O0` atau `-O2`, tanpa mematikan pemeriksaan keselamatan.

```fermi
@budget(work = 100_000, heap = 0)
export func count_positive(values: read i16[]) -> usize {
    if (values.len > 4096) {
        return 0;
    }
    var count: usize = 0;
    for (var i: usize = 0; i < values.len; i += 1) {
        if (values[i] > 0) {
            count += 1;
        }
    }
    return count;
}
```

`let` immutable, `var` mutable, dan tipe selalu tertulis. `read`/`edit` menentukan
akses buffer saat call. `Result` wajib ditangani atau diteruskan. Overflow,
pembagian nol, dan indeks ilegal menghasilkan fault terstruktur; wrapper C tidak
menghentikan proses host. Kontrak work menghitung operasi model bahasa, bukan
waktu CPU. Bound yang tidak dapat dibuktikan menyebabkan kontrak ditolak.

## Verifikasi

```sh
make test             # unit tests dan host C, pada -O0 serta -O2
make sanitize         # ASan/UBSan pada compiler, C hasil kompilasi, dan runtime
make test-runtime     # kontrak allocator dan buffer runtime C
make sanitize-runtime # ASan/UBSan khusus runtime buffer
make fuzz             # 10.000 input frontend/lowering dengan libFuzzer
make fuzz-telemetry   # 10.000 input paket pada parser hasil kompilasi
make benchmark        # decoding/filter/sum vs C, timing build, ukuran object; bukan tes kelulusan
make format
make check-format
```

Sanitizer dan fuzzing membutuhkan runtime Clang yang sesuai platform. Suite parser
menguji semua 65.536 nilai i16, 4.000 paket diferensial, domain error tanpa write,
dan validasi ABI. Benchmark awal masih menunjukkan overhead terhadap referensi C;
lihat [hasil verifikasi](docs/development/validation.md) dan
[benchmark tiga kernel](docs/research/2026-10-24-kernel-performance.md) untuk angka,
metode, serta batas pengukuran. [Optimasi reduksi terbatas](docs/research/2026-10-24-bounded-sum-optimization.md)
sudah mempercepat kernel penjumlahan; hasil tersebut bukan klaim seluruh bahasa
setara C. Artifact terbaru berada di `build/performance/`.

Byte yang dipinjam dapat disalin ke buffer independen melalui
`std.mem.from_bytes(read source, limit)`. Contoh executable tersedia di
[owned_bytes.fe](examples/cli/owned_bytes.fe); kontrak alokasi dan ownership
dijelaskan dalam [spesifikasi buffer](docs/spec/buffers.md#copying-borrowed-bytes).

## Aplikasi CLI

`std.args` membaca argumen pengguna ke buffer caller tanpa truncation. `std.fmt`
memformat `usize` sebagai decimal tanpa alokasi. Contoh
[bytecount](examples/tools/bytecount/README.md) menggabungkannya dengan stdin,
validasi angka, dan buffer tetap dalam aplikasi Fermi multi-file:

```sh
./build/fermi build examples/tools/bytecount/main.fe -o build/bytecount -O2
printf 'Fermi\n' | ./build/bytecount 100
# 6
```

## Waktu monotonic

`std.time` menyediakan timestamp nanosecond dan perhitungan deadline terperiksa.
[Kontrak waktu](docs/spec/time.md) membedakan pembacaan clock dari helper murni;
[contoh deadline](examples/cli/deadline.fe) sudah dapat dikompilasi dan dijalankan.
API ini belum membatalkan operasi I/O yang sedang blocking.

## Konfigurasi startup

`std.env.read_into` membaca environment ke storage berbatas tanpa truncation.
`std.num` memisahkan parser decimal/range bersertifikat dari akses eksternal;
`std.config.usize_env` menghasilkan nilai tervalidasi untuk startup.
[Contoh startup](examples/cli/startup_config.fe) dan
[kontrak API](docs/spec/configuration.md) menjelaskan batas dan kegagalannya.

## Dukungan Acode

Plugin [Fermi Language](tools/acode-fermi/README.md) menyediakan pewarnaan `.fe`,
completion, snippet dan linting leksikal offline. Paket ZIP serta petunjuk instalasi
tersedia di direktori plugin. Pemeriksaan tipe dan ownership tetap melalui compiler.

## File read-only

`std.fs.File` menyediakan owner opaque, pembacaan dengan `edit fs.File`, cleanup
otomatis, dan `fs.close(move(file))` yang menghasilkan `Result<void, fs.Error>`.
[Filecat](examples/tools/filecat/README.md) sudah dapat membuka dan menyalin file
melalui Fermi. Lihat [kontrak file](docs/spec/files.md) untuk batas platform dan I/O.

## Result tanpa payload sukses

`Result<void, E>` memakai `Result.ok()` dan `case Result.ok()` tanpa nilai palsu.
Penanganan error tetap wajib; lihat [kontrak unit Result](docs/spec/unit-results.md)
dan [contoh executable](examples/cli/unit_result.fe).

## Panduan repository

Source compiler dipisahkan menurut tanggung jawab di `compiler/`. `examples/`
berisi program Fermi, `runtime/` berisi implementasi buffer dan konteks startup proses,
`tests/` berisi unit/integrasi/fuzzing, dan `docs/` memisahkan
riset, proposal bahasa, spesifikasi pilot, dan keputusan implementasi.

- [Spesifikasi pilot](docs/spec/pilot.md), [model biaya](docs/spec/work-model.md),
  dan [paket telemetri serta ABI](docs/spec/telemetry.md).
- [Arsitektur compiler](docs/architecture/compiler.md) dan
  [gaya kode C](docs/development/coding-style.md).
- [Riset kebutuhan bahasa](docs/research/2026-10-language-landscape.md) dan
  [proposal bahasa v0.2](docs/design/fermi-v0.2.md).
- [Status pekerjaan](docs/PLAN.md) dan [catatan perubahan](docs/development/progress.md).

I/O file, allocator pengguna, metering runtime, concurrency, generics umum,
package manager, dan `contract diff` belum masuk pilot. Fermi bukan sandbox untuk
caller C berbahaya. Validitas alokasi dan akses thread tetap kewajiban host.

## Referensi aplikasi: sensor hub

[`examples/sensor_hub/hub.fe`](examples/sensor_hub/hub.fe) memproses paket sensor,
mengalibrasi sampel, menghitung statistik, dan menjaga hysteresis alarm. Jalankan
`make simulate` untuk simulasi tiga kanal. Lihat [kontrak dan cara pakai](examples/sensor_hub/README.md)
serta [temuan desain bahasa](docs/research/sensor-hub-reference.md).

## Aplikasi native dan modul

```sh
./build/fermi build examples/cli/exit_status.fe -o build/example
./build/example
./build/fermi build examples/cli/modules/main.fe -o build/modules-example
./build/modules-example
./build/fermi check examples/cli/modules/main.fe --diagnostic-format=json
```

Entry point memakai `func main() -> void` atau `func main() -> i32` untuk status eksplisit. Import lokal memakai `use math.statistics;`
dan API antarmodul memakai `pub`. Lihat [spesifikasi modul](docs/spec/modules.md)
dan [format diagnostik](docs/spec/diagnostics.md). Input/output standar tersedia melalui
[std.io](docs/spec/io.md); ownership resource masih dalam pengembangan.

```fermi
use std.io;

func main() -> void {
    io.println("Hello, world!");
}
```

```sh
./build/fermi build examples/cli/hello.fe -o build/hello
./build/hello
# Hello, world!
```

Build pengembangan mencari library standar di direktori `library/` absolut yang
tercatat saat `make`. Jika checkout dipindahkan, jalankan `make clean && make`.
Memindahkan executable compiler saja belum didukung.

Propagasi error memakai `try(expression)` dengan tipe error yang sama persis:

```fermi
use std.num;

func parse_limit(bytes: read u8[]) -> Result<usize, num.Error> {
    let value: usize = try(num.parse_usize(read bytes));
    return Result.ok(value);
}
```

Error keluar melalui jalur cleanup resource. Lihat [kontrak try](docs/spec/error-propagation.md)
dan jalankan `./build/fermi run examples/cli/try_result.fe` untuk contoh native.

Enum dapat membawa satu payload bertipe dan tetap diperiksa lengkap melalui `match`.
Contoh dispatcher lintas modul tersedia di
[event_dispatch](examples/cli/event_dispatch/main.fe):

```sh
./build/fermi run examples/cli/event_dispatch/main.fe
# Job accepted.
# Worker stopped.
```

Lihat [kontrak enum berpayload](docs/spec/payload-enums.md), termasuk aturan move/copy
dan cleanup resource pada varian aktif.

Contoh penyalin stdin ke stdout:

```sh
./build/fermi build examples/cli/copy_stdin.fe -o build/copy-stdin
printf 'Fermi reads stdin.\n' | ./build/copy-stdin
```

Program memakai buffer tetap, menerima byte biner, dan berhenti pada EOF.
Argumen CLI dan pembukaan file belum tersedia.

## Buffer milik program

`std.mem.bytes(length, limit)` mengalokasikan `Buffer<u8>` melalui `Result`.
Buffer mendukung move, read/edit view, bounds checks, dan cleanup otomatis pada
scope exit, return, serta fault. `mem.reserve` dan `mem.resize` memakai peminjaman
descriptor eksklusif `edit Buffer<u8>`; pertumbuhan gagal mempertahankan buffer lama.
Contoh lengkap: [buffer_growth.fe](examples/cli/buffer_growth.fe).
Lihat [kontrak buffer](docs/spec/buffers.md) dan [contoh executable](examples/cli/buffer.fe).

```sh
./build/fermi build examples/cli/buffer.fe -o build/buffer-example
./build/buffer-example
```

Output: `Fermi` diikuti newline.

## Struct pemilik resource

Struct kini dapat menyimpan buffer, file, dan resource bersarang. Konstruksi gagal,
return, loop exit, dan fault membersihkan field secara otomatis; field resource
mutable dapat dipinjam untuk operasi buffer/file. Partial move masih ditolak.

```sh
./build/fermi run examples/cli/owned_packet/main.fe
```

Contoh mencetak `Worker ready.` menggunakan payload struct dari modul lain.
Lihat [kontrak struct pemilik resource](docs/spec/resource-structs.md).

## Output file dan pengujian native

`std.fs` menyediakan `create_new`, `write`, `write_all`, dan `sync`.
Pembuatan file menolak tujuan yang sudah ada. Penulisan dapat gagal setelah
sebagian data tersimpan; API ini tidak menyediakan transaksi atau rollback.

`fermi test` menjalankan fungsi lokal `testName() -> bool` atau `test_*() -> bool`; `true` berarti lulus.
Tes dikompilasi native, dijalankan berurutan, dan dilaporkan satu per satu.

```sh
./build/fermi run examples/tools/filecopy/main.fe -- input.bin output.bin
./build/fermi test examples/testing/codec/tests.fe
```

Lihat [filecopy](examples/tools/filecopy/README.md),
[aturan file](docs/spec/files.md), dan [test runner](docs/spec/test-runner.md).

## Kontrak efek per fungsi

`@effects()` menolak efek eksternal/resource yang dilacak.
`@effects(memory)` mengizinkan efek memori tanpa mengizinkan file atau I/O.
Pemeriksaan mencakup helper transitif dan cleanup otomatis. Kontrak ini tidak
memerlukan work bound statis dan tidak membuktikan pure function atau terminasi.

```sh
./build/fermi test examples/cli/effect_contracts/main.fe
./build/fermi explain examples/cli/effect_contracts/main.fe
```

Lihat [kontrak efek](docs/spec/effect-contracts.md) untuk aturan dan batasnya.

## Contoh fitur terbaru

[Contoh runnable](examples/features/README.md) mencakup parser header biner,
job dengan payload milik sendiri, dan penulisan laporan file. Contoh menggunakan
kontrak efek, modul lokal, enum berpayload, struct pemilik resource, `try`, serta
`fermi test`. Dua contoh penolakan memperlihatkan efek I/O tersembunyi dan cleanup
yang tidak diizinkan oleh kontrak fungsi.

```sh
./build/fermi run examples/features/frame_check/main.fe
./build/fermi test examples/features/owned_jobs/main.fe
./build/fermi run examples/features/file_report/main.fe -- build/example-status.txt
```

Tujuan file pada perintah terakhir harus belum ada.

## Fungsi generik integer

Fungsi dengan satu parameter `T: integer` dapat dipakai ulang pada sembilan tipe
integer. Pemanggilan memilih tipe secara eksplisit, misalnya `larger<u32>(7, 9)`.
Constraint `signed_integer` dan `unsigned_integer` tersedia untuk domain lebih
sempit. Instance fungsi biasa dibuat dan diperiksa sesuai panggilan konkret, bukan untuk
seluruh domain constraint. Template yang tidak dipanggil hanya diperiksa sintaks
dan deklarasinya; generics struct dan tipe
resource sebagai T belum tersedia. Batas proyek adalah 128 template biasa dan
128 fungsi konkret; instance yang diminta memakai satu slot fungsi. Adapter
native integer allocate/resize masih diperiksa melalui ekspansi finite; adapter
data memiliki kontrak terpisah.

```sh
./build/fermi test examples/features/integer_generics/main.fe
./build/fermi run examples/features/integer_generics/main.fe
```

Lihat [kontrak fungsi generik integer](docs/spec/integer-generics.md).

`std.bits` menyediakan helper flag unsigned `has_all`, `has_any`, `set`, `clear`,
dan `toggle`, dengan kontrak tanpa efek eksternal dan work budget statis.

```sh
./build/fermi test examples/features/integer_domains/main.fe
./build/fermi run examples/features/integer_domains/main.fe
```

Lihat [kontrak std.bits](docs/spec/bits.md).

## Percobaan kemampuan bahasa

[Capability lab](examples/capability_lab/README.md) menyediakan empat aplikasi Fermi yang dapat dijalankan: algoritma bilangan, pencarian grid, evaluator instruksi, dan pengolah stok multi-file. Laporannya mencatat hasil pengujian serta keterbatasan yang ditemukan.

[Library circulation system](examples/library_system/README.md) adalah aplikasi batch Fermi 1.300 baris dengan 17 operasi, pemeriksaan invariant, serta pengujian jurnal dan laporan.

[Signed decimal example](examples/features/signed_numbers/README.md) menunjukkan parsing dan formatting `i32`/`i64` tanpa alokasi, termasuk nilai negatif dan batas integer.

[Array initialization example](examples/features/array_initializers/README.md) memakai `[value; count]` untuk mengisi array bertipe eksplisit tanpa mengulang initializer.

[Owned text example](examples/features/owned_text/README.md) membangun teks UTF-8 dari argumen runtime menggunakan `std.text.Text`, field private, dan pertumbuhan yang fallible.

[Text ranges](examples/features/text_ranges/README.md) menunjukkan pencarian, penyalinan rentang byte pada batas Unicode scalar, dan truncation tanpa memecah UTF-8.

[Dynamic integer buffers](examples/features/typed_buffers/README.md) menyediakan `Buffer<T>` untuk sembilan tipe integer melalui `std.buffer`, dengan alokasi fallible, reserve/resize, dan push berbatas byte.

[Buffer batches](examples/features/buffer_batches/README.md) menunjukkan konstruksi dari slice dan penambahan satu batch integer dengan pertumbuhan fallible.

[Inventory journal processor](examples/inventory/README.md) menguji nama UTF-8,
mutasi stok/harga, snapshot yang dapat dimuat ulang, dan slice array melalui
borrow struct. Rancangan [kandidat sintaks](docs/design/syntax-candidates.md)
dipisahkan dari fitur compiler yang sudah tersedia.

[data generics](docs/spec/data-generics.md) menyediakan T: data dan view record;
[record buffers](docs/spec/record-buffers.md) menyediakan penyimpanan dinamis
nonresource dengan inisialisasi eksplisit. Contoh [data algorithms](examples/features/data_generics/README.md)
dan [dynamic records](examples/features/record_buffers/README.md) dapat dijalankan.
Status seluruh pekerjaan ada di [foundation program](docs/development/foundation-program.md).

## Bootstrap frontend

The [Fermi-written bootstrap frontend](bootstrap/README.md) reads files and
collects tokens and [parses a bounded subset](bootstrap/PARSER.md) into an AST.
The production compiler remains C; semantic analysis and self-hosting have not
been ported. New bootstrap code follows the [naming convention](docs/development/naming.md).

## Dispatch workload

[Dispatch](dispatch/README.md) is a runnable job-broker simulation written in
current Fermi: tenant quotas, priority scheduling, fenced leases, retries,
dependencies, replay checkpoints, queue gauges and a bounded audit trail.
Build and run its scenario with `make -C dispatch demo`.

## Platform dan instalasi

Build compiler memakai C11 dan POSIX, tanpa path Termux yang ditulis tetap.
Host/target saat ini tetap 64-bit. Linux x86-64 dan AArch64 memiliki konfigurasi
CI Clang/GCC; macOS masih eksperimental dan runtime file belum didukung.
Windows native dan 32-bit belum didukung. Lihat [status platform dan build](docs/development/platforms.md).

```sh
make help
make -j4
make check
make install prefix="$HOME/.local"
# Staging paket, tanpa menulis ke /usr:
make install prefix=/usr DESTDIR="$PWD/build/package"
```
