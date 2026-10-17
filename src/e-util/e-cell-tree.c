/*
 * e-cell-tree.c - Tree cell object.
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * Authors:
 *   Chris Toshok <toshok@ximian.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
 * Copyright 1998, The Free Software Foundation
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "evolution-config.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>


#include "e-cell-tree.h"
#include "e-table-item.h"
#include "e-tree.h"
#include "e-tree-model.h"
#include "e-tree-table-adapter.h"

G_DEFINE_TYPE (ECellTree, e_cell_tree, E_TYPE_CELL)

typedef struct {
	ECellView    cell_view;
	ECellView   *subcell_view;

	GnomeCanvas *canvas;
	gboolean prelit;
	gint animate_timeout;

} ECellTreeView;

#define INDENT_AMOUNT 16

ECellView *
e_cell_tree_view_get_subcell_view (ECellView *ect)
{
	return ((ECellTreeView *) ect)->subcell_view;
}

static ETreePath
e_cell_tree_get_node (ETableModel *table_model,
                      gint row)
{
	return e_table_model_value_at (table_model, -1, row);
}

static ETreeModel *
e_cell_tree_get_tree_model (ETableModel *table_model,
                            gint row)
{
	return e_table_model_value_at (table_model, -2, row);
}

static ETreeTableAdapter *
e_cell_tree_get_tree_table_adapter (ETableModel *table_model,
                                    gint row)
{
	return e_table_model_value_at (table_model, -3, row);
}

static gint
visible_depth_of_node (ETableModel *model,
                       gint row)
{
	ETreeModel *tree_model = e_cell_tree_get_tree_model (model, row);
	ETreeTableAdapter *adapter = e_cell_tree_get_tree_table_adapter (model, row);
	ETreePath path = e_cell_tree_get_node (model, row);
	return (e_tree_model_node_depth (tree_model, path)
		- (e_tree_table_adapter_root_node_is_visible (adapter) ? 0 : 1));
}

/* If this is changed to not include the width of the expansion pixmap
 * if the path is not expandable, then max_width needs to change as
 * well. */
static gint
offset_of_node (ECellTreeView *tree_view,
		ETableModel *table_model,
		gint row,
		gint view_col)
{
	ETreeModel *tree_model = e_cell_tree_get_tree_model (table_model, row);
	ETreePath path = e_cell_tree_get_node (table_model, row);
	gint visible_depth;

	visible_depth = visible_depth_of_node (table_model, row);
	if (visible_depth >= 0 || e_tree_model_node_is_expandable (tree_model, path)) {
		if (visible_depth > 0) {
			gint width = 0;

			e_table_item_get_cell_geometry (
				tree_view->cell_view.e_table_item_view,
				&row, &view_col, NULL, NULL, &width, NULL);

			if (width > 0) {
				/* Use up to 70% of the column width */
				gint max_depth = (width * 70 / 100) / INDENT_AMOUNT;

				visible_depth = MIN (visible_depth, max_depth);
			}
		}

		return (MAX (visible_depth, 1)) * INDENT_AMOUNT;
	} else {
		return 0;
	}
}

/*
 * ECell::new_view method
 */
static ECellView *
ect_new_view (ECell *ecell,
              ETableModel *table_model,
              gpointer e_table_item_view)
{
	ECellTree *ect = E_CELL_TREE (ecell);
	ECellTreeView *tree_view = g_new0 (ECellTreeView, 1);
	GnomeCanvas *canvas = GNOME_CANVAS_ITEM (e_table_item_view)->canvas;

	tree_view->cell_view.ecell = ecell;
	tree_view->cell_view.e_table_model = table_model;
	tree_view->cell_view.e_table_item_view = e_table_item_view;
	tree_view->cell_view.kill_view_cb = NULL;
	tree_view->cell_view.kill_view_cb_data = NULL;

	/* create our subcell view */
	tree_view->subcell_view = e_cell_new_view (ect->subcell, table_model, e_table_item_view /* XXX */);

	tree_view->canvas = canvas;

	return (ECellView *) tree_view;
}

/*
 * ECell::kill_view method
 */
