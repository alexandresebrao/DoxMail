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

#include <string.h>

#include "e-gtk-dnd-compat.h"

#define SOURCE_SITE_KEY "e-gtk-drag-source-site"
#define DEST_SITE_KEY "e-gtk-drag-dest-site"
#define DRAG_CONTEXT_KEY "e-gtk-drag-context"

/* The drag-* signals GTK 3 had on GtkWidget */
enum {
	DRAG_BEGIN,
	DRAG_END,
	DRAG_DATA_GET,
	DRAG_DATA_DELETE,
	DRAG_FAILED,
	DRAG_LEAVE,
	DRAG_MOTION,
	DRAG_DROP,
	DRAG_DATA_RECEIVED,
	OWNER_CHANGE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static gboolean
boolean_handled_accumulator (GSignalInvocationHint *ihint,
                             GValue *return_accu,
                             const GValue *handler_return,
                             gpointer dummy)
{
	gboolean handled = g_value_get_boolean (handler_return);

	g_value_set_boolean (return_accu, handled);

	return !handled;
}

/* GLib allows adding signals to an existing type; do it before anything
 * connects to them, i.e. when the library loads. */
__attribute__((constructor))
static void
e_gtk_dnd_register_signals (void)
{
	GType type = GTK_TYPE_WIDGET;

	/* GtkClipboard::owner-change (void (GdkClipboard *, gpointer event)),
	 * emitted from GdkClipboard::changed with a NULL event */
	signals[OWNER_CHANGE] = g_signal_new ("owner-change", GDK_TYPE_CLIPBOARD,
		G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
		G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[DRAG_BEGIN] = g_signal_new ("drag-begin", type,
		G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_GTK_TYPE_DRAG_CONTEXT);

	signals[DRAG_END] = g_signal_new ("drag-end", type,
		G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_GTK_TYPE_DRAG_CONTEXT);

	signals[DRAG_DATA_GET] = g_signal_new ("drag-data-get", type,
		G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
		G_TYPE_NONE, 4, E_GTK_TYPE_DRAG_CONTEXT,
		E_GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT, G_TYPE_UINT);

	signals[DRAG_DATA_DELETE] = g_signal_new ("drag-data-delete", type,
		G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_GTK_TYPE_DRAG_CONTEXT);

	signals[DRAG_FAILED] = g_signal_new ("drag-failed", type,
		G_SIGNAL_RUN_LAST, 0, boolean_handled_accumulator, NULL, NULL,
		G_TYPE_BOOLEAN, 2, E_GTK_TYPE_DRAG_CONTEXT, G_TYPE_INT);

	signals[DRAG_LEAVE] = g_signal_new ("drag-leave", type,
		G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
		G_TYPE_NONE, 2, E_GTK_TYPE_DRAG_CONTEXT, G_TYPE_UINT);

	signals[DRAG_MOTION] = g_signal_new ("drag-motion", type,
		G_SIGNAL_RUN_LAST, 0, boolean_handled_accumulator, NULL, NULL,
		G_TYPE_BOOLEAN, 4, E_GTK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT, G_TYPE_INT, G_TYPE_UINT);

	signals[DRAG_DROP] = g_signal_new ("drag-drop", type,
		G_SIGNAL_RUN_LAST, 0, boolean_handled_accumulator, NULL, NULL,
		G_TYPE_BOOLEAN, 4, E_GTK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT, G_TYPE_INT, G_TYPE_UINT);

	signals[DRAG_DATA_RECEIVED] = g_signal_new ("drag-data-received", type,
		G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
		G_TYPE_NONE, 6, E_GTK_TYPE_DRAG_CONTEXT,
		G_TYPE_INT, G_TYPE_INT,
		E_GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		G_TYPE_UINT, G_TYPE_UINT);
}

/* ------------------------------------------------------------------ */
/* Atoms */

EGtkAtom
e_gtk_atom_intern (const gchar *name,
                   gboolean only_if_exists)
{
	return name ? g_intern_string (name) : NULL;
}

EGtkAtom
e_gtk_atom_intern_static_string (const gchar *name)
{
	return name ? g_intern_static_string (name) : NULL;
}

gchar *
e_gtk_atom_name (EGtkAtom atom)
{
	return g_strdup (atom);
}

static gboolean
atom_is_text (EGtkAtom atom)
{
	return atom && (
		g_str_has_prefix (atom, "text/plain") ||
		g_str_equal (atom, "UTF8_STRING") ||
		g_str_equal (atom, "STRING") ||
		g_str_equal (atom, "TEXT") ||
		g_str_equal (atom, "COMPOUND_TEXT"));
}

/* ------------------------------------------------------------------ */
/* Target lists */

typedef struct {
	EGtkAtom target;
	guint flags;
	guint info;
} TargetPair;

struct _EGtkTargetList {
	gint ref_count;
	GList *pairs; /* TargetPair */
};

G_DEFINE_BOXED_TYPE (EGtkTargetList, e_gtk_target_list, e_gtk_target_list_ref, e_gtk_target_list_unref)

EGtkTargetList *
e_gtk_target_list_new (const EGtkTargetEntry *targets,
                       guint ntargets)
{
	EGtkTargetList *list;

	list = g_new0 (EGtkTargetList, 1);
	list->ref_count = 1;

	if (targets)
		e_gtk_target_list_add_table (list, targets, ntargets);

	return list;
}

EGtkTargetList *
e_gtk_target_list_ref (EGtkTargetList *list)
{
	g_return_val_if_fail (list != NULL, NULL);

	g_atomic_int_inc (&list->ref_count);

	return list;
}

void
e_gtk_target_list_unref (EGtkTargetList *list)
{
	if (!list)
		return;

	if (g_atomic_int_dec_and_test (&list->ref_count)) {
		g_list_free_full (list->pairs, g_free);
		g_free (list);
	}
}

void
e_gtk_target_list_add (EGtkTargetList *list,
                       EGtkAtom target,
                       guint flags,
                       guint info)
{
	TargetPair *pair;

	g_return_if_fail (list != NULL);
	g_return_if_fail (target != NULL);

	pair = g_new0 (TargetPair, 1);
	pair->target = g_intern_string (target);
	pair->flags = flags;
	pair->info = info;

	list->pairs = g_list_append (list->pairs, pair);
}

void
e_gtk_target_list_add_table (EGtkTargetList *list,
                             const EGtkTargetEntry *targets,
                             guint ntargets)
{
	guint ii;

	g_return_if_fail (list != NULL);

	for (ii = 0; ii < ntargets; ii++)
		e_gtk_target_list_add (list, targets[ii].target, targets[ii].flags, targets[ii].info);
}

void
e_gtk_target_list_add_uri_targets (EGtkTargetList *list,
                                   guint info)
{
	e_gtk_target_list_add (list, "text/uri-list", 0, info);
}

void
e_gtk_target_list_add_text_targets (EGtkTargetList *list,
                                    guint info)
{
	e_gtk_target_list_add (list, "text/plain;charset=utf-8", 0, info);
	e_gtk_target_list_add (list, "UTF8_STRING", 0, info);
	e_gtk_target_list_add (list, "TEXT", 0, info);
	e_gtk_target_list_add (list, "STRING", 0, info);
	e_gtk_target_list_add (list, "text/plain", 0, info);
}

void
e_gtk_target_list_add_image_targets (EGtkTargetList *list,
                                     guint info,
                                     gboolean writable)
{
	e_gtk_target_list_add (list, "image/png", 0, info);
	e_gtk_target_list_add (list, "image/jpeg", 0, info);
	e_gtk_target_list_add (list, "image/bmp", 0, info);
}

static TargetPair *
target_list_lookup (EGtkTargetList *list,
                    EGtkAtom target)
{
	GList *link;

	if (!list || !target)
		return NULL;

	target = g_intern_string (target);

	for (link = list->pairs; link; link = g_list_next (link)) {
		TargetPair *pair = link->data;

		if (pair->target == target)
			return pair;
	}

	return NULL;
}

gboolean
e_gtk_target_list_find (EGtkTargetList *list,
                        EGtkAtom target,
                        guint *info)
{
	TargetPair *pair = target_list_lookup (list, target);

	if (pair && info)
		*info = pair->info;

	return pair != NULL;
}

GdkContentFormats *
e_gtk_target_list_to_formats (EGtkTargetList *list)
{
	GdkContentFormatsBuilder *builder;
	GList *link;

	builder = gdk_content_formats_builder_new ();

	for (link = list ? list->pairs : NULL; link; link = g_list_next (link)) {
		TargetPair *pair = link->data;

		gdk_content_formats_builder_add_mime_type (builder, pair->target);
	}

	return gdk_content_formats_builder_free_to_formats (builder);
}

EGtkTargetEntry *
e_gtk_target_table_new_from_list (EGtkTargetList *list,
                                  gint *n_targets)
{
	EGtkTargetEntry *table;
	GList *link;
	gint ii;

	g_return_val_if_fail (n_targets != NULL, NULL);

	*n_targets = list ? g_list_length (list->pairs) : 0;
	if (!*n_targets)
		return NULL;

	table = g_new0 (EGtkTargetEntry, *n_targets);

	for (link = list->pairs, ii = 0; link; link = g_list_next (link), ii++) {
		TargetPair *pair = link->data;

		table[ii].target = g_strdup (pair->target);
		table[ii].flags = pair->flags;
		table[ii].info = pair->info;
	}

	return table;
}

void
e_gtk_target_table_free (EGtkTargetEntry *targets,
                         gint n_targets)
{
	gint ii;

	for (ii = 0; ii < n_targets; ii++)
		g_free (targets[ii].target);

	g_free (targets);
}

/* ------------------------------------------------------------------ */
/* Selection data */

struct _EGtkSelectionData {
	EGtkAtom selection;
	EGtkAtom target;
	EGtkAtom type;
	gint format;
	guchar *data; /* always NUL-terminated, like with GTK 3 */
	gint length; /* -1 when there is no data */
	GdkDisplay *display;
};

G_DEFINE_BOXED_TYPE (EGtkSelectionData, e_gtk_selection_data, e_gtk_selection_data_copy, e_gtk_selection_data_free)

EGtkSelectionData *
e_gtk_selection_data_new (EGtkAtom selection,
                          EGtkAtom target)
{
	EGtkSelectionData *data;

	data = g_new0 (EGtkSelectionData, 1);
	data->selection = selection ? g_intern_string (selection) : NULL;
	data->target = target ? g_intern_string (target) : NULL;
	data->length = -1;
	data->display = gdk_display_get_default ();

	return data;
}

EGtkSelectionData *
e_gtk_selection_data_copy (const EGtkSelectionData *data)
{
	EGtkSelectionData *copy;

	g_return_val_if_fail (data != NULL, NULL);

	copy = g_new0 (EGtkSelectionData, 1);
	*copy = *data;

	if (data->data)
		copy->data = g_memdup2 (data->data, data->length + 1);

	return copy;
}

void
e_gtk_selection_data_free (EGtkSelectionData *data)
{
	if (!data)
		return;

	g_free (data->data);
	g_free (data);
}

EGtkAtom
e_gtk_selection_data_get_selection (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return data->selection;
}

EGtkAtom
e_gtk_selection_data_get_target (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return data->target;
}

EGtkAtom
e_gtk_selection_data_get_data_type (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return data->type;
}

gint
e_gtk_selection_data_get_format (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, 0);

	return data->format;
}

const guchar *
e_gtk_selection_data_get_data (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return data->data;
}

gint
e_gtk_selection_data_get_length (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, -1);

