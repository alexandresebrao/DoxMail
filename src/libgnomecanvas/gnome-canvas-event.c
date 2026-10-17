/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either the version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

/* GTK 4 implementation of GnomeCanvasEvent; with GTK 3 the type is GdkEvent
 * and everything here is a macro in gnome-canvas-event.h. */

#include "evolution-config.h"

#include <string.h>

#include "gnome-canvas-event.h"

#if GTK_CHECK_VERSION (4, 0, 0)

G_DEFINE_BOXED_TYPE (
	GnomeCanvasEvent,
	gnome_canvas_event,
	gnome_canvas_event_copy,
	gnome_canvas_event_free)

/* A copy owns references to its GdkEvent and device; events built on the
 * stack by the canvas only borrow them. */
GnomeCanvasEvent *
gnome_canvas_event_copy (const GnomeCanvasEvent *event)
{
	GnomeCanvasEvent *copy;

	g_return_val_if_fail (event != NULL, NULL);

	copy = g_new (GnomeCanvasEvent, 1);
	*copy = *event;

	if (copy->any.gdk_event)
		gdk_event_ref (copy->any.gdk_event);
	if (copy->any.device)
		g_object_ref (copy->any.device);

	return copy;
}

void
gnome_canvas_event_free (GnomeCanvasEvent *event)
{
	if (!event)
		return;

	if (event->any.gdk_event)
		gdk_event_unref (event->any.gdk_event);
	g_clear_object (&event->any.device);
	g_free (event);
}

/**
 * gnome_canvas_event_set_from_gdk:
 * @event: event to fill
 * @type: canvas event type
 * @gdk_event: (nullable): the GTK event it comes from
 * @x: X coordinate, already translated to the canvas window
 * @y: Y coordinate, already translated to the canvas window
 *
 * Fills the fields common to all event types; the caller sets the
 * type-specific ones.  No references are taken.
 **/
void
gnome_canvas_event_set_from_gdk (GnomeCanvasEvent *event,
                                 GnomeCanvasEventType type,
                                 GdkEvent *gdk_event,
                                 gdouble x,
                                 gdouble y)
{
	g_return_if_fail (event != NULL);

	memset (event, 0, sizeof (GnomeCanvasEvent));

	event->any.type = type;
	event->any.gdk_event = gdk_event;
	event->any.x = x;
	event->any.y = y;

	if (gdk_event) {
		event->any.time = gdk_event_get_time (gdk_event);
		event->any.state = gdk_event_get_modifier_state (gdk_event);
		event->any.device = gdk_event_get_device (gdk_event);
		if (!gdk_event_get_position (gdk_event, &event->any.x_root, &event->any.y_root)) {
			event->any.x_root = x;
			event->any.y_root = y;
		}
	} else {
		event->any.time = GDK_CURRENT_TIME;
		event->any.x_root = x;
		event->any.y_root = y;
	}
}

guint32
gnome_canvas_event_get_time (const GnomeCanvasEvent *event)
{
	return event ? event->any.time : GDK_CURRENT_TIME;
}

GdkDevice *
gnome_canvas_event_get_device (const GnomeCanvasEvent *event)
{
	g_return_val_if_fail (event != NULL, NULL);

	return event->any.device;
}

static gboolean
has_coords (const GnomeCanvasEvent *event)
{
	switch (event->type) {
	case GNOME_CANVAS_MOTION_NOTIFY:
	case GNOME_CANVAS_BUTTON_PRESS:
	case GNOME_CANVAS_2BUTTON_PRESS:
	case GNOME_CANVAS_3BUTTON_PRESS:
	case GNOME_CANVAS_BUTTON_RELEASE:
	case GNOME_CANVAS_ENTER_NOTIFY:
	case GNOME_CANVAS_LEAVE_NOTIFY:
	case GNOME_CANVAS_SCROLL:
		return TRUE;
	default:
		return FALSE;
	}
}

gboolean
gnome_canvas_event_get_button (const GnomeCanvasEvent *event,
                               guint *button)
{
	g_return_val_if_fail (event != NULL, FALSE);

	switch (event->type) {
	case GNOME_CANVAS_BUTTON_PRESS:
	case GNOME_CANVAS_2BUTTON_PRESS:
	case GNOME_CANVAS_3BUTTON_PRESS:
	case GNOME_CANVAS_BUTTON_RELEASE:
		if (button)
			*button = event->button.button;
		return TRUE;
	default:
		if (button)
			*button = 0;
		return FALSE;
	}
}

gboolean
gnome_canvas_event_get_coords (const GnomeCanvasEvent *event,
                               gdouble *x,
                               gdouble *y)
{
	gboolean ok;

	g_return_val_if_fail (event != NULL, FALSE);

	ok = has_coords (event);

	if (x)
		*x = ok ? event->any.x : 0;
	if (y)
		*y = ok ? event->any.y : 0;

	return ok;
}

gboolean
gnome_canvas_event_get_root_coords (const GnomeCanvasEvent *event,
                                    gdouble *x_root,
                                    gdouble *y_root)
{
	gboolean ok;

	g_return_val_if_fail (event != NULL, FALSE);

	ok = has_coords (event);

	if (x_root)
		*x_root = ok ? event->any.x_root : 0;
	if (y_root)
		*y_root = ok ? event->any.y_root : 0;

	return ok;
}

gboolean
gnome_canvas_event_get_keyval (const GnomeCanvasEvent *event,
                               guint *keyval)
{
	gboolean ok;

	g_return_val_if_fail (event != NULL, FALSE);

	ok = event->type == GNOME_CANVAS_KEY_PRESS ||
	     event->type == GNOME_CANVAS_KEY_RELEASE;

	if (keyval)
		*keyval = ok ? event->key.keyval : 0;

	return ok;
}

gboolean
gnome_canvas_event_get_state (const GnomeCanvasEvent *event,
                              GdkModifierType *state)
{
	gboolean ok;

	g_return_val_if_fail (event != NULL, FALSE);

	ok = event->type != GNOME_CANVAS_FOCUS_CHANGE &&
	     event->type != GNOME_CANVAS_NOTHING;

	if (state)
		*state = ok ? event->any.state : 0;

	return ok;
}

/* For GTK calls that insist on a real event (GtkIMContext, popovers).
 * NULL when the canvas made the event up. */
GdkEvent *
gnome_canvas_event_get_gdk_event (const GnomeCanvasEvent *event)
{
	g_return_val_if_fail (event != NULL, NULL);

	return event->any.gdk_event;
}

#endif /* GTK 4 */
