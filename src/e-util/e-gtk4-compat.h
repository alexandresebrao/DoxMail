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
#include <e-util/e-gtk-event-compat.h>
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
/* a secondary child goes first in the box */
void		e_gtk_button_box_set_child_secondary
						(gpointer box,
						 GtkWidget *child,
						 gboolean is_secondary);

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
gboolean	e_gtk_widget_has_focus		(gpointer widget);
gboolean	e_gtk_widget_is_focus		(gpointer widget);
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
GtkWidget *	e_gtk_button_get_image		(GtkButton *button);
void		e_gtk_button_set_label_and_icon	(GtkButton *button,
						 const gchar *label,
						 const gchar *icon_name);

/* Dialogs: GTK 4 dropped the nested main loop of gtk_dialog_run() */
gint		e_gtk_dialog_run		(GtkDialog *dialog);
gboolean	e_gtk_dialog_is_running		(GtkWindow *window);
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
GdkPixbuf *	e_gtk_icon_theme_load_icon_for_scale
						(GtkIconTheme *icon_theme,
						 const gchar *icon_name,
						 gint size,
						 gint scale,
						 gint flags,
						 GError **error);
void		e_gtk_render_icon_surface	(GtkStyleContext *context,
						 cairo_t *cr,
						 cairo_surface_t *surface,
						 gdouble x,
						 gdouble y);
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

/* GtkIconInfo as the GtkIconPaintable a lookup gives; NULL when the
 * theme has no such icon, like with GTK 3 */
typedef GtkIconPaintable EGtkIconInfo;

EGtkIconInfo *	e_gtk_icon_theme_lookup_icon	(GtkIconTheme *icon_theme,
						 const gchar *icon_name,
						 gint size,
						 gint flags);
EGtkIconInfo *	e_gtk_icon_theme_lookup_by_gicon
						(GtkIconTheme *icon_theme,
						 GIcon *icon,
						 gint size,
						 gint flags);
const gchar *	e_gtk_icon_info_get_filename	(EGtkIconInfo *icon_info);
GdkPixbuf *	e_gtk_icon_info_load_icon	(EGtkIconInfo *icon_info,
						 GError **error);
GdkPixbuf *	e_gtk_icon_info_load_symbolic	(EGtkIconInfo *icon_info,
						 const GdkRGBA *fg,
						 const GdkRGBA *success_color,
						 const GdkRGBA *warning_color,
						 const GdkRGBA *error_color,
						 gboolean *was_symbolic,
						 GError **error);
gboolean	e_gtk_icon_info_is_symbolic	(EGtkIconInfo *icon_info);

/* GTK 3 container child properties, on what GTK 4 has instead: the
 * GtkNotebookPage, GtkStackPage, GtkGridLayoutChild or the GtkPaned;
 * box packing properties are gone and get ignored */
void		e_gtk_container_child_set	(gpointer container,
						 GtkWidget *child,
						 const gchar *first_property_name,
						 ...) G_GNUC_NULL_TERMINATED;
void		e_gtk_container_child_get	(gpointer container,
						 GtkWidget *child,
						 const gchar *first_property_name,
						 ...) G_GNUC_NULL_TERMINATED;
void		e_gtk_container_child_set_property
						(gpointer container,
						 GtkWidget *child,
						 const gchar *property_name,
						 const GValue *value);
void		e_gtk_container_child_get_property
						(gpointer container,
						 GtkWidget *child,
						 const gchar *property_name,
						 GValue *value);

/* GTK 4 keeps the entry layout private: the character offset at entry
 * coordinates, or -1; assumes the text is not scrolled */
gint		e_gtk_entry_get_index_at_position
						(GtkEntry *entry,
						 gdouble x,
						 gdouble y);

/* a font for the widget through its own CSS provider; NULL unsets it */
void		e_gtk_widget_override_font	(GtkWidget *widget,
						 const PangoFontDescription *font_desc);

/* the keyboard modifiers held now, like gdk_window_get_pointer() gave */
GdkModifierType	e_gtk_widget_get_modifier_state	(GtkWidget *widget);

