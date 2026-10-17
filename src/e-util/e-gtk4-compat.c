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

#include <glib/gi18n-lib.h>
#include <gobject/gvaluecollector.h>

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
	BIN (E_GTK_IS_BIN, EGtkBin, e_gtk_bin_get_bin_child, e_gtk_bin_set_bin_child)
	/* the entry of a combo box with one, as with GTK 3 */
	BIN (GTK_IS_COMBO_BOX, GtkComboBox, gtk_combo_box_get_child, gtk_combo_box_set_child)

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

	if (E_GTK_IS_BIN (widget)) {
		E_GTK_BIN_GET_CLASS (widget)->remove (widget, child);
	} else if (GTK_IS_BOX (widget)) {
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
e_gtk_button_box_set_child_secondary (gpointer box,
				      GtkWidget *child,
				      gboolean is_secondary)
{
	g_return_if_fail (GTK_IS_BOX (box));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (is_secondary)
		gtk_box_reorder_child_after (box, child, NULL);
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

/* the image set by e_gtk_button_set_image(), or NULL */
GtkWidget *
e_gtk_button_get_image (GtkButton *button)
{
	GtkWidget *child;

	g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

	child = gtk_button_get_child (button);

	if (GTK_IS_BOX (child))
		child = gtk_widget_get_first_child (child);

	return GTK_IS_IMAGE (child) ? child : NULL;
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
	/* relative to the widget of a legacy event signal, as with GTK 3 */
	return e_gdk_event_get_widget_coords (event, e_gtk_event_get_legacy_widget (event), x, y);
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

static GHashTable *style_properties = NULL; /* "TypeName:prop-name" ~> GParamSpec */

void
e_gtk_widget_class_install_style_property (gpointer widget_class,
                                           GParamSpec *pspec)
{
	g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
	g_return_if_fail (G_IS_PARAM_SPEC (pspec));

	if (!style_properties)
		style_properties = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_param_spec_unref);

	g_hash_table_insert (style_properties,
		g_strconcat (G_OBJECT_CLASS_NAME (widget_class), ":", g_param_spec_get_name (pspec), NULL),
		g_param_spec_ref_sink (pspec));
}

static GParamSpec *
style_property_lookup (GType type,
                       const gchar *name)
{
	gchar *canonical;
	GParamSpec *pspec = NULL;

	if (!style_properties)
		return NULL;

	canonical = g_strdelimit (g_strdup (name), "_", '-');

	for (; type && !pspec; type = g_type_parent (type)) {
		gchar *key = g_strconcat (g_type_name (type), ":", canonical, NULL);

		pspec = g_hash_table_lookup (style_properties, key);
		g_free (key);
	}

	g_free (canonical);

	return pspec;
}

void
e_gtk_widget_style_get (gpointer widget,
                        const gchar *first_property_name,
                        ...)
{
	const gchar *name;
	va_list va;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	va_start (va, first_property_name);

	for (name = first_property_name; name; name = va_arg (va, const gchar *)) {
		GParamSpec *pspec = style_property_lookup (G_OBJECT_TYPE (widget), name);
		GValue value = G_VALUE_INIT;
		gchar *error = NULL;

		if (!pspec) {
			g_warning ("%s: %s has no style property '%s'", G_STRFUNC, G_OBJECT_TYPE_NAME (widget), name);
			break;
		}

		g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
		g_param_value_set_default (pspec, &value);

		G_VALUE_LCOPY (&value, va, 0, &error);
		g_value_unset (&value);

		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			break;
		}
	}

	va_end (va);
}

GtkWidget *
e_gtk_popover_new (GtkWidget *relative_to)
{
	GtkWidget *popover;

	popover = gtk_popover_new ();

	if (relative_to)
		gtk_widget_set_parent (popover, relative_to);

	return popover;
}

void
e_gtk_popover_set_relative_to (GtkPopover *popover,
                               GtkWidget *relative_to)
{
	GtkWidget *widget = GTK_WIDGET (popover);

	g_return_if_fail (GTK_IS_POPOVER (popover));

	if (gtk_widget_get_parent (widget) == relative_to)
		return;

	if (gtk_widget_get_parent (widget))
		gtk_widget_unparent (widget);

	if (relative_to)
		gtk_widget_set_parent (widget, relative_to);
}

void
e_gtk_widget_destroyed (GtkWidget *widget,
                        GtkWidget **widget_pointer)
{
	if (widget_pointer)
		*widget_pointer = NULL;
}

static GdkPixbuf *
pixbuf_from_paintable (GtkIconPaintable *paintable,
                       gint size)
{
	GdkPixbuf *pixbuf = NULL;
	GFile *file;

	if (!paintable)
		return NULL;

	file = gtk_icon_paintable_get_file (paintable);
	if (file) {
		GInputStream *stream;

		/* resource:// icons come as GResource files */
		stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
		if (stream) {
			pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream, size, size, TRUE, NULL, NULL);
			g_object_unref (stream);
		}
		g_object_unref (file);
	}

	if (!pixbuf) {
		/* render it, e.g. a symbolic or a builtin icon */
		GtkSnapshot *snapshot = gtk_snapshot_new ();
		GskRenderNode *node;

		gdk_paintable_snapshot (GDK_PAINTABLE (paintable), snapshot, size, size);
		node = gtk_snapshot_free_to_node (snapshot);
		if (node) {
			cairo_surface_t *surface;
			cairo_t *cr;

			surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
			cr = cairo_create (surface);
			gsk_render_node_draw (node, cr);
			cairo_destroy (cr);

			pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
			cairo_surface_destroy (surface);
			gsk_render_node_unref (node);
		}
	}

	return pixbuf;
}

