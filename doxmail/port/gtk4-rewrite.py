#!/usr/bin/env python3
"""Mechanical GTK 3 -> GTK 4 rewrites for the DoxMail sources.

Usage: gtk4-rewrite.py FILE...   (rewrites in place, prints a summary)

Only rewrites that need no knowledge beyond the call itself live here;
anything else is ported by hand.  Calls whose GTK 3 meaning needs a
type-dependent replacement go to the e_gtk_*() helpers in
src/e-util/e-gtk4-compat.c.
"""

import re
import sys
from collections import Counter

counts = Counter()


def find_call_end(text, open_paren):
    """Index just past the parenthesis closing the one at open_paren."""
    depth = 0
    i = open_paren
    in_str = None
    while i < len(text):
        c = text[i]
        if in_str:
            if c == '\\':
                i += 2
                continue
            if c == in_str:
                in_str = None
        elif c in '"\'':
            in_str = c
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def split_args(args):
    out, depth, cur, in_str, i = [], 0, '', None, 0
    while i < len(args):
        c = args[i]
        if in_str:
            cur += c
            if c == '\\' and i + 1 < len(args):
                cur += args[i + 1]
                i += 2
                continue
            if c == in_str:
                in_str = None
        elif c in '"\'':
            in_str = c
            cur += c
        elif c in '([{':
            depth += 1
            cur += c
        elif c in ')]}':
            depth -= 1
            cur += c
        elif c == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += c
        i += 1
    if cur.strip() or out:
        out.append(cur)
    return [a.strip() for a in out]


def strip_cast(arg, casts):
    for cast in casts:
        m = re.match(r'^' + cast + r'\s*\(', arg)
        if m:
            end = find_call_end(arg, m.end() - 1)
            if end == len(arg):
                return arg[m.end():end - 1].strip()
    return arg


def rewrite_calls(text, name, func):
    """Replace every call of name(...) by func(args) (None = keep)."""
    pattern = re.compile(r'(?<![\w.>])' + re.escape(name) + r'\s*\(')
    pos = 0
    out = []
    while True:
        m = pattern.search(text, pos)
        if not m:
            break
        end = find_call_end(text, m.end() - 1)
        if end < 0:
            break
        args = split_args(text[m.end():end - 1])
        try:
            new = func(args, text, m.start(), end)
        except IndexError:
            new = None  # unexpected arguments (a comment, a macro): leave it
        out.append(text[pos:m.start()])
        if new is None:
            out.append(text[m.start():end])
        else:
            out.append(new)
            counts[name] += 1
        pos = end
    out.append(text[pos:])
    return ''.join(out)


def drop_statement(text, name):
    """Delete statements that are just a call of name(...)."""
    pattern = re.compile(r'^([ \t]*)' + re.escape(name) + r'\s*\(', re.M)
    pos = 0
    out = []
    while True:
        m = pattern.search(text, pos)
        if not m:
            break
        end = find_call_end(text, m.end() - 1)
        rest = re.match(r'\s*;[ \t]*\n', text[end:]) if end > 0 else None
        if not rest:
            out.append(text[pos:m.end()])
            pos = m.end()
            continue
        # keep "if (x)\n\tcall ();" valid by leaving an empty statement
        before = text[:m.start()].rstrip()
        out.append(text[pos:m.start()])
        if re.search(r'(\bif\s*\(.*\)|\belse)$', before.split('\n')[-1]):
            out.append(m.group(1) + ';\n')
        counts['drop ' + name] += 1
        pos = end + rest.end()
    out.append(text[pos:])
    return ''.join(out)


def call(name, args):
    return '%s (%s)' % (name, ', '.join(args))