	return data->length;
}

const guchar *
e_gtk_selection_data_get_data_with_length (const EGtkSelectionData *data,
                                           gint *length)
{
	g_return_val_if_fail (data != NULL, NULL);

	if (length)
		*length = data->length;

	return data->data;
}

GdkDisplay *
e_gtk_selection_data_get_display (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return data->display;
}

void
e_gtk_selection_data_set (EGtkSelectionData *data,
                          EGtkAtom type,
                          gint format,
                          const guchar *bytes,
                          gint length)
{
	g_return_if_fail (data != NULL);

	g_clear_pointer (&data->data, g_free);

	data->type = type ? g_intern_string (type) : NULL;
	data->format = format;

	if (length < 0 || (!bytes && length > 0)) {
		data->length = -1;
		return;
	}

	data->data = g_malloc (length + 1);
	if (length > 0)
		memcpy (data->data, bytes, length);
	data->data[length] = '\0';
	data->length = length;
}

gboolean
e_gtk_selection_data_set_text (EGtkSelectionData *data,
                               const gchar *str,
                               gint len)
{
	g_return_val_if_fail (data != NULL, FALSE);

	if (!str)
		return FALSE;

	if (len < 0)
		len = strlen (str);

	e_gtk_selection_data_set (data,
		atom_is_text (data->target) ? data->target : g_intern_static_string ("text/plain;charset=utf-8"),
		8, (const guchar *) str, len);

	return TRUE;
}

guchar *
e_gtk_selection_data_get_text (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	if (data->length < 0 || !data->data)
		return NULL;

	if (!atom_is_text (data->type) && !atom_is_text (data->target))
		return NULL;

	return (guchar *) g_strndup ((const gchar *) data->data, data->length);
}

gboolean
e_gtk_selection_data_set_uris (EGtkSelectionData *data,
                               gchar **uris)
{
	GString *list;
	gint ii;

	g_return_val_if_fail (data != NULL, FALSE);

	list = g_string_new (NULL);

	for (ii = 0; uris && uris[ii]; ii++) {
		g_string_append (list, uris[ii]);
		g_string_append (list, "\r\n");
	}

	e_gtk_selection_data_set (data, "text/uri-list", 8, (const guchar *) list->str, list->len);
	g_string_free (list, TRUE);

	return TRUE;
}

gchar **
e_gtk_selection_data_get_uris (const EGtkSelectionData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	if (data->length < 0 || !data->data)
		return NULL;

	return g_uri_list_extract_uris ((const gchar *) data->data);
}

/* The "TARGETS" request stores an EGtkAtom array, as GTK 3 stored atoms */
gboolean
e_gtk_selection_data_get_targets (const EGtkSelectionData *data,
                                  EGtkAtom **targets,
                                  gint *n_atoms)
{
	g_return_val_if_fail (data != NULL, FALSE);

	if (data->length >= 0 && data->data && data->type == g_intern_static_string ("ATOM")) {
		if (targets)
			*targets = g_memdup2 (data->data, data->length);
		if (n_atoms)
			*n_atoms = data->length / sizeof (EGtkAtom);
		return TRUE;
	}

	if (targets)
		*targets = NULL;
	if (n_atoms)
		*n_atoms = -1;

	return FALSE;
}

gboolean
e_gtk_selection_data_targets_include_text (const EGtkSelectionData *data)
{
	EGtkAtom *targets = NULL;
	gint ii, n_targets = 0;
	gboolean result = FALSE;

	if (e_gtk_selection_data_get_targets (data, &targets, &n_targets)) {
		for (ii = 0; ii < n_targets && !result; ii++)
			result = atom_is_text (targets[ii]);
		g_free (targets);
	}

	return result;
}

gboolean
e_gtk_targets_include_text (EGtkAtom *targets,
                            gint n_targets)
{
	gint ii;

	for (ii = 0; ii < n_targets; ii++) {
		if (atom_is_text (targets[ii]))
			return TRUE;
	}

	return FALSE;
}

static void
selection_data_set_targets (EGtkSelectionData *data,
                            GdkContentFormats *formats)
{
	const gchar * const *mime_types;
	gsize n_mime_types = 0;

	mime_types = formats ? gdk_content_formats_get_mime_types (formats, &n_mime_types) : NULL;

	e_gtk_selection_data_set (data, "ATOM", 32,
		(const guchar *) mime_types, n_mime_types * sizeof (EGtkAtom));
}

/* ------------------------------------------------------------------ */
/* Drag context */

struct _EGtkDragContext {
	GObject parent;

	GdkDrag *drag; /* source side */
	GdkDrop *drop; /* destination side */
	GtkWidget *source_widget;
	GtkWidget *dest_widget;

	GdkDragAction actions;
	GdkDragAction suggested_action;
	GdkDragAction selected_action;
	GdkDragAction status_action;

	GList *targets; /* EGtkAtom */

	gint x, y;
	gboolean auto_finish;
	gboolean finished;
};

G_DEFINE_TYPE (EGtkDragContext, e_gtk_drag_context, G_TYPE_OBJECT)

static void
e_gtk_drag_context_dispose (GObject *object)
{
	EGtkDragContext *context = E_GTK_DRAG_CONTEXT (object);

	g_clear_object (&context->drop);
	g_clear_weak_pointer (&context->source_widget);
	g_clear_weak_pointer (&context->dest_widget);
	g_clear_pointer (&context->targets, g_list_free);

	G_OBJECT_CLASS (e_gtk_drag_context_parent_class)->dispose (object);
}

static void
e_gtk_drag_context_class_init (EGtkDragContextClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = e_gtk_drag_context_dispose;
}