/* gtk_icon_theme_load_icon() of GTK 3; flags are ignored */
GdkPixbuf *
e_gtk_icon_theme_load_icon (GtkIconTheme *icon_theme,
                            const gchar *icon_name,
                            gint size,
                            gint flags,
                            GError **error)
{
	GtkIconPaintable *paintable;
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (icon_name != NULL, NULL);

	if (!icon_theme)
		icon_theme = e_gtk_icon_theme_get_default ();

	if (!gtk_icon_theme_has_icon (icon_theme, icon_name)) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Icon '%s' not present in theme", icon_name);
		return NULL;
	}

	paintable = gtk_icon_theme_lookup_icon (icon_theme, icon_name, NULL, size, 1, GTK_TEXT_DIR_NONE, 0);
	pixbuf = pixbuf_from_paintable (paintable, size);
	g_clear_object (&paintable);

	if (!pixbuf)
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to load icon '%s'", icon_name);

	return pixbuf;
}

#define ICON_INFO_SIZE_KEY "e-gtk-icon-info-size"
#define ICON_INFO_PATH_KEY "e-gtk-icon-info-path"

static EGtkIconInfo *
icon_info_new (GtkIconPaintable *paintable,
	       gint size)
{
	if (paintable)
		g_object_set_data (G_OBJECT (paintable), ICON_INFO_SIZE_KEY, GINT_TO_POINTER (size));

	return paintable;
}

EGtkIconInfo *
e_gtk_icon_theme_lookup_icon (GtkIconTheme *icon_theme,
			      const gchar *icon_name,
			      gint size,
			      gint flags)
{
	g_return_val_if_fail (icon_name != NULL, NULL);

	if (!icon_theme)
		icon_theme = e_gtk_icon_theme_get_default ();

	if (!gtk_icon_theme_has_icon (icon_theme, icon_name))
		return NULL;

	return icon_info_new (gtk_icon_theme_lookup_icon (icon_theme, icon_name, NULL, size, 1, GTK_TEXT_DIR_NONE, flags), size);
}

EGtkIconInfo *
e_gtk_icon_theme_lookup_by_gicon (GtkIconTheme *icon_theme,
				  GIcon *icon,
				  gint size,
				  gint flags)
{
	g_return_val_if_fail (G_IS_ICON (icon), NULL);

	if (!icon_theme)
		icon_theme = e_gtk_icon_theme_get_default ();

	if (!gtk_icon_theme_has_gicon (icon_theme, icon) && !G_IS_FILE_ICON (icon) && !G_IS_LOADABLE_ICON (icon))
		return NULL;

	return icon_info_new (gtk_icon_theme_lookup_by_gicon (icon_theme, icon, size, 1, GTK_TEXT_DIR_NONE, flags), size);
}

const gchar *
e_gtk_icon_info_get_filename (EGtkIconInfo *icon_info)
{
	const gchar *path;

	g_return_val_if_fail (GTK_IS_ICON_PAINTABLE (icon_info), NULL);

	path = g_object_get_data (G_OBJECT (icon_info), ICON_INFO_PATH_KEY);
	if (!path) {
		GFile *file = gtk_icon_paintable_get_file (icon_info);

		if (file) {
			/* GTK 3 owned the string; keep it with the paintable */
			gchar *filename = g_file_get_path (file);

			if (filename)
				g_object_set_data_full (G_OBJECT (icon_info), ICON_INFO_PATH_KEY, filename, g_free);
			path = filename;
			g_object_unref (file);
		}
	}

	return path;
}

GdkPixbuf *
e_gtk_icon_info_load_icon (EGtkIconInfo *icon_info,
			   GError **error)
{
	GdkPixbuf *pixbuf;
	gint size;

	g_return_val_if_fail (GTK_IS_ICON_PAINTABLE (icon_info), NULL);

	size = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (icon_info), ICON_INFO_SIZE_KEY));
	if (size <= 0)
		size = gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (icon_info));
	if (size <= 0)
		size = 16;

	pixbuf = pixbuf_from_paintable (icon_info, size);
	if (!pixbuf)
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to load icon");

	return pixbuf;
}

GdkPixbuf *
e_gtk_icon_info_load_symbolic (EGtkIconInfo *icon_info,
			       const GdkRGBA *fg,
			       const GdkRGBA *success_color,
			       const GdkRGBA *warning_color,
			       const GdkRGBA *error_color,
			       gboolean *was_symbolic,
			       GError **error)
{
	/* the colors are not applied; symbolic icons render as the theme has them */
	if (was_symbolic)
		*was_symbolic = icon_info && gtk_icon_paintable_is_symbolic (icon_info);

	return e_gtk_icon_info_load_icon (icon_info, error);
}

gboolean
e_gtk_icon_info_is_symbolic (EGtkIconInfo *icon_info)
{
	g_return_val_if_fail (GTK_IS_ICON_PAINTABLE (icon_info), FALSE);

	return gtk_icon_paintable_is_symbolic (icon_info);
}

/* the pixbuf is size * scale pixels big, as with GTK 3 */
GdkPixbuf *
e_gtk_icon_theme_load_icon_for_scale (GtkIconTheme *icon_theme,
				      const gchar *icon_name,
				      gint size,
				      gint scale,
				      gint flags,
				      GError **error)
{
	return e_gtk_icon_theme_load_icon (icon_theme, icon_name, size * MAX (scale, 1), flags, error);
}

void
e_gtk_render_icon_surface (GtkStyleContext *context,
			   cairo_t *cr,
			   cairo_surface_t *surface,
			   gdouble x,
			   gdouble y)
{
	cairo_set_source_surface (cr, surface, x, y);
	cairo_paint (cr);
}

GdkPixbuf *
e_gtk_icon_theme_load_gicon (GtkIconTheme *icon_theme,
                             GIcon *icon,
                             gint size)
{
	GtkIconPaintable *paintable;
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (G_IS_ICON (icon), NULL);

	if (!icon_theme)
		icon_theme = e_gtk_icon_theme_get_default ();

	paintable = gtk_icon_theme_lookup_by_gicon (icon_theme, icon, size, 1, GTK_TEXT_DIR_NONE, 0);
	pixbuf = pixbuf_from_paintable (paintable, size);
	g_clear_object (&paintable);

	return pixbuf;
}

