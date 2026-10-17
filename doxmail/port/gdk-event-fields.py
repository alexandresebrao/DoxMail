#!/usr/bin/env python3
"""Replaces GTK 3 GdkEvent field access with the e_gdk_event_*() getters.

GdkEvent is opaque with GTK 4.  For variables declared as GdkEvent * in a
file, event->keyval, event->key.keyval, event->button.x and the like become
e_gdk_event_keyval (event), e_gdk_event_x (event), ... from
src/e-util/e-gtk-event-compat.h; GDK_2BUTTON_PRESS becomes
E_GDK_2BUTTON_PRESS.  Not for canvas code: see canvas-events.py.

Usage: gdk-event-fields.py FILE...
"""

import re
import sys

FIELDS = {
    'type': 'e_gdk_event_type',
    'keyval': 'e_gdk_event_keyval',
    'hardware_keycode': 'e_gdk_event_keycode',
    'is_modifier': 'e_gdk_event_is_modifier',
    'state': 'e_gdk_event_state',
    'button': 'e_gdk_event_button',
    'x': 'e_gdk_event_x',
    'y': 'e_gdk_event_y',
    'x_root': 'e_gdk_event_x_root',
    'y_root': 'e_gdk_event_y_root',
    'time': 'e_gdk_event_time',
    'direction': 'e_gdk_event_direction',
    'delta_x': 'e_gdk_event_delta_x',
    'delta_y': 'e_gdk_event_delta_y',
    'in': 'e_gdk_event_focus_in',
}
UNIONS = 'key|button|motion|any|crossing|scroll|focus_change'

for path in sys.argv[1:]:
    with open(path, encoding='utf-8') as f:
        text = f.read()
    names = set(re.findall(r'\bGdkEvent\s*\*\s*(\w+)', text))
    total = 0
    for name in names:
        def repl(m):
            return '%s (%s)' % (FIELDS[m.group(2)], name)
        fields = '|'.join(FIELDS)
        pattern = r'\b%s->(?:(%s)\.)?(%s)\b(?!\s*(?:=[^=]|\.|->))' % (re.escape(name), UNIONS, fields)
        text, n = re.subn(pattern, repl, text)
        total += n
        # &event->key was the same event
        text, n = re.subn(r'&\s*%s->(?:%s)\b(?!\s*\.)' % (re.escape(name), UNIONS), name, text)
        total += n
        text, n = re.subn(r'\b%s->send_event\b' % re.escape(name), 'FALSE', text)
        total += n
    text, n = re.subn(r'\bGDK_(2|DOUBLE_)BUTTON_PRESS\b', 'E_GDK_2BUTTON_PRESS', text)
    total += n
    text, n = re.subn(r'\bGDK_(3|TRIPLE_)BUTTON_PRESS\b', 'E_GDK_3BUTTON_PRESS', text)
    total += n
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    if total:
        print('%5d  %s' % (total, path))
