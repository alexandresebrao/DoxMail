/*
 * e-cell-combo.c: Combo cell renderer
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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ECellCombo - a subclass of ECellPopup used to support popup lists like a
 * GtkCombo widget. It only supports a basic popup list of strings at present,
 * with no auto-completion.
 */

/*
 * Notes: (handling pointer grabs and GTK+ grabs is a nightmare!)
 *
 * o We must grab the pointer when we show the popup, so that if any buttons
 *   are pressed outside the application we hide the popup.
 *
 * o We have to be careful when popping up any widgets which also grab the
 *   pointer at some point, since we will lose our own pointer grab.
 *   When we pop up a list it will grab the pointer itself when an item is
 *   selected, and release the grab when the button is released.
 *   Fortunately we hide the popup at this point, so it isn't a problem.
 *   But for other types of widgets in the popup it could cause trouble.
 *   - I think GTK+ should provide help for this (nested pointer grabs?).
 *
 * o We must set the 'owner_events' flag of the pointer grab to TRUE so that
 *   pointer events get reported to all the application windows as normal.
 *   If we don't do this then the widgets in the popup may not work properly.
 *
 * o We must do a gtk_grab_add() so that we only allow events to go to the
 *   widgets within the popup (though some special events still get reported
 *   to the widget owning the window). Doing th gtk_grab_add() on the toplevel
 *   popup window should be fine. We can then check for any events that should
 *   close the popup, like the Escape key, or a button press outside the popup.
 */

#include "evolution-config.h"

#include "e-cell-combo.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-cell-text.h"
#include "e-table-item.h"
#include "e-unicode.h"

#define d(x)

/* The height to make the popup list if there aren't any items in it. */
#define	E_CELL_COMBO_LIST_EMPTY_HEIGHT	15

static void	e_cell_combo_dispose		(GObject *object);
static gint	e_cell_combo_do_popup		(ECellPopup *ecp,
						 GnomeCanvasEvent *event,
						 gint row,
						 gint view_col);
static void	e_cell_combo_select_matching_item
						(ECellCombo *ecc);
static void	e_cell_combo_show_popup		(ECellCombo *ecc,
						 gint row,
						 gint view_col);
static void	e_cell_combo_selection_changed	(GtkTreeSelection *selection,
						 ECellCombo *ecc);
static void	e_cell_combo_row_activated	(GtkTreeView *tree_view,
						 GtkTreePath *path,
						 GtkTreeViewColumn *column,
						 ECellCombo *ecc);
static void	e_cell_combo_popover_closed	(GtkPopover *popover,
						 ECellCombo *ecc);
static void	e_cell_combo_update_cell	(ECellCombo *ecc);
static void	e_cell_combo_restart_edit	(ECellCombo *ecc);

G_DEFINE_TYPE (ECellCombo, e_cell_combo, E_TYPE_CELL_POPUP)

static void
e_cell_combo_class_init (ECellComboClass *class)
{
	ECellPopupClass *ecpc = E_CELL_POPUP_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = e_cell_combo_dispose;

	ecpc->popup = e_cell_combo_do_popup;
}

/* GTK 3 used a popup window with device grabs; GTK 4 has popovers, which
 * close themselves on a click outside of them and on Escape. */
