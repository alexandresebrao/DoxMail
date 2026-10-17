/*
 * SPDX-FileCopyrightText: (C) 2026 DoxMail contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-assistant.h"

/* GtkAssistant is final with GTK 4, but Evolution derives three
 * assistants from it.  This is a small window with the same page
 * model: a stack of pages, a title above them and the navigation
 * buttons below, laid out by page type the way GtkAssistant does. */

typedef struct _PageInfo {
	GtkWidget *page;
	GtkAssistantPageType type;
	gchar *title;
	gboolean complete;
} PageInfo;

struct _EAssistantPrivate {
	GPtrArray *pages; /* PageInfo * */
	gint current_page;
	GSList *visited; /* gint, previous pages, newest first */
	gboolean committed;
	gboolean use_header_bar;

	GtkWidget *header_bar;
	GtkWidget *title_label;
	GtkWidget *stack;
	GtkWidget *action_box;
	GtkWidget *cancel_button;
	GtkWidget *back_button;
	GtkWidget *forward_button;
	GtkWidget *apply_button;
	GtkWidget *close_button;

	EAssistantPageFunc forward_func;
	gpointer forward_data;
	GDestroyNotify forward_destroy;
};

enum {
	PROP_0,
	PROP_USE_HEADER_BAR
};

enum {
	PREPARE,
	APPLY,
	CLOSE,
	CANCEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EAssistant, e_assistant, GTK_TYPE_WINDOW)

static void
page_info_free (gpointer ptr)
{
	PageInfo *info = ptr;

	g_free (info->title);
	g_object_unref (info->page);
	g_free (info);
}

static gint
assistant_find_page (EAssistant *assistant,
		     GtkWidget *page)
{
	guint ii;

	for (ii = 0; ii < assistant->priv->pages->len; ii++) {
		PageInfo *info = g_ptr_array_index (assistant->priv->pages, ii);

		if (info->page == page)
			return (gint) ii;
	}

	return -1;
}

static PageInfo *
assistant_get_info (EAssistant *assistant,
		    gint page_num)
{
	if (page_num < 0 || page_num >= (gint) assistant->priv->pages->len)
		return NULL;

	return g_ptr_array_index (assistant->priv->pages, page_num);
}

static PageInfo *
assistant_get_info_for (EAssistant *assistant,
			GtkWidget *page)
{
	return assistant_get_info (assistant, assistant_find_page (assistant, page));
}

static gint
assistant_compute_next (EAssistant *assistant)
{
	EAssistantPrivate *priv = assistant->priv;
	gint next = priv->current_page, n_pages = priv->pages->len, tries;

	/* skip hidden pages, like GtkAssistant */
	for (tries = 0; tries < n_pages; tries++) {
		PageInfo *info;

		if (priv->forward_func)
			next = priv->forward_func (next, priv->forward_data);
		else
			next = next + 1;

		info = assistant_get_info (assistant, next);
		if (!info)
			return -1;

		if (gtk_widget_get_visible (info->page))
			return next;
	}

	return -1;
}

static void
assistant_update_title (EAssistant *assistant)
{
	PageInfo *info = assistant_get_info (assistant, assistant->priv->current_page);
	const gchar *title = info && info->title ? info->title : "";

	gtk_label_set_text (GTK_LABEL (assistant->priv->title_label), title);
}

static void
assistant_set_current_internal (EAssistant *assistant,
				gint page_num,
				gboolean remember)
{
	EAssistantPrivate *priv = assistant->priv;
	PageInfo *info;

	info = assistant_get_info (assistant, page_num);
	if (!info || page_num == priv->current_page)
		return;

	if (remember && priv->current_page >= 0)
		priv->visited = g_slist_prepend (priv->visited, GINT_TO_POINTER (priv->current_page));

	priv->current_page = page_num;

	gtk_stack_set_visible_child (GTK_STACK (priv->stack), info->page);
	assistant_update_title (assistant);
	e_assistant_update_buttons_state (assistant);

	g_signal_emit (assistant, signals[PREPARE], 0, info->page);

	/* prepare handlers may change pages or completeness */
	e_assistant_update_buttons_state (assistant);
}

static void
assistant_forward_clicked_cb (EAssistant *assistant)
{
	e_assistant_next_page (assistant);
}

static void
assistant_back_clicked_cb (EAssistant *assistant)
{
	e_assistant_previous_page (assistant);
}

static void
assistant_apply_clicked_cb (EAssistant *assistant)
{
	gint next;

	g_signal_emit (assistant, signals[APPLY], 0);

	next = assistant_compute_next (assistant);
	if (next >= 0)
		assistant_set_current_internal (assistant, next, TRUE);
	else
		g_signal_emit (assistant, signals[CLOSE], 0);
}

