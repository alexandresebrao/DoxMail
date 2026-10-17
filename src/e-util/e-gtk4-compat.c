/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include "e-gtk4-compat.h"

#define BORDER_WIDTH_KEY "e-gtk-border-width"
#define INNER_PANED_KEY "e-gtk-inner-paned"
#define END_BOX_KEY "e-gtk-box-end-box"

/* The single child of the "bin" widgets, i.e. those with get/set_child() */
static gboolean
bin_child (GtkWidget *bin,
           GtkWidget **child,
           GtkWidget *set_child,
           gboolean do_set)
{
	#define BIN(check, type, getter, setter) \
		if (check (bin)) { \
			if (do_set) \
				setter ((type *) bin, set_child); \
			else \
				*child = getter ((type *) bin); \
			return TRUE; \
		}

	BIN (GTK_IS_WINDOW, GtkWindow, gtk_window_get_child, gtk_window_set_child)
	BIN (GTK_IS_SCROLLED_WINDOW, GtkScrolledWindow, gtk_scrolled_window_get_child, gtk_scrolled_window_set_child)
	BIN (GTK_IS_VIEWPORT, GtkViewport, gtk_viewport_get_child, gtk_viewport_set_child)
	BIN (GTK_IS_FRAME, GtkFrame, gtk_frame_get_child, gtk_frame_set_child)
	BIN (GTK_IS_BUTTON, GtkButton, gtk_button_get_child, gtk_button_set_child)
	BIN (GTK_IS_CHECK_BUTTON, GtkCheckButton, gtk_check_button_get_child, gtk_check_button_set_child)
	BIN (GTK_IS_MENU_BUTTON, GtkMenuButton, gtk_menu_button_get_child, gtk_menu_button_set_child)
	BIN (GTK_IS_POPOVER, GtkPopover, gtk_popover_get_child, gtk_popover_set_child)
	BIN (GTK_IS_EXPANDER, GtkExpander, gtk_expander_get_child, gtk_expander_set_child)
	BIN (GTK_IS_REVEALER, GtkRevealer, gtk_revealer_get_child, gtk_revealer_set_child)
	BIN (GTK_IS_OVERLAY, GtkOverlay, gtk_overlay_get_child, gtk_overlay_set_child)
	BIN (GTK_IS_ASPECT_FRAME, GtkAspectFrame, gtk_aspect_frame_get_child, gtk_aspect_frame_set_child)
	BIN (GTK_IS_SEARCH_BAR, GtkSearchBar, gtk_search_bar_get_child, gtk_search_bar_set_child)
	BIN (GTK_IS_LIST_BOX_ROW, GtkListBoxRow, gtk_list_box_row_get_child, gtk_list_box_row_set_child)
	BIN (GTK_IS_FLOW_BOX_CHILD, GtkFlowBoxChild, gtk_flow_box_child_get_child, gtk_flow_box_child_set_child)
	BIN (GTK_IS_WINDOW_HANDLE, GtkWindowHandle, gtk_window_handle_get_child, gtk_window_handle_set_child)

	#undef BIN

	return FALSE;
}

static void
apply_border_width (GtkWidget *widget,
                    guint border_width)
{
	gtk_widget_set_margin_start (widget, border_width);
	gtk_widget_set_margin_end (widget, border_width);
	gtk_widget_set_margin_top (widget, border_width);
	gtk_widget_set_margin_bottom (widget, border_width);
}

GtkPaned *
e_gtk_paned_get_inner (gpointer paned)
{
	if (GTK_IS_PANED (paned))
		return paned;

	return G_IS_OBJECT (paned) ? g_object_get_data (paned, INNER_PANED_KEY) : NULL;
}

