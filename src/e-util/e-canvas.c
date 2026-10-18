/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include "e-canvas.h"
#include "e-misc-utils.h"

#define d(x)

enum {
	REFLOW,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	ECanvas,
	e_canvas,
	GNOME_TYPE_CANVAS)

/* Emits an event for an item in the canvas, be it the current
 * item, grabbed item, or focused item, as appropriate.  Unlike the
 * GnomeCanvas, events go to the grabbed item even when the current
 * item is outside of it. */
static gint
canvas_emit_event (GnomeCanvas *canvas,
                   GnomeCanvasEvent *event)
{
	GnomeCanvasEvent *ev;
	gint finished;
	GnomeCanvasItem *item;
	GnomeCanvasItem *parent;
	guint mask;

	/* Choose where we send the event */

	item = canvas->current_item;

	if (canvas->focused_item &&
		((event->type == GNOME_CANVAS_KEY_PRESS) ||
		 (event->type == GNOME_CANVAS_KEY_RELEASE) ||
		 (event->type == GNOME_CANVAS_FOCUS_CHANGE)))
		item = canvas->focused_item;

	if (canvas->grabbed_item)
		item = canvas->grabbed_item;

	/* Perform checks for grabbed items */

	if (canvas->grabbed_item) {
		switch (event->type) {
			case GNOME_CANVAS_ENTER_NOTIFY:
				mask = GNOME_CANVAS_ENTER_NOTIFY_MASK;
				break;

			case GNOME_CANVAS_LEAVE_NOTIFY:
				mask = GNOME_CANVAS_LEAVE_NOTIFY_MASK;
				break;

			case GNOME_CANVAS_MOTION_NOTIFY:
				mask = GNOME_CANVAS_POINTER_MOTION_MASK;
				break;

			case GNOME_CANVAS_BUTTON_PRESS:
			case GNOME_CANVAS_2BUTTON_PRESS:
			case GNOME_CANVAS_3BUTTON_PRESS:
				mask = GNOME_CANVAS_BUTTON_PRESS_MASK;
				break;

			case GNOME_CANVAS_BUTTON_RELEASE:
				mask = GNOME_CANVAS_BUTTON_RELEASE_MASK;
				break;

			case GNOME_CANVAS_KEY_PRESS:
				mask = GNOME_CANVAS_KEY_PRESS_MASK;
				break;

			case GNOME_CANVAS_KEY_RELEASE:
				mask = GNOME_CANVAS_KEY_RELEASE_MASK;
				break;

			default:
				mask = 0;
				break;
		}

		if (!(mask & canvas->grabbed_event_mask))
			return FALSE;
	}

	/* Convert to world coordinates */

	ev = gnome_canvas_event_copy (event);

	switch (ev->type) {
		case GNOME_CANVAS_ENTER_NOTIFY:
		case GNOME_CANVAS_LEAVE_NOTIFY:
		case GNOME_CANVAS_MOTION_NOTIFY:
		case GNOME_CANVAS_BUTTON_PRESS:
		case GNOME_CANVAS_2BUTTON_PRESS:
		case GNOME_CANVAS_3BUTTON_PRESS:
		case GNOME_CANVAS_BUTTON_RELEASE:
			gnome_canvas_window_to_world (
				canvas,
				ev->any.x, ev->any.y,
				&ev->any.x, &ev->any.y);
			break;

		default:
			break;
	}

	/* The event is propagated up the hierarchy (for if someone connected
	 * to a group instead of a leaf event), and emission is stopped if a
	 * handler returns TRUE, just like for GtkWidget events. */

	finished = FALSE;

	while (item && !finished) {
		g_object_ref (item);

		g_signal_emit_by_name (item, "event", ev, &finished);

		parent = item->parent;
		g_object_unref (item);

		item = parent;
	}

	gnome_canvas_event_free (ev);

	return finished;
}

