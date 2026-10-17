/*
 * SPDX-FileCopyrightText: (C) 2026 DoxMail contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include "e-gtk-event-compat.h"

enum {
	EVENT,
	KEY_PRESS_EVENT,
	KEY_RELEASE_EVENT,
	BUTTON_PRESS_EVENT,
	BUTTON_RELEASE_EVENT,
	MOTION_NOTIFY_EVENT,
	SCROLL_EVENT,
	ENTER_NOTIFY_EVENT,
	LEAVE_NOTIFY_EVENT,
	FOCUS_IN_EVENT,
	FOCUS_OUT_EVENT,
	POPUP_MENU,
	STYLE_UPDATED,
	DELETE_EVENT,
	SIZE_ALLOCATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* the widget whose handlers run now, with the event and its click count */
typedef struct _Emission {
	GtkWidget *widget;
	GdkEvent *event;
	gint n_press;
} Emission;

static GSList *emissions = NULL; /* Emission *, innermost first */

__attribute__((constructor))
static void
e_gtk_event_register_signals (void)
{
	static const struct {
		guint id;
		const gchar *name;
	} event_signals[] = {
		{ EVENT, "event" },
		{ KEY_PRESS_EVENT, "key-press-event" },
		{ KEY_RELEASE_EVENT, "key-release-event" },
		{ BUTTON_PRESS_EVENT, "button-press-event" },
		{ BUTTON_RELEASE_EVENT, "button-release-event" },
		{ MOTION_NOTIFY_EVENT, "motion-notify-event" },
		{ SCROLL_EVENT, "scroll-event" },
		{ ENTER_NOTIFY_EVENT, "enter-notify-event" },
		{ LEAVE_NOTIFY_EVENT, "leave-notify-event" },
		{ FOCUS_IN_EVENT, "focus-in-event" },
		{ FOCUS_OUT_EVENT, "focus-out-event" }
	};
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (event_signals); ii++) {
		signals[event_signals[ii].id] = g_signal_new (
			event_signals[ii].name, GTK_TYPE_WIDGET,
			G_SIGNAL_RUN_LAST, 0,
			g_signal_accumulator_true_handled, NULL, NULL,
			G_TYPE_BOOLEAN, 1,
			G_TYPE_POINTER /* GdkEvent */);
	}

	signals[POPUP_MENU] = g_signal_new (
		"popup-menu", GTK_TYPE_WIDGET,
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, 0,
		g_signal_accumulator_true_handled, NULL, NULL,
		G_TYPE_BOOLEAN, 0);

	signals[DELETE_EVENT] = g_signal_new (
		"delete-event", GTK_TYPE_WINDOW,
		G_SIGNAL_RUN_LAST, 0,
		g_signal_accumulator_true_handled, NULL, NULL,
		G_TYPE_BOOLEAN, 1,
		G_TYPE_POINTER /* GdkEvent */);

	signals[STYLE_UPDATED] = g_signal_new (
		"style-updated", GTK_TYPE_WIDGET,
		G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 0);

	signals[SIZE_ALLOCATE] = g_signal_new (
		"size-allocate", GTK_TYPE_WIDGET,
		G_SIGNAL_RUN_FIRST, 0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER /* GtkAllocation */);
}

static gboolean
legacy_emit (GtkWidget *widget,
	     guint signal_id,
	     GdkEvent *event,
	     gint n_press)
{
	Emission emission;
	gboolean handled = FALSE;

	emission.widget = widget;
	emission.event = event;
	emission.n_press = n_press;

	emissions = g_slist_prepend (emissions, &emission);

	g_object_ref (widget);

	/* GTK 3 emitted "event" first, then the specific signal */
	if (event)
		g_signal_emit (widget, signals[EVENT], 0, event, &handled);
	if (!handled)
		g_signal_emit (widget, signal_id, 0, event, &handled);

	g_object_unref (widget);

	emissions = g_slist_remove (emissions, &emission);

	return handled;
}

static Emission *
emission_for (GdkEvent *event)
{
	GSList *link;

	for (link = emissions; link; link = g_slist_next (link)) {
		Emission *emission = link->data;

		if (emission->event == event)
			return emission;
	}

	return NULL;
}

typedef struct _ClickData {
	guint32 time;
	guint button;
	gdouble x, y;
	gint n_press;
} ClickData;

