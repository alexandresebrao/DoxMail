#!/usr/bin/env python3
"""Lists handlers of the legacy widget event signals (e_gtk_widget_legacy_events()
connections and e_gtk_widget_class_override_legacy_event() overrides) whose
event parameter is not a GdkEvent; those signals pass a GdkEvent pointer.

Usage: legacy-audit.py FILE...
"""

import re
import sys

EVENTS = r'(?:button|key)[-_](?:press|release)[-_]event|motion[-_]notify[-_]event|scroll[-_]event|' \
         r'(?:enter|leave)[-_]notify[-_]event|focus[-_](?:in|out)[-_]event|delete[-_]event|event'
CONNECT = re.compile(r'(?:g_signal_connect\w* \([^;]*?,\s*"(%s)",\s*G_CALLBACK \((\w+)\))'
                     r'|(?:override_legacy_event \([^;]*?"([\w-]+)",\s*G_CALLBACK \((\w+)\))' % EVENTS, re.S)

for path in sys.argv[1:]:
    with open(path, encoding='utf-8', errors='replace') as f:
        text = f.read()
    for m in CONNECT.finditer(text):
        sig = m.group(1) or m.group(3)
        func = m.group(2) or m.group(4)
        if sig in ('popup-menu', 'popup_menu', 'style-updated'):
            continue
        d = re.search(r'\n%s \(([^)]*)\)\s*\n\{' % func, text)
        if not d:
            continue
        params = [p.strip() for p in d.group(1).split(',')]
        # "event" of canvas items gives GnomeCanvasEvent
        if sig == 'event' and len(params) > 1 and params[1].startswith('GnomeCanvasEvent'):
            continue
        if len(params) < 2 or not re.match(r'(const )?GdkEvent\b', params[1]):
            print(f'{path}: {func} ({sig}): {params[1] if len(params) > 1 else "?"}')
