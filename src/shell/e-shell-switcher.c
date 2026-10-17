/*
 * e-shell-switcher.c
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

/**
 * SECTION: e-shell-switcher
 * @short_description: buttons for switching views
 * @include: shell/e-shell-switcher.h
 **/

#include "evolution-config.h"

#include <glib/gi18n.h>
#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

#include "e-shell-switcher.h"

#define H_PADDING 6
#define V_PADDING 6

struct _EShellSwitcherPrivate {
	GList *proxies;
	gboolean style_set;
	GtkToolbarStyle style;
	gboolean toolbar_visible;
};

/* the image and the label of a switcher button */
#define BUTTON_IMAGE_KEY "e-shell-switcher-image"
#define BUTTON_LABEL_KEY "e-shell-switcher-label"

enum {
	PROP_0,
	PROP_TOOLBAR_STYLE,
	PROP_TOOLBAR_VISIBLE
};

enum {
	STYLE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EShellSwitcher, e_shell_switcher, E_GTK_TYPE_BIN,
	G_ADD_PRIVATE (EShellSwitcher)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

/* GTK 4 has no tool items; a button shows its parts by the style */
static void
shell_switcher_style_button (GtkWidget *button,
			     GtkToolbarStyle style)
{
	GtkWidget *image = g_object_get_data (G_OBJECT (button), BUTTON_IMAGE_KEY);
	GtkWidget *label = g_object_get_data (G_OBJECT (button), BUTTON_LABEL_KEY);
	GtkWidget *box = gtk_button_get_child (GTK_BUTTON (button));

	gtk_widget_set_visible (image, style != GTK_TOOLBAR_TEXT);
	gtk_widget_set_visible (label, style != GTK_TOOLBAR_ICONS);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (box),
		style == GTK_TOOLBAR_BOTH ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_halign (box, style == GTK_TOOLBAR_BOTH_HORIZ ? GTK_ALIGN_START : GTK_ALIGN_CENTER);
}

static gint
shell_switcher_layout_actions (EShellSwitcher *switcher,
			       gint width,
			       gint height)
{
	GtkAllocation allocation = { 0, 0, width, height };
	gint num_btns = g_list_length (switcher->priv->proxies), btns_per_row;
	GList **rows, *p;
	gboolean icons_only;
	gint row_number;
	gint max_width = 0;
	gint max_height = 0;
	gint row_last;
	gint x, y;
	gint i;

	y = allocation.y + allocation.height;

	if (num_btns == 0)
		return allocation.height;

	icons_only = (switcher->priv->style == GTK_TOOLBAR_ICONS);

	/* Figure out the max width and height. */
	for (p = switcher->priv->proxies; p != NULL; p = p->next) {
		GtkWidget *widget = p->data;
		GtkRequisition requisition;

		gtk_widget_get_preferred_size (widget, &requisition, NULL);
		max_height = MAX (max_height, requisition.height);
		max_width = MAX (max_width, requisition.width);
	}

	/* Figure out how many rows and columns we'll use. */
	btns_per_row = MAX (1, allocation.width / (max_width + H_PADDING));
	if (!icons_only) {
		/* If using text buttons, we want to try to have a
		 * completely filled-in grid, but if we can't, we want
		 * the odd row to have just a single button. */
		while (btns_per_row > 0 && num_btns % btns_per_row > 1)
			btns_per_row--;
	}

	if (btns_per_row <= 0)
		btns_per_row = 1;

	/* Assign buttons to rows. */
	rows = g_new0 (GList *, num_btns / btns_per_row + 1);

	if (!icons_only && num_btns % btns_per_row != 0 && switcher->priv->proxies) {
		rows[0] = g_list_append (rows[0], switcher->priv->proxies->data);

		p = switcher->priv->proxies->next;
		row_number = p ? 1 : 0;
	} else {
		p = switcher->priv->proxies;
		row_number = 0;
	}

	for (; p != NULL; p = p->next) {
		GtkWidget *widget = p->data;

		if (g_list_length (rows[row_number]) == btns_per_row)
			row_number++;

		rows[row_number] = g_list_append (rows[row_number], widget);
	}

	row_last = row_number;

	/* Layout the buttons. */
	for (i = row_last; i >= 0; i--) {
		gint len, extra_width;

		x = H_PADDING + allocation.x;
		y -= max_height + V_PADDING;
		len = g_list_length (rows[i]);
		if (!icons_only)
			extra_width =
				(allocation.width - (len * max_width) -
				(len * H_PADDING + H_PADDING)) / len;
		else
			extra_width = 0;
		for (p = rows[i]; p != NULL; p = p->next) {
			GtkAllocation child_allocation;

			child_allocation.x = x;
			child_allocation.y = y;
			child_allocation.width = max_width + extra_width;
			child_allocation.height = max_height;

			gtk_widget_size_allocate (
				GTK_WIDGET (p->data), &child_allocation, -1);

			x += child_allocation.width + H_PADDING;
		}
	}

	for (i = 0; i <= row_last; i++)
		g_list_free (rows[i]);
	g_free (rows);

	return y - allocation.y - V_PADDING;
}

static void
shell_switcher_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TOOLBAR_STYLE:
			e_shell_switcher_set_style (
				E_SHELL_SWITCHER (object),
				g_value_get_enum (value));
			return;