static void
e_gtk_drag_context_init (EGtkDragContext *context)
{
}

static GdkDragAction
pick_action (GdkDragAction actions)
{
	if (actions & GDK_ACTION_COPY)
		return GDK_ACTION_COPY;
	if (actions & GDK_ACTION_MOVE)
		return GDK_ACTION_MOVE;
	if (actions & GDK_ACTION_LINK)
		return GDK_ACTION_LINK;
	if (actions & GDK_ACTION_ASK)
		return GDK_ACTION_ASK;

	return 0;
}

static EGtkDragContext *
drag_context_new_for_source (GtkWidget *widget,
                             EGtkTargetList *targets,
                             GdkDragAction actions)
{
	EGtkDragContext *context;
	GList *link;

	context = g_object_new (E_GTK_TYPE_DRAG_CONTEXT, NULL);
	context->actions = actions;
	context->suggested_action = pick_action (actions);
	context->selected_action = context->suggested_action;
	g_set_weak_pointer (&context->source_widget, widget);

	for (link = targets ? targets->pairs : NULL; link; link = g_list_next (link)) {
		TargetPair *pair = link->data;
		context->targets = g_list_append (context->targets, (gpointer) pair->target);
	}

	return context;
}

GdkDragAction
e_gtk_drag_context_get_actions (EGtkDragContext *context)
{
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), 0);

	return context->actions;
}

GdkDragAction
e_gtk_drag_context_get_suggested_action (EGtkDragContext *context)
{
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), 0);

	return context->suggested_action;
}

GdkDragAction
e_gtk_drag_context_get_selected_action (EGtkDragContext *context)
{
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), 0);

	if (context->drag && gdk_drag_get_selected_action (context->drag))
		return gdk_drag_get_selected_action (context->drag);

	return context->selected_action;
}

GList *
e_gtk_drag_context_list_targets (EGtkDragContext *context)
{
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), NULL);

	return context->targets;
}

GdkDrag *
e_gtk_drag_context_get_drag (EGtkDragContext *context)
{
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), NULL);

	return context->drag;
}

GdkDrop *
e_gtk_drag_context_get_drop (EGtkDragContext *context)
{
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), NULL);

	return context->drop;
}

void
e_gtk_drag_status (EGtkDragContext *context,
                   GdkDragAction action,
                   guint32 time_)
{
	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));

	context->status_action = action;
	if (action)
		context->selected_action = action;
}

/* ------------------------------------------------------------------ */
/* Content provider: produces the data on request, through drag-data-get
 * for drags and through the get function for the clipboard. */

#define E_GTK_TYPE_DATA_PROVIDER (e_gtk_data_provider_get_type ())
G_DECLARE_FINAL_TYPE (EGtkDataProvider, e_gtk_data_provider, E_GTK, DATA_PROVIDER, GdkContentProvider)

struct _EGtkDataProvider {
	GdkContentProvider parent;

	EGtkTargetList *targets;

	/* drags */
	GWeakRef widget;
	EGtkDragContext *context;

	/* clipboard */
	GdkClipboard *clipboard; /* not referenced, it lives with the display */
	EGtkClipboardGetFunc get_func;
	EGtkClipboardClearFunc clear_func;
	gpointer user_data;
	GWeakRef owner;
	gboolean with_owner;
};

G_DEFINE_TYPE (EGtkDataProvider, e_gtk_data_provider, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
e_gtk_data_provider_ref_formats (GdkContentProvider *provider)
{
	return e_gtk_target_list_to_formats (E_GTK_DATA_PROVIDER (provider)->targets);
}

static void
data_provider_write_done_cb (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GTask *task = user_data;
	GError *error = NULL;

	if (g_output_stream_write_all_finish (G_OUTPUT_STREAM (source), result, NULL, &error))
		g_task_return_boolean (task, TRUE);
	else
		g_task_return_error (task, error);

	g_object_unref (task);
}

static void
e_gtk_data_provider_write_mime_type_async (GdkContentProvider *provider,
                                           const gchar *mime_type,
                                           GOutputStream *stream,
                                           gint io_priority,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data)
{
	EGtkDataProvider *self = E_GTK_DATA_PROVIDER (provider);
	EGtkSelectionData *data;
	GTask *task;
	guint info = 0;

	task = g_task_new (provider, cancellable, callback, user_data);
	g_task_set_priority (task, io_priority);

	e_gtk_target_list_find (self->targets, mime_type, &info);

	if (self->get_func) {
		GObject *owner = self->with_owner ? g_weak_ref_get (&self->owner) : NULL;

		data = e_gtk_selection_data_new (E_GTK_SELECTION_CLIPBOARD, mime_type);
		if (!self->with_owner || owner)
			self->get_func (self->clipboard, data, info, self->with_owner ? (gpointer) owner : self->user_data);
		g_clear_object (&owner);
	} else {
		GtkWidget *widget = g_weak_ref_get (&self->widget);

		data = e_gtk_selection_data_new (NULL, mime_type);
		if (widget) {
			g_signal_emit (widget, signals[DRAG_DATA_GET], 0,
				self->context, data, info, (guint) GDK_CURRENT_TIME);
			g_object_unref (widget);
		}
	}

	if (data->length < 0) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			"No data for '%s'", mime_type);
		g_object_unref (task);
		e_gtk_selection_data_free (data);
		return;
	}

	g_task_set_task_data (task, data, (GDestroyNotify) e_gtk_selection_data_free);

	g_output_stream_write_all_async (stream, data->data, data->length,
		io_priority, cancellable, data_provider_write_done_cb, task);
}

static gboolean
e_gtk_data_provider_write_mime_type_finish (GdkContentProvider *provider,
                                            GAsyncResult *result,
                                            GError **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
e_gtk_data_provider_detach_clipboard (GdkContentProvider *provider,
                                      GdkClipboard *clipboard)
{
	EGtkDataProvider *self = E_GTK_DATA_PROVIDER (provider);

	if (self->clear_func) {
		GObject *owner = self->with_owner ? g_weak_ref_get (&self->owner) : NULL;

		if (!self->with_owner || owner)
			self->clear_func (clipboard, self->with_owner ? (gpointer) owner : self->user_data);
		g_clear_object (&owner);

		self->clear_func = NULL;
	}
}

static void
e_gtk_data_provider_finalize (GObject *object)
{
	EGtkDataProvider *self = E_GTK_DATA_PROVIDER (object);

	e_gtk_target_list_unref (self->targets);
	g_weak_ref_clear (&self->widget);
	g_weak_ref_clear (&self->owner);
	g_clear_object (&self->context);

	G_OBJECT_CLASS (e_gtk_data_provider_parent_class)->finalize (object);
}

static void
e_gtk_data_provider_class_init (EGtkDataProviderClass *klass)
{
	GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (klass);

	G_OBJECT_CLASS (klass)->finalize = e_gtk_data_provider_finalize;

	provider_class->ref_formats = e_gtk_data_provider_ref_formats;
	provider_class->write_mime_type_async = e_gtk_data_provider_write_mime_type_async;
	provider_class->write_mime_type_finish = e_gtk_data_provider_write_mime_type_finish;
	provider_class->detach_clipboard = e_gtk_data_provider_detach_clipboard;
}

static void
e_gtk_data_provider_init (EGtkDataProvider *self)
{
	g_weak_ref_init (&self->widget, NULL);
	g_weak_ref_init (&self->owner, NULL);
}

static GdkContentProvider *
data_provider_new_for_drag (GtkWidget *widget,
                            EGtkDragContext *context,
                            EGtkTargetList *targets)
{
	EGtkDataProvider *self;

	self = g_object_new (E_GTK_TYPE_DATA_PROVIDER, NULL);
	self->targets = targets ? e_gtk_target_list_ref (targets) : e_gtk_target_list_new (NULL, 0);
	self->context = g_object_ref (context);
	g_weak_ref_set (&self->widget, widget);

	return GDK_CONTENT_PROVIDER (self);
}

/* ------------------------------------------------------------------ */
/* Drag icons */

static GdkTexture *
texture_from_surface (cairo_surface_t *surface,
                      gint *hot_x,
                      gint *hot_y)
{
	cairo_surface_t *image;
	GdkTexture *texture;
	GBytes *bytes;
	gdouble dx = 0, dy = 0;
	gint width, height;

	cairo_surface_get_device_offset (surface, &dx, &dy);
	*hot_x = -dx;
	*hot_y = -dy;

	if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE &&
	    cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32) {
		image = cairo_surface_reference (surface);
	} else {
		cairo_t *cr;
		gdouble x1, y1, x2, y2;

		cr = cairo_create (surface);
		cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
		cairo_destroy (cr);

		image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, x2 - x1, y2 - y1);
		cr = cairo_create (image);
		cairo_set_source_surface (cr, surface, -x1, -y1);
		cairo_paint (cr);
		cairo_destroy (cr);
	}

	cairo_surface_flush (image);

	width = cairo_image_surface_get_width (image);
	height = cairo_image_surface_get_height (image);

	bytes = g_bytes_new (cairo_image_surface_get_data (image),
		cairo_image_surface_get_stride (image) * height);
	texture = gdk_memory_texture_new (width, height,
		G_BYTE_ORDER == G_LITTLE_ENDIAN ? GDK_MEMORY_B8G8R8A8_PREMULTIPLIED : GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
		bytes, cairo_image_surface_get_stride (image));

	g_bytes_unref (bytes);
	cairo_surface_destroy (image);

	return texture;
}