/* what GTK 3 counted as a double or triple click */
static gint
legacy_count_press (GtkWidget *widget,
		    GdkEvent *event)
{
	ClickData *click;
	GtkSettings *settings;
	gint double_click_time = 400, double_click_distance = 5;
	gdouble x = 0, y = 0;
	guint32 time;
	guint button;

	click = g_object_get_data (G_OBJECT (widget), "e-gtk-legacy-click");
	if (!click) {
		click = g_new0 (ClickData, 1);
		g_object_set_data_full (G_OBJECT (widget), "e-gtk-legacy-click", click, g_free);
	}

	settings = gtk_widget_get_settings (widget);
	g_object_get (settings,
		"gtk-double-click-time", &double_click_time,
		"gtk-double-click-distance", &double_click_distance,
		NULL);

	gdk_event_get_position (event, &x, &y);
	time = gdk_event_get_time (event);
	button = gdk_button_event_get_button (event);

	if (click->n_press > 0 && click->n_press < 3 &&
	    button == click->button &&
	    time - click->time <= (guint32) double_click_time &&
	    ABS (x - click->x) <= double_click_distance &&
	    ABS (y - click->y) <= double_click_distance)
		click->n_press++;
	else
		click->n_press = 1;

	click->time = time;
	click->button = button;
	click->x = x;
	click->y = y;

	return click->n_press;
}

static gboolean
legacy_is_popup_menu_key (GdkEvent *event)
{
	guint keyval = gdk_key_event_get_keyval (event);
	GdkModifierType state = gdk_event_get_modifier_state (event) & gtk_accelerator_get_default_mod_mask ();

	return keyval == GDK_KEY_Menu ||
		(keyval == GDK_KEY_F10 && state == GDK_SHIFT_MASK);
}

static gboolean
legacy_event_cb (GtkEventControllerLegacy *controller,
		 GdkEvent *event,
		 gpointer user_data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
	gboolean handled = FALSE;
	gint n_press;

	switch (gdk_event_get_event_type (event)) {
		case GDK_KEY_PRESS:
			handled = legacy_emit (widget, signals[KEY_PRESS_EVENT], event, 0);
			/* GTK 3 key bindings ran after an unhandled key press */
			if (!handled && legacy_is_popup_menu_key (event))
				handled = legacy_emit (widget, signals[POPUP_MENU], NULL, 0);
			break;
		case GDK_KEY_RELEASE:
			handled = legacy_emit (widget, signals[KEY_RELEASE_EVENT], event, 0);
			break;
		case GDK_BUTTON_PRESS:
			n_press = legacy_count_press (widget, event);
			handled = legacy_emit (widget, signals[BUTTON_PRESS_EVENT], event, 1);
			if (n_press > 1)
				handled = legacy_emit (widget, signals[BUTTON_PRESS_EVENT], event, n_press) || handled;
			break;
		case GDK_BUTTON_RELEASE:
			handled = legacy_emit (widget, signals[BUTTON_RELEASE_EVENT], event, 0);
			break;
		case GDK_MOTION_NOTIFY:
			handled = legacy_emit (widget, signals[MOTION_NOTIFY_EVENT], event, 0);
			break;
		case GDK_SCROLL:
			handled = legacy_emit (widget, signals[SCROLL_EVENT], event, 0);
			break;
		default:
			break;
	}

	return handled;
}

static void
legacy_enter_cb (GtkEventControllerMotion *controller,
		 gdouble x,
		 gdouble y,
		 gpointer user_data)
{
	GtkEventController *ec = GTK_EVENT_CONTROLLER (controller);

	legacy_emit (gtk_event_controller_get_widget (ec), signals[ENTER_NOTIFY_EVENT],
		gtk_event_controller_get_current_event (ec), 0);
}

static void
legacy_leave_cb (GtkEventControllerMotion *controller,
		 gpointer user_data)
{
	GtkEventController *ec = GTK_EVENT_CONTROLLER (controller);

	legacy_emit (gtk_event_controller_get_widget (ec), signals[LEAVE_NOTIFY_EVENT],
		gtk_event_controller_get_current_event (ec), 0);
}

static void
legacy_focus_enter_cb (GtkEventControllerFocus *controller,
		       gpointer user_data)
{
	GtkEventController *ec = GTK_EVENT_CONTROLLER (controller);

	/* GTK 3 told only the focus widget itself */
	if (gtk_event_controller_focus_is_focus (controller))
		legacy_emit (gtk_event_controller_get_widget (ec), signals[FOCUS_IN_EVENT],
			gtk_event_controller_get_current_event (ec), 0);
}