static void
canvas_style_updated_recursive (GnomeCanvasItem *item)
{
	guint signal_id = g_signal_lookup ("style_updated", G_OBJECT_TYPE (item));
	if (signal_id >= 1) {
		GSignalQuery query;
		g_signal_query (signal_id, &query);
		if (query.return_type == G_TYPE_NONE &&
			query.n_params == 0) {
			g_signal_emit (item, signal_id, 0);
		}
	}

	if (GNOME_IS_CANVAS_GROUP (item)) {
		GList *items = GNOME_CANVAS_GROUP (item)->item_list;
		for (; items; items = items->next)
			canvas_style_updated_recursive (items->data);
	}
}

static void
canvas_dispose (GObject *object)
{
	ECanvas *canvas = E_CANVAS (object);

	if (canvas->idle_id)
		g_source_remove (canvas->idle_id);
	canvas->idle_id = 0;

	if (canvas->grab_cancelled_check_id)
		g_source_remove (canvas->grab_cancelled_check_id);
	canvas->grab_cancelled_check_id = 0;

	if (canvas->toplevel) {
		if (canvas->visibility_notify_id)
			g_signal_handler_disconnect (
				canvas->toplevel,
				canvas->visibility_notify_id);
		canvas->visibility_notify_id = 0;

		g_object_unref (canvas->toplevel);
		canvas->toplevel = NULL;
	}

	g_clear_object (&canvas->im_context);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_canvas_parent_class)->dispose (object);
}

static void
canvas_realize (GtkWidget *widget)
{
	ECanvas *ecanvas = E_CANVAS (widget);

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_canvas_parent_class)->realize (widget);

	gtk_im_context_set_client_widget (ecanvas->im_context, widget);
}

static void
canvas_unrealize (GtkWidget *widget)
{
	ECanvas * ecanvas = E_CANVAS (widget);

	if (ecanvas->idle_id) {
		g_source_remove (ecanvas->idle_id);
		ecanvas->idle_id = 0;
	}

	gtk_im_context_set_client_widget (ecanvas->im_context, NULL);

	/* Chain up to parent's unrealize() method. */
	GTK_WIDGET_CLASS (e_canvas_parent_class)->unrealize (widget);
}

static void
canvas_css_changed (GtkWidget *widget,
		    GtkCssStyleChange *change)
{
	GTK_WIDGET_CLASS (e_canvas_parent_class)->css_changed (widget, change);

	canvas_style_updated_recursive (
		GNOME_CANVAS_ITEM (gnome_canvas_root (
		GNOME_CANVAS (widget))));
}

/* The GnomeCanvas sends the focus change to the focused item */
static void
canvas_focus_enter_cb (GtkEventControllerFocus *controller,
		       ECanvas *ecanvas)
{
	gtk_im_context_focus_in (ecanvas->im_context);
}

static void
canvas_focus_leave_cb (GtkEventControllerFocus *controller,
		       ECanvas *ecanvas)
{
	gtk_im_context_focus_out (ecanvas->im_context);
}

static void
canvas_reflow (ECanvas *canvas)
{
	/* Placeholder so subclasses can safely chain up. */
}

