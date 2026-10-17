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

/* GTK 3 drag-and-drop, selection data and clipboard API on top of GTK 4
 * (DoxMail).
 *
 * Evolution moves data with the GTK 3 model: target lists, the drag-*
 * widget signals and GtkSelectionData filled on request.  This keeps that
 * model, under E/e_gtk_ names: the drag-* signals are added to GtkWidget
 * and emitted from GtkDragSource / GtkDropTargetAsync, data is produced
 * on demand by a GdkContentProvider, and the clipboard wraps
 * GdkClipboard.  Atoms are interned strings (mime types or GTK 3 target
 * names). */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_GTK_DND_COMPAT_H
#define E_GTK_DND_COMPAT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Atoms */

typedef const gchar *EGtkAtom;

#define GDK_POINTER_TO_ATOM(ptr) ((EGtkAtom) (ptr))
#define GDK_ATOM_TO_POINTER(atom) ((gpointer) (atom))

#define E_GTK_ATOM_NONE			NULL
#define E_GTK_SELECTION_PRIMARY		(g_intern_static_string ("PRIMARY"))
#define E_GTK_SELECTION_CLIPBOARD	(g_intern_static_string ("CLIPBOARD"))
#define E_GTK_SELECTION_TYPE_STRING	(g_intern_static_string ("STRING"))

EGtkAtom	e_gtk_atom_intern		(const gchar *name,
						 gboolean only_if_exists);
EGtkAtom	e_gtk_atom_intern_static_string	(const gchar *name);
gchar *		e_gtk_atom_name			(EGtkAtom atom);

/* Target lists */

typedef enum {
	E_GTK_TARGET_SAME_APP = 1 << 0,
	E_GTK_TARGET_SAME_WIDGET = 1 << 1,
	E_GTK_TARGET_OTHER_APP = 1 << 2,
	E_GTK_TARGET_OTHER_WIDGET = 1 << 3
} EGtkTargetFlags;

typedef struct _EGtkTargetEntry {
	gchar *target;
	guint flags;
	guint info;
} EGtkTargetEntry;

typedef struct _EGtkTargetList EGtkTargetList;

#define E_GTK_TYPE_TARGET_LIST (e_gtk_target_list_get_type ())

GType		e_gtk_target_list_get_type	(void) G_GNUC_CONST;
EGtkTargetList *e_gtk_target_list_new		(const EGtkTargetEntry *targets,
						 guint ntargets);
EGtkTargetList *e_gtk_target_list_ref		(EGtkTargetList *list);
void		e_gtk_target_list_unref		(EGtkTargetList *list);
void		e_gtk_target_list_add		(EGtkTargetList *list,
						 EGtkAtom target,
						 guint flags,
						 guint info);
void		e_gtk_target_list_add_table	(EGtkTargetList *list,
						 const EGtkTargetEntry *targets,
						 guint ntargets);
void		e_gtk_target_list_add_uri_targets
						(EGtkTargetList *list,
						 guint info);
void		e_gtk_target_list_add_text_targets
						(EGtkTargetList *list,
						 guint info);
void		e_gtk_target_list_add_image_targets
						(EGtkTargetList *list,
						 guint info,
						 gboolean writable);
gboolean	e_gtk_target_list_find		(EGtkTargetList *list,
						 EGtkAtom target,
						 guint *info);
GdkContentFormats *
		e_gtk_target_list_to_formats	(EGtkTargetList *list);
EGtkTargetEntry *
		e_gtk_target_table_new_from_list
						(EGtkTargetList *list,
						 gint *n_targets);
void		e_gtk_target_table_free		(EGtkTargetEntry *targets,
						 gint n_targets);

/* Selection data */

typedef struct _EGtkSelectionData EGtkSelectionData;

#define E_GTK_TYPE_SELECTION_DATA (e_gtk_selection_data_get_type ())

GType		e_gtk_selection_data_get_type	(void) G_GNUC_CONST;
EGtkSelectionData *
		e_gtk_selection_data_new	(EGtkAtom selection,
						 EGtkAtom target);
EGtkSelectionData *
		e_gtk_selection_data_copy	(const EGtkSelectionData *data);
void		e_gtk_selection_data_free	(EGtkSelectionData *data);
EGtkAtom	e_gtk_selection_data_get_selection
						(const EGtkSelectionData *data);
EGtkAtom	e_gtk_selection_data_get_target	(const EGtkSelectionData *data);
EGtkAtom	e_gtk_selection_data_get_data_type
						(const EGtkSelectionData *data);
gint		e_gtk_selection_data_get_format	(const EGtkSelectionData *data);
const guchar *	e_gtk_selection_data_get_data	(const EGtkSelectionData *data);
gint		e_gtk_selection_data_get_length	(const EGtkSelectionData *data);
const guchar *	e_gtk_selection_data_get_data_with_length
						(const EGtkSelectionData *data,
						 gint *length);
