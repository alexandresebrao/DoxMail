#!/usr/bin/env python3
"""Turns GTK 3 get_preferred_* widget methods into a GTK 4 measure method.

Usage: measure.py FILE...   (rewrites in place)

The get_preferred_width/height/height_for_width/width_for_height functions
stay as they are; a generated <prefix>_measure dispatches to them by
orientation and to the parent class for the rest, and their chain-ups to
the parent become measure calls.
"""

import re
import sys

KINDS = {
    'get_preferred_width': ('GTK_ORIENTATION_HORIZONTAL', False),
    'get_preferred_height': ('GTK_ORIENTATION_VERTICAL', False),
    'get_preferred_height_for_width': ('GTK_ORIENTATION_VERTICAL', True),
    'get_preferred_width_for_height': ('GTK_ORIENTATION_HORIZONTAL', True),
}


def chain_up(text):
    """GTK_WIDGET_CLASS (p)->get_preferred_x (w, a, b) -> measure (...)"""
    def repl(m):
        prefix, kind, args = m.group(1), m.group(2), [a.strip() for a in m.group(3).split(',')]
        orientation, for_size = KINDS[kind]
        if for_size:
            widget, size, mn, nat = args
        else:
            widget, mn, nat = args
            size = '-1'
        return '%s->measure (%s, %s, %s, %s, %s, NULL, NULL)' % (prefix, widget, orientation, size, mn, nat)

    pattern = (r'(GTK_WIDGET_CLASS\s*\(\s*\w+\s*\)\s*|\(\s*\*\s*GTK_WIDGET_CLASS\s*\(\s*\w+\s*\))'
               r'\s*->\s*(get_preferred_(?:width|height)(?:_for_(?:width|height))?)\)?\s*\(([^;]*?)\)')
    text = re.sub(r'\(\s*\*\s*(GTK_WIDGET_CLASS\s*\(\s*\w+\s*\))\s*->\s*(get_preferred_\w+)\s*\)',
                  r'\1->\2', text)
    return re.sub(pattern, repl, text)


def convert(path):
    with open(path, encoding='utf-8') as f:
        text = f.read()

    assigns = re.findall(r'^([ \t]*)(\w+)->(get_preferred_(?:width|height)(?:_for_(?:width|height))?) = (\w+);\n', text, re.M)
    if not assigns:
        return 0

    m = re.search(r'G_DEFINE[A-Z_]*TYPE[A-Z_]*\s*\(\s*\w+,\s*(\w+),', text)
    if not m:
        print('%s: no G_DEFINE_TYPE' % path)
        return 0
    prefix = m.group(1)
    parent = prefix + '_parent_class'

    funcs = {kind: func for _, _, kind, func in assigns}
    indent, klass = assigns[0][0], assigns[0][1]

    for ind, kl, kind, func in assigns:
        text = text.replace('%s%s->%s = %s;\n' % (ind, kl, kind, func), '', 1)

    text = text.replace(
        '%s%s->' % (indent, klass),
        '%s%s->measure = %s_measure;\n%s%s->' % (indent, klass, prefix, indent, klass), 1)

    text = chain_up(text)

    lines = ['static void',
             '%s_measure (GtkWidget *widget,' % prefix,
             '\t\tGtkOrientation orientation,',
             '\t\tgint for_size,',
             '\t\tgint *minimum,',
             '\t\tgint *natural,',
             '\t\tgint *minimum_baseline,',
             '\t\tgint *natural_baseline)',
             '{']
    cases = []
    for orientation in ('GTK_ORIENTATION_HORIZONTAL', 'GTK_ORIENTATION_VERTICAL'):
        plain = [k for k, (o, fs) in KINDS.items() if o == orientation and not fs and k in funcs]
        sized = [k for k, (o, fs) in KINDS.items() if o == orientation and fs and k in funcs]
        body = []
        if sized:
            body.append('\t\tif (for_size >= 0) {')
            body.append('\t\t\t%s (widget, for_size, minimum, natural);' % funcs[sized[0]])
            body.append('\t\t\treturn;')
            body.append('\t\t}')
        if plain:
            body.append('\t\t%s (widget, minimum, natural);' % funcs[plain[0]])
            body.append('\t\treturn;')
        if body:
            cases.append((orientation, body))
    for orientation, body in cases:
        lines.append('\tif (orientation == %s) {' % orientation)
        lines.extend(body)
        lines.append('\t}')
        lines.append('')
    lines.append('\tGTK_WIDGET_CLASS (%s)->measure (widget, orientation, for_size,' % parent)
    lines.append('\t\tminimum, natural, minimum_baseline, natural_baseline);')
    lines.append('}')
    lines.append('')
    func_text = '\n'.join(lines) + '\n'

    m = re.search(r'^static void\n%s_class_init\b' % prefix, text, re.M)
    if not m:
        m = re.search(r'^static void\n\w+_class_init\b', text, re.M)
    text = text[:m.start()] + func_text + text[m.start():]

    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)

    return len(assigns)


for path in sys.argv[1:]:
    n = convert(path)
    if n:
        print('%3d  %s' % (n, path))
