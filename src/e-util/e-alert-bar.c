/*
 * e-alert-bar.c
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-dialog-widgets.h"
#include "e-alert-dialog.h"
#include "e-alert-bar.h"

/* Dismiss warnings automatically after 5 minutes. */
#define WARNING_TIMEOUT_SECONDS (5 * 60)

/* Maximum height of the text message; after that value the message is scrolled. */
#define MAX_HEIGHT 200

/* GtkInfoBar cannot be subclassed with GTK 4, thus the bar is a box
 * holding one, and the buttons are in an own box, because the info bar
 * does not give its action area anymore. */
struct _EAlertBarPrivate {
	GQueue alerts;
	GtkWidget *info_bar;		/* not referenced */
	GtkWidget *action_area;		/* not referenced */
	GtkWidget *image;		/* not referenced */
	GtkWidget *scrolled_window;	/* not referenced */
	GtkWidget *message_label;	/* not referenced */
	gint max_content_height;
};

G_DEFINE_TYPE_WITH_PRIVATE (EAlertBar, e_alert_bar, GTK_TYPE_BOX)

static void
alert_bar_clear_action_area (EAlertBar *alert_bar)
{
	GtkWidget *child;

	while ((child = gtk_widget_get_first_child (alert_bar->priv->action_area)) != NULL)
		gtk_box_remove (GTK_BOX (alert_bar->priv->action_area), child);
}

static void
alert_bar_response_close (EAlert *alert)
{
	e_alert_response (alert, GTK_RESPONSE_CLOSE);
}

static void
alert_bar_show_alert (EAlertBar *alert_bar)
{
	GtkImage *image;
	GtkInfoBar *info_bar;
	GtkWidget *action_area;
	GtkWidget *widget;
	EAlert *alert;
	GList *link;
	GtkMessageType message_type;
	const gchar *primary_text;
	const gchar *secondary_text;
	const gchar *icon_name;
	gboolean have_primary_text;
	gboolean have_secondary_text;
	gboolean visible;
	guint n_alerts;
	gchar *markup;

	info_bar = GTK_INFO_BAR (alert_bar->priv->info_bar);
	action_area = alert_bar->priv->action_area;

	alert = g_queue_peek_head (&alert_bar->priv->alerts);
	g_return_if_fail (E_IS_ALERT (alert));

	/* Remove all buttons from the previous alert. */
	alert_bar_clear_action_area (alert_bar);

	/* Add alert-specific buttons. */
	link = e_alert_peek_actions (alert);
	while (link != NULL) {
		EUIAction *action = E_UI_ACTION (link->data);

		/* These actions are already wired to trigger an
		 * EAlert::response signal when activated, so
		 * we can add buttons directly to the action
		 * area without knowning their response IDs. */

		widget = e_alert_create_button_for_action (action);

		e_gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);

		link = g_list_next (link);
	}

	link = e_alert_peek_widgets (alert);
	while (link != NULL) {
		widget = link->data;

		e_gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);
		link = g_list_next (link);
	}

	/* Add a dismiss button. */
	widget = e_dialog_button_new_with_icon ("window-close", NULL);
	gtk_button_set_has_frame (GTK_BUTTON (widget), FALSE);
	gtk_widget_set_tooltip_text (
		/* Translators: Escape is a keyboard binding. */
		widget, _("Close this message (Escape)"));
	e_gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (alert_bar_response_close), alert);

	n_alerts = g_queue_get_length (&alert_bar->priv->alerts);

	if (n_alerts > 1) {
		gchar *str;

		/* Translators: there are always at least two messages to be closed */
		str = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "Close a message", "Close all %u messages", n_alerts), n_alerts);

		widget = e_dialog_button_new_with_icon ("edit-clear-all", NULL);
		gtk_button_set_has_frame (GTK_BUTTON (widget), FALSE);
		gtk_widget_set_tooltip_text (widget, str);
		e_gtk_box_pack_end (GTK_BOX (action_area), widget, FALSE, FALSE, 0);
		gtk_widget_show (widget);

		g_signal_connect_swapped (
			widget, "clicked",
			G_CALLBACK (e_alert_bar_clear), alert_bar);

		g_free (str);
	}

	widget = e_gtk_widget_get_toplevel (GTK_WIDGET (alert_bar));

	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (alert_bar->priv->scrolled_window), -1);

	if (widget) {
		gint max_height;

		/* Allow up to 20% of the window height being used by the alert, or at least MAX_HEIGHT pixels */
		max_height = MAX (gtk_widget_get_allocated_height (widget) / 5, MAX_HEIGHT);

		alert_bar->priv->max_content_height = max_height;

		gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (alert_bar->priv->scrolled_window), max_height);
	}

	primary_text = e_alert_get_primary_text (alert);
	secondary_text = e_alert_get_secondary_text (alert);

	if (primary_text == NULL)
		primary_text = "";

	if (secondary_text == NULL)
		secondary_text = "";

	have_primary_text = (*primary_text != '\0');
	have_secondary_text = (*secondary_text != '\0');

	message_type = e_alert_get_message_type (alert);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_info_bar_set_message_type (info_bar, message_type);
	G_GNUC_END_IGNORE_DEPRECATIONS

	if (have_primary_text && have_secondary_text)
		markup = g_markup_printf_escaped ("<b>%s</b>\n\n<small>%s</small>", primary_text, secondary_text);
	else if (have_primary_text)
		markup = g_markup_escape_text (primary_text, -1);
	else
		markup = g_markup_escape_text (secondary_text, -1);
	gtk_label_set_markup (GTK_LABEL (alert_bar->priv->message_label), markup);
	g_free (markup);

	icon_name = e_alert_get_icon_name (alert);
	image = GTK_IMAGE (alert_bar->priv->image);
	gtk_image_set_from_icon_name (image, icon_name);

	/* Avoid showing an image for empty alerts. */
	visible = have_primary_text || have_secondary_text;
	gtk_widget_set_visible (alert_bar->priv->image, visible);

	gtk_widget_show (GTK_WIDGET (alert_bar));

	/* Warnings are generally meant for transient errors.
	 * No need to leave them up indefinitely.  Close them
	 * automatically if the user hasn't responded after a
	 * reasonable period of time has elapsed. */
	if (message_type == GTK_MESSAGE_WARNING)
		e_alert_start_timer (alert, WARNING_TIMEOUT_SECONDS);
}

