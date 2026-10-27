# Platform and build policy

Fermi uses a C11 compiler implementation and C code generation. Host and target
must currently use 64-bit `size_t` and eight-bit bytes. Checked arithmetic uses
Clang/GCC overflow builtins. The process driver requires POSIX APIs, including
`fork`, `exec`, `waitpid`, and filesystem permissions. No Termux paths are stored
in the source; resource paths are selected when building.

## Support status

| Platform | Architecture | Status |
| --- | --- | --- |
| Android / Termux | AArch64 | Locally exercised development host |
| Linux | x86-64, AArch64 | CI configured for Clang and GCC; results require a hosted run |
| macOS | ARM64 | Experimental compiler/unit/hello CI; file runtime unsupported |
| Windows via WSL2 | x86-64, AArch64 | Uses the Linux build; not locally verified |
| Windows native | Any | Unsupported; needs a process/filesystem platform adapter |
| Any 32-bit host or target | Any | Unsupported by current type/layout model |

The file runtime deliberately requires Linux close semantics. Do not remove this
restriction without specifying and testing descriptor ownership after `EINTR`.
On non-ELF systems the run cache falls back to ordinary compilation. The macOS
cache timestamp field adaptation permits the compiler driver to build; it does
not establish full runtime support.

## Build and verification

Install GNU Make and Clang (or GCC). Build with `make -j4`; select the host compiler
with `make CC=gcc`. `FERMI_CC=gcc` separately selects the compiler for generated C.
`make check` runs C unit/runtime tests. `make test` additionally requires Node.js
and runs the complete shell integration suite. Sanitizers and libFuzzer are
optional development dependencies; `make fuzz` requires Clang/libFuzzer.

`make help` lists entry points. `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, and `LDLIBS` are
caller-configurable. Changes to compiler flags and embedded resource roots
invalidate compiler objects. Generated files stay under `build/`.

## Installation

```
make install prefix="$HOME/.local"
make install prefix=/usr DESTDIR="$PWD/build/package"
```

The installed driver is built separately with final library/runtime paths. Staging
with `DESTDIR` does not change those paths. Source-tree builds continue to use
repository resources. Installation layouts follow the [GNU directory and staging
conventions](https://www.gnu.org/prep/standards/html_node/DESTDIR.html).
Installation is prefix-bound; moving an installed tree requires rebuilding with
its new prefix. Installation is not an atomic package upgrade.

Cross-compilation is not yet a supported CLI contract. A cross C compiler alone
does not validate target ABI, runtime behavior, or executable launch support.
