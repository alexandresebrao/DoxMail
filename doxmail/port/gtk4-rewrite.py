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
    # GtkCheckButton is no GtkToggleButton on GTK 4; these take either
    'gtk_toggle_button_get_active': ('e_gtk_toggle_button_get_active', ['GTK_TOGGLE_BUTTON']),
    'gtk_toggle_button_set_active': ('e_gtk_toggle_button_set_active', ['GTK_TOGGLE_BUTTON']),
    'gtk_toggle_button_get_inconsistent': ('e_gtk_toggle_button_get_inconsistent', ['GTK_TOGGLE_BUTTON']),
    'gtk_toggle_button_set_inconsistent': ('e_gtk_toggle_button_set_inconsistent', ['GTK_TOGGLE_BUTTON']),
    # radio buttons are grouped GtkCheckButton-s
    'gtk_radio_button_new_with_mnemonic': ('e_gtk_radio_button_new_with_mnemonic', []),
    'gtk_radio_button_new_with_label': ('e_gtk_radio_button_new_with_label', []),
    'gtk_radio_button_new_with_mnemonic_from_widget': ('e_gtk_radio_button_new_with_mnemonic_from_widget', ['GTK_RADIO_BUTTON']),
    'gtk_radio_button_new_with_label_from_widget': ('e_gtk_radio_button_new_with_label_from_widget', ['GTK_RADIO_BUTTON']),
    'gtk_radio_button_get_group': ('e_gtk_radio_button_get_group', ['GTK_RADIO_BUTTON']),
    'gtk_radio_button_join_group': ('e_gtk_radio_button_join_group', ['GTK_RADIO_BUTTON']),
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
    'gtk_popover_set_relative_to': ('e_gtk_popover_set_relative_to', []),
    'gtk_icon_theme_load_icon': ('e_gtk_icon_theme_load_icon', []),
    'gdk_cairo_surface_create_from_pixbuf': ('e_gdk_cairo_surface_create_from_pixbuf', []),
    'gtk_icon_size_lookup': ('e_gtk_icon_size_lookup', []),
    'gtk_header_bar_set_title': ('e_gtk_header_bar_set_title', []),
    'gtk_targets_include_text': ('e_gtk_targets_include_text', []),
    'gdk_window_raise': ('gtk_window_present', []),
    'gtk_widget_class_install_style_property': ('e_gtk_widget_class_install_style_property', []),
    'gtk_widget_style_get': ('e_gtk_widget_style_get', []),
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
    'gtk_window_set_role',
    'gtk_window_set_type_hint',
    'gtk_toolbar_set_style',
    'gtk_toolbar_set_show_arrow',
    'gtk_toolbar_set_icon_size',
    'gtk_combo_box_set_wrap_width',
    'gtk_event_box_set_visible_window',
    'gtk_event_box_set_above_child',
    'gtk_window_set_position',
    'webkit_web_context_set_sandbox_enabled',
    'gtk_grab_add',
    'gtk_grab_remove',
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


def r_focus_child(name):
    def f(args, *_):
        args = [strip_cast(args[0], ['GTK_CONTAINER'])] + args[1:]
        return call(name, ['GTK_WIDGET (%s)' % args[0]] + args[1:])
    return f


def r_no_args(name):
    def f(args, *_):
        return '%s ()' % name
    return f


def r_popover_new(args, *_):
    if len(args) == 1 and args[0] not in ('', 'void'):
        return call('e_gtk_popover_new', args)
    return None


def r_editable_clipboard(action):
    def f(args, *_):
        return 'gtk_widget_activate_action (GTK_WIDGET (%s), "clipboard.%s", NULL)' % (strip_cast(args[0], ['GTK_EDITABLE']), action)
    return f


def r_icon_lookup(new):
    """GTK 3 lookups had 4 arguments and gave a GtkIconInfo."""
    def f(args, *_):
        if len(args) == 4:
            return call(new, args)
        return None
    return f


