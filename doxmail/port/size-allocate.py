#!/usr/bin/env python3
"""Turns GTK 3 size_allocate methods into the GTK 4 signature.

Usage: size-allocate.py FILE...   (rewrites in place)

For functions assigned to ->size_allocate that still take a GtkAllocation,
the parameters become (widget, width, height, baseline), the body gets an
allocation of 0, 0, width, height (children are placed relative to the
widget with GTK 4), chain-ups pass the new arguments and
gtk_widget_set_allocation() calls go away.
"""

import re
import sys

for path in sys.argv[1:]:
    with open(path, encoding='utf-8') as f:
        text = f.read()
    funcs = set(re.findall(r'->size_allocate\s*=\s*(\w+);', text))
    total = 0
    for func in funcs:
        m = re.search(r'^(static void\n%s \((\w+) \*(\w+),\s*\n?\s*GtkAllocation \*(\w+)\)\n\{\n)' % func, text, re.M)
        if not m:
            m = re.search(r'^(static void\n%s \(GtkWidget \*(\w+),\s*GtkAllocation \*(\w+)\)\n\{\n)' % func, text, re.M)
            if not m:
                continue
            wtype, wname, aname = 'GtkWidget', m.group(2), m.group(3)
        else:
            wtype, wname, aname = m.group(2), m.group(3), m.group(4)
        start = m.end()
        # the body ends at the first "\n}\n" after the head
        end = text.index('\n}\n', start)
        body = text[start:end]
        pad = ' ' * (len(func) + 2)
        head = ('static void\n%s (%s *%s,\n%sgint width,\n%sgint height,\n%sgint baseline)\n{\n'
                % (func, wtype, wname, pad, pad, pad))
        # declarations stay first: the local allocation goes before them
        decl = '\tGtkAllocation %s_local = { 0, 0, width, height };\n\tGtkAllocation *%s = &%s_local;\n' % (aname, aname, aname)
        body = re.sub(r'(->\s*size_allocate\s*\(\s*\w+,\s*)%s\s*\)' % aname, r'\1width, height, baseline)', body)
        body = re.sub(r'\n[ \t]*gtk_widget_set_allocation \([^;]*\);', '', body)
        text = text[:m.start()] + head + decl + body + text[end:]
        total += 1
    if total:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print('%3d  %s' % (total, path))
