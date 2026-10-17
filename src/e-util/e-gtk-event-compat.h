/*
 * SPDX-FileCopyrightText: (C) 2026 DoxMail contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_GTK_EVENT_COMPAT_H
#define E_GTK_EVENT_COMPAT_H

#include <gtk/gtk.h>
#include <libgnomecanvas/gnome-canvas-event.h>

G_BEGIN_DECLS

/* The GTK 3 widget event signals, emulated on GTK 4.
 *
 * GtkWidget gets "event", "key-press-event", "key-release-event",
 * "button-press-event", "button-release-event", "motion-notify-event",
 * "scroll-event", "enter-notify-event", "leave-notify-event",
 * "focus-in-event", "focus-out-event" (all gboolean (GtkWidget *,
 * GdkEvent *), stopping at the first TRUE) and "popup-menu" (gboolean
 * (GtkWidget *)), registered when the library loads.  They are emitted
 * only for widgets passed through e_gtk_widget_legacy_events(), which
 * returns its argument so it can wrap the instance of a g_signal_connect().
 *
 * While a handler runs, the e_gdk_event_*() value getters below give the
 * coordinates relative to the emitting widget, as GTK 3 did, and a second
 * or third click within the double-click time also comes as
 * E_GDK_2BUTTON_PRESS or E_GDK_3BUTTON_PRESS after its GDK_BUTTON_PRESS. */

#define E_GDK_2BUTTON_PRESS ((GdkEventType) 0x1002)
#define E_GDK_3BUTTON_PRESS ((GdkEventType) 0x1003)

gpointer	e_gtk_widget_legacy_events	(gpointer widget);

/* "delete-event" (gboolean (GtkWidget *, GdkEvent *), the event is NULL)
 * of GtkWindow, emitted from "close-request" of windows passed through
 * this; TRUE keeps the window open, as with GTK 3 */
gpointer	e_gtk_window_delete_events	(gpointer window);

/* GTK 4 lets the layout manager of widgets that have one (GtkBox, GtkGrid,
 * GtkFixed, GtkButton, ...) measure and allocate them, without calling
 * the measure and size_allocate methods of their classes; call this in
 * the class_init of classes that override them to have them called, with
 * chain-ups that reach the layout manager */
void		e_gtk_widget_class_layout_methods
						(GtkWidgetClass *widget_class);

/* "size-allocate" (void (GtkWidget *, GtkAllocation *)) is emitted after
 * widgets passed through this got allocated, with the allocation in their
 * own coordinates, right after the layout (the handlers may resize
 * widgets, which GTK 4 does not allow while allocating); the size_allocate
 * method of their class, or their layout manager, is wrapped */
gpointer	e_gtk_widget_size_allocations	(gpointer widget);

/* "style-updated" (void (GtkWidget *)) is emitted for widgets passed
 * through this when the theme, the dark preference or the font change */
gpointer	e_gtk_widget_style_updates	(gpointer widget);

/* GtkWidgetClass key_press_event and the other event vfuncs: the handler
 * overrides the class closure of the legacy signal for the class, whose
 * widgets then get the legacy events once realized; a chain-up becomes
 * e_gtk_widget_chain_legacy_event() */
void		e_gtk_widget_class_override_legacy_event
						(GtkWidgetClass *widget_class,
						 const gchar *signal_name,
						 GCallback handler);
gboolean	e_gtk_widget_chain_legacy_event	(GtkWidget *widget,
						 GdkEvent *event);
gboolean	e_gtk_widget_chain_legacy_popup_menu
						(GtkWidget *widget);

GdkEventType	e_gdk_event_type		(GdkEvent *event);
guint		e_gdk_event_keyval		(GdkEvent *event);
guint16		e_gdk_event_keycode		(GdkEvent *event);
gboolean	e_gdk_event_is_modifier		(GdkEvent *event);
GdkModifierType	e_gdk_event_state		(GdkEvent *event);
guint		e_gdk_event_button		(GdkEvent *event);
gdouble		e_gdk_event_x			(GdkEvent *event);
gdouble		e_gdk_event_y			(GdkEvent *event);
gdouble		e_gdk_event_x_root		(GdkEvent *event);
gdouble		e_gdk_event_y_root		(GdkEvent *event);
guint32		e_gdk_event_time		(GdkEvent *event);
GdkScrollDirection
		e_gdk_event_direction		(GdkEvent *event);
gdouble		e_gdk_event_delta_x		(GdkEvent *event);
gdouble		e_gdk_event_delta_y		(GdkEvent *event);
gboolean	e_gdk_event_focus_in		(GdkEvent *event);

/* the widget a legacy signal for @event is emitted on now, or NULL */
GtkWidget *	e_gtk_event_get_legacy_widget	(GdkEvent *event);

/* translates surface coordinates of @event into @widget ones */
gboolean	e_gdk_event_get_widget_coords	(GdkEvent *event,
						 GtkWidget *widget,
						 gdouble *x,
						 gdouble *y);

/* fills @cevent from @event, given by a legacy signal of @canvas (or of
 * another widget, when @canvas is NULL), with the canvas event type and
 * positions in canvas window coordinates, as GTK 3 gave them on the bin
 * window; for code shared by canvas item and canvas widget handlers.
 * Returns @cevent. */
GnomeCanvasEvent *
		e_gdk_event_to_canvas_event	(GdkEvent *event,
						 GtkWidget *canvas,
						 GnomeCanvasEvent *cevent);

G_END_DECLS

#endif /* E_GTK_EVENT_COMPAT_H */
