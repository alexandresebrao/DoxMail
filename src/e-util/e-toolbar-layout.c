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

/* DoxMail: GtkToolbar is gone with GTK 4 and the toolbars became boxes,
 * which never get narrower than all their items. This layout manager lays
 * the items out at their natural width while they fit; the others are not
 * shown and an overflow button at the end offers their actions in a menu,
 * as GtkToolbar did. The toolbar stays a GtkBox for its users. */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-toolbar-layout.h"

#define OVERFLOW_KEY "e-toolbar-layout-overflow"
#define ACTION_KEY "e-toolbar-layout-action"

struct _EToolbarLayout {
	GtkLayoutManager parent;
};

/* GtkBox reads its orientation from the layout manager */
enum {
	PROP_0,
	PROP_ORIENTATION
};

G_DEFINE_TYPE_WITH_CODE (EToolbarLayout, e_toolbar_layout, GTK_TYPE_LAYOUT_MANAGER,
	G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
toolbar_layout_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_ORIENTATION:
		/* toolbars are horizontal */
		g_warn_if_fail (g_value_get_enum (value) == GTK_ORIENTATION_HORIZONTAL);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
toolbar_layout_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_ORIENTATION:
		g_value_set_enum (value, GTK_ORIENTATION_HORIZONTAL);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static GtkWidget *
toolbar_get_overflow (GtkWidget *toolbar)
{
	return g_object_get_data (G_OBJECT (toolbar), OVERFLOW_KEY);
}

static gboolean
toolbar_item_takes_part (GtkWidget *child,
			 GtkWidget *overflow)
{
	return child != overflow && gtk_widget_get_visible (child) && !GTK_IS_NATIVE (child);
}

static void
toolbar_layout_measure (GtkLayoutManager *manager,
			GtkWidget *widget,
			GtkOrientation orientation,
			gint for_size,
			gint *minimum,
			gint *natural,
			gint *minimum_baseline,
			gint *natural_baseline)
{
	GtkWidget *overflow = toolbar_get_overflow (widget);
	GtkWidget *child;
	gint min = 0, nat = 0, overflow_min = 0;
	gboolean any_item = FALSE;

	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		gint child_min = 0, child_nat = 0;

		if (child != overflow && !toolbar_item_takes_part (child, overflow))
			continue;

		gtk_widget_measure (child, orientation, -1, &child_min, &child_nat, NULL, NULL);

		if (child == overflow) {
			overflow_min = child_nat;
		} else if (orientation == GTK_ORIENTATION_HORIZONTAL) {
			nat += child_nat;
			any_item = TRUE;
		} else {
			min = MAX (min, child_min);
			nat = MAX (nat, child_nat);
			any_item = TRUE;
		}
	}

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		/* down to the overflow button alone */
		min = any_item ? MIN (overflow_min, nat) : 0;
	} else if (any_item) {
		min = MAX (min, overflow_min);
		nat = MAX (nat, overflow_min);
	}

	*minimum = min;
	*natural = nat;
	*minimum_baseline = -1;
	*natural_baseline = -1;
}

static void
toolbar_layout_allocate (GtkLayoutManager *manager,
			 GtkWidget *widget,
			 gint width,
			 gint height,
			 gint baseline)
{
	GtkWidget *overflow = toolbar_get_overflow (widget);
	GtkWidget *child, *last_shown = NULL;
	gint total = 0, avail, xx = 0, overflow_width = 0;
	gboolean cut = FALSE, overflowing, showing;

	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		gint nat = 0;

		if (!toolbar_item_takes_part (child, overflow))
			continue;

		gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &nat, NULL, NULL);
		total += nat;
	}

	avail = width;
	if (total > width && overflow) {
		gtk_widget_measure (overflow, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &overflow_width, NULL, NULL);
		avail = MAX (0, width - overflow_width);
	}

	/* which items fit, in order; a separator does not end the row */
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		gint nat = 0;

		if (!toolbar_item_takes_part (child, overflow))
			continue;

		gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &nat, NULL, NULL);

		if (!cut && xx + nat <= avail) {
			xx += nat;
			if (!GTK_IS_SEPARATOR (child))
				last_shown = child;
		} else {
			cut = TRUE;
		}
	}

	overflowing = total > width && overflow;
	showing = !overflowing || last_shown;
	xx = 0;

	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		gint nat = 0;

		if (!toolbar_item_takes_part (child, overflow))
			continue;

		if (!showing) {
			gtk_widget_set_child_visible (child, FALSE);
			continue;
		}

		gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &nat, NULL, NULL);
		gtk_widget_set_child_visible (child, TRUE);
		gtk_widget_size_allocate (child, &(GtkAllocation) { xx, 0, nat, height }, baseline);
		xx += nat;

		/* separators after the last shown item go too */
		if (overflowing && child == last_shown)
			showing = FALSE;
	}

	if (overflow) {
		gtk_widget_set_child_visible (overflow, overflowing);
		if (overflowing)
			gtk_widget_size_allocate (overflow, &(GtkAllocation) { width - overflow_width, 0, overflow_width, height }, baseline);
	}
}

