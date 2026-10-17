/*
 * e-image-chooser-dialog.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-misc-utils.h"
#include "e-image-chooser-dialog.h"

GtkFileChooserNative *
e_image_chooser_dialog_new (const gchar *title,
                            GtkWindow *parent)
{
	GtkFileChooserNative *native;
	GtkFileFilter *file_filter;

	native = gtk_file_chooser_native_new (title, parent,
		GTK_FILE_CHOOSER_ACTION_OPEN, _("_Open"), _("_Cancel"));

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pixbuf_formats (file_filter);
	gtk_file_filter_set_name (file_filter, _("Image files"));
	e_gtk_file_chooser_add_filter (native, file_filter);
	e_gtk_file_chooser_set_filter (native, file_filter);
	g_object_unref (file_filter);

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (file_filter, _("All files"));
	gtk_file_filter_add_pattern (file_filter, "*");
	e_gtk_file_chooser_add_filter (native, file_filter);
	g_object_unref (file_filter);

	return native;
}

/* Runs the @dialog and frees it; returns the chosen file or %NULL */
GFile *
e_image_chooser_dialog_run (GtkFileChooserNative *dialog)
{
	GFile *file = NULL;

	g_return_val_if_fail (GTK_IS_FILE_CHOOSER_NATIVE (dialog), NULL);

	if (e_gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		file = e_gtk_file_chooser_get_file (dialog);

	g_object_unref (dialog);

	return file;
}
