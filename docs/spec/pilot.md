# Fermi 0.2 pilot: spesifikasi yang diimplementasikan

Dokumen ini membatasi proposal v0.2 ke fitur compiler pilot. Target adalah proses
native 64-bit dengan byte 8-bit. Status eksperimental; bukan spesifikasi ABI stabil
untuk seluruh roadmap. Satu file `.fe` menjadi satu unit sumber. Ekstensi compiler untuk proyek multi-file
dan executable dijelaskan dalam [modul](modules.md); diagnostik dalam
[diagnostics](diagnostics.md). Buffer heap berukuran tetap dijelaskan dalam
[owned byte buffers](buffers.md); batas resource pada dokumen itu melengkapi aturan
aggregate dan view di bawah.

Kontrak efek per fungsi dijelaskan dalam [effect contracts](effect-contracts.md).
Atribut budget/effects masing-masing paling banyak satu per fungsi.

## Grammar dan nama

```ebnf
file       = { enum | struct | function } ;
enum       = "enum" name "{" variant { "," variant } [","] "}" ;
variant    = name ["(" type ")"] ;
struct     = "struct" name "{" { name ":" type ";" } "}" ;
function   = { budget | effects } ["pub"] ["export"] "func" name [generic] "(" [parameters] ")" "->" type block ;
generic    = "<" name ":" ("integer" | "signed_integer" | "unsigned_integer") ">" ;
effects    = "@effects" "(" [effect { "," effect }] ")" ;
effect     = name ["." name] ;
budget     = "@budget" "(" "work" "=" integer "," "heap" "=" "0" ")" ;
parameters = name ":" type { "," name ":" type } ;
type       = name ["[" integer "]"] | ("read" | "edit") name "[]"
           | "Result" "<" type "," name ">" | "Buffer" "<" "u8" ">" | "edit" "Buffer" "<" "u8" ">" ;
block      = "{" { statement } "}" ;
statement  = block | simple ";" | "return" [expression] ";"
           | ("break" | "continue") ";"
           | "if" "(" expression ")" block ["else" (block | statement_if)]
           | "while" "(" expression ")" block
           | "for" "(" simple ";" expression ";" update ")" block
           | "match" "(" expression ")" "{" { case } "}" ;
case       = "case" "Result" "." ("ok" | "err")
             "(" ["let" name ":" type] ")" block
           | "case" [name "."] name "." name ["(" "let" name ":" type ")"] block ;
simple     = ("let" | "var") name ":" type ["=" expression] | update ;
update     = expression [("=" | "+=" | "-=" | "*=" | "/=" | "%=") expression] ;
```

`statement_if` berarti produksi `if` di atas; singkatan `else if` tidak berlaku
untuk statement lain. Enum dan struct tidak boleh kosong. Tipe nominal dideklarasikan
sebelum dipakai; fungsi boleh dipanggil sebelum definisinya. Scope bersifat leksikal;
duplikasi binding dalam satu scope ditolak. Nama tipe dan fungsi memiliki namespace
global; fungsi tidak boleh memakai nama tipe atau intrinsic.

Identifier ASCII `[A-Za-z_][A-Za-z_0-9]*`, maksimum 128 byte. Integer desimal memakai
underscore hanya di antara digit. Lexer memahami string dengan escape `\n`, `\r`,
`\t`, `\"`, `\\`, dan literal bertipe `read u8[]` dengan storage statis immutable. Komentar `//` dan
`/* ... */` tidak bersarang. NUL ditolak. Kolom diagnostic dihitung dalam byte.

Expression mendukung literal integer/bool, nama, `(...)`, literal array `[a, b]`,
call, `try(expression)`, field/varian `.`, indeks `[i]`, slice `[start..end]`, dan unary `!`, `+`, `-`, `~`.
Urutan prioritas binary dari rendah: `||`, `&&`, `|`, `^`, `&`, `== !=`,
`< > <= >=`, `<< >>`, `+ -`, `* / %`; semuanya left-associative. Unary lebih kuat daripada seluruh binary; postfix lebih kuat daripada unary.
Perbaikan 27 Oktober 2026: `-a + b` berarti `(-a) + b`; tulis `-(a + b)` untuk menegasikan jumlah.
Semua evaluasi kiri ke kanan; boolean short-circuit. Assignment hanya statement,
dan target compound assignment dievaluasi sekali. Operasi bit unsigned dan shift dijelaskan dalam [binary](binary.md).
Tidak ada increment, implicit return, atau implicit semicolon pada pilot.
`break` dan `continue` memerlukan loop yang melingkupinya; lihat
[loop control](loop-control.md).