static void
alert_bar_response_cb (EAlert *alert,
                       gint response_id,
                       EAlertBar *alert_bar)
{
	GQueue *queue;
	EAlert *head;
	gboolean was_head;

	queue = &alert_bar->priv->alerts;
	head = g_queue_peek_head (queue);
	was_head = (alert == head);

	g_signal_handlers_disconnect_by_func (
		alert, alert_bar_response_cb, alert_bar);

	if (g_queue_remove (queue, alert))
		g_object_unref (alert);

	if (g_queue_is_empty (queue)) {
		gtk_widget_set_visible (GTK_WIDGET (alert_bar), FALSE);

		/* Remove all buttons from the previous alert. */
		alert_bar_clear_action_area (alert_bar);
	} else if (was_head) {
		alert_bar_show_alert (alert_bar);
	}
}

static void
alert_bar_dispose (GObject *object)
{
	EAlertBar *self = E_ALERT_BAR (object);

	self->priv->message_label = NULL;

	while (!g_queue_is_empty (&self->priv->alerts)) {
		EAlert *alert = g_queue_pop_head (&self->priv->alerts);
		g_signal_handlers_disconnect_by_func (
			alert, alert_bar_response_cb, object);
		g_object_unref (alert);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_alert_bar_parent_class)->dispose (object);
}

static void
alert_bar_close_cb (GtkInfoBar *info_bar,
		    EAlertBar *alert_bar)
{
	/* GtkInfoBar's close looks for a button with a response code of
	 * GTK_RESPONSE_CANCEL, which does not apply here. */
	e_alert_bar_close_alert (alert_bar);
}

static void
alert_bar_constructed (GObject *object)
{
	EAlertBar *self;
	GtkWidget *container;
	GtkWidget *widget;

	self = E_ALERT_BAR (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_alert_bar_parent_class)->constructed (object);

	g_queue_init (&self->priv->alerts);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	widget = gtk_info_bar_new ();
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_box_append (GTK_BOX (self), widget);
	self->priv->info_bar = widget;

	g_signal_connect (widget, "close",
		G_CALLBACK (alert_bar_close_cb), self);

	container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_hexpand (container, TRUE);
	gtk_info_bar_add_child (GTK_INFO_BAR (self->priv->info_bar), container);
	G_GNUC_END_IGNORE_DEPRECATIONS

	widget = gtk_image_new ();
	gtk_image_set_icon_size (GTK_IMAGE (widget), GTK_ICON_SIZE_LARGE);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_box_append (GTK_BOX (container), widget);
	self->priv->image = widget;

	/* the message gets its natural height, up to the maximum, then it scrolls */
	widget = gtk_scrolled_window_new ();
	g_object_set (G_OBJECT (widget),
		"valign", GTK_ALIGN_CENTER,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"hscrollbar-policy", GTK_POLICY_NEVER,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"propagate-natural-height", TRUE,
		"has-frame", FALSE,
		NULL);
	gtk_box_append (GTK_BOX (container), widget);
	self->priv->scrolled_window = widget;

	widget = gtk_label_new (NULL);
	gtk_label_set_wrap (GTK_LABEL (widget), TRUE);
	gtk_label_set_wrap_mode (GTK_LABEL (widget), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_selectable (GTK_LABEL (widget), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (widget), 20);
	gtk_label_set_xalign (GTK_LABEL (widget), 0);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->priv->scrolled_window), widget);
	self->priv->message_label = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_box_append (GTK_BOX (container), widget);
	self->priv->action_area = widget;
}

