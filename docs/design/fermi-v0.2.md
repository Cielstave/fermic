# Fermi v0.2 — Komponen native dengan kontrak sumber daya

**Status: proposal desain, 23 Oktober 2026. Subset pilot sudah diimplementasikan.**
Spesifikasi implementasi tersedia di [pilot](../spec/pilot.md); fitur roadmap
dalam proposal ini tidak otomatis tersedia pada compiler.
Dokumen ini menggantikan arah dan sintaks v0.1. Dasarnya adalah
[kajian bahasa dan kebutuhan](../research/2026-10-language-landscape.md).

> Arah produk proposal ini bersifat historis. Fokus backend dan urutan pekerjaan
> saat ini ditetapkan dalam [plan aktif](../PLAN.md). Spesifikasi pilot tetap
> menjelaskan fitur yang benar-benar tersedia.

## 1. Tujuan produk

Fermi adalah bahasa untuk menulis **komponen pemroses data native yang aman,
dengan akses memori dan biaya pekerjaan yang dapat diperiksa**, lalu memasukkannya
ke aplikasi yang sudah ada. Ekstensi sumber tetap `.fe`; compiler ditulis dalam C11.

Kasus utama pertama adalah parsing, validasi, dan transformasi buffer berukuran
terbatas. Program host mengatur transport, scheduling, storage, dan lifecycle.
Fermi menyediakan fungsi yang mengolah data dan mengembalikan hasil bertipe.

Identitas Fermi terletak pada kontrak yang menjadi bagian artefak library:
**apa yang boleh disentuh, berapa pekerjaan yang dibatasi, dan asumsi apa yang
harus dipenuhi pemanggil**. Ini hipotesis nilai produk, bukan klaim penemuan
ownership, efek, atau analisis biaya.

Tidak menjadi target awal: mengganti sistem operasi, framework service, GPU stack,
browser scripting, menjalankan plugin bermusuhan, atau verifikasi matematis penuh.
Program host tetap dapat menggunakan C, C++, Rust, atau bahasa lain melalui C ABI.

## 2. Contoh yang menjadi acuan

Contoh ini menyaring batch sensor ke buffer milik pemanggil. Budget contoh 150.000 memakai `fermi-work-1`; bound pilot yang dihitung adalah
114.733. Angka tersebut bukan waktu CPU.

```fermi
enum FilterError {
    input_too_large,
    output_too_small,
}

@budget(work = 150_000, heap = 0)
export func filter_above(
    input: read i16[],
    output: edit i16[],
    threshold: i16
) -> Result<usize, FilterError> {
    if (input.len > 4096) {
        return Result.err(FilterError.input_too_large);
    }

    if (output.len < input.len) {
        return Result.err(FilterError.output_too_small);
    }

    var written: usize = 0;

    for (var i: usize = 0; i < input.len; i += 1) {
        let value: i16 = input[i];

        if (value >= threshold) {
            output[written] = value;
            written += 1;
        }
    }

    return Result.ok(written);
}
```

`read` memberi akses baca tanpa menyalin buffer. `edit` memberi akses baca/tulis
eksklusif untuk durasi panggilan. Panjang buffer adalah bagian view, bukan pointer
mentah. Parameter scalar `threshold` diterima sebagai nilai immutable.

Pemanggilan dari Fermi menyebut akses tersebut:

```fermi
let samples: i16[4] = [2, 7, 1, 9];
var selected: i16[4] = [0, 0, 0, 0];
let result: Result<usize, FilterError> =
    filter_above(read samples, edit selected, 5);
```

Bagian hasil yang valid pada sukses adalah `selected[0..count]`, dengan `count`
dari `Result.ok`. Sisa buffer tidak dianggap hasil. Pada contoh ini, kedua error
validasi muncul sebelum penulisan. Bahasa tidak menjanjikan rollback otomatis
untuk setiap fungsi yang menerima `edit`.

## 3. Aturan sintaks yang konsisten