static void
ect_kill_view (ECellView *ecv)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecv;

	if (tree_view->animate_timeout) {
		g_source_remove (tree_view->animate_timeout);
		tree_view->animate_timeout = 0;
	}

	if (tree_view->cell_view.kill_view_cb)
	    (tree_view->cell_view.kill_view_cb)(ecv, tree_view->cell_view.kill_view_cb_data);

	if (tree_view->cell_view.kill_view_cb_data)
	    g_list_free (tree_view->cell_view.kill_view_cb_data);

	/* kill our subcell view */
	e_cell_kill_view (tree_view->subcell_view);

	g_free (tree_view);
}

/*
 * ECell::realize method
 */
static void
ect_realize (ECellView *ecell_view)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	/* realize our subcell view */
	e_cell_realize (tree_view->subcell_view);

	if (E_CELL_CLASS (e_cell_tree_parent_class)->realize)
		(* E_CELL_CLASS (e_cell_tree_parent_class)->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ect_unrealize (ECellView *ecv)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecv;

	/* unrealize our subcell view. */
	e_cell_unrealize (tree_view->subcell_view);

	if (E_CELL_CLASS (e_cell_tree_parent_class)->unrealize)
		(* E_CELL_CLASS (e_cell_tree_parent_class)->unrealize) (ecv);
}

/* GTK 4 has no expander styles nor GtkStateType */
typedef enum {
	E_EXPANDER_COLLAPSED,
	E_EXPANDER_SEMI_COLLAPSED,
	E_EXPANDER_SEMI_EXPANDED,
	E_EXPANDER_EXPANDED
} EExpanderStyle;

/* An arrow, like GtkTreeView draws, pointing to the side when collapsed
 * and down when expanded; the "semi" styles are half-way. */
static void
draw_expander (ECellTreeView *ectv,
               cairo_t *cr,
               EExpanderStyle expander_style,
               gboolean selected,
               GdkRectangle *rect)
{
	GtkWidget *tree;
	GdkRGBA rgba;
	gdouble angle, cx, cy, size;
	gint exp_size = 12;

	if (!E_CELL_TREE (ectv->cell_view.ecell)->show_expander)
		return;

	tree = gtk_widget_get_parent (GTK_WIDGET (ectv->canvas));

	e_gtk_widget_style_get (tree, "expander_size", &exp_size, NULL);

	switch (expander_style) {
		case E_EXPANDER_COLLAPSED:
			angle = 0;
			break;
		case E_EXPANDER_SEMI_COLLAPSED:
		case E_EXPANDER_SEMI_EXPANDED:
			angle = G_PI / 4;
			break;
		default:
			angle = G_PI / 2;
			break;
	}

	if (gtk_widget_get_direction (tree) == GTK_TEXT_DIR_RTL && expander_style != E_EXPANDER_EXPANDED)
		angle = G_PI - angle;

	size = exp_size / 2.0;
	cx = rect->x + rect->width - exp_size / 2.0;
	cy = rect->y + rect->height / 2.0;

	gtk_widget_get_color (tree, &rgba);
	if (!selected)
		rgba.alpha *= 0.7;

	cairo_save (cr);
	cairo_translate (cr, cx, cy);
	cairo_rotate (cr, angle);
	cairo_move_to (cr, -size / 2, -size / 2);
	cairo_line_to (cr, size / 2, 0);
	cairo_line_to (cr, -size / 2, size / 2);
	cairo_close_path (cr);
	gdk_cairo_set_source_rgba (cr, &rgba);
	cairo_fill (cr);
	cairo_restore (cr);
}

/*
 * ECell::draw method
 */
