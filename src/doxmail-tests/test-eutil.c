/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either the version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

/* e-util on GTK 4: the compat layers and some of the ported widgets.
 *
 *   test-eutil                  runs the checks (needs a display)
 *   test-eutil --interactive    shows the widgets in a window
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <e-util/e-util.h>
#include <e-util/e-toolbar-layout.h> /* private */

/* lets the widgets map and lay out */
static void
spin (guint ms)
{
	gint64 until = g_get_monotonic_time () + ms * 1000;

	while (g_get_monotonic_time () < until)
		g_main_context_iteration (NULL, FALSE);
}

static GtkWidget *
show_in_window (GtkWidget *child)
{
	GtkWidget *window = gtk_window_new ();

	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
	gtk_window_set_child (GTK_WINDOW (window), child);
	gtk_window_present (GTK_WINDOW (window));
	spin (300);

	return window;
}

static void
test_toggle_radio (void)
{
	GtkWidget *a, *b, *c, *toggle;
	GSList *group;

	a = e_gtk_radio_button_new_with_label (NULL, "a");
	group = e_gtk_radio_button_get_group (a);
	b = e_gtk_radio_button_new_with_label (group, "b");
	c = e_gtk_radio_button_new_with_mnemonic_from_widget (a, "_c");
	g_object_ref_sink (a);
	g_object_ref_sink (b);
	g_object_ref_sink (c);

	/* the first of a GTK 3 group starts active, the group is exclusive */
	g_assert_true (e_gtk_toggle_button_get_active (a));
	e_gtk_toggle_button_set_active (c, TRUE);
	g_assert_false (e_gtk_toggle_button_get_active (a));
	g_assert_false (e_gtk_toggle_button_get_active (b));
	g_assert_true (e_gtk_toggle_button_get_active (c));

	toggle = g_object_ref_sink (gtk_toggle_button_new_with_label ("t"));
	e_gtk_toggle_button_set_active (toggle, TRUE);
	g_assert_true (e_gtk_toggle_button_get_active (toggle));
	e_gtk_toggle_button_set_inconsistent (toggle, TRUE);
	g_assert_true (e_gtk_toggle_button_get_inconsistent (toggle));

	g_object_unref (a);
	g_object_unref (b);
	g_object_unref (c);
	g_object_unref (toggle);
}

static void
test_container_child (void)
{
	GtkWidget *grid, *label, *notebook, *page;
	gint top = -1, left = -1;
	gboolean tab_fill = FALSE;

	grid = g_object_ref_sink (gtk_grid_new ());
	label = gtk_label_new ("x");
	gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);

	e_gtk_container_child_get (grid, label, "left-attach", &left, "top-attach", &top, NULL);
	g_assert_cmpint (left, ==, 1);
	g_assert_cmpint (top, ==, 2);

	/* GTK 3 box packing is gone; the vararg is still skipped */
	e_gtk_container_child_set (grid, label, "expand", TRUE, "top-attach", 5, NULL);
	e_gtk_container_child_get (grid, label, "top-attach", &top, NULL);
	g_assert_cmpint (top, ==, 5);

	notebook = g_object_ref_sink (gtk_notebook_new ());
	page = gtk_label_new ("page");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, NULL);
	e_gtk_container_child_set (notebook, page, "tab-fill", TRUE, NULL);
	e_gtk_container_child_get (notebook, page, "tab-fill", &tab_fill, NULL);
	g_assert_true (tab_fill);

	g_object_unref (grid);
	g_object_unref (notebook);
}

static void
test_calendar_compat (void)
{
	GtkWidget *calendar;
	guint year = 0, month = 0, day = 0;

	calendar = g_object_ref_sink (gtk_calendar_new ());

	e_gtk_calendar_select_month (GTK_CALENDAR (calendar), 1, 2024);
	e_gtk_calendar_select_day (GTK_CALENDAR (calendar), 29);
	e_gtk_calendar_get_date (GTK_CALENDAR (calendar), &year, &month, &day);

	/* GTK 3 months count from 0 */
	g_assert_cmpuint (year, ==, 2024);
	g_assert_cmpuint (month, ==, 1);
	g_assert_cmpuint (day, ==, 29);

	g_object_unref (calendar);
}

typedef struct {
	gint prepared;
	gint applied;
	gint closed;
} AssistantCounts;