| Konsep | Bentuk kanonis | Aturan |
| --- | --- | --- |
| Deklarasi tetap | `let count: u32 = 4;` | Tipe eksplisit, nilai owning tidak dapat dimutasi |
| Deklarasi mutable | `var count: u32 = 4;` | Inisialisasi wajib sebelum pembacaan |
| Fungsi | `func f(x: u32) -> u32 { ... }` | Hasil selalu tertulis, termasuk `void` |
| Percabangan | `if (condition) { ... } else { ... }` | Kurung dan braces wajib |
| Loop penghitung | `for (var i: usize = 0; i < n; i += 1) { ... }` | Tiga bagian, dipisah `;`; tidak ada bentuk `for ... in` pada pilot |
| Loop kondisi | `while (condition) { ... }` | Kurung dan braces wajib; budget statis memerlukan bukti batas |
| Pemilihan varian | `match (value) { case ... { ... } }` | Semua varian harus ditangani; tidak ada fallthrough |
| Array owning | `u8[256]` | Panjang tetap adalah bagian tipe |
| View | `read u8[]`, `edit u8[]` | Akses berbeda dinyatakan pada tipe parameter |
| Indeks / subrange | `data[i]`, `data[start..end]` | Rentang setengah terbuka, selalu dibatasi |
| Modul / anggota | `std.io`, `io.println`, `sample.value` | Pemisah selalu `.` |
| Return | `return value;` | Tidak ada implicit last-expression return |
| Kontrak | `@budget(work = 100_000, heap = 0)` | Argumen bernama memakai `=` |

Pernyataan sederhana diakhiri `;`. Blok dan deklarasi fungsi/struct/enum tidak
memerlukan `;` setelah `}`. Tidak ada semicolon insertion. Assignment adalah
statement, bukan expression; `if (x = 3)` ditolak. Whitespace tidak menentukan blok;
formatter resmi menghasilkan empat spasi. Komentar `//` dan `/* ... */`; komentar
blok tidak bersarang. String literal UTF-8, identifier ASCII untuk pilot.

Operator aritmetika/comparison mengikuti prioritas C yang umum. Boolean memakai
`&&`, `||`, `!`; tidak ada alias keyword `and/or/not`. Tidak ada `++` atau `--`:
gunakan `+= 1`/`-= 1`. Ini membatasi bentuk operasi yang mempunyai efek samping.
`else if (...)` adalah satu-satunya singkatan blok kontrol.

## 4. Tipe, nilai, dan error

Target inti pilot: `bool`, `u8`, `i16`, `u32`, `i64`, `usize`, `void`, array tetap,
struct nominal, enum, dan `Result<T, E>` sebagai tagged value tanpa heap.
Jenis integer lain ditambahkan ketika diperlukan oleh komponen pembanding.
`usize` mengikuti target dan digunakan untuk panjang/indeks, bukan layout wire.

Tidak ada konversi implisit antarjenis angka. Literal memperoleh tipe dari konteks
jika nilainya muat; ini tidak mengizinkan konversi variabel bertipe lain secara
implisit. Konversi widening eksplisit melalui operasi yang ditetapkan spesifikasi;
konversi narrowing yang bisa gagal menghasilkan `Result`.

`let` dan `var` mengatur seluruh nilai owning, termasuk elemen array/field.
Parameter nilai immutable. Parameter `edit` secara eksplisit mengizinkan mutasi
data pemanggil; descriptor view tidak dapat diubah agar menunjuk buffer lain.
Aggregate besar tidak disalin secara tersembunyi: gunakan `read`/`edit` saat
memanggil, dan `copy(value)` jika salinan owning memang diinginkan. Biaya copy
dihitung berdasarkan ukuran elemennya.

Transfer nilai aggregate harus eksplisit: `copy(value)` membuat nilai terpisah,
`move(value)` memindahkan hak kepemilikan dan membuat sumber tidak dapat dibaca
sampai diinisialisasi kembali. Assignment aggregate bernama tanpa salah satu
operasi tersebut ditolak. Temporary hasil konstruktor/call dapat langsung menjadi
milik binding tujuan. Return aggregate bernama juga memakai `move` atau `copy`.
Pilot tidak mendukung partial move per field. Move adalah perpindahan ownership;
representasi inline mungkin tetap perlu transfer byte pada ABI, yang biayanya
tetap masuk analisis. Mutabilitas dan hak memindahkan ownership adalah dua aturan
berbeda: `let` melarang perubahan nilai, bukan transfer eksplisitnya.

Struct memakai field `name: Type;`; tidak ada inheritance. Konstruktor nilai
memakai argumen bernama, misalnya `Sample(tick = 12, value = 7)`. User-defined
operator overloading, implicit constructors, macros, reflection, dan general
metaprogramming tidak masuk pilot. `Result` disediakan prelude; user generics
menunggu kebutuhan yang terbukti.