## Tipe, kepemilikan, dan hasil

Tipe dasar: `void`, `bool`, `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`,
dan `usize`; lihat [integer types](integer-types.md). Integer bertanda
memakai rentang two's-complement; `usize` 64-bit pada target pilot. Literal bertipe
kontekstual bila tersedia, selain itu `i64`. Tidak ada konversi variabel implisit.
`widen<T>(value)` diterima hanya ketika seluruh rentang sumber muat pada T.
`narrow<T>(value)` menghasilkan `Result<T, CastError>` dengan varian
`CastError.out_of_range`. Enum tidak dikonversi diam-diam ke integer.

`let` wajib diinisialisasi dan immutable sampai elemen/field. `var` boleh belum
bernilai saat deklarasi, tetapi pembacaan memerlukan inisialisasi pada semua jalur.
Parameter nilai immutable. Kedua cabang `if` digabung konservatif; loop mungkin
berjalan nol kali. Return wajib ada di semua jalur fungsi non-void. Kode setelah
return, break, atau continue tanpa jalur lanjutan ditolak.

Array panjang tetap dan struct adalah nilai owning. Struct memakai konstruktor
`Pair(left = 1, right = 2)` dengan seluruh field dalam urutan deklarasi. Transfer
aggregate bernama harus `copy(binding)` atau `move(binding)`, termasuk return.
Move menonaktifkan sumber sampai reinitialisasi `var`; partial move tidak didukung.
Transfer temporary tidak memerlukan wrapper. Move tidak menjamin penghapusan
semua transfer byte oleh backend.

Result tidak boleh dibuang atau ditimpa sebelum ditangani. `match` mengonsumsi
binding Result dan wajib memiliki satu `ok` serta satu `err`; payload memiliki
binding bertipe. Khusus `Result<void, E>`, success memakai `Result.ok()` dan
`case Result.ok()` tanpa payload; lihat [unit Result](unit-results.md).
Forwarding memakai return constructor/temporary atau `move`.
Pilot menolak Result sebagai field struct/elemen array agar pemeriksaan hasil
wajib tidak hilang di dalam aggregate. `Result` boleh mempunyai payload owning;
fungsi generik mendukung constraint integer dan data nonresource; generic struct
dan parameter resource belum tersedia. Lihat [integer generics](integer-generics.md)
dan [data generics](data-generics.md). `match` enum tanpa payload dijelaskan dalam
[enum matches](enum-matches.md), [scalar matches](scalar-matches.md), dan [strings](strings.md).
[File read-only](files.md) menambahkan owner opaque `fs.File` serta borrow
`edit fs.File`, dengan transfer dan cleanup yang mengikuti aturan resource.
Deklarasi `pub native struct File;` hanya diterima dalam library `std.fs` tepercaya.

View elemen hanya parameter `read T[]` atau `edit T[]`, dengan T data nonresource.
Parameter khusus `edit Buffer<u8>` meminjam descriptor eksklusif untuk growth;
kontrak lengkap serta `.capacity` dijelaskan dalam [buffer](buffers.md). Call menuliskan
hak akses: `process(read input, edit output)`. View tidak dapat disimpan dalam
binding owning, struct, array, atau dikembalikan. Descriptor dan `.len` immutable.
`edit` membutuhkan storage mutable dan eksklusif. Dua slice langsung dari binding owner yang sama
diterima bersama hanya bila rentang literal terbukti disjoint. Pembuktian
disjoint untuk slice bersarang belum tersedia. Parameter berbeda
sudah memiliki kewajiban disjoint dari caller. Penggunaan owner yang dipinjam di
argumen nilai lain atau ekspresi batas view lain ditolak konservatif; hitung
nilai tersebut sebelum call. Owner view yang berbeda tidak mengecualikan ekspresi
batasnya dari pemeriksaan ini.
Ekspresi indeks dan batas slice tidak boleh mengonsumsi owner storage yang
sedang diakses. Aturan ini juga berlaku pada array tetap.

## Fault dan batas implementasi