/* gdk_cairo_surface_create_from_pixbuf() of GTK 3 */
cairo_surface_t *
e_gdk_cairo_surface_create_from_pixbuf (const GdkPixbuf *pixbuf,
                                        gint scale,
                                        gpointer for_window)
{
	cairo_surface_t *surface;
	cairo_t *cr;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	surface = cairo_image_surface_create (
		gdk_pixbuf_get_has_alpha (pixbuf) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
		gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf));

	cr = cairo_create (surface);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	G_GNUC_END_IGNORE_DEPRECATIONS
	cairo_paint (cr);
	cairo_destroy (cr);

	if (scale > 1)
		cairo_surface_set_device_scale (surface, scale, scale);

	return surface;
}

gboolean
e_gtk_icon_size_lookup (gint size,
                        gint *width,
                        gint *height)
{
	gint pixels = size == GTK_ICON_SIZE_LARGE ? 32 : 16;

	if (width)
		*width = pixels;
	if (height)
		*height = pixels;

	return TRUE;
}

void
e_gtk_header_bar_set_title (GtkHeaderBar *header_bar,
                            const gchar *title)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_HEADER_BAR (header_bar));

	label = gtk_header_bar_get_title_widget (header_bar);
	if (!GTK_IS_LABEL (label)) {
		label = gtk_label_new (NULL);
		gtk_widget_add_css_class (label, "title");
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_header_bar_set_title_widget (header_bar, label);
	}

	gtk_label_set_text (GTK_LABEL (label), title);
}

void
e_gtk_scrollable_override_properties (GObjectClass *object_class,
				      guint first_property_id)
{
	g_object_class_override_property (object_class, first_property_id, "hadjustment");
	g_object_class_override_property (object_class, first_property_id + 1, "vadjustment");
	g_object_class_override_property (object_class, first_property_id + 2, "hscroll-policy");
	g_object_class_override_property (object_class, first_property_id + 3, "vscroll-policy");
}

gboolean
e_gtk_scrollable_forward_property (GtkWidget *inner,
				   guint first_property_id,
				   guint property_id,
				   GValue *value,
				   gboolean set,
				   GParamSpec *pspec)
{
	if (property_id < first_property_id || property_id > first_property_id + 3)
		return FALSE;

	if (set)
		g_object_set_property (G_OBJECT (inner), pspec->name, value);
	else
		g_object_get_property (G_OBJECT (inner), pspec->name, value);

	return TRUE;
}

gboolean
e_gtk_toggle_button_get_active (gpointer button)
{
	if (GTK_IS_CHECK_BUTTON (button))
		return gtk_check_button_get_active (button);

	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (button), FALSE);

	return gtk_toggle_button_get_active (button);
}

void
e_gtk_toggle_button_set_active (gpointer button,
				gboolean active)
{
	if (GTK_IS_CHECK_BUTTON (button)) {
		gtk_check_button_set_active (button, active);
		return;
	}

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

	gtk_toggle_button_set_active (button, active);
}

gboolean
e_gtk_toggle_button_get_inconsistent (gpointer button)
{
	if (GTK_IS_CHECK_BUTTON (button))
		return gtk_check_button_get_inconsistent (button);

	g_return_val_if_fail (GTK_IS_WIDGET (button), FALSE);

	/* GtkToggleButton has no inconsistent state on GTK 4 */
	return GPOINTER_TO_INT (g_object_get_data (button, "e-inconsistent"));
}

void
e_gtk_toggle_button_set_inconsistent (gpointer button,
				      gboolean inconsistent)
{
	if (GTK_IS_CHECK_BUTTON (button)) {
		gtk_check_button_set_inconsistent (button, inconsistent);
		return;
	}

	g_return_if_fail (GTK_IS_WIDGET (button));

	g_object_set_data (button, "e-inconsistent", GINT_TO_POINTER (inconsistent ? 1 : 0));

	if (inconsistent)
		gtk_widget_set_state_flags (button, GTK_STATE_FLAG_INCONSISTENT, FALSE);
	else
		gtk_widget_unset_state_flags (button, GTK_STATE_FLAG_INCONSISTENT);
}

#define RADIO_GROUP_KEY "e-gtk-radio-group"

GSList *
e_gtk_radio_button_get_group (gpointer button)
{
	GSList *group;

	g_return_val_if_fail (GTK_IS_CHECK_BUTTON (button), NULL);

	group = g_object_get_data (button, RADIO_GROUP_KEY);
	if (!group) {
		group = g_slist_prepend (NULL, button);
		g_object_set_data_full (button, RADIO_GROUP_KEY, group, (GDestroyNotify) g_slist_free);
	}

	return group;
}

void
e_gtk_radio_button_join_group (gpointer button,
			       gpointer group_source)
{
	g_return_if_fail (GTK_IS_CHECK_BUTTON (button));

	gtk_check_button_set_group (button, group_source);
}

static GtkWidget *
radio_button_new (gpointer member,
		  const gchar *label,
		  gboolean mnemonic)
{
	GtkWidget *button;

	button = mnemonic ? gtk_check_button_new_with_mnemonic (label) : gtk_check_button_new_with_label (label);

	if (member)
		gtk_check_button_set_group (GTK_CHECK_BUTTON (button), member);
	else /* the first button of a GTK 3 group starts active */
		gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);

	return button;
}

GtkWidget *
e_gtk_radio_button_new_with_mnemonic (GSList *group,
				      const gchar *label)
{
	return radio_button_new (group ? group->data : NULL, label, TRUE);
}

GtkWidget *
e_gtk_radio_button_new_with_label (GSList *group,
				   const gchar *label)
{
	return radio_button_new (group ? group->data : NULL, label, FALSE);
}

GtkWidget *
e_gtk_radio_button_new_with_mnemonic_from_widget (gpointer member,
						  const gchar *label)
{
	return radio_button_new (member, label, TRUE);
}

GtkWidget *
e_gtk_radio_button_new_with_label_from_widget (gpointer member,
					       const gchar *label)
{
	return radio_button_new (member, label, FALSE);
}

EGtkAccelGroup *
e_gtk_accel_group_new (void)
{
	return GTK_SHORTCUT_CONTROLLER (gtk_shortcut_controller_new ());
}

typedef struct _AccelData {
	GWeakRef widget;
	gchar *signal_name;
} AccelData;

