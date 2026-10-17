#!/usr/bin/env python3
"""Turns GtkWidgetClass style_updated overrides into "style-updated" handlers.

Usage: style-updated.py FILE...   (rewrites in place)

GTK 4 has no style_updated vfunc; e_gtk_widget_style_updates() emits a
"style-updated" signal instead (src/e-util/e-gtk-event-compat.h).  For a
`widget_class->style_updated = FN;` the assignment and FN's chain-up go
away, and the constructed() method (from `object_class->constructed`)
connects FN after its own chain-up.  Files without a constructed() method
are reported and left alone.
"""

import re
import sys

for path in sys.argv[1:]:
    with open(path, encoding='utf-8') as f:
        text = f.read()

    m = re.search(r'\n\s*\w+->style_updated\s*=\s*(\w+);\n', text)
    if not m:
        continue
    func = m.group(1)

    c = re.search(r'->constructed\s*=\s*(\w+);', text)
    if not c:
        print(f'{path}: no constructed() method, skipped', file=sys.stderr)
        continue
    cfunc = c.group(1)

    # the chain-up in the style_updated function, with its comment
    text = re.sub(r'\n(\t/\*[^\n]*\*/\n)?\tGTK_WIDGET_CLASS \(\w+_parent_class\)\s*->\s*\n?\s*style_updated \(\w+\);\n\n?',
                  '\n', text)
    text = text.replace(m.group(0), '\n', 1)

    # connect after the constructed() chain-up
    cdef = re.search(r'\n%s \((\w+) \*(\w+)\)\n\{' % cfunc, text)
    if not cdef:
        print(f'{path}: {cfunc} definition not found, skipped', file=sys.stderr)
        continue
    obj = cdef.group(2)
    chain = re.compile(r'(\tG_OBJECT_CLASS \(\w+_parent_class\)\s*->\s*\n?\s*constructed \(%s\);\n)' % obj)
    cm = chain.search(text, cdef.end())
    if not cm:
        print(f'{path}: no chain-up in {cfunc}, skipped', file=sys.stderr)
        continue
    connect = ('\n\tg_signal_connect (\n\t\te_gtk_widget_style_updates (%s), "style-updated",\n'
               '\t\tG_CALLBACK (%s), NULL);\n' % (obj, func))
    text = text[:cm.end()] + connect + text[cm.end():]

    # forward declaration when the handler comes after constructed()
    fdef = re.search(r'\nstatic void\n%s \((\w+) \*(\w+)\)' % func, text)
    if fdef and fdef.start() > cdef.start():
        decl = 'static void %s (%s *%s);\n\n' % (func, fdef.group(1), fdef.group(2))
        start = text.rfind('\nstatic', 0, cdef.start()) + 1
        text = text[:start] + decl + text[start:]

    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    print(f'{path}: {func} connected in {cfunc}')