static void
legacy_focus_leave_cb (GtkEventControllerFocus *controller,
		       gpointer user_data)
{
	GtkEventController *ec = GTK_EVENT_CONTROLLER (controller);
	GtkWidget *widget = gtk_event_controller_get_widget (ec);

	if (!gtk_widget_has_focus (widget))
		legacy_emit (widget, signals[FOCUS_OUT_EVENT],
			gtk_event_controller_get_current_event (ec), 0);
}

gpointer
e_gtk_widget_legacy_events (gpointer widget)
{
	GtkEventController *controller;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), widget);

	if (g_object_get_data (widget, "e-gtk-legacy-events"))
		return widget;

	g_object_set_data (widget, "e-gtk-legacy-events", GINT_TO_POINTER (1));

	controller = gtk_event_controller_legacy_new ();
	g_signal_connect (controller, "event", G_CALLBACK (legacy_event_cb), NULL);
	gtk_widget_add_controller (widget, controller);

	controller = gtk_event_controller_motion_new ();
	g_signal_connect (controller, "enter", G_CALLBACK (legacy_enter_cb), NULL);
	g_signal_connect (controller, "leave", G_CALLBACK (legacy_leave_cb), NULL);
	gtk_widget_add_controller (widget, controller);

	controller = gtk_event_controller_focus_new ();
	g_signal_connect (controller, "enter", G_CALLBACK (legacy_focus_enter_cb), NULL);
	g_signal_connect (controller, "leave", G_CALLBACK (legacy_focus_leave_cb), NULL);
	gtk_widget_add_controller (widget, controller);

	return widget;
}

static GQuark
legacy_class_quark (void)
{
	return g_quark_from_static_string ("e-gtk-legacy-class");
}

static gboolean
legacy_realize_hook (GSignalInvocationHint *ihint,
		     guint n_param_values,
		     const GValue *param_values,
		     gpointer user_data)
{
	GObject *object = g_value_get_object (&param_values[0]);
	GType type;

	for (type = G_OBJECT_TYPE (object); type && type != GTK_TYPE_WIDGET; type = g_type_parent (type)) {
		if (g_type_get_qdata (type, legacy_class_quark ())) {
			e_gtk_widget_legacy_events (object);
			break;
		}
	}

	return TRUE;
}

void
e_gtk_widget_class_override_legacy_event (GtkWidgetClass *widget_class,
					  const gchar *signal_name,
					  GCallback handler)
{
	static gboolean hooked = FALSE;
	GType type;

	g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
	g_return_if_fail (signal_name != NULL);
	g_return_if_fail (handler != NULL);

	type = G_TYPE_FROM_CLASS (widget_class);

	g_signal_override_class_handler (signal_name, type, handler);
	g_type_set_qdata (type, legacy_class_quark (), GINT_TO_POINTER (1));

	if (!hooked) {
		hooked = TRUE;
		g_signal_add_emission_hook (g_signal_lookup ("realize", GTK_TYPE_WIDGET), 0,
			legacy_realize_hook, NULL, NULL);
	}
}

gboolean
e_gtk_widget_chain_legacy_event (GtkWidget *widget,
				 GdkEvent *event)
{
	GValue params[2] = { G_VALUE_INIT, G_VALUE_INIT };
	GValue ret = G_VALUE_INIT;
	gboolean handled;

	g_value_init (&params[0], G_TYPE_FROM_INSTANCE (widget));
	g_value_set_object (&params[0], widget);
	/* GdkEvent is a fundamental type the signal marshallers cannot pass,
	 * thus the legacy signals take a pointer */
	g_value_init (&params[1], G_TYPE_POINTER);
	g_value_set_pointer (&params[1], event);
	g_value_init (&ret, G_TYPE_BOOLEAN);

	g_signal_chain_from_overridden (params, &ret);

	handled = g_value_get_boolean (&ret);

	g_value_unset (&params[0]);
	g_value_unset (&params[1]);
	g_value_unset (&ret);

	return handled;
}

gboolean
e_gtk_widget_chain_legacy_popup_menu (GtkWidget *widget)
{
	GValue param = G_VALUE_INIT;
	GValue ret = G_VALUE_INIT;
	gboolean handled;

	g_value_init (&param, G_TYPE_FROM_INSTANCE (widget));
	g_value_set_object (&param, widget);
	g_value_init (&ret, G_TYPE_BOOLEAN);

	g_signal_chain_from_overridden (&param, &ret);

	handled = g_value_get_boolean (&ret);

	g_value_unset (&param);
	g_value_unset (&ret);

	return handled;
}