static gboolean
accel_emit_signal_cb (GtkWidget *widget,
		      GVariant *args,
		      gpointer user_data)
{
	AccelData *data = user_data;
	GtkWidget *target = g_weak_ref_get (&data->widget);

	if (!target)
		return FALSE;

	/* "activate", "clicked" and the like take no arguments */
	g_signal_emit_by_name (target, data->signal_name);
	g_object_unref (target);

	return TRUE;
}

static void
accel_data_free (gpointer ptr)
{
	AccelData *data = ptr;

	g_weak_ref_clear (&data->widget);
	g_free (data->signal_name);
	g_free (data);
}

void
e_gtk_widget_add_accelerator (GtkWidget *widget,
			      const gchar *accel_signal,
			      EGtkAccelGroup *accel_group,
			      guint accel_key,
			      GdkModifierType accel_mods,
			      guint accel_flags)
{
	AccelData *data;
	GtkShortcut *shortcut;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (accel_group));

	data = g_new0 (AccelData, 1);
	g_weak_ref_init (&data->widget, widget);
	data->signal_name = g_strdup (accel_signal);

	shortcut = gtk_shortcut_new (
		gtk_keyval_trigger_new (accel_key, accel_mods),
		gtk_callback_action_new (accel_emit_signal_cb, data, accel_data_free));
	gtk_shortcut_controller_add_shortcut (accel_group, shortcut);
}

void
e_gtk_window_add_accel_group (GtkWindow *window,
			      EGtkAccelGroup *accel_group)
{
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (accel_group));

	/* the window takes a reference, as with GTK 3 */
	gtk_widget_add_controller (GTK_WIDGET (window), g_object_ref (GTK_EVENT_CONTROLLER (accel_group)));
}

void
e_gtk_window_remove_accel_group (GtkWindow *window,
				 EGtkAccelGroup *accel_group)
{
	g_return_if_fail (GTK_IS_WINDOW (window));
	g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (accel_group));

	gtk_widget_remove_controller (GTK_WIDGET (window), GTK_EVENT_CONTROLLER (accel_group));
}

void
e_gtk_calendar_get_date (GtkCalendar *calendar,
			 guint *year,
			 guint *month,
			 guint *day)
{
	GDateTime *dt;

	g_return_if_fail (GTK_IS_CALENDAR (calendar));

	dt = gtk_calendar_get_date (calendar);

	if (year)
		*year = g_date_time_get_year (dt);
	if (month)
		*month = g_date_time_get_month (dt) - 1;
	if (day)
		*day = g_date_time_get_day_of_month (dt);

	g_date_time_unref (dt);
}

static void
calendar_select (GtkCalendar *calendar,
		 guint year,
		 guint month,
		 guint day)
{
	GDateTime *dt;
	guint days;

	days = g_date_get_days_in_month (month + 1, year);
	dt = g_date_time_new_local (year, month + 1, CLAMP (day, 1, days), 0, 0, 0);
	if (dt) {
		gtk_calendar_select_day (calendar, dt);
		g_date_time_unref (dt);
	}
}

void
e_gtk_calendar_select_month (GtkCalendar *calendar,
			     guint month,
			     guint year)
{
	guint day = 1;

	g_return_if_fail (GTK_IS_CALENDAR (calendar));

	e_gtk_calendar_get_date (calendar, NULL, NULL, &day);
	calendar_select (calendar, year, month, day);
}

void
e_gtk_calendar_select_day (GtkCalendar *calendar,
			   guint day)
{
	guint year, month;

	g_return_if_fail (GTK_IS_CALENDAR (calendar));

	/* GTK 4 always has a day selected */
	if (!day)
		return;

	e_gtk_calendar_get_date (calendar, &year, &month, NULL);
	calendar_select (calendar, year, month, day);
}

GtkWidget *
e_gtk_button_new_from_stock (const gchar *stock_id)
{
	static const struct {
		const gchar *stock_id;
		const gchar *label;
	} stock_labels[] = {
		{ "gtk-add", N_("_Add") },
		{ "gtk-apply", N_("_Apply") },
		{ "gtk-cancel", N_("_Cancel") },
		{ "gtk-clear", N_("_Clear") },
		{ "gtk-close", N_("_Close") },
		{ "gtk-copy", N_("_Copy") },
		{ "gtk-cut", N_("Cu_t") },
		{ "gtk-delete", N_("_Delete") },
		{ "gtk-edit", N_("_Edit") },
		{ "gtk-find", N_("_Find") },
		{ "gtk-go-back", N_("_Back") },
		{ "gtk-go-forward", N_("_Forward") },
		{ "gtk-help", N_("_Help") },
		{ "gtk-no", N_("_No") },
		{ "gtk-ok", N_("_OK") },
		{ "gtk-open", N_("_Open") },
		{ "gtk-paste", N_("_Paste") },
		{ "gtk-print", N_("_Print") },
		{ "gtk-properties", N_("_Properties") },
		{ "gtk-quit", N_("_Quit") },
		{ "gtk-refresh", N_("_Refresh") },
		{ "gtk-remove", N_("_Remove") },
		{ "gtk-revert-to-saved", N_("_Revert") },
		{ "gtk-save", N_("_Save") },
		{ "gtk-stop", N_("_Stop") },
		{ "gtk-yes", N_("_Yes") }
	};
	guint ii;

	g_return_val_if_fail (stock_id != NULL, NULL);

	for (ii = 0; ii < G_N_ELEMENTS (stock_labels); ii++) {
		if (g_str_equal (stock_id, stock_labels[ii].stock_id))
			return gtk_button_new_with_mnemonic (_(stock_labels[ii].label));
	}

	return gtk_button_new_from_icon_name (stock_id);
}

