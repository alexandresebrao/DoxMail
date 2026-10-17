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

/* GnomeCanvas on GTK 4.
 *
 *   test-canvas                 runs the checks (needs a display)
 *   test-canvas --interactive   opens the demo window: items light up on
 *                               enter, drag the blue one, double-click logs
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#define CANVAS_W 300
#define CANVAS_H 200

static const GdkRGBA red = { 1, 0, 0, 1 };
static const GdkRGBA blue = { 0, 0, 1, 1 };
static const GdkRGBA black = { 0, 0, 0, 1 };
static const GdkRGBA yellow = { 1, 1, 0, 1 };

typedef struct {
	GtkWidget *window;
	GtkWidget *scrolled;
	GnomeCanvas *canvas;
	GnomeCanvasItem *red;
	GnomeCanvasItem *blue;
	GnomeCanvasItem *button_item;
	GtkWidget *button;
} Demo;

static gboolean
item_event_cb (GnomeCanvasItem *item,
               GnomeCanvasEvent *event,
               gpointer user_data)
{
	static gboolean dragging;
	static gdouble last_x, last_y;
	const gchar *name = user_data;

	switch (event->type) {
	case GNOME_CANVAS_ENTER_NOTIFY:
		g_print ("%s: enter at %.0f,%.0f\n", name, event->crossing.x, event->crossing.y);
		gnome_canvas_item_set (item, "outline-color", &yellow, NULL);
		return TRUE;
	case GNOME_CANVAS_LEAVE_NOTIFY:
		g_print ("%s: leave\n", name);
		gnome_canvas_item_set (item, "outline-color", &black, NULL);
		return TRUE;
	case GNOME_CANVAS_BUTTON_PRESS:
		g_print ("%s: press %u at %.0f,%.0f\n", name,
			event->button.button, event->button.x, event->button.y);
		if (event->button.button == 1) {
			dragging = gnome_canvas_item_grab (
				item, GNOME_CANVAS_POINTER_MOTION_MASK |
				GNOME_CANVAS_BUTTON_RELEASE_MASK,
				gdk_cursor_new_from_name ("grabbing", NULL),
				event->button.device, event->button.time) ==
				GNOME_CANVAS_GRAB_SUCCESS;
			last_x = event->button.x;
			last_y = event->button.y;
		}
		return TRUE;
	case GNOME_CANVAS_2BUTTON_PRESS:
		g_print ("%s: double click\n", name);
		return TRUE;
	case GNOME_CANVAS_3BUTTON_PRESS:
		g_print ("%s: triple click\n", name);
		return TRUE;
	case GNOME_CANVAS_MOTION_NOTIFY:
		if (dragging) {
			gnome_canvas_item_move (
				item, event->motion.x - last_x, event->motion.y - last_y);
			last_x = event->motion.x;
			last_y = event->motion.y;
		}
		return dragging;
	case GNOME_CANVAS_BUTTON_RELEASE:
		g_print ("%s: release %u\n", name, event->button.button);
		if (dragging) {
			gnome_canvas_item_ungrab (item, event->button.time);
			dragging = FALSE;
		}
		return TRUE;
	case GNOME_CANVAS_KEY_PRESS:
		g_print ("%s: key %s\n", name, gdk_keyval_name (event->key.keyval));
		return TRUE;
	case GNOME_CANVAS_FOCUS_CHANGE:
		g_print ("%s: focus %s\n", name, event->focus_change.in ? "in" : "out");
		return FALSE;
	default:
		return FALSE;
	}
}

static void
demo_build (Demo *demo)
{
	GnomeCanvasGroup *root;

	demo->window = gtk_window_new ();
	gtk_window_set_title (GTK_WINDOW (demo->window), "DoxMail canvas test");
	gtk_window_set_default_size (GTK_WINDOW (demo->window), CANVAS_W, CANVAS_H);

	demo->canvas = GNOME_CANVAS (gnome_canvas_new ());
	gnome_canvas_set_scroll_region (demo->canvas, 0, 0, 800, 600);

	demo->scrolled = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (demo->scrolled),
		GTK_POLICY_EXTERNAL, GTK_POLICY_EXTERNAL);
	gtk_scrolled_window_set_child (
		GTK_SCROLLED_WINDOW (demo->scrolled), GTK_WIDGET (demo->canvas));
	gtk_window_set_child (GTK_WINDOW (demo->window), demo->scrolled);

	root = gnome_canvas_root (demo->canvas);

	demo->red = gnome_canvas_item_new (
		root, GNOME_TYPE_CANVAS_RECT,
		"x1", 10.0, "y1", 10.0, "x2", 110.0, "y2", 90.0,
		"fill-color", &red, "outline-color", &black,
		NULL);
	g_signal_connect (demo->red, "event", G_CALLBACK (item_event_cb), (gpointer) "red");

	demo->blue = gnome_canvas_item_new (
		root, GNOME_TYPE_CANVAS_RECT,
		"x1", 150.0, "y1", 20.0, "x2", 250.0, "y2", 120.0,
		"fill-color", &blue, "outline-color", &black,
		NULL);
	g_signal_connect (demo->blue, "event", G_CALLBACK (item_event_cb), (gpointer) "blue");

	demo->button = gtk_button_new_with_label ("GTK 4 child");
	demo->button_item = gnome_canvas_item_new (
		root, GNOME_TYPE_CANVAS_WIDGET,
		"widget", demo->button,
		"x", 20.0, "y", 130.0, "width", 120.0, "height", 40.0,
		NULL);
}

/* Renders the window into a texture, like the compositor would get it. */
static GdkTexture *
render_window (Demo *demo)
{
	GtkWidget *widget = demo->window;
	GdkPaintable *paintable;
	GtkSnapshot *snapshot;
	GskRenderNode *node;
	GdkTexture *texture;
	gint width, height;

	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);

	paintable = gtk_widget_paintable_new (widget);
	snapshot = gtk_snapshot_new ();
	gdk_paintable_snapshot (paintable, snapshot, width, height);
	node = gtk_snapshot_free_to_node (snapshot);
	if (!node)
		g_printerr ("empty snapshot: window %dx%d mapped %d canvas %dx%d\n",
			width, height, gtk_widget_get_mapped (widget),
			gtk_widget_get_width (GTK_WIDGET (demo->canvas)),
			gtk_widget_get_height (GTK_WIDGET (demo->canvas)));
	g_assert_nonnull (node);

	texture = gsk_renderer_render_texture (
		gtk_native_get_renderer (GTK_NATIVE (widget)), node,
		&GRAPHENE_RECT_INIT (0, 0, width, height));

	gsk_render_node_unref (node);
	g_object_unref (paintable);

	return texture;
}

