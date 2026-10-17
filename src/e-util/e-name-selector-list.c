/*
 * Single-line text entry widget for EDestinations.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@novell.com>
 *		Devashish Sharma  <sdevashish@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>

#include "e-name-selector-list.h"
#include "e-name-selector-entry.h"
#include "e-misc-utils.h"

#define MAX_ROW	10

/* GTK 3 used a popup window with device grabs for the list; with GTK 4 it
 * is a popover, which closes itself on a click outside of it. */
struct _ENameSelectorListPrivate {
	GtkWidget *popup;
	GtkWidget *tree_view;
	gint rows;
};

G_DEFINE_TYPE_WITH_PRIVATE (ENameSelectorList, e_name_selector_list, E_TYPE_NAME_SELECTOR_ENTRY)

static void
enl_popup_size (ENameSelectorList *list)
{
	gint height = 0, count;
	GtkTreeViewColumn *column = NULL;

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (list->priv->tree_view), 0);
	if (column)
		gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, &height);

	/* Show a maximum of 10 rows in the popup list view */
	count = list->priv->rows;
	if (count > MAX_ROW)
		count = MAX_ROW;
	if (count <= 0)
		count = 1;

	gtk_widget_set_size_request (list->priv->tree_view,
		MAX (gtk_widget_get_width (GTK_WIDGET (list)) - 3, 50), height * count);
}

static void
enl_popup_hide (ENameSelectorList *list)
{
	gtk_popover_popdown (GTK_POPOVER (list->priv->popup));
}

static void
enl_tree_select_node (ENameSelectorList *list,
                      gint n)
{
	EDestinationStore *store;
	ENameSelectorEntry *entry;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	GtkTreePath *path;

	entry = E_NAME_SELECTOR_ENTRY (list);
	tree_view = GTK_TREE_VIEW (list->priv->tree_view);
	store = e_name_selector_entry_peek_destination_store (entry);
	selection = gtk_tree_view_get_selection (tree_view);
	iter.stamp = e_destination_store_get_stamp (store);
	iter.user_data = GINT_TO_POINTER (n - 1);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	column = gtk_tree_view_get_column (tree_view, 0);
	path = e_destination_store_get_path (GTK_TREE_MODEL (store), &iter);
	gtk_tree_view_scroll_to_cell (tree_view, path, column, FALSE, 0, 0);
	gtk_tree_view_set_cursor (tree_view, path, column, FALSE);
	gtk_widget_grab_focus (GTK_WIDGET (tree_view));

	gtk_tree_path_free (path);
}

static void
delete_row (GtkTreePath *path,
            ENameSelectorList *list)
{
	ENameSelectorEntry *entry;
	EDestinationStore *store;
	GtkTreeIter   iter;
	gint n, len;
	GtkTreeSelection *selection;

	entry = E_NAME_SELECTOR_ENTRY (list);
	store = e_name_selector_entry_peek_destination_store (entry);

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path))
		return;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->tree_view));
	len = e_destination_store_get_destination_count (store);
	n = GPOINTER_TO_INT (iter.user_data);

	e_destination_store_remove_destination_nth (store, n);

	/* If the last one is deleted select the last but one or the deleted +1 */
	if (n == len -1)
		n -= 1;

	/* We deleted the last entry */
	if (len == 1) {
		enl_popup_hide (list);
		return;
	}

	iter.stamp = e_destination_store_get_stamp (store);
	iter.user_data = GINT_TO_POINTER (n);

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	list->priv->rows = e_destination_store_get_destination_count (store);
	enl_popup_size (list);
}

/* The context menu of a destination */

typedef struct {
	ENameSelectorList *list;
	EDestination *destination;
	GtkTreePath *path;
} MenuData;

static void
menu_data_free (gpointer ptr)
{
	MenuData *md = ptr;

	g_clear_object (&md->destination);
	gtk_tree_path_free (md->path);
	g_free (md);
}

static void
menu_email_change_state_cb (GSimpleAction *action,
			    GVariant *value,
			    gpointer user_data)
{
	MenuData *md = user_data;
	EContact *contact;

	g_simple_action_set_state (action, value);

	contact = e_destination_get_contact (md->destination);
	if (contact)
		e_destination_set_contact (md->destination, contact, g_variant_get_int32 (value));
}

static void
menu_list_member_change_state_cb (GSimpleAction *action,
				  GVariant *value,
				  gpointer user_data)
{
	EDestination *dest = user_data;

	g_simple_action_set_state (action, value);
	e_destination_set_ignored (dest, !g_variant_get_boolean (value));
}

static void
menu_delete_cb (GSimpleAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	MenuData *md = user_data;

	delete_row (md->path, md->list);
}

