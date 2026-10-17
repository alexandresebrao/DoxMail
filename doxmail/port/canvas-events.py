#!/usr/bin/env python3
"""Switches canvas item code from GdkEvent to GnomeCanvasEvent.

Usage: canvas-events.py FILE...   (rewrites in place)

Only for files whose events all come from the GnomeCanvas (canvas items,
ECell implementations, the ETable/ETree item plumbing).  The GDK event
type and mask values differ from the GNOME_CANVAS_* ones with GTK 4, so a
file must be switched as a whole; places that need a real GdkEvent use
gnome_canvas_event_get_gdk_event().
"""

import re
import sys

TYPES = ('BUTTON_PRESS|BUTTON_RELEASE|MOTION_NOTIFY|KEY_PRESS|KEY_RELEASE|'
         'ENTER_NOTIFY|LEAVE_NOTIFY|FOCUS_CHANGE|SCROLL|NOTHING')

RULES = [
    (r'\bGdkEvent\b', 'GnomeCanvasEvent'),
    (r'\bGdkEventType\b', 'GnomeCanvasEventType'),
    (r'\bGDK_(2|3)BUTTON_PRESS\b', r'GNOME_CANVAS_\1BUTTON_PRESS'),
    (r'\bGDK_DOUBLE_BUTTON_PRESS\b', 'GNOME_CANVAS_2BUTTON_PRESS'),
    (r'\bGDK_TRIPLE_BUTTON_PRESS\b', 'GNOME_CANVAS_3BUTTON_PRESS'),
    (r'\bGDK_(' + TYPES + r')\b', r'GNOME_CANVAS_\1'),
    (r'\bGDK_(POINTER_MOTION|BUTTON_MOTION|BUTTON_PRESS|BUTTON_RELEASE|KEY_PRESS|'
     r'KEY_RELEASE|ENTER_NOTIFY|LEAVE_NOTIFY|FOCUS_CHANGE|SCROLL)_MASK\b',
     r'GNOME_CANVAS_\1_MASK'),
    (r'\bGDK_BUTTON[1-5]_MOTION_MASK\b', 'GNOME_CANVAS_BUTTON_MOTION_MASK'),
    (r'\bGDK_GRAB_([A-Z_]+)\b', r'GNOME_CANVAS_GRAB_\1'),
    (r'\bGdkGrabStatus\b', 'GnomeCanvasGrabStatus'),
    (r'\bGDK_TYPE_EVENT\b', 'GNOME_TYPE_CANVAS_EVENT'),
    (r'\be_gdk_event_get_(state|keyval|button|coords|root_coords)\b', r'gnome_canvas_event_get_\1'),
    (r'\bgdk_event_get_(time|device)\b', r'gnome_canvas_event_get_\1'),
    (r'\bgdk_event_ref\b', 'gnome_canvas_event_copy'),
    (r'\bgdk_event_unref\b', 'gnome_canvas_event_free'),
]


def main():
    for path in sys.argv[1:]:
        with open(path, encoding='utf-8') as f:
            text = f.read()
        total = 0
        for pattern, repl in RULES:
            text, n = re.subn(pattern, repl, text)
            total += n
        with open(path, 'w', encoding='utf-8') as f:
            f.write(text)
        print('%5d  %s' % (total, path))


if __name__ == '__main__':
    main()