static gboolean
window_close_request_cb (GtkWindow *window,
			 gpointer user_data)
{
	gboolean handled = FALSE;

	g_signal_emit (window, signals[DELETE_EVENT], 0, NULL, &handled);

	return handled;
}

gpointer
e_gtk_window_delete_events (gpointer window)
{
	g_return_val_if_fail (GTK_IS_WINDOW (window), window);

	if (!g_object_get_data (window, "e-gtk-delete-events")) {
		g_object_set_data (window, "e-gtk-delete-events", GINT_TO_POINTER (1));
		g_signal_connect (window, "close-request", G_CALLBACK (window_close_request_cb), NULL);
	}

	return window;
}

static void
style_settings_notify_cb (GtkSettings *settings,
			  GParamSpec *param,
			  GtkWidget *widget)
{
	g_signal_emit (widget, signals[STYLE_UPDATED], 0);
}

/* GTK 4 hands the measuring and allocation of widgets that have a layout
 * manager (GtkBox, GtkGrid, GtkFixed, GtkButton, ...) to the layout
 * manager, without calling the measure and size_allocate methods of their
 * classes.  The layout manager types used by classes set up with
 * e_gtk_widget_class_layout_methods(), or by widgets that want the
 * "size-allocate" signal, are wrapped: for those classes their methods run
 * instead, and their chain-ups reach the layout manager. */

typedef void (* LayoutMeasureFunc) (GtkLayoutManager *manager, GtkWidget *widget,
				    GtkOrientation orientation, gint for_size,
				    gint *minimum, gint *natural,
				    gint *minimum_baseline, gint *natural_baseline);
typedef void (* LayoutAllocateFunc) (GtkLayoutManager *manager, GtkWidget *widget,
				     gint width, gint height, gint baseline);

typedef struct _LayoutOriginals {
	LayoutMeasureFunc measure;
	LayoutAllocateFunc allocate;
} LayoutOriginals;

/* LayoutOriginals *, by the GType of the wrapped layout manager class */
static GHashTable *layout_originals = NULL;

#define SIZE_ALLOCATE_KEY "e-gtk-size-allocate"

/* The handlers of GTK 3 often resized widgets, which GTK 4 does not allow
 * while it allocates them; GTK 3 ran another layout cycle then.  The
 * signal is emitted right after the layout instead, with the latest size. */
typedef struct _SizeAllocateEmit {
	GWeakRef widget;
	gint width;
	gint height;
	guint idle_id;
} SizeAllocateEmit;

#define SIZE_ALLOCATE_EMIT_KEY "e-gtk-size-allocate-emit"

static gboolean
size_allocate_emit_idle_cb (gpointer user_data)
{
	SizeAllocateEmit *emit = user_data;
	GtkWidget *widget = g_weak_ref_get (&emit->widget);

	emit->idle_id = 0;

	if (widget) {
		GtkAllocation allocation = { 0, 0, emit->width, emit->height };

		g_signal_emit (widget, signals[SIZE_ALLOCATE], 0, &allocation);
		g_object_unref (widget);
	}

	return G_SOURCE_REMOVE;
}

static void
size_allocate_emit_free (gpointer ptr)
{
	SizeAllocateEmit *emit = ptr;

	if (emit->idle_id)
		g_source_remove (emit->idle_id);
	g_weak_ref_clear (&emit->widget);
	g_free (emit);
}

static void
size_allocate_schedule_emit (GtkWidget *widget,
			     gint width,
			     gint height)
{
	SizeAllocateEmit *emit = g_object_get_data (G_OBJECT (widget), SIZE_ALLOCATE_EMIT_KEY);

	if (!emit) {
		emit = g_new0 (SizeAllocateEmit, 1);
		g_weak_ref_init (&emit->widget, widget);
		g_object_set_data_full (G_OBJECT (widget), SIZE_ALLOCATE_EMIT_KEY, emit, size_allocate_emit_free);
	}

	emit->width = width;
	emit->height = height;

	if (!emit->idle_id)
		emit->idle_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE, size_allocate_emit_idle_cb, emit, NULL);
}

static GQuark
layout_methods_quark (void)
{
	return g_quark_from_static_string ("e-gtk-layout-methods");
}