static void
enl_tree_popup_menu (ENameSelectorList *list,
		     EDestination *destination,
		     GtkTreePath *path,
		     gdouble x,
		     gdouble y)
{
	GSimpleActionGroup *actions;
	GSimpleAction *action;
	GtkWidget *popover;
	GMenu *menu, *section;
	EContact *contact;
	MenuData *md;
	gchar *delete_label;

	contact = e_destination_get_contact (destination);
	if (!contact)
		return;

	md = g_new0 (MenuData, 1);
	md->list = list;
	md->destination = g_object_ref (destination);
	md->path = gtk_tree_path_copy (path);

	actions = g_simple_action_group_new ();
	menu = g_menu_new ();

	section = g_menu_new ();
	delete_label = g_strdup_printf (_("_Delete %s"), (gchar *) e_contact_get_const (contact, E_CONTACT_FILE_AS));
	g_menu_append (section, delete_label, "enl.delete");
	g_free (delete_label);
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	action = g_simple_action_new ("delete", NULL);
	g_signal_connect (action, "activate", G_CALLBACK (menu_delete_cb), md);
	g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
	g_object_unref (action);

	section = g_menu_new ();

	if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
		const GList *dests = e_destination_list_get_dests (destination);
		const GList *iter;
		gint index = 0, length = g_list_length ((GList *) dests);

		for (iter = dests; iter; iter = iter->next, index++) {
			EDestination *dest = iter->data;
			const gchar *email = e_destination_get_email (dest);

			if (!email || *email == '\0')
				continue;

			if (length > 1) {
				gchar *name = g_strdup_printf ("member-%d", index);
				gchar *detailed = g_strconcat ("enl.", name, NULL);

				action = g_simple_action_new_stateful (name, NULL,
					g_variant_new_boolean (!e_destination_is_ignored (dest)));
				g_signal_connect (action, "change-state", G_CALLBACK (menu_list_member_change_state_cb), dest);
				g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
				g_object_unref (action);

				g_menu_append (section, email, detailed);

				g_free (detailed);
				g_free (name);
			} else {
				g_menu_append (section, email, NULL);
			}
		}
	} else {
		GList *email_list, *link;
		gint ii, len;

		email_list = e_contact_get (contact, E_CONTACT_EMAIL);
		len = g_list_length (email_list);

		if (len > 1) {
			action = g_simple_action_new_stateful ("email", G_VARIANT_TYPE_INT32,
				g_variant_new_int32 (e_destination_get_email_num (destination)));
			g_signal_connect (action, "change-state", G_CALLBACK (menu_email_change_state_cb), md);
			g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
			g_object_unref (action);
		}

		for (link = email_list, ii = 0; link; link = g_list_next (link), ii++) {
			const gchar *email = link->data;
			GMenuItem *item;

			if (!email || *email == '\0')
				continue;

			item = g_menu_item_new (email, NULL);
			if (len > 1)
				g_menu_item_set_action_and_target_value (item, "enl.email", g_variant_new_int32 (ii));
			g_menu_append_item (section, item);
			g_object_unref (item);
		}

		g_list_free_full (email_list, g_free);
	}

	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
	gtk_widget_insert_action_group (popover, "enl", G_ACTION_GROUP (actions));
	g_object_set_data_full (G_OBJECT (popover), "enl-menu-data", md, menu_data_free);
	g_object_unref (actions);
	g_object_unref (menu);

	e_util_popup_menu (popover, list->priv->tree_view, x, y);
}

static void
enl_tree_pressed_cb (GtkGestureClick *gesture,
		     gint n_press,
		     gdouble x,
		     gdouble y,
		     ENameSelectorList *list)
{
	ENameSelectorEntry *entry;
	EDestinationStore *store;
	EDestination *destination;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	gint bx = 0, by = 0;

	entry = E_NAME_SELECTOR_ENTRY (list);
	tree_view = GTK_TREE_VIEW (list->priv->tree_view);
	store = e_name_selector_entry_peek_destination_store (entry);

	gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, x, y, &bx, &by);

	if (!gtk_tree_view_get_path_at_pos (tree_view, bx, by, &path, NULL, NULL, NULL))
		return;

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_iter (selection, &iter);

	destination = e_destination_store_get_destination (store, &iter);
	if (destination) {
		enl_tree_popup_menu (list, destination, path, x, y);
		gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
	}

	gtk_tree_path_free (path);
}

static gboolean
enl_tree_key_pressed_cb (GtkEventControllerKey *controller,
			 guint keyval,
			 guint keycode,
			 GdkModifierType state,
			 ENameSelectorList *list)
{
	if (keyval == GDK_KEY_Escape) {
		enl_popup_hide (list);
		return TRUE;
	} else if (keyval == GDK_KEY_Delete) {
		GtkTreeSelection *selection;
		GtkTreeView *tree_view;
		GList *paths;

		tree_view = GTK_TREE_VIEW (list->priv->tree_view);
		selection = gtk_tree_view_get_selection (tree_view);
		paths = gtk_tree_selection_get_selected_rows (selection, NULL);
		paths = g_list_reverse (paths);
		g_list_foreach (paths, (GFunc) delete_row, list);
		g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
		return TRUE;
	} else if (keyval != GDK_KEY_Up && keyval != GDK_KEY_Down
		   && keyval != GDK_KEY_Shift_R && keyval != GDK_KEY_Shift_L
		   && keyval != GDK_KEY_Control_R && keyval != GDK_KEY_Control_L) {
		/* GTK 3 passed the key to the entry; GTK 4 cannot re-send
		 * events, the entry gets the focus and the next keys */
		enl_popup_hide (list);
		gtk_widget_grab_focus (GTK_WIDGET (list));
		return TRUE;
	}

	return FALSE;
}

