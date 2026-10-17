#!/usr/bin/env python3
"""Turns handlers of legacy widget event signals back to GdkEvent input
when the code they share with canvas item handlers uses GnomeCanvasEvent.

Usage: canvas-entry.py FILE CANVAS_EXPR FUNC...   (rewrites in place)

For each FUNC taking `GnomeCanvasEvent *param`, the parameter becomes
`GdkEvent *gdk_event` (also in prototypes), and the body starts with a
local GnomeCanvasEvent filled by e_gdk_event_to_canvas_event() for the
canvas CANVAS_EXPR (a parameter like `widget`, or NULL for non-canvas
widgets), so `param` keeps its meaning in the rest of the function.
"""

import re
import sys

path, canvas, funcs = sys.argv[1], sys.argv[2], sys.argv[3:]

with open(path, encoding='utf-8') as f:
    text = f.read()

for func in funcs:
    d = re.search(r'\n%s \((?:[^()]|\n)*?GnomeCanvasEvent \*(\w+)(?:[^()]|\n)*?\)\n\{\n' % func, text)
    if not d:
        print(f'{path}: {func}: definition not found', file=sys.stderr)
        continue
    param = d.group(1)

    # prototypes and the definition
    pat = re.compile(r'(\b%s\s*\((?:[^()]|\n)*?)GnomeCanvasEvent \*\w+' % func)
    text = pat.sub(lambda m: m.group(1) + 'GdkEvent *gdk_event', text)

    d = re.search(r'\n%s \((?:[^()]|\n)*?\)\n\{\n' % func, text)
    decl = '\tGnomeCanvasEvent cevent, *%s = e_gdk_event_to_canvas_event (gdk_event, %s, &cevent);\n' % (param, canvas)
    text = text[:d.end()] + decl + text[d.end():]
    print(f'{path}: {func} ({param})')

with open(path, 'w', encoding='utf-8') as f:
    f.write(text)