static void
assistant_close_clicked_cb (EAssistant *assistant)
{
	g_signal_emit (assistant, signals[CLOSE], 0);
}

static void
assistant_cancel_clicked_cb (EAssistant *assistant)
{
	g_signal_emit (assistant, signals[CANCEL], 0);
}

static gboolean
assistant_escape_cb (GtkWidget *widget,
		     GVariant *args,
		     gpointer user_data)
{
	g_signal_emit (widget, signals[CANCEL], 0);

	return TRUE;
}

static gboolean
assistant_close_request (GtkWindow *window)
{
	/* the window manager's close button cancels, as with GtkAssistant */
	g_signal_emit (window, signals[CANCEL], 0);

	return TRUE;
}

static void
assistant_map (GtkWidget *widget)
{
	EAssistant *assistant = E_ASSISTANT (widget);

	GTK_WIDGET_CLASS (e_assistant_parent_class)->map (widget);

	/* show the first visible page */
	if (assistant->priv->current_page < 0) {
		guint ii;

		for (ii = 0; ii < assistant->priv->pages->len; ii++) {
			PageInfo *info = g_ptr_array_index (assistant->priv->pages, ii);

			if (gtk_widget_get_visible (info->page)) {
				assistant_set_current_internal (assistant, ii, FALSE);
				break;
			}
		}
	}
}

static GtkWidget *
assistant_add_button (EAssistant *assistant,
		      const gchar *label,
		      GCallback callback,
		      gboolean suggested)
{
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic (label);
	gtk_widget_set_visible (button, FALSE);
	if (suggested)
		gtk_widget_add_css_class (button, "suggested-action");
	g_signal_connect_swapped (button, "clicked", callback, assistant);
	gtk_box_append (GTK_BOX (assistant->priv->action_box), button);

	return button;
}

static void
assistant_set_property (GObject *object,
			guint property_id,
			const GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_USE_HEADER_BAR:
			E_ASSISTANT (object)->priv->use_header_bar = g_value_get_boolean (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
assistant_get_property (GObject *object,
			guint property_id,
			GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_USE_HEADER_BAR:
			g_value_set_boolean (value, E_ASSISTANT (object)->priv->use_header_bar);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
assistant_constructed (GObject *object)
{
	EAssistant *assistant = E_ASSISTANT (object);
	EAssistantPrivate *priv = assistant->priv;
	GtkWidget *vbox, *spacer;

	G_OBJECT_CLASS (e_assistant_parent_class)->constructed (object);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_margin_start (vbox, 12);
	gtk_widget_set_margin_end (vbox, 12);
	gtk_widget_set_margin_top (vbox, 12);
	gtk_widget_set_margin_bottom (vbox, 12);
	gtk_window_set_child (GTK_WINDOW (assistant), vbox);

	priv->title_label = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (priv->title_label), 0.0);
	gtk_widget_add_css_class (priv->title_label, "title-2");

	if (priv->use_header_bar) {
		/* the page title goes to the header bar then */
		priv->header_bar = gtk_header_bar_new ();
		gtk_header_bar_set_title_widget (GTK_HEADER_BAR (priv->header_bar), priv->title_label);
		gtk_widget_remove_css_class (priv->title_label, "title-2");
		gtk_widget_add_css_class (priv->title_label, "title");
		gtk_window_set_titlebar (GTK_WINDOW (assistant), priv->header_bar);
	} else {
		gtk_box_append (GTK_BOX (vbox), priv->title_label);
	}

	priv->stack = gtk_stack_new ();
	gtk_stack_set_transition_type (GTK_STACK (priv->stack), GTK_STACK_TRANSITION_TYPE_NONE);
	gtk_widget_set_vexpand (priv->stack, TRUE);
	gtk_widget_set_hexpand (priv->stack, TRUE);
	gtk_box_append (GTK_BOX (vbox), priv->stack);

	priv->action_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append (GTK_BOX (vbox), priv->action_box);

	/* action widgets get prepended, the navigation stays at the end */
	spacer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand (spacer, TRUE);
	gtk_box_append (GTK_BOX (priv->action_box), spacer);

	priv->cancel_button = assistant_add_button (assistant, _("_Cancel"), G_CALLBACK (assistant_cancel_clicked_cb), FALSE);
	priv->back_button = assistant_add_button (assistant, _("_Back"), G_CALLBACK (assistant_back_clicked_cb), FALSE);
	priv->forward_button = assistant_add_button (assistant, _("_Next"), G_CALLBACK (assistant_forward_clicked_cb), TRUE);
	priv->apply_button = assistant_add_button (assistant, _("_Apply"), G_CALLBACK (assistant_apply_clicked_cb), TRUE);
	priv->close_button = assistant_add_button (assistant, _("_Close"), G_CALLBACK (assistant_close_clicked_cb), TRUE);

	/* pages added before construction finished */
	if (priv->pages->len) {
		guint ii;

		for (ii = 0; ii < priv->pages->len; ii++) {
			PageInfo *info = g_ptr_array_index (priv->pages, ii);

			gtk_stack_add_child (GTK_STACK (priv->stack), info->page);
		}
	}
}

static void
assistant_dispose (GObject *object)
{
	EAssistant *assistant = E_ASSISTANT (object);

	if (assistant->priv->forward_destroy)
		assistant->priv->forward_destroy (assistant->priv->forward_data);
	assistant->priv->forward_func = NULL;
	assistant->priv->forward_data = NULL;
	assistant->priv->forward_destroy = NULL;

	g_slist_free (assistant->priv->visited);
	assistant->priv->visited = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_assistant_parent_class)->dispose (object);
}

static void
assistant_finalize (GObject *object)
{
	EAssistant *assistant = E_ASSISTANT (object);

	g_ptr_array_unref (assistant->priv->pages);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_assistant_parent_class)->finalize (object);
}