static void
e_toolbar_layout_class_init (EToolbarLayoutClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

	object_class->set_property = toolbar_layout_set_property;
	object_class->get_property = toolbar_layout_get_property;

	g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

	layout_class->measure = toolbar_layout_measure;
	layout_class->allocate = toolbar_layout_allocate;
}

static void
e_toolbar_layout_init (EToolbarLayout *self)
{
}

/* the actions of the items not shown, sections split at separators */
static void
toolbar_overflow_create_popup_cb (GtkMenuButton *button,
				  gpointer user_data)
{
	GtkWidget *toolbar = user_data;
	GMenu *menu, *section = NULL;
	GtkWidget *child;

	menu = G_MENU (gtk_menu_button_get_menu_model (button));
	g_menu_remove_all (menu);

	for (child = gtk_widget_get_first_child (toolbar); child; child = gtk_widget_get_next_sibling (child)) {
		EUIAction *action;
		GMenuItem *item;
		GVariant *target;
		const gchar *icon_name;
		gchar *detailed;

		if (!toolbar_item_takes_part (child, GTK_WIDGET (button)) || gtk_widget_get_child_visible (child))
			continue;

		if (GTK_IS_SEPARATOR (child)) {
			section = NULL;
			continue;
		}

		action = g_object_get_data (G_OBJECT (child), ACTION_KEY);
		if (!action || !e_ui_action_get_label (action))
			continue;

		if (!section) {
			section = g_menu_new ();
			g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
			g_object_unref (section);
		}

		detailed = g_strconcat (e_ui_action_get_map_name (action), ".", g_action_get_name (G_ACTION (action)), NULL);
		item = g_menu_item_new (e_ui_action_get_label (action), NULL);

		target = g_action_get_parameter_type (G_ACTION (action)) ? e_ui_action_ref_target (action) : NULL;
		g_menu_item_set_action_and_target_value (item, detailed, target);
		g_clear_pointer (&target, g_variant_unref);

		icon_name = e_ui_action_get_icon_name (action);
		if (icon_name && !g_str_has_prefix (icon_name, "gicon::")) {
			GIcon *icon = g_themed_icon_new (icon_name);

			g_menu_item_set_icon (item, icon);
			g_object_unref (icon);
		}

		g_menu_append_item (section, item);
		g_object_unref (item);
		g_free (detailed);
	}
}

/**
 * e_toolbar_layout_install:
 * @toolbar: a horizontal #GtkBox used as a toolbar
 *
 * Gives @toolbar an #EToolbarLayout and the overflow button, or moves the
 * button back to the end after the items changed.
 **/
void
e_toolbar_layout_install (GtkWidget *toolbar)
{
	GtkWidget *overflow;

	g_return_if_fail (GTK_IS_BOX (toolbar));

	overflow = toolbar_get_overflow (toolbar);

	if (!overflow) {
		GMenu *menu;

		gtk_widget_set_layout_manager (toolbar, g_object_new (E_TYPE_TOOLBAR_LAYOUT, NULL));

		overflow = gtk_menu_button_new ();
		gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (overflow), "view-more-symbolic");
		gtk_menu_button_set_has_frame (GTK_MENU_BUTTON (overflow), FALSE);
		gtk_widget_set_tooltip_text (overflow, _("More"));
		gtk_widget_add_css_class (overflow, "toolbar-overflow");

		menu = g_menu_new ();
		gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (overflow), G_MENU_MODEL (menu));
		g_object_unref (menu);

		gtk_menu_button_set_create_popup_func (GTK_MENU_BUTTON (overflow),
			toolbar_overflow_create_popup_cb, toolbar, NULL);

		g_object_set_data (G_OBJECT (toolbar), OVERFLOW_KEY, overflow);
	}

	/* last, also in the focus chain */
	if (gtk_widget_get_parent (overflow) != toolbar)
		gtk_box_append (GTK_BOX (toolbar), overflow);
	else if (gtk_widget_get_last_child (toolbar) != overflow)
		gtk_box_reorder_child_after (GTK_BOX (toolbar), overflow, gtk_widget_get_last_child (toolbar));
}

/**
 * e_toolbar_layout_get_overflow:
 * @toolbar: a toolbar
 *
 * Returns: (transfer none) (nullable): the overflow button of @toolbar
 **/
GtkWidget *
e_toolbar_layout_get_overflow (GtkWidget *toolbar)
{
	g_return_val_if_fail (GTK_IS_WIDGET (toolbar), NULL);

	return toolbar_get_overflow (toolbar);
}

/**
 * e_toolbar_layout_set_item_action:
 * @item: a toolbar item
 * @action: the #EUIAction the @item stands for
 *
 * Remembers the @action, for the overflow menu.
 **/
void
e_toolbar_layout_set_item_action (GtkWidget *item,
				  EUIAction *action)
{
	g_return_if_fail (GTK_IS_WIDGET (item));

	g_object_set_data_full (G_OBJECT (item), ACTION_KEY,
		action ? g_object_ref (action) : NULL, action ? g_object_unref : NULL);
}
