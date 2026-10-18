#!/bin/bash
# run-evolution.sh [--gdb] [SECONDS [EVOLUTION ARGS...]]: installs the GTK 4 build into
# _inst-gtk4 (configured with that prefix, camel_providerdir and
# edsuimoduledir inside it) and runs evolution there through
# run-isolated.sh; with --gdb, prints a backtrace on a crash; SHOT=FILE.png
# takes a screenshot (see run-isolated.sh); the first-run assistant and the
# default mailer prompt stay away unless FIRST_RUN=1
cd "$(dirname "$0")/../.." || exit 1
gdb=
[ "$1" = "--gdb" ] && { gdb="gdb -q -batch -ex 'set debuginfod enabled off' -ex run -ex 'bt 40' --args"; shift; }
ninja -C _build-gtk4 install >/dev/null || exit 1
glib-compile-schemas _inst-gtk4/share/glib-2.0/schemas || exit 1
if [ "${FIRST_RUN:-}" != 1 ]; then
	PRE="gsettings set org.gnome.evolution.mail show-startup-wizard false;
		gsettings set org.gnome.evolution.mail prompt-check-if-default-mailer false; ${PRE:-}"
	export PRE
fi
GSETTINGS_SCHEMA_DIR=$PWD/_inst-gtk4/share/glib-2.0/schemas \
	eval sh doxmail/port/run-isolated.sh "${1:-25}" $gdb _inst-gtk4/bin/evolution '"${@:2}"' 2>&1 |
	grep -v '^\[New Thread\|^\[Thread\|^\[Detaching\|xdg-desktop-portal\|a11y\|AT-SPI\|MESA\|PipeWire\|^rm: \|^$'