static void
e_assistant_class_init (EAssistantClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkWindowClass *window_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = assistant_set_property;
	object_class->get_property = assistant_get_property;
	object_class->constructed = assistant_constructed;
	object_class->dispose = assistant_dispose;
	object_class->finalize = assistant_finalize;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->map = assistant_map;

	window_class = GTK_WINDOW_CLASS (class);
	window_class->close_request = assistant_close_request;

	gtk_widget_class_add_binding (widget_class, GDK_KEY_Escape, 0, assistant_escape_cb, NULL);

	g_object_class_install_property (
		object_class,
		PROP_USE_HEADER_BAR,
		g_param_spec_boolean (
			"use-header-bar",
			NULL, NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[PREPARE] = g_signal_new (
		"prepare",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAssistantClass, prepare),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		GTK_TYPE_WIDGET);

	signals[APPLY] = g_signal_new (
		"apply",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAssistantClass, apply),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[CLOSE] = g_signal_new (
		"close",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAssistantClass, close),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[CANCEL] = g_signal_new (
		"cancel",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAssistantClass, cancel),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);
}

static void
e_assistant_init (EAssistant *assistant)
{
	assistant->priv = e_assistant_get_instance_private (assistant);
	assistant->priv->pages = g_ptr_array_new_with_free_func (page_info_free);
	assistant->priv->current_page = -1;
}

GtkWidget *
e_assistant_new (void)
{
	return g_object_new (E_TYPE_ASSISTANT, NULL);
}

gint
e_assistant_get_current_page (EAssistant *assistant)
{
	g_return_val_if_fail (E_IS_ASSISTANT (assistant), -1);

	return assistant->priv->current_page;
}

void
e_assistant_set_current_page (EAssistant *assistant,
			      gint page_num)
{
	g_return_if_fail (E_IS_ASSISTANT (assistant));

	if (page_num < 0)
		page_num = assistant->priv->pages->len - 1;

	assistant_set_current_internal (assistant, page_num, TRUE);
}

void
e_assistant_next_page (EAssistant *assistant)
{
	gint next;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	next = assistant_compute_next (assistant);
	if (next >= 0)
		assistant_set_current_internal (assistant, next, TRUE);
	else
		g_critical ("%s: page flow is broken", G_STRFUNC);
}

void
e_assistant_previous_page (EAssistant *assistant)
{
	EAssistantPrivate *priv;
	gint prev;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	priv = assistant->priv;
	if (!priv->visited)
		return;

	prev = GPOINTER_TO_INT (priv->visited->data);
	priv->visited = g_slist_delete_link (priv->visited, priv->visited);

	assistant_set_current_internal (assistant, prev, FALSE);
}

gint
e_assistant_get_n_pages (EAssistant *assistant)
{
	g_return_val_if_fail (E_IS_ASSISTANT (assistant), 0);

	return assistant->priv->pages->len;
}

GtkWidget *
e_assistant_get_nth_page (EAssistant *assistant,
			  gint page_num)
{
	PageInfo *info;

	g_return_val_if_fail (E_IS_ASSISTANT (assistant), NULL);

	if (page_num < 0)
		page_num = assistant->priv->pages->len - 1;

	info = assistant_get_info (assistant, page_num);

	return info ? info->page : NULL;
}