Error domain seperti frame rusak dikembalikan melalui `Result`, wajib ditangani
atau diteruskan eksplisit. Tidak ada exception tersembunyi dan tidak ada `null`
implisit. Contoh penanganan:

```fermi
match (result) {
    case Result.ok(let count: usize) {
        io.println(count);
    }
    case Result.err(let reason: FilterError) {
        handle_error(reason);
    }
}
```

Contoh handler berada di fungsi host-facing yang mempunyai izin I/O; contoh ini
bukan penambahan I/O ke fungsi `filter_above` yang dibatasi budget.

## 5. Model memori yang sengaja dibatasi

Pilot memakai nilai owning dan view yang hanya hidup selama panggilan:

1. Pemanggil memiliki storage; `read`/`edit` tidak memindahkan ownership.
2. Banyak view `read` ke area sama diperbolehkan selama tidak ada penulis.
3. View `edit` eksklusif: area yang disentuh tidak boleh tumpang-tindih dengan view
   aktif lain yang membacanya atau menulisnya.
4. View dapat diteruskan ke helper dalam lingkup call; tidak dapat disimpan di
   global/struct, dikembalikan, atau ditangkap closure. Tidak ada lifetime generik
   pada tahap ini.
5. Pemanggil tidak boleh memindah, mengubah ukuran, atau menghancurkan storage
   selama akses aktif. Pilot tidak mempunyai resizing maupun user raw pointer.
6. Pemisahan subrange hanya diterima jika compiler membuktikan tidak overlap;
   jika tidak, API pemisahan yang memeriksa rentang dapat digunakan pada tahap
   berikutnya. Klaim disjoint dari pengguna tidak dipercaya begitu saja.

Ini merupakan aturan peminjaman yang lebih sempit, bukan cara menghindari masalah
alias/lifetime. Penolakan sebagian pola valid adalah trade-off yang harus diuji
pada komponen nyata. Dynamic owners/allocator bukan prasyarat rilis pertama.

View C yang diimpor berbeda: wrapper dapat memeriksa panjang, alignment,
overflow ukuran, dan overlap yang relevan, tetapi **tidak dapat membuktikan raw
pointer pemanggil masih valid atau tidak sedang ditulis thread lain**. Kontrak C
harus menyatakan kewajiban itu. Fermi bukan sandbox native untuk caller berbahaya.

## 6. Keselamatan tidak berubah karena optimasi

- Membaca nilai belum diinisialisasi, memakai nama tak dikenal, atau melanggar
  mutabilitas adalah error kompilasi.
- Overflow, pembagian nol, dan indeks tidak sah tidak menjadi undefined behavior
  Fermi. Compiler menolak kasus yang pasti salah dan menjaga pemeriksaan runtime
  ketika belum ada bukti statis.
- Penghapusan check membutuhkan bukti; `-O2` tidak berarti "safety off".
- Evaluasi kiri ke kanan. Argumen, indeks assignment, dan RHS mempunyai urutan
  tertulis. `&&`/`||` short-circuit. Compound assignment mengevaluasi target sekali.
- Pembagian integer membulatkan menuju nol; sisa mengikuti tanda dividend.
- Error domain `Result` dibedakan dari fault bahasa. Fault dipropagasikan menuju
  wrapper export sebagai status fault melalui lowering eksplisit; tidak ada
  exception C++ atau unwinding yang melewati ABI.
- Pada fault, output mutable mungkin sudah terisi sebagian. Caller wajib
  mengabaikannya kecuali API menyatakan aturan partial-result yang lebih kuat.

Detail status/fault adalah bagian ABI yang harus dibakukan sebelum backend.
Backend awal tidak boleh sekadar `abort()` dan tetap mengklaim isolasi fault host.
Stack exhaustion, bug compiler/runtime, dan pelanggaran host C tetap merupakan
batas keamanan yang perlu dilaporkan.

## 7. Kontrak resource: properti yang berbeda tetap terpisah

### Batas kerja statis

`@budget(work = N, heap = 0)` meminta pemeriksaan seluruh call graph yang reachable.
`work` menghitung operasi model Fermi yang diberi versi, bukan instruksi mesin,
waktu, konsumsi energi, atau biaya cloud. `heap = 0` melarang permintaan alokasi
heap transitif. Literal input 4096 pada contoh diperiksa oleh kode biasa;
compiler memanfaatkan early return itu untuk membatasi loop selanjutnya.

