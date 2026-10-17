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

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-ellipsized-combo-box-text.h"

#define MAX_WIDTH_KEY "e-ellipsized-max-natural-width"

GType
e_ellipsized_combo_box_text_get_type (void)
{
	return GTK_TYPE_COMBO_BOX_TEXT;
}

static void
ellipsized_combo_box_text_update_cells (GtkComboBoxText *combo_box,
					gint max_natural_width)
{
	GList *cells, *link;
	gint max_chars;

	/* cells measure in characters; an average one is about 8px wide */
	max_chars = max_natural_width > 0 ? MAX (max_natural_width / 8, 1) : -1;

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (combo_box));
	for (link = cells; link; link = g_list_next (link)) {
		if (GTK_IS_CELL_RENDERER_TEXT (link->data)) {
			g_object_set (link->data,
				"ellipsize", PANGO_ELLIPSIZE_END,
				"max-width-chars", max_chars,
				NULL);
		}
	}

	g_list_free (cells);
}

GtkWidget *
e_ellipsized_combo_box_text_new (gboolean has_entry)
{
	GtkWidget *widget;

	widget = has_entry ? gtk_combo_box_text_new_with_entry () : gtk_combo_box_text_new ();
	e_ellipsized_combo_box_text_set_max_natural_width (GTK_COMBO_BOX_TEXT (widget), 225);

	return widget;
}

gint
e_ellipsized_combo_box_text_get_max_natural_width (EEllipsizedComboBoxText *combo_box)
{
	g_return_val_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box), -1);

	return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo_box), MAX_WIDTH_KEY));
}

void
e_ellipsized_combo_box_text_set_max_natural_width (EEllipsizedComboBoxText *combo_box,
						   gint max_natural_width)
{
	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo_box));

	g_object_set_data (G_OBJECT (combo_box), MAX_WIDTH_KEY, GINT_TO_POINTER (max_natural_width));
	ellipsized_combo_box_text_update_cells (combo_box, max_natural_width);
}