void
e_gtk_container_add (gpointer container,
                     GtkWidget *child)
{
	GtkWidget *widget = container;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (e_gtk_paned_get_inner (widget))
		widget = GTK_WIDGET (e_gtk_paned_get_inner (widget));

	if (GTK_IS_BOX (widget)) {
		gtk_box_append (GTK_BOX (widget), child);
	} else if (GTK_IS_LIST_BOX (widget)) {
		gtk_list_box_append (GTK_LIST_BOX (widget), child);
	} else if (GTK_IS_FLOW_BOX (widget)) {
		gtk_flow_box_append (GTK_FLOW_BOX (widget), child);
	} else if (GTK_IS_GRID (widget)) {
		GtkOrientation orientation;

		orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
		gtk_grid_attach_next_to (
			GTK_GRID (widget), child, NULL,
			orientation == GTK_ORIENTATION_HORIZONTAL ?
			GTK_POS_RIGHT : GTK_POS_BOTTOM, 1, 1);
	} else if (GTK_IS_STACK (widget)) {
		gtk_stack_add_child (GTK_STACK (widget), child);
	} else if (GTK_IS_NOTEBOOK (widget)) {
		gtk_notebook_append_page (GTK_NOTEBOOK (widget), child, NULL);
	} else if (GTK_IS_PANED (widget)) {
		if (!gtk_paned_get_start_child (GTK_PANED (widget)))
			gtk_paned_set_start_child (GTK_PANED (widget), child);
		else
			gtk_paned_set_end_child (GTK_PANED (widget), child);
	} else if (GTK_IS_INFO_BAR (widget)) {
		gtk_info_bar_add_child (GTK_INFO_BAR (widget), child);
	} else if (GTK_IS_ACTION_BAR (widget)) {
		gtk_action_bar_pack_start (GTK_ACTION_BAR (widget), child);
	} else if (GTK_IS_HEADER_BAR (widget)) {
		gtk_header_bar_pack_start (GTK_HEADER_BAR (widget), child);
	} else if (bin_child (widget, NULL, child, TRUE)) {
		/* a window's border width applies to its child */
		if (GTK_IS_WINDOW (widget)) {
			guint border_width;

			border_width = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), BORDER_WIDTH_KEY));
			if (border_width)
				apply_border_width (child, border_width);
		}
	} else {
		g_warning ("%s: don't know how to add a child to %s",
			G_STRFUNC, G_OBJECT_TYPE_NAME (widget));
		gtk_widget_set_parent (child, widget);
	}
}

void
e_gtk_container_remove (gpointer container,
                        GtkWidget *child)
{
	GtkWidget *widget = container;
	GtkWidget *parent;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (e_gtk_paned_get_inner (widget))
		widget = GTK_WIDGET (e_gtk_paned_get_inner (widget));

	parent = gtk_widget_get_parent (child);

	if (GTK_IS_BOX (widget)) {
		/* children packed with e_gtk_box_pack_end() sit in an inner box */
		if (parent && parent != widget &&
		    parent == g_object_get_data (G_OBJECT (widget), END_BOX_KEY))
			gtk_box_remove (GTK_BOX (parent), child);
		else
			gtk_box_remove (GTK_BOX (widget), child);
	} else if (GTK_IS_LIST_BOX (widget)) {
		gtk_list_box_remove (GTK_LIST_BOX (widget), child);
	} else if (GTK_IS_FLOW_BOX (widget)) {
		gtk_flow_box_remove (GTK_FLOW_BOX (widget), child);
	} else if (GTK_IS_GRID (widget)) {
		gtk_grid_remove (GTK_GRID (widget), child);
	} else if (GTK_IS_STACK (widget)) {
		gtk_stack_remove (GTK_STACK (widget), child);
	} else if (GTK_IS_NOTEBOOK (widget)) {
		gint page = gtk_notebook_page_num (GTK_NOTEBOOK (widget), child);

		if (page >= 0)
			gtk_notebook_remove_page (GTK_NOTEBOOK (widget), page);
	} else if (GTK_IS_PANED (widget)) {
		if (gtk_paned_get_start_child (GTK_PANED (widget)) == child)
			gtk_paned_set_start_child (GTK_PANED (widget), NULL);
		else if (gtk_paned_get_end_child (GTK_PANED (widget)) == child)
			gtk_paned_set_end_child (GTK_PANED (widget), NULL);
	} else if (GTK_IS_INFO_BAR (widget)) {
		gtk_info_bar_remove_child (GTK_INFO_BAR (widget), child);
	} else if (GTK_IS_ACTION_BAR (widget)) {
		gtk_action_bar_remove (GTK_ACTION_BAR (widget), child);
	} else if (GTK_IS_HEADER_BAR (widget)) {
		gtk_header_bar_remove (GTK_HEADER_BAR (widget), child);
	} else {
		GtkWidget *current = NULL;

		if (bin_child (widget, &current, NULL, FALSE)) {
			/* a scrolled window may have wrapped the child in a viewport */
			if (current == child || (current && parent == current))
				bin_child (widget, NULL, NULL, TRUE);
		} else if (parent == widget) {
			gtk_widget_unparent (child);
		}
	}
}