/* finds the object holding a GTK 3 child property; NULL to ignore it */
static GObject *
container_child_resolve (GtkWidget *container,
			 GtkWidget *child,
			 const gchar *name,
			 const gchar **out_name)
{
	*out_name = name;

	if (GTK_IS_NOTEBOOK (container))
		return G_OBJECT (gtk_notebook_get_page (GTK_NOTEBOOK (container), child));

	if (GTK_IS_STACK (container))
		return G_OBJECT (gtk_stack_get_page (GTK_STACK (container), child));

	if (GTK_IS_PANED (container)) {
		gboolean is_start = gtk_paned_get_start_child (GTK_PANED (container)) == child;

		if (g_str_equal (name, "resize"))
			*out_name = is_start ? "resize-start-child" : "resize-end-child";
		else if (g_str_equal (name, "shrink"))
			*out_name = is_start ? "shrink-start-child" : "shrink-end-child";
		else
			return NULL;

		return G_OBJECT (container);
	}

	if (GTK_IS_GRID (container)) {
		GtkLayoutManager *layout = gtk_widget_get_layout_manager (container);

		if (g_str_equal (name, "left-attach"))
			*out_name = "column";
		else if (g_str_equal (name, "top-attach"))
			*out_name = "row";
		else if (g_str_equal (name, "width"))
			*out_name = "column-span";
		else if (g_str_equal (name, "height"))
			*out_name = "row-span";
		else
			return NULL;

		return G_OBJECT (gtk_layout_manager_get_layout_child (layout, child));
	}

	return NULL;
}

static GParamSpec *
container_child_find_pspec (GtkWidget *container,
			    GtkWidget *child,
			    const gchar *name,
			    GObject **out_object)
{
	const gchar *real_name;
	GObject *object;

	object = container_child_resolve (container, child, name, &real_name);
	*out_object = object;

	if (!object) {
		g_debug ("%s: child property '%s' of %s is gone with GTK 4", G_STRFUNC, name, G_OBJECT_TYPE_NAME (container));
		return NULL;
	}

	return g_object_class_find_property (G_OBJECT_GET_CLASS (object), real_name);
}

void
e_gtk_container_child_set_property (gpointer container,
				    GtkWidget *child,
				    const gchar *property_name,
				    const GValue *value)
{
	GObject *object;
	GParamSpec *pspec;

	g_return_if_fail (GTK_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (child));

	pspec = container_child_find_pspec (container, child, property_name, &object);
	if (pspec)
		g_object_set_property (object, pspec->name, value);
}

void
e_gtk_container_child_get_property (gpointer container,
				    GtkWidget *child,
				    const gchar *property_name,
				    GValue *value)
{
	GObject *object;
	GParamSpec *pspec;

	g_return_if_fail (GTK_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (child));

	pspec = container_child_find_pspec (container, child, property_name, &object);
	if (pspec)
		g_object_get_property (object, pspec->name, value);
}

/* the type of a property GTK 4 dropped, to skip its vararg */
static GType
container_child_gone_type (const gchar *name)
{
	if (g_str_equal (name, "padding") || g_str_equal (name, "position") ||
	    g_str_equal (name, "pack-type"))
		return G_TYPE_INT;

	return G_TYPE_BOOLEAN;
}

void
e_gtk_container_child_set (gpointer container,
			   GtkWidget *child,
			   const gchar *first_property_name,
			   ...)
{
	const gchar *name;
	va_list va;

	g_return_if_fail (GTK_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (child));

	va_start (va, first_property_name);

	for (name = first_property_name; name; name = va_arg (va, const gchar *)) {
		GObject *object;
		GParamSpec *pspec;
		GValue value = G_VALUE_INIT;
		gchar *error = NULL;

		pspec = container_child_find_pspec (container, child, name, &object);

		G_VALUE_COLLECT_INIT (&value, pspec ? pspec->value_type : container_child_gone_type (name), va, 0, &error);
		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			break;
		}

		if (pspec)
			g_object_set_property (object, pspec->name, &value);

		g_value_unset (&value);
	}

	va_end (va);
}

void
e_gtk_container_child_get (gpointer container,
			   GtkWidget *child,
			   const gchar *first_property_name,
			   ...)
{
	const gchar *name;
	va_list va;

	g_return_if_fail (GTK_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (child));

	va_start (va, first_property_name);

	for (name = first_property_name; name; name = va_arg (va, const gchar *)) {
		GObject *object;
		GParamSpec *pspec;
		GValue value = G_VALUE_INIT;
		gchar *error = NULL;

		pspec = container_child_find_pspec (container, child, name, &object);

		g_value_init (&value, pspec ? pspec->value_type : container_child_gone_type (name));
		if (pspec)
			g_object_get_property (object, pspec->name, &value);

		G_VALUE_LCOPY (&value, va, 0, &error);
		g_value_unset (&value);

		if (error) {
			g_warning ("%s: %s", G_STRFUNC, error);
			g_free (error);
			break;
		}
	}

	va_end (va);
}

void
e_gtk_widget_set_labelled_by (GtkWidget *widget,
			      GtkWidget *label)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_WIDGET (label));

	gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
		GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
		-1);
}

gint
e_gtk_entry_get_index_at_position (GtkEntry *entry,
				   gdouble x,
				   gdouble y)
{
	GtkWidget *text;
	PangoLayout *layout;
	graphene_point_t point;
	const gchar *str;
	gint index = -1, trailing = 0, height = 0;

	g_return_val_if_fail (GTK_IS_ENTRY (entry), -1);

	text = GTK_WIDGET (gtk_editable_get_delegate (GTK_EDITABLE (entry)));
	if (!text || !gtk_widget_compute_point (GTK_WIDGET (entry), text, &GRAPHENE_POINT_INIT (x, y), &point))
		return -1;

	str = gtk_editable_get_text (GTK_EDITABLE (entry));
	layout = gtk_widget_create_pango_layout (text, str);
	pango_layout_get_pixel_size (layout, NULL, &height);

	/* the text is centered vertically in the GtkText */
	if (point.x < 0 ||
	    !pango_layout_xy_to_index (layout, point.x * PANGO_SCALE, (height / 2) * PANGO_SCALE, &index, &trailing))
		index = -1;
	else
		index = g_utf8_pointer_to_offset (str, str + index);

	g_object_unref (layout);

	return index;
}

