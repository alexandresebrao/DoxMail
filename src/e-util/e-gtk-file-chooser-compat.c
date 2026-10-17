/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-gtk-file-chooser-compat.h"

struct _EGtkFileChooserButton {
	GtkButton parent;

	gchar *title;
	GtkFileChooserAction action;
	GFile *file;
	GFile *current_folder;
	GListStore *filters;
	GtkFileFilter *filter;

	GtkWidget *label;
	GtkFileChooserNative *native;
};

enum {
	FILE_SET,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EGtkFileChooserButton, e_gtk_file_chooser_button, GTK_TYPE_BUTTON)

static void
file_chooser_button_update_label (EGtkFileChooserButton *self)
{
	gchar *basename = NULL;

	if (self->file)
		basename = g_file_get_basename (self->file);

	gtk_label_set_text (GTK_LABEL (self->label), basename ? basename : _("(None)"));

	g_free (basename);
}

static void
file_chooser_button_set_file_internal (EGtkFileChooserButton *self,
                                       GFile *file,
                                       gboolean emit_file_set)
{
	if (self->file == file || (self->file && file && g_file_equal (self->file, file)))
		return;

	g_clear_object (&self->file);
	if (file)
		self->file = g_object_ref (file);

	file_chooser_button_update_label (self);

	g_signal_emit (self, signals[SELECTION_CHANGED], 0);

	if (emit_file_set)
		g_signal_emit (self, signals[FILE_SET], 0);
}

static void
file_chooser_button_response_cb (GtkNativeDialog *native,
                                 gint response_id,
                                 EGtkFileChooserButton *self)
{
	if (response_id == GTK_RESPONSE_ACCEPT) {
		GFile *file;

		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (native));
		file_chooser_button_set_file_internal (self, file, TRUE);
		g_clear_object (&file);
	}

	g_clear_object (&self->native);
}

static void
file_chooser_button_clicked (GtkButton *button)
{
	EGtkFileChooserButton *self = E_GTK_FILE_CHOOSER_BUTTON (button);
	GtkRoot *root;
	guint ii, n_filters;

	if (self->native)
		return;

	root = gtk_widget_get_root (GTK_WIDGET (self));

	self->native = gtk_file_chooser_native_new (
		self->title,
		GTK_IS_WINDOW (root) ? GTK_WINDOW (root) : NULL,
		self->action, NULL, NULL);
	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (self->native), TRUE);

	n_filters = g_list_model_get_n_items (G_LIST_MODEL (self->filters));
	for (ii = 0; ii < n_filters; ii++) {
		GtkFileFilter *filter = g_list_model_get_item (G_LIST_MODEL (self->filters), ii);

		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (self->native), filter);
		g_object_unref (filter);
	}

	if (self->filter)
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (self->native), self->filter);

	if (self->file && self->action != GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		gtk_file_chooser_set_file (GTK_FILE_CHOOSER (self->native), self->file, NULL);
	else if (self->file)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->native), self->file, NULL);
	else if (self->current_folder)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->native), self->current_folder, NULL);

	g_signal_connect (self->native, "response",
		G_CALLBACK (file_chooser_button_response_cb), self);

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (self->native));
}

static void
file_chooser_button_dispose (GObject *object)
{
	EGtkFileChooserButton *self = E_GTK_FILE_CHOOSER_BUTTON (object);

	if (self->native) {
		g_signal_handlers_disconnect_by_data (self->native, self);
		gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (self->native));
		g_clear_object (&self->native);
	}

	g_clear_object (&self->file);
	g_clear_object (&self->current_folder);
	g_clear_object (&self->filters);
	g_clear_object (&self->filter);

	G_OBJECT_CLASS (e_gtk_file_chooser_button_parent_class)->dispose (object);
}

static void
file_chooser_button_finalize (GObject *object)
{
	EGtkFileChooserButton *self = E_GTK_FILE_CHOOSER_BUTTON (object);

	g_free (self->title);

	G_OBJECT_CLASS (e_gtk_file_chooser_button_parent_class)->finalize (object);
}

