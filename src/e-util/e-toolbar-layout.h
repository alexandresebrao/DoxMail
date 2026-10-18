/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either the version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

/* DoxMail: GTK 4 toolbars are boxes; this layout gives them the overflow
 * menu GtkToolbar had. Private to e-util. */

#ifndef E_TOOLBAR_LAYOUT_H
#define E_TOOLBAR_LAYOUT_H

#include <gtk/gtk.h>

#ifdef LIBEUTIL_COMPILATION
#include <e-util/e-ui-action.h>
#else
#include <e-util/e-util.h>
#endif

G_BEGIN_DECLS

#define E_TYPE_TOOLBAR_LAYOUT (e_toolbar_layout_get_type ())
G_DECLARE_FINAL_TYPE (EToolbarLayout, e_toolbar_layout, E, TOOLBAR_LAYOUT, GtkLayoutManager)

void		e_toolbar_layout_install	(GtkWidget *toolbar);
GtkWidget *	e_toolbar_layout_get_overflow	(GtkWidget *toolbar);
void		e_toolbar_layout_set_item_action
						(GtkWidget *item,
						 EUIAction *action);

G_END_DECLS

#endif /* E_TOOLBAR_LAYOUT_H */