GList *
e_gtk_widget_get_children (gpointer widget)
{
	GtkWidget *container = widget;
	GtkWidget *child = NULL;
	GList *children = NULL;

	g_return_val_if_fail (GTK_IS_WIDGET (container), NULL);

	if (bin_child (container, &child, NULL, FALSE))
		return child ? g_list_prepend (NULL, child) : NULL;

	if (GTK_IS_NOTEBOOK (container)) {
		gint ii, n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (container));

		for (ii = n_pages - 1; ii >= 0; ii--)
			children = g_list_prepend (children, gtk_notebook_get_nth_page (GTK_NOTEBOOK (container), ii));

		return children;
	}

	if (e_gtk_paned_get_inner (container))
		container = GTK_WIDGET (e_gtk_paned_get_inner (container));

	if (GTK_IS_PANED (container)) {
		if (gtk_paned_get_end_child (GTK_PANED (container)))
			children = g_list_prepend (children, gtk_paned_get_end_child (GTK_PANED (container)));
		if (gtk_paned_get_start_child (GTK_PANED (container)))
			children = g_list_prepend (children, gtk_paned_get_start_child (GTK_PANED (container)));

		return children;
	}

	for (child = gtk_widget_get_last_child (container); child; child = gtk_widget_get_prev_sibling (child)) {
		/* list the e_gtk_box_pack_end() children in place of their box */
		if (child == g_object_get_data (G_OBJECT (container), END_BOX_KEY)) {
			GtkWidget *end_child;

			for (end_child = gtk_widget_get_last_child (child); end_child; end_child = gtk_widget_get_prev_sibling (end_child))
				children = g_list_prepend (children, end_child);
		} else {
			children = g_list_prepend (children, child);
		}
	}

	return children;
}

void
e_gtk_container_foreach (gpointer container,
                         EGtkCallback callback,
                         gpointer user_data)
{
	GList *children, *link;

	children = e_gtk_widget_get_children (container);

	for (link = children; link; link = g_list_next (link))
		callback (link->data, user_data);

	g_list_free (children);
}

void
e_gtk_widget_set_border_width (gpointer widget,
                               guint border_width)
{
	GtkWidget *child = NULL;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	/* GTK 3 border width was space inside the container; margins are
	 * outside the widget, so a window gets them on its child. */
	if (GTK_IS_WINDOW (widget)) {
		g_object_set_data (G_OBJECT (widget), BORDER_WIDTH_KEY, GUINT_TO_POINTER (border_width));

		child = gtk_window_get_child (GTK_WINDOW (widget));
		if (child)
			apply_border_width (child, border_width);
		return;
	}

	apply_border_width (widget, border_width);
}

GtkWidget *
e_gtk_bin_get_child (gpointer bin)
{
	GtkWidget *child = NULL;

	g_return_val_if_fail (GTK_IS_WIDGET (bin), NULL);

	if (!bin_child (bin, &child, NULL, FALSE))
		child = gtk_widget_get_first_child (bin);

	return child;
}

