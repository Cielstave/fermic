# Rencana Fermi

Status: rancangan untuk implementasi; belum ada compiler atau klaim performa
terukur. Dokumen ini menggantikan arah prototipe Python sebelumnya.

## 1. Keputusan dasar

- Nama bahasa: **Fermi**. Ekstensi sumber: **`.fe`**.
- Implementasi compiler: **C11**, dengan Clang/GCC sebagai toolchain pengembang.
- Hasil kompilasi: executable atau library native; tidak memerlukan interpreter.
- Backend awal: C11 yang dihasilkan dari representasi antara bertipe. Backend
  mesin lain dipertimbangkan setelah ada kebutuhan dan hasil pengukuran.
- Abstraksi: fungsi, tipe bernama, koleksi, dan modul yang nyaman seperti bahasa
  sistem modern. Pengguna tidak perlu mengelola register atau menulis assembly.
- Sintaks: `{}`, `()`, `[]`, `;`, operator biasa, tipe eksplisit, `let`/`var`.
- Fokus: library komputasi, pemrosesan buffer, dan komponen sistem yang perlu
  memahami biaya pekerjaan. Kernel numerik merupakan sasaran pertama yang kecil
  dan dapat diuji, bukan batas akhir seluruh bahasa.
- Tidak mengejar framework web, GUI, mesin virtual, makro umum, inheritance
  kompleks, atau ekosistem package manager dalam versi awal.

## 2. Sintaks yang dijadikan dasar

```fermi
use std.io;

@work(100)
func dot(left: [i64; 4], right: [i64; 4]) -> i64 {
    var total: i64 = 0;

    for i: i64 in 0..4 {
        total += left[i] * right[i];
    }

    return total;
}

@effects(io)
func main() -> i64 {
    let samples: [i64; 4] = [2, 4, 6, 8];
    let weights: [i64; 4] = [1, 2, 3, 4];
    let result: i64 = dot(samples, weights);

    if result > 50 {
        io.println("Hasil melewati ambang");
    } else {
        io.println("Hasil dalam ambang");
    }

    io.println(result);
    return 0;
}
```

`func` mendefinisikan fungsi, `->` menyatakan hasil, dan `return` mengembalikannya.
`let` tidak bisa diubah setelah inisialisasi; `var` dapat menerima assignment.
Parameter bersifat immutable secara default. Semua binding, parameter, hasil,
dan indeks loop menyebut tipe. Tidak ada konversi implisit antarjenis angka atau
antara angka dan boolean. Rentang `0..4` mencakup 0 hingga 3. Komentar memakai `//`.

Operator awal: `+ - * / %`, `== != < <= > >=`, `&& || !`, dan assignment
`= += -= *= /= %=`. Evaluasi berlangsung kiri ke kanan; `&&` dan `||` memakai
short-circuit. Kondisi `if` harus bertipe `bool`.

Subset pertama menyediakan `i64`, `bool`, array tetap seperti `[i64; 4]`, serta
literal teks untuk library output. Jenis integer lain, float, record, dan tagged
union menyusul dengan aturan ukuran/layout yang tertulis; bukan sekadar keyword
parser tanpa semantik. Tidak ada `null` implisit.

## 3. Identitas: kontrak sumber daya yang komposisional

Fermi mengutamakan kemampuan membaca batas biaya sebuah API dari deklarasinya.
Dua kontrak pertama adalah `@work(N)` dan `@effects(...)`.

### Batas kerja

`@work(100)` meminta pembuktian bahwa batas atas operasi abstrak fungsi tidak
melebihi 100. Analisis mencakup fungsi yang dipanggil, loop, cabang, dan penyalinan
array. Compiler menampilkan batas yang dihitung beserta sumber biaya terbesar.

Model biaya harus diberi versi dan didokumentasikan sebelum implementasi:

- Operasi scalar, pembacaan, dan penulisan mempunyai biaya dasar yang tetap.
- Salinan array dihitung berdasarkan jumlah elemennya.
- Cabang mengambil biaya kondisi ditambah cabang termahal.
- Loop mengalikan batas iterasi dengan biaya tubuh dan kontrol loop.
- Call memasukkan evaluasi argumen, transfer nilai, dan batas fungsi tujuan.
- Early return dan short-circuit boleh dianalisis secara konservatif; compiler
  tidak boleh melaporkan batas lebih kecil daripada model yang dijanjikan.

Kontrak ini merupakan ukuran kerja menurut semantik Fermi, **bukan WCET, jumlah
instruksi mesin, durasi I/O, atau jaminan hard real-time**. OS, cache, frekuensi CPU,
dan backend memengaruhi waktu nyata.

