# Read-only file streaming

filecat is a Fermi executable that copies one regular file to stdout using a
single 4096-byte owned buffer. The same storage first receives the CLI path,
then is reused after open returns; no path view is retained by the file owner.

```sh
make
./build/fermi build examples/tools/filecat/main.fe -o build/filecat -O2
./build/filecat library/std/fs.fe
```

Supply exactly one path, up to 4096 argument bytes. Paths are raw bytes; the tool
does not interpret options, expand patterns or treat '-' specially. A directory,
FIFO, device or missing path is rejected by the regular-file API. Symlinks are
followed. This is a local file tool, not a path sandbox.

main.fe separates CLI setup, acquisition, streaming and reporting. run consumes
an owner, creates a mutable binding for exclusive read calls and explicitly closes
it on both successful and failed streaming. A close failure after successful
streaming is reported. If streaming already failed, that error remains primary;
a secondary close error does not replace its diagnostic. All Results are handled.

| Exit | Meaning |
| --- | --- |
| 0 | EOF reached, output written and explicit close succeeded |
| 64 | Invalid argument count or unavailable/oversized path argument |
| 66 | Path cannot be opened as a regular file |
| 70 | Unexpected internal fallthrough |
| 71 | Startup context or buffer allocation unavailable |
| 74 | Reported read, write, close or diagnostic-output failure |

Output may be partial on I/O failure. The tool does not enforce a file-size limit,
verify immutable file contents, cancel storage waits or define new signal behavior.
The buffer capacity bounds its payload allocation, not process RSS.

Run sh tests/files.sh for native execution, byte comparisons across multiple
chunks, empty files, source ownership rejection, injected lifecycle failures,
independent object linkage and explicit close-error reporting. Write operations
and resource-containing application structs remain outside the current subset.