static LayoutOriginals *
layout_originals_for (GtkLayoutManager *manager)
{
	GType type;

	if (!layout_originals)
		return NULL;

	/* subclasses initialized after the wrapping inherit the wrappers */
	for (type = G_TYPE_FROM_INSTANCE (manager); type; type = g_type_parent (type)) {
		LayoutOriginals *originals = g_hash_table_lookup (layout_originals, GSIZE_TO_POINTER (type));

		if (originals)
			return originals;
	}

	return NULL;
}

static gboolean
widget_uses_layout_methods (GtkWidget *widget)
{
	GType type;

	for (type = G_TYPE_FROM_INSTANCE (widget); type; type = g_type_parent (type)) {
		if (g_type_get_qdata (type, layout_methods_quark ()))
			return TRUE;
	}

	return FALSE;
}

static void
layout_measure_wrapper (GtkLayoutManager *manager,
			GtkWidget *widget,
			GtkOrientation orientation,
			gint for_size,
			gint *minimum,
			gint *natural,
			gint *minimum_baseline,
			gint *natural_baseline)
{
	if (widget_uses_layout_methods (widget)) {
		GTK_WIDGET_GET_CLASS (widget)->measure (widget, orientation, for_size,
			minimum, natural, minimum_baseline, natural_baseline);
	} else {
		layout_originals_for (manager)->measure (manager, widget, orientation, for_size,
			minimum, natural, minimum_baseline, natural_baseline);
	}
}

static void
layout_allocate_wrapper (GtkLayoutManager *manager,
			 GtkWidget *widget,
			 gint width,
			 gint height,
			 gint baseline)
{
	if (widget_uses_layout_methods (widget))
		GTK_WIDGET_GET_CLASS (widget)->size_allocate (widget, width, height, baseline);
	else
		layout_originals_for (manager)->allocate (manager, widget, width, height, baseline);

	if (g_object_get_data (G_OBJECT (widget), SIZE_ALLOCATE_KEY))
		size_allocate_schedule_emit (widget, width, height);
}

static void
wrap_layout_manager_type (GType manager_type)
{
	GtkLayoutManagerClass *klass;
	LayoutOriginals *originals;

	if (!layout_originals)
		layout_originals = g_hash_table_new (g_direct_hash, g_direct_equal);

	if (g_hash_table_contains (layout_originals, GSIZE_TO_POINTER (manager_type)))
		return;

	klass = g_type_class_ref (manager_type);

	originals = g_new0 (LayoutOriginals, 1);
	originals->measure = klass->measure;
	originals->allocate = klass->allocate;
	g_hash_table_insert (layout_originals, GSIZE_TO_POINTER (manager_type), originals);

	klass->measure = layout_measure_wrapper;
	klass->allocate = layout_allocate_wrapper;
}

/* the chain-ups of the methods: they run the layout manager */
static void
layout_chain_measure (GtkWidget *widget,
		      GtkOrientation orientation,
		      gint for_size,
		      gint *minimum,
		      gint *natural,
		      gint *minimum_baseline,
		      gint *natural_baseline)
{
	GtkLayoutManager *manager = gtk_widget_get_layout_manager (widget);
	LayoutOriginals *originals = manager ? layout_originals_for (manager) : NULL;
	gint min = 0, nat = 0, min_baseline = -1, nat_baseline = -1;

	/* the layout managers expect all of them, GTK 3 code passes NULLs */
	if (originals) {
		originals->measure (manager, widget, orientation, for_size,
			&min, &nat, &min_baseline, &nat_baseline);
	}

	if (minimum)
		*minimum = min;
	if (natural)
		*natural = nat;
	if (minimum_baseline)
		*minimum_baseline = min_baseline;
	if (natural_baseline)
		*natural_baseline = nat_baseline;
}

static void
layout_chain_size_allocate (GtkWidget *widget,
			    gint width,
			    gint height,
			    gint baseline)
{
	GtkLayoutManager *manager = gtk_widget_get_layout_manager (widget);
	LayoutOriginals *originals = manager ? layout_originals_for (manager) : NULL;

	if (originals)
		originals->allocate (manager, widget, width, height, baseline);
}