void
e_gtk_widget_override_font (GtkWidget *widget,
			    const PangoFontDescription *font_desc)
{
	GtkCssProvider *provider;
	GString *css;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	provider = g_object_get_data (G_OBJECT (widget), "e-gtk-font-provider");
	if (!provider) {
		provider = gtk_css_provider_new ();
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
			GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		G_GNUC_END_IGNORE_DEPRECATIONS
		g_object_set_data_full (G_OBJECT (widget), "e-gtk-font-provider", provider, g_object_unref);
	}

	css = g_string_new ("* {");

	if (font_desc) {
		PangoFontMask mask = pango_font_description_get_set_fields (font_desc);

		if (mask & PANGO_FONT_MASK_FAMILY)
			g_string_append_printf (css, " font-family: \"%s\";", pango_font_description_get_family (font_desc));
		if (mask & PANGO_FONT_MASK_SIZE)
			g_string_append_printf (css, " font-size: %g%s;",
				(gdouble) pango_font_description_get_size (font_desc) / PANGO_SCALE,
				pango_font_description_get_size_is_absolute (font_desc) ? "px" : "pt");
		if (mask & PANGO_FONT_MASK_WEIGHT)
			g_string_append_printf (css, " font-weight: %d;", (gint) pango_font_description_get_weight (font_desc));
		if (mask & PANGO_FONT_MASK_STYLE)
			g_string_append_printf (css, " font-style: %s;",
				pango_font_description_get_style (font_desc) == PANGO_STYLE_ITALIC ? "italic" :
				pango_font_description_get_style (font_desc) == PANGO_STYLE_OBLIQUE ? "oblique" : "normal");
	}

	g_string_append (css, " }");

	gtk_css_provider_load_from_string (provider, css->str);
	g_string_free (css, TRUE);
}

GdkModifierType
e_gtk_widget_get_modifier_state (GtkWidget *widget)
{
	GdkSeat *seat;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

	seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
	if (!seat || !gdk_seat_get_keyboard (seat))
		return 0;

	return gdk_device_get_modifier_state (gdk_seat_get_keyboard (seat));
}

gboolean
e_gtk_widget_get_pointer_position (GtkWidget *widget,
				   gdouble *x,
				   gdouble *y)
{
	GtkNative *native;
	GdkSurface *surface;
	GdkSeat *seat;
	GdkDevice *pointer;
	graphene_point_t point;
	gdouble px = 0, py = 0, nx = 0, ny = 0;
	gboolean res = FALSE;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	native = gtk_widget_get_native (widget);
	surface = native ? gtk_native_get_surface (native) : NULL;
	seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
	pointer = seat ? gdk_seat_get_pointer (seat) : NULL;

	if (surface && pointer && gdk_surface_get_device_position (surface, pointer, &px, &py, NULL)) {
		gtk_native_get_surface_transform (native, &nx, &ny);
		res = gtk_widget_compute_point (GTK_WIDGET (native), widget,
			&GRAPHENE_POINT_INIT (px - nx, py - ny), &point);
		if (res) {
			px = point.x;
			py = point.y;
		}
	}

	if (x)
		*x = px;
	if (y)
		*y = py;

	return res;
}

typedef struct _EGtkBinPrivate {
	GtkWidget *child;
} EGtkBinPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EGtkBin, e_gtk_bin, GTK_TYPE_WIDGET)

static void
e_gtk_bin_real_remove (GtkWidget *container,
		       GtkWidget *child)
{
	EGtkBinPrivate *priv = e_gtk_bin_get_instance_private (E_GTK_BIN (container));

	if (child == priv->child) {
		e_gtk_bin_set_bin_child (E_GTK_BIN (container), NULL);
	} else if (gtk_widget_get_parent (child) == container) {
		gtk_widget_unparent (child);
	}
}

static void
e_gtk_bin_measure (GtkWidget *widget,
		   GtkOrientation orientation,
		   gint for_size,
		   gint *minimum,
		   gint *natural,
		   gint *minimum_baseline,
		   gint *natural_baseline)
{
	EGtkBinPrivate *priv = e_gtk_bin_get_instance_private (E_GTK_BIN (widget));

	if (priv->child && gtk_widget_get_visible (priv->child))
		gtk_widget_measure (priv->child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
e_gtk_bin_size_allocate (GtkWidget *widget,
			 gint width,
			 gint height,
			 gint baseline)
{
	EGtkBinPrivate *priv = e_gtk_bin_get_instance_private (E_GTK_BIN (widget));

	if (priv->child && gtk_widget_get_visible (priv->child))
		gtk_widget_allocate (priv->child, width, height, baseline, NULL);
}

static void
e_gtk_bin_dispose (GObject *object)
{
	GtkWidget *child;

	/* the main child and anything else a subclass did not drop */
	e_gtk_bin_set_bin_child (E_GTK_BIN (object), NULL);

	while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))) != NULL)
		gtk_widget_unparent (child);

	G_OBJECT_CLASS (e_gtk_bin_parent_class)->dispose (object);
}

static void
e_gtk_bin_class_init (EGtkBinClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = e_gtk_bin_dispose;

	widget_class->measure = e_gtk_bin_measure;
	widget_class->size_allocate = e_gtk_bin_size_allocate;

	klass->remove = e_gtk_bin_real_remove;
}

static void
e_gtk_bin_init (EGtkBin *bin)
{
}

GtkWidget *
e_gtk_bin_get_bin_child (EGtkBin *bin)
{
	EGtkBinPrivate *priv;

	g_return_val_if_fail (E_GTK_IS_BIN (bin), NULL);

	priv = e_gtk_bin_get_instance_private (bin);

	return priv->child;
}

void
e_gtk_bin_set_bin_child (EGtkBin *bin,
			 GtkWidget *child)
{
	EGtkBinPrivate *priv;

	g_return_if_fail (E_GTK_IS_BIN (bin));
	g_return_if_fail (child == NULL || GTK_IS_WIDGET (child));

	priv = e_gtk_bin_get_instance_private (bin);

	if (priv->child == child)
		return;

	if (priv->child)
		gtk_widget_unparent (priv->child);

	priv->child = child;

	if (child)
		gtk_widget_set_parent (child, GTK_WIDGET (bin));
}