		case PROP_TOOLBAR_VISIBLE:
			e_shell_switcher_set_visible (
				E_SHELL_SWITCHER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_switcher_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TOOLBAR_STYLE:
			g_value_set_enum (
				value, e_shell_switcher_get_style (
				E_SHELL_SWITCHER (object)));
			return;

		case PROP_TOOLBAR_VISIBLE:
			g_value_set_boolean (
				value, e_shell_switcher_get_visible (
				E_SHELL_SWITCHER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_switcher_dispose (GObject *object)
{
	EShellSwitcher *self = E_SHELL_SWITCHER (object);

	while (self->priv->proxies != NULL) {
		GtkWidget *widget = self->priv->proxies->data;
		e_gtk_container_remove (object, widget);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_switcher_parent_class)->dispose (object);
}

static void
shell_switcher_get_preferred_width (GtkWidget *widget,
                                    gint *minimum,
                                    gint *natural)
{
	EShellSwitcher *self = E_SHELL_SWITCHER (widget);
	GtkWidget *child;
	GList *iter;

	*minimum = *natural = 0;

	child = e_gtk_bin_get_child (widget);
	if (child != NULL)
		e_gtk_widget_get_preferred_width (child, minimum, natural);

	if (!self->priv->toolbar_visible)
		return;

	for (iter = self->priv->proxies; iter != NULL; iter = iter->next) {
		GtkWidget *widget_proxy = iter->data;
		gint child_min, child_nat;

		e_gtk_widget_get_preferred_width (widget_proxy, &child_min, &child_nat);

		child_min += H_PADDING;
		child_nat += H_PADDING;

		*minimum = MAX (*minimum, child_min);
		*natural = MAX (*natural, child_nat);
	}
}

static void
shell_switcher_get_preferred_height (GtkWidget *switcher_widget,
                                     gint *minimum,
                                     gint *natural)
{
	EShellSwitcher *self = E_SHELL_SWITCHER (switcher_widget);
	GtkWidget *child;
	GList *iter;

	*minimum = *natural = 0;

	child = e_gtk_bin_get_child (switcher_widget);
	if (child != NULL)
		e_gtk_widget_get_preferred_height (child, minimum, natural);

	if (!self->priv->toolbar_visible)
		return;

	for (iter = self->priv->proxies; iter != NULL; iter = iter->next) {
		GtkWidget *widget = iter->data;
		gint child_min, child_nat;

		e_gtk_widget_get_preferred_height (widget, &child_min, &child_nat);

		child_min += V_PADDING;
		child_nat += V_PADDING;

		*minimum += child_min;
		*natural += child_nat;
	}
}

static void
shell_switcher_size_allocate (GtkWidget *widget,
                              gint width,
                              gint height,
                              gint baseline)
{
	EShellSwitcher *switcher;
	GtkAllocation child_allocation;
	GtkWidget *child;
	gint child_height;

	switcher = E_SHELL_SWITCHER (widget);

	if (switcher->priv->toolbar_visible)
		child_height = shell_switcher_layout_actions (switcher, width, height);
	else
		child_height = height;

	child_allocation.x = 0;
	child_allocation.y = 0;
	child_allocation.width = width;
	child_allocation.height = child_height;

	child = e_gtk_bin_get_child (widget);
	if (child != NULL)
		gtk_widget_size_allocate (child, &child_allocation, -1);
}

static void
shell_switcher_remove (GtkWidget *container,
                       GtkWidget *remove_widget)
{
	EShellSwitcher *self = E_SHELL_SWITCHER (container);
	GList *link;

	/* Look in the internal widgets first. */

	link = g_list_find (self->priv->proxies, remove_widget);
	if (link != NULL) {
		GtkWidget *widget = link->data;

		gtk_widget_unparent (widget);
		self->priv->proxies = g_list_delete_link (self->priv->proxies, link);
		gtk_widget_queue_resize (GTK_WIDGET (container));
		return;
	}

	/* Chain up to parent's remove() method. */
	E_GTK_BIN_CLASS (e_shell_switcher_parent_class)->remove (container, remove_widget);
}

static void
shell_switcher_style_changed (EShellSwitcher *switcher,
                              GtkToolbarStyle style)
{
	if (switcher->priv->style == style)
		return;

	switcher->priv->style = style;

	g_list_foreach (
		switcher->priv->proxies,
		(GFunc) shell_switcher_style_button, GINT_TO_POINTER (style));

	gtk_widget_queue_resize (GTK_WIDGET (switcher));
	g_object_notify (G_OBJECT (switcher), "toolbar-style");
}

static void
e_shell_switcher_measure (GtkWidget *widget,
		GtkOrientation orientation,
		gint for_size,
		gint *minimum,
		gint *natural,
		gint *minimum_baseline,
		gint *natural_baseline)
{
	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		shell_switcher_get_preferred_width (widget, minimum, natural);
		return;
	}

	if (orientation == GTK_ORIENTATION_VERTICAL) {
		shell_switcher_get_preferred_height (widget, minimum, natural);
		return;
	}

	GTK_WIDGET_CLASS (e_shell_switcher_parent_class)->measure (widget, orientation, for_size,
		minimum, natural, minimum_baseline, natural_baseline);
}

static void
e_shell_switcher_class_init (EShellSwitcherClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	EGtkBinClass *bin_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_switcher_set_property;
	object_class->get_property = shell_switcher_get_property;
	object_class->dispose = shell_switcher_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->measure = e_shell_switcher_measure;
	widget_class->size_allocate = shell_switcher_size_allocate;

	bin_class = E_GTK_BIN_CLASS (class);
	bin_class->remove = shell_switcher_remove;

	class->style_changed = shell_switcher_style_changed;

	/**
	 * EShellSwitcher:toolbar-style
	 *
	 * The switcher's toolbar style.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_STYLE,
		g_param_spec_enum (
			"toolbar-style",
			"Toolbar Style",
			"The switcher's toolbar style",
			GTK_TYPE_TOOLBAR_STYLE,
			E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellSwitcher:toolbar-visible
	 *
	 * Whether the switcher is visible.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_TOOLBAR_VISIBLE,
		g_param_spec_boolean (
			"toolbar-visible",
			"Toolbar Visible",
			"Whether the switcher is visible",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShellSwitcher::style-changed
	 * @switcher: the #EShellSwitcher which emitted the signal
	 * @style: the new #GtkToolbarStyle of the switcher
	 *
	 * Emitted when the style of the switcher changes.
	 **/
	signals[STYLE_CHANGED] = g_signal_new (
		"style-changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellSwitcherClass, style_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__ENUM,
		G_TYPE_NONE, 1,
		GTK_TYPE_TOOLBAR_STYLE);
}

static void
e_shell_switcher_init (EShellSwitcher *switcher)
{
	switcher->priv = e_shell_switcher_get_instance_private (switcher);


	e_extensible_load_extensions (E_EXTENSIBLE (switcher));
}

/**
 * e_shell_switcher_new:
 *
 * Creates a new #EShellSwitcher instance.
 *
 * Returns: a new #EShellSwitcher instance
 **/
GtkWidget *
e_shell_switcher_new (void)
{
	return g_object_new (E_TYPE_SHELL_SWITCHER, NULL);
}

/* A middle click, or a shift click, opens a new window of the view */
static void
switcher_button_pressed_cb (GtkGestureClick *gesture,
			    gint n_press,
			    gdouble x,
			    gdouble y,
			    EUIAction *action)
{
	GdkModifierType my_mods = GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK |
		GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_META_MASK;
	GdkModifierType state;
	guint button;

	g_return_if_fail (E_IS_UI_ACTION (action));

	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
	state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

	if (button == 2 || (button == 1 && (state & my_mods) == GDK_SHIFT_MASK)) {
		GVariant *target;

		gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

		target = e_ui_action_ref_target (action);
		g_action_activate (G_ACTION (action), target);
		g_clear_pointer (&target, g_variant_unref);
	}
}

/**
 * e_shell_switcher_add_action:
 * @switcher: an #EShellSwitcher
 * @switch_action: an #EUIAction
 * @new_window_action: an #EUIAction
 *
 * Adds a button to @switcher that proxies for @switcher_action.
 * Switcher buttons appear in the order they were added. A middle
 * click opens a new window of this type.
 *
 * #EShellWindow adds switcher actions in the order given by the
 * <structfield>sort_order</structfield> field in #EShellBackendClass.
 **/
void
e_shell_switcher_add_action (EShellSwitcher *switcher,
                             EUIAction *switch_action,
                             EUIAction *new_window_action)
{
	GtkWidget *widget, *box, *image, *label;
	GtkGesture *gesture;
	GSettings *settings;
	GVariant *target;
	const gchar *view_name;
	gchar **strv;
	gint ii;
	gboolean skip = FALSE;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));
	g_return_if_fail (E_IS_UI_ACTION (switch_action));
	g_return_if_fail (E_IS_UI_ACTION (new_window_action));

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	strv = g_settings_get_strv (settings, "buttons-hide");
	g_clear_object (&settings);

	target = e_ui_action_ref_target (switch_action);
	view_name = g_variant_get_string (target, NULL);

	for (ii = 0; strv && strv[ii] && !skip; ii++) {
		skip = g_strcmp0 (view_name, strv[ii]) == 0;
	}

	g_clear_pointer (&target, g_variant_unref);
	g_strfreev (strv);

	if (skip)
		return;

	g_object_ref (switch_action);

	image = gtk_image_new ();
	label = gtk_label_new (NULL);
	gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_append (GTK_BOX (box), image);
	gtk_box_append (GTK_BOX (box), label);

	widget = gtk_toggle_button_new ();
	gtk_button_set_child (GTK_BUTTON (widget), box);
	g_object_set_data (G_OBJECT (widget), BUTTON_IMAGE_KEY, image);
	g_object_set_data (G_OBJECT (widget), BUTTON_LABEL_KEY, label);
	shell_switcher_style_button (widget, switcher->priv->style);

	e_binding_bind_property (
		switch_action, "label",
		label, "label",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		switch_action, "icon-name",
		image, "icon-name",
		G_BINDING_SYNC_CREATE);

	gtk_widget_set_tooltip_text (widget, e_ui_action_get_tooltip (switch_action));

	/* the action state keeps the button of the current view pushed */
	e_ui_action_util_assign_to_widget (switch_action, widget);

	gesture = gtk_gesture_click_new ();
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
	g_signal_connect_object (gesture, "pressed",
		G_CALLBACK (switcher_button_pressed_cb), new_window_action, 0);
	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));

	gtk_widget_set_visible (widget, switcher->priv->toolbar_visible);

	switcher->priv->proxies = g_list_append (
		switcher->priv->proxies, widget);

	gtk_widget_set_parent (widget, GTK_WIDGET (switcher));
	gtk_widget_queue_resize (GTK_WIDGET (switcher));
}

