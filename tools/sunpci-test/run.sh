#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_binary=$(mktemp "${TMPDIR:-/tmp}/sunpci-test.XXXXXX")
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
"${CXX:-clang++}" -std=c++11 -Wall -Wextra -Werror \
    -Wno-missing-field-initializers -fsanitize=address,undefined \
    -I"$root/include" "$root/tools/sunpci-test/test.cpp" -o "$test_binary"
"$test_binary"