static void
e_cell_combo_init (ECellCombo *ecc)
{
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkScrolledWindow *scrolled_window;
	GtkCellRenderer *renderer;

	/* We create one popup for the ECell, since there will only
	 * ever be one popup in use at a time. */
	ecc->popup_window = gtk_popover_new ();
	g_object_ref_sink (ecc->popup_window);
	gtk_popover_set_has_arrow (GTK_POPOVER (ecc->popup_window), FALSE);

	ecc->popup_scrolled_window = gtk_scrolled_window_new ();
	scrolled_window = GTK_SCROLLED_WINDOW (ecc->popup_scrolled_window);

	gtk_scrolled_window_set_policy (
		scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height (scrolled_window, TRUE);
	gtk_scrolled_window_set_propagate_natural_width (scrolled_window, TRUE);
	gtk_scrolled_window_set_max_content_height (scrolled_window, 300);
	gtk_popover_set_child (GTK_POPOVER (ecc->popup_window), ecc->popup_scrolled_window);

	store = gtk_list_store_new (1, G_TYPE_STRING);
	ecc->popup_tree_view =
		gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	ecc->popup_renderer = renderer;

	gtk_tree_view_append_column (
		GTK_TREE_VIEW (ecc->popup_tree_view),
		gtk_tree_view_column_new_with_attributes (
			"Text", renderer, "text", 0, NULL));

	gtk_tree_view_set_headers_visible (
		GTK_TREE_VIEW (ecc->popup_tree_view), FALSE);
	gtk_tree_view_set_activate_on_single_click (
		GTK_TREE_VIEW (ecc->popup_tree_view), TRUE);

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (ecc->popup_tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_scrolled_window_set_child (scrolled_window, ecc->popup_tree_view);

	e_gtk_widget_set_accessible_name (ecc->popup_tree_view, _("popup list"));

	g_signal_connect (
		ecc->popup_tree_view, "row-activated",
		G_CALLBACK (e_cell_combo_row_activated), ecc);
	g_signal_connect (
		ecc->popup_window, "closed",
		G_CALLBACK (e_cell_combo_popover_closed), ecc);
}

/**
 * e_cell_combo_new:
 *
 * Creates a new ECellCombo renderer.
 *
 * Returns: an ECellCombo object.
 */
ECell *
e_cell_combo_new (void)
{
	return g_object_new (E_TYPE_CELL_COMBO, NULL);
}

/*
 * GObject::dispose method
 */
static void
e_cell_combo_dispose (GObject *object)
{
	ECellCombo *ecc = E_CELL_COMBO (object);

	if (ecc->popup_window) {
		g_signal_handlers_disconnect_by_data (ecc->popup_window, ecc);
		if (gtk_widget_get_parent (ecc->popup_window))
			gtk_widget_unparent (ecc->popup_window);
		g_clear_object (&ecc->popup_window);
	}

	G_OBJECT_CLASS (e_cell_combo_parent_class)->dispose (object);
}

void
e_cell_combo_set_popdown_strings (ECellCombo *ecc,
                                  GList *strings)
{
	GList *elem;
	GtkListStore *store;

	g_return_if_fail (E_IS_CELL_COMBO (ecc));
	g_return_if_fail (strings != NULL);

	store = GTK_LIST_STORE (
		gtk_tree_view_get_model (
		GTK_TREE_VIEW (ecc->popup_tree_view)));
	gtk_list_store_clear (store);

	for (elem = strings; elem; elem = elem->next) {
		GtkTreeIter iter;
		gchar *utf8_text = elem->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, utf8_text, -1);
	}
}

void
e_cell_combo_use_tabular_numbers (ECellCombo *ecc)
{
	PangoAttrList *tnum = pango_attr_list_new ();
	PangoAttribute *attr = pango_attr_font_features_new ("tnum=1");
	pango_attr_list_insert_before (tnum, attr);
	g_object_set (ecc->popup_renderer, "attributes", tnum, NULL);
	pango_attr_list_unref (tnum);
}

static gint
e_cell_combo_do_popup (ECellPopup *ecp,
                       GnomeCanvasEvent *event,
                       gint row,
                       gint view_col)
{
	ECellCombo *ecc = E_CELL_COMBO (ecp);
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (ecc->popup_tree_view));

	g_signal_handlers_block_by_func (
		selection, e_cell_combo_selection_changed, ecc);

	e_cell_combo_show_popup (ecc, row, view_col);
	e_cell_combo_select_matching_item (ecc);

	g_signal_handlers_unblock_by_func (
		selection, e_cell_combo_selection_changed, ecc);

	gtk_widget_grab_focus (ecc->popup_tree_view);

	return TRUE;
}

static void
e_cell_combo_select_matching_item (ECellCombo *ecc)
{
	ECellPopup *ecp = E_CELL_POPUP (ecc);
	ECellView *ecv = (ECellView *) ecp->popup_cell_view;
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ETableItem *eti;
	ETableCol *ecol;
	gboolean found = FALSE;
	gchar *cell_text;
	gboolean valid;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;

	eti = E_TABLE_ITEM (ecp->popup_cell_view->cell_view.e_table_item_view);

	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);
	cell_text = e_cell_text_get_text (
		ecell_text, ecv->e_table_model,
		ecol->spec->model_col, ecp->popup_row);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (ecc->popup_tree_view));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (ecc->popup_tree_view));

	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid && !found;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gchar *str = NULL;

		gtk_tree_model_get (model, &iter, 0, &str, -1);

		if (str && g_str_equal (str, cell_text)) {
			GtkTreePath *path;

			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_view_set_cursor (
				GTK_TREE_VIEW (ecc->popup_tree_view),
				path, NULL, FALSE);
			gtk_tree_path_free (path);

			found = TRUE;
		}

		g_free (str);
	}

	if (!found)
		gtk_tree_selection_unselect_all (selection);

	e_cell_text_free_text (ecell_text, ecv->e_table_model,
		ecol->spec->model_col, cell_text);
}