static void
ect_draw (ECellView *ecell_view,
          cairo_t *cr,
          gint model_col,
          gint view_col,
          gint row,
          ECellFlags flags,
          gint x1,
          gint y1,
          gint x2,
          gint y2)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
	ETreeTableAdapter *tree_table_adapter = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
	ETreePath node;
	GdkRectangle rect;
	gint offset, subcell_offset = 0;

	cairo_save (cr);

	/* only draw the tree effects if we're the active sort */
	if (E_CELL_TREE (tree_view->cell_view.ecell)->grouped_view) {
		tree_view->prelit = FALSE;

		node = e_cell_tree_get_node (ecell_view->e_table_model, row);

		offset = offset_of_node (tree_view, ecell_view->e_table_model, row, view_col);
		subcell_offset = offset;

		/*
		 * Be a nice citizen: clip to the region we are supposed to draw on
		 */
		rect.x = x1;
		rect.y = y1;
		rect.width = subcell_offset;
		rect.height = y2 - y1;

		/* now draw our icon if we're expandable */
		if (E_CELL_TREE (tree_view->cell_view.ecell)->show_expander &&
		    e_tree_model_node_is_expandable (tree_model, node)) {
			gboolean expanded = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);
			GdkRectangle r;

			r = rect;
			r.width -= 2;
			draw_expander (tree_view, cr,
				expanded ? E_EXPANDER_EXPANDED : E_EXPANDER_COLLAPSED,
				(flags & E_CELL_SELECTED) != 0, &r);
		}
	}

	/* Now cause our subcell to draw its contents, shifted by
	 * subcell_offset pixels */
	e_cell_draw (
		tree_view->subcell_view, cr,
		model_col, view_col, row, flags,
		x1 + subcell_offset, y1, x2, y2);

	cairo_restore (cr);
}

static void
adjust_event_position (GnomeCanvasEvent *event,
                       gint offset)
{
	switch (event->type) {
	case GNOME_CANVAS_BUTTON_PRESS:
	case GNOME_CANVAS_BUTTON_RELEASE:
	case GNOME_CANVAS_2BUTTON_PRESS:
	case GNOME_CANVAS_3BUTTON_PRESS:
		event->button.x += offset;
		break;
	case GNOME_CANVAS_MOTION_NOTIFY:
		event->motion.x += offset;
		break;
	default:
		break;
	}
}

static gboolean
event_in_expander (GnomeCanvasEvent *event,
                   gint offset,
                   gint height)
{
	switch (event->type) {
	case GNOME_CANVAS_BUTTON_PRESS:
		return (event->button.x > (offset - INDENT_AMOUNT) && event->button.x < offset);
	case GNOME_CANVAS_MOTION_NOTIFY:
		return (event->motion.x > (offset - INDENT_AMOUNT) && event->motion.x < offset &&
			event->motion.y > 2 && event->motion.y < (height - 2));
	default:
		break;
	}

	return FALSE;
}

/*
 * ECell::height method
 */
static gint
ect_height (ECellView *ecell_view,
            gint model_col,
            gint view_col,
            gint row)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return (((e_cell_height (tree_view->subcell_view, model_col, view_col, row)) + 1) / 2) * 2;
}

typedef struct {
	ECellTreeView *ectv;
	ETreeTableAdapter *etta;
	ETreePath node;
	gboolean expanded;
	gboolean selected;
	gboolean finish;
	GdkRectangle area;
} animate_closure_t;

static gboolean
animate_expander (gpointer data)
{
	animate_closure_t *closure = (animate_closure_t *) data;

	if (g_source_is_destroyed (g_main_current_source ()))
		return FALSE;

	/* GTK 3 painted a half-way expander first; GTK 4 cannot paint outside
	 * of a frame, thus it just changes the state */
	e_tree_table_adapter_node_set_expanded (closure->etta, closure->node, !closure->expanded);
	closure->ectv->animate_timeout = 0;

	return FALSE;
}

/*
 * ECell::event method
 */