void
e_gtk_drag_set_icon_surface (EGtkDragContext *context,
                             cairo_surface_t *surface)
{
	GdkTexture *texture;
	gint hot_x = 0, hot_y = 0;

	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (surface != NULL);

	if (!context->drag)
		return;

	texture = texture_from_surface (surface, &hot_x, &hot_y);
	gtk_drag_icon_set_from_paintable (context->drag, GDK_PAINTABLE (texture), hot_x, hot_y);
	g_object_unref (texture);
}

void
e_gtk_drag_set_icon_paintable (EGtkDragContext *context,
			       GdkPaintable *paintable,
			       gint hot_x,
			       gint hot_y)
{
	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (GDK_IS_PAINTABLE (paintable));

	if (context->drag)
		gtk_drag_icon_set_from_paintable (context->drag, paintable, hot_x, hot_y);
}

void
e_gtk_drag_set_icon_pixbuf (EGtkDragContext *context,
                            GdkPixbuf *pixbuf,
                            gint hot_x,
                            gint hot_y)
{
	GdkTexture *texture;

	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

	if (!context->drag)
		return;

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	texture = gdk_texture_new_for_pixbuf (pixbuf);
	G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_drag_icon_set_from_paintable (context->drag, GDK_PAINTABLE (texture), hot_x, hot_y);
	g_object_unref (texture);
}

void
e_gtk_drag_set_icon_default (EGtkDragContext *context)
{
	GtkIconPaintable *icon;

	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));

	if (!context->drag)
		return;

	icon = gtk_icon_theme_lookup_icon (
		gtk_icon_theme_get_for_display (gdk_display_get_default ()),
		"text-x-generic", NULL, 32, 1, GTK_TEXT_DIR_NONE, 0);

	gtk_drag_icon_set_from_paintable (context->drag, GDK_PAINTABLE (icon), -2, -2);
	g_object_unref (icon);
}

GtkWidget *
e_gtk_drag_get_source_widget (EGtkDragContext *context)
{
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), NULL);

	return context->source_widget;
}

/* ------------------------------------------------------------------ */
/* Drag source */

typedef struct {
	GtkDragSource *controller;
	EGtkTargetList *targets;
	EGtkDragContext *context;
} SourceSite;

static void
source_site_free (gpointer ptr)
{
	SourceSite *site = ptr;

	e_gtk_target_list_unref (site->targets);
	g_clear_object (&site->context);
	g_free (site);
}

static GdkContentProvider *
drag_source_prepare_cb (GtkDragSource *source,
                        gdouble x,
                        gdouble y,
                        GtkWidget *widget)
{
	SourceSite *site = g_object_get_data (G_OBJECT (widget), SOURCE_SITE_KEY);

	if (!site || !site->targets || !site->targets->pairs)
		return NULL;

	g_clear_object (&site->context);
	site->context = drag_context_new_for_source (widget, site->targets,
		gtk_drag_source_get_actions (source));

	return data_provider_new_for_drag (widget, site->context, site->targets);
}

static void
drag_source_begin_cb (GtkDragSource *source,
                      GdkDrag *drag,
                      GtkWidget *widget)
{
	SourceSite *site = g_object_get_data (G_OBJECT (widget), SOURCE_SITE_KEY);

	if (!site || !site->context)
		return;

	site->context->drag = drag;
	g_object_set_data_full (G_OBJECT (drag), DRAG_CONTEXT_KEY,
		g_object_ref (site->context), g_object_unref);

	e_gtk_drag_set_icon_default (site->context);

	g_signal_emit (widget, signals[DRAG_BEGIN], 0, site->context);
}

static void
drag_source_end_cb (GtkDragSource *source,
                    GdkDrag *drag,
                    gboolean delete_data,
                    GtkWidget *widget)
{
	EGtkDragContext *context = g_object_get_data (G_OBJECT (drag), DRAG_CONTEXT_KEY);

	if (!context)
		return;

	g_object_ref (context);

	if (delete_data)
		g_signal_emit (widget, signals[DRAG_DATA_DELETE], 0, context);

	g_signal_emit (widget, signals[DRAG_END], 0, context);

	context->drag = NULL;
	g_object_unref (context);
}

static gboolean
drag_source_cancel_cb (GtkDragSource *source,
                       GdkDrag *drag,
                       GdkDragCancelReason reason,
                       GtkWidget *widget)
{
	EGtkDragContext *context = g_object_get_data (G_OBJECT (drag), DRAG_CONTEXT_KEY);
	gboolean handled = FALSE;

	if (context)
		g_signal_emit (widget, signals[DRAG_FAILED], 0, context, (gint) reason, &handled);

	return handled;
}

void
e_gtk_drag_source_set (GtkWidget *widget,
                       GdkModifierType start_button_mask,
                       const EGtkTargetEntry *targets,
                       gint n_targets,
                       GdkDragAction actions)
{
	SourceSite *site;
	guint button = 0;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	site = g_object_get_data (G_OBJECT (widget), SOURCE_SITE_KEY);
	if (!site) {
		site = g_new0 (SourceSite, 1);
		site->controller = gtk_drag_source_new ();

		g_signal_connect (site->controller, "prepare", G_CALLBACK (drag_source_prepare_cb), widget);
		g_signal_connect (site->controller, "drag-begin", G_CALLBACK (drag_source_begin_cb), widget);
		g_signal_connect (site->controller, "drag-end", G_CALLBACK (drag_source_end_cb), widget);
		g_signal_connect (site->controller, "drag-cancel", G_CALLBACK (drag_source_cancel_cb), widget);

		gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (site->controller));
		g_object_set_data_full (G_OBJECT (widget), SOURCE_SITE_KEY, site, source_site_free);
	}

	/* one button only; GTK 3 accepted a mask */
	if (start_button_mask == GDK_BUTTON1_MASK)
		button = 1;
	else if (start_button_mask == GDK_BUTTON2_MASK)
		button = 2;
	else if (start_button_mask == GDK_BUTTON3_MASK)
		button = 3;

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (site->controller), button);
	gtk_drag_source_set_actions (site->controller, actions);

	e_gtk_target_list_unref (site->targets);
	site->targets = e_gtk_target_list_new (targets, n_targets);
}

void
e_gtk_drag_source_unset (GtkWidget *widget)
{
	SourceSite *site;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	site = g_object_get_data (G_OBJECT (widget), SOURCE_SITE_KEY);
	if (site) {
		gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (site->controller));
		g_object_set_data (G_OBJECT (widget), SOURCE_SITE_KEY, NULL);
	}
}

void
e_gtk_drag_source_set_target_list (GtkWidget *widget,
                                   EGtkTargetList *target_list)
{
	SourceSite *site;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	site = g_object_get_data (G_OBJECT (widget), SOURCE_SITE_KEY);
	if (!site) {
		g_warning ("%s: %s is not a drag source", G_STRFUNC, G_OBJECT_TYPE_NAME (widget));
		return;
	}

	if (target_list)
		e_gtk_target_list_ref (target_list);
	e_gtk_target_list_unref (site->targets);
	site->targets = target_list;
}

EGtkTargetList *
e_gtk_drag_source_get_target_list (GtkWidget *widget)
{
	SourceSite *site;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	site = g_object_get_data (G_OBJECT (widget), SOURCE_SITE_KEY);

	return site ? site->targets : NULL;
}

typedef struct {
	GWeakRef widget;
	EGtkDragContext *context;
} ManualDrag;

static void
manual_drag_free (gpointer ptr)
{
	ManualDrag *md = ptr;

	g_weak_ref_clear (&md->widget);
	g_clear_object (&md->context);
	g_free (md);
}

