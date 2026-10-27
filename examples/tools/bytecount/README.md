# Bounded byte counter

An executable Fermi application that counts stdin bytes under a configured input
limit. It uses the standard libraries directly; no application-specific C host is
required. main.fe validates the CLI and processes input. report.fe formats numeric
output and reports fixed diagnostics without echoing raw arguments.

```sh
make
./build/fermi build examples/tools/bytecount/main.fe -o build/bytecount -O2
printf 'Fermi\n' | ./build/bytecount 100
# 6
```

Supply exactly one decimal MAX_BYTES argument, from zero to usize maximum. Invalid
CLI input is rejected before allocating the input buffer or reading stdin. A zero
limit accepts only empty input. Leading zeros are accepted within the decimal
parser's 20-byte input limit. Results are canonical decimal followed by newline.

The application owns one 4096-byte input buffer and reads chunks until EOF. It
checks count against limit - total before addition, so accounting cannot wrap.
Payload storage stays fixed while total input can span many chunks. The limit
bounds accepted input; the process can consume up to one additional chunk before
detecting excess. It does not bound native stack, allocator overhead, RSS or time
spent waiting for a producer that never closes stdin.

| Exit | Meaning |
| --- | --- |
| 0 | EOF reached within the limit; count written |
| 64 | Invalid argument count, length or decimal value |
| 65 | Input exceeds MAX_BYTES |
| 70 | Internal formatting invariant failed |
| 71 | Argument context or input allocation unavailable |
| 74 | Input, output or diagnostic write failure |

On input or limit failure, no count is printed. An output failure may have already
written a numeric prefix, following std.io semantics. Numeric output and newline
are separate checked writes, not a transactional logging interface. The owned
buffer is released on return and language faults.

Tests cover multiple input chunks, exact/exceeded limits, empty input, invalid and
oversized arguments, unreadable stdin, unwritable stdout/stderr and injected
allocation failure. Run sh tests/arguments.sh or the complete make test suite.
This is a synchronous command-line tool, not a daemon or task scheduler.
