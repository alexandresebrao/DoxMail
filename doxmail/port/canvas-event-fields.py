#!/usr/bin/env python3
"""Fixes field access on variables that were GdkEventKey/Button/Motion *.

The GTK 3 subtypes were collapsed into GdkEvent and then GnomeCanvasEvent,
a union; their fields are now reached through the member:
key->keyval becomes key->key.keyval, button->x becomes button->any.x.
Only variables declared as GnomeCanvasEvent * in the file are touched.

Usage: canvas-event-fields.py FILE...
"""

import re
import sys

ANY = 'x|y|x_root|y_root|time|state|device|send_event'
KEY = 'keyval|hardware_keycode|group|is_modifier|string|length'

for path in sys.argv[1:]:
    with open(path, encoding='utf-8') as f:
        text = f.read()
    names = set(re.findall(r'GnomeCanvasEvent\s*\*\s*(\w+)', text))
    total = 0
    for name in names:
        rules = [
            (r'\b%s->(%s)\b(?!\s*\.)' % (name, ANY), r'%s->any.\1' % name),
            (r'\b%s->(%s)\b' % (name, KEY), r'%s->key.\1' % name),
            (r'\b%s->button\b(?!\s*\.)' % name, r'%s->button.button' % name),
            (r'\b%s->in\b' % name, r'%s->focus_change.in' % name),
            (r'\b%s->direction\b' % name, r'%s->scroll.direction' % name),
            (r'\b%s->(mode|detail|focus)\b(?!\s*\.)' % name, r'%s->crossing.\1' % name),
        ]
        for pattern, repl in rules:
            text, n = re.subn(pattern, repl, text)
            total += n
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    print('%5d  %s' % (total, path))
