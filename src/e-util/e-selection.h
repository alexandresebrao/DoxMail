/*
 * e-selection.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SELECTION_H
#define E_SELECTION_H

#include <gtk/gtk.h>
#include <e-util/e-gtk4-compat.h>

/* This API mimics GTK's API for dealing with text, image and URI data. */

G_BEGIN_DECLS

/* Selection Functions */

void		e_target_list_add_calendar_targets
					(EGtkTargetList *list,
					 guint info);
void		e_target_list_add_directory_targets
					(EGtkTargetList *list,
					 guint info);
void		e_target_list_add_html_targets
					(EGtkTargetList *list,
					 guint info);
gboolean	e_selection_data_set_calendar
					(EGtkSelectionData *selection_data,
					 const gchar *source,
					 gint length);
gboolean	e_selection_data_set_directory
					(EGtkSelectionData *selection_data,
					 const gchar *source,
					 gint length);
gboolean	e_selection_data_set_html
					(EGtkSelectionData *selection_data,
					 const gchar *source,
					 gint length);
gchar *		e_selection_data_get_calendar
					(EGtkSelectionData *selection_data);
gchar *		e_selection_data_get_directory
					(EGtkSelectionData *selection_data);
gchar *		e_selection_data_get_html
					(EGtkSelectionData *selection_data);
gboolean	e_selection_data_targets_include_calendar
					(EGtkSelectionData *selection_data);
gboolean	e_selection_data_targets_include_directory
					(EGtkSelectionData *selection_data);
gboolean	e_selection_data_targets_include_html
					(EGtkSelectionData *selection_data);
gboolean	e_targets_include_calendar
					(EGtkAtom *targets,
					 gint n_targets);
gboolean	e_targets_include_directory
					(EGtkAtom *targets,
					 gint n_targets);
gboolean	e_targets_include_html	(EGtkAtom *targets,
					 gint n_targets);

/* Clipboard Functions */

void		e_clipboard_set_calendar
					(GdkClipboard *clipboard,
					 const gchar *source,
					 gint length);
void		e_clipboard_set_directory
					(GdkClipboard *clipboard,
					 const gchar *source,
					 gint length);
void		e_clipboard_set_html	(GdkClipboard *clipboard,
					 const gchar *source,
					 gint length);
void		e_clipboard_request_calendar
					(GdkClipboard *clipboard,
					 EGtkClipboardTextReceivedFunc callback,
					 gpointer user_data);
void		e_clipboard_request_directory
					(GdkClipboard *clipboard,
					 EGtkClipboardTextReceivedFunc callback,
					 gpointer user_data);
void		e_clipboard_request_html
					(GdkClipboard *clipboard,
					 EGtkClipboardTextReceivedFunc callback,
					 gpointer user_data);
gchar *		e_clipboard_wait_for_calendar
					(GdkClipboard *clipboard);
gchar *		e_clipboard_wait_for_directory
					(GdkClipboard *clipboard);
gchar *		e_clipboard_wait_for_html
					(GdkClipboard *clipboard);
gboolean	e_clipboard_wait_is_calendar_available
					(GdkClipboard *clipboard);
gboolean	e_clipboard_wait_is_directory_available
					(GdkClipboard *clipboard);
gboolean	e_clipboard_wait_is_html_available
					(GdkClipboard *clipboard);

/* Drag and Drop Functions */

void		e_drag_dest_add_calendar_targets
					(GtkWidget *widget);
void		e_drag_dest_add_directory_targets
					(GtkWidget *widget);
void		e_drag_dest_add_html_targets
					(GtkWidget *widget);
void		e_drag_source_add_calendar_targets
					(GtkWidget *widget);
void		e_drag_source_add_directory_targets
					(GtkWidget *widget);
void		e_drag_source_add_html_targets
					(GtkWidget *widget);

G_END_DECLS

#endif /* E_SELECTION_H */
