#!/bin/sh
# Used only by executable integration tests; library tests instrument generated C directly.
exec "${CC:-clang}" -fsanitize=address,undefined "$@"