static void
box_pack (GtkBox *box,
          GtkWidget *child,
          gboolean expand,
          gboolean fill,
          guint padding)
{
	if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL) {
		if (expand) {
			gtk_widget_set_hexpand (child, TRUE);
			if (!fill)
				gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
		}
		if (padding) {
			gtk_widget_set_margin_start (child, gtk_widget_get_margin_start (child) + padding);
			gtk_widget_set_margin_end (child, gtk_widget_get_margin_end (child) + padding);
		}
	} else {
		if (expand) {
			gtk_widget_set_vexpand (child, TRUE);
			if (!fill)
				gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
		}
		if (padding) {
			gtk_widget_set_margin_top (child, gtk_widget_get_margin_top (child) + padding);
			gtk_widget_set_margin_bottom (child, gtk_widget_get_margin_bottom (child) + padding);
		}
	}
}

/* With GTK 3 the end children got the space no start child took; the
 * end box takes it only while no start child expands. */
static void
update_end_box_expand (GtkBox *box)
{
	GtkWidget *end_box, *child;
	gboolean horizontal, start_expands = FALSE;

	end_box = g_object_get_data (G_OBJECT (box), END_BOX_KEY);
	if (!end_box)
		return;

	horizontal = gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL;

	for (child = gtk_widget_get_first_child (GTK_WIDGET (box)); child; child = gtk_widget_get_next_sibling (child)) {
		if (child != end_box && (horizontal ? gtk_widget_get_hexpand (child) : gtk_widget_get_vexpand (child))) {
			start_expands = TRUE;
			break;
		}
	}

	if (horizontal)
		gtk_widget_set_hexpand (end_box, !start_expands);
	else
		gtk_widget_set_vexpand (end_box, !start_expands);
}

void
e_gtk_box_pack_start (GtkBox *box,
                      GtkWidget *child,
                      gboolean expand,
                      gboolean fill,
                      guint padding)
{
	GtkWidget *end_box;

	g_return_if_fail (GTK_IS_BOX (box));
	g_return_if_fail (GTK_IS_WIDGET (child));

	box_pack (box, child, expand, fill, padding);

	end_box = g_object_get_data (G_OBJECT (box), END_BOX_KEY);
	if (end_box)
		gtk_box_insert_child_after (box, child, gtk_widget_get_prev_sibling (end_box));
	else
		gtk_box_append (box, child);

	update_end_box_expand (box);
}

void
e_gtk_box_pack_end (GtkBox *box,
                    GtkWidget *child,
                    gboolean expand,
                    gboolean fill,
                    guint padding)
{
	GtkWidget *end_box;

	g_return_if_fail (GTK_IS_BOX (box));
	g_return_if_fail (GTK_IS_WIDGET (child));

	end_box = g_object_get_data (G_OBJECT (box), END_BOX_KEY);
	if (!end_box) {
		GtkOrientation orientation;

		orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (box));
		end_box = gtk_box_new (orientation, gtk_box_get_spacing (box));
		if (orientation == GTK_ORIENTATION_HORIZONTAL)
			gtk_widget_set_halign (end_box, GTK_ALIGN_END);
		else
			gtk_widget_set_valign (end_box, GTK_ALIGN_END);
		gtk_box_append (box, end_box);
		g_object_set_data (G_OBJECT (box), END_BOX_KEY, end_box);
	}

	box_pack (box, child, expand, fill, padding);

	/* later pack_end() children go before the earlier ones */
	gtk_box_prepend (GTK_BOX (end_box), child);

	update_end_box_expand (box);
}

void
e_gtk_box_reorder_child (GtkBox *box,
                         GtkWidget *child,
                         gint position)
{
	GtkWidget *sibling = NULL, *iter;
	gint index = 0;

	g_return_if_fail (GTK_IS_BOX (box));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (position < 0) {
		for (iter = gtk_widget_get_last_child (GTK_WIDGET (box)); iter == child; iter = gtk_widget_get_prev_sibling (iter))
			;
		gtk_box_reorder_child_after (box, child, iter);
		return;
	}

	for (iter = gtk_widget_get_first_child (GTK_WIDGET (box)); iter && index < position; iter = gtk_widget_get_next_sibling (iter)) {
		if (iter == child)
			continue;
		sibling = iter;
		index++;
	}

	gtk_box_reorder_child_after (box, child, sibling);
}