static gint
ect_event (ECellView *ecell_view,
           GnomeCanvasEvent *event,
           gint model_col,
           gint view_col,
           gint row,
           ECellFlags flags,
           ECellActions *actions)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
	ETreeTableAdapter *etta = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
	ETreePath node = e_cell_tree_get_node (ecell_view->e_table_model, row);
	gint offset = offset_of_node (tree_view, ecell_view->e_table_model, row, view_col);
	gboolean selected = e_table_item_get_row_selected (tree_view->cell_view.e_table_item_view, row);
	gint result;

	switch (event->type) {
	case GNOME_CANVAS_BUTTON_PRESS:

		if (E_CELL_TREE (tree_view->cell_view.ecell)->show_expander &&
		    event_in_expander (event, offset, 0)) {
			if (e_tree_model_node_is_expandable (tree_model, node)) {
				gboolean expanded = e_tree_table_adapter_node_is_expanded (etta, node);
				gint tmp_row = row;
				GdkRectangle area;
				animate_closure_t *closure = g_new0 (animate_closure_t, 1);
				cairo_t *cr;
				gint hgt;

				e_table_item_get_cell_geometry (
					tree_view->cell_view.e_table_item_view,
					&tmp_row, &view_col, &area.x, &area.y, NULL, &area.height);
				area.width = offset - 2;
				hgt = e_cell_height (ecell_view, model_col, view_col, row);

				if (hgt != area.height) /* Composite cells */
					area.height += hgt;


				closure->ectv = tree_view;
				closure->etta = etta;
				closure->node = node;
				closure->expanded = expanded;
				closure->selected = selected;
				closure->area = area;
				tree_view->animate_timeout =
					e_named_timeout_add_full (G_PRIORITY_DEFAULT,
					50, animate_expander, closure, g_free);
				return TRUE;
			}
		}
		else if (event->button.x < (offset - INDENT_AMOUNT))
			return FALSE;
		break;

	case GNOME_CANVAS_MOTION_NOTIFY:

		if (E_CELL_TREE (tree_view->cell_view.ecell)->show_expander &&
		    e_tree_model_node_is_expandable (tree_model, node)) {
			gint height = ect_height (ecell_view, model_col, view_col, row);
			GdkRectangle area;
			gboolean in_expander = event_in_expander (event, offset, height);

			if (tree_view->prelit ^ in_expander) {
				gint tmp_row = row;
				cairo_t *cr;

				e_table_item_get_cell_geometry (
					tree_view->cell_view.e_table_item_view,
					&tmp_row, &view_col, &area.x, &area.y, NULL, &area.height);
				area.width = offset - 2;

				/* FIXME-GTK4: no prelight of the expander */
				gtk_widget_queue_draw (GTK_WIDGET (tree_view->canvas));

				tree_view->prelit = in_expander;
				return TRUE;
			}

		}
		break;

	case GNOME_CANVAS_LEAVE_NOTIFY:

		if (tree_view->prelit) {
			gint tmp_row = row;
			GdkRectangle area;
			cairo_t *cr;

			e_table_item_get_cell_geometry (
				tree_view->cell_view.e_table_item_view,
				&tmp_row, &view_col, &area.x, &area.y, NULL, &area.height);
			area.width = offset - 2;

			gtk_widget_queue_draw (GTK_WIDGET (tree_view->canvas));

			tree_view->prelit = FALSE;
		}
		return TRUE;

	default:
		break;
	}

	adjust_event_position (event, -offset);
	result = e_cell_event (tree_view->subcell_view, event, model_col, view_col, row, flags, actions);
	adjust_event_position (event, offset);

	return result;
}

/*
 * ECell::max_width method
 */
static gint
ect_max_width (ECellView *ecell_view,
               gint model_col,
               gint view_col)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	gint row;
	gint number_of_rows;
	gint max_width = 0;
	gint width = 0;
	gint subcell_max_width = 0;
	gboolean per_row = e_cell_max_width_by_row_implemented (tree_view->subcell_view);

	number_of_rows = e_table_model_row_count (ecell_view->e_table_model);

	if (!per_row)
		subcell_max_width = e_cell_max_width (tree_view->subcell_view, model_col, view_col);

	for (row = 0; row < number_of_rows; row++) {
		gint offset, subcell_offset;
#if 0
		gboolean expanded, expandable;
		ETreeTableAdapter *tree_table_adapter = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
#endif

		offset = offset_of_node (tree_view, ecell_view->e_table_model, row, view_col);
		subcell_offset = offset;

		width = subcell_offset;

		if (per_row)
			width += e_cell_max_width_by_row (tree_view->subcell_view, model_col, view_col, row);
		else
			width += subcell_max_width;

#if 0
		expandable = e_tree_model_node_is_expandable (tree_model, node);
		expanded = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);

		/* This is unnecessary since this is already handled
		 * by the offset_of_node function.  If that changes,
		 * this will have to change too. */

		if (expandable) {
			GdkPixbuf *image;

			image = (expanded
				 ? E_CELL_TREE (tree_view->cell_view.ecell)->open_pixbuf
				 : E_CELL_TREE (tree_view->cell_view.ecell)->closed_pixbuf);

			width += gdk_pixbuf_get_width (image);
		}
#endif

		max_width = MAX (max_width, width);
	}

	return max_width;
}

/*
 * ECellView::get_bg_color method
 */
