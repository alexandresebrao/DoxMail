
/* e-color-chooser-widget.c
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

#include "e-color-chooser-widget.h"

#include <glib/gi18n-lib.h>

/**
 * EColorChooserWidget:
 *
 * This widget wraps a #GtkColorChooserWidget and allows the widget to be
 * used as a delegate for #GtkComboBox for instance.
 *
 * GtkColorChooserWidget is final with GTK 4, so the inner chooser is a
 * child and the #GtkColorChooser interface is forwarded to it.
 */

struct _EColorChooserWidgetPrivate {
	GtkWidget *chooser;
	GtkWidget *pressed_swatch;
	gboolean showing_editor;
};

enum {
	PROP_0,
	PROP_RGBA,
	PROP_USE_ALPHA,
	PROP_SHOW_EDITOR
};

enum {
	SIGNAL_EDITOR_ACTIVATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void	e_color_chooser_widget_color_chooser_init
						(GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EColorChooserWidget, e_color_chooser_widget, GTK_TYPE_WIDGET,
	G_ADD_PRIVATE (EColorChooserWidget)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER, e_color_chooser_widget_color_chooser_init))

/* GtkColorChooserWidget sends "color-activated" only when the user
 * double-clicks a color, but in a combobox-like widget a single click
 * should do.  The swatches are private widgets, so watch clicks on the
 * chooser in the capture phase: a swatch that is selected once the
 * button is released was the one clicked. */
static GtkWidget *
color_chooser_widget_find_swatch (GtkWidget *widget,
				  GtkWidget *stop)
{
	while (widget && widget != stop) {
		if (g_strcmp0 (G_OBJECT_TYPE_NAME (widget), "GtkColorSwatch") == 0)
			return widget;
		widget = gtk_widget_get_parent (widget);
	}

	return NULL;
}

static void
color_chooser_widget_pressed_cb (GtkGestureClick *gesture,
				 gint n_press,
				 gdouble x,
				 gdouble y,
				 EColorChooserWidget *self)
{
	GtkWidget *picked;

	picked = gtk_widget_pick (self->priv->chooser, x, y, GTK_PICK_DEFAULT);
	self->priv->pressed_swatch = n_press == 1 ?
		color_chooser_widget_find_swatch (picked, self->priv->chooser) : NULL;
}

static void
color_chooser_widget_released_cb (GtkGestureClick *gesture,
				  gint n_press,
				  gdouble x,
				  gdouble y,
				  EColorChooserWidget *self)
{
	GtkWidget *swatch = self->priv->pressed_swatch;

	self->priv->pressed_swatch = NULL;

	if (swatch && (gtk_widget_get_state_flags (swatch) & GTK_STATE_FLAG_SELECTED) != 0) {
		GdkRGBA color;

		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->priv->chooser), &color);
		g_signal_emit_by_name (self, "color-activated", &color);
	}
}

static void
color_chooser_widget_inner_activated_cb (GtkColorChooser *chooser,
					 const GdkRGBA *color,
					 EColorChooserWidget *self)
{
	g_signal_emit_by_name (self, "color-activated", color);
}

static gboolean
run_color_chooser_dialog (gpointer user_data)
{
	EColorChooserWidget *self;
	GtkWidget *parent_window;
	GtkWidget *parent_chooser;
	GtkWidget *dialog;
	GtkWidget *chooser;

	parent_chooser = user_data;
	self = E_COLOR_CHOOSER_WIDGET (parent_chooser);

	g_object_set (
		G_OBJECT (self->priv->chooser), "show-editor", FALSE, NULL);

	parent_window = g_object_get_data (G_OBJECT (parent_chooser), "window");
	if (!GTK_IS_WINDOW (parent_window))
		parent_window = e_gtk_widget_get_toplevel (parent_chooser);
	dialog = gtk_dialog_new_with_buttons (
		N_("Choose custom color"),
		GTK_WINDOW (parent_window),
		GTK_DIALOG_MODAL,
		_("_Cancel"), GTK_RESPONSE_REJECT,
		_("_OK"), GTK_RESPONSE_ACCEPT, NULL);

	chooser = gtk_color_chooser_widget_new ();
	g_object_set (G_OBJECT (chooser), "show-editor", TRUE, NULL);
	e_gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), chooser, TRUE, TRUE, 5);

	if (e_gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		GdkRGBA color;

		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (chooser), &color);
		gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (parent_chooser), &color);

		g_signal_emit_by_name (parent_chooser, "color-activated", &color);
	}

	e_gtk_widget_destroy (dialog);

	self->priv->showing_editor = FALSE;

	return FALSE;
}