static void
e_canvas_class_init (ECanvasClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = canvas_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = canvas_realize;
	widget_class->unrealize = canvas_unrealize;
	widget_class->css_changed = canvas_css_changed;

	GNOME_CANVAS_CLASS (class)->emit_event = canvas_emit_event;

	class->reflow = canvas_reflow;

	signals[REFLOW] = g_signal_new (
		"reflow",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECanvasClass, reflow),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_canvas_init (ECanvas *canvas)
{
	GtkEventController *controller;

	canvas->im_context = gtk_im_multicontext_new ();

	controller = gtk_event_controller_focus_new ();
	g_signal_connect (controller, "enter", G_CALLBACK (canvas_focus_enter_cb), canvas);
	g_signal_connect (controller, "leave", G_CALLBACK (canvas_focus_leave_cb), canvas);
	gtk_widget_add_controller (GTK_WIDGET (canvas), controller);
}

GtkWidget *
e_canvas_new (void)
{
	return g_object_new (E_TYPE_CANVAS, NULL);
}

/**
 * e_canvas_im_filter_keypress:
 * @im_context: a #GtkIMContext
 * @event: a key event received by a canvas item
 *
 * Passes the key event to the input method. Events the canvas made up
 * have no #GdkEvent and are not filtered.
 *
 * Returns: whether the input method handled the event
 *
 * Since: DoxMail
 **/
gboolean
e_canvas_im_filter_keypress (GtkIMContext *im_context,
			     GnomeCanvasEvent *event)
{
	GdkEvent *gdk_event;

	g_return_val_if_fail (GTK_IS_IM_CONTEXT (im_context), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	gdk_event = gnome_canvas_event_get_gdk_event (event);

	return gdk_event && gtk_im_context_filter_keypress (im_context, gdk_event);
}

/**
 * e_canvas_popup_menu:
 * @menu: a #GtkPopover, usually a #GtkPopoverMenu, without a parent
 * @canvas: the #GnomeCanvas to show the @menu for
 * @event: (nullable): the canvas item event which triggered the menu
 *
 * Shows @menu with e_util_popup_menu(), pointing at the position of the
 * @event, which is in world coordinates, as items receive it. Key events,
 * or no event, point at the whole @canvas.
 *
 * Since: DoxMail
 **/
void
e_canvas_popup_menu (GtkWidget *menu,
		     GnomeCanvas *canvas,
		     GnomeCanvasEvent *event)
{
	gdouble x = -1, y = -1;

	g_return_if_fail (GNOME_IS_CANVAS (canvas));

	if (event && event->type != GNOME_CANVAS_KEY_PRESS && event->type != GNOME_CANVAS_KEY_RELEASE &&
	    event->type != GNOME_CANVAS_FOCUS_CHANGE) {
		gnome_canvas_world_to_window (canvas, event->any.x, event->any.y, &x, &y);

		/* window coordinates include the scroll offset */
		x -= gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas)));
		y -= gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas)));
	}

	e_util_popup_menu (menu, GTK_WIDGET (canvas), x, y);
}

/**
 * e_canvas_item_grab_focus:
 * @item: A canvas item.
 * @widget_too: Whether or not to grab the widget-level focus too
 *
 * Makes the specified item take the keyboard focus, so all keyboard
 * events will be sent to it. If the canvas widget itself did not have
 * the focus and @widget_too is %TRUE, it grabs that focus as well.
 **/
void
e_canvas_item_grab_focus (GnomeCanvasItem *item,
                          gboolean widget_too)
{
	GnomeCanvasItem *focused_item;
	GnomeCanvasEvent ev;

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (gtk_widget_get_focusable (GTK_WIDGET (item->canvas)));

	focused_item = item->canvas->focused_item;

	if (focused_item) {
		gnome_canvas_event_set_from_gdk (&ev, GNOME_CANVAS_FOCUS_CHANGE, NULL, 0, 0);
		ev.focus_change.in = FALSE;

		canvas_emit_event (item->canvas, &ev);
	}

	item->canvas->focused_item = item;

	if (widget_too && !e_gtk_widget_has_focus (GTK_WIDGET (item->canvas))) {
		gtk_widget_grab_focus (GTK_WIDGET (item->canvas));
	}

	gnome_canvas_event_set_from_gdk (&ev, GNOME_CANVAS_FOCUS_CHANGE, NULL, 0, 0);
	ev.focus_change.in = TRUE;

	canvas_emit_event (item->canvas, &ev);
}

static void
e_canvas_item_invoke_reflow (GnomeCanvasItem *item,
                             gint flags)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *child;

	if (GNOME_IS_CANVAS_GROUP (item)) {
		group = GNOME_CANVAS_GROUP (item);
		for (list = group->item_list; list; list = list->next) {
			child = GNOME_CANVAS_ITEM (list->data);
			if (child->flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
				e_canvas_item_invoke_reflow (child, flags);
		}
	}

	if (item->flags & E_CANVAS_ITEM_NEEDS_REFLOW) {
		ECanvasItemReflowFunc func;
		func = (ECanvasItemReflowFunc)
			g_object_get_data (
				G_OBJECT (item),
				"ECanvasItem::reflow_callback");
		if (func)
			func (item, flags);
	}

	item->flags &= ~E_CANVAS_ITEM_NEEDS_REFLOW;
	item->flags &= ~E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW;
}