/**
 * e_shell_switcher_get_style:
 * @switcher: an #EShellSwitcher
 *
 * Returns whether @switcher has text, icons or both.
 *
 * Returns: the current style of @shell
 **/
GtkToolbarStyle
e_shell_switcher_get_style (EShellSwitcher *switcher)
{
	g_return_val_if_fail (
		E_IS_SHELL_SWITCHER (switcher),
		E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE);

	return switcher->priv->style;
}

/**
 * e_shell_switcher_set_style:
 * @switcher: an #EShellSwitcher
 * @style: the new style for @switcher
 *
 * Alters the view of @switcher to display either icons only, text only,
 * or both.
 **/
void
e_shell_switcher_set_style (EShellSwitcher *switcher,
                            GtkToolbarStyle style)
{
	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	switcher->priv->style_set = TRUE;
	g_signal_emit (switcher, signals[STYLE_CHANGED], 0, style);
}

/**
 * e_shell_switcher_unset_style:
 * @switcher: an #EShellSwitcher
 *
 * Unsets a switcher style set with e_shell_switcher_set_style(), so
 * that user preferences will be used to determine the switcher style.
 **/
void
e_shell_switcher_unset_style (EShellSwitcher *switcher)
{
	GtkToolbarStyle style;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	if (!switcher->priv->style_set)
		return;

	/* GTK 4 has no toolbar style setting */
	style = E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE;

	if (style != switcher->priv->style)
		g_signal_emit (switcher, signals[STYLE_CHANGED], 0, style);

	switcher->priv->style_set = FALSE;
}

