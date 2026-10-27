# Native run cache

`fermi run` reuses native executables for unchanged default-backend programs.
The cache lives under `build/.fermi-run` in the current working directory and is
an optimization of [run](cli.md), not a package manager or frontend IR cache.

## Identity and validation

Every invocation still loads modules, lexes/parses/checks the program, analyzes
its effects, and emits the complete executable C source. Its SHA-256 digest
covers generated code from user modules, standard modules, embedded runtime
sources, and the executable entry wrapper. Comments or other source changes
that leave generated C identical need not cause recompilation. Invalid source
is rejected before a cached program can execute.

The cache key also covers optimization level, the shared native argument list,
host architecture, canonical Clang path, its device/inode/size and nanosecond
modification/change timestamps, and PATH. Loader environment values and metadata
of their named files/directories participate, including normal Termux
LD_LIBRARY_PATH and LD_PRELOAD settings. Program arguments are not build inputs;
the cached executable receives the current invocation's arguments and environment.

Only the default Clang backend is eligible. Explicit FERMI_CC, custom include or
library search variables, compiler configuration overrides, and similar build
environment overrides bypass caching. A script masquerading as the default Clang
is also excluded. Explicit `-o` uses ordinary executable publication instead.

This fingerprint assumes a coherent installed system toolchain. It does not
track every system header, linker binary, implicit configuration file, startup
object, or library as a separate dependency. After manual changes to those
inputs without a corresponding compiler identity change, remove
`build/.fermi-run` before the next cached run, or use `--no-cache`. This is not
a claim of complete system-level dependency tracking.

## Artifact ownership and publication

The build parent must be owned by the current user without group/world write
access. The cache root and entry directories must be private, owned directories,
not symlinks. Unavailable or unsafe cache roots fall back to temporary execution.

A cache entry contains `program` and its `sha256` digest. Both must be owned
regular files without group/world write access; the program must be executable.
Every hit verifies the binary digest. Corrupt or symlinked programs are rebuilt.

Misses compile in a fresh private staging directory. Only complete entries are
published by directory rename. Concurrent builders may use an already-published
valid entry. Repair and cleanup remove only named cache artifacts; unknown files
prevent directory removal. No recursive deletion follows untrusted paths.
There is no automatic eviction; deleting the cache discards rebuildable output.
`make clean` also removes it with the rest of build output.

## Escape hatch and measurement

```sh
fermi run program.fe             # Reuse eligible unchanged native output
fermi run program.fe --no-cache  # Always compile temporary output; do not reuse/store
```

A cold cache still invokes Clang. Warm runs avoid that backend's startup,
compilation/linking time and peak memory, but still pay frontend, hashing and
process-launch costs. Benchmark cold builds, warm runs, and direct executable
launches separately; this optimization does not change program execution speed.

`tests/run-cache.sh` checks reuse, optimization and module invalidation, source
errors after a prior build, binary corruption and symlinks, parallel publication,
current runtime arguments, explicit output and unsupported-environment bypass.
