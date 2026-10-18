/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2022 Cédric Bellegarde <cedric.bellegarde@adishatz.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-menu-bar.h"

struct _EMenuBarPrivate {
	GtkWidget *inner_menu_bar;
	GtkWidget *menu_button;

	guint visible : 1;
	gulong delayed_show_id;
	gulong delayed_hide_id;
};

G_DEFINE_TYPE_WITH_CODE (EMenuBar, e_menu_bar, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EMenuBar))

enum {
	PROP_0,
	PROP_VISIBLE
};

static void
menu_bar_visible_settings_changed_cb (GSettings *settings,
				      const gchar *key,
				      gpointer data)
{
	g_return_if_fail (E_IS_MENU_BAR (data));

	e_menu_bar_set_visible (
		E_MENU_BAR (data),
		g_settings_get_boolean (settings, key));
}

static void
menu_bar_set_property (GObject *object,
		       guint property_id,
		       const GValue *value,
		       GParamSpec *pspec)
{
	EMenuBar *menubar = E_MENU_BAR (object);

	switch (property_id) {
		case PROP_VISIBLE:
			e_menu_bar_set_visible (
				menubar,
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
menu_bar_get_property (GObject *object,
		       guint property_id,
		       GValue *value,
		       GParamSpec *pspec)
{
	EMenuBar *menubar = E_MENU_BAR (object);

	switch (property_id) {
		case PROP_VISIBLE:
			g_value_set_boolean (
				value, e_menu_bar_get_visible (menubar));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
menu_bar_dispose (GObject *menu_bar)
{
	EMenuBar *self = E_MENU_BAR (menu_bar);

	if (self->priv->delayed_show_id) {
		g_source_remove (self->priv->delayed_show_id);
		self->priv->delayed_show_id = 0;
	}

	if (self->priv->delayed_hide_id) {
		g_source_remove (self->priv->delayed_hide_id);
		self->priv->delayed_hide_id = 0;
	}

	g_clear_object (&self->priv->inner_menu_bar);
	g_clear_object (&self->priv->menu_button);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_menu_bar_parent_class)->dispose (menu_bar);
}

static void
e_menu_bar_class_init (EMenuBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = menu_bar_set_property;
	object_class->get_property = menu_bar_get_property;
	object_class->dispose = menu_bar_dispose;

	g_object_class_install_property (
		object_class,
		PROP_VISIBLE,
		g_param_spec_boolean (
			"visible",
			"Visible",
			"Inner menubar visible",
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_menu_bar_init (EMenuBar *self)
{
	self->priv = e_menu_bar_get_instance_private (self);
}

static gboolean
delayed_show_cb (gpointer user_data)
{
	EMenuBar *self = user_data;

	self->priv->delayed_show_id = 0;

	if (!self->priv->visible) {
		gtk_widget_set_visible (GTK_WIDGET (self->priv->inner_menu_bar), TRUE);
	}

	return FALSE;
}

static gboolean
delayed_hide_cb (gpointer user_data)
{
	EMenuBar *self = user_data;
	GtkWidget *widget = self->priv->inner_menu_bar;

	self->priv->delayed_hide_id = 0;

	/* GTK 4 cannot tell whether a menu of the bar is open, thus keep
	 * the bar while it has the focus */
	if (!self->priv->visible && widget &&
	    !self->priv->delayed_show_id) {
		if (gtk_widget_get_visible (widget) &&
		    !e_gtk_widget_has_focus (widget) &&
		    !gtk_widget_get_focus_child (widget))
			gtk_widget_set_visible (widget, FALSE);
	}

	return FALSE;
}

static void
menu_bar_schedule_hide (EMenuBar *self)
{
	if (self->priv->delayed_show_id) {
		g_source_remove (self->priv->delayed_show_id);
		self->priv->delayed_show_id = 0;
	}

	if (gtk_widget_get_visible (GTK_WIDGET (self->priv->inner_menu_bar)) &&
	    !self->priv->delayed_hide_id) {
		self->priv->delayed_hide_id =
			g_timeout_add (500, delayed_hide_cb, self);
	}
}

/* GTK 3 watched "event-after" of the window; GTK 4 needs controllers */
static gboolean
menu_bar_window_key_pressed_cb (GtkEventControllerKey *controller,
				guint keyval,
				guint keycode,
				GdkModifierType state,
				EMenuBar *self)
{
	gboolean has_modifier_pressed;

	has_modifier_pressed = (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
				GDK_SUPER_MASK | GDK_HYPER_MASK |
				GDK_META_MASK)) != 0;

	if ((keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R) &&
	    !has_modifier_pressed) {
		if (self->priv->delayed_hide_id) {
			g_source_remove (self->priv->delayed_hide_id);
			self->priv->delayed_hide_id = 0;
		}

		if (self->priv->delayed_show_id) {
			g_source_remove (self->priv->delayed_show_id);
			self->priv->delayed_show_id = 0;

			delayed_show_cb (self);
		} else {
			/* To not flash when using Alt+Tab or
			   similar system-wide shortcuts */
			self->priv->delayed_show_id =
				g_timeout_add (250, delayed_show_cb, self);
		}
	} else if (keyval == GDK_KEY_F10 && !has_modifier_pressed && self->priv->menu_button &&
		   gtk_widget_get_visible (self->priv->menu_button)) {
		gtk_menu_button_popup (GTK_MENU_BUTTON (self->priv->menu_button));
		return TRUE;
	} else {
		menu_bar_schedule_hide (self);
	}

	return FALSE;
}

static void
menu_bar_window_key_released_cb (GtkEventControllerKey *controller,
				 guint keyval,
				 guint keycode,
				 GdkModifierType state,
				 EMenuBar *self)
{
	if (keyval != GDK_KEY_Alt_L && keyval != GDK_KEY_Alt_R)
		menu_bar_schedule_hide (self);
}

static void
menu_bar_window_released_cb (GtkGestureClick *gesture,
			     gint n_press,
			     gdouble x,
			     gdouble y,
			     EMenuBar *self)
{
	GdkModifierType state;

	state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

	if (!(state & GDK_ALT_MASK))
		menu_bar_schedule_hide (self);
}

EMenuBar *
e_menu_bar_new (GtkWidget *inner_menu_bar, GtkWindow *window, GtkWidget **out_menu_button)
{
	EMenuBar *self;
	GSettings *settings;
	GtkEventController *controller;

	g_return_val_if_fail (GTK_IS_POPOVER_MENU_BAR (inner_menu_bar), NULL);
	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	self = g_object_new (E_TYPE_MENU_BAR, NULL);
	self->priv->inner_menu_bar = GTK_WIDGET (g_object_ref_sink (inner_menu_bar));

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_signal_connect_object (
		settings, "changed::menubar-visible",
		G_CALLBACK (menu_bar_visible_settings_changed_cb),
		self, 0);
	e_menu_bar_set_visible (
		self,
		g_settings_get_boolean (settings, "menubar-visible"));

	if (out_menu_button) {
		GtkWidget *button;

		button = gtk_menu_button_new ();
		gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (button), "open-menu");
		gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button),
			gtk_popover_menu_bar_get_menu_model (GTK_POPOVER_MENU_BAR (inner_menu_bar)));

		g_settings_bind (
			settings, "menubar-visible",
			button, "visible",
			G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN | G_SETTINGS_BIND_NO_SENSITIVITY);

		self->priv->menu_button = g_object_ref_sink (button);

		*out_menu_button = button;
	}

	g_object_unref (settings);

	controller = gtk_event_controller_key_new ();
	gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
	g_signal_connect_object (controller, "key-pressed",
		G_CALLBACK (menu_bar_window_key_pressed_cb), self, 0);
	g_signal_connect_object (controller, "key-released",
		G_CALLBACK (menu_bar_window_key_released_cb), self, 0);
	gtk_widget_add_controller (GTK_WIDGET (window), controller);

	controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
	gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
	g_signal_connect_object (controller, "released",
		G_CALLBACK (menu_bar_window_released_cb), self, 0);
	gtk_widget_add_controller (GTK_WIDGET (window), controller);

	return self;
}

/**
 * e_menu_bar_get_visible:
 * @self: an #EMenuBar
 *
 * Determines whether the inner menu bar is visible.
 *
 * Returns: %TRUE if the inner menu bar is visible
 *
 * Since: 3.46
 **/
gboolean
e_menu_bar_get_visible (EMenuBar *self)
{
	g_return_val_if_fail (E_IS_MENU_BAR (self), FALSE);

	return self->priv->visible;
}

/**
 * e_menu_bar_set_visible:
 * @self: an #EMenuBar
 * @visible: whether the inner menu bar should be shown or not
 *
 * Sets the visibility state of the inner menu bar.
 *
 * Since: 3.46
 **/
void
e_menu_bar_set_visible (EMenuBar *menu_bar,
			gboolean visible)
{
	g_return_if_fail (E_IS_MENU_BAR (menu_bar));

	menu_bar->priv->visible = visible;
	gtk_widget_set_visible (GTK_WIDGET (menu_bar->priv->inner_menu_bar), visible);
}