static GtkSizeRequestMode
alert_bar_get_request_mode (GtkWidget *widget)
{
	/* GtkBox does width-for-height by default.  But we
	 * want the alert bar to be as short as possible. */
	return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
e_alert_bar_class_init (EAlertBarClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = alert_bar_dispose;
	object_class->constructed = alert_bar_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_request_mode = alert_bar_get_request_mode;
}

static void
e_alert_bar_init (EAlertBar *alert_bar)
{
	alert_bar->priv = e_alert_bar_get_instance_private (alert_bar);
	alert_bar->priv->max_content_height = MAX_HEIGHT;
}

GtkWidget *
e_alert_bar_new (void)
{
	return g_object_new (E_TYPE_ALERT_BAR, NULL);
}

void
e_alert_bar_clear (EAlertBar *alert_bar)
{
	GQueue *queue;
	EAlert *alert;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));

	queue = &alert_bar->priv->alerts;

	while ((alert = g_queue_pop_head (queue)) != NULL)
		alert_bar_response_close (alert);
}

gboolean
e_alert_bar_remove_alert_by_tag (EAlertBar *alert_bar,
				 const gchar *tag)
{
	GList *link;

	g_return_val_if_fail (E_IS_ALERT_BAR (alert_bar), FALSE);
	g_return_val_if_fail (tag != NULL, FALSE);

	for (link = g_queue_peek_head_link (&alert_bar->priv->alerts); link; link = g_list_next (link)) {
		EAlert *alert = link->data;

		if (g_strcmp0 (tag, e_alert_get_tag (alert)) == 0) {
			alert_bar_response_close (alert);
			return TRUE;
		}
	}

	return FALSE;
}

typedef struct {
	gboolean found;
	EAlert *looking_for;
} DuplicateData;

static void
alert_bar_find_duplicate_cb (EAlert *alert,
                             DuplicateData *dd)
{
	g_return_if_fail (dd->looking_for != NULL);

	dd->found |= (
		e_alert_get_message_type (alert) ==
		e_alert_get_message_type (dd->looking_for) &&
		g_strcmp0 (
			e_alert_get_primary_text (alert),
			e_alert_get_primary_text (dd->looking_for)) == 0 &&
		g_strcmp0 (
			e_alert_get_secondary_text (alert),
			e_alert_get_secondary_text (dd->looking_for)) == 0);
}

void
e_alert_bar_add_alert (EAlertBar *alert_bar,
                       EAlert *alert)
{
	DuplicateData dd;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));
	g_return_if_fail (E_IS_ALERT (alert));

	dd.found = FALSE;
	dd.looking_for = alert;

	g_queue_foreach (
		&alert_bar->priv->alerts,
		(GFunc) alert_bar_find_duplicate_cb, &dd);

	if (dd.found)
		return;

	g_signal_connect (
		alert, "response",
		G_CALLBACK (alert_bar_response_cb), alert_bar);

	g_queue_push_head (&alert_bar->priv->alerts, g_object_ref (alert));

	alert_bar_show_alert (alert_bar);
}

/**
 * e_alert_bar_close_alert:
 * @alert_bar: an #EAlertBar
 *
 * Closes the active #EAlert and returns %TRUE, or else returns %FALSE if
 * there is no active #EAlert.
 *
 * Returns: whether an #EAlert was closed
 **/
gboolean
e_alert_bar_close_alert (EAlertBar *alert_bar)
{
	EAlert *alert;
	gboolean alert_closed = FALSE;

	g_return_val_if_fail (E_IS_ALERT_BAR (alert_bar), FALSE);

	alert = g_queue_peek_head (&alert_bar->priv->alerts);

	if (alert != NULL) {
		alert_bar_response_close (alert);
		alert_closed = TRUE;
	}

	return alert_closed;
}

/**
 * e_alert_bar_submit_alert:
 * @alert_bar: an #EAlertBar
 * @alert: an #EAlert
 *
 * Depending on the @alert type either shows a dialog or adds
 * the alert into the @alert_bar. This is meant to be used
 * by #EAlertSink implementations which use the #EAlertBar.
 *
 * Since: 3.26
 **/
void
e_alert_bar_submit_alert (EAlertBar *alert_bar,
			  EAlert *alert)
{
	GtkWidget *toplevel;
	GtkWidget *widget;
	GtkWindow *parent;

	g_return_if_fail (E_IS_ALERT_BAR (alert_bar));
	g_return_if_fail (E_IS_ALERT (alert));

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
		case GTK_MESSAGE_WARNING:
		case GTK_MESSAGE_QUESTION:
		case GTK_MESSAGE_ERROR:
			e_alert_bar_add_alert (alert_bar, alert);
			break;

		default:
			toplevel = e_gtk_widget_get_toplevel (GTK_WIDGET (alert_bar));
			if (GTK_IS_WINDOW (toplevel))
				parent = GTK_WINDOW (toplevel);
			else
				parent = NULL;
			widget = e_alert_dialog_new (parent, alert);
			e_gtk_dialog_run (GTK_DIALOG (widget));
			e_gtk_widget_destroy (widget);
	}
}
