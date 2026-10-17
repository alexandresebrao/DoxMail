/*
 * Copyright (C) 2020 Red Hat (www.redhat.com)
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ELLIPSIZED_COMBO_BOX_TEXT_H
#define E_ELLIPSIZED_COMBO_BOX_TEXT_H

#include <gtk/gtk.h>
#include <e-util/e-gtk4-compat.h>

/* GtkComboBoxText is final on GTK 4, so this is a plain GtkComboBoxText
 * with ellipsized cells and a capped natural width */
#define E_TYPE_ELLIPSIZED_COMBO_BOX_TEXT \
	(e_ellipsized_combo_box_text_get_type ())
#define E_ELLIPSIZED_COMBO_BOX_TEXT(obj) \
	(GTK_COMBO_BOX_TEXT (obj))
#define E_IS_ELLIPSIZED_COMBO_BOX_TEXT(obj) \
	(GTK_IS_COMBO_BOX_TEXT (obj))

G_BEGIN_DECLS

typedef GtkComboBoxText EEllipsizedComboBoxText;

GType		e_ellipsized_combo_box_text_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_ellipsized_combo_box_text_new		(gboolean has_entry);
gint		e_ellipsized_combo_box_text_get_max_natural_width
							(EEllipsizedComboBoxText *combo_box);
void		e_ellipsized_combo_box_text_set_max_natural_width
							(EEllipsizedComboBoxText *combo_box,
							 gint max_natural_width);

G_END_DECLS

#endif /* E_ELLIPSIZED_COMBO_BOX_TEXT_H */