void
e_gtk_widget_class_layout_methods (GtkWidgetClass *widget_class)
{
	GtkWidgetClass *base_class, *parent_class;
	GType manager_type;

	g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));

	manager_type = gtk_widget_class_get_layout_manager_type (widget_class);
	g_return_if_fail (manager_type != G_TYPE_INVALID);

	g_type_set_qdata (G_TYPE_FROM_CLASS (widget_class), layout_methods_quark (), GINT_TO_POINTER (1));
	wrap_layout_manager_type (manager_type);

	/* GTK 4 does not use the methods of the parent classes either, thus
	 * where they are the GtkWidget ones the chain-ups run the layout manager */
	base_class = g_type_class_peek (GTK_TYPE_WIDGET);
	parent_class = g_type_class_peek_parent (widget_class);

	if (parent_class->measure == base_class->measure)
		parent_class->measure = layout_chain_measure;
	if (parent_class->size_allocate == base_class->size_allocate)
		parent_class->size_allocate = layout_chain_size_allocate;
}

/* original size_allocate methods of the classes wrapped below, by GType */
static GHashTable *size_allocate_originals = NULL;

typedef void (* SizeAllocateFunc) (GtkWidget *widget, gint width, gint height, gint baseline);

static void
size_allocate_wrapper (GtkWidget *widget,
		       gint width,
		       gint height,
		       gint baseline)
{
	SizeAllocateFunc original = NULL;
	GType type;

	/* classes initialized after the wrapping inherit the wrapper,
	 * thus look up the nearest wrapped ancestor */
	for (type = G_TYPE_FROM_INSTANCE (widget); type && !original; type = g_type_parent (type))
		original = g_hash_table_lookup (size_allocate_originals, GSIZE_TO_POINTER (type));

	if (original)
		original (widget, width, height, baseline);

	if (g_object_get_data (G_OBJECT (widget), SIZE_ALLOCATE_KEY))
		size_allocate_schedule_emit (widget, width, height);
}

gpointer
e_gtk_widget_size_allocations (gpointer widget)
{
	GtkWidgetClass *klass;
	GtkLayoutManager *manager;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), widget);

	g_object_set_data (G_OBJECT (widget), SIZE_ALLOCATE_KEY, GINT_TO_POINTER (1));

	/* the layout manager allocates these, not the class method */
	manager = gtk_widget_get_layout_manager (widget);
	if (manager) {
		wrap_layout_manager_type (G_TYPE_FROM_INSTANCE (manager));
		return widget;
	}

	klass = GTK_WIDGET_GET_CLASS (widget);

	if (klass->size_allocate != size_allocate_wrapper) {
		if (!size_allocate_originals)
			size_allocate_originals = g_hash_table_new (g_direct_hash, g_direct_equal);

		g_hash_table_insert (size_allocate_originals,
			GSIZE_TO_POINTER (G_TYPE_FROM_CLASS (klass)), klass->size_allocate);
		klass->size_allocate = size_allocate_wrapper;
	}

	return widget;
}

static void
style_realize_cb (GtkWidget *widget,
		  gpointer user_data)
{
	g_signal_emit (widget, signals[STYLE_UPDATED], 0);
}

gpointer
e_gtk_widget_style_updates (gpointer widget)
{
	static const gchar *properties[] = {
		"notify::gtk-theme-name",
		"notify::gtk-application-prefer-dark-theme",
		"notify::gtk-font-name",
		"notify::gtk-icon-theme-name"
	};
	GtkSettings *settings;
	guint ii;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), widget);

	if (g_object_get_data (widget, "e-gtk-style-updates"))
		return widget;

	g_object_set_data (widget, "e-gtk-style-updates", GINT_TO_POINTER (1));

	settings = gtk_settings_get_default ();
	for (ii = 0; settings && ii < G_N_ELEMENTS (properties); ii++) {
		g_signal_connect_object (settings, properties[ii],
			G_CALLBACK (style_settings_notify_cb), widget, 0);
	}

	/* GTK 3 emitted it also when the widget got its style, on realize;
	 * code computes its colors and sizes there */
	g_signal_connect (widget, "realize", G_CALLBACK (style_realize_cb), NULL);

	return widget;
}

GdkEventType
e_gdk_event_type (GdkEvent *event)
{
	Emission *emission;
	GdkEventType type;

	g_return_val_if_fail (event != NULL, GDK_EVENT_LAST);

	type = gdk_event_get_event_type (event);

	emission = emission_for (event);
	if (type == GDK_BUTTON_PRESS && emission) {
		if (emission->n_press == 2)
			return E_GDK_2BUTTON_PRESS;
		if (emission->n_press == 3)
			return E_GDK_3BUTTON_PRESS;
	}

	return type;
}