GType
e_gtk_toolbar_style_get_type (void)
{
	static gsize type = 0;

	if (g_once_init_enter (&type)) {
		/* the GTK 3 names and nicks */
		static const GEnumValue values[] = {
			{ GTK_TOOLBAR_ICONS, "GTK_TOOLBAR_ICONS", "icons" },
			{ GTK_TOOLBAR_TEXT, "GTK_TOOLBAR_TEXT", "text" },
			{ GTK_TOOLBAR_BOTH, "GTK_TOOLBAR_BOTH", "both" },
			{ GTK_TOOLBAR_BOTH_HORIZ, "GTK_TOOLBAR_BOTH_HORIZ", "both-horiz" },
			{ 0, NULL, NULL }
		};

		g_once_init_leave (&type, g_enum_register_static (g_intern_static_string ("GtkToolbarStyle"), values));
	}

	return type;
}

static GSList *main_loops = NULL;

void
e_gtk_main (void)
{
	GMainLoop *loop = g_main_loop_new (NULL, FALSE);

	main_loops = g_slist_prepend (main_loops, loop);
	g_main_loop_run (loop);
	main_loops = g_slist_remove (main_loops, loop);

	g_main_loop_unref (loop);
}

void
e_gtk_main_quit (void)
{
	g_return_if_fail (main_loops != NULL);

	g_main_loop_quit (main_loops->data);
}

typedef struct _EGtkLayoutPrivate {
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	GtkScrollablePolicy hscroll_policy;
	GtkScrollablePolicy vscroll_policy;
	guint width;
	guint height;
} EGtkLayoutPrivate;

enum {
	LAYOUT_PROP_0,
	LAYOUT_PROP_HADJUSTMENT,
	LAYOUT_PROP_VADJUSTMENT,
	LAYOUT_PROP_HSCROLL_POLICY,
	LAYOUT_PROP_VSCROLL_POLICY,
	LAYOUT_PROP_WIDTH,
	LAYOUT_PROP_HEIGHT
};

#define LAYOUT_X_KEY "e-gtk-layout-x"
#define LAYOUT_Y_KEY "e-gtk-layout-y"

G_DEFINE_TYPE_WITH_CODE (EGtkLayout, e_gtk_layout, GTK_TYPE_WIDGET,
	G_ADD_PRIVATE (EGtkLayout)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
layout_adjustment_changed_cb (GtkAdjustment *adjustment,
			      EGtkLayout *layout)
{
	gtk_widget_queue_allocate (GTK_WIDGET (layout));
}

static void
layout_set_adjustment (EGtkLayout *layout,
		       GtkAdjustment **padjustment,
		       GtkAdjustment *adjustment)
{
	if (!adjustment)
		adjustment = gtk_adjustment_new (0, 0, 0, 0, 0, 0);

	if (*padjustment == adjustment)
		return;

	if (*padjustment) {
		g_signal_handlers_disconnect_by_func (*padjustment, layout_adjustment_changed_cb, layout);
		g_object_unref (*padjustment);
	}

	*padjustment = g_object_ref_sink (adjustment);
	g_signal_connect (adjustment, "value-changed", G_CALLBACK (layout_adjustment_changed_cb), layout);

	gtk_widget_queue_allocate (GTK_WIDGET (layout));
}

static void
layout_configure_adjustment (GtkAdjustment *adjustment,
			     gdouble size,
			     gdouble page)
{
	gdouble upper = MAX (size, page);
	gdouble value = gtk_adjustment_get_value (adjustment);

	gtk_adjustment_configure (adjustment,
		CLAMP (value, 0, MAX (0, upper - page)),
		0, upper, page * 0.1, page * 0.9, page);
}

static void
e_gtk_layout_set_property (GObject *object,
			   guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	EGtkLayout *layout = E_GTK_LAYOUT (object);
	EGtkLayoutPrivate *priv = e_gtk_layout_get_instance_private (layout);

	switch (property_id) {
		case LAYOUT_PROP_HADJUSTMENT:
			layout_set_adjustment (layout, &priv->hadjustment, g_value_get_object (value));
			return;
		case LAYOUT_PROP_VADJUSTMENT:
			layout_set_adjustment (layout, &priv->vadjustment, g_value_get_object (value));
			return;
		case LAYOUT_PROP_HSCROLL_POLICY:
			priv->hscroll_policy = g_value_get_enum (value);
			gtk_widget_queue_resize (GTK_WIDGET (layout));
			return;
		case LAYOUT_PROP_VSCROLL_POLICY:
			priv->vscroll_policy = g_value_get_enum (value);
			gtk_widget_queue_resize (GTK_WIDGET (layout));
			return;
		case LAYOUT_PROP_WIDTH:
			e_gtk_layout_set_size (layout, g_value_get_uint (value), priv->height);
			return;
		case LAYOUT_PROP_HEIGHT:
			e_gtk_layout_set_size (layout, priv->width, g_value_get_uint (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_gtk_layout_get_property (GObject *object,
			   guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	EGtkLayoutPrivate *priv = e_gtk_layout_get_instance_private (E_GTK_LAYOUT (object));

	switch (property_id) {
		case LAYOUT_PROP_HADJUSTMENT:
			g_value_set_object (value, priv->hadjustment);
			return;
		case LAYOUT_PROP_VADJUSTMENT:
			g_value_set_object (value, priv->vadjustment);
			return;
		case LAYOUT_PROP_HSCROLL_POLICY:
			g_value_set_enum (value, priv->hscroll_policy);
			return;
		case LAYOUT_PROP_VSCROLL_POLICY:
			g_value_set_enum (value, priv->vscroll_policy);
			return;
		case LAYOUT_PROP_WIDTH:
			g_value_set_uint (value, priv->width);
			return;
		case LAYOUT_PROP_HEIGHT:
			g_value_set_uint (value, priv->height);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_gtk_layout_dispose (GObject *object)
{
	EGtkLayout *layout = E_GTK_LAYOUT (object);
	EGtkLayoutPrivate *priv = e_gtk_layout_get_instance_private (layout);
	GtkWidget *child;

	while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))) != NULL)
		gtk_widget_unparent (child);

	if (priv->hadjustment) {
		g_signal_handlers_disconnect_by_func (priv->hadjustment, layout_adjustment_changed_cb, layout);
		g_clear_object (&priv->hadjustment);
	}

	if (priv->vadjustment) {
		g_signal_handlers_disconnect_by_func (priv->vadjustment, layout_adjustment_changed_cb, layout);
		g_clear_object (&priv->vadjustment);
	}

	G_OBJECT_CLASS (e_gtk_layout_parent_class)->dispose (object);
}

static void
e_gtk_layout_measure (GtkWidget *widget,
		      GtkOrientation orientation,
		      gint for_size,
		      gint *minimum,
		      gint *natural,
		      gint *minimum_baseline,
		      gint *natural_baseline)
{
	/* like GtkLayout, which asked for no space */
	*minimum = *natural = 0;
}

static void
e_gtk_layout_size_allocate (GtkWidget *widget,
			    gint width,
			    gint height,
			    gint baseline)
{
	EGtkLayout *layout = E_GTK_LAYOUT (widget);
	EGtkLayoutPrivate *priv = e_gtk_layout_get_instance_private (layout);
	GtkWidget *child;
	gdouble dx, dy;

	layout_configure_adjustment (priv->hadjustment, priv->width, width);
	layout_configure_adjustment (priv->vadjustment, priv->height, height);

	dx = gtk_adjustment_get_value (priv->hadjustment);
	dy = gtk_adjustment_get_value (priv->vadjustment);

	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		GtkRequisition req;
		GtkAllocation alloc;

		if (!gtk_widget_should_layout (child))
			continue;

		gtk_widget_get_preferred_size (child, NULL, &req);

		alloc.x = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), LAYOUT_X_KEY)) - dx;
		alloc.y = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), LAYOUT_Y_KEY)) - dy;
		alloc.width = req.width;
		alloc.height = req.height;

		gtk_widget_size_allocate (child, &alloc, -1);
	}
}