GdkDisplay *	e_gtk_selection_data_get_display
						(const EGtkSelectionData *data);
void		e_gtk_selection_data_set	(EGtkSelectionData *data,
						 EGtkAtom type,
						 gint format,
						 const guchar *bytes,
						 gint length);
gboolean	e_gtk_selection_data_set_text	(EGtkSelectionData *data,
						 const gchar *str,
						 gint len);
guchar *	e_gtk_selection_data_get_text	(const EGtkSelectionData *data);
gboolean	e_gtk_selection_data_set_uris	(EGtkSelectionData *data,
						 gchar **uris);
gchar **	e_gtk_selection_data_get_uris	(const EGtkSelectionData *data);
gboolean	e_gtk_selection_data_get_targets
						(const EGtkSelectionData *data,
						 EGtkAtom **targets,
						 gint *n_atoms);
gboolean	e_gtk_selection_data_targets_include_text
						(const EGtkSelectionData *data);
gboolean	e_gtk_targets_include_text	(EGtkAtom *targets,
						 gint n_targets);
gboolean	e_gtk_targets_include_image	(EGtkAtom *targets,
						 gint n_targets,
						 gboolean writable);
gboolean	e_gtk_targets_include_uri	(EGtkAtom *targets,
						 gint n_targets);

/* Drag contexts */

#define E_GTK_TYPE_DRAG_CONTEXT (e_gtk_drag_context_get_type ())
G_DECLARE_FINAL_TYPE (EGtkDragContext, e_gtk_drag_context, E_GTK, DRAG_CONTEXT, GObject)

GdkDragAction	e_gtk_drag_context_get_actions	(EGtkDragContext *context);
GdkDragAction	e_gtk_drag_context_get_suggested_action
						(EGtkDragContext *context);
GdkDragAction	e_gtk_drag_context_get_selected_action
						(EGtkDragContext *context);
GList *		e_gtk_drag_context_list_targets	(EGtkDragContext *context); /* EGtkAtom, owned by the context */
GdkDrag *	e_gtk_drag_context_get_drag	(EGtkDragContext *context);
GdkDrop *	e_gtk_drag_context_get_drop	(EGtkDragContext *context);
void		e_gtk_drag_status		(EGtkDragContext *context,
						 GdkDragAction action,
						 guint32 time_);

/* Drag source */

void		e_gtk_drag_source_set		(GtkWidget *widget,
						 GdkModifierType start_button_mask,
						 const EGtkTargetEntry *targets,
						 gint n_targets,
						 GdkDragAction actions);
void		e_gtk_drag_source_unset		(GtkWidget *widget);
void		e_gtk_drag_source_set_target_list
						(GtkWidget *widget,
						 EGtkTargetList *target_list);
EGtkTargetList *e_gtk_drag_source_get_target_list
						(GtkWidget *widget);
EGtkDragContext *
		e_gtk_drag_begin		(GtkWidget *widget,
						 EGtkTargetList *targets,
						 GdkDragAction actions,
						 gint button,
						 GdkEvent *event);
EGtkDragContext *
		e_gtk_drag_begin_with_coordinates
						(GtkWidget *widget,
						 EGtkTargetList *targets,
						 GdkDragAction actions,
						 gint button,
						 GdkEvent *event,
						 gint x,
						 gint y);
void		e_gtk_drag_cancel		(EGtkDragContext *context);
void		e_gtk_drag_set_icon_surface	(EGtkDragContext *context,
						 cairo_surface_t *surface);
void		e_gtk_drag_set_icon_paintable	(EGtkDragContext *context,
						 GdkPaintable *paintable,
						 gint hot_x,
						 gint hot_y);
void		e_gtk_drag_set_icon_pixbuf	(EGtkDragContext *context,
						 GdkPixbuf *pixbuf,
						 gint hot_x,
						 gint hot_y);
void		e_gtk_drag_set_icon_default	(EGtkDragContext *context);
GtkWidget *	e_gtk_drag_get_source_widget	(EGtkDragContext *context);

/* Drag destination */

typedef enum {
	E_GTK_DEST_DEFAULT_MOTION = 1 << 0,
	E_GTK_DEST_DEFAULT_HIGHLIGHT = 1 << 1,
	E_GTK_DEST_DEFAULT_DROP = 1 << 2,
	E_GTK_DEST_DEFAULT_ALL = 0x07
} EGtkDestDefaults;

void		e_gtk_drag_dest_set		(GtkWidget *widget,
						 EGtkDestDefaults flags,
						 const EGtkTargetEntry *targets,
						 gint n_targets,
						 GdkDragAction actions);
