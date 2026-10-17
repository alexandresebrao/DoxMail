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

/* GTK 3 file chooser API on GTK 4 (DoxMail).
 *
 * GTK 4 dropped GtkFileChooserButton and changed the GtkFileChooser
 * functions to GFile-s and list models.  EGtkFileChooserButton is a button
 * showing the chosen file and opening a GtkFileChooserNative, and the
 * e_gtk_file_chooser_*() functions take the GTK 3 arguments and work on
 * both that button and any GtkFileChooser. */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_GTK_FILE_CHOOSER_COMPAT_H
#define E_GTK_FILE_CHOOSER_COMPAT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_GTK_TYPE_FILE_CHOOSER_BUTTON (e_gtk_file_chooser_button_get_type ())
G_DECLARE_FINAL_TYPE (EGtkFileChooserButton, e_gtk_file_chooser_button, E_GTK, FILE_CHOOSER_BUTTON, GtkButton)

GtkWidget *	e_gtk_file_chooser_button_new	(const gchar *title,
						 GtkFileChooserAction action);
GtkWidget *	e_gtk_file_chooser_button_new_with_dialog
						(gpointer dialog);

gchar *		e_gtk_file_chooser_get_filename	(gpointer chooser);
gboolean	e_gtk_file_chooser_set_filename	(gpointer chooser,
						 const gchar *filename);
gchar *		e_gtk_file_chooser_get_uri	(gpointer chooser);
gboolean	e_gtk_file_chooser_set_uri	(gpointer chooser,
						 const gchar *uri);
GFile *		e_gtk_file_chooser_get_file	(gpointer chooser);
gboolean	e_gtk_file_chooser_set_file	(gpointer chooser,
						 GFile *file,
						 GError **error);
GSList *	e_gtk_file_chooser_get_files	(gpointer chooser); /* GFile * */
GSList *	e_gtk_file_chooser_get_uris	(gpointer chooser); /* gchar * */
void		e_gtk_file_chooser_unselect_all	(gpointer chooser);
void		e_gtk_file_chooser_add_filter	(gpointer chooser,
						 GtkFileFilter *filter);
void		e_gtk_file_chooser_set_filter	(gpointer chooser,
						 GtkFileFilter *filter);
GtkFileFilter *	e_gtk_file_chooser_get_filter	(gpointer chooser);
GSList *	e_gtk_file_chooser_list_filters	(gpointer chooser); /* GtkFileFilter * */
gboolean	e_gtk_file_chooser_set_current_folder
						(gpointer chooser,
						 const gchar *filename);
gboolean	e_gtk_file_chooser_set_current_folder_uri
						(gpointer chooser,
						 const gchar *uri);
gchar *		e_gtk_file_chooser_get_current_folder_uri
						(gpointer chooser);

/* GTK 4 file choosers have no preview; "update-preview" exists, but is
 * never emitted, and the preview functions do nothing */
gchar *		e_gtk_file_chooser_get_preview_filename
						(gpointer chooser);
GtkWidget *	e_gtk_file_chooser_get_preview_widget
						(gpointer chooser);
void		e_gtk_file_chooser_set_preview_widget
						(gpointer chooser,
						 GtkWidget *preview_widget);
void		e_gtk_file_chooser_set_preview_widget_active
						(gpointer chooser,
						 gboolean active);
void		e_gtk_file_chooser_set_use_preview_label
						(gpointer chooser,
						 gboolean use_label);
/* below the chooser of a GtkFileChooserDialog; kept unshown elsewhere */
void		e_gtk_file_chooser_set_extra_widget
						(gpointer chooser,
						 GtkWidget *extra_widget);
GtkWidget *	e_gtk_file_chooser_get_extra_widget
						(gpointer chooser);

G_END_DECLS

#endif /* E_GTK_FILE_CHOOSER_COMPAT_H */