static void
e_cell_combo_show_popup (ECellCombo *ecc,
                         gint row,
                         gint view_col)
{
	GtkWidget *canvas = NULL;
	GdkRectangle rect;

	/* at least as wide as the column */
	if (e_cell_popup_get_cell_rect (E_CELL_POPUP (ecc), &canvas, &rect))
		gtk_widget_set_size_request (ecc->popup_scrolled_window, rect.width, -1);

	e_cell_popup_show_popover (E_CELL_POPUP (ecc), ecc->popup_window);
	d (g_print ("%s: popup_shown = TRUE\n", G_STRFUNC));
}

static void
e_cell_combo_selection_changed (GtkTreeSelection *selection,
                                ECellCombo *ecc)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (!gtk_widget_get_visible (ecc->popup_window) ||
	    !gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	e_cell_combo_update_cell (ecc);
	e_cell_combo_restart_edit (ecc);
}

/* Picking a row updates the cell and closes the popup */
static void
e_cell_combo_row_activated (GtkTreeView *tree_view,
			    GtkTreePath *path,
			    GtkTreeViewColumn *column,
			    ECellCombo *ecc)
{
	e_cell_combo_update_cell (ecc);

	gtk_popover_popdown (GTK_POPOVER (ecc->popup_window));
}

/* A click outside or Escape: the cell is not changed. Since the list is in
 * browse mode there will always be one item selected, which does not need to
 * match the current text of the cell. */
static void
e_cell_combo_popover_closed (GtkPopover *popover,
			     ECellCombo *ecc)
{
	e_cell_popup_set_shown (E_CELL_POPUP (ecc), FALSE);
	d (g_print ("%s: popup_shown = FALSE\n", G_STRFUNC));

	e_cell_combo_restart_edit (ecc);
}

static void
e_cell_combo_update_cell (ECellCombo *ecc)
{
	ECellPopup *ecp = E_CELL_POPUP (ecc);
	ECellView *ecv = (ECellView *) ecp->popup_cell_view;
	ECellText *ecell_text = E_CELL_TEXT (ecp->child);
	ETableItem *eti = E_TABLE_ITEM (ecv->e_table_item_view);
	ETableCol *ecol;
	GtkTreeSelection *selection = gtk_tree_view_get_selection (
		GTK_TREE_VIEW (ecc->popup_tree_view));
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text = NULL, *old_text;

	/* Return if no item is selected. */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	/* Get the text of the selected item. */
	gtk_tree_model_get (model, &iter, 0, &text, -1);
	g_return_if_fail (text != NULL);

	/* Compare it with the existing cell contents. */
	ecol = e_table_header_get_column (eti->header, ecp->popup_view_col);

	old_text = e_cell_text_get_text (
		ecell_text, ecv->e_table_model,
		ecol->spec->model_col, ecp->popup_row);

	/* If they are different, update the cell contents. */
	if (old_text && strcmp (old_text, text)) {
		e_cell_text_set_value (
			ecell_text, ecv->e_table_model,
			ecol->spec->model_col, ecp->popup_row, text);
	}

	e_cell_text_free_text (ecell_text, ecv->e_table_model,
		ecol->spec->model_col, old_text);
	g_free (text);
}

static void
e_cell_combo_restart_edit (ECellCombo *ecc)
{
	/* This doesn't work. ETable stops the edit straight-away again. */
#if 0
	ECellView *ecv = (ECellView *) ecc->popup_cell_view;
	ETableItem *eti = E_TABLE_ITEM (ecv->e_table_item_view);

	e_table_item_enter_edit (eti, ecc->popup_view_col, ecc->popup_row);
#endif
}