BINDING_TYPES = {
    'G_TYPE_INT': 'i', 'G_TYPE_UINT': 'u', 'G_TYPE_BOOLEAN': 'b',
    'G_TYPE_STRING': 's', 'G_TYPE_DOUBLE': 'd', 'G_TYPE_INT64': 'x',
}


def r_binding_signal(args, *_):
    """gtk_binding_entry_add_signal (set, key, mods, signal, n, type, value, ...)"""
    if len(args) < 5:
        return None
    try:
        n = int(args[4])
    except ValueError:
        return None
    rest = [a for a in args[5:] if a != 'NULL']
    if len(rest) != 2 * n:
        return None
    fmt = ''
    values = []
    for i in range(n):
        t, v = rest[2 * i], rest[2 * i + 1]
        # enums travel as ints
        fmt += BINDING_TYPES.get(t, 'i')
        values.append(v)
    cls = args[0]
    m = re.match(r'^gtk_binding_set_by_class\s*\((.*)\)$', cls, re.S)
    if m:
        cls = 'GTK_WIDGET_CLASS (%s)' % m.group(1).strip()
    if n == 0:
        return call('gtk_widget_class_add_binding_signal', [cls] + args[1:4] + ['NULL'])
    return call('gtk_widget_class_add_binding_signal', [cls] + args[1:4] + ['"(%s)"' % fmt] + values)


LEGACY_EVENT_SIGNALS = {
    '"key-press-event"', '"key-release-event"', '"button-press-event"',
    '"button-release-event"', '"motion-notify-event"', '"scroll-event"',
    '"enter-notify-event"', '"leave-notify-event"', '"focus-in-event"',
    '"focus-out-event"', '"popup-menu"',
}


def wrap_legacy_connects(text):
    """g_signal_connect (w, "key-press-event", ...) ->
    g_signal_connect (e_gtk_widget_legacy_events (w), "key-press-event", ...)"""
    pattern = re.compile(r'\bg_signal_connect(?:_swapped|_after|_object|_data)?\s*\(\s*')
    pos = 0
    out = []
    while True:
        m = pattern.search(text, pos)
        if not m:
            break
        # the first argument ends at the first top-level comma
        depth, i = 0, m.end()
        while i < len(text):
            c = text[i]
            if c in '([':
                depth += 1
            elif c in ')]':
                if depth == 0:
                    break
                depth -= 1
            elif c == ',' and depth == 0:
                break
            i += 1
        first = text[m.end():i]
        sig = re.match(r',\s*("[a-z_-]+")', text[i:])
        out.append(text[pos:m.end()])
        name = sig.group(1).replace('_', '-') if sig else None
        if (name in LEGACY_EVENT_SIGNALS and
                not first.startswith('e_gtk_widget_legacy_events')):
            out.append('e_gtk_widget_legacy_events (%s)' % first.strip())
            counts['legacy event connect'] += 1
        elif name == '"delete-event"' and not first.startswith('e_gtk_window_delete_events'):
            out.append('e_gtk_window_delete_events (%s)' % first.strip())
            counts['delete-event connect'] += 1
        elif name == '"style-updated"' and not first.startswith('e_gtk_widget_style_updates'):
            out.append('e_gtk_widget_style_updates (%s)' % first.strip())
            counts['style-updated connect'] += 1
        else:
            out.append(first)
        pos = i
    out.append(text[pos:])
    return ''.join(out)


def r_tooltip_context(args, *_):
    """GTK 4 takes the coordinates by value"""
    if len(args) == 7 and args[1].startswith('&') and args[2].startswith('&'):
        return call('gtk_tree_view_get_tooltip_context', [args[0], args[1][1:].strip(), args[2][1:].strip()] + args[3:])
    return None


def r_similar_surface(args, *_):
    if len(args) == 4:
        return call('cairo_image_surface_create', ['CAIRO_FORMAT_ARGB32', args[2], args[3]])
    return None