/* the pointer position in @widget coordinates */
gboolean	e_gtk_widget_get_pointer_position
						(GtkWidget *widget,
						 gdouble *x,
						 gdouble *y);

/* the "labelled-by" ATK relation */
void		e_gtk_widget_set_labelled_by	(GtkWidget *widget,
						 GtkWidget *label);

/* GTK 3 GtkCalendar API: months count from 0, day 0 means none */
void		e_gtk_calendar_get_date		(GtkCalendar *calendar,
						 guint *year,
						 guint *month,
						 guint *day);
void		e_gtk_calendar_select_month	(GtkCalendar *calendar,
						 guint month,
						 guint year);
void		e_gtk_calendar_select_day	(GtkCalendar *calendar,
						 guint day);

/* stock buttons: a "gtk-*" stock id gives its label, others an icon */
GtkWidget *	e_gtk_button_new_from_stock	(const gchar *stock_id);

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
/* GTK 3 check and radio buttons were toggle buttons; with GTK 4 they are
 * unrelated types, both handled by e_gtk_toggle_button_*() (which check
 * the real type), thus this cast does not check */
#undef GTK_TOGGLE_BUTTON
#define GTK_TOGGLE_BUTTON(obj) ((GtkToggleButton *) (obj))

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

/* GTK 4 has no focus chains; the default order applies */
gboolean	e_gtk_container_get_focus_chain	(gpointer container,
						 GList **focusable_widgets);
void		e_gtk_container_set_focus_chain	(gpointer container,
						 GList *focusable_widgets);

/* gtk_main() and gtk_main_quit(): nested GMainLoop-s */
void		e_gtk_main			(void);
void		e_gtk_main_quit			(void);

/* GtkToolbarStyle is gone with GTK 4; the shell switcher and the
 * settings still talk about it */
/* GdkPoint is gone with GTK 4 */
typedef struct _GdkPoint {
	gint x;
	gint y;
} GdkPoint;

typedef enum {
	GTK_TOOLBAR_ICONS,
	GTK_TOOLBAR_TEXT,
	GTK_TOOLBAR_BOTH,
	GTK_TOOLBAR_BOTH_HORIZ
} GtkToolbarStyle;

#define GTK_TYPE_TOOLBAR_STYLE (e_gtk_toolbar_style_get_type ())
GType		e_gtk_toolbar_style_get_type	(void) G_GNUC_CONST;

/* GtkBin is gone with GTK 4: a widget with one main child, which it
 * measures and allocates by default; subclasses may parent more widgets
 * and drop them in remove(), called by e_gtk_container_remove() */
#define E_GTK_TYPE_BIN (e_gtk_bin_get_type ())
G_DECLARE_DERIVABLE_TYPE (EGtkBin, e_gtk_bin, E_GTK, BIN, GtkWidget)

struct _EGtkBinClass {
	GtkWidgetClass parent_class;

	void	(* remove)	(GtkWidget *container,
				 GtkWidget *child);
};

GtkWidget *	e_gtk_bin_get_bin_child		(EGtkBin *bin);
void		e_gtk_bin_set_bin_child		(EGtkBin *bin,
						 GtkWidget *child);

/* GtkLayout is gone with GTK 4: children at fixed positions in an area of
 * a set size, scrolled through the GtkScrollable adjustments */
#define E_GTK_TYPE_LAYOUT (e_gtk_layout_get_type ())
G_DECLARE_DERIVABLE_TYPE (EGtkLayout, e_gtk_layout, E_GTK, LAYOUT, GtkWidget)

struct _EGtkLayoutClass {
	GtkWidgetClass parent_class;
};

GtkWidget *	e_gtk_layout_new		(GtkAdjustment *hadjustment,
						 GtkAdjustment *vadjustment);
void		e_gtk_layout_put		(EGtkLayout *layout,
						 GtkWidget *child,
						 gint x,
						 gint y);
void		e_gtk_layout_move		(EGtkLayout *layout,
						 GtkWidget *child,
						 gint x,
						 gint y);
void		e_gtk_layout_set_size		(EGtkLayout *layout,
						 guint width,
						 guint height);
void		e_gtk_layout_get_size		(EGtkLayout *layout,
						 guint *width,
						 guint *height);

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