void
e_name_selector_list_expand_clicked (ENameSelectorList *list)
{
	ENameSelectorEntry *entry;
	EDestinationStore *store;

	entry = E_NAME_SELECTOR_ENTRY (list);
	store = e_name_selector_entry_peek_destination_store (entry);

	if (!gtk_widget_get_visible (list->priv->popup)) {
		gtk_tree_view_set_model (
			GTK_TREE_VIEW (list->priv->tree_view),
			GTK_TREE_MODEL (store));

		list->priv->rows = e_destination_store_get_destination_count (store);
		enl_popup_size (list);
		gtk_popover_popup (GTK_POPOVER (list->priv->popup));
		enl_tree_select_node (list, 1);
	} else {
		enl_popup_hide (list);
	}
}

static void
name_selector_list_realize (GtkWidget *widget)
{
	ENameSelectorList *list;
	ENameSelectorEntry *entry;
	EDestinationStore *store;

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_name_selector_list_parent_class)->realize (widget);

	list = E_NAME_SELECTOR_LIST (widget);
	entry = E_NAME_SELECTOR_ENTRY (widget);
	store = e_name_selector_entry_peek_destination_store (entry);

	gtk_tree_view_set_model (
		GTK_TREE_VIEW (list->priv->tree_view), GTK_TREE_MODEL (store));
}

static void
name_selector_list_size_allocate (GtkWidget *widget,
				  gint width,
				  gint height,
				  gint baseline)
{
	ENameSelectorList *list = E_NAME_SELECTOR_LIST (widget);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_name_selector_list_parent_class)->size_allocate (widget, width, height, baseline);

	gtk_popover_present (GTK_POPOVER (list->priv->popup));
}

static void
name_selector_list_dispose (GObject *object)
{
	ENameSelectorList *list;

	list = E_NAME_SELECTOR_LIST (object);

	g_clear_pointer (&list->priv->popup, gtk_widget_unparent);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_name_selector_list_parent_class)->dispose (object);
}

static void
e_name_selector_list_class_init (ENameSelectorListClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = name_selector_list_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = name_selector_list_realize;
	widget_class->size_allocate = name_selector_list_size_allocate;
}

static void
e_name_selector_list_init (ENameSelectorList *list)
{
	GtkCellRenderer *renderer;
	GtkWidget *scroll;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	ENameSelectorEntry *entry;
	EDestinationStore *store;
	GtkEntryCompletion *completion;
	GtkEventController *controller;

	list->priv = e_name_selector_list_get_instance_private (list);

	entry = E_NAME_SELECTOR_ENTRY (list);
	store = e_name_selector_entry_peek_destination_store (entry);

	list->priv->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list->priv->tree_view), FALSE);
	gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (list->priv->tree_view), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_unselect_all (selection);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (list->priv->tree_view), FALSE);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	completion = gtk_entry_get_completion (GTK_ENTRY (list));
	if (completion) {
		gtk_entry_completion_set_inline_completion (completion, TRUE);
		gtk_entry_completion_set_popup_completion (completion, TRUE);
	}
	G_GNUC_END_IGNORE_DEPRECATIONS

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", E_DESTINATION_STORE_COLUMN_ADDRESS, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (list->priv->tree_view), column);
	gtk_tree_view_column_set_clickable (column, TRUE);

	scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (scroll), FALSE);
	gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), list->priv->tree_view);

	list->priv->popup = gtk_popover_new ();
	gtk_popover_set_has_arrow (GTK_POPOVER (list->priv->popup), FALSE);
	gtk_popover_set_position (GTK_POPOVER (list->priv->popup), GTK_POS_BOTTOM);
	gtk_popover_set_child (GTK_POPOVER (list->priv->popup), scroll);
	gtk_widget_set_parent (list->priv->popup, GTK_WIDGET (list));

	controller = gtk_event_controller_key_new ();
	g_signal_connect (controller, "key-pressed", G_CALLBACK (enl_tree_key_pressed_cb), list);
	gtk_widget_add_controller (list->priv->tree_view, controller);

	controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), GDK_BUTTON_SECONDARY);
	g_signal_connect (controller, "pressed", G_CALLBACK (enl_tree_pressed_cb), list);
	gtk_widget_add_controller (list->priv->tree_view, controller);
}

GtkWidget *
e_name_selector_list_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_NAME_SELECTOR_LIST,
		"client-cache", client_cache, NULL);
}