static void
assistant_prepare_cb (EAssistant *assistant,
		      GtkWidget *page,
		      AssistantCounts *counts)
{
	counts->prepared++;
}

static void
assistant_apply_cb (EAssistant *assistant,
		    AssistantCounts *counts)
{
	counts->applied++;
}

static void
assistant_close_cb (EAssistant *assistant,
		    AssistantCounts *counts)
{
	counts->closed++;
}

static void
test_assistant (void)
{
	AssistantCounts counts = { 0, };
	EAssistant *assistant;
	GtkWidget *intro, *content, *hidden, *confirm, *summary;

	assistant = E_ASSISTANT (e_assistant_new ());
	g_signal_connect (assistant, "prepare", G_CALLBACK (assistant_prepare_cb), &counts);
	g_signal_connect (assistant, "apply", G_CALLBACK (assistant_apply_cb), &counts);
	g_signal_connect (assistant, "close", G_CALLBACK (assistant_close_cb), &counts);

	intro = gtk_label_new ("intro");
	content = gtk_label_new ("content");
	hidden = gtk_label_new ("hidden");
	confirm = gtk_label_new ("confirm");
	summary = gtk_label_new ("summary");

	g_assert_cmpint (e_assistant_append_page (assistant, intro), ==, 0);
	g_assert_cmpint (e_assistant_append_page (assistant, content), ==, 1);
	g_assert_cmpint (e_assistant_append_page (assistant, hidden), ==, 2);
	g_assert_cmpint (e_assistant_append_page (assistant, confirm), ==, 3);
	g_assert_cmpint (e_assistant_append_page (assistant, summary), ==, 4);
	g_assert_cmpint (e_assistant_get_n_pages (assistant), ==, 5);

	e_assistant_set_page_type (assistant, intro, GTK_ASSISTANT_PAGE_INTRO);
	e_assistant_set_page_type (assistant, confirm, GTK_ASSISTANT_PAGE_CONFIRM);
	e_assistant_set_page_type (assistant, summary, GTK_ASSISTANT_PAGE_SUMMARY);
	e_assistant_set_page_title (assistant, content, "Content");
	gtk_widget_set_visible (hidden, FALSE);

	gtk_window_present (GTK_WINDOW (assistant));
	spin (300);

	/* mapping shows the first page */
	g_assert_cmpint (e_assistant_get_current_page (assistant), ==, 0);
	g_assert_cmpint (counts.prepared, ==, 1);

	e_assistant_next_page (assistant);
	g_assert_cmpint (e_assistant_get_current_page (assistant), ==, 1);
	g_assert_cmpstr (e_assistant_get_page_title (assistant, content), ==, "Content");

	/* the hidden page is skipped */
	e_assistant_next_page (assistant);
	g_assert_cmpint (e_assistant_get_current_page (assistant), ==, 3);

	/* back goes where it came from */
	e_assistant_previous_page (assistant);
	g_assert_cmpint (e_assistant_get_current_page (assistant), ==, 1);

	e_assistant_set_current_page (assistant, 3);
	g_assert_cmpint (counts.applied, ==, 0);

	e_assistant_commit (assistant);
	e_assistant_set_current_page (assistant, -1);
	g_assert_cmpint (e_assistant_get_current_page (assistant), ==, 4);
	g_assert_true (e_assistant_get_nth_page (assistant, -1) == summary);

	gtk_window_destroy (GTK_WINDOW (assistant));
	spin (50);
}

