# Command-line interface

Fermi is a native compiler. The CLI currently operates on source files and local
modules; it does not resolve packages or require a package manifest.

```sh
fermi
fermi --help
fermi help run
fermi --version
fermi check program.fe
fermi explain program.fe
fermi build program.fe -o program -O2
fermi build --library component.fe --out-dir output
fermi run program.fe -- first "two words"
fermi test tests.fe -O2
```

In a repository checkout, use `./build/fermi` after `make`. The bare `fermi` name
assumes the executable is already on PATH. To select the checkout build in the
current shell, run `export PATH="$PWD/build:$PATH"` from the repository root; this
also takes precedence over an older installed compiler. No arguments prints the command
summary and exits successfully. `-h` and `-V` are help/version aliases. Supported
command aliases are `b` (build), `c` (check), `r` (run), and `t` (test). Each command supports
`--help`; `fermi help COMMAND` prints the same command-specific help.

## Build and run

```
fermi run [FILE.fe] [-o PROGRAM] [-O0|-O2] [--no-cache] [-- ARGS...]
```

Without FILE, use `main.fe` in the current directory. There is no search for
`src/main.fe`, package discovery, or directory change. Entry validation, module
resolution, safety checks, and native generation use the same pipeline as build.
The default optimization is `-O2`; `-O0` is also supported. Every invocation checks
the source and modules. Eligible unchanged programs reuse native output from the
[run cache](run-cache.md), skipping Clang compilation and linking.

With `--no-cache`, a custom backend, or an unavailable cache, create a private
directory with `mkdtemp` under nonempty `TMPDIR`,
or the platform's `P_tmpdir` fallback. The generated executable and its known
parent directory are removed after build failure, launch failure, or collected
program termination. There is no recursive removal of arbitrary program-created
files. Cleanup is not guaranteed if the driver itself is forcibly terminated.
A missing or unwritable temporary root produces `E_RUN`.

With `-o`, the output parent must exist. Retain the executable after the program
finishes, including when it exits unsuccessfully. Build publication uses the
existing atomic replacement behavior: a failed build preserves earlier output
and never launches it. `--library` and `--out-dir` are invalid for run.

## Process behavior

The program inherits the working directory, environment, stdin, stdout, and
stderr. Run adds no stdout banners, so pipelines can consume program output.
The compiled executable is resolved to an explicit absolute path and launched
with `execv`, without shell evaluation or a PATH search.

Arguments following the first `--` are program arguments only. Empty arguments,
spaces, punctuation, and leading hyphens are preserved. A forwarded `--help`,
`-O0`, or `--diagnostic-format=json` does not become a compiler option. Fermi
programs access these through `std.args`, which excludes argv[0].

Run returns the program's exit status. Signal termination returns `128 + signal`.
During execution, the driver ignores terminal SIGINT/SIGQUIT while the foreground
child uses the caller's original dispositions; the driver then collects status,
cleans temporary output, and restores its dispositions. The child remains in
the inherited process group. There is no job control, daemon supervision, or
forwarding of signals sent exclusively to the driver PID.

An execution-status pipe distinguishes failure to execute from a program that
legitimately exits with 127. Cached executables remain available after execution;
temporary executables are removed. Launch and cleanup errors produce `E_RUN`. Native
backend failures retain `E_TOOLCHAIN`; source and entry failures retain their
existing diagnostic codes. Runtime faults remain the program's own stderr.

## Diagnostics and options

Compiler options must precede `--`. `--diagnostic-format=text|json` controls
compiler diagnostics on stderr, including usage and launch errors. It does not
reformat a running program's output. Invalid usage exits with status 2; build or
launch failures normally exit with status 1. Program exit codes can coincide
with these values, so callers should inspect stderr when distinguishing failures.

Unsupported commands and options are rejected. Help lists implemented commands
only. Repeated output, optimization, library, or diagnostic-format options are
rejected rather than silently combining configurations.

`tests/run-command.sh` covers help, aliases, default entry, literal argument
forwarding, standard streams, working directory, explicit output preservation,
temporary cleanup, runtime faults, backend/launch failures, signal termination,
and a legitimate exit status of 127. `tests/run-cache.sh` covers cache reuse,
invalidation, integrity checks, concurrent publication and bypass behavior.

## Native tests

`fermi test [FILE.fe] [-O0|-O2]` discovers local `testName() -> bool` and `test_*() -> bool` functions.
The default source is `tests.fe`. Tests run sequentially in a temporary native
executable, with individual outcomes and a final count. See the
[test runner contract](test-runner.md) for fault handling, CLI limits and isolation.