static void
color_chooser_show_editor_notify_cb (GtkWidget *inner,
                                     GParamSpec *pspec,
                                     EColorChooserWidget *chooser)
{
	gboolean show_editor;

	g_object_get (G_OBJECT (inner), "show-editor", &show_editor, NULL);

	g_object_notify (G_OBJECT (chooser), "show-editor");

	/* Nothing to do here... */
	if ((show_editor == FALSE) || (chooser->priv->showing_editor == TRUE))
		return;

	chooser->priv->showing_editor = TRUE;

	/* Hide the editor - we don't want to display the single-color editor
	 * within this widget. We rather create a dialog window with the editor
	 * (we can't do it from this callback as Gtk would stop it in order to
	 * prevent endless recursion probably) */
	g_idle_add (run_color_chooser_dialog, chooser);
	g_signal_emit (chooser, signals[SIGNAL_EDITOR_ACTIVATED], 0);
}

static void
color_chooser_widget_forward_notify_cb (GObject *inner,
					GParamSpec *pspec,
					GObject *self)
{
	g_object_notify (self, pspec->name);
}

static void
color_chooser_widget_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_RGBA:
		case PROP_USE_ALPHA:
		case PROP_SHOW_EDITOR:
			g_object_set_property (G_OBJECT (E_COLOR_CHOOSER_WIDGET (object)->priv->chooser),
				pspec->name, value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_chooser_widget_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_RGBA:
		case PROP_USE_ALPHA:
		case PROP_SHOW_EDITOR:
			g_object_get_property (G_OBJECT (E_COLOR_CHOOSER_WIDGET (object)->priv->chooser),
				pspec->name, value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
color_chooser_widget_dispose (GObject *object)
{
	EColorChooserWidget *self = E_COLOR_CHOOSER_WIDGET (object);

	g_clear_pointer (&self->priv->chooser, gtk_widget_unparent);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_color_chooser_widget_parent_class)->dispose (object);
}

static gboolean
color_chooser_widget_grab_focus (GtkWidget *widget)
{
	return gtk_widget_grab_focus (E_COLOR_CHOOSER_WIDGET (widget)->priv->chooser);
}

static void
color_chooser_widget_get_rgba (GtkColorChooser *chooser,
			       GdkRGBA *color)
{
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (E_COLOR_CHOOSER_WIDGET (chooser)->priv->chooser), color);
}

static void
color_chooser_widget_set_rgba (GtkColorChooser *chooser,
			       const GdkRGBA *color)
{
	gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (E_COLOR_CHOOSER_WIDGET (chooser)->priv->chooser), color);
}

static void
color_chooser_widget_add_palette (GtkColorChooser *chooser,
				  GtkOrientation orientation,
				  gint colors_per_line,
				  gint n_colors,
				  GdkRGBA *colors)
{
	gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (E_COLOR_CHOOSER_WIDGET (chooser)->priv->chooser),
		orientation, colors_per_line, n_colors, colors);
}

static void
e_color_chooser_widget_color_chooser_init (GtkColorChooserInterface *iface)
{
	iface->get_rgba = color_chooser_widget_get_rgba;
	iface->set_rgba = color_chooser_widget_set_rgba;
	iface->add_palette = color_chooser_widget_add_palette;
}

static void
e_color_chooser_widget_class_init (EColorChooserWidgetClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = color_chooser_widget_set_property;
	object_class->get_property = color_chooser_widget_get_property;
	object_class->dispose = color_chooser_widget_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->grab_focus = color_chooser_widget_grab_focus;
	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

	g_object_class_override_property (object_class, PROP_RGBA, "rgba");
	g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");

	g_object_class_install_property (
		object_class,
		PROP_SHOW_EDITOR,
		g_param_spec_boolean (
			"show-editor",
			NULL, NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	signals[SIGNAL_EDITOR_ACTIVATED] = g_signal_new (
		"editor-activated",
		E_TYPE_COLOR_CHOOSER_WIDGET,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EColorChooserWidgetClass, editor_activated),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_color_chooser_widget_init (EColorChooserWidget *widget)
{
	GtkGesture *gesture;

	widget->priv = e_color_chooser_widget_get_instance_private (widget);
	widget->priv->showing_editor = FALSE;

	widget->priv->chooser = gtk_color_chooser_widget_new ();
	gtk_widget_set_parent (widget->priv->chooser, GTK_WIDGET (widget));

	gesture = gtk_gesture_click_new ();
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
	g_signal_connect (gesture, "pressed", G_CALLBACK (color_chooser_widget_pressed_cb), widget);
	g_signal_connect (gesture, "released", G_CALLBACK (color_chooser_widget_released_cb), widget);
	gtk_widget_add_controller (widget->priv->chooser, GTK_EVENT_CONTROLLER (gesture));

	g_signal_connect (
		widget->priv->chooser, "color-activated",
		G_CALLBACK (color_chooser_widget_inner_activated_cb), widget);

	g_signal_connect (
		widget->priv->chooser, "notify::show-editor",
		G_CALLBACK (color_chooser_show_editor_notify_cb), widget);

	g_signal_connect (
		widget->priv->chooser, "notify::rgba",
		G_CALLBACK (color_chooser_widget_forward_notify_cb), widget);
}

GtkWidget *
e_color_chooser_widget_new (void)
{
	return g_object_new (
		E_TYPE_COLOR_CHOOSER_WIDGET,
		"show-editor", FALSE,
		"use-alpha", FALSE,
		NULL);
}