static void
test_widgets (void)
{
	GtkWidget *box, *window, *widget;
	GdkRGBA color = { 0.5, 0.25, 0.75, 1.0 }, got;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

	widget = e_color_combo_new ();
	e_color_combo_set_current_color (E_COLOR_COMBO (widget), &color);
	e_color_combo_get_current_color (E_COLOR_COMBO (widget), &got);
	g_assert_true (gdk_rgba_equal (&color, &got));
	gtk_box_append (GTK_BOX (box), widget);

	widget = e_month_widget_new ();
	e_month_widget_set_month (E_MONTH_WIDGET (widget), G_DATE_FEBRUARY, 2024);
	gtk_box_append (GTK_BOX (box), widget);

	widget = e_date_edit_new ();
	gtk_box_append (GTK_BOX (box), widget);

	/* e_gtk_button_get_image() finds the image with and without a label */
	{
		GtkWidget *image = gtk_image_new_from_icon_name ("folder");

		widget = gtk_button_new ();
		e_gtk_button_set_image (GTK_BUTTON (widget), image);
		g_assert_true (e_gtk_button_get_image (GTK_BUTTON (widget)) == image);
		gtk_box_append (GTK_BOX (box), widget);

		image = gtk_image_new_from_icon_name ("folder");
		widget = gtk_button_new_with_mnemonic ("_Icon");
		e_gtk_button_set_image (GTK_BUTTON (widget), image);
		g_assert_true (e_gtk_button_get_image (GTK_BUTTON (widget)) == image);
		gtk_box_append (GTK_BOX (box), widget);

		/* like GTK 3, a label hides the image, which shows again
		 * once the label is gone; label changes keep it */
		g_assert_false (gtk_widget_is_ancestor (image, widget));
		gtk_button_set_label (GTK_BUTTON (widget), "_Other");
		g_assert_true (e_gtk_button_get_image (GTK_BUTTON (widget)) == image);
		gtk_button_set_label (GTK_BUTTON (widget), NULL);
		g_assert_true (gtk_button_get_child (GTK_BUTTON (widget)) == image);

		/* a binding to the label does not loop through the rebuilds */
		{
			GtkWidget *source = gtk_label_new ("_Bound");

			g_object_bind_property (source, "label", widget, "label", G_BINDING_SYNC_CREATE);
			gtk_label_set_label (GTK_LABEL (source), "_Bound again");
			g_assert_cmpstr (gtk_button_get_label (GTK_BUTTON (widget)), ==, "_Bound again");
			g_object_unref (g_object_ref_sink (source));
		}
	}

	widget = e_ellipsized_combo_box_text_new (FALSE);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "a very long text that should be ellipsized");
	g_assert_cmpint (e_ellipsized_combo_box_text_get_max_natural_width (E_ELLIPSIZED_COMBO_BOX_TEXT (widget)), ==, 225);
	gtk_box_append (GTK_BOX (box), widget);

	window = show_in_window (box);

	g_assert_true (gtk_widget_get_mapped (box));

	gtk_window_destroy (GTK_WINDOW (window));
	spin (50);
}

static void
test_picture_gallery (void)
{
	GtkWidget *scrolled, *gallery, *window;
	gchar *dir;

	dir = g_dir_make_tmp ("test-eutil-XXXXXX", NULL);
	g_assert_nonnull (dir);

	gallery = e_picture_gallery_new (dir);
	g_assert_cmpstr (e_picture_gallery_get_path (E_PICTURE_GALLERY (gallery)), ==, dir);

	/* the wrapper scrolls like the GTK 3 icon view */
	scrolled = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), gallery);
	g_assert_true (GTK_IS_ICON_VIEW (e_picture_gallery_get_icon_view (E_PICTURE_GALLERY (gallery))));

	window = show_in_window (scrolled);
	gtk_window_destroy (GTK_WINDOW (window));
	spin (50);

	g_rmdir (dir);
	g_free (dir);
}

static gboolean
style_updated_cb (GtkWidget *widget,
		  gint *count)
{
	(*count)++;

	return FALSE;
}

static gboolean
legacy_event_cb (GtkWidget *widget,
		 GdkEvent *event,
		 gpointer user_data)
{
	GdkEvent **out_event = user_data;

	*out_event = event;

	return TRUE;
}

static void
test_legacy_signals (void)
{
	GtkWidget *widget;
	gint count = 0;

	widget = g_object_ref_sink (gtk_label_new ("x"));

	/* the GTK 3 names exist on every widget */
	g_assert_cmpuint (g_signal_lookup ("key-press-event", GTK_TYPE_WIDGET), !=, 0);
	g_assert_cmpuint (g_signal_lookup ("button-press-event", GTK_TYPE_WIDGET), !=, 0);
	g_assert_cmpuint (g_signal_lookup ("popup-menu", GTK_TYPE_WIDGET), !=, 0);
	g_assert_cmpuint (g_signal_lookup ("drag-data-received", GTK_TYPE_WIDGET), !=, 0);

	g_assert_true (e_gtk_widget_legacy_events (widget) == widget);
	g_assert_true (e_gtk_widget_style_updates (widget) == widget);

	g_signal_connect (widget, "style-updated", G_CALLBACK (style_updated_cb), &count);
	g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", TRUE, NULL);
	g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", FALSE, NULL);
	g_assert_cmpint (count, ==, 2);

	/* the event reaches handlers; GdkEvent is a fundamental type the
	 * generic marshaller cannot pass, thus the signal takes a pointer */
	{
		GdkEvent *fake_event = (GdkEvent *) &count, *got_event = NULL;
		gboolean handled = FALSE;

		g_signal_connect (widget, "focus-in-event", G_CALLBACK (legacy_event_cb), &got_event);
		g_signal_emit_by_name (widget, "focus-in-event", fake_event, &handled);
		g_assert_true (handled);
		g_assert_true (got_event == fake_event);
	}

	g_object_unref (widget);
}

