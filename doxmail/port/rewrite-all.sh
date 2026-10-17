#!/bin/sh
# rewrite-all.sh: run gtk4-rewrite.py over the whole tree, skipping the
# already ported canvas, the tests and the compat layers themselves
cd "$(dirname "$0")/../.." || exit 1
find src \( -name '*.c' -o -name '*.h' \) \
	-not -path 'src/libgnomecanvas/*' \
	-not -path 'src/doxmail-tests/*' \
	-not -name 'e-gtk4-compat.*' \
	-not -name 'e-gtk-dnd-compat.*' \
	-not -name 'e-gtk-file-chooser-compat.*' \
	-not -name 'e-assistant.*' \
	-not -name 'e-paned.c' \
	-not -name 'gal-a11y-*' \
	-not -name 'ea-*' \
	-print0 | xargs -0 python3 doxmail/port/gtk4-rewrite.py
