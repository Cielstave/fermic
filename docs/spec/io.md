# Standard input and output

`use std.io;` menyediakan interface bootstrap berikut:

```fermi
pub enum Error { write_failed, read_failed, }
pub native func write(bytes: read u8[]) -> Result<usize, Error>;
pub native func read_into(bytes: edit u8[]) -> Result<usize, Error>;
pub native func write_stderr(bytes: read u8[]) -> Result<usize, Error>;
pub native func print(bytes: read u8[]) -> void;
pub native func println(bytes: read u8[]) -> void;
```

`native` hanya diizinkan pada library standar bawaan. Compiler memvalidasi
signature ini; deklarasi tersebut bukan fasilitas FFI untuk program pengguna.
Implementasi POSIX tersedia untuk target pengembangan Termux/Linux. Pengujian
saat ini dilakukan di Termux; Linux belum diverifikasi dalam sesi ini.

## Pemakaian

```fermi
use std.io;

func main() -> void {
    io.println("Hello, world!");
}
```

Literal memiliki tipe `read u8[]`, storage statis immutable, dan panjang byte
tanpa terminator NUL. Escape yang tersedia: `\n`, `\r`, `\t`, `\"`, `\\`.
Byte non-ASCII dipertahankan; belum ada validasi Unicode atau tipe string umum.
Literal langsung dapat menjadi argumen `read u8[]` tanpa keyword `read`; view lokal, view return, dan
peminjaman slice literal sebagai argumen belum tersedia. Array byte dan parameter
view tetap dapat diteruskan. `println` menambahkan newline; `print` dan `write` tidak menambahkannya.
Belum ada formatting.

## Convenience output

`print` dan `println` mengembalikan `void`, tanpa alokasi heap. Keduanya
menangani short write dan mengulang `EINTR`. Kegagalan output, termasuk zero
progress, memicu runtime fault 5 (`standard output write failed`), menghentikan
alur fungsi, dan membersihkan resource. Executable keluar dengan status 1.
`println` mengirim newline melalui syscall terpisah setelah seluruh input;
output parsial mungkin sudah terkirim sebelum fault. Gunakan `io.write` dengan
`match` jika aplikasi perlu menangani kegagalan melalui `Result`. Semua fungsi
output tetap memiliki efek I/O dan tidak dapat dipanggil dari fungsi berbudget.

Penghilangan `read` hanya berlaku untuk literal string immutable. Binding,
array, dan slice tetap membutuhkan akses eksplisit `read` atau `edit`.

## Kontrak penulisan

Keberhasilan berarti seluruh byte diterima oleh syscall `write`, bukan jaminan
persistensi media. Nilai sukses adalah jumlah byte. Input kosong berhasil dengan
nilai nol tanpa syscall. Runtime menangani short write dan mengulang `EINTR`.
Kegagalan lain, termasuk zero progress, menghasilkan `Error.write_failed`.
Sebagian byte mungkin sudah terkirim sebelum kegagalan; tidak ada rollback atau
jumlah parsial dalam error. Mengulang seluruh input dapat menggandakan output.

Output memakai descriptor 1 tanpa buffer stdio tambahan. Tidak ada perubahan
kebijakan sinyal proses: pipe tertutup tetap dapat memicu SIGPIPE. Tidak ada
jaminan atomicity, nonblocking completion, atau koordinasi dengan writer lain.

## Kontrak pembacaan

`io.read_into(edit buffer)` membaca descriptor 0 ke view mutable yang sudah
terinisialisasi. Fungsi ini mengulang `EINTR`, tetapi mengembalikan hasil setelah
satu syscall berhasil; short read bukan error dan bukan bukti EOF. Jumlah byte
maksimum per pemanggilan adalah `min(buffer.len, 4096)`. Byte sesudah jumlah yang
dikembalikan tetap memiliki nilai sebelumnya. Tidak ada decoding atau terminator.

View kosong berhasil dengan nol tanpa syscall; hasil itu tidak menguji EOF.
Untuk view tidak kosong, nol meneruskan hasil nol dari OS: biasanya EOF untuk
file atau pipe. Terminal noncanonical dengan timeout juga dapat menghasilkan
nol; library tidak mengubah konfigurasi terminal. Error selain `EINTR`, termasuk
`EAGAIN`, menjadi `Error.read_failed`; API ini belum menyediakan readiness atau
pembedaan errno. Pada error, caller tidak boleh menganggap buffer tidak berubah.
Tidak ada alokasi atau penutupan descriptor milik proses.

Contoh [copy_stdin.fe](../../examples/cli/copy_stdin.fe) memakai buffer tetap dan
menyalin hanya prefix yang berhasil dibaca. Program berhenti pada hasil nol dan
mengembalikan status 1 untuk kegagalan input atau 2 untuk kegagalan output.
Nama `read_into` menghindari benturan dengan keyword akses immutable `read`;
tidak ada perubahan grammar atau pengecualian keyword.

Perilaku syscall merujuk [POSIX read](https://pubs.opengroup.org/onlinepubs/9799919799/functions/read.html).

## Efek dan distribusi

Efek `io.read` dan `io.write` dicatat terpisah dan diteruskan melalui call graph, termasuk rekursi. Fungsi dengan
`@budget` yang mencapai I/O ditolak dengan `E_EFFECT`. Batas kerja dan heap pada
manifest fungsi I/O bernilai `null`; batas runtime eksternal belum disertifikasi.

`library/std/io.fe` dicari melalui path absolut yang disimpan saat compiler
dibangun. Modul proyek tidak dapat menggantikannya. Instalasi relocatable belum
tersedia; rebuild compiler setelah memindahkan checkout. `make test` dan
`make sanitize` mencakup input/output byte, EOF, error, efek, serta batas akses view.

Perilaku syscall mengikuti [POSIX write](https://pubs.opengroup.org/onlinepubs/9699919799/functions/write.html),
terutama partial write dan `EINTR`. Kontrak Fermi di atas menambahkan kewajiban
menangani `Result` serta pelacakan efek pada compiler.

## Standard error

`io.write_stderr` uses descriptor 2 with the same full-write, EINTR, error and
empty-input behavior as `io.write`. It carries `io.write`, not a new effect class.
It adds no newline, formatting or redaction; callers choose safe diagnostic content.
Startup examples report fixed field/constraint messages and never echo raw values.