GtkWidget *
e_gtk_button_box_new (GtkOrientation orientation)
{
	GtkWidget *box;

	box = gtk_box_new (orientation, 6);
	gtk_box_set_homogeneous (GTK_BOX (box), TRUE);

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		gtk_widget_set_halign (box, GTK_ALIGN_END);
	else
		gtk_widget_set_valign (box, GTK_ALIGN_START);

	return box;
}

void
e_gtk_paned_pack1 (gpointer paned,
                   GtkWidget *child,
                   gboolean resize,
                   gboolean shrink)
{
	GtkPaned *inner = e_gtk_paned_get_inner (paned);

	g_return_if_fail (GTK_IS_PANED (inner));

	gtk_paned_set_start_child (inner, child);
	gtk_paned_set_resize_start_child (inner, resize);
	gtk_paned_set_shrink_start_child (inner, shrink);
}

void
e_gtk_paned_pack2 (gpointer paned,
                   GtkWidget *child,
                   gboolean resize,
                   gboolean shrink)
{
	GtkPaned *inner = e_gtk_paned_get_inner (paned);

	g_return_if_fail (GTK_IS_PANED (inner));

	gtk_paned_set_end_child (inner, child);
	gtk_paned_set_resize_end_child (inner, resize);
	gtk_paned_set_shrink_end_child (inner, shrink);
}

GtkWidget *
e_gtk_paned_get_child1 (gpointer paned)
{
	GtkPaned *inner = e_gtk_paned_get_inner (paned);

	g_return_val_if_fail (GTK_IS_PANED (inner), NULL);

	return gtk_paned_get_start_child (inner);
}

GtkWidget *
e_gtk_paned_get_child2 (gpointer paned)
{
	GtkPaned *inner = e_gtk_paned_get_inner (paned);

	g_return_val_if_fail (GTK_IS_PANED (inner), NULL);

	return gtk_paned_get_end_child (inner);
}

gint
e_gtk_paned_get_position (gpointer paned)
{
	GtkPaned *inner = e_gtk_paned_get_inner (paned);

	g_return_val_if_fail (GTK_IS_PANED (inner), 0);

	return gtk_paned_get_position (inner);
}

void
e_gtk_paned_set_position (gpointer paned,
                          gint position)
{
	GtkPaned *inner = e_gtk_paned_get_inner (paned);

	g_return_if_fail (GTK_IS_PANED (inner));

	gtk_paned_set_position (inner, position);
}

void
e_gtk_widget_destroy (gpointer widget)
{
	GtkWidget *parent, *ancestor;

	if (!widget)
		return;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (GTK_IS_WINDOW (widget)) {
		gtk_window_destroy (widget);
		return;
	}

	parent = gtk_widget_get_parent (widget);
	if (!parent) {
		/* what gtk_widget_destroy() did to an unparented widget */
		if (g_object_is_floating (widget))
			g_object_ref_sink (widget);
		else
			g_object_ref (widget);
		g_object_run_dispose (widget);
		g_object_unref (widget);
		return;
	}

	/* notebook pages live in an internal stack */
	for (ancestor = parent; ancestor; ancestor = gtk_widget_get_parent (ancestor)) {
		if (GTK_IS_NOTEBOOK (ancestor) &&
		    gtk_notebook_page_num (GTK_NOTEBOOK (ancestor), widget) >= 0) {
			e_gtk_container_remove (ancestor, widget);
			return;
		}
	}

	/* a viewport added by a scrolled window, or the end box of a box */
	if (GTK_IS_VIEWPORT (parent) && GTK_IS_SCROLLED_WINDOW (gtk_widget_get_parent (parent)))
		parent = gtk_widget_get_parent (parent);
	else if (GTK_IS_BOX (parent) && gtk_widget_get_parent (parent) &&
		 g_object_get_data (G_OBJECT (gtk_widget_get_parent (parent)), END_BOX_KEY) == parent)
		parent = gtk_widget_get_parent (parent);

	e_gtk_container_remove (parent, widget);
}

