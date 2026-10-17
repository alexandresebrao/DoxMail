/*
 * e-image-chooser-dialog.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_IMAGE_CHOOSER_DIALOG_H
#define E_IMAGE_CHOOSER_DIALOG_H

#include <gtk/gtk.h>
#include <e-util/e-gtk4-compat.h>

G_BEGIN_DECLS

/* GTK 4 file choosers have no preview widget, thus this is no longer an own
 * dialog type, but a GtkFileChooserNative set up for images. */
GtkFileChooserNative *
		e_image_chooser_dialog_new	(const gchar *title,
						 GtkWindow *parent);
GFile *		e_image_chooser_dialog_run	(GtkFileChooserNative *dialog);

G_END_DECLS

#endif /* E_IMAGE_CHOOSER_DIALOG_H */