/* Pixel of the canvas at canvas widget coordinates, as 0xRRGGBB */
static guint32
canvas_pixel (Demo *demo,
              GdkTexture *texture,
              gint x,
              gint y)
{
	graphene_point_t point;
	GdkTextureDownloader *downloader;
	GBytes *bytes;
	gsize stride;
	const guint8 *data;
	guint32 rgb;

	g_assert_true (gtk_widget_compute_point (
		GTK_WIDGET (demo->canvas), demo->window,
		&GRAPHENE_POINT_INIT (x, y), &point));

	downloader = gdk_texture_downloader_new (texture);
	gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8);
	bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
	data = g_bytes_get_data (bytes, NULL);

	data += ((gint) point.y) * stride + ((gint) point.x) * 3;
	rgb = (data[0] << 16) | (data[1] << 8) | data[2];

	g_bytes_unref (bytes);
	gdk_texture_downloader_free (downloader);

	return rgb;
}

static void
wait_frames (Demo *demo)
{
	gint64 end = g_get_monotonic_time () + 300 * G_TIME_SPAN_MILLISECOND;

	while (g_get_monotonic_time () < end)
		g_main_context_iteration (NULL, FALSE);
}

static void
demo_show (Demo *demo)
{
	gtk_window_present (GTK_WINDOW (demo->window));

	while (!gtk_widget_get_mapped (GTK_WIDGET (demo->canvas)))
		g_main_context_iteration (NULL, TRUE);

	wait_frames (demo);
}

static void
test_draw (void)
{
	Demo demo = { 0 };
	GdkTexture *texture;

	demo_build (&demo);
	demo_show (&demo);

	texture = render_window (&demo);
	if (g_getenv ("DOXMAIL_TEST_PNG")) {
		GtkAdjustment *hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (demo.canvas));

		gdk_texture_save_to_png (texture, g_getenv ("DOXMAIL_TEST_PNG"));
		g_printerr ("window %dx%d canvas %dx%d texture %dx%d zoom %d,%d hadj %g/%g page %g\n",
			gtk_widget_get_width (demo.window), gtk_widget_get_height (demo.window),
			gtk_widget_get_width (GTK_WIDGET (demo.canvas)), gtk_widget_get_height (GTK_WIDGET (demo.canvas)),
			gdk_texture_get_width (texture), gdk_texture_get_height (texture),
			demo.canvas->zoom_xofs, demo.canvas->zoom_yofs,
			gtk_adjustment_get_value (hadj), gtk_adjustment_get_upper (hadj),
			gtk_adjustment_get_page_size (hadj));
	}
	g_assert_cmphex (canvas_pixel (&demo, texture, 50, 50), ==, 0xff0000);
	g_assert_cmphex (canvas_pixel (&demo, texture, 200, 70), ==, 0x0000ff);
	g_assert_cmphex (canvas_pixel (&demo, texture, 130, 50), !=, 0xff0000);
	g_object_unref (texture);

	gtk_window_destroy (GTK_WINDOW (demo.window));
}

static void
test_scroll (void)
{
	Demo demo = { 0 };
	GtkAdjustment *hadj, *vadj;
	GdkTexture *texture;

	demo_build (&demo);
	demo_show (&demo);

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (demo.canvas));
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (demo.canvas));

	/* the scroll region is bigger than the window, so it scrolls */
	g_assert_cmpfloat (gtk_adjustment_get_upper (hadj), ==, 800);
	g_assert_cmpfloat (gtk_adjustment_get_upper (vadj), ==, 600);

	gnome_canvas_scroll_to (demo.canvas, 140, 10);
	wait_frames (&demo);

	g_assert_cmpfloat (gtk_adjustment_get_value (hadj), ==, 140);

	/* the blue rect (150..250) is now at the left edge */
	texture = render_window (&demo);
	g_assert_cmphex (canvas_pixel (&demo, texture, 50, 50), ==, 0x0000ff);
	g_object_unref (texture);

	gtk_window_destroy (GTK_WINDOW (demo.window));
}