static void
quit_cb (GtkWidget *widget,
	 gpointer user_data)
{
	*(gboolean *) user_data = TRUE;
}

/* a GtkBox subclass overriding measure and size_allocate, which GTK 4
 * does not call for widgets with a layout manager by itself */
typedef GtkBox TestLayoutBox;
typedef GtkBoxClass TestLayoutBoxClass;

G_DEFINE_TYPE (TestLayoutBox, test_layout_box, GTK_TYPE_BOX)

static gint test_layout_box_allocations = 0;

static void
test_layout_box_measure (GtkWidget *widget,
			 GtkOrientation orientation,
			 gint for_size,
			 gint *minimum,
			 gint *natural,
			 gint *minimum_baseline,
			 gint *natural_baseline)
{
	GTK_WIDGET_CLASS (test_layout_box_parent_class)->measure (widget, orientation, for_size,
		minimum, natural, minimum_baseline, natural_baseline);

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		*minimum = MAX (*minimum, 321);
		*natural = MAX (*natural, 321);
	}
}

static void
test_layout_box_size_allocate (GtkWidget *widget,
			       gint width,
			       gint height,
			       gint baseline)
{
	GTK_WIDGET_CLASS (test_layout_box_parent_class)->size_allocate (widget, width, height, baseline);

	test_layout_box_allocations++;
}

static void
test_layout_box_class_init (TestLayoutBoxClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->measure = test_layout_box_measure;
	widget_class->size_allocate = test_layout_box_size_allocate;
	e_gtk_widget_class_layout_methods (widget_class);
}

static void
test_layout_box_init (TestLayoutBox *self)
{
}

static void
size_allocate_signal_cb (GtkWidget *widget,
			 GtkAllocation *allocation,
			 gpointer user_data)
{
	gint *out_width = user_data;

	*out_width = allocation->width;
}

/* overrides size_allocate only: its measure stays the layout manager's */
typedef GtkBox TestAllocBox;
typedef GtkBoxClass TestAllocBoxClass;

G_DEFINE_TYPE (TestAllocBox, test_alloc_box, GTK_TYPE_BOX)

static void
test_alloc_box_size_allocate (GtkWidget *widget,
			      gint width,
			      gint height,
			      gint baseline)
{
	GTK_WIDGET_CLASS (test_alloc_box_parent_class)->size_allocate (widget, width, height, baseline);
}

static void
test_alloc_box_class_init (TestAllocBoxClass *klass)
{
	GTK_WIDGET_CLASS (klass)->size_allocate = test_alloc_box_size_allocate;
	e_gtk_widget_class_layout_methods (GTK_WIDGET_CLASS (klass));
}

static void
test_alloc_box_init (TestAllocBox *self)
{
}

