# Riset bahasa pemrograman untuk Fermi

Tanggal kajian: **23 Oktober 2026**. Bentuk: kajian sumber primer dan dokumentasi,
bukan survei pengguna Fermi atau benchmark implementasi. Tujuannya menentukan
masalah yang layak diselesaikan sebelum membakukan bahasa.

## Ringkasan keputusan

Hipotesis yang paling layak diuji adalah **bahasa untuk komponen native pemroses
data dengan kontrak sumber daya yang dapat diperiksa dan dibawa melintasi batas
library**. Pengguna awal adalah pengembang yang memasukkan parser, validator,
atau transformasi buffer ke aplikasi C/C++ yang sudah berjalan.

Hipotesis ini belum membuktikan adanya pasar untuk bahasa baru. Keamanan memori,
ownership, efek, batas kerja, dan C interoperability masing-masing sudah memiliki
pendahulu kuat. Fermi harus memenangkan pekerjaan nyata melalui integrasi,
keterbacaan, diagnostic, dan keterlacakan biaya. Jika keunggulan itu dapat dicapai
lebih murah dengan tooling Rust/C++ atau Wuffs, proyek harus menerima temuan itu.

## Metode dan batas bukti

Sumber dipilih dari dokumentasi implementasi, publikasi peneliti pembuat teknologi,
survei penyelenggara, dan laporan adopsi langsung. Peringkat popularitas digunakan
sebagai konteks ekosistem, bukan ukuran kualitas atau ramalan pemenang.

**Label dalam kajian:**

- **Temuan:** dinyatakan langsung oleh sumber.
- **Inferensi:** implikasi yang kami tarik untuk desain Fermi.
- **Hipotesis:** harus dibuktikan lewat prototipe, pengukuran, atau pengguna.

Survei tidak mempunyai sampel atau pertanyaan yang sama. Persentase antar-survei
tidak boleh dijumlahkan. Dokumentasi membuktikan rancangan/fitur yang dinyatakan,
bukan otomatis performa, ketiadaan bug, atau kemudahan penggunaan. Tidak ada
wawancara pengguna dan tidak ada pengujian performa bahasa dalam kajian ini.
Dokumentasi `master` bisa berubah; eksperimen berikutnya harus mengunci versinya.

## 1. Apa yang benar-benar dibutuhkan sekarang?

### 1.1 Ekosistem lama tetap besar; migrasi menyeluruh mahal