static void
test_pick (void)
{
	Demo demo = { 0 };

	demo_build (&demo);
	demo_show (&demo);

	g_assert_true (gnome_canvas_get_item_at (demo.canvas, 50, 50) == demo.red);
	g_assert_true (gnome_canvas_get_item_at (demo.canvas, 200, 50) == demo.blue);
	g_assert_null (gnome_canvas_get_item_at (demo.canvas, 130, 50));

	gnome_canvas_item_move (demo.red, 100, 0);
	wait_frames (&demo);
	g_assert_true (gnome_canvas_get_item_at (demo.canvas, 130, 50) == demo.red);

	gtk_window_destroy (GTK_WINDOW (demo.window));
}

static void
test_child_widget (void)
{
	Demo demo = { 0 };
	graphene_rect_t bounds;
	GtkAdjustment *vadj;

	demo_build (&demo);
	demo_show (&demo);

	g_assert_true (gtk_widget_get_parent (demo.button) == GTK_WIDGET (demo.canvas));
	g_assert_true (gtk_widget_get_mapped (demo.button));

	/* border box; gtk_widget_get_width() would leave out the CSS padding */
	g_assert_true (gtk_widget_compute_bounds (
		demo.button, GTK_WIDGET (demo.canvas), &bounds));
	g_assert_cmpfloat (bounds.origin.x, ==, 20);
	g_assert_cmpfloat (bounds.origin.y, ==, 130);
	g_assert_cmpfloat (bounds.size.width, ==, 120);
	g_assert_cmpfloat (bounds.size.height, ==, 40);

	/* the child scrolls with the canvas */
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (demo.canvas));
	gtk_adjustment_set_value (vadj, 100);
	wait_frames (&demo);

	g_assert_true (gtk_widget_compute_bounds (
		demo.button, GTK_WIDGET (demo.canvas), &bounds));
	g_assert_cmpfloat (bounds.origin.y, ==, 30);

	/* moving the item moves the widget */
	gnome_canvas_item_set (demo.button_item, "x", 60.0, NULL);
	wait_frames (&demo);

	g_assert_true (gtk_widget_compute_bounds (
		demo.button, GTK_WIDGET (demo.canvas), &bounds));
	g_assert_cmpfloat (bounds.origin.x, ==, 60);

	/* destroying the item releases the widget */
	g_object_ref (demo.button);
	g_object_run_dispose (G_OBJECT (demo.button_item));
	g_assert_null (gtk_widget_get_parent (demo.button));
	g_object_unref (demo.button);

	gtk_window_destroy (GTK_WINDOW (demo.window));
}

static void
test_event_copy (void)
{
	GnomeCanvasEvent event, *copy;
	guint button = 0;
	gdouble x = 0, y = 0;

	gnome_canvas_event_set_from_gdk (&event, GNOME_CANVAS_2BUTTON_PRESS, NULL, 12, 34);
	event.button.button = 3;

	copy = gnome_canvas_event_copy (&event);
	g_assert_cmpint (copy->type, ==, GNOME_CANVAS_2BUTTON_PRESS);
	g_assert_true (gnome_canvas_event_get_button (copy, &button));
	g_assert_cmpuint (button, ==, 3);
	g_assert_true (gnome_canvas_event_get_coords (copy, &x, &y));
	g_assert_cmpfloat (x, ==, 12);
	g_assert_cmpfloat (copy->motion.y, ==, 34);
	g_assert_false (gnome_canvas_event_get_keyval (copy, NULL));
	g_assert_null (gnome_canvas_event_get_gdk_event (copy));
	gnome_canvas_event_free (copy);
}

static void
quit_cb (GtkWindow *window,
         gpointer user_data)
{
	*(gboolean *) user_data = TRUE;
}

gint
main (gint argc,
      gchar **argv)
{
	if (argc > 1 && g_str_equal (argv[1], "--interactive")) {
		Demo demo = { 0 };
		gboolean done = FALSE;

		gtk_init ();
		demo_build (&demo);
		gnome_canvas_item_grab_focus (demo.blue);
		g_signal_connect (demo.window, "destroy", G_CALLBACK (quit_cb), &done);
		gtk_window_present (GTK_WINDOW (demo.window));

		while (!done)
			g_main_context_iteration (NULL, TRUE);

		return 0;
	}

	gtk_test_init (&argc, &argv, NULL);

	g_test_add_func ("/canvas/event-copy", test_event_copy);
	g_test_add_func ("/canvas/draw", test_draw);
	g_test_add_func ("/canvas/scroll", test_scroll);
	g_test_add_func ("/canvas/pick", test_pick);
	g_test_add_func ("/canvas/child-widget", test_child_widget);

	return g_test_run ();
}