Model pertama mendefinisikan cost operasi scalar, branch, loop control, call,
dan transfer/copy nilai. Array dan helper tidak dianggap satu operasi konstan.
Join cabang memilih upper bound, bukan rata-rata. Work arithmetic pada analyzer
harus overflow-safe; overflow tidak boleh berubah menjadi bound kecil.

Call tanpa model tepercaya, rekursi yang tak dibatasi, atau loop yang belum dapat
dibuktikan menghasilkan error kontrak. Ketiadaan bukti tidak sama dengan program
tidak pernah berhenti. Fungsi biasa tanpa budget masih boleh mempunyai loop umum.
Tidak ada klaim menyelesaikan halting problem untuk seluruh bahasa.

### Batas yang ditegakkan runtime

Metering adalah jalur pengembangan terpisah, **bukan fallback diam-diam** saat
pembuktian statis gagal. Target lanjutannya adalah budget bersama untuk satu call
export, didebit di seluruh helper; pemanggilan fungsi tidak mereset jatah.
Kehabisan budget mengembalikan fault terstruktur ke host, bukan bukti bahwa fungsi
berhasil menyelesaikan pekerjaannya.

Sintaks metering belum dibakukan dan tidak masuk compiler pilot. Manifest
menyediakan klasifikasi `static`, `metered`, dan `unknown` untuk mencegah laporan
menyamakan properti tersebut. Check elimination lintas mode memerlukan semantik
charging yang dibakukan; tidak dijanjikan gratis.

### Memori

Laporan harus membedakan:

- buffer caller: ukuran dan hak akses;
- heap: jumlah byte alokasi transitif yang dianalisis, awalnya hanya kontrak nol;
- scratch: arena kerja eksplisit jika fitur ini kelak tersedia;
- storage lokal menurut IR;
- native stack pada target/backend tertentu.

`heap = 0` tidak berarti seluruh memori bernilai nol atau native stack terbukti
aman. Backend C tidak boleh melaporkan bound stack hanya dengan menjumlah field
AST. Stack certificate baru dapat tersedia jika backend/ABI dan semua callee
mempunyai data yang memadai; selain itu laporkan `unknown`.

### Efek dan kemampuan

Komputasi lokal dan perubahan melalui `edit` tampak dari signature. Efek eksternal
harus dideklarasikan, misalnya `@effects(io)`. Import `use std.io;` hanya membuat
nama tersedia, tidak memberi izin efek secara otomatis.

Fungsi dengan budget statis pilot tidak boleh memanggil I/O, blocking, atau foreign
code yang tidak dimodelkan. I/O dan allocator berada di library; enforcement atas
efek/akses tetap tanggung jawab compiler. Efek adalah pembatasan bahasa, bukan
pengganti izin proses OS.

## 8. Artefak dan pemeriksaan upgrade

Target build library menghasilkan:

```text
telemetry.o              kode native
telemetry.h              C interface dan kewajiban caller
telemetry.contract.json  hasil analisis serta asumsi
```

Manifest menyebut versi bahasa/compiler/model biaya, target, signature, akses,
efek, budget diminta, bound ditemukan, status bukti, asumsi native, dan identitas
artefak. Hash mengikat metadata ke artefak; hash bukan bukti kebenaran compiler.
Tidak disebut proof-carrying code sampai ada checker bukti independen yang nyata.

Pemeriksaan perubahan library harus menandai: penambahan efek, perubahan akses
read/edit, penguatan precondition, kenaikan bound melampaui kebijakan host,
perubahan partial-write/error behavior, serta model biaya yang tidak kompatibel.
Bound dari versi model biaya berbeda tidak boleh dibandingkan langsung.

Tidak semua perubahan biaya memecahkan ABI C. Karena itu perubahan kontrak dan
perubahan ABI dilaporkan terpisah. Target CLI:

```sh
fermi check telemetry.fe
fermi explain telemetry.fe
fermi build --library telemetry.fe --out-dir build
fermi contract diff old.contract.json new.contract.json
```

`check`, `explain`, dan `build --library` tersedia pada pilot. `contract diff`
belum diimplementasikan. Manifest saat ini memakai schema version 1.

## 9. Mengapa bisa berguna, dan kapan memilih yang lain