**Temuan.** Survei Stack Overflow 2026 mencatat penggunaan C++ 24,3%, C 21,8%,
Rust 18,0%, serta Python 58,0% dari 14.099 responden pada pertanyaan bahasa.
Angka ini menggambarkan responden pertanyaan tersebut, bukan pangsa seluruh
industri. Metodologinya melaporkan 30.903 respons valid secara keseluruhan;
jumlah jawaban berbeda menurut bagian. [Data bahasa](https://survey.stackoverflow.co/2026/technology/data),
[metodologi](https://survey.stackoverflow.co/2026/methodology).

**Inferensi.** Jalur adopsi yang masuk akal adalah mengganti satu komponen dan
memanggilnya dari aplikasi yang ada. Header C, aturan buffer, dan hasil error
lebih mendesak daripada framework aplikasi baru.

### 1.2 Keamanan memori merupakan kebutuhan operasional

**Temuan.** Panduan NSA/CISA Juni 2025 mendorong adopsi bahasa memory-safe untuk
mengurangi kelas kerentanan memori. Ini adalah panduan, bukan dasar mengklaim
semua penggunaan C/C++ dilarang. [Pengumuman NSA/CISA](https://www.nsa.gov/Press-Room/Press-Releases-Statements/Press-Release-View/Article/4223298/nsa-and-cisa-release-csi-highlighting-importance-of-memory-safe-languages-in-so/).

Google melaporkan proporsi kerentanan memory-safety Android turun di bawah 20%
pada data 2025, serta hasil positif pada perubahan Rust. Itu hasil dalam konteks
Android, dengan komposisi, umur, dan pemilihan kode tertentu; bukan eksperimen
acak yang membuktikan angka yang sama untuk setiap proyek.
[Laporan Android, 13 November 2025](https://blog.google/security/rust-in-android-move-fast-fix-things/).

**Inferensi.** Fermi memerlukan model alias, lifetime, overflow, dan batas buffer
yang tertulis. Nama keyword dan pemeriksaan beberapa indeks tidak cukup.

### 1.3 Produktivitas juga berarti waktu kompilasi, diagnostic, dan debugging

**Temuan.** Survei Rust 2025 yang diterbitkan Maret 2026 masih mengidentifikasi
waktu kompilasi dan penggunaan storage sebagai persoalan produktivitas, serta
kekhawatiran kompleksitas bahasa. Penyelenggara sendiri mengingatkan bahwa sekitar
7.000 respons tidak boleh digeneralisasi terlalu jauh.
[State of Rust 2025](https://blog.rust-lang.org/2026/03/02/2025-State-Of-Rust-Survey-results/).

Survei khusus performa compiler menunjukkan pengalaman build sangat bervariasi;
sebagian pengguna puas dan sebagian terhambat. Sekitar 45% dari kelompok yang
berhenti memakai Rust menyebut waktu kompilasi sebagai salah satu alasan—bukan
45% seluruh pengguna Rust. [Compiler performance survey](https://blog.rust-lang.org/2025/09/10/rust-compiler-performance-survey-2025-results/).

**Inferensi.** Grammar kecil, pemeriksaan cepat, error dengan lokasi dan sebab,
serta build yang mudah direproduksi masuk persyaratan awal, bukan tambahan akhir.

### 1.4 AI meningkatkan pentingnya validasi, bukan menghapusnya

**Temuan.** Stack Overflow 2026 melaporkan 48% responden pada pembahasan trust
mempercayai AI ketika jawabannya mudah divalidasi. GitHub Octoverse 2025 mencatat
pertumbuhan TypeScript dan perubahan penggunaan bahasa; hubungan kausal antara
AI, tipe, dan kualitas kode tidak dapat dibuktikan hanya dari aktivitas GitHub.
[Survei AI](https://survey.stackoverflow.co/2026/ai),
[Octoverse 2025, diperbarui Februari 2026](https://github.blog/news-insights/octoverse/octoverse-a-new-developer-joins-github-every-second-as-ai-leads-typescript-to-1/).

**Inferensi.** Fermi sebaiknya mudah diperiksa manusia maupun alat: sintaks konsisten,
tipe eksplisit, diagnostic terstruktur, formatter tunggal, dan kontrak yang dapat
diuji. Tidak ada bukti bahwa membuat bahasa baru otomatis membuat hasil AI lebih
benar; bahasa baru justru mempunyai lebih sedikit contoh dan tooling.

### 1.5 Biaya komputasi dan memori harus dinyatakan dengan jujur

**Temuan.** Wasmtime mempunyai fuel deterministik dan epoch interruption, dengan
konsekuensi berbeda terhadap overhead dan determinisme. Dokumentasi API juga
membedakan eksekusi Wasm dari call host; budget tidak otomatis membatasi operasi
host yang blocking. [Interruption](https://docs.wasmtime.dev/examples-interrupting-wasm.html),
[Config](https://docs.wasmtime.dev/api/wasmtime/struct.Config.html).

**Inferensi.** Batas operasi abstrak, batas waktu nyata, dan keselamatan memori
adalah tiga properti berbeda. Fermi tidak boleh mengemas satu angka `work`
seolah-olah membuktikan semuanya.

## 2. Peta pendekatan yang sudah ada

Kolom peluang adalah **penilaian desain kami**, bukan hasil benchmark komparatif.

| Pendekatan | Yang sudah diselesaikan / kekuatan | Konsekuensi bagi Fermi |
| --- | --- | --- |
| C/C++ + Clang | Integrasi native; Safe Buffers dan analisis efek `nonblocking`/`nonallocating` | Larangan alokasi atau blocking saja bukan pembeda baru. Bandingkan dengan C++ yang memakai alat ini. |
| Rust | Ekosistem sistem, ownership, API aman, FFI | Jangan membuat klaim umum "lebih aman/mudah/cepat". Buktikan keunggulan pada pekerjaan khusus. |
| Zig | Allocator eksplisit dan mode kompilasi yang memberi kontrol atas checks | Kontrol biaya saja sudah tersedia; Fermi menetapkan checks aman tetap aktif dalam build release. |
| Go | Produktivitas service dengan runtime/GC yang terus berkembang | Ketiadaan GC bukan bukti otomatis lebih cepat. Hindari mengejar backend service umum. |
| Embedded Swift | Subset untuk perangkat terbatas dan peningkatan C interop | "Bahasa modern untuk embedded" terlalu umum sebagai alasan proyek. |
| Mojo / ISPC | Ownership dan komputasi heterogen; SIMD/SPMD dan integrasi native | GPU/SIMD bukan medan pertama yang realistis bagi compiler kecil ini. |
| Koka | Efek bertipe dan handlers | `@effects` merupakan penerapan gagasan lama; efek bukan fitur eksklusif Fermi. |
| SPARK | Kontrak, flow analysis, pembuktian sifat program/terminasi | Jangan mengklaim kekuatan formal yang belum dimiliki. Ukur beban anotasi dan cakupan bukti. |
| Wuffs | Library pemroses format tidak tepercaya, output C, pemeriksaan safety statis | Pembanding terdekat. Fermi harus menunjukkan manfaat yang melebihi mengganti sintaks. |
| EverParse | Generator parser zero-copy terverifikasi dari deskripsi format | Untuk parser deklaratif, generator khusus bisa lebih tepat daripada bahasa imperatif baru. |
| RaML / AARA | Inferensi statis batas penggunaan resource | Perhitungan biaya otomatis bukan penemuan baru; analisis praktis harus membatasi bentuk yang didukung. |
| Wasmtime | Isolasi guest, fuel, interruption dan resource controls | Pilihan lebih tepat bila kebutuhan utamanya menjalankan kode pihak ketiga yang tidak dipercaya. |
| Move/Sui | Pemakaian budget komputasi/storage dalam transaksi | Metering bukan konsep baru dan tidak perlu membawa model blockchain ke Fermi. |
| Cmajor | Bahasa dan model eksekusi khusus pemrosesan DSP | Audio real-time bukan ruang kosong yang perlu diisi dengan bahasa generik baru. |

Sumber primer untuk baris-baris tersebut:

- [Clang Safe Buffers](https://clang.llvm.org/docs/SafeBuffers.html) dan
  [Function Effect Analysis](https://clang.llvm.org/docs/FunctionEffectAnalysis.html).
  Dokumentasi Clang yang dibaca bersifat in-progress; versi compiler yang
  digunakan pada pembanding harus diverifikasi saat eksperimen.
- [Rust FFI](https://doc.rust-lang.org/nomicon/ffi.html). Foreign code dan validitas
  pointer tetap menjadi batas kepercayaan; menghubungkan bahasa aman ke C tidak
  otomatis mengamankan caller C.
- [Zig 0.15.2 reference](https://ziglang.org/documentation/0.15.2/#Build-Mode).
  Versi ini digunakan sebagai referensi yang terkunci, bukan klaim versi terbaru.
  ReleaseSafe dan ReleaseFast mempunyai kebijakan pemeriksaan berbeda.
- [Go 1.26](https://go.dev/blog/go1.26) dan
  [Green Tea GC](https://go.dev/blog/greenteagc). Perbaikan GC menegaskan bahwa
  performa bergantung workload dan layout; tidak cukup membandingkan label GC.
- [Embedded Swift](https://www.swift.org/blog/embedded-swift-improvements-coming-in-swift-6.3/).
- [Mojo ownership](https://mojolang.org/docs/manual/values/ownership/) dan
  [ISPC](https://ispc.github.io/ispc.html).
- [Koka](https://www.microsoft.com/en-us/research/project/koka/).
- [SPARK contracts](https://docs.adacore.com/spark2014-docs/html/ug/en/source/how_to_write_subprogram_contracts.html)
  dan [loop reasoning](https://docs.adacore.com/spark2014-docs/html/ug/en/source/loop.html).
- [Wuffs](https://github.com/google/wuffs). Wuffs secara eksplisit membatasi diri
  pada library komputasi tanpa syscall/allocator dan menjelaskan biaya anotasi
  bukti keselamatannya.
- [EverParse, USENIX Security 2019](https://www.microsoft.com/en-us/research/publication/everparse/)
  dan [proyek saat ini](https://github.com/project-everest/everparse).
- [Resource Aware ML](https://www.raml.co/) dan
  [paper Automatic Amortized Resource Analysis](https://arxiv.org/abs/2304.13627).
- [Wasmtime interruption](https://docs.wasmtime.dev/examples-interrupting-wasm.html).
- [Sui gas](https://www.docs.sui.io/develop/transaction-payment/gas-in-sui).
- [Cmajor language reference](https://cmajor.dev/docs/LanguageReference).

Dokumentasi eBPF juga ditinjau, tetapi beberapa halaman kernel yang ditemukan
masih memiliki penjelasan historis tentang larangan loop. Kajian ini tidak memakai
bagian itu untuk menyimpulkan batas verifier Linux modern.

## 3. Kebutuhan beberapa tahun ke depan

Rentang pertimbangan: 2027–2031. Ini **skenario**, bukan prediksi pasar yang pasti.

| Skenario | Keyakinan | Dasar dan implikasi |
| --- | --- | --- |
| Adopsi memory safety berlangsung bertahap di sistem campuran | Tinggi | Panduan dan kasus Android mendukung arah ini; utamakan C ABI dan migrasi per komponen. |
| Kode yang dihasilkan alat membutuhkan validasi dan provenance yang baik | Menengah–tinggi | Aktivitas AI sudah nyata; kebutuhan fitur khusus Fermi masih hipotesis. Sediakan pemeriksaan dan metadata yang dapat diotomasi. |
| Audit dependency mempertimbangkan kemampuan serta perubahan perilaku | Menengah | Transparansi komponen semakin diperhatikan; kontrak resource dapat membantu, tetapi belum terbukti diminta pembeli. |
| CPU/GPU/accelerator makin beragam | Menengah–tinggi | Mojo dan ISPC menunjukkan investasi nyata; jangan membangun backend GPU sebelum workload memerlukannya. |
| Latensi ekor, memori, dan biaya pemrosesan tetap penting | Tinggi untuk workload tertentu | Fuel, analisis resource, dan pekerjaan runtime menunjukkan kebutuhan; unit `work` tetap tidak sama dengan uang atau energi. |
| Bahasa baru akan menggantikan mayoritas C++, Rust, atau Python | Tidak didukung | Tidak menjadi asumsi bisnis atau teknis Fermi. |

Transparansi dependency mempunyai dukungan institusional, misalnya
[panduan bersama SBOM 2025](https://www.nsa.gov/Press-Room/Press-Releases-Statements/Press-Release-View/Article/4292020/nsa-cisa-and-others-release-a-shared-vision-of-software-bill-of-materials-sbom/).
Manifest kontrak Fermi yang diusulkan berbeda dari SBOM dan tidak menggantikannya.
Tidak ada klaim kepatuhan hukum dari desain ini.

## 4. Memilih masalah, bukan daftar fitur

Alternatif yang dievaluasi:

1. **Pengganti umum C++/Rust.** Ditolak: lingkup, ekosistem, dan alasan migrasinya
   terlalu besar dibanding bukti kebutuhan yang ditemukan.
2. **Bahasa GPU/AI baru.** Ditunda: toolchain/hardware luas dan banyak solusi kuat;
   tidak ada workload pengguna khusus yang membenarkan investasi awal.
3. **Parser generator saja.** Tidak dipilih: EverParse dan generator lain merupakan
   pembanding kuat; sebagian pekerjaan target membutuhkan transformasi imperatif
   setelah parsing, bukan hanya deskripsi format.
4. **Komponen native berkontrak resource.** Dipilih sebagai eksperimen: bisa
   diintegrasikan kecil, diuji pada data adversarial, dan dibandingkan secara adil.

Pengguna awal yang dituju: tim dengan parser atau transformasi buffer terbatas,
komponen dipanggil sangat sering, dan aplikasi host yang tidak akan ditulis ulang.
Kasus awal: frame telemetri, validasi record biner, dan filter batch sensor.
Bukan plugin dari penerbit bermusuhan; untuk itu isolasi proses/Wasm tetap relevan.

**Pekerjaan yang ingin dibantu:** "Saya perlu menambahkan pemroses buffer native,
membatasi kemampuan dan biaya satu panggilan, menangani input salah, lalu
meng-upgrade dependency tanpa kehilangan batas tersebut."

## 5. Hipotesis pembeda yang dapat digagalkan

Fermi mengusulkan satu alur kerja: kode familiar → kontrak diperiksa → library
native + header C + manifest kontrak → pemeriksaan kompatibilitas saat upgrade.
Metadata memisahkan tipe, akses baca/tulis, efek, batas input, biaya model sumber,
dan asumsi foreign code. Memori kerja, native stack, dan heap tidak dicampur.

Nilai yang diharapkan adalah kemudahan mengoperasikan kombinasi tersebut, bukan
penemuan masing-masing elemennya. Tidak ditemukan bukti yang cukup untuk
mengklaim kombinasi ini unik secara menyeluruh.

Mengapa bukan library biasa? Library bisa memberi buffer aman dan allocator
terbatas, tetapi pemeriksaan semua jalur, call graph, dan perubahan efek memerlukan
analisis kode atau runtime. Mengapa bukan plugin analyzer Rust/C++? Itu alternatif
serius: prototipe Fermi harus mengalahkannya dalam kejelasan aturan, diagnostic,
dan integrasi, dengan biaya adopsi yang masuk akal.

## 6. Eksperimen penerimaan sebelum memperluas bahasa

Angka berikut **target produk yang diusulkan**, bukan hasil penelitian.

### Prototipe pembanding

Bangun tiga komponen kecil: parser frame telemetri, validator record dengan panjang
field dari input, dan filter/transformasi batch. Sediakan implementasi pembanding
C dengan checks setara serta Rust; gunakan Wuffs/EverParse bila modelnya cocok.
Sertakan versi yang memakai tooling Clang agar pembanding tidak sengaja dilemahkan.

Ukuran yang dicatat:

- Correctness dan error behavior pada input valid serta malformed.
- Fuzzing, sanitizers, batas waktu dan memori kompilasi.
- Latensi median/p95/p99, throughput, ukuran binary, peak memory.
- Waktu clean/incremental build, lokasi diagnostic, jumlah anotasi manual.
- Waktu menambahkan library ke host C dan mengganti versinya.
- Regresi yang sengaja disisipkan: loop kuadrat, alokasi tersembunyi, ukuran input
  meningkat, perubahan akses buffer, dan call yang blocking.

Hasil performa harus menyebut target CPU, compiler/flags, dataset, jumlah run,
dan variasi. P95/p99 hasil pengukuran bukan batas waktu terburuk yang terbukti.

### Validasi kebutuhan

Cari 3–5 calon pengguna dengan komponen nyata, bukan hanya meminta penilaian
sintaks. Ukur apakah mereka dapat mengintegrasikan satu komponen dan memahami
penolakan kontrak tanpa bantuan perancang. Wawancara dan perekrutan belum dilakukan.

Target keputusan awal: tidak ada bug correctness yang diketahui pada korpus;
tidak ada hidden allocation di jalur `heap = 0`; pemeriksaan menangkap seluruh
regresi kontrak yang disisipkan; overhead median ditargetkan paling banyak 15%
terhadap pembanding aman pada workload terpilih. Bila meleset, dokumentasikan
trade-off dan revisi desain; angka ini bukan janji universal.

### Kapan mengubah atau menghentikan arah ini

- Tooling bahasa yang ada memberi hasil setara dengan biaya adopsi lebih rendah.
- Anotasi bukti lebih sulit dipelihara daripada kode komponen.
- Kontrak terlalu longgar untuk menemukan regresi yang relevan.
- Integrasi C membutuhkan copy/adapter yang menghilangkan manfaat performa.
- Tidak ada pengguna dengan masalah nyata setelah eksperimen awal.

Keputusan saat ini: lanjutkan **eksperimen bahasa terfokus**, bukan klaim bahwa
bahasa baru sudah terbukti diperlukan. Desain turunannya ada di
[proposal Fermi v0.2](../design/fermi-v0.2.md).
