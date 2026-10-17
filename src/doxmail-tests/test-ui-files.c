/*
 * SPDX-FileCopyrightText: (C) 2026 DoxMail contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* Loads every GtkBuilder .ui file of the source tree with GTK 4, which
 * fails on GTK 3 leftovers (unknown classes, properties, packing) */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include <e-util/e-util.h>
#include <mail/e-mail-junk-options.h>
#include <calendar/gui/e-timezone-entry.h>
#include <addressbook/gui/contact-editor/e-contact-editor-dyntable.h>

static void
collect_ui_files (const gchar *dirname,
		  GPtrArray *files)
{
	GDir *dir;
	const gchar *name;

	dir = g_dir_open (dirname, 0, NULL);
	if (!dir)
		return;

	while ((name = g_dir_read_name (dir))) {
		gchar *path = g_build_filename (dirname, name, NULL);

		if (g_file_test (path, G_FILE_TEST_IS_DIR))
			collect_ui_files (path, files);
		else if (g_str_has_suffix (name, ".ui"))
			g_ptr_array_add (files, g_steal_pointer (&path));

		g_free (path);
	}

	g_dir_close (dir);
}

/* a scope with a no-op for every signal handler name: the code sets the
 * real scopes, this only checks the files themselves */
typedef GtkBuilderCScope AnyHandlerScope;
typedef GtkBuilderCScopeClass AnyHandlerScopeClass;

static void any_handler_scope_iface_init (GtkBuilderScopeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (AnyHandlerScope, any_handler_scope, GTK_TYPE_BUILDER_CSCOPE,
	G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDER_SCOPE, any_handler_scope_iface_init))

static void
any_handler_noop (void)
{
}

static GClosure *
any_handler_scope_create_closure (GtkBuilderScope *scope,
				  GtkBuilder *builder,
				  const gchar *function_name,
				  GtkBuilderClosureFlags flags,
				  GObject *object,
				  GError **error)
{
	return g_cclosure_new (G_CALLBACK (any_handler_noop), NULL, NULL);
}

static void
any_handler_scope_iface_init (GtkBuilderScopeInterface *iface)
{
	GtkBuilderScopeInterface *parent = g_type_interface_peek_parent (iface);

	iface->get_type_from_name = parent->get_type_from_name;
	iface->get_type_from_function = parent->get_type_from_function;
	iface->create_closure = any_handler_scope_create_closure;
}

static void
any_handler_scope_class_init (AnyHandlerScopeClass *klass)
{
}

static void
any_handler_scope_init (AnyHandlerScope *self)
{
}

static void
test_ui_file (gconstpointer data)
{
	const gchar *path = data;
	GtkBuilder *builder;
	GtkBuilderScope *scope;
	GError *error = NULL;

	builder = gtk_builder_new ();
	scope = g_object_new (any_handler_scope_get_type (), NULL);
	gtk_builder_set_scope (builder, scope);
	g_object_unref (scope);
	gtk_builder_add_from_file (builder, path, &error);
	g_assert_no_error (error);
	g_object_unref (builder);
}

gint
main (gint argc,
      gchar **argv)
{
	GPtrArray *files;
	guint ii;
	gint res;

	g_test_init (&argc, &argv, NULL);
	/* report every file, not only the first failing one */
	g_test_set_nonfatal_assertions ();
	gtk_init ();

	/* the custom widgets of the files */
	g_type_ensure (E_TYPE_URL_ENTRY);
	g_type_ensure (E_TYPE_SPLIT_DATE_EDIT);
	g_type_ensure (E_TYPE_DATE_EDIT);
	g_type_ensure (E_TYPE_CLIENT_COMBO_BOX);
	g_type_ensure (E_TYPE_IMAGE_CHOOSER);
	g_type_ensure (E_TYPE_MAIL_JUNK_OPTIONS);
	g_type_ensure (E_TYPE_TIMEZONE_ENTRY);
	g_type_ensure (E_TYPE_CONTACT_EDITOR_DYNTABLE);

	files = g_ptr_array_new_with_free_func (g_free);
	collect_ui_files (DOXMAIL_SOURCE_DIR "/src", files);
	g_ptr_array_sort (files, (GCompareFunc) g_strcmp0);

	for (ii = 0; ii < files->len; ii++) {
		const gchar *path = g_ptr_array_index (files, ii);
		gchar *test_path;

		test_path = g_strconcat ("/ui/", path + strlen (DOXMAIL_SOURCE_DIR "/src/"), NULL);
		g_test_add_data_func (test_path, path, test_ui_file);
		g_free (test_path);
	}

	res = g_test_run ();

	g_ptr_array_unref (files);

	return res;
}
