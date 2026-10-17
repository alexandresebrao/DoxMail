#!/bin/sh
# gdb-test.sh TEST [TESTPATH]: backtrace of the first fatal warning of a
# doxmail-tests program, run inside a headless weston
cd "$(dirname "$0")/../.." || exit 1
runtime=$(mktemp -d)
XDG_RUNTIME_DIR=$runtime weston --backend=headless --socket=wayland-test \
	--width=1024 --height=768 --idle-time=0 >/dev/null 2>&1 &
weston=$!
while [ ! -S "$runtime/wayland-test" ]; do sleep 0.1; done
XDG_RUNTIME_DIR=$runtime WAYLAND_DISPLAY=wayland-test GDK_BACKEND=wayland GSK_RENDERER=cairo \
	timeout 120 gdb -q -batch -ex 'set debuginfod enabled off' -ex run -ex bt \
	--args "_build-gtk4/src/doxmail-tests/$1" ${2:+-p "$2"} 2>&1 |
	grep -v '^\[New\|^\[Thread\|GLib-GIO-DEBUG\|libthread_db' | grep -A25 'not ok\|signal SIG\|CRITICAL\|WARNING'
kill $weston
rm -rf "$runtime"