static void
e_gtk_layout_class_init (EGtkLayoutClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->set_property = e_gtk_layout_set_property;
	object_class->get_property = e_gtk_layout_get_property;
	object_class->dispose = e_gtk_layout_dispose;

	widget_class->measure = e_gtk_layout_measure;
	widget_class->size_allocate = e_gtk_layout_size_allocate;

	g_object_class_override_property (object_class, LAYOUT_PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (object_class, LAYOUT_PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (object_class, LAYOUT_PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (object_class, LAYOUT_PROP_VSCROLL_POLICY, "vscroll-policy");

	g_object_class_install_property (object_class, LAYOUT_PROP_WIDTH,
		g_param_spec_uint ("width", NULL, NULL, 0, G_MAXINT, 100,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class, LAYOUT_PROP_HEIGHT,
		g_param_spec_uint ("height", NULL, NULL, 0, G_MAXINT, 100,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
e_gtk_layout_init (EGtkLayout *layout)
{
	EGtkLayoutPrivate *priv = e_gtk_layout_get_instance_private (layout);

	priv->width = 100;
	priv->height = 100;

	layout_set_adjustment (layout, &priv->hadjustment, NULL);
	layout_set_adjustment (layout, &priv->vadjustment, NULL);

	gtk_widget_set_overflow (GTK_WIDGET (layout), GTK_OVERFLOW_HIDDEN);
}

GtkWidget *
e_gtk_layout_new (GtkAdjustment *hadjustment,
		  GtkAdjustment *vadjustment)
{
	return g_object_new (E_GTK_TYPE_LAYOUT,
		"hadjustment", hadjustment,
		"vadjustment", vadjustment,
		NULL);
}

void
e_gtk_layout_put (EGtkLayout *layout,
		  GtkWidget *child,
		  gint x,
		  gint y)
{
	g_return_if_fail (E_GTK_IS_LAYOUT (layout));
	g_return_if_fail (GTK_IS_WIDGET (child));

	g_object_set_data (G_OBJECT (child), LAYOUT_X_KEY, GINT_TO_POINTER (x));
	g_object_set_data (G_OBJECT (child), LAYOUT_Y_KEY, GINT_TO_POINTER (y));
	gtk_widget_set_parent (child, GTK_WIDGET (layout));
}

void
e_gtk_layout_move (EGtkLayout *layout,
		   GtkWidget *child,
		   gint x,
		   gint y)
{
	g_return_if_fail (E_GTK_IS_LAYOUT (layout));
	g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (layout));

	g_object_set_data (G_OBJECT (child), LAYOUT_X_KEY, GINT_TO_POINTER (x));
	g_object_set_data (G_OBJECT (child), LAYOUT_Y_KEY, GINT_TO_POINTER (y));
	gtk_widget_queue_allocate (GTK_WIDGET (layout));
}

void
e_gtk_layout_set_size (EGtkLayout *layout,
		       guint width,
		       guint height)
{
	EGtkLayoutPrivate *priv;

	g_return_if_fail (E_GTK_IS_LAYOUT (layout));

	priv = e_gtk_layout_get_instance_private (layout);

	if (priv->width == width && priv->height == height)
		return;

	priv->width = width;
	priv->height = height;

	g_object_notify (G_OBJECT (layout), "width");
	g_object_notify (G_OBJECT (layout), "height");

	gtk_widget_queue_allocate (GTK_WIDGET (layout));
}

void
e_gtk_layout_get_size (EGtkLayout *layout,
		       guint *width,
		       guint *height)
{
	EGtkLayoutPrivate *priv;

	g_return_if_fail (E_GTK_IS_LAYOUT (layout));

	priv = e_gtk_layout_get_instance_private (layout);

	if (width)
		*width = priv->width;
	if (height)
		*height = priv->height;
}

gboolean
e_gtk_container_get_focus_chain (gpointer container,
				 GList **focusable_widgets)
{
	if (focusable_widgets)
		*focusable_widgets = NULL;

	return FALSE;
}

void
e_gtk_container_set_focus_chain (gpointer container,
				 GList *focusable_widgets)
{
}