MVP membuktikan rentang literal dan call graph tanpa siklus. Rekursi atau loop
berbatas tak diketahui di dalam rantai kontrak menghasilkan diagnostic
"batas belum dapat dibuktikan". Setelah loop umum tersedia, fungsi tanpa
`@work` boleh memakainya; fitur kontrak tidak memaksa seluruh bahasa menjadi
bahasa loop statis. Memanggil fungsi tersebut dari fungsi berkontrak membutuhkan
bukti batas, bukan anotasi yang dipercaya begitu saja.

### Efek

Tanpa anotasi efek, fungsi tidak boleh melakukan I/O atau memanggil fungsi yang
memerlukannya. Mutasi variabel lokal tetap diperbolehkan. `@effects(io)` menyatakan
kebutuhan efek yang juga diperiksa melalui rantai pemanggilan.

Efek bahasa bukan sandbox OS. Library native dipercaya hanya di batas yang jelas;
FFI tidak dapat mengklaim biaya atau efek arbitrer tanpa menjadi bagian basis
kepercayaan. MVP tidak membuka FFI umum kepada pengguna.

Pemeriksaan efek dan pembuktian terminasi sudah mempunyai pendahulu, antara lain
[Koka](https://www.microsoft.com/en-us/research/project/koka/) dan
[SPARK](https://docs.adacore.com/spark2014-docs/html/ug/en/source/assertion_pragmas.html).
Identitas yang diusulkan adalah kombinasi kontrak biaya yang praktis, tipe eksplisit,
dan kompilasi native. Belum ada dasar untuk mengklaim fitur ini unik di dunia.

## 4. Keamanan dan efisiensi

Aturan MVP:

- Semua nilai diinisialisasi sebelum dibaca; setiap jalur fungsi mengembalikan
  tipe hasil yang benar. Parameter dan binding immutable tidak bisa dimutasi.
- Overflow integer, pembagian nol, dan overflow pembagian bertanda diperiksa.
  Build optimasi tidak mematikan pemeriksaan keselamatan.
- Indeks yang pasti salah ditolak saat kompilasi. Indeks yang belum terbukti aman
  diperiksa saat runtime; pemeriksaan hanya dihapus jika ada bukti yang sah.
- Array tetap memakai semantik nilai dan penyimpanan otomatis. Salinan dan biaya
  ukuran array harus terlihat dalam laporan kontrak. Optimizer boleh menghapus
  salinan jika perilakunya sama.
- Tidak ada raw pointer, heap, alias mutable, atau akses memori tanpa inisialisasi
  pada subset pertama. Ini memperkecil permukaan masalah, bukan bukti keamanan
  untuk fitur yang belum dibangun. Stack exhaustion tetap merupakan keterbatasan.
- Trap menghentikan program dengan lokasi sumber yang berguna; penanganan error
  yang dapat dipulihkan akan menggunakan hasil bertipe setelah tagged union ada.

Tahap memori lanjutan menambahkan pemilik tunggal untuk resource dinamis, cleanup
deterministik, serta akses baca/tulis dengan lingkup yang dibatasi. Tidak ada GC
wajib atau reference counting tersembunyi. Sebelum fitur ini diterima, spesifikasi
harus menjawab transfer kepemilikan, alias, invalidasi, escape, dan cleanup pada
semua jalur. Sintaksnya belum dibakukan. FFI/unsafe menunggu batas ini siap.

## 5. Inti bahasa dan library

Inti bahasa menangani tipe, fungsi, modul, kontrol alur, operasi nilai, aturan
memori, serta verifikasi kontrak. `len` untuk array tetap merupakan intrinsic yang
bisa menjadi konstanta saat kompilasi.

`std.io` menyediakan `io.println`; printing bukan keyword bahasa. Implementasi
native berada dalam library terpisah dengan metadata signature dan efek yang
terpusat. Compiler melakukan resolusi modul dan pemanggilan, bukan menyisipkan
implementasi I/O ke setiap tahap parser. Literal teks tidak diteruskan sebagai
format string C. Biaya operasi eksternal yang tidak diketahui tidak boleh
menghasilkan bukti kontrak palsu.

File system, jaringan, allocator, koleksi dinamis, waktu, dan thread menjadi
library bertahap. Penambahan masing-masing mengikuti dukungan tipe dan memori yang
diperlukannya; tidak semuanya dimasukkan ke MVP.

## 6. Arsitektur compiler C

```text
.fe -> lexer -> parser/AST -> resolusi nama & tipe
    -> analisis mutabilitas, alur, efek, dan biaya
    -> IR bertipe -> backend C -> Clang/GCC -> executable native
```

Struktur yang akan dibuat:

```text
src/           CLI, source manager, lexer, parser, checker, IR, backend
include/       interface internal compiler
lib/std/       modul standar dan adapter native
runtime/       trap, pemeriksaan aritmetika dan indeks
examples/      program .fe yang dapat dikompilasi
tests/unit/    pengujian compiler dalam C
tests/cases/   program diterima, ditolak, dan hasil runtime
bench/         pasangan program Fermi/C dan prosedur pengukuran
docs/          spesifikasi, model biaya, dan keputusan desain
```

AST menggunakan arena milik proses kompilasi; seluruh alokasi memeriksa kegagalan.
String sumber direpresentasikan dengan panjang eksplisit. Lexer/parser membatasi
ukuran input dan kedalaman nesting, melaporkan error dengan kode, baris, dan kolom.
Parser memulihkan error seperlunya agar diagnostic berikutnya tetap bermakna.

IR menormalisasi urutan evaluasi. Backend tidak mengandalkan urutan argumen C yang
tidak ditentukan, signed overflow, atau perilaku pointer yang tidak sah. Native
compiler dipanggil dengan argument vector, bukan penggabungan string shell.
Source, generated C, dan output binary tidak boleh saling menimpa tanpa sengaja.

Tooling awal menggunakan Make, C, dan POSIX shell. `clang-format` mengatur format;
peringatan compiler dan sanitizer menjadi pemeriksaan pengembangan. Build biasa
tidak memerlukan Python, CMake, unduhan paket, atau LLVM development libraries.

## 7. Tahapan dan kriteria selesai

| Tahap | Hasil | Kriteria penerimaan |
| --- | --- | --- |
| M0 — desain | Grammar, contoh, aturan tipe, model biaya | Contoh dan spesifikasi konsisten; fitur rencana ditandai jelas |
| M1 — frontend C | Lexer, parser, AST, diagnostic, type checker | Program benar diterima; mutasi immutable, tipe salah, nama tak dikenal, dan return hilang ditolak |
| M2 — executable native | IR, backend C, runtime aman, adapter std.io | Contoh dot product, filter, Fibonacci iteratif, dan cabang berjalan; hasil sama pada build debug/optimasi |
| M3 — kontrak | Efek transitif, work bound, laporan biaya | Perubahan loop/callee yang melampaui budget ditolak; tidak ada klaim proof untuk call tak diketahui |
| M4 — kegunaan sistem | Modul pengguna, record/tagged union, tipe numerik tambahan, loop umum | Library kecil dapat dipisah antarmodul; kontrak tetap benar saat fitur bertambah |
| M5 — memori terkendali | Resource owner, view, mutasi terbatas, cleanup | Uji negatif untuk use-after-transfer, dangling view, alias mutable, dan cleanup ganda |

M2 dan M3 membentuk rilis eksperimen pertama. M4/M5 membutuhkan desain dan
validasi tersendiri; keduanya tidak dijanjikan selesai bersama compiler pertama.
Perkembangan berikutnya mengikuti kebutuhan nyata, bukan daftar fitur bahasa lain.

## 8. Pengujian dan pengukuran

- Unit test C untuk lexer, parser, tabel simbol, tipe, dan aritmetika model biaya.
- Compile-fail cases memeriksa kode diagnostic dan lokasi, termasuk kontrak yang
  melampaui budget melalui pemanggilan tidak langsung dan loop bersarang.
- Runtime tests untuk batas integer, pembagian nol, indeks negatif/terlalu besar,
  short-circuit, urutan efek, dan array yang tidak saling mengubah setelah disalin.
- Uji parser dengan input malformed, sangat panjang, dan nesting dalam; fuzzing
  serta ASan/UBSan dijalankan pada host yang mendukung. Batas dukungan Termux
  dicatat; kegagalan tooling tidak disamakan dengan keberhasilan pemeriksaan.
- Benchmark native Fermi dibanding C dengan algoritma, overflow/bounds checks,
  input, flags optimasi, dan target CPU yang setara. Ukur runtime, ukuran binary,
  waktu kompilasi, dan peak memory compiler secara terpisah.
- Laporan benchmark mencantumkan versi compiler, mesin, pengulangan, dan variasi
  pengukuran. Tidak ada klaim "lebih cepat" sebelum data mendukungnya.

Interface CLI berikut adalah **target yang belum tersedia**:

```sh
make
make test
./build/fermi check examples/dot.fe
./build/fermi explain examples/dot.fe
./build/fermi build examples/dot.fe -o build/dot
./build/dot
```

Langkah implementasi berikutnya adalah M0: membakukan grammar dan tabel biaya,
kemudian membangun lexer/parser C beserta tes sebelum mengerjakan backend.