static void
do_reflow (ECanvas *canvas)
{
	if (GNOME_CANVAS (canvas)->root->flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
		e_canvas_item_invoke_reflow (GNOME_CANVAS (canvas)->root, 0);
}

/* Idle handler for the e-canvas.  It deals with pending reflows. */
static gint
idle_handler (gpointer data)
{
	ECanvas *canvas;

	canvas = E_CANVAS (data);
	do_reflow (canvas);

	/* Reset idle id */
	canvas->idle_id = 0;

	g_signal_emit (canvas, signals[REFLOW], 0);

	return FALSE;
}

/* Convenience function to add an idle handler to a canvas */
static void
add_idle (ECanvas *canvas)
{
	if (canvas->idle_id != 0)
		return;

	canvas->idle_id = g_idle_add_full (
		G_PRIORITY_HIGH_IDLE, idle_handler, (gpointer) canvas, NULL);
}

static void
e_canvas_item_descendent_needs_reflow (GnomeCanvasItem *item)
{
	if (item->flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
		return;

	item->flags |= E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW;
	if (item->parent)
		e_canvas_item_descendent_needs_reflow (item->parent);
}

void
e_canvas_item_request_reflow (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (item->flags & GNOME_CANVAS_ITEM_REALIZED) {
		item->flags |= E_CANVAS_ITEM_NEEDS_REFLOW;
		e_canvas_item_descendent_needs_reflow (item);
		add_idle (E_CANVAS (item->canvas));
	}
}

void
e_canvas_item_request_parent_reflow (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	e_canvas_item_request_reflow (item->parent);
}

void
e_canvas_item_set_reflow_callback (GnomeCanvasItem *item,
                                   ECanvasItemReflowFunc func)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (func != NULL);

	g_object_set_data (
		G_OBJECT (item), "ECanvasItem::reflow_callback",
		(gpointer) func);
}

static gboolean
grab_cancelled_check (gpointer data)
{
	ECanvas *canvas = data;

	if (GNOME_CANVAS (canvas)->grabbed_item == NULL) {
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		return FALSE;
	}

	/* FIXME-GTK4: GTK 3 checked for a GTK grab taking over (a popup
	 * or a modal dialog); GTK 4 has no way to ask, so the item keeps
	 * its grab until it ungrabs */
	if (FALSE) {
		gnome_canvas_item_ungrab (
			GNOME_CANVAS (canvas)->grabbed_item,
			canvas->grab_cancelled_time);
		if (canvas->grab_cancelled_cb)
			canvas->grab_cancelled_cb (
				canvas, GNOME_CANVAS (canvas)->grabbed_item,
				canvas->grab_cancelled_data);
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		return FALSE;
	}
	return TRUE;
}

gint
e_canvas_item_grab (ECanvas *canvas,
                    GnomeCanvasItem *item,
                    guint event_mask,
                    GdkCursor *cursor,
                    GdkDevice *device,
                    guint32 etime,
                    ECanvasItemGrabCancelled cancelled_cb,
                    gpointer cancelled_data)
{
	GnomeCanvasGrabStatus grab_status;

	g_return_val_if_fail (E_IS_CANVAS (canvas), -1);
	g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (item), -1);
	g_return_val_if_fail (GDK_IS_DEVICE (device), -1);

	grab_status = gnome_canvas_item_grab (
		item, event_mask, cursor, device, etime);
	if (grab_status == GNOME_CANVAS_GRAB_SUCCESS) {
		canvas->grab_cancelled_cb = cancelled_cb;
		canvas->grab_cancelled_check_id = e_named_timeout_add_full (
			G_PRIORITY_LOW, 100,
			grab_cancelled_check, canvas, NULL);
		canvas->grab_cancelled_time = etime;
		canvas->grab_cancelled_data = cancelled_data;
	}

	return grab_status;
}

void
e_canvas_item_ungrab (ECanvas *canvas,
                      GnomeCanvasItem *item,
                      guint32 etime)
{
	g_return_if_fail (E_IS_CANVAS (canvas));
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (canvas->grab_cancelled_check_id) {
		g_source_remove (canvas->grab_cancelled_check_id);
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		gnome_canvas_item_ungrab (item, etime);
	}
}
