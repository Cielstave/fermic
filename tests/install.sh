#!/bin/sh
# Verify both executable resource lookup and package staging.
set -eu
root=$(pwd)
work=$(mktemp -d "$root/build/install-test-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
make install prefix="$work/prefix" > "$work/install.log" 2>&1
mkdir "$work/client"
cp examples/cli/hello.fe "$work/client/main.fe"
(cd "$work/client" && "$work/prefix/bin/fermi" run --no-cache main.fe) > "$work/output"
printf 'Hello, world!\n' > "$work/expected"
cmp "$work/expected" "$work/output"
make install prefix=/usr DESTDIR="$work/package" > "$work/stage.log" 2>&1
test -x "$work/package/usr/bin/fermi"
test -f "$work/package/usr/share/fermi/library/std/io.fe"
test -f "$work/package/usr/share/fermi/runtime/io/file.c"
printf '%s\n' 'PASS: installed resource lookup and DESTDIR staging'
