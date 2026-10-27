# Paket telemetri dan C ABI pilot

## Format paket

| Offset | Isi |
| --- | --- |
| 0–1 | Byte ASCII `FE` (`70`, `69`) |
| 2 | Versi `1` |
| 3–4 | Jumlah sampel unsigned 16-bit little-endian |
| 5 dst. | Sampel signed 16-bit little-endian |

Jumlah maksimum 4096. Panjang wajib tepat `5 + 2 × count`. Nol sampel valid;
trailing bytes ditolak. Parser tidak bergantung alignment atau endian host.
Urutan error: header terpotong, magic salah, versi tidak didukung, jumlah berlebih,
payload terpotong, panjang berlebih, kapasitas keluaran kurang. Semua pemeriksaan
ini mendahului write. Setiap pola bit payload valid sebagai sampel i16.

`parse_samples` mengembalikan jumlah sampel pada sukses. Hanya prefix sebanyak itu
yang ditulis. Pada domain error, seluruh output tidak berubah. Input tidak berubah.
Lihat [implementasi Fermi](../../examples/telemetry/telemetry.fe).

## C ABI v1

Header hasil build mendeklarasikan fungsi berikut dengan nama parameter bernomor:

```c
fermi_parse_samples_result fermi_parse_samples(
    const uint8_t *packet, size_t packet_len,
    int16_t *output, size_t output_len);
```

Struct return memiliki `uint32_t status`, `uint32_t code`, dan `size_t value`.
Pada sukses, `value` adalah count; selain sukses, `value` nol. Enum domain pada
header memakai macro `FERMI_ParseError_*`, berurutan dari nol sesuai source.

| Status | Arti code |
| --- | --- |
| `FERMI_OK` (0) | Nol; value valid |
| `FERMI_DOMAIN_ERROR` (1) | Varian enum error API |
| `FERMI_FAULT` (2) | 1 bounds, 2 overflow, 3 division by zero, 4 invalid shift |
| `FERMI_INVALID_ARGUMENT` (3) | 1 null nonempty, 2 size/address overflow, 3 alignment, 4 overlap, 5 enum invalid |

Argumen diperiksa sebelum fungsi Fermi: null hanya untuk panjang nol; perkalian
ukuran tidak overflow; alignment sesuai elemen; rentang alamat tidak overflow;
read/edit dan edit/edit tidak overlap. Wrapper tidak dereference empty buffers.

Caller wajib menyediakan alokasi yang benar-benar hidup, panjang yang sesuai,
representasi scalar valid, dan tidak melakukan akses thread yang berkonflik.
Angka pointer yang terlihat masuk akal bukan bukti alokasi valid. Kontrak ini bukan
sandbox terhadap host C berbahaya. Fault berbeda dari domain error; bahasa tidak
menjanjikan rollback universal atas perubahan sebelum fault.

## Artefak

Build menyediakan `.o`, `.h`, `.contract.json`, serta `.c` untuk pemeriksaan.
Manifest schema 1 menyertakan hash SHA-256 object dan snapshot source yang dibaca,
backend target, signature, rights read/edit, biaya, status bukti, dan asumsi host.
Hash mengikat identitas, bukan membuktikan compiler benar. File dipersiapkan dalam
direktori sementara; compiler C dijalankan tanpa shell. Manifest dipublikasikan
terakhir. Publikasi empat file belum merupakan transaksi filesystem atomik;
konsumen harus memeriksa hash bila membaca sambil build berjalan.
