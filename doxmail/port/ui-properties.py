#!/usr/bin/env python3
"""Fixes or drops GTK 3 properties of Gtk* objects in GTK 4 .ui files.

Usage: ui-properties.py FILE.ui...   (rewrites in place, lists what it did)

GTK 3 icon sizes (1 menu ... 6 dialog) become normal or large.
Each <property> of a Gtk* object is looked up in the real GTK 4 class
through GObject introspection.  Known GTK 3 ones are translated
(border-width to the four margins, xpad/ypad to margins, ...); the
others the class does not have, or cannot set, are removed.  Objects of other classes
(the E* widgets) are left alone.
"""

import re
import sys
import xml.etree.ElementTree as ET

import gi
gi.require_version('Gtk', '4.0')
from gi.repository import GObject, Gtk  # noqa: E402

MARGINS_ALL = ('margin-start', 'margin-end', 'margin-top', 'margin-bottom')
TRANSLATE = {
    'border-width': MARGINS_ALL,
    'xpad': ('margin-start', 'margin-end'),
    'ypad': ('margin-top', 'margin-bottom'),
    'can-default': (),
    'has-default': (),
}


def gtk_class(name):
    if not name or not name.startswith('Gtk'):
        return None
    return getattr(Gtk, name[3:], None)


def fix(root, path):
    changed = False

    for obj in root.iter('object'):
        cls = gtk_class(obj.get('class'))
        if cls is None:
            continue

        for p in list(obj.findall('property')):
            name = p.get('name').replace('_', '-')
            if name == 'icon-size' and p.text and p.text.strip().isdigit():
                old = p.text
                p.text = {'0': 'inherit', '3': 'large', '5': 'large', '6': 'large'}.get(p.text.strip(), 'normal')
                print(f'{path}: {obj.get("class")}.icon-size {old} -> {p.text}')
                changed = True
                continue
            pspec = cls.find_property(name)
            if pspec is not None and pspec.flags & GObject.ParamFlags.WRITABLE:
                continue

            index = list(obj).index(p)
            obj.remove(p)
            changed = True

            targets = TRANSLATE.get(name)
            if targets:
                for target in targets:
                    if cls.find_property(target) is None:
                        continue
                    if any(q.get('name').replace('_', '-') == target for q in obj.findall('property')):
                        continue
                    new = ET.Element('property', {'name': target})
                    new.text = p.text
                    new.tail = p.tail
                    obj.insert(index, new)
                print(f'{path}: {obj.get("class")}.{name} -> {", ".join(targets) or "dropped"}')
            else:
                print(f'{path}: {obj.get("class")}.{name} dropped')

    return changed


for path in sys.argv[1:]:
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    tree = ET.parse(path, parser)
    if fix(tree.getroot(), path):
        ET.indent(tree, space='  ')
        tree.write(path, encoding='UTF-8', xml_declaration=True)
        # GTK 4 needs <col></col> for empty cells, ElementTree self-closes
        with open(path, encoding='utf-8') as f:
            text = f.read()
        with open(path, 'w', encoding='utf-8') as f:
            f.write(re.sub(r'<col ([^>]*?) />', r'<col \1></col>', text))
