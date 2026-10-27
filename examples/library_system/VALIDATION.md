# Execution record

Validated on Termux, Linux AArch64, 2026-10-27, using the repository compiler.

- Application: 13 Fermi files, 1,369 total lines; 1,296 nonblank,
  noncomment lines. Tests and documentation are excluded.
- Native suite: 11 tests passed at both optimization levels.
- Differential suite: four seeded journals, 4,096 catalog commands each;
  complete reports match the independent C oracle at `-O0` and `-O2`.
- Boundary tests: exactly 1 MiB of blank input and exactly 16,384 commands
  accepted; exceeding either limit rejected without publishing a report.
- Failure tests: incomplete command, repeated spaces, out-of-range number,
  CRLF, unknown operation, embedded NUL, missing input, missing arguments,
  and existing output rejected with the expected exit status.
- Supplied circulation journal: 28 commands, 26 accepted, two rejected;
  three loans, two returns, one renewal, one loss; 102 fee units charged
  and paid. Final report has one state line and 28 audit entries.
- C formatting and local documentation links checked.

Reproduce with `sh tests/library-system.sh`. Integration is registered in
`make test` and `make sanitize`. The randomized oracle covers catalog policies;
it does not independently prove circulation policies or all possible programs.

The focused suite also passed with AddressSanitizer and UndefinedBehaviorSanitizer
on the compiler and generated executables at both optimization levels. Logs:
`build/library-system-tests.log` and `build/library-system-sanitize.log`.

After adopting repeated array initialization, the current application has
1,300 lines (1,227 nonblank/noncomment lines). The original 1,369-line count
above records the version before removing 69 lines of repeated initializers.