static void
test_layout_methods (void)
{
	GtkWidget *box, *label, *window;
	gint minimum = 0;

	box = g_object_new (test_layout_box_get_type (), NULL);
	label = gtk_label_new ("child");
	gtk_box_append (GTK_BOX (box), label);

	/* a class overriding only size_allocate is measured by the layout manager */
	{
		GtkWidget *alloc_box = g_object_ref_sink (g_object_new (test_alloc_box_get_type (), NULL));
		GtkWidget *child = gtk_label_new ("child");
		gint min_height = 0, plain_height = 0;

		gtk_widget_set_size_request (child, -1, 40);
		gtk_box_append (GTK_BOX (alloc_box), child);
		gtk_widget_measure (alloc_box, GTK_ORIENTATION_VERTICAL, -1, &min_height, NULL, NULL, NULL);
		gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1, &plain_height, NULL, NULL, NULL);
		g_assert_cmpint (min_height, ==, plain_height);
		g_assert_cmpint (min_height, >=, 40);
		g_object_unref (alloc_box);
	}

	/* the measure override runs, its chain-up measures the children */
	gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, NULL, NULL, NULL);
	g_assert_cmpint (minimum, ==, 321);

	window = show_in_window (box);

	/* GTK 3 style chain-ups pass NULLs the layout managers do not take */
	GTK_WIDGET_GET_CLASS (box)->measure (box, GTK_ORIENTATION_VERTICAL, -1, &minimum, NULL, NULL, NULL);
	g_assert_cmpint (minimum, >, 0);

	/* the size_allocate override runs, its chain-up allocates the children */
	g_assert_cmpint (test_layout_box_allocations, >, 0);
	g_assert_cmpint (gtk_widget_get_width (box), >=, 321);
	g_assert_cmpint (gtk_widget_get_width (label), >, 0);

	/* the GTK 3 size-allocate signal, for a widget with a layout manager */
	{
		GtkWidget *other = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gint allocated_width = 0;

		gtk_widget_set_size_request (other, 123, 10);
		g_signal_connect (e_gtk_widget_size_allocations (other), "size-allocate",
			G_CALLBACK (size_allocate_signal_cb), &allocated_width);
		gtk_box_append (GTK_BOX (box), other);
		spin (200);
		g_assert_cmpint (allocated_width, >=, 123);
	}

	gtk_window_destroy (GTK_WINDOW (window));
	spin (50);
}

/* typing reaches the GtkText delegate of an entry; the undo listens there */
static void
test_entry_undo (void)
{
	GtkWidget *entry;
	GtkEditable *delegate;
	gint pos = 0;

	entry = g_object_ref_sink (gtk_entry_new ());
	e_widget_undo_attach (entry, NULL);

	delegate = gtk_editable_get_delegate (GTK_EDITABLE (entry));
	g_assert_nonnull (delegate);

	gtk_editable_insert_text (delegate, "typed", -1, &pos);
	g_assert_cmpstr (gtk_editable_get_text (GTK_EDITABLE (entry)), ==, "typed");
	g_assert_true (e_widget_undo_has_undo (entry));

	e_widget_undo_do_undo (entry);
	g_assert_cmpstr (gtk_editable_get_text (GTK_EDITABLE (entry)), ==, "");
	g_assert_true (e_widget_undo_has_redo (entry));

	e_widget_undo_do_redo (entry);
	g_assert_cmpstr (gtk_editable_get_text (GTK_EDITABLE (entry)), ==, "typed");

	g_object_unref (entry);
}