static void
manual_drag_finish (GdkDrag *drag,
                    ManualDrag *md,
                    gboolean success)
{
	GtkWidget *widget = g_weak_ref_get (&md->widget);

	if (widget && md->context) {
		if (success && gdk_drag_get_selected_action (drag) == GDK_ACTION_MOVE)
			g_signal_emit (widget, signals[DRAG_DATA_DELETE], 0, md->context);

		g_signal_emit (widget, signals[DRAG_END], 0, md->context);
		md->context->drag = NULL;
	}

	g_clear_object (&widget);
	g_object_set_data (G_OBJECT (drag), "e-gtk-manual-drag", NULL);
}

static void
manual_drag_dnd_finished_cb (GdkDrag *drag,
                             ManualDrag *md)
{
	manual_drag_finish (drag, md, TRUE);
}

static void
manual_drag_drop_performed_cb (GdkDrag *drag,
                               ManualDrag *md)
{
	gdk_drag_drop_done (drag, TRUE);
}

static void
manual_drag_cancel_cb (GdkDrag *drag,
                       GdkDragCancelReason reason,
                       ManualDrag *md)
{
	GtkWidget *widget = g_weak_ref_get (&md->widget);
	gboolean handled = FALSE;

	if (widget && md->context)
		g_signal_emit (widget, signals[DRAG_FAILED], 0, md->context, (gint) reason, &handled);

	g_clear_object (&widget);

	manual_drag_finish (drag, md, FALSE);
}

EGtkDragContext *
e_gtk_drag_begin_with_coordinates (GtkWidget *widget,
                                   EGtkTargetList *targets,
                                   GdkDragAction actions,
                                   gint button,
                                   GdkEvent *event,
                                   gint x,
                                   gint y)
{
	GtkNative *native;
	GdkSurface *surface;
	GdkDevice *device = NULL;
	GdkContentProvider *provider;
	EGtkDragContext *context;
	ManualDrag *md;
	GdkDrag *drag;
	gdouble sx = 0, sy = 0;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	native = gtk_widget_get_native (widget);
	if (!native)
		return NULL;

	surface = gtk_native_get_surface (native);

	if (event)
		device = gdk_event_get_device (event);
	if (!device)
		device = gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (widget)));

	if (x >= 0 && y >= 0) {
		graphene_point_t point;
		gdouble nx = 0, ny = 0;

		if (gtk_widget_compute_point (widget, GTK_WIDGET (native), &GRAPHENE_POINT_INIT (x, y), &point)) {
			gtk_native_get_surface_transform (native, &nx, &ny);
			sx = point.x + nx;
			sy = point.y + ny;
		}
	} else if (!event || !gdk_event_get_position (event, &sx, &sy)) {
		gdk_surface_get_device_position (surface, device, &sx, &sy, NULL);
	}

	context = drag_context_new_for_source (widget, targets, actions);
	provider = data_provider_new_for_drag (widget, context, targets);

	drag = gdk_drag_begin (surface, device, provider, actions, sx, sy);
	g_object_unref (provider);

	if (!drag) {
		g_object_unref (context);
		return NULL;
	}

	context->drag = drag;

	md = g_new0 (ManualDrag, 1);
	g_weak_ref_init (&md->widget, widget);
	md->context = context; /* takes the reference */

	g_object_set_data_full (G_OBJECT (drag), DRAG_CONTEXT_KEY, g_object_ref (context), g_object_unref);
	g_object_set_data_full (G_OBJECT (drag), "e-gtk-manual-drag", md, manual_drag_free);

	g_signal_connect (drag, "drop-performed", G_CALLBACK (manual_drag_drop_performed_cb), md);
	g_signal_connect (drag, "dnd-finished", G_CALLBACK (manual_drag_dnd_finished_cb), md);
	g_signal_connect (drag, "cancel", G_CALLBACK (manual_drag_cancel_cb), md);

	e_gtk_drag_set_icon_default (context);
	g_signal_emit (widget, signals[DRAG_BEGIN], 0, context);

	/* GTK holds the drag; the context lives as long as the drag */
	g_object_unref (drag);

	return context;
}

EGtkDragContext *
e_gtk_drag_begin (GtkWidget *widget,
                  EGtkTargetList *targets,
                  GdkDragAction actions,
                  gint button,
                  GdkEvent *event)
{
	return e_gtk_drag_begin_with_coordinates (widget, targets, actions, button, event, -1, -1);
}

void
e_gtk_drag_cancel (EGtkDragContext *context)
{
	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));

	if (context->drag)
		gdk_drag_drop_done (context->drag, FALSE);
}

/* ------------------------------------------------------------------ */
/* Drag destination */

typedef struct {
	GtkWidget *widget;
	GtkDropTargetAsync *controller;
	EGtkDestDefaults flags;
	EGtkTargetList *targets;
	EGtkDragContext *context;
	gboolean leave_sent;
} DestSite;

static void
dest_site_free (gpointer ptr)
{
	DestSite *site = ptr;

	e_gtk_target_list_unref (site->targets);
	g_clear_object (&site->context);
	g_free (site);
}

static EGtkDragContext *
dest_context_for_drop (DestSite *site,
                       GdkDrop *drop)
{
	EGtkDragContext *context;
	GdkContentFormats *formats;
	const gchar * const *mime_types;
	gsize ii, n_mime_types = 0;
	GdkDrag *drag;

	if (site->context && site->context->drop == drop)
		return site->context;

	g_clear_object (&site->context);

	context = g_object_new (E_GTK_TYPE_DRAG_CONTEXT, NULL);
	context->drop = g_object_ref (drop);
	context->actions = gdk_drop_get_actions (drop);
	context->suggested_action = pick_action (context->actions);
	context->selected_action = context->suggested_action;
	g_set_weak_pointer (&context->dest_widget, site->widget);

	formats = gdk_drop_get_formats (drop);
	mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);
	for (ii = 0; ii < n_mime_types; ii++)
		context->targets = g_list_append (context->targets, (gpointer) g_intern_string (mime_types[ii]));

	/* a drag from this application: find its source widget */
	drag = gdk_drop_get_drag (drop);
	if (drag) {
		EGtkDragContext *source_context = g_object_get_data (G_OBJECT (drag), DRAG_CONTEXT_KEY);

		if (source_context)
			g_set_weak_pointer (&context->source_widget, source_context->source_widget);
	}

	site->context = context;

	return context;
}

static GdkDragAction
dest_handle_motion (DestSite *site,
                    GdkDrop *drop,
                    gdouble x,
                    gdouble y)
{
	EGtkDragContext *context = dest_context_for_drop (site, drop);
	gboolean handled = FALSE;

	context->x = x;
	context->y = y;
	context->actions = gdk_drop_get_actions (drop);
	context->suggested_action = pick_action (context->actions);
	context->status_action = 0;

	g_signal_emit (site->widget, signals[DRAG_MOTION], 0, context,
		(gint) x, (gint) y, (guint) GDK_CURRENT_TIME, &handled);

	if (!handled && (site->flags & E_GTK_DEST_DEFAULT_MOTION) != 0) {
		if (e_gtk_drag_dest_find_target (site->widget, context, NULL))
			e_gtk_drag_status (context, context->suggested_action, GDK_CURRENT_TIME);
	}

	/* GTK wants one of the offered actions */
	if (!(context->status_action & context->actions))
		return 0;

	return context->status_action;
}

static GdkDragAction
dest_drag_enter_cb (GtkDropTargetAsync *target,
                    GdkDrop *drop,
                    gdouble x,
                    gdouble y,
                    DestSite *site)
{
	site->leave_sent = FALSE;

	return dest_handle_motion (site, drop, x, y);
}

static GdkDragAction
dest_drag_motion_cb (GtkDropTargetAsync *target,
                     GdkDrop *drop,
                     gdouble x,
                     gdouble y,
                     DestSite *site)
{
	return dest_handle_motion (site, drop, x, y);
}

static void
dest_drag_leave_cb (GtkDropTargetAsync *target,
                    GdkDrop *drop,
                    DestSite *site)
{
	if (site->leave_sent)
		return;

	site->leave_sent = TRUE;
	g_signal_emit (site->widget, signals[DRAG_LEAVE], 0,
		dest_context_for_drop (site, drop), (guint) GDK_CURRENT_TIME);
}

