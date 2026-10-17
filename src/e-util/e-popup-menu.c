/*
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
 * Authors:
 *		Miguel de Icaza <miguel@ximian.com>
 *		Jody Goldberg (jgoldberg@home.com)
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <libintl.h>
#include <string.h>

#include <gtk/gtk.h>

#include "e-popup-menu.h"

#define MENU_KEY "e-popup-menu-menu"
#define ACTIONS_KEY "e-popup-menu-actions"

typedef struct {
	GCallback fn;
	gpointer closure;
	GWeakRef popover;
} ItemData;

static void
item_data_free (gpointer ptr)
{
	ItemData *data = ptr;

	g_weak_ref_clear (&data->popover);
	g_free (data);
}

static void
item_activate_cb (GSimpleAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	ItemData *data = user_data;
	GtkWidget *popover = g_weak_ref_get (&data->popover);

	((void (*) (GtkWidget *, gpointer)) data->fn) (popover, data->closure);

	g_clear_object (&popover);
}

GtkWidget *
e_popup_menu_create_with_domain (EPopupMenu *menu_list,
                                 guint32 disable_mask,
                                 guint32 hide_mask,
                                 gpointer default_closure,
                                 const gchar *domain)
{
	GtkWidget *popover;
	GSimpleActionGroup *actions;
	GMenu *menu, *section;
	gint last_non_separator = -1;
	gint i;

	menu = g_menu_new ();
	actions = g_simple_action_group_new ();

	popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
	gtk_widget_insert_action_group (popover, "epm", G_ACTION_GROUP (actions));

	g_object_set_data_full (G_OBJECT (popover), MENU_KEY, menu, g_object_unref);
	g_object_set_data_full (G_OBJECT (popover), ACTIONS_KEY, actions, g_object_unref);

	for (i = 0; menu_list[i].name; i++) {
		if (strcmp ("", menu_list[i].name) && !(menu_list[i].disable_mask & hide_mask)) {
			last_non_separator = i;
		}
	}

	/* separators start a new section */
	section = g_menu_new ();
	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));

	for (i = 0; i <= last_non_separator; i++) {
		gchar *action_name;

		if (menu_list[i].disable_mask & hide_mask)
			continue;

		if (!strcmp ("", menu_list[i].name)) {
			if (g_menu_model_get_n_items (G_MENU_MODEL (section)) > 0) {
				g_object_unref (section);
				section = g_menu_new ();
				g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
			}
			continue;
		}

		action_name = g_strdup_printf ("item-%d", i);

		if (menu_list[i].fn) {
			GSimpleAction *action;
			ItemData *data;

			data = g_new0 (ItemData, 1);
			data->fn = menu_list[i].fn;
			data->closure = default_closure;
			g_weak_ref_init (&data->popover, popover);

			action = g_simple_action_new (action_name, NULL);
			g_signal_connect_data (action, "activate",
				G_CALLBACK (item_activate_cb), data,
				(GClosureNotify) item_data_free, 0);
			g_simple_action_set_enabled (action, !(menu_list[i].disable_mask & disable_mask));
			g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));
			g_object_unref (action);
		}

		{
			gchar *detailed = g_strconcat ("epm.", action_name, NULL);
			g_menu_append (section, dgettext (domain, menu_list[i].name), detailed);
			g_free (detailed);
		}

		g_free (action_name);
	}

	g_object_unref (section);

	return popover;
}

GMenu *
e_popup_menu_get_menu (GtkWidget *popup_menu)
{
	g_return_val_if_fail (GTK_IS_POPOVER_MENU (popup_menu), NULL);

	return g_object_get_data (G_OBJECT (popup_menu), MENU_KEY);
}

GActionMap *
e_popup_menu_get_actions (GtkWidget *popup_menu)
{
	g_return_val_if_fail (GTK_IS_POPOVER_MENU (popup_menu), NULL);

	return g_object_get_data (G_OBJECT (popup_menu), ACTIONS_KEY);
}
