#!/usr/bin/env bash
# Counts GTK3-only API still used under src/, per pattern and per top-level
# directory, so the GTK4 port has a number that goes down.
#
#   doxmail/gtk4-audit.sh            table by pattern
#   doxmail/gtk4-audit.sh --dirs     table by directory (all patterns summed)
#   doxmail/gtk4-audit.sh --where P  files using pattern name P, most hits first
set -euo pipefail
cd "$(dirname "$0")/../src"

# Directories that already build with WITH_GTK4; their GTK 3 code is the
# #else side of version checks, kept so the GTK 3 build keeps working.
PORTED=(libgnomecanvas doxmail-tests)

# name|extended regex. Each one is gone or reworked in GTK4.
PATTERNS=(
	'container|gtk_container_[a-z_]+|GTK_CONTAINER|GtkContainer'
	'box_pack|gtk_box_pack_(start|end)'
	'show_all|gtk_widget_show_all'
	'widget_destroy|gtk_widget_destroy'
	'dialog_run|gtk_dialog_run'
	'menu|gtk_menu_[a-z_]+|GtkMenu(Item|Shell)?\b|GTK_MENU\b'
	'toolbar|gtk_tool(bar|_item|_button)_[a-z_]+|GtkTool(bar|Item|Button)'
	'gdk_window|GdkWindow|gtk_widget_get_window'
	'gdk_event|GdkEvent[A-Z][a-z]+|gdk_event_(new|get_coords|get_root_coords|get_button|get_keyval|get_state|free)'
	'event_signals|"(button|key)-(press|release)-event"|"(motion-notify|scroll|enter-notify|leave-notify|focus-in|focus-out|delete)-event"'
	'draw_signal|"draw"|gtk_widget_queue_draw_area'
	'clipboard|GtkClipboard|gtk_clipboard_[a-z_]+'
	'dnd|gtk_drag_[a-z_]+|GtkTargetList|GtkTargetEntry|"drag-[a-z-]+"'
	'canvas_event|GDK_[23]BUTTON_PRESS|gnome_canvas_item_grab \([^)]*GDK_'
	'layout|GtkLayout|gtk_layout_[a-z_]+|GTK_LAYOUT'
	'event_box|GtkEventBox|gtk_event_box_[a-z_]+'
	'style_context|gtk_style_context_[a-z_]+'
	'window_pos|gtk_window_(set_position|move|get_position)'
	'size_req|gtk_widget_(get_preferred|size_request)[a-z_]*'
	'webkit2|webkit_web_view_[a-z_]+|WebKitWebView|webkit_web_extension|WebKitWebExtension'
	'icon_size|GTK_ICON_SIZE_(MENU|SMALL_TOOLBAR|LARGE_TOOLBAR|BUTTON|DND|DIALOG)'
	'alignment|gtk_(misc|alignment)_[a-z_]+'
)

EXCLUDE=()
for d in "${PORTED[@]}"; do EXCLUDE+=(--exclude-dir="$d"); done

grep_count() { grep -rEo "${EXCLUDE[@]}" --include='*.c' --include='*.h' "$1" "${2:-.}" 2>/dev/null | wc -l || true; }

case "${1:-}" in
--dirs)
	all=$(printf '%s\n' "${PATTERNS[@]}" | cut -d'|' -f2- | paste -sd'|')
	for d in */; do
		d=${d%/}
		if printf '%s\n' "${PORTED[@]}" | grep -qx "$d"; then
			printf '%-20s %7s\n' "$d" ported
		else
			printf '%-20s %7d\n' "$d" "$(grep_count "$all" "$d")"
		fi
	done | sort -k2 -nr
	;;
--where)
	re=$(printf '%s\n' "${PATTERNS[@]}" | awk -F'|' -v n="$2" '$1 == n { sub(/^[^|]*\|/, ""); print }')
	[ -n "$re" ] || { echo "unknown pattern: $2" >&2; exit 1; }
	grep -rEc "${EXCLUDE[@]}" --include='*.c' --include='*.h' "$re" . | { grep -v ':0$' || true; } | sort -t: -k2 -nr
	;;
*)
	total=0
	for p in "${PATTERNS[@]}"; do
		n=$(grep_count "${p#*|}")
		total=$((total + n))
		printf '%-15s %7d\n' "${p%%|*}" "$n"
	done
	printf '%-15s %7d\n' TOTAL "$total"
	;;
esac
