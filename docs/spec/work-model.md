# Model biaya `fermi-work-1`

Model ini menghitung pekerjaan abstrak pada pohon bertipe, sebelum optimasi C.
Bukan instruksi mesin, WCET, jumlah byte stack, atau bukti keselamatan keseluruhan.
Model merupakan kontrak bernomor versi; perbandingan angka lintas versi memerlukan
analisis ulang. Pemeriksaan wrapper C berada di luar biaya fungsi Fermi.

## Satuan dan komposisi

Satu sel berarti satu nilai scalar, termasuk enum. View memiliki dua sel.
Array memiliki `length × cells(element)`. Struct menjumlah sel seluruh field.
Result memakai `1 + cells(value) + cells(error)`; ukuran ini bukan `sizeof(C)`.

- Literal/nama/operasi expression dasar: satu unit plus seluruh child expression.
  Expression aggregate menambahkan jumlah sel hasil untuk transfer nilainya.
- Call: satu unit, biaya seluruh argumen, dan biaya fungsi tujuan. Fungsi memasukkan
  biaya transfer parameter. Tidak ada specialization berdasarkan nilai argumen caller.
  Fungsi generik integer dianalisis per tipe konkret yang dipilih secara eksplisit.
- Literal array/struct menghitung setiap initializer. Array berulang `[value; N]`
  menghitung initializer sekali dan sel hasil sebanyak `N × cells(element)`. `copy` dan `move` termasuk
  biaya operand serta sel aggregate; nama aggregate sendiri juga dihitung.
- Deklarasi: satu + initializer + sel storage. Assignment: satu + target + nilai
  + sel yang ditulis. Return dan expression statement: satu + expression.
- `if`: satu + kondisi + maksimum kedua cabang. `match`: satu + subject + maksimum
  biaya body case beserta transfer payload. Match enum tanpa payload menambahkan
  satu unit per case sebagai bound konservatif untuk dispatch perbandingan.
  Short-circuit memakai jumlah kedua
  operand sebagai batas konservatif.
- Blok menjumlah semua statement. Early return tidak mengurangi jumlah biaya
  berikutnya; hasil sengaja konservatif.
- Loop penghitung berbatas N: `init + (N + 1) × condition + 1 +
  N × (body + step + 1)`. Cabang fault dapat berhenti lebih cepat; tidak menambah
  kerja model dibanding jalur yang sudah dibatasi.

## Pembuktian yang tersedia

Pengenal loop menerima counter unsigned dimulai literal nol, kondisi `i < limit`,
step `i += 1`, tanpa penulisan counter oleh body atau perubahan dependency limit.
Upper bound nilai berasal dari literal, rentang tipe, initializer/assignment,
aritmetika unsigned konservatif, dan guard `< <= > >=`. Guard branch yang return
membatasi jalur lanjutan. `&&` true dan `||` false dapat mempersempit kedua operand.

Fakta tentang outer binding yang ditulis body dilupakan sebelum menghitung body
berulang. Counter memiliki batas N. Nested loop menghitung perkalian biaya;
helper dianalisis transitif. `while`, loop nonkanonis, rekursi, atau arithmetic
analisis yang melampaui representasi menghasilkan `unknown`.

`UINT64_MAX` dicadangkan sebagai unknown. Operasi analisis tidak boleh wrap menjadi
angka kecil. `@budget(work = N, heap = 0)` ditolak bila hasil unknown atau melebihi
N. Fungsi tanpa anotasi boleh unknown. Heap nol mengikuti tidak adanya operasi
alokasi/foreign call pada fungsi yang tidak memiliki efek. Alokasi buffer dan
pelepasan resource menambahkan efek `memory` secara transitif. Fungsi dengan
efek tersebut memiliki bound work/heap unknown; anotasi budget ditolak dengan
`E_EFFECT`. Lihat [kontrak buffer](buffers.md).

`fermi explain` menampilkan bound tiap fungsi. Manifest menyebut model, requested
budget, proof status, computed bound, efek, parameter, dan asumsi. Native stack
selalu `unknown`; tidak disimpulkan dari banyaknya local variable.

## Input/output standar

Pemanggilan `std.io.write` dan `std.io.read_into` memiliki efek I/O dan batas kerja unknown. Efek ini
menjalar melalui seluruh call graph secara konservatif, termasuk cabang yang
mungkin tidak dijalankan. `@budget` yang mencapai I/O ditolak dengan `E_EFFECT`.
Manifest memakai `heap_bytes: null` untuk fungsi tersebut karena runtime eksternal
belum disertifikasi; fungsi tanpa efek tetap memakai aturan heap nol di atas.
Literal teks sendiri memakai storage statis dan menghasilkan descriptor view;
model tidak menghitung panjang byte sebagai penyalinan runtime.

