# Porting DoxMail to GTK 4

Evolution is about 630k lines of C on GTK 3, WebKitGTK 4.1 and a bundled
copy of libgnomecanvas. GTK 3 and GTK 4 cannot live in one process, so the
port goes bottom-up, one library at a time:

- every ported directory compiles with **both** GTK 3 and GTK 4
  (`#if GTK_CHECK_VERSION (4, 0, 0)` where they differ), so the GTK 3
  DoxMail keeps working the whole time;
- `-DWITH_GTK4=ON` builds only the directories listed in
  `src/CMakeLists.txt` under `if(WITH_GTK4)`; a directory joins the list once
  it builds and its checks pass;
- when the whole tree builds with GTK 4, the GTK 3 side is deleted.

Progress is measured with `doxmail/gtk4-audit.sh`, which counts uses of
GTK 3-only API outside the ported directories:

| step | GTK 3-only uses |
|------|----------------:|
| Evolution 3.60.2 | 5467 |
| libgnomecanvas ported | 5390 |

`doxmail/gtk4-audit.sh --dirs` shows where they are (e-util is half of it),
`--where <pattern>` lists the files for one pattern.

## Building and testing

```sh
cmake -S . -B _build-gtk4 -G Ninja -DWITH_GTK4=ON
doxmail/run-tests.sh _build-gtk4
```

`run-tests.sh` starts a headless weston and runs every
`src/doxmail-tests/test-*` in it. Use it rather than running the tests on
the desktop: a compositor stops sending frame callbacks to windows it does
not show (mango did), and then GTK 4 never lays the window out again.
`DOXMAIL_TEST_WRAPPER="gdb -batch -ex run -ex bt --args"` runs each test
under a wrapper.

`src/doxmail-tests/test-canvas --interactive` opens a demo canvas (hover,
drag the blue square, double-click, type) for checking input by hand.

Check the GTK 3 build too after touching shared code:
`cmake -S . -B _build -G Ninja && ninja -C _build`.

## Done

### 0. Fork

- Branch `doxmail` from tag 3.60.2 (`master` follows upstream and needs
  EDS 3.63, which Fedora 44 does not have).
- `WITH_GTK4` CMake option: GTK 4, WebKitGTK 6.0, libedataserverui4;
  turns off what needs GTK 3 libraries (gnome-desktop-3, canberra-gtk3,
  gspell, champlain, gnome-autoar-gtk, help).

### 1. libgnomecanvas

The canvas under ETable/ETree, the calendar views, EText, the contact
views and more. Changes:

- **`GnomeCanvasEvent`** (`gnome-canvas-event.h`). GTK 4 made `GdkEvent`
  opaque and read-only, but the canvas rewrites event coordinates to world
  space and makes up enter/leave events for items. With GTK 3 the type is
  `GdkEvent`; with GTK 4 it is a union with the same member and field
  names (`event->button.x`, `event->key.keyval`, ...), plus
  `GNOME_CANVAS_*` names for event types (including `2BUTTON_PRESS`),
  event masks and grab results, and `gnome_canvas_event_get_*()`
  accessors with the GTK 3 signatures.
  `gnome_canvas_event_get_gdk_event()` gives the real event where GTK
  insists on one (input methods, popovers); it is NULL for made-up events.
- **`GnomeCanvas`** is a `GtkWidget` implementing `GtkScrollable` instead of
  a `GtkLayout`: it keeps the layout size and adjustments itself, draws in
  `snapshot()` through a cairo node (GTK 4 has no partial invalidation, so
  `gnome_canvas_request_redraw()` redraws everything), and places child
  widgets in `size_allocate()`.
- **Input** comes from a `GtkEventControllerLegacy` (press, motion,
  release, keys; `GtkGestureClick` cancels itself on drags), a motion
  controller (enter/leave) and a focus controller. Double and triple clicks
  are counted from `gtk-double-click-time/-distance` to send GTK 3's
  `2BUTTON_PRESS`/`3BUTTON_PRESS` after the plain press.