gint
e_assistant_insert_page (EAssistant *assistant,
			 GtkWidget *page,
			 gint position)
{
	EAssistantPrivate *priv;
	PageInfo *info;
	GSList *link;

	g_return_val_if_fail (E_IS_ASSISTANT (assistant), -1);
	g_return_val_if_fail (GTK_IS_WIDGET (page), -1);

	priv = assistant->priv;

	if (position < 0 || position > (gint) priv->pages->len)
		position = priv->pages->len;

	info = g_new0 (PageInfo, 1);
	info->page = g_object_ref_sink (page);
	info->type = GTK_ASSISTANT_PAGE_CONTENT;

	g_ptr_array_insert (priv->pages, position, info);

	if (priv->stack)
		gtk_stack_add_child (GTK_STACK (priv->stack), page);

	/* indexes at and after the new page moved by one */
	if (priv->current_page >= position)
		priv->current_page++;
	for (link = priv->visited; link; link = g_slist_next (link)) {
		if (GPOINTER_TO_INT (link->data) >= position)
			link->data = GINT_TO_POINTER (GPOINTER_TO_INT (link->data) + 1);
	}

	g_signal_connect_object (page, "notify::visible",
		G_CALLBACK (e_assistant_update_buttons_state), assistant, G_CONNECT_SWAPPED);

	if (priv->stack)
		e_assistant_update_buttons_state (assistant);

	return position;
}

gint
e_assistant_append_page (EAssistant *assistant,
			 GtkWidget *page)
{
	return e_assistant_insert_page (assistant, page, -1);
}

gint
e_assistant_prepend_page (EAssistant *assistant,
			  GtkWidget *page)
{
	return e_assistant_insert_page (assistant, page, 0);
}

void
e_assistant_remove_page (EAssistant *assistant,
			 gint page_num)
{
	EAssistantPrivate *priv;
	PageInfo *info;
	GSList *link, *next;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	priv = assistant->priv;

	if (page_num < 0)
		page_num = priv->pages->len - 1;

	info = assistant_get_info (assistant, page_num);
	if (!info)
		return;

	g_signal_handlers_disconnect_by_data (info->page, assistant);

	if (priv->stack)
		gtk_stack_remove (GTK_STACK (priv->stack), info->page);

	g_ptr_array_remove_index (priv->pages, page_num);

	for (link = priv->visited; link; link = next) {
		gint num = GPOINTER_TO_INT (link->data);

		next = g_slist_next (link);

		if (num == page_num)
			priv->visited = g_slist_delete_link (priv->visited, link);
		else if (num > page_num)
			link->data = GINT_TO_POINTER (num - 1);
	}

	if (priv->current_page == page_num) {
		priv->current_page = -1;
		if (priv->pages->len)
			assistant_set_current_internal (assistant, MIN (page_num, (gint) priv->pages->len - 1), FALSE);
	} else if (priv->current_page > page_num) {
		priv->current_page--;
	}

	e_assistant_update_buttons_state (assistant);
}

void
e_assistant_set_forward_page_func (EAssistant *assistant,
				   EAssistantPageFunc page_func,
				   gpointer data,
				   GDestroyNotify destroy)
{
	EAssistantPrivate *priv;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	priv = assistant->priv;

	if (priv->forward_destroy)
		priv->forward_destroy (priv->forward_data);

	priv->forward_func = page_func;
	priv->forward_data = data;
	priv->forward_destroy = destroy;

	e_assistant_update_buttons_state (assistant);
}

void
e_assistant_set_page_type (EAssistant *assistant,
			   GtkWidget *page,
			   GtkAssistantPageType type)
{
	PageInfo *info;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	info = assistant_get_info_for (assistant, page);
	g_return_if_fail (info != NULL);

	info->type = type;

	e_assistant_update_buttons_state (assistant);
}

GtkAssistantPageType
e_assistant_get_page_type (EAssistant *assistant,
			   GtkWidget *page)
{
	PageInfo *info;

	g_return_val_if_fail (E_IS_ASSISTANT (assistant), GTK_ASSISTANT_PAGE_CONTENT);

	info = assistant_get_info_for (assistant, page);
	g_return_val_if_fail (info != NULL, GTK_ASSISTANT_PAGE_CONTENT);

	return info->type;
}