guint
e_gdk_event_keyval (GdkEvent *event)
{
	GdkEventType type = event ? gdk_event_get_event_type (event) : GDK_EVENT_LAST;

	if (type != GDK_KEY_PRESS && type != GDK_KEY_RELEASE)
		return 0;

	return gdk_key_event_get_keyval (event);
}

guint16
e_gdk_event_keycode (GdkEvent *event)
{
	GdkEventType type = event ? gdk_event_get_event_type (event) : GDK_EVENT_LAST;

	if (type != GDK_KEY_PRESS && type != GDK_KEY_RELEASE)
		return 0;

	return gdk_key_event_get_keycode (event);
}

gboolean
e_gdk_event_is_modifier (GdkEvent *event)
{
	GdkEventType type = event ? gdk_event_get_event_type (event) : GDK_EVENT_LAST;

	if (type != GDK_KEY_PRESS && type != GDK_KEY_RELEASE)
		return FALSE;

	return gdk_key_event_is_modifier (event);
}

GdkModifierType
e_gdk_event_state (GdkEvent *event)
{
	return event ? gdk_event_get_modifier_state (event) : 0;
}

guint
e_gdk_event_button (GdkEvent *event)
{
	GdkEventType type = event ? gdk_event_get_event_type (event) : GDK_EVENT_LAST;

	if (type != GDK_BUTTON_PRESS && type != GDK_BUTTON_RELEASE)
		return 0;

	return gdk_button_event_get_button (event);
}

gboolean
e_gdk_event_get_widget_coords (GdkEvent *event,
			       GtkWidget *widget,
			       gdouble *x,
			       gdouble *y)
{
	GtkNative *native;
	graphene_point_t point;
	gdouble ex = 0, ey = 0, nx = 0, ny = 0;
	gboolean has;

	has = event && gdk_event_get_position (event, &ex, &ey);

	if (has && widget && (native = gtk_widget_get_native (widget)) != NULL) {
		gtk_native_get_surface_transform (native, &nx, &ny);

		if (gtk_widget_compute_point (GTK_WIDGET (native), widget,
			&GRAPHENE_POINT_INIT (ex - nx, ey - ny), &point)) {
			ex = point.x;
			ey = point.y;
		}
	}

	if (x)
		*x = ex;
	if (y)
		*y = ey;

	return has;
}

GtkWidget *
e_gtk_event_get_legacy_widget (GdkEvent *event)
{
	Emission *emission = event ? emission_for (event) : NULL;

	return emission ? emission->widget : NULL;
}

static void
event_coords (GdkEvent *event,
	      gdouble *x,
	      gdouble *y)
{
	Emission *emission = event ? emission_for (event) : NULL;

	/* relative to the emitting widget, like GTK 3 window coordinates */
	e_gdk_event_get_widget_coords (event, emission ? emission->widget : NULL, x, y);
}

gdouble
e_gdk_event_x (GdkEvent *event)
{
	gdouble x;

	event_coords (event, &x, NULL);

	return x;
}

gdouble
e_gdk_event_y (GdkEvent *event)
{
	gdouble y;

	event_coords (event, NULL, &y);

	return y;
}

/* GTK 4 has no global coordinates; these are surface coordinates */
gdouble
e_gdk_event_x_root (GdkEvent *event)
{
	gdouble x = 0;

	if (event)
		gdk_event_get_position (event, &x, NULL);

	return x;
}

gdouble
e_gdk_event_y_root (GdkEvent *event)
{
	gdouble y = 0;

	if (event)
		gdk_event_get_position (event, NULL, &y);

	return y;
}

guint32
e_gdk_event_time (GdkEvent *event)
{
	return event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
}

GdkScrollDirection
e_gdk_event_direction (GdkEvent *event)
{
	if (!event || gdk_event_get_event_type (event) != GDK_SCROLL)
		return GDK_SCROLL_SMOOTH;

	return gdk_scroll_event_get_direction (event);
}

gdouble
e_gdk_event_delta_x (GdkEvent *event)
{
	gdouble dx = 0;

	if (event && gdk_event_get_event_type (event) == GDK_SCROLL)
		gdk_scroll_event_get_deltas (event, &dx, NULL);

	return dx;
}

gdouble
e_gdk_event_delta_y (GdkEvent *event)
{
	gdouble dy = 0;

	if (event && gdk_event_get_event_type (event) == GDK_SCROLL)
		gdk_scroll_event_get_deltas (event, NULL, &dy);

	return dy;
}