static gboolean
dest_drop_cb (GtkDropTargetAsync *target,
              GdkDrop *drop,
              gdouble x,
              gdouble y,
              DestSite *site)
{
	EGtkDragContext *context = dest_context_for_drop (site, drop);
	gboolean handled = FALSE;

	g_object_ref (context);

	context->x = x;
	context->y = y;
	if (context->status_action)
		context->selected_action = context->status_action;

	/* GTK 3 sent drag-leave before drag-drop */
	if (!site->leave_sent) {
		site->leave_sent = TRUE;
		g_signal_emit (site->widget, signals[DRAG_LEAVE], 0, context, (guint) GDK_CURRENT_TIME);
	}

	g_signal_emit (site->widget, signals[DRAG_DROP], 0, context,
		(gint) x, (gint) y, (guint) GDK_CURRENT_TIME, &handled);

	if (!handled && (site->flags & E_GTK_DEST_DEFAULT_DROP) != 0) {
		EGtkAtom found = e_gtk_drag_dest_find_target (site->widget, context, NULL);

		if (found) {
			context->auto_finish = TRUE;
			e_gtk_drag_get_data (site->widget, context, found, GDK_CURRENT_TIME);
			handled = TRUE;
		}
	}

	g_object_unref (context);

	return handled;
}

static DestSite *
dest_site_ensure (GtkWidget *widget,
                  GdkDragAction actions)
{
	DestSite *site;

	site = g_object_get_data (G_OBJECT (widget), DEST_SITE_KEY);
	if (!site) {
		site = g_new0 (DestSite, 1);
		site->widget = widget;
		site->controller = gtk_drop_target_async_new (NULL, actions);

		g_signal_connect (site->controller, "drag-enter", G_CALLBACK (dest_drag_enter_cb), site);
		g_signal_connect (site->controller, "drag-motion", G_CALLBACK (dest_drag_motion_cb), site);
		g_signal_connect (site->controller, "drag-leave", G_CALLBACK (dest_drag_leave_cb), site);
		g_signal_connect (site->controller, "drop", G_CALLBACK (dest_drop_cb), site);

		gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (site->controller));
		g_object_set_data_full (G_OBJECT (widget), DEST_SITE_KEY, site, dest_site_free);
	} else {
		gtk_drop_target_async_set_actions (site->controller, actions);
	}

	return site;
}

static void
dest_site_update_formats (DestSite *site)
{
	GdkContentFormats *formats = NULL;

	/* no targets: accept anything, the handlers decide */
	if (site->targets && site->targets->pairs)
		formats = e_gtk_target_list_to_formats (site->targets);

	gtk_drop_target_async_set_formats (site->controller, formats);

	if (formats)
		gdk_content_formats_unref (formats);
}

void
e_gtk_drag_dest_set (GtkWidget *widget,
                     EGtkDestDefaults flags,
                     const EGtkTargetEntry *targets,
                     gint n_targets,
                     GdkDragAction actions)
{
	DestSite *site;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	site = dest_site_ensure (widget, actions);
	site->flags = flags;

	e_gtk_target_list_unref (site->targets);
	site->targets = targets ? e_gtk_target_list_new (targets, n_targets) : NULL;

	dest_site_update_formats (site);
}

void
e_gtk_drag_dest_unset (GtkWidget *widget)
{
	DestSite *site;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	site = g_object_get_data (G_OBJECT (widget), DEST_SITE_KEY);
	if (site) {
		gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (site->controller));
		g_object_set_data (G_OBJECT (widget), DEST_SITE_KEY, NULL);
	}
}

void
e_gtk_drag_dest_set_target_list (GtkWidget *widget,
                                 EGtkTargetList *target_list)
{
	DestSite *site;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	site = g_object_get_data (G_OBJECT (widget), DEST_SITE_KEY);
	if (!site) {
		g_warning ("%s: %s is not a drag destination", G_STRFUNC, G_OBJECT_TYPE_NAME (widget));
		return;
	}

	if (target_list)
		e_gtk_target_list_ref (target_list);
	e_gtk_target_list_unref (site->targets);
	site->targets = target_list;

	dest_site_update_formats (site);
}

EGtkTargetList *
e_gtk_drag_dest_get_target_list (GtkWidget *widget)
{
	DestSite *site;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	site = g_object_get_data (G_OBJECT (widget), DEST_SITE_KEY);

	return site ? site->targets : NULL;
}

static void
dest_add_targets (GtkWidget *widget,
                  void (*add) (EGtkTargetList *list, guint info))
{
	DestSite *site;

	g_return_if_fail (GTK_IS_WIDGET (widget));

	site = g_object_get_data (G_OBJECT (widget), DEST_SITE_KEY);
	if (!site)
		return;

	if (!site->targets)
		site->targets = e_gtk_target_list_new (NULL, 0);

	add (site->targets, 0);
	dest_site_update_formats (site);
}

void
e_gtk_drag_dest_add_uri_targets (GtkWidget *widget)
{
	dest_add_targets (widget, e_gtk_target_list_add_uri_targets);
}

void
e_gtk_drag_dest_add_text_targets (GtkWidget *widget)
{
	dest_add_targets (widget, e_gtk_target_list_add_text_targets);
}

EGtkAtom
e_gtk_drag_dest_find_target (GtkWidget *widget,
                             EGtkDragContext *context,
                             EGtkTargetList *target_list)
{
	GList *link;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (E_GTK_IS_DRAG_CONTEXT (context), NULL);

	if (!target_list)
		target_list = e_gtk_drag_dest_get_target_list (widget);

	if (!target_list)
		return NULL;

	for (link = target_list->pairs; link; link = g_list_next (link)) {
		TargetPair *pair = link->data;

		if ((pair->flags & E_GTK_TARGET_SAME_APP) && !context->source_widget)
			continue;
		if ((pair->flags & E_GTK_TARGET_OTHER_APP) && context->source_widget)
			continue;
		if ((pair->flags & E_GTK_TARGET_SAME_WIDGET) && context->source_widget != widget)
			continue;
		if ((pair->flags & E_GTK_TARGET_OTHER_WIDGET) && context->source_widget == widget)
			continue;

		if (g_list_find (context->targets, pair->target))
			return pair->target;
	}

	return NULL;
}

typedef struct {
	GtkWidget *widget;
	EGtkDragContext *context;
	EGtkAtom target;
	GOutputStream *output;
} GetDataInfo;

static void
get_data_info_free (GetDataInfo *gdi)
{
	g_clear_object (&gdi->widget);
	g_clear_object (&gdi->context);
	g_clear_object (&gdi->output);
	g_free (gdi);
}

static void
get_data_deliver (GetDataInfo *gdi,
                  const gchar *mime_type,
                  GBytes *bytes)
{
	EGtkSelectionData *data;
	EGtkTargetList *targets;
	guint info = 0;

	data = e_gtk_selection_data_new (NULL, gdi->target);

	if (bytes) {
		gsize size = 0;
		gconstpointer ptr = g_bytes_get_data (bytes, &size);

		e_gtk_selection_data_set (data, mime_type ? mime_type : gdi->target, 8, ptr, size);
	}

	targets = e_gtk_drag_dest_get_target_list (gdi->widget);
	e_gtk_target_list_find (targets, gdi->target, &info);

	g_signal_emit (gdi->widget, signals[DRAG_DATA_RECEIVED], 0, gdi->context,
		gdi->context->x, gdi->context->y, data, info, (guint) GDK_CURRENT_TIME);

	if (gdi->context->auto_finish)
		e_gtk_drag_finish (gdi->context, data->length >= 0,
			gdi->context->selected_action == GDK_ACTION_MOVE, GDK_CURRENT_TIME);

	e_gtk_selection_data_free (data);
}

static void
get_data_splice_cb (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GetDataInfo *gdi = user_data;
	GBytes *bytes = NULL;

	if (g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, NULL) >= 0)
		bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (gdi->output));

	get_data_deliver (gdi, g_object_get_data (G_OBJECT (gdi->output), "mime-type"), bytes);

	if (bytes)
		g_bytes_unref (bytes);
	get_data_info_free (gdi);
}

static void
get_data_read_cb (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
	GetDataInfo *gdi = user_data;
	GInputStream *input;
	const gchar *mime_type = NULL;

	input = gdk_drop_read_finish (GDK_DROP (source), result, &mime_type, NULL);
	if (!input) {
		get_data_deliver (gdi, NULL, NULL);
		get_data_info_free (gdi);
		return;
	}

	gdi->output = g_memory_output_stream_new_resizable ();
	g_object_set_data_full (G_OBJECT (gdi->output), "mime-type", g_strdup (mime_type), g_free);

	g_output_stream_splice_async (gdi->output, input,
		G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
		G_PRIORITY_DEFAULT, NULL, get_data_splice_cb, gdi);

	g_object_unref (input);
}