Sasaran Fermi: perubahan satu helper dari loop linear menjadi loop bersarang
memunculkan perubahan bound yang dapat ditolak kebijakan host; penambahan call
allocator tidak lolos sebagai perubahan internal kecil pada API `heap = 0`.
Developer memperoleh alasan dan jalur pemanggilan yang menyebabkan pelanggaran.

Jika kebutuhan utama adalah parser deklaratif yang terverifikasi, evaluasi
EverParse. Jika berupa codec hermetic dengan proof statis, evaluasi Wuffs. Jika
berupa aplikasi sistem besar dengan ekosistem luas, Rust/C++ tetap pembanding
kuat. Jika menjalankan kode pihak ketiga bermusuhan, gunakan isolasi yang sesuai,
misalnya proses atau Wasm. Fermi harus membuktikan manfaat pada celah operasionalnya.

## 10. Urutan implementasi yang direvisi

1. **Spesifikasi dan contoh:** tetapkan grammar, aturan view, error ABI, model
   biaya dan kasus compile-fail. Akhiri ketidakjelasan semantik sebelum fitur baru.
2. **Frontend C:** lexer/parser, AST, binding, tipe, mutabilitas, diagnostic;
   batas source/nesting/allocation compiler dan fuzzing.
3. **Nilai native:** IR dengan urutan evaluasi eksplisit, backend C, checks yang
   konsisten, Result/fault lowering. Unit test dan end-to-end C host.
4. **Buffer dan export:** view berbatas, analisis alias/lifetime yang sempit,
   wrappers, header, serta parser/filter yang mengolah input dari host nyata.
5. **Analisis kontrak:** bounds melalui guard/induction, efek/alokasi transitif,
   laporan dan manifest. Kasus yang belum terbukti tetap ditolak/unknown sesuai API.
6. **Eksperimen produk:** tiga komponen pembanding, benchmark setara, uji integrasi,
   dan validasi calon pengguna. Baru perluas bahasa jika hasil membenarkan.

Repository memisahkan `compiler/{support,frontend,analysis,ir,backend,driver}`,
`library/std`, `runtime`, `tests/{unit,cases,integration,fuzz}`, serta
`docs/{research,design,spec,architecture,development}`. Directory dibuat ketika
memiliki isi nyata. Build dan tes memakai C, Make, dan POSIX shell.

Compiler yang ditulis dalam C tidak otomatis cepat atau aman. Waktu compile,
jumlah copy, output native, dan safety diuji terpisah. Rilis pertama tetap
berstatus eksperimen sampai bukti penggunaan, audit, dan stabilitas mendukung
klaim yang lebih kuat.

## 11. Kasus penerimaan semantik

Tabel ini tetap menjadi kriteria proposal. Untuk hasil tes dan batas subset pilot,
lihat [verifikasi](../development/validation.md).

| Kasus | Perilaku wajib |
| --- | --- |
| `if ready { ... }` / `for var i ...` | Ditolak: bentuk kontrol memerlukan `(...)` |
| `if (ready) do ... end` | Ditolak: semua blok menggunakan `{ ... }` |
| Assignment pada elemen array `let` | Ditolak walaupun nama binding tidak diganti |
| `f(read data, edit data)` pada rentang overlap | Ditolak atau diperiksa pada batas import yang tidak dapat dianalisis; tidak diterima atas kepercayaan saja |
| Mengembalikan view ke storage lokal atau parameter | Ditolak pada model call-scoped pilot |
| Membaca aggregate setelah `move` | Ditolak sampai ada inisialisasi baru yang sah |
| Input filter `[2, 7, 1, 9]`, threshold 5, output capacity 4 | Sukses count 2, prefix output `[7, 9]` |
| Input filter lebih dari 4096 elemen | `input_too_large`, tanpa penulisan output |
| Capacity output lebih kecil dari panjang input | `output_too_small`, tanpa penulisan output |
| Input kosong, output kosong | Sukses count 0; tidak ada dereference storage kosong |
| Overflow aritmetika / indeks ilegal | Compile error jika pasti salah; selain itu fault terstruktur, checks tetap di build release |
| Mengganti helper linear dengan nested loop sampai budget terlampaui | Penolakan kontrak dengan jalur penyebab biaya |
| Menambahkan alokasi di callee `heap = 0` | Penolakan transitif walaupun caller tidak berubah |
| Call foreign tanpa summary tepercaya | Tidak menghasilkan klaim bound statis |
| Native stack belum dianalisis | Manifest `unknown`, bukan angka dari ukuran variabel sumber |