static gchar *
ect_get_bg_color (ECellView *ecell_view,
                  gint row)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return e_cell_get_bg_color (tree_view->subcell_view, row);
}

/*
 * ECellView::enter_edit method
 */
static gpointer
ect_enter_edit (ECellView *ecell_view,
                gint model_col,
                gint view_col,
                gint row)
{
	/* just defer to our subcell's view */
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	return e_cell_enter_edit (tree_view->subcell_view, model_col, view_col, row);
}

/*
 * ECellView::leave_edit method
 */
static void
ect_leave_edit (ECellView *ecell_view,
                gint model_col,
                gint view_col,
                gint row,
                gpointer edit_context)
{
	/* just defer to our subcell's view */
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;

	e_cell_leave_edit (tree_view->subcell_view, model_col, view_col, row, edit_context);
}

static void
ect_print (ECellView *ecell_view,
           GtkPrintContext *context,
           gint model_col,
           gint view_col,
           gint row,
           gdouble width,
           gdouble height)
{
	ECellTreeView *tree_view = (ECellTreeView *) ecell_view;
	cairo_t *cr = gtk_print_context_get_cairo_context (context);

	cairo_save (cr);

	if (E_CELL_TREE (tree_view->cell_view.ecell)->grouped_view) {
		ETreeModel *tree_model = e_cell_tree_get_tree_model (ecell_view->e_table_model, row);
		ETreeTableAdapter *tree_table_adapter = e_cell_tree_get_tree_table_adapter (ecell_view->e_table_model, row);
		ETreePath node = e_cell_tree_get_node (ecell_view->e_table_model, row);
		gint offset = offset_of_node (tree_view, ecell_view->e_table_model, row, view_col);
		gint subcell_offset = offset;
		gboolean expandable = e_tree_model_node_is_expandable (tree_model, node);

		/* draw our lines */
		if (E_CELL_TREE (tree_view->cell_view.ecell)->draw_lines) {
			gint depth;

			if (!e_tree_model_node_is_root (tree_model, node)
			    || e_tree_model_node_get_n_children (tree_model, node) > 0) {
				cairo_move_to (
					cr,
					offset - INDENT_AMOUNT / 2,
					height / 2);
				cairo_line_to (cr, offset, height / 2);
			}

			if (visible_depth_of_node (ecell_view->e_table_model, row) != 0) {
				cairo_move_to (
					cr,
					offset - INDENT_AMOUNT / 2, height);
				cairo_line_to (
					cr,
					offset - INDENT_AMOUNT / 2,
					e_tree_table_adapter_node_get_next
					(tree_table_adapter, node) ? 0 :
					height / 2);
			}

			/* now traverse back up to the root of the tree, checking at
			 * each level if the node has siblings, and drawing the
			 * correct vertical pipe for it's configuration. */
			node = e_tree_model_node_get_parent (tree_model, node);
			depth = visible_depth_of_node (ecell_view->e_table_model, row) - 1;
			offset -= INDENT_AMOUNT;
			while (node && depth != 0) {
				if (e_tree_table_adapter_node_get_next (tree_table_adapter, node)) {
					cairo_move_to (
						cr,
						offset - INDENT_AMOUNT / 2,
						height);
					cairo_line_to (
						cr,
						offset - INDENT_AMOUNT / 2, 0);
				}
				node = e_tree_model_node_get_parent (tree_model, node);
				depth--;
				offset -= INDENT_AMOUNT;
			}
		}

		/* now draw our icon if we're expandable */
		if (expandable && E_CELL_TREE (tree_view->cell_view.ecell)->show_expander) {
			gboolean expanded;
			GdkRectangle r;
			gint exp_size = 0;

			e_gtk_widget_style_get (GTK_WIDGET (gtk_widget_get_parent (GTK_WIDGET (tree_view->canvas))), "expander_size", &exp_size, NULL);

			node = e_cell_tree_get_node (ecell_view->e_table_model, row);
			expanded = e_tree_table_adapter_node_is_expanded (tree_table_adapter, node);

			r.x = 0;
			r.y = 0;
			r.width = MIN (width, exp_size);
			r.height = height;

			draw_expander (tree_view, cr, expanded ? E_EXPANDER_EXPANDED : E_EXPANDER_COLLAPSED, FALSE, &r);
		}

		cairo_stroke (cr);

		cairo_translate (cr, subcell_offset, 0);
		width -= subcell_offset;
	}

	cairo_restore (cr);

	e_cell_print (tree_view->subcell_view, context, model_col, view_col, row, width, height);
}

