#!/bin/sh
# run-isolated.sh SECONDS CMD...: runs a GTK 4 program in a headless weston
# with its own HOME and D-Bus session, so it never touches the user's data
cd "$(dirname "$0")/../.." || exit 1
secs=$1; shift
tmp=$(mktemp -d)
mkdir -p "$tmp/home" "$tmp/run"
chmod 700 "$tmp/run"
XDG_RUNTIME_DIR=$tmp/run weston --backend=headless --socket=wayland-iso \
	--width=1280 --height=800 --idle-time=0 >"$tmp/weston.log" 2>&1 &
weston=$!
while [ ! -S "$tmp/run/wayland-iso" ]; do sleep 0.1; done
env -i PATH="$PATH" HOME="$tmp/home" XDG_RUNTIME_DIR="$tmp/run" \
	WAYLAND_DISPLAY=wayland-iso GDK_BACKEND=wayland GSK_RENDERER=cairo \
	GSETTINGS_SCHEMA_DIR="${GSETTINGS_SCHEMA_DIR:-}" GTK_A11Y=none \
	dbus-run-session -- timeout "$secs" "$@"
echo "exit: $?"
kill $weston
rm -rf "$tmp"
