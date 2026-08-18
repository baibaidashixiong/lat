#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

emulator=$1
source_file=$2
guest_root=${LATX_X86_64_SYSROOT:-/usr/gnemul/lat-x86_64}
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM

if [ "$(uname -m)" != loongarch64 ]; then
    echo "SKIP: the test requires a LoongArch host"
    exit 77
fi
if [ ! -d "$guest_root" ]; then
    echo "SKIP: x86_64 guest sysroot not found: $guest_root"
    exit 77
fi
if command -v clang-19 >/dev/null 2>&1; then
    clang=clang-19
elif command -v clang >/dev/null 2>&1; then
    clang=clang
else
    echo "SKIP: clang is required to build the x86_64 guest"
    exit 77
fi
if [ ! -f "$guest_root/usr/lib/libc.so.6" ]; then
    echo "SKIP: x86_64 guest libc not found: $guest_root/usr/lib/libc.so.6"
    exit 77
fi
if [ ! -f "$guest_root/usr/lib/libdl.so.2" ]; then
    echo "SKIP: x86_64 guest libdl not found: $guest_root/usr/lib/libdl.so.2"
    exit 77
fi

"$clang" --target=x86_64-linux-gnu -fuse-ld=lld -nostdlib -no-pie \
    -Wl,--build-id=none -Wl,-dynamic-linker,/usr/lib/ld-linux-x86-64.so.2 \
    -L"$guest_root/usr/lib" "$source_file" \
    -Wl,--no-as-needed -Wl,-l:libdl.so.2 -Wl,--as-needed -Wl,-l:libc.so.6 \
    -o "$workdir/dlopen-loaded-libc-x86"

set +e
LD_LIBRARY_PATH=/usr/lib LATX_AOT=0 LATX_KZT=1 \
    "$emulator" -L "$guest_root" "$workdir/dlopen-loaded-libc-x86"
ret=$?
set -e

if [ "$ret" -ne 0 ]; then
    echo "FAIL: guest dlopen with a host library path exited with $ret" >&2
    exit "$ret"
fi

echo "PASS: dlopen reused the loaded guest libc"