CALLS = {
    'gdk_window_create_similar_surface': r_similar_surface,
    'gdk_surface_create_similar_surface': r_similar_surface,
    'gtk_tree_view_get_tooltip_context': r_tooltip_context,
    'gtk_binding_entry_add_signal': r_binding_signal,
    'gtk_icon_theme_lookup_icon': r_icon_lookup('e_gtk_icon_theme_lookup_icon'),
    'gtk_icon_theme_lookup_by_gicon': r_icon_lookup('e_gtk_icon_theme_lookup_by_gicon'),
    'gtk_scrolled_window_new': r_no_args('gtk_scrolled_window_new'),
    'gtk_popover_new': r_popover_new,
    'gtk_editable_cut_clipboard': r_editable_clipboard('cut'),
    'gtk_editable_copy_clipboard': r_editable_clipboard('copy'),
    'gtk_editable_paste_clipboard': r_editable_clipboard('paste'),
    'gtk_container_get_focus_child': r_focus_child('gtk_widget_get_focus_child'),
    'gtk_container_set_focus_child': r_focus_child('gtk_widget_set_focus_child'),
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
    # keep the GTK 4 GtkDragSource/GtkDragIcon API
    (r'\bgtk_drag_(?!check_threshold\b|source_(?:new|set_actions|set_content|get_drag|get_actions|get_content|drag_cancel|set_icon)\b|icon_)([a-z_]+)\s*\(', r'e_gtk_drag_\1 ('),
    (r'\bgdk_drag_status\s*\(', 'e_gtk_drag_status ('),
    (r'\bgdk_drag_context_([a-z_]+)\s*\(', r'e_gtk_drag_context_\1 ('),
    (r'\bgtk_target_(list|table)_([a-z_]+)\s*\(', r'e_gtk_target_\1_\2 ('),
    (r'(?<!e_)\bgtk_(target_list|target_table|selection_data)_(unref|free|ref|copy)\b', r'e_gtk_\1_\2'),
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
    (r'\bgtk_popover_get_relative_to\s*\(\s*GTK_POPOVER\s*\(', 'gtk_widget_get_parent (GTK_WIDGET ('),
    (r'\bgtk_popover_get_relative_to\s*\(', 'gtk_widget_get_parent ('),
    (r'\bgtk_window_new \(GTK_WINDOW_TOPLEVEL\)', 'gtk_window_new ()'),
    (r'g_signal_connect \(\s*([^,]+),\s*"delete[-_]event",\s*G_CALLBACK \(gtk_widget_hide_on_delete\),\s*NULL\);', r'gtk_window_set_hide_on_close (GTK_WINDOW (\1), TRUE);'),
    (r'G_CALLBACK \(gtk_widget_destroyed\)', 'G_CALLBACK (e_gtk_widget_destroyed)'),
    (r'G_CALLBACK \(gtk_widget_destroy\)', 'G_CALLBACK (e_gtk_widget_destroy)'),
    (r'\(GDestroyNotify\) gtk_widget_destroy\b', '(GDestroyNotify) e_gtk_widget_destroy'),
    (r'(g_clear_pointer \([^,]+, )gtk_widget_destroy\)', r'\1e_gtk_widget_destroy)'),
    (r'\bGTK_IS_CONTAINER\b', 'GTK_IS_WIDGET'),
    # GtkBin subclasses -> EGtkBin of e-gtk4-compat
    (r'\bGTK_TYPE_BIN\b', 'E_GTK_TYPE_BIN'),
    (r'\bGTK_TYPE_EVENT_BOX\b', 'E_GTK_TYPE_BIN'),
    # the default modifier intents of GTK 3
    (r'\b(?:gtk_widget_get_modifier_mask|gdk_keymap_get_modifier_mask)\s*\((?:[^()]|\([^()]*\)|\((?:[^()]|\([^()]*\))*\))*?GDK_MODIFIER_INTENT_MODIFY_SELECTION\s*\)', 'GDK_CONTROL_MASK'),
    (r'\b(?:gtk_widget_get_modifier_mask|gdk_keymap_get_modifier_mask)\s*\((?:[^()]|\([^()]*\)|\((?:[^()]|\([^()]*\))*\))*?GDK_MODIFIER_INTENT_EXTEND_SELECTION\s*\)', 'GDK_SHIFT_MASK'),
    (r'\b(?:gtk_widget_get_modifier_mask|gdk_keymap_get_modifier_mask)\s*\((?:[^()]|\([^()]*\)|\((?:[^()]|\([^()]*\))*\))*?GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR\s*\)', 'GDK_CONTROL_MASK'),
    (r'\bgtk_container_check_resize\s*\(', 'gtk_widget_queue_allocate ('),
    # GtkLayout -> EGtkLayout of e-gtk4-compat
    (r'\bGTK_TYPE_LAYOUT\b', 'E_GTK_TYPE_LAYOUT'),
    (r'\bGtkLayout(Class)?\b', r'EGtkLayout\1'),
    (r'\bGTK_LAYOUT\s*\(', 'E_GTK_LAYOUT ('),
    (r'\bgtk_layout_(put|move|set_size|get_size|new)\s*\(', r'e_gtk_layout_\1 ('),
    (r'\bGtkEventBox(Class)?\b', r'EGtkBin\1'),
    (r'\bGtkBin(Class)?\b', r'EGtkBin\1'),
    (r'\n[ \t]*\w+->forall = \w+;', ''),
    # GTK 4 has no containers; any widget may have children
    (r'\bGtkContainer\b(?!Class)', 'GtkWidget'),
    (r'\bGTK_CONTAINER\s*\(', 'GTK_WIDGET ('),
    # WebKitGTK 6.0
    (r'\bWebKitJavascriptResult \*', 'JSCValue *'),
    (r'\bwebkit_javascript_result_get_js_value \((\w+)\)', r'(\1)'),
    (r'\bwebkit_javascript_result_unref\b', 'g_object_unref'),
    (r'\bwebkit_web_context_set_web_extensions_directory\b', 'webkit_web_context_set_web_process_extensions_directory'),
    (r'\bwebkit_web_context_set_web_extensions_initialization_user_data\b', 'webkit_web_context_set_web_process_extensions_initialization_user_data'),
    (r'(webkit_user_content_manager_(?:un)?register_script_message_handler \(\s*[^,()]+,\s*"[^"]*")\)', r'\1, NULL)'),
    (r'\bGDK_MOD[2-5]_MASK\b', '0'),
    (r'\bGDK_POINTER_MOTION_HINT_MASK\b', '0'),
    (r'(\w+) = e_gtk_widget_get_surface \(([^;]+)\);\n(\s*)gdk_window_set_cursor \(\1, ', r'\1 = NULL; /* GTK 4 sets cursors on widgets */\n\3gtk_widget_set_cursor (\2, '),
    (r'\bGdkWindow\b', 'GdkSurface'),
    (r'\bAtkObject\b', 'GtkWidget'),
    (r'\bgtk_widget_get_accessible\s*\(', 'GTK_WIDGET ('),
    (r'\batk_object_set_name\s*\(', 'e_gtk_widget_set_accessible_name ('),
    (r'\batk_object_set_description\s*\(', 'e_gtk_widget_set_accessible_description ('),
    (r'\bGtkRadioButton\b', 'GtkCheckButton'),
    (r'\bGtkFileChooserButton\b', 'EGtkFileChooserButton'),
    (r'\bGtkButtonBox\b', 'GtkBox'),
    (r'\bGTK_BUTTON_BOX\s*\(', 'GTK_BOX ('),
    (r'\bgtk_button_clicked \(\s*GTK_BUTTON \(([^()]*(?:\([^()]*\))?[^()]*)\)\s*\)', r'g_signal_emit_by_name (\1, "clicked")'),
    (r'\bGtkAccelGroup\b', 'EGtkAccelGroup'),
    (r'\bgtk_main \(\)', 'e_gtk_main ()'),
    (r'\bgtk_main_quit\b', 'e_gtk_main_quit'),
    (r'\bgtk_init_check \([^()]*\)', 'gtk_init_check ()'),
    (r'\bgtk_paned_add(1|2) \(\s*GTK_PANED \(([^()]*)\),\s*([^;]*)\);', lambda m: 'e_gtk_paned_pack%s (%s, %s, %s, TRUE);' % (m.group(1), m.group(2), m.group(3), 'FALSE' if m.group(1) == '1' else 'TRUE')),
    (r'\bgtk_widget_override_font\s*\(', 'e_gtk_widget_override_font ('),
    (r'\bGtkBindingSet \*', 'GtkWidgetClass *'),
    (r'\bgtk_button_box_set_child_secondary \(\s*GTK_BUTTON_BOX \(([^()]*)\)', r'e_gtk_button_box_set_child_secondary (\1'),
    (r'\n[ \t]*"layout-style", GTK_BUTTONBOX_[A-Z]+,', ''),
    # GTK 4 combo boxes have no grid popup
    (r'\bgtk_combo_box_get_wrap_width \([^()]*\)', '0'),
    (r'\bgtk_entry_get_width_chars \(\s*GTK_ENTRY \(([^()]*)\)\s*\)', r'gtk_editable_get_width_chars (GTK_EDITABLE (\1))'),
    # an event box only held a child
    (r'\bgtk_event_box_new \(\)', 'gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0)'),
    (r'\n[ \t]*"type", GTK_WINDOW_TOPLEVEL,', ''),
    # GTK 4 does not place windows
    (r'\n[ \t]*"window-position", GTK_WIN_POS_[A-Z_]+,', ''),
    (r'= gtk_binding_set_by_class \(([^;]*)\);', r'= GTK_WIDGET_CLASS (\1);'),
    (r'\bgtk_window_new \(\s*GTK_WINDOW_(TOPLEVEL|POPUP)\s*\)', 'gtk_window_new ()'),
    (r'\bgtk_container_child_(set|get)(_property)?\s*\(\s*GTK_CONTAINER\s*\(([^()]*)\)', r'e_gtk_container_child_\1\2 (\3'),
    (r'\bgtk_container_child_(set|get)(_property)?\s*\(', r'e_gtk_container_child_\1\2 ('),
    (r'\bGtkIconInfo\b', 'EGtkIconInfo'),
    (r'\bgtk_icon_info_(get_filename|load_icon|load_symbolic|is_symbolic)\s*\(', r'e_gtk_icon_info_\1 ('),
    (r'\bgtk_file_chooser_((?:get|set)_(?:preview_filename|preview_widget|preview_widget_active|extra_widget)|set_use_preview_label)\s*\(', r'e_gtk_file_chooser_\1 ('),
    (r'\bgtk_calendar_(get_date|select_month|select_day)\s*\(', r'e_gtk_calendar_\1 ('),
    (r'\bgtk_button_new_from_stock\s*\(', 'e_gtk_button_new_from_stock ('),
    (r'\bgtk_image_new_from_stock\s*\(', 'gtk_image_new_from_icon_name ('),
    (r'\bGTK_STOCK_OK\b', '_("_OK")'),
    (r'\bGTK_STOCK_CANCEL\b', '_("_Cancel")'),
    (r'\bgtk_style_context_add_provider_for_screen \(\s*gdk_screen_get_default \(\)', 'gtk_style_context_add_provider_for_display (gdk_display_get_default ()'),
    (r'\bgtk_style_context_remove_provider_for_screen \(\s*gdk_screen_get_default \(\)', 'gtk_style_context_remove_provider_for_display (gdk_display_get_default ()'),
    (r'\bgtk_icon_theme_(append|prepend)_search_path\s*\(', 'gtk_icon_theme_add_search_path ('),
    (r'\n[ \t]*"type-hint", GDK_WINDOW_TYPE_HINT_[A-Z_]+,[^\n]*', ''),
    (r'\bgtk_icon_theme_load_icon_for_scale\s*\(', 'e_gtk_icon_theme_load_icon_for_scale ('),
    (r'\bgtk_render_icon_surface\s*\(', 'e_gtk_render_icon_surface ('),
    (r'\bGTK_ICON_LOOKUP_(FORCE_SIZE|USE_BUILTIN|GENERIC_FALLBACK|NO_SVG|FORCE_SVG)\b', '0'),
    (r'\bGTK_ACCEL_VISIBLE\b', '0'),
    (r'\bgtk_accel_group_new\s*\(', 'e_gtk_accel_group_new ('),
    (r'\bgtk_widget_add_accelerator\s*\(', 'e_gtk_widget_add_accelerator ('),
    (r'\bgtk_window_(add|remove)_accel_group\s*\(', r'e_gtk_window_\1_accel_group ('),
    (r'\bGTK_RADIO_BUTTON\s*\(', 'GTK_CHECK_BUTTON ('),
    (r'\bGTK_IS_RADIO_BUTTON\s*\(', 'GTK_IS_CHECK_BUTTON ('),
    (r'\bGTK_TYPE_RADIO_BUTTON\b', 'GTK_TYPE_CHECK_BUTTON'),
    # GTK 3 drag vfuncs -> overrides of the drag-* signals of e-gtk-dnd-compat
    (r'(\w+)->(drag_(?:begin|end|data_get|data_delete|failed|leave|motion|drop|data_received))\s*=\s*(\w+);',
     lambda m: 'g_signal_override_class_handler ("%s", G_TYPE_FROM_CLASS (%s), G_CALLBACK (%s));' % (m.group(2).replace('_', '-'), m.group(1), m.group(3))),
    # GTK 3 event vfuncs -> overrides of the emulated signals
    (r'(\w+)->(key_press_event|key_release_event|button_press_event|button_release_event|motion_notify_event|scroll_event|enter_notify_event|leave_notify_event|focus_in_event|focus_out_event|popup_menu)\s*=\s*(\w+);',
     lambda m: 'e_gtk_widget_class_override_legacy_event (GTK_WIDGET_CLASS (%s), "%s", G_CALLBACK (%s));' % (m.group(1), m.group(2).replace('_', '-'), m.group(3))),
    (r'(?:\(\s*\*\s*)?(?:\w+|GTK_WIDGET_CLASS\s*\(\s*\w+\s*\))\s*->\s*popup_menu\s*\)?\s*\(\s*(\w+)\s*\)',
     r'e_gtk_widget_chain_legacy_popup_menu (\1)'),
    (r'(?:\(\s*\*\s*)?(?:\w+|GTK_WIDGET_CLASS\s*\(\s*\w+\s*\))\s*->\s*(?:key_press_event|key_release_event|button_press_event|button_release_event|motion_notify_event|scroll_event|enter_notify_event|leave_notify_event|focus_in_event|focus_out_event)\s*\)?\s*\(\s*(\w+)\s*,\s*([^;()]+?)\s*\)',
     r'e_gtk_widget_chain_legacy_event (\1, \2)'),
    # GtkAssistant is final -> EAssistant (src/e-util/e-assistant.[ch])
    (r'\bGtkAssistant(Class|PageFunc)?\b', r'EAssistant\1'),
    (r'\bGTK_TYPE_ASSISTANT\b', 'E_TYPE_ASSISTANT'),
    (r'\bGTK_(IS_)?ASSISTANT(_CLASS)?\s*\(', r'E_\1ASSISTANT\2 ('),
    (r'\bgtk_assistant_([a-z_]+)\s*\(', r'e_assistant_\1 ('),
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
    text = wrap_legacy_connects(text)
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