/**
 * e_shell_switcher_get_visible:
 * @switcher: an #EShellSwitcher
 *
 * Returns %TRUE if the switcher buttons are visible.
 *
 * Note that switcher button visibility is different than
 * @switcher<!-- -->'s GTK_VISIBLE flag, since #EShellSwitcher
 * is actually a container widget for #EShellSidebar.
 *
 * Returns: %TRUE if the switcher buttons are visible
 **/
gboolean
e_shell_switcher_get_visible (EShellSwitcher *switcher)
{
	g_return_val_if_fail (E_IS_SHELL_SWITCHER (switcher), FALSE);

	return switcher->priv->toolbar_visible;
}

/**
 * e_shell_switcher_set_visible:
 * @switcher: an #EShellSwitcher
 * @visible: whether the switcher buttons should be visible
 *
 * Sets the switcher button visiblity to @visible.
 *
 * Note that switcher button visibility is different than
 * @switcher<!-- -->'s GTK_VISIBLE flag, since #EShellSwitcher
 * is actually a container widget for #EShellSidebar.
 **/
void
e_shell_switcher_set_visible (EShellSwitcher *switcher,
                              gboolean visible)
{
	GList *iter;

	g_return_if_fail (E_IS_SHELL_SWITCHER (switcher));

	if (switcher->priv->toolbar_visible == visible)
		return;

	switcher->priv->toolbar_visible = visible;

	for (iter = switcher->priv->proxies; iter != NULL; iter = iter->next)
		g_object_set (iter->data, "visible", visible, NULL);

	gtk_widget_queue_resize (GTK_WIDGET (switcher));

	g_object_notify (G_OBJECT (switcher), "toolbar-visible");
}