Indeks/slice ilegal, overflow, pembagian nol, dan jumlah shift ilegal menghasilkan fault. Kasus konstan
langsung yang dikenali ditolak saat check; yang belum terbukti tetap diperiksa
saat runtime, termasuk di `-O2`. Pembagian membulatkan ke nol. `MIN / -1` dan
`MIN % -1` menghasilkan overflow fault. Tidak ada abort atau unwinding lintas ABI.
Fault dapat meninggalkan edit buffer sebagian terisi; domain error mengikuti
kontrak API komponen.

Batas compiler: sumber 1 MiB, arena 64 MiB, 131071 token non-EOF, 32768 syntax
allocations, nesting 128, rantai binary per tingkat 64, 128 fungsi, 16 parameter,
512 binding per fungsi, 1024 tipe, 128 field/varian, 65536 sel per aggregate.
Batas membatasi input compiler, bukan menjamin native stack. Input/output minimum
tersedia melalui [std.io](io.md), dengan efek yang dilacak compiler.
I/O lainnya, allocator pengguna, foreign call umum, metering, dan concurrency
belum tersedia. Sintaks yang belum didukung harus gagal, bukan diabaikan.

## Compound arithmetic assignments

`+=`, `-=`, `*=`, `/=`, and `%=` accept integer targets and operands of the
same type. The target must be mutable and initialized. Fermi evaluates the target
address once, reads its value, evaluates the right operand, and performs the
checked operation before storing the result. A fault prevents the final assignment
store; effects already performed while evaluating either operand remain.

Division truncates toward zero. A signed remainder has the dividend's sign or
is zero. Division and remainder by zero produce fault 3; signed minimum divided
or reduced modulo -1 produces overflow fault 2. A literal zero divisor is rejected
with `E_CONST`, consistently with ordinary `/` and `%` expressions.

Integer conditions are not converted to booleans. `if`, `while`, and `for`
conditions require `bool`; incompatible integer literals produce `E_TYPE`, while
out-of-range integer literals in integer contexts produce `E_RANGE`.

The regression suite `tests/control-flow.sh` runs native code at `-O0` and `-O2`
for nested loops, `else if`, exhaustive Result branches in either order,
short-circuit fault suppression, function returns, and single target evaluation.

## Contextual integer literals

A bare integer literal, or a literal wrapped in unary `+`, `-`, or `~`, can acquire
its type from the other operand of a binary operation. For example, both
`value > -1` and `-1 < value` are valid when `value` is `i32`. Parentheses do not
change this rule. The shift count remains `usize`, independently of the left
operand's type.

The unary operation still follows the inferred type's rules: `-1 < unsigned_value`
is invalid, as is `~0 == signed_value`. Signed minimum literals are supported;
negating a signed minimum again produces a checked overflow fault. Literals that
do not fit their contextual integer type are rejected with `E_RANGE`.

This rule does not convert typed bindings or function results. Comparing `i16`
and `i32` values still requires an explicit conversion. Without a contextual type,
integer literals default to `i64`. This is local literal typing, not general
inference for arbitrary arithmetic expression trees. Runtime evaluation remains
left to right; examining the other operand's type does not evaluate it.

## Literal arguments and executable entry

A string literal can satisfy a `read u8[]` parameter without an explicit `read`
keyword. Its storage remains static and immutable. Other views and owned arrays
require explicit access; mutable access is never inferred. Executable entry may
use `func main() -> void`; normal completion or `return;` exits with status zero.
Use `main() -> i32` when the program needs an explicit process status.

## Propagasi Result

`try(expression)` menghasilkan payload sukses atau mengembalikan error dari
fungsi yang melingkupinya. Fungsi tersebut harus mengembalikan Result dengan
tipe error yang sama persis. Binding Result dikonsumsi seperti pada match;
resource tetap dibersihkan pada jalur keluar. Lihat [kontrak try](error-propagation.md).

## Payload enums

Enum variants can carry one explicit owning payload type. Construction, consuming
matches, nested resource cleanup, representation limits, and supported payload
types are specified in [payload enums](payload-enums.md).
[Resource struct fields](resource-structs.md) are supported; resource arrays remain
unavailable. Non-resource aggregate fields and elements
can be copied explicitly with `copy(place)`; partial moves remain unsupported.

Type names may also be declared through transparent [type aliases](type-aliases.md). The `type` keyword is reserved.

Struct parameters also admit call-scoped [read/edit borrows](struct-borrows.md). These references cannot be stored or returned.