/* toolbars shrink to an overflow button whose menu has the hidden actions */
static void
test_toolbar_overflow (void)
{
	static const EUIActionEntry entries[] = {
		{ "first", "document-new", "_First", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "second", "document-open", "_Second", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "third", "document-save", "_Third", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "fourth", "edit-copy", "F_ourth", NULL, NULL, NULL, NULL, NULL, NULL }
	};
	static const gchar *eui =
		"<eui>"
		" <toolbar id='test-toolbar' primary='true'>"
		"  <item action='first' important='true'/>"
		"  <item action='second' important='true'/>"
		"  <separator/>"
		"  <item action='third' important='true'/>"
		"  <item action='fourth' important='true'/>"
		" </toolbar>"
		"</eui>";
	EUIManager *manager;
	GtkWidget *toolbar, *overflow, *window, *child;
	GMenuModel *menu;
	gint min = 0, nat = 0, n_hidden = 0, n_items = 0, ii;

	manager = e_ui_manager_new (NULL);
	e_ui_manager_add_actions_with_eui_data (manager, "test", NULL,
		entries, G_N_ELEMENTS (entries), NULL, eui);

	toolbar = GTK_WIDGET (e_ui_manager_create_item (manager, "test-toolbar"));
	g_assert_true (GTK_IS_BOX (toolbar));

	overflow = e_toolbar_layout_get_overflow (toolbar);
	g_assert_nonnull (overflow);
	g_assert_true (gtk_widget_get_last_child (toolbar) == overflow);

	gtk_widget_measure (toolbar, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);
	g_assert_cmpint (min, <, nat / 2);

	window = gtk_window_new ();
	gtk_window_set_child (GTK_WINDOW (window), toolbar);
	gtk_window_set_default_size (GTK_WINDOW (window), min + (nat - min) / 3, -1);
	gtk_window_present (GTK_WINDOW (window));
	spin (300);

	g_assert_true (gtk_widget_get_child_visible (overflow));
	g_assert_true (gtk_widget_get_child_visible (gtk_widget_get_first_child (toolbar)));

	for (child = gtk_widget_get_first_child (toolbar); child != overflow; child = gtk_widget_get_next_sibling (child)) {
		if (!gtk_widget_get_child_visible (child) && !GTK_IS_SEPARATOR (child))
			n_hidden++;
	}
	g_assert_cmpint (n_hidden, >, 0);

	gtk_menu_button_popup (GTK_MENU_BUTTON (overflow));
	spin (100);

	menu = gtk_menu_button_get_menu_model (GTK_MENU_BUTTON (overflow));
	for (ii = 0; ii < g_menu_model_get_n_items (menu); ii++) {
		GMenuModel *section = g_menu_model_get_item_link (menu, ii, G_MENU_LINK_SECTION);
		gint jj;

		g_assert_nonnull (section);
		for (jj = 0; jj < g_menu_model_get_n_items (section); jj++) {
			gchar *action_name = NULL;

			g_assert_true (g_menu_model_get_item_attribute (section, jj, G_MENU_ATTRIBUTE_ACTION, "s", &action_name));
			g_assert_true (g_str_has_prefix (action_name, "test."));
			g_free (action_name);
			n_items++;
		}
		g_object_unref (section);
	}
	g_assert_cmpint (n_items, ==, n_hidden);

	gtk_menu_button_popdown (GTK_MENU_BUTTON (overflow));

	/* wide enough again */
	gtk_window_set_default_size (GTK_WINDOW (window), nat + 50, -1);
	gtk_widget_queue_resize (window);
	spin (300);

	g_assert_false (gtk_widget_get_child_visible (overflow));
	for (child = gtk_widget_get_first_child (toolbar); child != overflow; child = gtk_widget_get_next_sibling (child))
		g_assert_true (gtk_widget_get_child_visible (child));

	gtk_window_destroy (GTK_WINDOW (window));
	g_object_unref (manager);
}

/* users clearing a box with gtk_box_remove() drop the inner box of the
 * pack_end() children too (EAlertBar did; the next pack_end() crashed) */
static void
test_box_pack_end_after_clear (void)
{
	GtkWidget *box, *child;
	gint ii;

	box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));

	for (ii = 0; ii < 3; ii++) {
		e_gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("start"), FALSE, FALSE, 0);
		e_gtk_box_pack_end (GTK_BOX (box), gtk_button_new_with_label ("end"), FALSE, FALSE, 0);

		child = gtk_widget_get_last_child (box);
		g_assert_true (GTK_IS_BOX (child));
		g_assert_true (GTK_IS_BUTTON (gtk_widget_get_first_child (child)));

		while ((child = gtk_widget_get_first_child (box)) != NULL)
			gtk_box_remove (GTK_BOX (box), child);
	}

	g_object_unref (box);
}

/* an expanding pack_end() child takes the free space, as with GTK 3 (the
 * folder tree of the "Go to Folder" dialog sat under an empty gap) */
static void
test_box_pack_end_expand (void)
{
	GtkWidget *box, *start, *tree, *window;
	graphene_rect_t start_bounds, tree_bounds;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	start = gtk_label_new ("search");
	tree = gtk_label_new ("tree");
	e_gtk_box_pack_start (GTK_BOX (box), start, FALSE, FALSE, 0);
	e_gtk_box_pack_end (GTK_BOX (box), gtk_label_new ("bar"), FALSE, FALSE, 0);
	e_gtk_box_pack_end (GTK_BOX (box), tree, TRUE, TRUE, 0);

	window = show_in_window (box);

	g_assert_true (gtk_widget_compute_bounds (start, box, &start_bounds));
	g_assert_true (gtk_widget_compute_bounds (tree, box, &tree_bounds));
	g_assert_cmpfloat (tree_bounds.origin.y, ==, start_bounds.origin.y + start_bounds.size.height);
	g_assert_cmpint (gtk_widget_get_height (tree), >, 200);

	gtk_window_destroy (GTK_WINDOW (window));
}