void		e_gtk_drag_dest_unset		(GtkWidget *widget);
void		e_gtk_drag_dest_set_target_list	(GtkWidget *widget,
						 EGtkTargetList *target_list);
EGtkTargetList *e_gtk_drag_dest_get_target_list	(GtkWidget *widget);
void		e_gtk_drag_dest_add_uri_targets	(GtkWidget *widget);
void		e_gtk_drag_dest_add_text_targets
						(GtkWidget *widget);
EGtkAtom	e_gtk_drag_dest_find_target	(GtkWidget *widget,
						 EGtkDragContext *context,
						 EGtkTargetList *target_list);
void		e_gtk_drag_get_data		(GtkWidget *widget,
						 EGtkDragContext *context,
						 EGtkAtom target,
						 guint32 time_);
void		e_gtk_drag_finish		(EGtkDragContext *context,
						 gboolean success,
						 gboolean del,
						 guint32 time_);

/* Clipboard */

typedef void	(*EGtkClipboardTextReceivedFunc)
						(GdkClipboard *clipboard,
						 const gchar *text,
						 gpointer user_data);
typedef void	(*EGtkClipboardReceivedFunc)	(GdkClipboard *clipboard,
						 EGtkSelectionData *selection_data,
						 gpointer user_data);
typedef void	(*EGtkClipboardTargetsReceivedFunc)
						(GdkClipboard *clipboard,
						 EGtkAtom *atoms,
						 gint n_atoms,
						 gpointer user_data);
typedef void	(*EGtkClipboardGetFunc)		(GdkClipboard *clipboard,
						 EGtkSelectionData *selection_data,
						 guint info,
						 gpointer user_data_or_owner);
typedef void	(*EGtkClipboardClearFunc)	(GdkClipboard *clipboard,
						 gpointer user_data_or_owner);

GdkClipboard *	e_gtk_clipboard_get		(EGtkAtom selection);
GdkClipboard *	e_gtk_clipboard_get_for_display	(GdkDisplay *display,
						 EGtkAtom selection);
void		e_gtk_clipboard_set_text	(GdkClipboard *clipboard,
						 const gchar *text,
						 gint len);
void		e_gtk_clipboard_set_image	(GdkClipboard *clipboard,
						 GdkPixbuf *pixbuf);
gboolean	e_gtk_clipboard_set_with_data	(GdkClipboard *clipboard,
						 const EGtkTargetEntry *targets,
						 guint n_targets,
						 EGtkClipboardGetFunc get_func,
						 EGtkClipboardClearFunc clear_func,
						 gpointer user_data);
gboolean	e_gtk_clipboard_set_with_owner	(GdkClipboard *clipboard,
						 const EGtkTargetEntry *targets,
						 guint n_targets,
						 EGtkClipboardGetFunc get_func,
						 EGtkClipboardClearFunc clear_func,
						 GObject *owner);
GObject *	e_gtk_clipboard_get_owner	(GdkClipboard *clipboard);
void		e_gtk_clipboard_clear		(GdkClipboard *clipboard);
void		e_gtk_clipboard_store		(GdkClipboard *clipboard);
void		e_gtk_clipboard_set_can_store	(GdkClipboard *clipboard,
						 const EGtkTargetEntry *targets,
						 gint n_targets);
void		e_gtk_clipboard_request_text	(GdkClipboard *clipboard,
						 EGtkClipboardTextReceivedFunc callback,
						 gpointer user_data);
void		e_gtk_clipboard_request_contents
						(GdkClipboard *clipboard,
						 EGtkAtom target,
						 EGtkClipboardReceivedFunc callback,
						 gpointer user_data);
void		e_gtk_clipboard_request_targets	(GdkClipboard *clipboard,
						 EGtkClipboardTargetsReceivedFunc callback,
						 gpointer user_data);
gchar *		e_gtk_clipboard_wait_for_text	(GdkClipboard *clipboard);
EGtkSelectionData *
		e_gtk_clipboard_wait_for_contents
						(GdkClipboard *clipboard,
						 EGtkAtom target);
gboolean	e_gtk_clipboard_wait_for_targets
						(GdkClipboard *clipboard,
						 EGtkAtom **targets,
						 gint *n_targets);
gboolean	e_gtk_clipboard_wait_is_text_available
						(GdkClipboard *clipboard);
gboolean	e_gtk_clipboard_wait_is_image_available
						(GdkClipboard *clipboard);
gboolean	e_gtk_clipboard_wait_is_target_available
						(GdkClipboard *clipboard,
						 EGtkAtom target);
GdkPixbuf *	e_gtk_clipboard_wait_for_image	(GdkClipboard *clipboard);
gchar **	e_gtk_clipboard_wait_for_uris	(GdkClipboard *clipboard);

G_END_DECLS

#endif /* E_GTK_DND_COMPAT_H */