Pembuktian overflow berada dalam [pass terpisah](range-proofs.md). Angka budget
tidak dipakai sebagai bukti penghapusan runtime check, dan biaya abstrak tetap
menghitung operasi sumber walaupun backend berhasil mengoptimasinya.

## Descriptor buffer yang dapat berubah

Parameter `edit Buffer<u8>` tidak memiliki efek alokasi hanya karena dipinjam.
Helper yang hanya membaca metadata dapat tetap memiliki heap nol. Pemanggilan
`mem.reserve`/`mem.resize` membawa efek memori transitif dan menolak sertifikat
heap nol. Analyzer memperlakukan `.len`/`.capacity` descriptor sebagai unknown;
guard metadata tidak menghasilkan bound loop tetap atau bukti rentang immutable.
Ini pilihan konservatif, bukan janji sertifikasi loop dinamis.

## Environment startup

`std.env.read_into` dan helper `std.config.usize_env` memiliki efek `env.read`
transitif. Bound work/heap tetap unknown; budget menolak akses environment dengan
`E_EFFECT`. `std.num.parse_usize` dan `std.num.usize_range` adalah fungsi Fermi
pure dengan sertifikat budget/heap nol tersendiri. Lihat [configuration](configuration.md).

## Process arguments and numeric output

std.args.count and std.args.read_into carry process.args. The effect propagates
through helpers and prevents @budget certificates, including for constant-time
count queries, because process access is outside the current pure model. Native
work and heap metadata remain unknown. The adapter does not explicitly allocate.

std.fmt.usize_into is ordinary Fermi with two fixed counting loops. It retains a
finite work certificate, heap zero and no effects. Calling it from an I/O function
does not make the caller pure; parsing/formatting and external access compose
through the existing conservative effect rules.

## Clock access

std.time.monotonic_ns has clock.read with unknown native work/heap metadata and
is rejected inside @budget functions. deadline_after and remaining_ns take
explicit integer samples, perform no clock query and retain pure certificates.
Abstract work bounds do not establish a wall-clock deadline or interrupt I/O.

## Unit-success Results

For Result<void, E>, void contributes zero payload cells. The tag and error enum
still contribute their existing cells. Unit success cases have no binding or
payload transfer cost; other case and expression rules remain unchanged. Functions
using this Result retain the effects of their actual operations and cleanup.

## Loop control

`break` and `continue` each cost one abstract unit. Canonical counting loops
retain the conservative full-iteration bound: break can shorten execution, and
continue still executes the for step. The analyzer includes all body statements
in its upper bound even when a jump skips some of them. A while loop still has
an unknown bound.

For loops with direct break/continue paths, variables assigned by the body or
step lose their bounds after the loop. A jump may bypass a later assignment that
would otherwise appear to restore an earlier bound. Nested loop jumps target
only their own loop. Resource cleanup keeps its existing transitive effects;
these operations do not receive pure finite certificates.

## Result propagation

`try(operand)` costs one unit plus the operand cost, the success payload's cells,
and the enclosing function Result's cells. Both transfer costs are included as a
conservative bound even though success and error take different paths. Void has
zero payload cells. Existing source constructs retain their `fermi-work-1` rules.
Propagation preserves operand and cleanup effects; native I/O and resource
operations do not gain a finite work certificate through try. Early error returns
do not reduce subsequent statement costs.

## Payload enum storage and dispatch

A payload enum contributes one tag cell plus the maximum payload cell count.
Constructor cost includes its operand and the aggregate result cells. Match counts
its subject, one unit per variant comparison, and the largest case including its
payload cells. Fieldless enums retain one cell. Cleanup effects union the effects
of all resource variants, propagating through nested enums and Result success
payloads. Native stack bounds remain unknown. Existing `fermi-work-1` rules for
previously available constructs remain unchanged.

## Buffer metadata operations

`std.buffer.truncate<T>` checks the requested length and updates the owner
descriptor without reading elements or allocating storage. Its native work cost
is one abstract primitive unit, independent of element size and buffer length.
Argument evaluation and call costs are added normally. `std.buffer.clear` and
`pop` compose this cost through their ordinary Fermi bodies. They have no tracked
external effects when operating on a borrowed buffer; allocation and owner
cleanup remain memory effects. `@effects()` permits explicit `edit` mutation and
does not mean referential purity. These bounds do not certify elapsed time,
native stack size, or erasure of removed bytes.
