# Diagnostik compiler

Setiap error memiliki kode, kategori, lokasi utama, akhir rentang token,
pesan, serta lokasi terkait dan petunjuk bila tersedia. Compiler berhenti pada
error pertama; recovery untuk melaporkan banyak error belum tersedia.
Kode pilot seperti `E_TYPE`, `E_INIT`, dan `E_BUDGET` dipertahankan.

```sh
./build/fermi check program.fe --diagnostic-format=text
./build/fermi check program.fe --diagnostic-format=json
```

Format teks menampilkan `path:line:column`, `error[CODE]`, cuplikan source, dan
caret. Error penggunaan nilai setelah move menyertakan lokasi transfer yang
mungkin menyebabkan nilai tidak tersedia. Lokasi tersebut adalah saksi analisis,
bukan jejak eksekusi runtime.

Keluaran JSON adalah satu object per baris pada stderr, schema version 1:

- `severity`: saat ini `error`.
- `category`: `user` atau `compiler` untuk internal compiler error.
- `code`, `message`, `path`: identitas dan penjelasan error.
- `primary`, `end`: offset byte nol-based, line/column satu-based; akhir eksklusif.
- `related`: null atau object dengan `span` dan `message`.
- `help`: petunjuk perbaikan; string kosong jika tidak tersedia.
- `encoding`: `byte-preserving`. Byte non-ASCII di-escape menjadi U+0080–U+00FF;
  konsumen dapat memulihkan byte asli sebelum mendekode UTF-8/path platform.

Byte kontrol dan non-ASCII pada format teks di-escape agar nama file atau input
rusak tidak menyisipkan terminal control sequences. Tab source ditampilkan sebagai
empat spasi; kolom diagnostik tetap menghitung byte. Cuplikan panjang dipotong di
sekitar lokasi error. Warna hanya aktif pada terminal dan dimatikan oleh `NO_COLOR`.

Exit code compiler: 0 berhasil, 1 error kompilasi/build/I/O, 2 penggunaan CLI salah.
Pesan diagnostik selalu stderr; hasil `check`, `explain`, dan build library tetap
stdout. `--diagnostic-format` mengatur diagnostik, bukan seluruh output perintah.

Error driver menggunakan `E_USAGE`, `E_IO`, `E_PATH`, `E_ENTRY`, `E_EXPORT`, atau
`E_TOOLCHAIN`. Test runner menggunakan `E_TEST` untuk signature tes yang tidak valid
atau source tanpa fungsi tes. Peluncuran program dan pengelolaan output sementara memakai `E_RUN`. Backend dijalankan tanpa shell; keluaran kegagalannya ditampung,
di-escape, dan dibatasi 319 byte pada diagnostik. Kegagalan backend belum otomatis
diklasifikasikan sebagai bug compiler karena juga dapat berasal dari instalasi
atau konfigurasi toolchain. Error modul memakai `E_MODULE`, `E_MODULE_CYCLE`,
dan `E_PRIVATE`.

`run` meneruskan exit status program (0–255), atau `128 + signal` bila child
berakhir karena sinyal. Nilai itu dapat sama dengan exit code driver. Diagnostik
compiler tetap mengatur error driver saja; stderr program tidak diubah menjadi
JSON. Lihat [CLI](cli.md).

`E_TRY` reports invalid Result propagation: an incompatible enclosing return type,
a non-Result operand, or a different error type. Error-type mismatches include
both type names and a hint to match and convert explicitly. Consumed Result
bindings retain the existing moved-value diagnostics, including the propagation
location.