void
e_gtk_drag_get_data (GtkWidget *widget,
                     EGtkDragContext *context,
                     EGtkAtom target,
                     guint32 time_)
{
	GetDataInfo *gdi;
	const gchar *mime_types[2] = { NULL, NULL };

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));
	g_return_if_fail (target != NULL);

	gdi = g_new0 (GetDataInfo, 1);
	gdi->widget = g_object_ref (widget);
	gdi->context = g_object_ref (context);
	gdi->target = g_intern_string (target);

	if (!context->drop) {
		get_data_deliver (gdi, NULL, NULL);
		get_data_info_free (gdi);
		return;
	}

	mime_types[0] = gdi->target;

	gdk_drop_read_async (context->drop, mime_types, G_PRIORITY_DEFAULT, NULL, get_data_read_cb, gdi);
}

void
e_gtk_drag_finish (EGtkDragContext *context,
                   gboolean success,
                   gboolean del,
                   guint32 time_)
{
	GdkDragAction action = 0;

	g_return_if_fail (E_GTK_IS_DRAG_CONTEXT (context));

	if (!context->drop || context->finished)
		return;

	if (success) {
		action = del ? GDK_ACTION_MOVE : context->selected_action;
		if (!(action & gdk_drop_get_actions (context->drop)))
			action = pick_action (gdk_drop_get_actions (context->drop));
	}

	context->finished = TRUE;
	gdk_drop_finish (context->drop, action);
}

/* ------------------------------------------------------------------ */
/* Clipboard */

static void
clipboard_changed_cb (GdkClipboard *clipboard,
		      gpointer user_data)
{
	g_signal_emit (clipboard, signals[OWNER_CHANGE], 0, NULL);
}

GdkClipboard *
e_gtk_clipboard_get_for_display (GdkDisplay *display,
                                 EGtkAtom selection)
{
	GdkClipboard *clipboard;

	g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

	if (selection && g_str_equal (selection, "PRIMARY"))
		clipboard = gdk_display_get_primary_clipboard (display);
	else
		clipboard = gdk_display_get_clipboard (display);

	if (!g_object_get_data (G_OBJECT (clipboard), "e-gtk-owner-change")) {
		g_object_set_data (G_OBJECT (clipboard), "e-gtk-owner-change", GINT_TO_POINTER (1));
		g_signal_connect (clipboard, "changed", G_CALLBACK (clipboard_changed_cb), NULL);
	}

	return clipboard;
}

GdkClipboard *
e_gtk_clipboard_get (EGtkAtom selection)
{
	return e_gtk_clipboard_get_for_display (gdk_display_get_default (), selection);
}

void
e_gtk_clipboard_set_text (GdkClipboard *clipboard,
                          const gchar *text,
                          gint len)
{
	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

	if (len < 0) {
		gdk_clipboard_set_text (clipboard, text);
	} else {
		gchar *copy = g_strndup (text, len);

		gdk_clipboard_set_text (clipboard, copy);
		g_free (copy);
	}
}

void
e_gtk_clipboard_set_image (GdkClipboard *clipboard,
                           GdkPixbuf *pixbuf)
{
	GdkTexture *texture;

	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
	g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	texture = gdk_texture_new_for_pixbuf (pixbuf);
	G_GNUC_END_IGNORE_DEPRECATIONS
	gdk_clipboard_set_texture (clipboard, texture);
	g_object_unref (texture);
}

static gboolean
clipboard_set_provider (GdkClipboard *clipboard,
                        const EGtkTargetEntry *targets,
                        guint n_targets,
                        EGtkClipboardGetFunc get_func,
                        EGtkClipboardClearFunc clear_func,
                        gpointer user_data,
                        GObject *owner)
{
	EGtkDataProvider *provider;
	gboolean success;

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);
	g_return_val_if_fail (get_func != NULL, FALSE);

	provider = g_object_new (E_GTK_TYPE_DATA_PROVIDER, NULL);
	provider->targets = e_gtk_target_list_new (targets, n_targets);
	provider->clipboard = clipboard;
	provider->get_func = get_func;
	provider->clear_func = clear_func;
	provider->user_data = user_data;
	if (owner) {
		provider->with_owner = TRUE;
		g_weak_ref_set (&provider->owner, owner);
	}

	success = gdk_clipboard_set_content (clipboard, GDK_CONTENT_PROVIDER (provider));
	g_object_unref (provider);

	return success;
}

gboolean
e_gtk_clipboard_set_with_data (GdkClipboard *clipboard,
                               const EGtkTargetEntry *targets,
                               guint n_targets,
                               EGtkClipboardGetFunc get_func,
                               EGtkClipboardClearFunc clear_func,
                               gpointer user_data)
{
	return clipboard_set_provider (clipboard, targets, n_targets, get_func, clear_func, user_data, NULL);
}

gboolean
e_gtk_clipboard_set_with_owner (GdkClipboard *clipboard,
                                const EGtkTargetEntry *targets,
                                guint n_targets,
                                EGtkClipboardGetFunc get_func,
                                EGtkClipboardClearFunc clear_func,
                                GObject *owner)
{
	g_return_val_if_fail (G_IS_OBJECT (owner), FALSE);

	return clipboard_set_provider (clipboard, targets, n_targets, get_func, clear_func, NULL, owner);
}

GObject *
e_gtk_clipboard_get_owner (GdkClipboard *clipboard)
{
	GdkContentProvider *content;
	GObject *owner;

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

	content = gdk_clipboard_get_content (clipboard);
	if (!E_GTK_IS_DATA_PROVIDER (content) || !E_GTK_DATA_PROVIDER (content)->with_owner)
		return NULL;

	/* GTK 3 returned it without a reference */
	owner = g_weak_ref_get (&E_GTK_DATA_PROVIDER (content)->owner);
	if (owner)
		g_object_unref (owner);

	return owner;
}

void
e_gtk_clipboard_clear (GdkClipboard *clipboard)
{
	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

	if (gdk_clipboard_is_local (clipboard))
		gdk_clipboard_set_content (clipboard, NULL);
}

void
e_gtk_clipboard_store (GdkClipboard *clipboard)
{
	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));

	if (gdk_clipboard_is_local (clipboard))
		gdk_clipboard_store_async (clipboard, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
}

void
e_gtk_clipboard_set_can_store (GdkClipboard *clipboard,
                               const EGtkTargetEntry *targets,
                               gint n_targets)
{
	/* GdkClipboard stores whatever it holds */
}

typedef struct {
	GdkClipboard *clipboard;
	EGtkAtom target;
	GOutputStream *output;
	gchar *mime_type;
	EGtkClipboardTextReceivedFunc text_func;
	EGtkClipboardReceivedFunc contents_func;
	EGtkClipboardTargetsReceivedFunc targets_func;
	gpointer user_data;
} ClipboardRequest;

static void
clipboard_request_free (ClipboardRequest *cr)
{
	g_clear_object (&cr->clipboard);
	g_clear_object (&cr->output);
	g_free (cr->mime_type);
	g_free (cr);
}

static void
clipboard_read_text_cb (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
	ClipboardRequest *cr = user_data;
	gchar *text;

	text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (source), result, NULL);
	cr->text_func (cr->clipboard, text, cr->user_data);

	g_free (text);
	clipboard_request_free (cr);
}

void
e_gtk_clipboard_request_text (GdkClipboard *clipboard,
                              EGtkClipboardTextReceivedFunc callback,
                              gpointer user_data)
{
	ClipboardRequest *cr;

	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
	g_return_if_fail (callback != NULL);

	cr = g_new0 (ClipboardRequest, 1);
	cr->clipboard = g_object_ref (clipboard);
	cr->text_func = callback;
	cr->user_data = user_data;

	gdk_clipboard_read_text_async (clipboard, NULL, clipboard_read_text_cb, cr);
}

static void
clipboard_deliver_contents (ClipboardRequest *cr,
                            GBytes *bytes)
{
	EGtkSelectionData *data;

	data = e_gtk_selection_data_new (E_GTK_SELECTION_CLIPBOARD, cr->target);

	if (bytes) {
		gsize size = 0;
		gconstpointer ptr = g_bytes_get_data (bytes, &size);

		e_gtk_selection_data_set (data, cr->mime_type ? cr->mime_type : cr->target, 8, ptr, size);
	}

	cr->contents_func (cr->clipboard, data, cr->user_data);
	e_gtk_selection_data_free (data);
}

