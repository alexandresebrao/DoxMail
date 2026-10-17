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

/* Events delivered to canvas items.
 *
 * GTK 3 lets the canvas copy, edit and synthesize GdkEvents (it makes up
 * enter/leave events for items and rewrites coordinates to world space).
 * GTK 4 made GdkEvent opaque and read-only, so the canvas carries its own
 * event type.  With GTK 3 it is plain GdkEvent; with GTK 4 it is a union
 * with the same member and field names, so item code written against
 * GnomeCanvasEvent and the GNOME_CANVAS_* names builds with both.
 */

#ifndef GNOME_CANVAS_EVENT_H
#define GNOME_CANVAS_EVENT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#if GTK_CHECK_VERSION (4, 0, 0)

typedef enum {
	GNOME_CANVAS_NOTHING = -1,
	GNOME_CANVAS_MOTION_NOTIFY,
	GNOME_CANVAS_BUTTON_PRESS,
	GNOME_CANVAS_2BUTTON_PRESS,
	GNOME_CANVAS_3BUTTON_PRESS,
	GNOME_CANVAS_BUTTON_RELEASE,
	GNOME_CANVAS_KEY_PRESS,
	GNOME_CANVAS_KEY_RELEASE,
	GNOME_CANVAS_ENTER_NOTIFY,
	GNOME_CANVAS_LEAVE_NOTIFY,
	GNOME_CANVAS_FOCUS_CHANGE,
	GNOME_CANVAS_SCROLL
} GnomeCanvasEventType;

typedef enum {
	GNOME_CANVAS_POINTER_MOTION_MASK = 1 << 2,
	GNOME_CANVAS_BUTTON_MOTION_MASK = 1 << 4,
	GNOME_CANVAS_BUTTON_PRESS_MASK = 1 << 8,
	GNOME_CANVAS_BUTTON_RELEASE_MASK = 1 << 9,
	GNOME_CANVAS_KEY_PRESS_MASK = 1 << 10,
	GNOME_CANVAS_KEY_RELEASE_MASK = 1 << 11,
	GNOME_CANVAS_ENTER_NOTIFY_MASK = 1 << 12,
	GNOME_CANVAS_LEAVE_NOTIFY_MASK = 1 << 13,
	GNOME_CANVAS_FOCUS_CHANGE_MASK = 1 << 14,
	GNOME_CANVAS_SCROLL_MASK = 1 << 21
} GnomeCanvasEventMask;

typedef enum {
	GNOME_CANVAS_GRAB_SUCCESS,
	GNOME_CANVAS_GRAB_ALREADY_GRABBED,
	GNOME_CANVAS_GRAB_INVALID_TIME,
	GNOME_CANVAS_GRAB_NOT_VIEWABLE,
	GNOME_CANVAS_GRAB_FROZEN,
	GNOME_CANVAS_GRAB_FAILED
} GnomeCanvasGrabStatus;

/* Every member starts with the same fields, so event->motion.x and
 * event->button.x are the same value.  Coordinates are in the canvas
 * window (scrolled) space when the canvas receives the event and in world
 * space when an item does; x_root/y_root are surface coordinates, since
 * GTK 4 has no global pointer position.  gdk_event is the GTK event this
 * one came from, NULL for events the canvas made up. */
#define GNOME_CANVAS_EVENT_FIELDS \
	GnomeCanvasEventType type; \
	GdkEvent *gdk_event; \
	gint8 send_event; \
	guint32 time; \
	GdkModifierType state; \
	GdkDevice *device; \
	gdouble x; \
	gdouble y; \
	gdouble x_root; \
	gdouble y_root

typedef struct {
	GNOME_CANVAS_EVENT_FIELDS;
} GnomeCanvasEventAny, GnomeCanvasEventMotion;

typedef struct {
	GNOME_CANVAS_EVENT_FIELDS;
	guint button;
} GnomeCanvasEventButton;

typedef struct {
	GNOME_CANVAS_EVENT_FIELDS;
	guint keyval;
	guint16 hardware_keycode;
	guint8 group;
	guint is_modifier : 1;
	/* the text of the key, as GTK 3 gave it (deprecated there) */
	gint length;
	gchar string[8];
} GnomeCanvasEventKey;

typedef struct {
	GNOME_CANVAS_EVENT_FIELDS;
	GdkCrossingMode mode;
	GdkNotifyType detail;
	gboolean focus;
} GnomeCanvasEventCrossing;

typedef struct {
	GNOME_CANVAS_EVENT_FIELDS;
	gint16 in;
} GnomeCanvasEventFocus;

typedef struct {
	GNOME_CANVAS_EVENT_FIELDS;
	GdkScrollDirection direction;
	gdouble delta_x;
	gdouble delta_y;
} GnomeCanvasEventScroll;

typedef union {
	GnomeCanvasEventType type;
	GnomeCanvasEventAny any;
	GnomeCanvasEventMotion motion;
	GnomeCanvasEventButton button;
	GnomeCanvasEventKey key;
	GnomeCanvasEventCrossing crossing;
	GnomeCanvasEventFocus focus_change;
	GnomeCanvasEventScroll scroll;
} GnomeCanvasEvent;

#define GNOME_TYPE_CANVAS_EVENT (gnome_canvas_event_get_type ())

GType		gnome_canvas_event_get_type	(void) G_GNUC_CONST;
GnomeCanvasEvent *
		gnome_canvas_event_copy		(const GnomeCanvasEvent *event);
