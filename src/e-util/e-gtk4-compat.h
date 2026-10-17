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

/* Transitional helpers for the GTK 3 → GTK 4 port (DoxMail).
 *
 * Each one stands in for a GTK 3 call that GTK 4 removed and whose
 * replacement depends on the widget type (gtk_container_add() becomes
 * gtk_box_append(), gtk_window_set_child(), ...) or needs more than one
 * call.  They let the code base move to GTK 4 mechanically; code that is
 * touched for other reasons should use the GTK 4 API directly instead. */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_GTK4_COMPAT_H
#define E_GTK4_COMPAT_H

#include <gtk/gtk.h>
#include <e-util/e-gtk-dnd-compat.h>
#include <e-util/e-gtk-file-chooser-compat.h>

G_BEGIN_DECLS

/* GtkContainer */
typedef void (*EGtkCallback) (GtkWidget *widget, gpointer user_data);

void		e_gtk_container_add		(gpointer container,
						 GtkWidget *child);
void		e_gtk_container_remove		(gpointer container,
						 GtkWidget *child);
GList *		e_gtk_widget_get_children	(gpointer widget);
void		e_gtk_container_foreach		(gpointer container,
						 EGtkCallback callback,
						 gpointer user_data);
void		e_gtk_widget_set_border_width	(gpointer widget,
						 guint border_width);
GtkWidget *	e_gtk_bin_get_child		(gpointer bin);

/* GtkBox */
void		e_gtk_box_pack_start		(GtkBox *box,
						 GtkWidget *child,
						 gboolean expand,
						 gboolean fill,
						 guint padding);
void		e_gtk_box_pack_end		(GtkBox *box,
						 GtkWidget *child,
						 gboolean expand,
						 gboolean fill,
						 guint padding);
void		e_gtk_box_reorder_child		(GtkBox *box,
						 GtkWidget *child,
						 gint position);
GtkWidget *	e_gtk_button_box_new		(GtkOrientation orientation);

/* GtkPaned; these also take widgets made of a paned, like EPaned */
GtkPaned *	e_gtk_paned_get_inner		(gpointer paned);
void		e_gtk_paned_pack1		(gpointer paned,
						 GtkWidget *child,
						 gboolean resize,
						 gboolean shrink);
void		e_gtk_paned_pack2		(gpointer paned,
						 GtkWidget *child,
						 gboolean resize,
						 gboolean shrink);
GtkWidget *	e_gtk_paned_get_child1		(gpointer paned);
GtkWidget *	e_gtk_paned_get_child2		(gpointer paned);
gint		e_gtk_paned_get_position	(gpointer paned);
void		e_gtk_paned_set_position	(gpointer paned,
						 gint position);

/* Widgets and windows */
void		e_gtk_widget_destroy		(gpointer widget);
GtkWidget *	e_gtk_widget_get_toplevel	(gpointer widget);
gboolean	e_gtk_widget_is_toplevel	(gpointer widget);
void		e_gtk_widget_grab_default	(GtkWidget *widget);
void		e_gtk_widget_get_preferred_width
						(GtkWidget *widget,
						 gint *minimum,
						 gint *natural);
void		e_gtk_widget_get_preferred_height
						(GtkWidget *widget,
						 gint *minimum,
						 gint *natural);
void		e_gtk_widget_get_preferred_height_for_width
						(GtkWidget *widget,
						 gint width,
						 gint *minimum,
						 gint *natural);
GdkSurface *	e_gtk_widget_get_surface	(GtkWidget *widget);
void		e_gtk_widget_get_monitor_area	(GtkWidget *widget,
						 GdkRectangle *out_area);
void		e_gtk_button_set_image		(GtkButton *button,
						 GtkWidget *image);
void		e_gtk_button_set_label_and_icon	(GtkButton *button,
						 const gchar *label,
						 const gchar *icon_name);

/* Dialogs: GTK 4 dropped the nested main loop of gtk_dialog_run() */
gint		e_gtk_dialog_run		(GtkDialog *dialog);
gint		e_gtk_native_dialog_run		(GtkNativeDialog *dialog);
GtkWidget *	e_gtk_dialog_get_action_area	(GtkDialog *dialog);

/* Pixel size of the GTK 3 icon sizes, for code that drew icons by size */
gint		e_gtk_icon_size_to_pixels	(gint gtk3_icon_size);

/* The GTK 3 GtkIconSize values, for the places that sized icons by them */
#define E_GTK_ICON_SIZE_MENU		1
#define E_GTK_ICON_SIZE_SMALL_TOOLBAR	2
#define E_GTK_ICON_SIZE_LARGE_TOOLBAR	3
#define E_GTK_ICON_SIZE_BUTTON		4
#define E_GTK_ICON_SIZE_DND		5
#define E_GTK_ICON_SIZE_DIALOG		6

GtkIconTheme *	e_gtk_icon_theme_get_default	(void);

/* GTK 4 has no gtk_get_current_event(); windows that call
 * e_gtk_track_current_event() record the event being dispatched. */
void		e_gtk_track_current_event	(GtkWidget *window);
GdkEvent *	e_gtk_get_current_event		(void);
guint32		e_gtk_get_current_event_time	(void);
gboolean	e_gtk_get_current_event_state	(GdkModifierType *state);
GdkDevice *	e_gtk_get_current_event_device	(void);