gboolean
e_gdk_event_focus_in (GdkEvent *event)
{
	if (!event || gdk_event_get_event_type (event) != GDK_FOCUS_CHANGE)
		return FALSE;

	return gdk_focus_event_get_in (event);
}

GnomeCanvasEvent *
e_gdk_event_to_canvas_event (GdkEvent *event,
			     GtkWidget *canvas,
			     GnomeCanvasEvent *cevent)
{
	GnomeCanvasEventType type;
	gdouble x, y;

	g_return_val_if_fail (cevent != NULL, NULL);

	switch (e_gdk_event_type (event)) {
	case GDK_BUTTON_PRESS:
		type = GNOME_CANVAS_BUTTON_PRESS;
		break;
	case E_GDK_2BUTTON_PRESS:
		type = GNOME_CANVAS_2BUTTON_PRESS;
		break;
	case E_GDK_3BUTTON_PRESS:
		type = GNOME_CANVAS_3BUTTON_PRESS;
		break;
	case GDK_BUTTON_RELEASE:
		type = GNOME_CANVAS_BUTTON_RELEASE;
		break;
	case GDK_MOTION_NOTIFY:
		type = GNOME_CANVAS_MOTION_NOTIFY;
		break;
	case GDK_KEY_PRESS:
		type = GNOME_CANVAS_KEY_PRESS;
		break;
	case GDK_KEY_RELEASE:
		type = GNOME_CANVAS_KEY_RELEASE;
		break;
	case GDK_ENTER_NOTIFY:
		type = GNOME_CANVAS_ENTER_NOTIFY;
		break;
	case GDK_LEAVE_NOTIFY:
		type = GNOME_CANVAS_LEAVE_NOTIFY;
		break;
	case GDK_FOCUS_CHANGE:
		type = GNOME_CANVAS_FOCUS_CHANGE;
		break;
	case GDK_SCROLL:
		type = GNOME_CANVAS_SCROLL;
		break;
	default:
		type = GNOME_CANVAS_NOTHING;
		break;
	}

	x = event ? e_gdk_event_x (event) : 0;
	y = event ? e_gdk_event_y (event) : 0;

	/* the GTK 3 bin window of a canvas scrolled with it */
	if (canvas && GTK_IS_SCROLLABLE (canvas)) {
		GtkAdjustment *adjustment;

		adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas));
		if (adjustment)
			x += gtk_adjustment_get_value (adjustment);

		adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas));
		if (adjustment)
			y += gtk_adjustment_get_value (adjustment);
	}

	gnome_canvas_event_set_from_gdk (cevent, type, event, x, y);

	if (!event)
		return cevent;

	cevent->any.state = e_gdk_event_state (event);

	switch (type) {
	case GNOME_CANVAS_BUTTON_PRESS:
	case GNOME_CANVAS_2BUTTON_PRESS:
	case GNOME_CANVAS_3BUTTON_PRESS:
	case GNOME_CANVAS_BUTTON_RELEASE:
		cevent->button.button = e_gdk_event_button (event);
		break;
	case GNOME_CANVAS_KEY_PRESS:
	case GNOME_CANVAS_KEY_RELEASE:
		cevent->key.keyval = e_gdk_event_keyval (event);
		cevent->key.hardware_keycode = e_gdk_event_keycode (event);
		cevent->key.is_modifier = e_gdk_event_is_modifier (event);

		if (!(cevent->key.state & (GDK_CONTROL_MASK | GDK_ALT_MASK))) {
			gunichar uc = gdk_keyval_to_unicode (cevent->key.keyval);

			if (uc && g_unichar_isprint (uc))
				cevent->key.length = g_unichar_to_utf8 (uc, cevent->key.string);
		}
		break;
	case GNOME_CANVAS_ENTER_NOTIFY:
	case GNOME_CANVAS_LEAVE_NOTIFY:
		cevent->crossing.mode = GDK_CROSSING_NORMAL;
		cevent->crossing.detail = GDK_NOTIFY_NONLINEAR;
		break;
	case GNOME_CANVAS_FOCUS_CHANGE:
		cevent->focus_change.in = e_gdk_event_focus_in (event);
		break;
	case GNOME_CANVAS_SCROLL:
		cevent->scroll.direction = e_gdk_event_direction (event);
		cevent->scroll.delta_x = e_gdk_event_delta_x (event);
		cevent->scroll.delta_y = e_gdk_event_delta_y (event);
		break;
	default:
		break;
	}

	return cevent;
}