GtkWidget *
e_gtk_widget_get_toplevel (gpointer widget)
{
	GtkWidget *toplevel = widget, *parent;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	while ((parent = gtk_widget_get_parent (toplevel)) != NULL)
		toplevel = parent;

	return toplevel;
}

gboolean
e_gtk_widget_is_toplevel (gpointer widget)
{
	return GTK_IS_ROOT (widget);
}

void
e_gtk_widget_grab_default (GtkWidget *widget)
{
	GtkRoot *root;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	root = gtk_widget_get_root (widget);
	if (GTK_IS_WINDOW (root))
		gtk_window_set_default_widget (GTK_WINDOW (root), widget);
}

void
e_gtk_widget_get_preferred_width (GtkWidget *widget,
                                  gint *minimum,
                                  gint *natural)
{
	gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, minimum, natural, NULL, NULL);
}

void
e_gtk_widget_get_preferred_height (GtkWidget *widget,
                                   gint *minimum,
                                   gint *natural)
{
	gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, minimum, natural, NULL, NULL);
}

void
e_gtk_widget_get_preferred_height_for_width (GtkWidget *widget,
                                             gint width,
                                             gint *minimum,
                                             gint *natural)
{
	gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, width, minimum, natural, NULL, NULL);
}

GdkSurface *
e_gtk_widget_get_surface (GtkWidget *widget)
{
	GtkNative *native;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	native = gtk_widget_get_native (widget);

	return native ? gtk_native_get_surface (native) : NULL;
}

/* The monitor the @widget is on, or the first one; GTK 4 has no work area,
 * thus this is the whole monitor */
void
e_gtk_widget_get_monitor_area (GtkWidget *widget,
                               GdkRectangle *out_area)
{
	GdkDisplay *display;
	GdkMonitor *monitor = NULL;
	GdkSurface *surface;

	g_return_if_fail (out_area != NULL);

	out_area->x = 0;
	out_area->y = 0;
	out_area->width = 1024;
	out_area->height = 768;

	display = widget ? gtk_widget_get_display (widget) : gdk_display_get_default ();
	surface = widget ? e_gtk_widget_get_surface (widget) : NULL;

	if (surface)
		monitor = gdk_display_get_monitor_at_surface (display, surface);

	if (monitor) {
		g_object_ref (monitor);
	} else {
		monitor = g_list_model_get_item (gdk_display_get_monitors (display), 0);
	}

	if (monitor) {
		gdk_monitor_get_geometry (monitor, out_area);
		g_object_unref (monitor);
	}
}

void
e_gtk_button_set_image (GtkButton *button,
                        GtkWidget *image)
{
	const gchar *label;

	g_return_if_fail (GTK_IS_BUTTON (button));

	label = gtk_button_get_label (button);

	if (image && label && *label) {
		GtkWidget *box, *widget;

		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
		gtk_box_append (GTK_BOX (box), image);

		widget = gtk_label_new_with_mnemonic (label);
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget), GTK_WIDGET (button));
		gtk_box_append (GTK_BOX (box), widget);

		gtk_button_set_child (button, box);
	} else {
		gtk_button_set_child (button, image);
	}
}

void
e_gtk_button_set_label_and_icon (GtkButton *button,
                                 const gchar *label,
                                 const gchar *icon_name)
{
	g_return_if_fail (GTK_IS_BUTTON (button));

	gtk_button_set_label (button, label);
	gtk_button_set_use_underline (button, TRUE);

	if (icon_name)
		e_gtk_button_set_image (button, gtk_image_new_from_icon_name (icon_name));
}

typedef struct {
	GMainLoop *loop;
	gint response_id;
} RunInfo;

static void
run_response_cb (gpointer dialog,
                 gint response_id,
                 RunInfo *ri)
{
	ri->response_id = response_id;
	if (g_main_loop_is_running (ri->loop))
		g_main_loop_quit (ri->loop);
}

