#!/usr/bin/env bash
# Runs the GTK 4 port checks inside a headless weston, so they get frame
# callbacks like on a real screen without opening windows on the desktop.
#
#   doxmail/run-tests.sh [build-dir] [test args...]
set -euo pipefail

build=${1:-_build-gtk4}
shift || true
cd "$(dirname "$0")/.."

ninja -C "$build" src/doxmail-tests/all >/dev/null

runtime=$(mktemp -d)
trap 'kill "$weston_pid" 2>/dev/null; rm -rf "$runtime"' EXIT

XDG_RUNTIME_DIR=$runtime weston --backend=headless --socket=wayland-test \
	--width=1024 --height=768 --idle-time=0 >"$runtime/weston.log" 2>&1 &
weston_pid=$!

for _ in $(seq 50); do
	[ -S "$runtime/wayland-test" ] && break
	sleep 0.1
done
[ -S "$runtime/wayland-test" ] || { cat "$runtime/weston.log" >&2; exit 1; }

status=0
for t in "$build"/src/doxmail-tests/test-*; do
	[ -x "$t" ] || continue
	echo "== $(basename "$t")"
	XDG_RUNTIME_DIR=$runtime WAYLAND_DISPLAY=wayland-test GDK_BACKEND=wayland \
		GSK_RENDERER=cairo timeout 120 ${DOXMAIL_TEST_WRAPPER:-} "$t" "$@" || status=1
done
exit $status