- **Grabs**: GTK 4 only has the implicit grab of a pressed button.
  `gnome_canvas_item_grab()` routes events to the item and sets the cursor.
- **Child widgets**: `gnome_canvas_put_widget()`, `_move_widget()`,
  `_remove_widget()` replace `gtk_layout_put()` and friends (GTK 3 maps
  them back to those).
- **Accessibility**: the GAIL/ATK code builds only with GTK 3. TODO: a
  `GtkAccessible` implementation for items.
- The background is the CSS node `canvas`; there is no
  `theme_bg_color` lookup with GTK 4.

Checked: `test-canvas` (drawing by pixel, scrolling, picking, a GTK 4
button inside the canvas following scroll and item moves, event copies) in
headless weston; the GTK 3 tree builds and `test-calendar` (ECalendar, a
canvas) runs without warnings. **Not checked yet: real mouse and keyboard
input on GTK 4** — no input injection in headless weston; use
`test-canvas --interactive`.

## Next

### 2. Canvas items in e-util and the apps → `GnomeCanvasEvent`

46 files have `GnomeCanvasItem` event handlers. Switch them to
`GnomeCanvasEvent`, `GNOME_CANVAS_*` and `gnome_canvas_event_get_*()`.
This builds and runs with GTK 3, so it can land and be tested on the
current app before anything else. Watch for:

- `gdk_event_new()` in `e-table-item.c` and `e-text.c` (made-up events:
  build a `GnomeCanvasEvent` instead) and `event->*.window` (gone);
- about 25 `gtk_layout_get_bin_window()` calls on canvases (e-canvas,
  e-day-view, e-week-view, e-meeting-time-sel, e-calendar, e-cell-tree,
  the gal-a11y cells): `doxmail/gtk4-audit.sh --where layout`;
- `gdk_device_grab()` next to them (e-day-view, e-week-view, e-cell-combo,
  e-dateedit, e-name-selector-list): with GTK 4 the canvas items should use
  `gnome_canvas_item_grab()`, popups a `GtkPopover`.

`doxmail/gtk4-audit.sh --where canvas_event` lists what still uses the
GTK 3 names.

### 3. e-util on GTK 3, prepared for GTK 4

The GNOME porting guide's "preparation in GTK 3" steps, done in e-util
while it still runs on GTK 3, then the switch:

- `gtk_widget_show_all` / `gtk_widget_destroy` / `gtk_container_*` /
  `gtk_box_pack_*` (the bulk; mostly mechanical);
- `gtk_dialog_run` → async responses (121 uses);
- `GtkMenu`, `GtkToolbar` → `GMenuModel` + `GtkPopoverMenu`, boxes
  (Evolution 3.56 already moved actions to `EUIManager`/`GAction`, which
  helps a lot);
- `GtkClipboard` → `GdkClipboard`, DnD → `GtkDragSource`/`GtkDropTarget`;
- event signals → event controllers; `GdkWindow` → `GdkSurface`;
- `EWebView`: WebKitGTK 6.0, and the web extensions in
  `src/web-extensions` and `src/modules/webkit-editor/web-extension`
  become a web process extension.

Then add `e-util` to the `WITH_GTK4` list.

### 4. The rest, in dependency order

shell → libemail-engine (no GTK) → em-format → composer → mail →
addressbook → calendar → smime → modules → plugins.

### 5. DoxIA

- application ID and binary name (`org.doxia.DoxMail`, `doxmail`) once it
  runs on GTK 4 — renaming earlier would only make upstream merges harder;
- DoxIA theme and look (doxia-theme colors, maybe libadwaita);
- packaging in the DoxIA image.

## Keeping up with upstream

`upstream` is gitlab.gnome.org/GNOME/evolution. Stable fixes come from the
`gnome-50` branch (3.60.x); merge them when Fedora's EDS reaches the same
version, since Evolution requires the matching EDS version.