static void
run_unmap_cb (GtkWidget *dialog,
              RunInfo *ri)
{
	if (g_main_loop_is_running (ri->loop))
		g_main_loop_quit (ri->loop);
}

/* gtk_dialog_run() as GTK 3 had it: show modal, spin a main loop until a
 * response.  New code should connect to GtkDialog::response instead. */
gint
e_gtk_dialog_run (GtkDialog *dialog)
{
	RunInfo ri = { NULL, GTK_RESPONSE_NONE };
	gulong response_handler, unmap_handler, destroy_handler;

	g_return_val_if_fail (GTK_IS_DIALOG (dialog), GTK_RESPONSE_NONE);

	g_object_ref (dialog);

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	response_handler = g_signal_connect (dialog, "response", G_CALLBACK (run_response_cb), &ri);
	unmap_handler = g_signal_connect (dialog, "unmap", G_CALLBACK (run_unmap_cb), &ri);
	destroy_handler = g_signal_connect (dialog, "destroy", G_CALLBACK (run_unmap_cb), &ri);

	ri.loop = g_main_loop_new (NULL, FALSE);

	gtk_window_present (GTK_WINDOW (dialog));
	g_main_loop_run (ri.loop);

	g_signal_handler_disconnect (dialog, response_handler);
	g_signal_handler_disconnect (dialog, unmap_handler);
	g_signal_handler_disconnect (dialog, destroy_handler);

	g_main_loop_unref (ri.loop);
	g_object_unref (dialog);

	return ri.response_id;
}

gint
e_gtk_native_dialog_run (GtkNativeDialog *dialog)
{
	RunInfo ri = { NULL, GTK_RESPONSE_NONE };
	gulong response_handler;

	g_return_val_if_fail (GTK_IS_NATIVE_DIALOG (dialog), GTK_RESPONSE_NONE);

	g_object_ref (dialog);

	gtk_native_dialog_set_modal (dialog, TRUE);
	response_handler = g_signal_connect (dialog, "response", G_CALLBACK (run_response_cb), &ri);

	ri.loop = g_main_loop_new (NULL, FALSE);

	gtk_native_dialog_show (dialog);
	g_main_loop_run (ri.loop);

	g_signal_handler_disconnect (dialog, response_handler);

	g_main_loop_unref (ri.loop);
	g_object_unref (dialog);

	return ri.response_id;
}

/* GTK 4 keeps the action area private; buttons go through
 * gtk_dialog_add_action_widget(), other widgets into this box, which is
 * added at the bottom of the content area on first use. */
GtkWidget *
e_gtk_dialog_get_action_area (GtkDialog *dialog)
{
	GtkWidget *area;

	g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

	area = g_object_get_data (G_OBJECT (dialog), "e-gtk-action-area");
	if (!area) {
		area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_halign (area, GTK_ALIGN_END);
		gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (dialog)), area);
		g_object_set_data (G_OBJECT (dialog), "e-gtk-action-area", area);
	}

	return area;
}

gint
e_gtk_icon_size_to_pixels (gint gtk3_icon_size)
{
	switch (gtk3_icon_size) {
	case E_GTK_ICON_SIZE_LARGE_TOOLBAR:
		return 24;
	case E_GTK_ICON_SIZE_DND:
		return 32;
	case E_GTK_ICON_SIZE_DIALOG:
		return 48;
	default:
		return 16;
	}
}

GtkIconTheme *
e_gtk_icon_theme_get_default (void)
{
	return gtk_icon_theme_get_for_display (gdk_display_get_default ());
}

static GdkEvent *current_event = NULL;

static gboolean
track_current_event_cb (GtkEventControllerLegacy *controller,
                        GdkEvent *event,
                        gpointer user_data)
{
	if (current_event != event) {
		g_clear_pointer (&current_event, gdk_event_unref);
		current_event = gdk_event_ref (event);
	}

	return FALSE;
}