static void
clipboard_splice_cb (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	ClipboardRequest *cr = user_data;
	GBytes *bytes = NULL;

	if (g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, NULL) >= 0)
		bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (cr->output));

	clipboard_deliver_contents (cr, bytes);

	if (bytes)
		g_bytes_unref (bytes);
	clipboard_request_free (cr);
}

static void
clipboard_read_cb (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
	ClipboardRequest *cr = user_data;
	GInputStream *input;
	const gchar *mime_type = NULL;

	input = gdk_clipboard_read_finish (GDK_CLIPBOARD (source), result, &mime_type, NULL);
	if (!input) {
		clipboard_deliver_contents (cr, NULL);
		clipboard_request_free (cr);
		return;
	}

	cr->mime_type = g_strdup (mime_type);
	cr->output = g_memory_output_stream_new_resizable ();

	g_output_stream_splice_async (cr->output, input,
		G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
		G_PRIORITY_DEFAULT, NULL, clipboard_splice_cb, cr);

	g_object_unref (input);
}

static gboolean
clipboard_targets_idle_cb (gpointer user_data)
{
	ClipboardRequest *cr = user_data;

	if (cr->targets_func) {
		const gchar * const *mime_types;
		gsize n_mime_types = 0;

		mime_types = gdk_content_formats_get_mime_types (
			gdk_clipboard_get_formats (cr->clipboard), &n_mime_types);

		cr->targets_func (cr->clipboard, (EGtkAtom *) mime_types, n_mime_types, cr->user_data);
	} else {
		EGtkSelectionData *data;

		data = e_gtk_selection_data_new (E_GTK_SELECTION_CLIPBOARD, cr->target);
		selection_data_set_targets (data, gdk_clipboard_get_formats (cr->clipboard));
		cr->contents_func (cr->clipboard, data, cr->user_data);
		e_gtk_selection_data_free (data);
	}

	clipboard_request_free (cr);

	return G_SOURCE_REMOVE;
}

void
e_gtk_clipboard_request_contents (GdkClipboard *clipboard,
                                  EGtkAtom target,
                                  EGtkClipboardReceivedFunc callback,
                                  gpointer user_data)
{
	ClipboardRequest *cr;
	const gchar *mime_types[2] = { NULL, NULL };

	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
	g_return_if_fail (target != NULL);
	g_return_if_fail (callback != NULL);

	cr = g_new0 (ClipboardRequest, 1);
	cr->clipboard = g_object_ref (clipboard);
	cr->target = g_intern_string (target);
	cr->contents_func = callback;
	cr->user_data = user_data;

	if (g_str_equal (target, "TARGETS")) {
		g_idle_add (clipboard_targets_idle_cb, cr);
		return;
	}

	mime_types[0] = cr->target;
	gdk_clipboard_read_async (clipboard, mime_types, G_PRIORITY_DEFAULT, NULL, clipboard_read_cb, cr);
}

void
e_gtk_clipboard_request_targets (GdkClipboard *clipboard,
                                 EGtkClipboardTargetsReceivedFunc callback,
                                 gpointer user_data)
{
	ClipboardRequest *cr;

	g_return_if_fail (GDK_IS_CLIPBOARD (clipboard));
	g_return_if_fail (callback != NULL);

	cr = g_new0 (ClipboardRequest, 1);
	cr->clipboard = g_object_ref (clipboard);
	cr->targets_func = callback;
	cr->user_data = user_data;

	g_idle_add (clipboard_targets_idle_cb, cr);
}

/* The wait functions spin a main loop, which is what GTK 3 did as well */

typedef struct {
	GMainLoop *loop;
	gchar *text;
	EGtkSelectionData *data;
	GdkTexture *texture;
} WaitData;

static void
wait_text_cb (GdkClipboard *clipboard,
              const gchar *text,
              gpointer user_data)
{
	WaitData *wd = user_data;

	wd->text = g_strdup (text);
	g_main_loop_quit (wd->loop);
}

gchar *
e_gtk_clipboard_wait_for_text (GdkClipboard *clipboard)
{
	WaitData wd = { NULL, };

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

	wd.loop = g_main_loop_new (NULL, FALSE);
	e_gtk_clipboard_request_text (clipboard, wait_text_cb, &wd);
	g_main_loop_run (wd.loop);
	g_main_loop_unref (wd.loop);

	return wd.text;
}

static void
wait_contents_cb (GdkClipboard *clipboard,
                  EGtkSelectionData *data,
                  gpointer user_data)
{
	WaitData *wd = user_data;

	wd->data = e_gtk_selection_data_copy (data);
	g_main_loop_quit (wd->loop);
}

EGtkSelectionData *
e_gtk_clipboard_wait_for_contents (GdkClipboard *clipboard,
                                   EGtkAtom target)
{
	WaitData wd = { NULL, };

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

	wd.loop = g_main_loop_new (NULL, FALSE);
	e_gtk_clipboard_request_contents (clipboard, target, wait_contents_cb, &wd);
	g_main_loop_run (wd.loop);
	g_main_loop_unref (wd.loop);

	if (wd.data && wd.data->length < 0)
		g_clear_pointer (&wd.data, e_gtk_selection_data_free);

	return wd.data;
}

gboolean
e_gtk_clipboard_wait_for_targets (GdkClipboard *clipboard,
                                  EGtkAtom **targets,
                                  gint *n_targets)
{
	const gchar * const *mime_types;
	gsize n_mime_types = 0;

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

	mime_types = gdk_content_formats_get_mime_types (gdk_clipboard_get_formats (clipboard), &n_mime_types);

	if (targets)
		*targets = n_mime_types ? g_memdup2 (mime_types, n_mime_types * sizeof (EGtkAtom)) : NULL;
	if (n_targets)
		*n_targets = n_mime_types;

	return n_mime_types > 0;
}

gboolean
e_gtk_clipboard_wait_is_text_available (GdkClipboard *clipboard)
{
	GdkContentFormats *formats;

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

	formats = gdk_clipboard_get_formats (clipboard);

	return gdk_content_formats_contain_gtype (formats, G_TYPE_STRING) ||
	       gdk_content_formats_contain_mime_type (formats, "text/plain;charset=utf-8") ||
	       gdk_content_formats_contain_mime_type (formats, "text/plain") ||
	       gdk_content_formats_contain_mime_type (formats, "UTF8_STRING") ||
	       gdk_content_formats_contain_mime_type (formats, "STRING");
}

gboolean
e_gtk_clipboard_wait_is_image_available (GdkClipboard *clipboard)
{
	GdkContentFormats *formats;
	const gchar * const *mime_types;
	gsize ii, n_mime_types = 0;

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

	formats = gdk_clipboard_get_formats (clipboard);
	if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_TEXTURE))
		return TRUE;

	mime_types = gdk_content_formats_get_mime_types (formats, &n_mime_types);
	for (ii = 0; ii < n_mime_types; ii++) {
		if (g_str_has_prefix (mime_types[ii], "image/"))
			return TRUE;
	}

	return FALSE;
}

gboolean
e_gtk_clipboard_wait_is_target_available (GdkClipboard *clipboard,
                                          EGtkAtom target)
{
	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), FALSE);

	return target && gdk_content_formats_contain_mime_type (gdk_clipboard_get_formats (clipboard), target);
}

static void
wait_texture_cb (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	WaitData *wd = user_data;

	wd->texture = gdk_clipboard_read_texture_finish (GDK_CLIPBOARD (source), result, NULL);
	g_main_loop_quit (wd->loop);
}

GdkPixbuf *
e_gtk_clipboard_wait_for_image (GdkClipboard *clipboard)
{
	WaitData wd = { NULL, };
	GdkPixbuf *pixbuf = NULL;

	g_return_val_if_fail (GDK_IS_CLIPBOARD (clipboard), NULL);

	wd.loop = g_main_loop_new (NULL, FALSE);
	gdk_clipboard_read_texture_async (clipboard, NULL, wait_texture_cb, &wd);
	g_main_loop_run (wd.loop);
	g_main_loop_unref (wd.loop);

	if (wd.texture) {
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		pixbuf = gdk_pixbuf_get_from_texture (wd.texture);
		G_GNUC_END_IGNORE_DEPRECATIONS
		g_object_unref (wd.texture);
	}

	return pixbuf;
}

gchar **
e_gtk_clipboard_wait_for_uris (GdkClipboard *clipboard)
{
	EGtkSelectionData *data;
	gchar **uris;

	data = e_gtk_clipboard_wait_for_contents (clipboard, "text/uri-list");
	if (!data)
		return NULL;

	uris = e_gtk_selection_data_get_uris (data);
	e_gtk_selection_data_free (data);

	return uris;
}
