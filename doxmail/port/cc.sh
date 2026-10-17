#!/bin/sh
# cc.sh FILE.c...: compile single e-util objects, errors only
for f in "$@"; do
	ninja -C _build-gtk4 "src/e-util/CMakeFiles/evolution-util.dir/$(basename "$f").o" 2>&1 | grep -E "error|warning: (implicit|incompatible)" | sed 's|^.*/src/||'
done