void
e_gtk_track_current_event (GtkWidget *window)
{
	GtkEventController *controller;

	g_return_if_fail (GTK_IS_WIDGET (window));

	if (g_object_get_data (G_OBJECT (window), "e-gtk-track-current-event"))
		return;

	controller = gtk_event_controller_legacy_new ();
	gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
	g_signal_connect (controller, "event", G_CALLBACK (track_current_event_cb), NULL);
	gtk_widget_add_controller (window, controller);

	g_object_set_data (G_OBJECT (window), "e-gtk-track-current-event", controller);
}

GdkEvent *
e_gtk_get_current_event (void)
{
	return current_event ? gdk_event_ref (current_event) : NULL;
}

guint32
e_gtk_get_current_event_time (void)
{
	return current_event ? gdk_event_get_time (current_event) : GDK_CURRENT_TIME;
}

gboolean
e_gtk_get_current_event_state (GdkModifierType *state)
{
	if (!current_event) {
		if (state)
			*state = 0;
		return FALSE;
	}

	if (state)
		*state = gdk_event_get_modifier_state (current_event);

	return TRUE;
}

GdkDevice *
e_gtk_get_current_event_device (void)
{
	return current_event ? gdk_event_get_device (current_event) : NULL;
}

gboolean
e_gdk_event_get_state (GdkEvent *event,
                       GdkModifierType *state)
{
	if (!event) {
		if (state)
			*state = 0;
		return FALSE;
	}

	if (state)
		*state = gdk_event_get_modifier_state (event);

	return TRUE;
}

gboolean
e_gdk_event_get_keyval (GdkEvent *event,
                        guint *keyval)
{
	gboolean is_key = event && (gdk_event_get_event_type (event) == GDK_KEY_PRESS ||
		gdk_event_get_event_type (event) == GDK_KEY_RELEASE);

	if (keyval)
		*keyval = is_key ? gdk_key_event_get_keyval (event) : 0;

	return is_key;
}

gboolean
e_gdk_event_get_button (GdkEvent *event,
                        guint *button)
{
	gboolean is_button = event && (gdk_event_get_event_type (event) == GDK_BUTTON_PRESS ||
		gdk_event_get_event_type (event) == GDK_BUTTON_RELEASE);

	if (button)
		*button = is_button ? gdk_button_event_get_button (event) : 0;

	return is_button;
}

gboolean
e_gdk_event_get_coords (GdkEvent *event,
                        gdouble *x,
                        gdouble *y)
{
	gdouble ex = 0, ey = 0;
	gboolean has = event && gdk_event_get_position (event, &ex, &ey);

	if (x)
		*x = ex;
	if (y)
		*y = ey;

	return has;
}

/* GTK 4 has no global coordinates; these are surface coordinates */
gboolean
e_gdk_event_get_root_coords (GdkEvent *event,
                             gdouble *x,
                             gdouble *y)
{
	return e_gdk_event_get_coords (event, x, y);
}

/* What gtk_show_uri_on_window() did outside a sandbox */
gboolean
e_gtk_show_uri_on_window (GtkWindow *parent,
                          const gchar *uri,
                          guint32 timestamp,
                          GError **error)
{
	GdkAppLaunchContext *context;
	gboolean success;

	g_return_val_if_fail (uri != NULL, FALSE);

	context = gdk_display_get_app_launch_context (
		parent ? gtk_widget_get_display (GTK_WIDGET (parent)) : gdk_display_get_default ());
	gdk_app_launch_context_set_timestamp (context, timestamp);

	success = g_app_info_launch_default_for_uri (uri, G_APP_LAUNCH_CONTEXT (context), error);

	g_object_unref (context);

	return success;
}

void
e_gtk_widget_set_accessible_name (GtkWidget *widget,
                                  const gchar *name)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
		GTK_ACCESSIBLE_PROPERTY_LABEL, name, -1);
}

void
e_gtk_widget_set_accessible_description (GtkWidget *widget,
                                         const gchar *description)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
		GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description, -1);
}