/* the emulated dialog action area keeps buttons in the order they were
 * added, as a GTK 3 GtkButtonBox did with pack_end() (EAlertDialog put
 * its default button at the left) */
static void
test_dialog_action_area_order (void)
{
	GtkWidget *dialog, *area, *first, *second;

	dialog = gtk_dialog_new ();
	area = e_gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	first = gtk_button_new_with_label ("_Exit");
	second = gtk_button_new_with_label ("_Migrate");
	e_gtk_box_pack_end (GTK_BOX (area), first, FALSE, FALSE, 0);
	e_gtk_box_pack_end (GTK_BOX (area), second, FALSE, FALSE, 0);
	e_gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
		gtk_label_new ("text"), FALSE, FALSE, 0);

	g_assert_true (gtk_widget_get_first_child (area) == first);
	g_assert_true (gtk_widget_get_next_sibling (first) == second);
	/* the action area stays after the content */
	g_assert_true (GTK_IS_LABEL (gtk_widget_get_first_child (gtk_dialog_get_content_area (GTK_DIALOG (dialog)))));

	gtk_window_destroy (GTK_WINDOW (dialog));
}

/* a drag moves the divider in steps; a later allocation keeps the last */
static void
test_paned_drag (void)
{
	GtkWidget *paned, *window;
	gint pos, ii;

	paned = e_paned_new (GTK_ORIENTATION_VERTICAL);
	e_gtk_paned_pack1 (paned, gtk_label_new ("list"), TRUE, FALSE);
	e_gtk_paned_pack2 (paned, gtk_label_new ("preview"), FALSE, FALSE);

	window = show_in_window (paned);
	spin (300);

	for (ii = 1; ii <= 5; ii++) {
		g_object_set (paned, "position", 100 + ii * 10, NULL);
		spin (30);
	}

	/* like a change of the list or the preview */
	gtk_widget_queue_resize (paned);
	spin (200);

	g_object_get (paned, "position", &pos, NULL);
	g_assert_cmpint (pos, ==, 150);

	gtk_window_destroy (GTK_WINDOW (window));
}

/* GTK 3 expanded packed children along the current orientation of the box */
static void
test_box_pack_orientation (void)
{
	GtkWidget *box, *child, *end_child;

	box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
	child = gtk_label_new ("start");
	end_child = gtk_label_new ("end");
	e_gtk_box_pack_start (GTK_BOX (box), child, TRUE, TRUE, 3);
	e_gtk_box_pack_end (GTK_BOX (box), end_child, TRUE, FALSE, 0);

	g_assert_true (gtk_widget_get_vexpand (child));
	g_assert_false (gtk_widget_get_hexpand (child));
	g_assert_cmpint (gtk_widget_get_margin_top (child), ==, 3);
	g_assert_cmpint (gtk_widget_get_valign (end_child), ==, GTK_ALIGN_CENTER);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_HORIZONTAL);

	g_assert_true (gtk_widget_get_hexpand (child));
	g_assert_false (gtk_widget_get_vexpand (child));
	g_assert_cmpint (gtk_widget_get_margin_top (child), ==, 0);
	g_assert_cmpint (gtk_widget_get_margin_start (child), ==, 3);
	g_assert_true (gtk_widget_get_hexpand (end_child));
	g_assert_cmpint (gtk_widget_get_valign (end_child), ==, GTK_ALIGN_FILL);
	g_assert_cmpint (gtk_widget_get_halign (end_child), ==, GTK_ALIGN_CENTER);
	g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (gtk_widget_get_parent (end_child))), ==, GTK_ORIENTATION_HORIZONTAL);

	g_object_unref (box);
}

/* GtkPopoverMenu names submenu pages by label and keeps the pages of nested
 * submenus of removed items: hiding items must not leave two submenus with
 * the same label (warnings are fatal here) */