void
e_assistant_set_page_title (EAssistant *assistant,
			    GtkWidget *page,
			    const gchar *title)
{
	PageInfo *info;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	info = assistant_get_info_for (assistant, page);
	g_return_if_fail (info != NULL);

	g_free (info->title);
	info->title = g_strdup (title);

	if (assistant->priv->title_label)
		assistant_update_title (assistant);
}

const gchar *
e_assistant_get_page_title (EAssistant *assistant,
			    GtkWidget *page)
{
	PageInfo *info;

	g_return_val_if_fail (E_IS_ASSISTANT (assistant), NULL);

	info = assistant_get_info_for (assistant, page);
	g_return_val_if_fail (info != NULL, NULL);

	return info->title;
}

void
e_assistant_set_page_complete (EAssistant *assistant,
			       GtkWidget *page,
			       gboolean complete)
{
	PageInfo *info;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	info = assistant_get_info_for (assistant, page);
	g_return_if_fail (info != NULL);

	if (info->complete == (complete != FALSE))
		return;

	info->complete = complete != FALSE;

	e_assistant_update_buttons_state (assistant);
}

gboolean
e_assistant_get_page_complete (EAssistant *assistant,
			       GtkWidget *page)
{
	PageInfo *info;

	g_return_val_if_fail (E_IS_ASSISTANT (assistant), FALSE);

	info = assistant_get_info_for (assistant, page);
	g_return_val_if_fail (info != NULL, FALSE);

	return info->complete;
}

void
e_assistant_add_action_widget (EAssistant *assistant,
			       GtkWidget *child)
{
	g_return_if_fail (E_IS_ASSISTANT (assistant));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_box_prepend (GTK_BOX (assistant->priv->action_box), child);
}

void
e_assistant_remove_action_widget (EAssistant *assistant,
				  GtkWidget *child)
{
	g_return_if_fail (E_IS_ASSISTANT (assistant));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (gtk_widget_get_parent (child) == assistant->priv->action_box)
		gtk_box_remove (GTK_BOX (assistant->priv->action_box), child);
}

void
e_assistant_update_buttons_state (EAssistant *assistant)
{
	EAssistantPrivate *priv;
	PageInfo *info;
	gboolean cancel = FALSE, back = FALSE, forward = FALSE, apply = FALSE, close = FALSE;
	gboolean has_next;

	g_return_if_fail (E_IS_ASSISTANT (assistant));

	priv = assistant->priv;
	if (!priv->stack)
		return;

	info = assistant_get_info (assistant, priv->current_page);
	if (info) {
		has_next = assistant_compute_next (assistant) >= 0;

		switch (info->type) {
			case GTK_ASSISTANT_PAGE_INTRO:
				cancel = forward = TRUE;
				break;
			case GTK_ASSISTANT_PAGE_CONFIRM:
				cancel = back = apply = TRUE;
				break;
			case GTK_ASSISTANT_PAGE_CONTENT:
			case GTK_ASSISTANT_PAGE_PROGRESS:
				cancel = back = TRUE;
				forward = has_next;
				close = !has_next;
				break;
			case GTK_ASSISTANT_PAGE_SUMMARY:
				close = TRUE;
				break;
			case GTK_ASSISTANT_PAGE_CUSTOM:
			default:
				break;
		}

		if (priv->committed || !priv->visited || info->type == GTK_ASSISTANT_PAGE_PROGRESS)
			back = FALSE;
	}

	gtk_widget_set_visible (priv->cancel_button, cancel);
	gtk_widget_set_visible (priv->back_button, back);
	gtk_widget_set_visible (priv->forward_button, forward);
	gtk_widget_set_visible (priv->apply_button, apply);
	gtk_widget_set_visible (priv->close_button, close);

	gtk_widget_set_sensitive (priv->forward_button, info && info->complete);
	gtk_widget_set_sensitive (priv->apply_button, info && info->complete);
	gtk_widget_set_sensitive (priv->close_button, info && (info->complete || info->type == GTK_ASSISTANT_PAGE_SUMMARY));

	if (forward && info->complete)
		gtk_window_set_default_widget (GTK_WINDOW (assistant), priv->forward_button);
	else if (apply && info->complete)
		gtk_window_set_default_widget (GTK_WINDOW (assistant), priv->apply_button);
	else if (close)
		gtk_window_set_default_widget (GTK_WINDOW (assistant), priv->close_button);
}

void
e_assistant_commit (EAssistant *assistant)
{
	g_return_if_fail (E_IS_ASSISTANT (assistant));

	g_slist_free (assistant->priv->visited);
	assistant->priv->visited = NULL;
	assistant->priv->committed = TRUE;

	e_assistant_update_buttons_state (assistant);
}