void		gnome_canvas_event_free		(GnomeCanvasEvent *event);
void		gnome_canvas_event_set_from_gdk	(GnomeCanvasEvent *event,
						 GnomeCanvasEventType type,
						 GdkEvent *gdk_event,
						 gdouble x,
						 gdouble y);
guint32		gnome_canvas_event_get_time	(const GnomeCanvasEvent *event);
GdkDevice *	gnome_canvas_event_get_device	(const GnomeCanvasEvent *event);
gboolean	gnome_canvas_event_get_button	(const GnomeCanvasEvent *event,
						 guint *button);
gboolean	gnome_canvas_event_get_coords	(const GnomeCanvasEvent *event,
						 gdouble *x,
						 gdouble *y);
gboolean	gnome_canvas_event_get_root_coords
						(const GnomeCanvasEvent *event,
						 gdouble *x_root,
						 gdouble *y_root);
gboolean	gnome_canvas_event_get_keyval	(const GnomeCanvasEvent *event,
						 guint *keyval);
gboolean	gnome_canvas_event_get_state	(const GnomeCanvasEvent *event,
						 GdkModifierType *state);
GdkEvent *	gnome_canvas_event_get_gdk_event
						(const GnomeCanvasEvent *event);

#else /* GTK 3 */

typedef GdkEvent GnomeCanvasEvent;
typedef GdkEventType GnomeCanvasEventType;
typedef GdkEventMask GnomeCanvasEventMask;
typedef GdkGrabStatus GnomeCanvasGrabStatus;

#define GNOME_TYPE_CANVAS_EVENT GDK_TYPE_EVENT

#define GNOME_CANVAS_NOTHING		GDK_NOTHING
#define GNOME_CANVAS_MOTION_NOTIFY	GDK_MOTION_NOTIFY
#define GNOME_CANVAS_BUTTON_PRESS	GDK_BUTTON_PRESS
#define GNOME_CANVAS_2BUTTON_PRESS	GDK_2BUTTON_PRESS
#define GNOME_CANVAS_3BUTTON_PRESS	GDK_3BUTTON_PRESS
#define GNOME_CANVAS_BUTTON_RELEASE	GDK_BUTTON_RELEASE
#define GNOME_CANVAS_KEY_PRESS		GDK_KEY_PRESS
#define GNOME_CANVAS_KEY_RELEASE	GDK_KEY_RELEASE
#define GNOME_CANVAS_ENTER_NOTIFY	GDK_ENTER_NOTIFY
#define GNOME_CANVAS_LEAVE_NOTIFY	GDK_LEAVE_NOTIFY
#define GNOME_CANVAS_FOCUS_CHANGE	GDK_FOCUS_CHANGE
#define GNOME_CANVAS_SCROLL		GDK_SCROLL

#define GNOME_CANVAS_POINTER_MOTION_MASK	GDK_POINTER_MOTION_MASK
#define GNOME_CANVAS_BUTTON_MOTION_MASK		GDK_BUTTON_MOTION_MASK
#define GNOME_CANVAS_BUTTON_PRESS_MASK		GDK_BUTTON_PRESS_MASK
#define GNOME_CANVAS_BUTTON_RELEASE_MASK	GDK_BUTTON_RELEASE_MASK
#define GNOME_CANVAS_KEY_PRESS_MASK		GDK_KEY_PRESS_MASK
#define GNOME_CANVAS_KEY_RELEASE_MASK		GDK_KEY_RELEASE_MASK
#define GNOME_CANVAS_ENTER_NOTIFY_MASK		GDK_ENTER_NOTIFY_MASK
#define GNOME_CANVAS_LEAVE_NOTIFY_MASK		GDK_LEAVE_NOTIFY_MASK
#define GNOME_CANVAS_FOCUS_CHANGE_MASK		GDK_FOCUS_CHANGE_MASK
#define GNOME_CANVAS_SCROLL_MASK		GDK_SCROLL_MASK

#define GNOME_CANVAS_GRAB_SUCCESS		GDK_GRAB_SUCCESS
#define GNOME_CANVAS_GRAB_ALREADY_GRABBED	GDK_GRAB_ALREADY_GRABBED
#define GNOME_CANVAS_GRAB_INVALID_TIME		GDK_GRAB_INVALID_TIME
#define GNOME_CANVAS_GRAB_NOT_VIEWABLE		GDK_GRAB_NOT_VIEWABLE
#define GNOME_CANVAS_GRAB_FROZEN		GDK_GRAB_FROZEN
#define GNOME_CANVAS_GRAB_FAILED		GDK_GRAB_FAILED

#define gnome_canvas_event_copy(ev)		gdk_event_copy (ev)
#define gnome_canvas_event_free(ev)		gdk_event_free (ev)
#define gnome_canvas_event_get_time(ev)		gdk_event_get_time (ev)
#define gnome_canvas_event_get_device(ev)	gdk_event_get_device (ev)
#define gnome_canvas_event_get_button(ev, b)	gdk_event_get_button ((ev), (b))
#define gnome_canvas_event_get_coords(ev, x, y)	gdk_event_get_coords ((ev), (x), (y))
#define gnome_canvas_event_get_root_coords(ev, x, y) \
	gdk_event_get_root_coords ((ev), (x), (y))
#define gnome_canvas_event_get_keyval(ev, k)	gdk_event_get_keyval ((ev), (k))
#define gnome_canvas_event_get_state(ev, s)	gdk_event_get_state ((ev), (s))
#define gnome_canvas_event_get_gdk_event(ev)	((GdkEvent *) (ev))

#endif

G_END_DECLS

#endif /* GNOME_CANVAS_EVENT_H */