static void
test_menu_sync (void)
{
	static const EUIActionEntry entries[] = {
		{ "format-menu", NULL, "For_mat", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "font-style-menu", NULL, "_Font Style", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "paragraph-style-menu", NULL, "_Paragraph Style", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "justify-menu", NULL, "_Alignment", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "bold", NULL, "_Bold", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "style-normal", NULL, "_Normal", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "justify-left", NULL, "_Left", NULL, NULL, NULL, NULL, NULL, NULL },
		{ "indent", NULL, "_Indent", NULL, NULL, NULL, NULL, NULL, NULL }
	};
	static const gchar *eui =
		"<eui>"
		" <menu id='main-menu'>"
		"  <submenu action='format-menu'>"
		"   <submenu action='font-style-menu'><item action='bold'/></submenu>"
		"   <separator/>"
		"   <submenu action='paragraph-style-menu'><item action='style-normal'/></submenu>"
		"   <submenu action='justify-menu'><item action='justify-left'/></submenu>"
		"   <separator/>"
		"   <item action='indent'/>"
		"  </submenu>"
		" </menu>"
		"</eui>";
	/* the section of Font Style empties and Paragraph Style moves into
	 * it; Format takes nested submenus along */
	const gchar *hide[] = { "font-style-menu", "paragraph-style-menu", "format-menu" };
	EUIManager *manager;
	GObject *menu;
	GtkWidget *button, *window;
	guint ii;

	manager = e_ui_manager_new (NULL);
	e_ui_manager_add_actions_with_eui_data (manager, "test", NULL,
		entries, G_N_ELEMENTS (entries), NULL, eui);
	e_ui_manager_set_actions_usable_for_kinds (manager, E_UI_ELEMENT_KIND_MENU,
		"format-menu", "font-style-menu", "paragraph-style-menu", "justify-menu", NULL);

	menu = e_ui_manager_create_item (manager, "main-menu");
	g_assert_true (E_IS_UI_MENU (menu));

	button = gtk_menu_button_new ();
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (menu));
	window = show_in_window (button);

	for (ii = 0; ii < G_N_ELEMENTS (hide); ii++) {
		EUIAction *action = e_ui_manager_get_action (manager, hide[ii]);

		g_assert_nonnull (action);
		e_ui_action_set_visible (action, FALSE);
		spin (50);
		e_ui_action_set_visible (action, TRUE);
		spin (50);
	}

	gtk_window_destroy (GTK_WINDOW (window));
	g_object_unref (menu);
	g_object_unref (manager);
}

gint
main (gint argc,
      gchar **argv)
{
	if (argc > 1 && g_str_equal (argv[1], "--interactive")) {
		GtkWidget *box, *window;
		gboolean done = FALSE;

		gtk_init ();

		box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
		gtk_box_append (GTK_BOX (box), e_color_combo_new ());
		gtk_box_append (GTK_BOX (box), e_month_widget_new ());
		gtk_box_append (GTK_BOX (box), e_date_edit_new ());

		window = gtk_window_new ();
		gtk_window_set_child (GTK_WINDOW (window), box);
		g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
		gtk_window_present (GTK_WINDOW (window));

		while (!done)
			g_main_context_iteration (NULL, TRUE);

		return 0;
	}

	gtk_test_init (&argc, &argv, NULL);

	g_test_add_func ("/eutil/toggle-radio", test_toggle_radio);
	g_test_add_func ("/eutil/container-child", test_container_child);
	g_test_add_func ("/eutil/calendar-compat", test_calendar_compat);
	g_test_add_func ("/eutil/legacy-signals", test_legacy_signals);
	g_test_add_func ("/eutil/assistant", test_assistant);
	g_test_add_func ("/eutil/widgets", test_widgets);
	g_test_add_func ("/eutil/picture-gallery", test_picture_gallery);
	g_test_add_func ("/eutil/layout-methods", test_layout_methods);
	g_test_add_func ("/eutil/menu-sync", test_menu_sync);
	g_test_add_func ("/eutil/box-pack-orientation", test_box_pack_orientation);
	g_test_add_func ("/eutil/toolbar-overflow", test_toolbar_overflow);
	g_test_add_func ("/eutil/entry-undo", test_entry_undo);
	g_test_add_func ("/eutil/box-pack-end-after-clear", test_box_pack_end_after_clear);
	g_test_add_func ("/eutil/paned-drag", test_paned_drag);
	g_test_add_func ("/eutil/box-pack-end-expand", test_box_pack_end_expand);
	g_test_add_func ("/eutil/dialog-action-area-order", test_dialog_action_area_order);

	return g_test_run ();
}
