/*
 * SPDX-FileCopyrightText: (C) 2026 DoxMail contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ASSISTANT_H
#define E_ASSISTANT_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_ASSISTANT \
	(e_assistant_get_type ())
#define E_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ASSISTANT, EAssistant))
#define E_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ASSISTANT, EAssistantClass))
#define E_IS_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ASSISTANT))
#define E_IS_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ASSISTANT))
#define E_ASSISTANT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ASSISTANT, EAssistantClass))

G_BEGIN_DECLS

typedef struct _EAssistant EAssistant;
typedef struct _EAssistantClass EAssistantClass;
typedef struct _EAssistantPrivate EAssistantPrivate;

/**
 * EAssistant:
 *
 * A derivable stand-in for #GtkAssistant, which is final with GTK 4.
 * The API mirrors gtk_assistant_*() and keeps its #GtkAssistantPageType.
 *
 * Since: DoxMail
 **/
struct _EAssistant {
	GtkWindow parent;
	EAssistantPrivate *priv;
};

struct _EAssistantClass {
	GtkWindowClass parent_class;

	void		(*prepare)		(EAssistant *assistant,
						 GtkWidget *page);
	void		(*apply)		(EAssistant *assistant);
	void		(*close)		(EAssistant *assistant);
	void		(*cancel)		(EAssistant *assistant);
};

typedef gint	(*EAssistantPageFunc)		(gint current_page,
						 gpointer data);

GType		e_assistant_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_assistant_new			(void);
gint		e_assistant_get_current_page	(EAssistant *assistant);
void		e_assistant_set_current_page	(EAssistant *assistant,
						 gint page_num);
void		e_assistant_next_page		(EAssistant *assistant);
void		e_assistant_previous_page	(EAssistant *assistant);
gint		e_assistant_get_n_pages		(EAssistant *assistant);
GtkWidget *	e_assistant_get_nth_page	(EAssistant *assistant,
						 gint page_num);
gint		e_assistant_append_page		(EAssistant *assistant,
						 GtkWidget *page);
gint		e_assistant_prepend_page	(EAssistant *assistant,
						 GtkWidget *page);
gint		e_assistant_insert_page		(EAssistant *assistant,
						 GtkWidget *page,
						 gint position);
void		e_assistant_remove_page		(EAssistant *assistant,
						 gint page_num);
void		e_assistant_set_forward_page_func
						(EAssistant *assistant,
						 EAssistantPageFunc page_func,
						 gpointer data,
						 GDestroyNotify destroy);
void		e_assistant_set_page_type	(EAssistant *assistant,
						 GtkWidget *page,
						 GtkAssistantPageType type);
GtkAssistantPageType
		e_assistant_get_page_type	(EAssistant *assistant,
						 GtkWidget *page);
void		e_assistant_set_page_title	(EAssistant *assistant,
						 GtkWidget *page,
						 const gchar *title);
const gchar *	e_assistant_get_page_title	(EAssistant *assistant,
						 GtkWidget *page);
void		e_assistant_set_page_complete	(EAssistant *assistant,
						 GtkWidget *page,
						 gboolean complete);
gboolean	e_assistant_get_page_complete	(EAssistant *assistant,
						 GtkWidget *page);
void		e_assistant_add_action_widget	(EAssistant *assistant,
						 GtkWidget *child);
void		e_assistant_remove_action_widget
						(EAssistant *assistant,
						 GtkWidget *child);
void		e_assistant_update_buttons_state
						(EAssistant *assistant);
void		e_assistant_commit		(EAssistant *assistant);

G_END_DECLS

#endif /* E_ASSISTANT_H */
