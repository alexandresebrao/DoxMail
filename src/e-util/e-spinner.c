/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
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
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-spinner.h"

/* GTK 4 has a spinner of its own; this keeps the ESpinner API on top of it */

struct _ESpinnerPrivate {
	GtkWidget *spinner;
};

enum {
	PROP_0,
	PROP_ACTIVE
};

G_DEFINE_TYPE_WITH_PRIVATE (ESpinner, e_spinner, GTK_TYPE_WIDGET)

static void
e_spinner_set_property (GObject *object,
			guint property_id,
			const GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE:
			e_spinner_set_active (
				E_SPINNER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_spinner_get_property (GObject *object,
			guint property_id,
			GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE:
			g_value_set_boolean (
				value,
				e_spinner_get_active (E_SPINNER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_spinner_dispose (GObject *object)
{
	ESpinner *spinner = E_SPINNER (object);

	g_clear_pointer (&spinner->priv->spinner, gtk_widget_unparent);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_spinner_parent_class)->dispose (object);
}

static void
e_spinner_class_init (ESpinnerClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_spinner_set_property;
	object_class->get_property = e_spinner_get_property;
	object_class->dispose = e_spinner_dispose;

	gtk_widget_class_set_layout_manager_type (GTK_WIDGET_CLASS (klass), GTK_TYPE_BIN_LAYOUT);

	/**
	 * ESpinner:active:
	 *
	 * Whether the animation is active.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ACTIVE,
		g_param_spec_boolean (
			"active",
			"Active",
			"Whether the animation is active",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_spinner_init (ESpinner *spinner)
{
	spinner->priv = e_spinner_get_instance_private (spinner);

	spinner->priv->spinner = gtk_spinner_new ();
	gtk_widget_set_parent (spinner->priv->spinner, GTK_WIDGET (spinner));
}

GtkWidget *
e_spinner_new (void)
{
	return g_object_new (E_TYPE_SPINNER, NULL);
}

gboolean
e_spinner_get_active (ESpinner *spinner)
{
	g_return_val_if_fail (E_IS_SPINNER (spinner), FALSE);

	return gtk_spinner_get_spinning (GTK_SPINNER (spinner->priv->spinner));
}

void
e_spinner_set_active (ESpinner *spinner,
		      gboolean active)
{
	g_return_if_fail (E_IS_SPINNER (spinner));

	if ((active ? 1 : 0) == (e_spinner_get_active (spinner) ? 1 : 0))
		return;

	gtk_spinner_set_spinning (GTK_SPINNER (spinner->priv->spinner), active);

	g_object_notify (G_OBJECT (spinner), "active");
}

void
e_spinner_start (ESpinner *spinner)
{
	e_spinner_set_active (spinner, TRUE);
}

void
e_spinner_stop (ESpinner *spinner)
{
	e_spinner_set_active (spinner, FALSE);
}