static void
e_gtk_file_chooser_button_class_init (EGtkFileChooserButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = file_chooser_button_dispose;
	object_class->finalize = file_chooser_button_finalize;

	GTK_BUTTON_CLASS (klass)->clicked = file_chooser_button_clicked;

	signals[FILE_SET] = g_signal_new ("file-set",
		G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 0);

	signals[SELECTION_CHANGED] = g_signal_new ("selection-changed",
		G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,
		NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
e_gtk_file_chooser_button_init (EGtkFileChooserButton *self)
{
	GtkWidget *box;

	self->filters = g_list_store_new (GTK_TYPE_FILE_FILTER);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_append (GTK_BOX (box), gtk_image_new_from_icon_name ("document-open-symbolic"));

	self->label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (self->label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_xalign (GTK_LABEL (self->label), 0);
	gtk_widget_set_hexpand (self->label, TRUE);
	gtk_box_append (GTK_BOX (box), self->label);

	gtk_button_set_child (GTK_BUTTON (self), box);

	file_chooser_button_update_label (self);
}

GtkWidget *
e_gtk_file_chooser_button_new (const gchar *title,
                               GtkFileChooserAction action)
{
	EGtkFileChooserButton *self;

	self = g_object_new (E_GTK_TYPE_FILE_CHOOSER_BUTTON, NULL);
	self->title = g_strdup (title);
	self->action = action;

	return GTK_WIDGET (self);
}

/* The dialog is a GtkFileChooserNative (or other GtkFileChooser); the
 * button takes over its title, action and filters. */
GtkWidget *
e_gtk_file_chooser_button_new_with_dialog (gpointer dialog)
{
	GtkWidget *button;
	GListModel *filters;
	guint ii, n_filters;

	g_return_val_if_fail (GTK_IS_FILE_CHOOSER (dialog), NULL);

	if (GTK_IS_NATIVE_DIALOG (dialog))
		button = e_gtk_file_chooser_button_new (
			gtk_native_dialog_get_title (dialog),
			gtk_file_chooser_get_action (dialog));
	else
		button = e_gtk_file_chooser_button_new (
			GTK_IS_WINDOW (dialog) ? gtk_window_get_title (dialog) : NULL,
			gtk_file_chooser_get_action (dialog));

	filters = gtk_file_chooser_get_filters (dialog);
	n_filters = g_list_model_get_n_items (filters);

	for (ii = 0; ii < n_filters; ii++) {
		GtkFileFilter *filter = g_list_model_get_item (filters, ii);

		e_gtk_file_chooser_add_filter (button, filter);
		g_object_unref (filter);
	}

	g_object_unref (filters);

	if (gtk_file_chooser_get_filter (dialog))
		e_gtk_file_chooser_set_filter (button, gtk_file_chooser_get_filter (dialog));

	if (GTK_IS_NATIVE_DIALOG (dialog))
		gtk_native_dialog_destroy (dialog);
	else if (GTK_IS_WINDOW (dialog))
		gtk_window_destroy (dialog);

	g_object_unref (dialog);

	return button;
}

#define IS_BUTTON(chooser) E_GTK_IS_FILE_CHOOSER_BUTTON (chooser)
#define BUTTON(chooser) E_GTK_FILE_CHOOSER_BUTTON (chooser)

GFile *
e_gtk_file_chooser_get_file (gpointer chooser)
{
	if (IS_BUTTON (chooser))
		return BUTTON (chooser)->file ? g_object_ref (BUTTON (chooser)->file) : NULL;

	g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

	return gtk_file_chooser_get_file (chooser);
}

gboolean
e_gtk_file_chooser_set_file (gpointer chooser,
                             GFile *file,
                             GError **error)
{
	if (IS_BUTTON (chooser)) {
		file_chooser_button_set_file_internal (BUTTON (chooser), file, FALSE);
		return TRUE;
	}

	g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), FALSE);

	return gtk_file_chooser_set_file (chooser, file, error);
}

gchar *
e_gtk_file_chooser_get_filename (gpointer chooser)
{
	GFile *file = e_gtk_file_chooser_get_file (chooser);
	gchar *filename = NULL;

	if (file) {
		filename = g_file_get_path (file);
		g_object_unref (file);
	}

	return filename;
}

gboolean
e_gtk_file_chooser_set_filename (gpointer chooser,
                                 const gchar *filename)
{
	GFile *file;
	gboolean success;

	file = filename && *filename ? g_file_new_for_path (filename) : NULL;
	success = e_gtk_file_chooser_set_file (chooser, file, NULL);
	g_clear_object (&file);

	return success;
}

gchar *
e_gtk_file_chooser_get_uri (gpointer chooser)
{
	GFile *file = e_gtk_file_chooser_get_file (chooser);
	gchar *uri = NULL;

	if (file) {
		uri = g_file_get_uri (file);
		g_object_unref (file);
	}

	return uri;
}

gboolean
e_gtk_file_chooser_set_uri (gpointer chooser,
                            const gchar *uri)
{
	GFile *file;
	gboolean success;

	file = uri && *uri ? g_file_new_for_uri (uri) : NULL;
	success = e_gtk_file_chooser_set_file (chooser, file, NULL);
	g_clear_object (&file);

	return success;
}

GSList *
e_gtk_file_chooser_get_files (gpointer chooser)
{
	GListModel *files;
	GSList *list = NULL;
	guint ii;

	if (IS_BUTTON (chooser)) {
		GFile *file = e_gtk_file_chooser_get_file (chooser);

		return file ? g_slist_prepend (NULL, file) : NULL;
	}

	g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

	files = gtk_file_chooser_get_files (chooser);

	for (ii = g_list_model_get_n_items (files); ii > 0; ii--)
		list = g_slist_prepend (list, g_list_model_get_item (files, ii - 1));

	g_object_unref (files);

	return list;
}

GSList *
e_gtk_file_chooser_get_uris (gpointer chooser)
{
	GSList *files, *link, *uris = NULL;

	files = e_gtk_file_chooser_get_files (chooser);

	for (link = files; link; link = g_slist_next (link))
		uris = g_slist_prepend (uris, g_file_get_uri (link->data));

	g_slist_free_full (files, g_object_unref);

	return g_slist_reverse (uris);
}

void
e_gtk_file_chooser_unselect_all (gpointer chooser)
{
	if (IS_BUTTON (chooser))
		file_chooser_button_set_file_internal (BUTTON (chooser), NULL, FALSE);

	/* GTK 4 file choosers have no unselect */
}

void
e_gtk_file_chooser_add_filter (gpointer chooser,
                               GtkFileFilter *filter)
{
	if (IS_BUTTON (chooser)) {
		g_list_store_append (BUTTON (chooser)->filters, filter);
		return;
	}

	g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

	gtk_file_chooser_add_filter (chooser, filter);
}

void
e_gtk_file_chooser_set_filter (gpointer chooser,
                               GtkFileFilter *filter)
{
	if (IS_BUTTON (chooser)) {
		g_set_object (&BUTTON (chooser)->filter, filter);
		return;
	}

	g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

	gtk_file_chooser_set_filter (chooser, filter);
}

GtkFileFilter *
e_gtk_file_chooser_get_filter (gpointer chooser)
{
	if (IS_BUTTON (chooser))
		return BUTTON (chooser)->filter;

	g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

	return gtk_file_chooser_get_filter (chooser);
}

GSList *
e_gtk_file_chooser_list_filters (gpointer chooser)
{
	GListModel *filters;
	GSList *list = NULL;
	guint ii;

	if (IS_BUTTON (chooser))
		filters = g_object_ref (G_LIST_MODEL (BUTTON (chooser)->filters));
	else
		filters = gtk_file_chooser_get_filters (chooser);

	/* GTK 3 returned the filters without references */
	for (ii = g_list_model_get_n_items (filters); ii > 0; ii--) {
		GtkFileFilter *filter = g_list_model_get_item (filters, ii - 1);

		list = g_slist_prepend (list, filter);
		g_object_unref (filter);
	}

	g_object_unref (filters);

	return list;
}

gboolean
e_gtk_file_chooser_set_current_folder (gpointer chooser,
                                       const gchar *filename)
{
	GFile *folder;
	gboolean success = TRUE;

	if (!filename || !*filename)
		return FALSE;

	folder = g_file_new_for_path (filename);

	if (IS_BUTTON (chooser))
		g_set_object (&BUTTON (chooser)->current_folder, folder);
	else
		success = gtk_file_chooser_set_current_folder (chooser, folder, NULL);

	g_object_unref (folder);

	return success;
}

gboolean
e_gtk_file_chooser_set_current_folder_uri (gpointer chooser,
                                           const gchar *uri)
{
	GFile *folder;
	gboolean success = TRUE;

	if (!uri || !*uri)
		return FALSE;

	folder = g_file_new_for_uri (uri);

	if (IS_BUTTON (chooser))
		g_set_object (&BUTTON (chooser)->current_folder, folder);
	else
		success = gtk_file_chooser_set_current_folder (chooser, folder, NULL);

	g_object_unref (folder);

	return success;
}

gchar *
e_gtk_file_chooser_get_current_folder_uri (gpointer chooser)
{
	GFile *folder;
	gchar *uri = NULL;

	if (IS_BUTTON (chooser))
		folder = BUTTON (chooser)->current_folder ? g_object_ref (BUTTON (chooser)->current_folder) : NULL;
	else
		folder = gtk_file_chooser_get_current_folder (chooser);

	if (folder) {
		uri = g_file_get_uri (folder);
		g_object_unref (folder);
	}

	return uri;
}

__attribute__((constructor))
static void
e_gtk_file_chooser_register_signals (void)
{
	GType types[3];
	guint ii;

	types[0] = GTK_TYPE_FILE_CHOOSER_DIALOG;
	types[1] = GTK_TYPE_FILE_CHOOSER_NATIVE;
	types[2] = GTK_TYPE_FILE_CHOOSER_WIDGET;

	for (ii = 0; ii < G_N_ELEMENTS (types); ii++) {
		g_signal_new ("update-preview", types[ii],
			G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
			G_TYPE_NONE, 0);
	}
}

gchar *
e_gtk_file_chooser_get_preview_filename (gpointer chooser)
{
	return NULL;
}

GtkWidget *
e_gtk_file_chooser_get_preview_widget (gpointer chooser)
{
	return NULL;
}

void
e_gtk_file_chooser_set_preview_widget (gpointer chooser,
				       GtkWidget *preview_widget)
{
	if (preview_widget && g_object_is_floating (preview_widget))
		g_object_unref (g_object_ref_sink (preview_widget));
}

void
e_gtk_file_chooser_set_preview_widget_active (gpointer chooser,
					      gboolean active)
{
}

void
e_gtk_file_chooser_set_use_preview_label (gpointer chooser,
					  gboolean use_label)
{
}

#define EXTRA_WIDGET_KEY "e-gtk-file-chooser-extra-widget"

void
e_gtk_file_chooser_set_extra_widget (gpointer chooser,
				     GtkWidget *extra_widget)
{
	g_return_if_fail (G_IS_OBJECT (chooser));

	if (!extra_widget) {
		g_object_set_data (chooser, EXTRA_WIDGET_KEY, NULL);
		return;
	}

	g_object_set_data_full (chooser, EXTRA_WIDGET_KEY, g_object_ref_sink (extra_widget), g_object_unref);

	if (GTK_IS_DIALOG (chooser)) {
		GtkWidget *content_area = gtk_dialog_get_content_area (chooser);

		gtk_widget_set_margin_start (extra_widget, 6);
		gtk_widget_set_margin_end (extra_widget, 6);
		gtk_widget_set_margin_bottom (extra_widget, 6);
		gtk_box_append (GTK_BOX (content_area), extra_widget);
	}
}

GtkWidget *
e_gtk_file_chooser_get_extra_widget (gpointer chooser)
{
	g_return_val_if_fail (G_IS_OBJECT (chooser), NULL);

	return g_object_get_data (chooser, EXTRA_WIDGET_KEY);
}
