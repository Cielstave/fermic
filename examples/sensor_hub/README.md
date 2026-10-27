# Sensor hub: referensi aplikasi Fermi

Contoh yang bisa dikompilasi sekarang, bukan pseudocode fitur masa depan.
Inti aplikasi berada di [hub.fe](hub.fe): validasi paket, kalibrasi integer,
statistik batch, hysteresis, dan adapter ABI. [simulate.c](simulate.c) menjalankan
tiga kanal dengan buffer serta status alarm terpisah. Semua keputusan pengolahan
berada di Fermi; C menyediakan data sintetis, penyimpanan lintas batch, dan I/O.
Ini referensi komponen aplikasi, belum aplikasi industri lengkap.

## Menjalankan

Dari root repository:

```sh
make simulate
./build/fermi explain examples/sensor_hub/hub.fe
make test
make sanitize
```

Simulasi mengaktifkan alarm pada batch pertama, memulihkannya pada batch kedua,
dan memproses nilai negatif pada batch ketiga. Mengubah gain mengubah unit output;
nama kanal hanya label simulasi, bukan model fisik sensor nyata.

## Alur pemrosesan

1. Host memilih kanal dan menyediakan paket, buffer sampel, buffer laporan,
   konfigurasi kalibrasi/ambang, serta status alarm sebelumnya.
2. `process_batch` memvalidasi konfigurasi, struktur paket, dan kapasitas output.
3. `analyze_batch` mendekode setiap i16, menerapkan kalibrasi, menghitung statistik,
   dan memperbarui alarm lokal. Tidak ada alokasi heap.
4. `write_summary` menulis laporan setelah analisis selesai.
5. Host memperbarui status kanal hanya jika hasil `FERMI_OK`.

Paket: byte `70, 69, 1`, jumlah sampel u16 little-endian, lalu sampel i16
little-endian. Jumlah 1–4096; panjang paket harus tepat `5 + 2 * count`.
Paket kosong ditolak karena mean/min/max tidak terdefinisi pada contoh ini.

Kalibrasi: `raw * gain / divisor + offset`; pembagian membulat ke nol.
Gain dibatasi ±1.000.000, divisor 1–1.000.000, offset ±1.000.000.000.
Nilai absolut hasil paling besar 33.768.000.000; jumlah absolut 4096 sampel
paling besar 138.313.728.000.000, masih muat dalam i64. Tidak ada saturasi tersembunyi.
Mean absolute adalah rata-rata magnitudo, bukan RMS atau variance.

Alarm aktif ketika `value >= trigger`, pulih ketika `value <= release`.
Di antara kedua ambang, status sebelumnya dipertahankan. Syarat:
`-40.000.000.000 <= release < trigger <= 40.000.000.000`.
Urutan sampel bermakna; tidak ada pengurutan ulang atau deteksi paket duplikat.

## Kontrak output

`calibrated[0..count]` berisi sampel hasil kalibrasi. Buffer `report` minimal
9 elemen i64. Elemen di luar rentang yang ditulis tetap utuh.

| Indeks | Isi |
| --- | --- |
| 0 | Jumlah sampel |
| 1, 2 | Minimum, maksimum |
| 3, 4 | Mean, mean absolute, dibulatkan ke nol |
| 5 | Jumlah sampel dengan alarm aktif setelah transisi |
| 6, 7 | Jumlah aktivasi, pemulihan alarm |
| 8 | Alarm akhir: 0 atau 1 |

Struct `Calibration`, `Thresholds`, dan `Summary` menjaga kode inti tetap memakai
nama field. Layout indeks hanya ada di batas ABI karena export struct belum
didukung. Jangan memperlakukan layout ini sebagai ABI bahasa yang sudah stabil.

Semua domain error mempertahankan kedua buffer output. Error konfigurasi diperiksa
lebih dulu, kemudian paket, kemudian kapasitas. Penolakan internal pada awal
`analyze_batch` juga terjadi sebelum write. Fault bahasa pada umumnya tidak
menjamin rollback; contoh ini membatasi aritmetika dan indeks sebelum pemrosesan.
Host wajib menyediakan alokasi hidup, buffer tidak overlap, serta akses tanpa
konflik thread selama panggilan. Tidak ada validasi checksum atau autentikasi.

## Batas kerja dan pengujian

Analyzer saat ini menghitung 848.516 unit `fermi-work-1` untuk `process_batch`,
di bawah deklarasi 2.000.000. Ini batas biaya model source, bukan waktu CPU,
jaminan deadline, atau batas native stack. Tidak ada klaim performa dari simulasi.

`tests/integration/hub.c` memeriksa 2.000 batch acak terhadap referensi C,
4096 sampel pada ekstrem kalibrasi, negatif, gain nol, hysteresis tepat di ambang,
status lintas batch, semua varian domain error, overlap, dan sentinel output.
Suite dijalankan pada `-O0`/`-O2`; target sanitizer menginstrumentasi C hasil compiler.

## Pelajaran untuk pengembangan bahasa

Contoh ini sengaja tetap satu unit `.fe`: compiler belum memiliki import.
Pemisahan parser paket, algoritma, dan ABI saat ini melalui fungsi. Riset dan
prioritas lanjut ada di [catatan desain](../../docs/research/sensor-hub-reference.md).