/* GdkEvent accessors with the GTK 3 signatures */
gboolean	e_gdk_event_get_state		(GdkEvent *event,
						 GdkModifierType *state);
gboolean	e_gdk_event_get_keyval		(GdkEvent *event,
						 guint *keyval);
gboolean	e_gdk_event_get_button		(GdkEvent *event,
						 guint *button);
gboolean	e_gdk_event_get_coords		(GdkEvent *event,
						 gdouble *x,
						 gdouble *y);
gboolean	e_gdk_event_get_root_coords	(GdkEvent *event,
						 gdouble *x,
						 gdouble *y);

/* Popovers: GTK 4 parents them instead of "relative-to" */
GtkWidget *	e_gtk_popover_new		(GtkWidget *relative_to);
void		e_gtk_popover_set_relative_to	(GtkPopover *popover,
						 GtkWidget *relative_to);

/* the "destroy" handler that clears a widget pointer */
void		e_gtk_widget_destroyed		(GtkWidget *widget,
						 GtkWidget **widget_pointer);

/* Icons as pixbufs and cairo surfaces, which GTK 4 replaced with paintables */
GdkPixbuf *	e_gtk_icon_theme_load_icon	(GtkIconTheme *icon_theme,
						 const gchar *icon_name,
						 gint size,
						 gint flags,
						 GError **error);
GdkPixbuf *	e_gtk_icon_theme_load_gicon	(GtkIconTheme *icon_theme,
						 GIcon *icon,
						 gint size);
cairo_surface_t *
		e_gdk_cairo_surface_create_from_pixbuf
						(const GdkPixbuf *pixbuf,
						 gint scale,
						 gpointer for_window);
gboolean	e_gtk_icon_size_lookup		(gint size,
						 gint *width,
						 gint *height);
void		e_gtk_header_bar_set_title	(GtkHeaderBar *header_bar,
						 const gchar *title);

/* GTK 4 has no widget style properties; these keep the declared defaults */
void		e_gtk_widget_class_install_style_property
						(gpointer widget_class,
						 GParamSpec *pspec);
void		e_gtk_widget_style_get		(gpointer widget,
						 const gchar *first_property_name,
						 ...) G_GNUC_NULL_TERMINATED;

/* atk_object_set_name/description on a widget's accessible */
void		e_gtk_widget_set_accessible_name
						(GtkWidget *widget,
						 const gchar *name);
void		e_gtk_widget_set_accessible_description
						(GtkWidget *widget,
						 const gchar *description);

gboolean	e_gtk_show_uri_on_window	(GtkWindow *parent,
						 const gchar *uri,
						 guint32 timestamp,
						 GError **error);

/* GtkAccelGroup as a GtkShortcutController added to the window */
typedef GtkShortcutController EGtkAccelGroup;

EGtkAccelGroup *e_gtk_accel_group_new		(void);
void		e_gtk_widget_add_accelerator	(GtkWidget *widget,
						 const gchar *accel_signal,
						 EGtkAccelGroup *accel_group,
						 guint accel_key,
						 GdkModifierType accel_mods,
						 guint accel_flags);
void		e_gtk_window_add_accel_group	(GtkWindow *window,
						 EGtkAccelGroup *accel_group);
void		e_gtk_window_remove_accel_group	(GtkWindow *window,
						 EGtkAccelGroup *accel_group);

/* GtkToggleButton or GtkCheckButton (no longer a toggle button) */
gboolean	e_gtk_toggle_button_get_active	(gpointer button);
void		e_gtk_toggle_button_set_active	(gpointer button,
						 gboolean active);
gboolean	e_gtk_toggle_button_get_inconsistent
						(gpointer button);
void		e_gtk_toggle_button_set_inconsistent
						(gpointer button,
						 gboolean inconsistent);

/* GtkRadioButton as GtkCheckButton-s in a group; a "group" list holds one
 * member of the group, owned by that button */
GtkWidget *	e_gtk_radio_button_new_with_mnemonic
						(GSList *group,
						 const gchar *label);
GtkWidget *	e_gtk_radio_button_new_with_label
						(GSList *group,
						 const gchar *label);
GtkWidget *	e_gtk_radio_button_new_with_mnemonic_from_widget
						(gpointer member,
						 const gchar *label);
GtkWidget *	e_gtk_radio_button_new_with_label_from_widget
						(gpointer member,
						 const gchar *label);
GSList *	e_gtk_radio_button_get_group	(gpointer button);
void		e_gtk_radio_button_join_group	(gpointer button,
						 gpointer group_source);

/* GtkScrollable implemented by a wrapper and forwarded to the wrapped child;
 * the four properties take ids first_property_id .. first_property_id + 3 */
void		e_gtk_scrollable_override_properties
						(GObjectClass *object_class,
						 guint first_property_id);
gboolean	e_gtk_scrollable_forward_property
						(GtkWidget *inner,
						 guint first_property_id,
						 guint property_id,
						 GValue *value,
						 gboolean set,
						 GParamSpec *pspec);

G_END_DECLS

#endif /* E_GTK4_COMPAT_H */
