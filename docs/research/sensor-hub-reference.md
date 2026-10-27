# Referensi sensor hub: keputusan dan kebutuhan bahasa

Tanggal riset: 23 Oktober 2026. Hasil implementasi:
[`examples/sensor_hub/hub.fe`](../../examples/sensor_hub/hub.fe).

## Dasar pemilihan kasus

Zephyr memisahkan pengolahan sensor dari driver dan menyerahkan protokol host
kepada aplikasi. Subsystem sensing mendukung batching serta representasi integer
atau fixed-point. Referensi Fermi mengikuti pembagian tanggung jawab tersebut:
komponen komputasi menerima buffer, host mengurus perangkat dan transport.
Sumber: [Zephyr Sensing Subsystem](https://docs.zephyrproject.org/latest/services/sensing/index.html).

API Read and Decode Zephyr meminta decoder stateless dan menyimpan informasi
konversi dalam buffer. Ini mendukung eksplorasi fungsi pemrosesan dengan input dan
state eksplisit, tetapi contoh Fermi bukan implementasi API Zephyr dan belum
terintegrasi dengannya.
Sumber: [Zephyr Sensors](https://docs.zephyrproject.org/latest/hardware/peripherals/sensor/index.html).

## Keputusan lokal, bukan klaim dari sumber

Paket maksimum 4096 sampel, formula kalibrasi rasional integer, rentang aritmetika,
serta aturan hysteresis merupakan pilihan contoh ini. Tidak ada kebutuhan float,
allocator, jaringan, atau scheduler dalam kernel pemrosesannya. Memaksakan semuanya
ke bahasa inti tidak membantu kasus ini.

Hysteresis dibawa lintas batch oleh host, sehingga tidak ada state global atau
alokasi tersembunyi. Validasi lengkap sebelum write memberikan kontrak domain-error
atomicity. Kontrak ini ditopang struktur program dan tes, belum merupakan efek
transaksi yang dibuktikan compiler.

## Hambatan yang benar-benar muncul

1. **Batas ABI masih terlalu datar.** Core memakai struct, tetapi wrapper harus
   menerima scalar terpisah dan mengembalikan array berindeks. Prioritas desain:
   export record dengan layout/version eksplisit dan header C bernama field.
2. **Komposisi masih satu file.** Import dan visibilitas modul dibutuhkan ketika
   decoder dipakai ulang. Desain harus mendefinisikan identitas simbol, siklus
   dependensi, dan penggabungan kontrak; menambahkan keyword saja tidak cukup.
3. **Bukti batas tidak melewati parameter.** Helper mengulang guard `count <= 4096`
   agar analyzer mengenali batas. Perlu kontrak rentang parameter yang diperiksa
   saat call sebelum menghapus pemeriksaan berulang.
4. **State bertipe belum dapat dipinjam.** `read`/`edit` hanya menerima view scalar.
   Borrow record perlu aturan field overlap dan lifetime sebelum dipakai untuk
   kanal dengan state yang lebih kompleks.

Urutan investigasi berikutnya: record ABI, modularitas, lalu kontrak rentang.
Ini temuan dari contoh, belum komitmen sintaks atau fitur yang telah selesai.
Biaya runtime pemeriksaan juga tetap perlu diukur; contoh ini tidak menghapus
hasil benchmark pilot yang masih lebih lambat daripada referensi C.

## Lingkup yang belum disimulasikan

Belum ada transport perangkat, timestamp, kehilangan/duplikasi paket, checksum,
konkurensi, penjadwalan deadline, persistensi, atau deployment perangkat nyata.
Tes numerik dan batas tidak menggantikan validasi sistem tersebut. Contoh ini
menguji apakah Fermi dapat menulis komponen bounded yang berguna dan dapat dipanggil
aplikasi C, bukan membuktikan kesiapan production seluruh bahasa.