# name -> (new name, casts to strip from the first argument)
RENAMES = {
    'gtk_container_add': ('e_gtk_container_add', ['GTK_CONTAINER']),
    'gtk_container_remove': ('e_gtk_container_remove', ['GTK_CONTAINER']),
    'gtk_container_get_children': ('e_gtk_widget_get_children', ['GTK_CONTAINER']),
    'gtk_container_foreach': ('e_gtk_container_foreach', ['GTK_CONTAINER']),
    'gtk_container_forall': ('e_gtk_container_foreach', ['GTK_CONTAINER']),
    'gtk_container_set_border_width': ('e_gtk_widget_set_border_width', ['GTK_CONTAINER']),
    'gtk_bin_get_child': ('e_gtk_bin_get_child', ['GTK_BIN']),
    'gtk_box_pack_start': ('e_gtk_box_pack_start', []),
    'gtk_box_pack_end': ('e_gtk_box_pack_end', []),
    'gtk_box_reorder_child': ('e_gtk_box_reorder_child', []),
    'gtk_button_box_new': ('e_gtk_button_box_new', []),
    'gtk_paned_pack1': ('e_gtk_paned_pack1', ['GTK_PANED']),
    'gtk_paned_pack2': ('e_gtk_paned_pack2', ['GTK_PANED']),
    'e_gtk_paned_pack1': ('e_gtk_paned_pack1', ['GTK_PANED']),
    'e_gtk_paned_pack2': ('e_gtk_paned_pack2', ['GTK_PANED']),
    'gtk_paned_get_child1': ('e_gtk_paned_get_child1', ['GTK_PANED']),
    'gtk_paned_get_child2': ('e_gtk_paned_get_child2', ['GTK_PANED']),
    'gtk_paned_get_position': ('e_gtk_paned_get_position', ['GTK_PANED']),
    'gtk_paned_set_position': ('e_gtk_paned_set_position', ['GTK_PANED']),
    'gtk_widget_destroy': ('e_gtk_widget_destroy', []),
    'gtk_widget_get_toplevel': ('e_gtk_widget_get_toplevel', []),
    'gtk_widget_is_toplevel': ('e_gtk_widget_is_toplevel', []),
    'gtk_widget_grab_default': ('e_gtk_widget_grab_default', []),
    'gtk_widget_get_preferred_width': ('e_gtk_widget_get_preferred_width', []),
    'gtk_widget_get_preferred_height': ('e_gtk_widget_get_preferred_height', []),
    'gtk_widget_get_preferred_height_for_width': ('e_gtk_widget_get_preferred_height_for_width', []),
    'gtk_dialog_run': ('e_gtk_dialog_run', []),
    'gtk_native_dialog_run': ('e_gtk_native_dialog_run', []),
    'gtk_dialog_get_action_area': ('e_gtk_dialog_get_action_area', []),
    'gtk_button_set_image': ('e_gtk_button_set_image', []),
    'gtk_icon_theme_get_default': ('e_gtk_icon_theme_get_default', []),
    'gtk_label_set_line_wrap': ('gtk_label_set_wrap', []),
    'gtk_label_set_line_wrap_mode': ('gtk_label_set_wrap_mode', []),
    'gtk_label_get_line_wrap': ('gtk_label_get_wrap', []),
    'gtk_widget_set_margin_left': ('gtk_widget_set_margin_start', []),
    'gtk_widget_set_margin_right': ('gtk_widget_set_margin_end', []),
    'gtk_widget_get_margin_left': ('gtk_widget_get_margin_start', []),
    'gtk_widget_get_margin_right': ('gtk_widget_get_margin_end', []),
    'gtk_widget_set_can_focus': ('gtk_widget_set_focusable', []),
    'gtk_widget_get_can_focus': ('gtk_widget_get_focusable', []),
    'gtk_widget_queue_resize_no_redraw': ('gtk_widget_queue_resize', []),
    'gtk_header_bar_set_show_close_button': ('gtk_header_bar_set_show_title_buttons', []),
    'gtk_popover_set_modal': ('gtk_popover_set_autohide', []),
    'gtk_window_resize': ('gtk_window_set_default_size', []),
    'gtk_menu_bar_new_from_model': ('gtk_popover_menu_bar_new_from_model', []),
    'gtk_menu_new_from_model': ('gtk_popover_menu_new_from_model', []),
    'e_menu_bar_new': ('e_menu_bar_new', ['GTK_MENU_BAR']),
    'gtk_file_chooser_button_new': ('e_gtk_file_chooser_button_new', []),
    'gtk_file_chooser_button_new_with_dialog': ('e_gtk_file_chooser_button_new_with_dialog', []),
    'gtk_file_chooser_get_filename': ('e_gtk_file_chooser_get_filename', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_set_filename': ('e_gtk_file_chooser_set_filename', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_get_uri': ('e_gtk_file_chooser_get_uri', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_set_uri': ('e_gtk_file_chooser_set_uri', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_get_file': ('e_gtk_file_chooser_get_file', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_set_file': ('e_gtk_file_chooser_set_file', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_get_files': ('e_gtk_file_chooser_get_files', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_get_uris': ('e_gtk_file_chooser_get_uris', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_unselect_all': ('e_gtk_file_chooser_unselect_all', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_add_filter': ('e_gtk_file_chooser_add_filter', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_set_filter': ('e_gtk_file_chooser_set_filter', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_get_filter': ('e_gtk_file_chooser_get_filter', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_list_filters': ('e_gtk_file_chooser_list_filters', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_set_current_folder': ('e_gtk_file_chooser_set_current_folder', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_set_current_folder_uri': ('e_gtk_file_chooser_set_current_folder_uri', ['GTK_FILE_CHOOSER']),
    'gtk_file_chooser_get_current_folder_uri': ('e_gtk_file_chooser_get_current_folder_uri', ['GTK_FILE_CHOOSER']),
    'gtk_widget_get_window': ('e_gtk_widget_get_surface', []),
}

# GTK 3 calls with nothing to do in GTK 4, dropped when they are a statement
DROPPED = [
    'gtk_window_set_position',
    'gtk_window_move',
    'gtk_window_set_type_hint',
    'gtk_window_set_urgency_hint',
    'gtk_widget_set_can_default',
    'gtk_widget_set_has_window',
    'gtk_widget_set_redraw_on_allocate',
    'gtk_widget_set_app_paintable',
    'gtk_widget_set_double_buffered',
    'gtk_widget_add_events',
    'gtk_widget_set_events',
    'gtk_container_set_focus_vadjustment',
    'gtk_container_set_focus_hadjustment',
    'gtk_container_set_resize_mode',
    'gtk_tree_view_set_rules_hint',
    'gtk_expander_set_spacing',
    'gtk_expander_set_label_fill',
    'gtk_size_group_set_ignore_hidden',
    'gtk_file_chooser_set_local_only',
    'gtk_file_chooser_set_do_overwrite_confirmation',
    'gtk_button_set_always_show_image',
    'gtk_button_set_use_stock',
    'gtk_button_box_set_layout',
    'gtk_button_box_set_child_non_homogeneous',
    'gtk_frame_set_shadow_type',
    'gtk_file_chooser_set_preview_widget',
    'gtk_file_chooser_set_preview_widget_active',
    'gtk_viewport_set_shadow_type',
    'gtk_widget_set_no_show_all',
    'gtk_menu_button_set_use_popover',
]


def r_show_all(args, *_):
    return call('gtk_widget_set_visible', [args[0], 'TRUE'])


def r_entry_get_text(args, *_):
    return 'gtk_editable_get_text (GTK_EDITABLE (%s))' % strip_cast(args[0], ['GTK_ENTRY'])


def r_entry_set_text(args, *_):
    return 'gtk_editable_set_text (GTK_EDITABLE (%s), %s)' % (strip_cast(args[0], ['GTK_ENTRY']), args[1])


def r_entry_width_chars(args, *_):
    return 'gtk_editable_set_width_chars (GTK_EDITABLE (%s), %s)' % (strip_cast(args[0], ['GTK_ENTRY']), args[1])


def r_shadow(args, *_):
    if args[1] == 'GTK_SHADOW_NONE':
        return call('gtk_scrolled_window_set_has_frame', [args[0], 'FALSE'])
    if args[1].startswith('GTK_SHADOW_'):
        return call('gtk_scrolled_window_set_has_frame', [args[0], 'TRUE'])
    return None


def r_relief(args, *_):
    if args[1] == 'GTK_RELIEF_NONE':
        return call('gtk_button_set_has_frame', [args[0], 'FALSE'])
    if args[1] == 'GTK_RELIEF_NORMAL':
        return call('gtk_button_set_has_frame', [args[0], 'TRUE'])
    return None


def r_drop_icon_size(nargs):
    """GTK 4 icon helpers lost the GtkIconSize argument."""
    def f(args, *_):
        if len(args) == nargs and ('ICON_SIZE' in args[-1] or 'size' in args[-1].lower()):
            return call(f.name, args[:-1])
        return None
    return f


def r_style_class(new):
    def f(args, *_):
        m = re.match(r'^gtk_widget_get_style_context\s*\(', args[0])
        if m and find_call_end(args[0], m.end() - 1) == len(args[0]):
            widget = args[0][m.end():-1].strip()
            return call(new, [widget] + args[1:])
        return None
    return f


def r_gtk_init(args, *_):
    return 'gtk_init ()'


def r_widget_clipboard(args, *_):
    if len(args) != 2:
        return None
    if 'PRIMARY' in args[1]:
        return call('gtk_widget_get_primary_clipboard', [args[0]])
    return call('gtk_widget_get_clipboard', [args[0]])


def r_style_get_with_state(name):
    def f(args, *_):
        if len(args) == 3:
            return call(name, [args[0], args[2]])
        return None
    return f


def r_cursor_from_name(args, *_):
    if len(args) == 2 and 'display' in args[0].lower():
        return call('gdk_cursor_new_from_name', [args[1], 'NULL'])
    return None


def r_css_load_data(args, *_):
    if len(args) == 4:
        return call('gtk_css_provider_load_from_string', [args[0], args[1]])
    return None


CALLS = {
    'gtk_css_provider_load_from_data': r_css_load_data,
    'gdk_cursor_new_from_name': r_cursor_from_name,
    'gtk_style_context_get_padding': r_style_get_with_state('gtk_style_context_get_padding'),
    'gtk_style_context_get_border': r_style_get_with_state('gtk_style_context_get_border'),
    'gtk_style_context_get_margin': r_style_get_with_state('gtk_style_context_get_margin'),
    'gtk_style_context_get_color': r_style_get_with_state('gtk_style_context_get_color'),
    'gtk_widget_get_clipboard': r_widget_clipboard,
    'gtk_widget_show_all': r_show_all,
    'gtk_entry_get_text': r_entry_get_text,
    'gtk_entry_set_text': r_entry_set_text,
    'gtk_entry_set_width_chars': r_entry_width_chars,
    'gtk_scrolled_window_set_shadow_type': r_shadow,
    'gtk_button_set_relief': r_relief,
    'gtk_style_context_add_class': r_style_class('gtk_widget_add_css_class'),
    'gtk_style_context_remove_class': r_style_class('gtk_widget_remove_css_class'),
    'gtk_style_context_has_class': r_style_class('gtk_widget_has_css_class'),
    'gtk_init': r_gtk_init,
}

ICON_SIZE_CALLS = {
    'gtk_image_new_from_icon_name': 2,
    'gtk_image_set_from_icon_name': 3,
    'gtk_button_new_from_icon_name': 2,
    'gtk_image_new_from_gicon': 2,
    'gtk_image_set_from_gicon': 3,
}


# GTK 3 drag-and-drop, selection and clipboard names -> the emulation in
# src/e-util/e-gtk-dnd-compat.[ch]
REGEX_RENAMES = [
    (r'\bGtkTargetEntry\b', 'EGtkTargetEntry'),
    (r'\bGtkTargetList\b', 'EGtkTargetList'),
    (r'\bGtkSelectionData\b', 'EGtkSelectionData'),
    (r'\bGdkDragContext\b', 'EGtkDragContext'),
    (r'\bGdkAtom\b', 'EGtkAtom'),
    (r'\bGtkDestDefaults\b', 'EGtkDestDefaults'),
    (r'\bGtkTargetFlags\b', 'EGtkTargetFlags'),
    (r'\bGtkClipboard(TextReceivedFunc|ReceivedFunc|TargetsReceivedFunc|GetFunc|ClearFunc)\b', r'EGtkClipboard\1'),
    (r'\bGtkClipboard\b', 'GdkClipboard'),
    (r'\bGTK_DEST_DEFAULT_', 'E_GTK_DEST_DEFAULT_'),
    (r'\bGTK_TARGET_(SAME_APP|SAME_WIDGET|OTHER_APP|OTHER_WIDGET)\b', r'E_GTK_TARGET_\1'),
    (r'\bGDK_SELECTION_(PRIMARY|CLIPBOARD|TYPE_STRING)\b', r'E_GTK_SELECTION_\1'),
    (r'\bGDK_NONE\b', 'E_GTK_ATOM_NONE'),
    (r'\bGTK_TYPE_SELECTION_DATA\b', 'E_GTK_TYPE_SELECTION_DATA'),
    (r'\bGTK_TYPE_TARGET_LIST\b', 'E_GTK_TYPE_TARGET_LIST'),
    (r'\bGDK_TYPE_DRAG_CONTEXT\b', 'E_GTK_TYPE_DRAG_CONTEXT'),
    (r'\bGDK_IS_DRAG_CONTEXT\b', 'E_GTK_IS_DRAG_CONTEXT'),
    (r'\bgtk_drag_(?!check_threshold\b)([a-z_]+)\s*\(', r'e_gtk_drag_\1 ('),
    (r'\bgdk_drag_status\s*\(', 'e_gtk_drag_status ('),
    (r'\bgdk_drag_context_([a-z_]+)\s*\(', r'e_gtk_drag_context_\1 ('),
    (r'\bgtk_target_(list|table)_([a-z_]+)\s*\(', r'e_gtk_target_\1_\2 ('),
    (r'\bgtk_selection_data_([a-z_]+)\s*\(', r'e_gtk_selection_data_\1 ('),
    (r'\bgtk_clipboard_([a-z_]+)\s*\(', r'e_gtk_clipboard_\1 ('),
    (r'\bgdk_atom_(intern_static_string|intern|name)\s*\(', r'e_gtk_atom_\1 ('),
    (r'\bgtk_get_current_event(_time|_state|_device)?\s*\(', r'e_gtk_get_current_event\1 ('),
    (r'\bgdk_event_get_(state|keyval|button|coords|root_coords)\s*\(', r'e_gdk_event_get_\1 ('),
    (r'\bgdk_event_free\s*\(', 'gdk_event_unref ('),
    (r'\bgdk_event_copy\s*\(', 'gdk_event_ref ('),
    (r'\bgtk_show_uri_on_window\s*\(', 'e_gtk_show_uri_on_window ('),
    (r'\bGDK_MOD1_MASK\b', 'GDK_ALT_MASK'),
    (r'\bGdkEvent(Button|Key|Motion|Crossing|Scroll|Focus|Configure|WindowState|Expose|Any|Touch)\b', 'GdkEvent'),
    (r'\bGTK_ICON_SIZE_(MENU|SMALL_TOOLBAR|BUTTON)\b', 'GTK_ICON_SIZE_NORMAL'),
    (r'\bGTK_ICON_SIZE_(LARGE_TOOLBAR|DND|DIALOG)\b', 'GTK_ICON_SIZE_LARGE'),
    (r'\bGTK_IS_CLIPBOARD\b', 'GDK_IS_CLIPBOARD'),
    (r'"stock-size"', '"icon-size"'),
    (r'\bgdk_event_free\b', 'gdk_event_unref'),
    (r'\bAtkObject\b', 'GtkWidget'),
    (r'\bgtk_widget_get_accessible\s*\(', 'GTK_WIDGET ('),
    (r'\batk_object_set_name\s*\(', 'e_gtk_widget_set_accessible_name ('),
    (r'\batk_object_set_description\s*\(', 'e_gtk_widget_set_accessible_description ('),
    (r'\bGTK_STYLE_CLASS_([A-Z_]+)\b', lambda m: '"%s"' % m.group(1).lower().replace('_', '-')),
]


def rewrite(text):
    for pattern, repl in REGEX_RENAMES:
        text, n = re.subn(pattern, repl, text)
        if n:
            counts['re ' + pattern[:40]] += n
    for name, (new, casts) in RENAMES.items():
        def f(args, *_, new=new, casts=casts):
            if args:
                args = [strip_cast(args[0], casts)] + args[1:]
            return call(new, args)
        text = rewrite_calls(text, name, f)
    for name, func in CALLS.items():
        text = rewrite_calls(text, name, func)
    for name, nargs in ICON_SIZE_CALLS.items():
        func = r_drop_icon_size(nargs)
        func.name = name
        text = rewrite_calls(text, name, func)
    for name in DROPPED:
        text = drop_statement(text, name)
    return text


def main():
    changed = 0
    for path in sys.argv[1:]:
        with open(path, encoding='utf-8') as f:
            old = f.read()
        new = rewrite(old)
        if new != old:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(new)
            changed += 1
    for name, n in counts.most_common():
        print('%6d  %s' % (n, name))
    print('%d files changed' % changed)


if __name__ == '__main__':
    main()