static gdouble
ect_print_height (ECellView *ecell_view,
                  GtkPrintContext *context,
                  gint model_col,
                  gint view_col,
                  gint row,
                  gdouble width)
{
	return 12; /* XXX */
}

/*
 * GObject::dispose method
 */
static void
ect_dispose (GObject *object)
{
	ECellTree *ect = E_CELL_TREE (object);

	/* destroy our subcell */
	g_clear_object (&ect->subcell);

	G_OBJECT_CLASS (e_cell_tree_parent_class)->dispose (object);
}

static void
e_cell_tree_class_init (ECellTreeClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	ECellClass *ecc = E_CELL_CLASS (class);

	object_class->dispose = ect_dispose;

	ecc->new_view = ect_new_view;
	ecc->kill_view = ect_kill_view;
	ecc->realize = ect_realize;
	ecc->unrealize = ect_unrealize;
	ecc->draw = ect_draw;
	ecc->event = ect_event;
	ecc->height = ect_height;
	ecc->enter_edit = ect_enter_edit;
	ecc->leave_edit = ect_leave_edit;
	ecc->print = ect_print;
	ecc->print_height = ect_print_height;
	ecc->max_width = ect_max_width;
	ecc->get_bg_color = ect_get_bg_color;

}

static void
e_cell_tree_init (ECellTree *ect)
{
	/* nothing to do */
}

/**
 * e_cell_tree_construct:
 * @ect: the ECellTree we're constructing.
 * @draw_lines: whether or not to draw the lines between parents/children/siblings.
 * @show_expander: whether to show expander
 * @subcell: the ECell to render to the right of the tree effects.
 *
 * Constructs an ECellTree.  used by subclasses that need to
 * initialize a nested ECellTree.  See e_cell_tree_new() for more info.
 *
 **/
void
e_cell_tree_construct (ECellTree *ect,
                       gboolean draw_lines,
		       gboolean show_expander,
                       ECell *subcell)
{
	ect->subcell = subcell;
	if (subcell)
		g_object_ref_sink (subcell);

	ect->draw_lines = draw_lines;
	ect->show_expander = show_expander;
	ect->grouped_view = TRUE;
}

/**
 * e_cell_tree_new:
 * @draw_lines: whether or not to draw the lines between parents/children/siblings.
 * @show_expander: whether to show expander
 * @subcell: the ECell to render to the right of the tree effects.
 *
 * Creates a new ECell renderer that can be used to render tree
 * effects that come from an ETreeModel.  Various assumptions are made
 * as to the fact that the ETableModel the ETable this cell is
 * associated with is in fact an ETreeModel.  The cell uses special
 * columns to get at structural information (needed to draw the
 * lines/icons.
 *
 * Return value: an ECell object that can be used to render trees.
 **/
ECell *
e_cell_tree_new (gboolean draw_lines,
		 gboolean show_expander,
                 ECell *subcell)
{
	ECellTree *ect = g_object_new (E_TYPE_CELL_TREE, NULL);

	e_cell_tree_construct (ect, draw_lines, show_expander, subcell);

	return (ECell *) ect;
}

gboolean
e_cell_tree_get_grouped_view (ECellTree *cell_tree)
{
	g_return_val_if_fail (E_IS_CELL_TREE (cell_tree), FALSE);

	return cell_tree->grouped_view;
}

void
e_cell_tree_set_grouped_view (ECellTree *cell_tree,
			      gboolean grouped_view)
{
	g_return_if_fail (E_IS_CELL_TREE (cell_tree));

	cell_tree->grouped_view = grouped_view;
}

gboolean
e_cell_tree_get_show_expander (ECellTree *cell_tree)
{
	g_return_val_if_fail (E_IS_CELL_TREE (cell_tree), FALSE);

	return cell_tree->show_expander;
}

void
e_cell_tree_set_show_expander (ECellTree *cell_tree,
			       gboolean show_expander)
{
	g_return_if_fail (E_IS_CELL_TREE (cell_tree));

	cell_tree->show_expander = show_expander;
}
