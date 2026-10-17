#!/usr/bin/env python3
"""Converts what `gtk4-builder-tool simplify --3to4` leaves of GTK 3 in .ui files.

Usage: ui-leftovers.py FILE.ui...   (rewrites in place)

- GtkAlignment becomes a vertical GtkBox (its child then fills the width,
  like the usual xscale 1); the padding properties become margins and
  the alignment ones are dropped;
- GtkButtonBox, GtkHButtonBox and GtkVButtonBox become GtkBox; the
  layout style is dropped, an "end" or "center" one moves the box with
  halign;
- the AtkObject of an accessible internal child becomes a GTK 4
  <accessibility> label (accessible-name) or description;
- GTK 3 <relation type="..." target="id"/> in <accessibility> becomes
  <relation name="...">id</relation>; the GTK 3 <action> ones go;
- empty list store cells are written <col></col>: GTK 4 leaves a
  self-closed <col/> without a value type;
- <packing> left under GtkBox parents (simplify --3to4 keeps them under
  classes it does not know, like the button boxes) goes; an expanding
  child gets hexpand or vexpand by the box orientation.
"""

import re
import sys
import xml.etree.ElementTree as ET

PADDING = {
    'top_padding': 'margin-top', 'top-padding': 'margin-top',
    'bottom_padding': 'margin-bottom', 'bottom-padding': 'margin-bottom',
    'left_padding': 'margin-start', 'left-padding': 'margin-start',
    'right_padding': 'margin-end', 'right-padding': 'margin-end',
}
ALIGNMENT_DROP = {'xalign', 'yalign', 'xscale', 'yscale'}
BUTTONBOX = {'GtkButtonBox', 'GtkHButtonBox', 'GtkVButtonBox'}


def prop(obj, name):
    for p in obj.findall('property'):
        if p.get('name').replace('_', '-') == name:
            return p
    return None


def convert(root):
    changed = False

    for obj in root.iter('object'):
        cls = obj.get('class')

        if cls == 'GtkAlignment':
            obj.set('class', 'GtkBox')
            for p in list(obj.findall('property')):
                name = p.get('name')
                if name in PADDING:
                    p.set('name', PADDING[name])
                elif name.replace('-', '_') in ALIGNMENT_DROP:
                    obj.remove(p)
            orientation = ET.Element('property', {'name': 'orientation'})
            orientation.text = 'vertical'
            obj.insert(0, orientation)
            changed = True

        elif cls in BUTTONBOX:
            obj.set('class', 'GtkBox')
            if cls == 'GtkVButtonBox' and prop(obj, 'orientation') is None:
                orientation = ET.Element('property', {'name': 'orientation'})
                orientation.text = 'vertical'
                obj.insert(0, orientation)
            style = prop(obj, 'layout-style')
            if style is not None:
                if style.text in ('end', 'center') and prop(obj, 'halign') is None:
                    halign = ET.Element('property', {'name': 'halign'})
                    halign.text = style.text
                    obj.append(halign)
                obj.remove(style)
            changed = True

    # AtkObject internal children
    for parent in root.iter():
        for child in list(parent.findall('child')):
            if child.get('internal-child') != 'accessible':
                continue
            atk = child.find('object')
            acc = ET.Element('accessibility')
            for p in (atk.findall('property') if atk is not None else []):
                name = p.get('name').split('::')[-1]
                kind = {'accessible-name': 'label',
                        'accessible-description': 'description'}.get(name)
                if kind:
                    new = ET.SubElement(acc, 'property', {'name': kind})
                    if p.get('translatable'):
                        new.set('translatable', p.get('translatable'))
                    new.text = p.text
            index = list(parent).index(child)
            parent.remove(child)
            if len(acc):
                parent.insert(index, acc)
            changed = True

    for col in root.iter('col'):
        if col.text is None:
            col.text = ''
            changed = True

    for acc in root.iter('accessibility'):
        for rel in list(acc):
            if rel.tag == 'relation' and rel.get('type'):
                rel.set('name', rel.attrib.pop('type'))
                rel.text = rel.attrib.pop('target', '')
                changed = True
            elif rel.tag == 'action':
                acc.remove(rel)
                changed = True

    for obj in root.iter('object'):
        if obj.get('class') != 'GtkBox':
            continue
        orientation = prop(obj, 'orientation')
        vertical = orientation is not None and orientation.text == 'vertical'
        for child in obj.findall('child'):
            packing = child.find('packing')
            if packing is None:
                continue
            expand = prop(packing, 'expand')
            widget = child.find('object')
            if expand is not None and expand.text.lower() in ('true', '1', 'yes') and widget is not None:
                name = 'vexpand' if vertical else 'hexpand'
                if prop(widget, name) is None:
                    new = ET.Element('property', {'name': name})
                    new.text = '1'
                    widget.insert(0, new)
            child.remove(packing)
            changed = True

    return changed


for path in sys.argv[1:]:
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    tree = ET.parse(path, parser)
    if convert(tree.getroot()):
        ET.indent(tree, space='  ')
        tree.write(path, encoding='UTF-8', xml_declaration=True)
        # GTK 4 needs <col></col> for empty cells, ElementTree self-closes
        with open(path, encoding='utf-8') as f:
            text = f.read()
        with open(path, 'w', encoding='utf-8') as f:
            f.write(re.sub(r'<col ([^>]*?) />', r'<col \1></col>', text))
        print(path)
