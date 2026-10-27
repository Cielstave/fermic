# Modul lokal dan executable native

Status: tersedia pada compiler pengembangan. Ekstensi ini mempertahankan mode
library pilot; menyediakan I/O standar dan [alokasi byte buffer](buffers.md).

## Import dan visibilitas

```fermi
use math.statistics;

func main() -> i32 {
    let values: i32[2] = [10, 20];
    return statistics.total(read values);
}
```

`use math.statistics;` membaca `math/statistics.fe` relatif terhadap direktori
entrypoint. Untuk memilih file secara langsung, gunakan path dalam tanda kutip:

```fermi
use "../shared/codec.fe";          // Alias: codec
use "../shared/codec.fe" as wire; // Alias eksplisit: wire
```

Path file relatif terhadap file `.fe` yang memuat pernyataan `use`; contoh `../shared/codec.fe` mengasumsikan pemanggil berada di subdirektori proyek. Alias default
berasal dari nama file tanpa ekstensi `.fe`. Path impor wajib relatif dan setelah
resolusi symlink harus tetap berada di root proyek, yaitu direktori entrypoint.
Karena itu `..` dapat menunjuk direktori saudara selama hasil akhirnya tetap di
root tersebut. Import dotted tetap relatif ke root entrypoint. Semua import harus
mendahului deklarasi. Tidak ada pencarian implisit melalui working directory,
variabel environment, registry, atau lokasi induk. `std` dicadangkan;
`std.io`, `std.mem`, `std.env`, `std.num`, `std.config`, `std.binary`, `std.args`,
`std.fmt`, `std.time`, `std.fs`, `std.bits`, dan `std.text` dimuat dari library
bawaan compiler, bukan direktori proyek.
Lihat [kontrak I/O](io.md) dan [buffer](buffers.md).

Fungsi dan tipe nominal bersifat privat. `pub func`, `pub struct`, dan `pub enum`
membuka akses melalui alias; misalnya `statistics.Summary` dan
`statistics.Error.empty`. Field struct dan varian enum mengikuti visibilitas tipe.
Atribut mendahului modifier: `@budget(...) pub func ...`.

`export` membuka simbol pada C ABI dan tidak otomatis membuat fungsi `pub`.
Nama export C harus unik dalam seluruh proyek. Nama yang berbenturan dengan
simbol runtime internal (misalnya `file_read`) atau tipe hasil export lain
(misalnya `inspect` dan `inspect_result`) ditolak dengan `E_ABI`. Helper privat
boleh memiliki nama sama di modul berbeda. Alias import tidak boleh berbenturan dengan binding,
deklarasi lokal, alias lain, atau nama prelude. Tidak ada wildcard atau re-export.

Setiap file canonical diparse sekali. Dua jalur dependensi ke file sama memakai
identitas tipe yang sama. Siklus import ditolak dengan rantai dependensinya.
Symlink yang terdeteksi keluar dari root proyek ditolak. Source entrypoint dan
modul impor harus berupa file reguler; FIFO ditolak tanpa menunggu penulis. Ini
bukan sandbox filesystem terhadap proses lain yang mengubah directory secara
bersamaan.

Batas proyek: 64 modul, total sumber 1 MiB, arena bersama 64 MiB, 128 fungsi,
dan 1024 tipe. Batas node/nesting pilot tetap berlaku per unit. Nominal type lokal
harus dideklarasikan sebelum digunakan; deklarasi fungsi dapat dirujuk maju.

Contoh import bertingkat tersedia di [local_paths](../../examples/cli/local_paths/main.fe). Jalankan dengan `./build/fermi run examples/cli/local_paths/main.fe`.

## Build executable

`fermi run FILE.fe [-- ARGS...]` membangun dan menjalankan executable dalam satu
perintah; tanpa FILE memakai `main.fe`. Kontrak output sementara, status proses,
dan opsi CLI ada di [CLI](cli.md).


```sh
./build/fermi build examples/cli/exit_status.fe -o build/example
./build/example
./build/fermi build examples/cli/modules/main.fe -o build/modules-example
./build/modules-example
```

Entry point wajib `func main() -> void` atau `func main() -> i32` tanpa parameter atau `export`, di file
entrypoint. Fungsi bernama `main` di modul impor tidak menjadi entrypoint.
Argumen pengguna tersedia melalui [std.args](process-arguments.md), tanpa
mengubah signature entry Fermi. Entry `void` yang selesai normal menghasilkan status 0.
Entry `i32` mengembalikan status eksplisit. Nilai 0–255 menjadi exit status proses. Nilai lain dan fault bahasa menghasilkan
pesan stderr serta exit status 1. Lokasi source untuk fault runtime belum tersedia;
lokasi source yang dijanjikan diagnostik saat ini berlaku untuk error kompilasi.

Compiler membuat staging directory di sebelah target dan memublikasikan executable
melalui satu rename hanya setelah backend berhasil. Direktori induk harus ada.
Kegagalan sebelum publikasi mempertahankan executable lama. Argumen toolchain
menggunakan argv tanpa shell. `FERMI_CC` tetap satu executable, bukan command string.

## Library dan manifest

Mode `build --library` juga memuat import. Batas kerja dihitung melewati pemanggilan
antarmodul. Manifest menambahkan daftar `modules`, masing-masing dengan nama dan
SHA-256 snapshot source; entrypoint bernama `<entry>`. Hash object tetap dicatat.
Hash ini tidak membuktikan bahwa compiler atau filesystem tidak dimodifikasi.

Nama enum C dari entrypoint tetap memakai `FERMI_Type_variant`. Enum modul impor
memakai `FERMI_module_<hex nama modul>_Type_variant` untuk menghindari benturan.
Awalan nama tipe `module_` pada entrypoint dicadangkan untuk namespace tersebut.
ABI ini masih eksperimental. Nama nominal impor pada manifest mencantumkan modul.

## Tipe i32

`i32` adalah integer bertanda 32 bit dengan rentang −2.147.483.648 hingga
2.147.483.647. Aritmetika, unary negation, pembagian, dan remainder tetap diperiksa,
termasuk `MIN / -1`. Konversi widening memakai `widen`, sedangkan narrowing
menghasilkan `Result<..., CastError>`. C ABI memakai `int32_t`.

## Library startup yang tersedia

`std.env`, `std.num` dan `std.config` kini tersedia dari library tepercaya bersama
`std.io` dan `std.mem`. Project tidak dapat menggantikannya dengan file shadow.
Native hanya tersedia untuk signature bawaan yang divalidasi; parser/range dan
komposisi konfigurasi diimplementasikan dalam Fermi. Lihat [configuration](configuration.md).
