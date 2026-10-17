#!/bin/sh
# errs.sh NAME [LOG]: compile errors of one source file, without the noise
grep -E "/$1:[0-9]+:[0-9]+: error" "${2:-_build-gtk4/eutil.log}" | sed 's|^.*/src/||' | sort -t: -k2,2n -u
