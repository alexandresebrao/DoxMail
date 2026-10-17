# Porting DoxMail to GTK 4

Evolution is about 630k lines of C on GTK 3, WebKitGTK 4.1 and a bundled
copy of libgnomecanvas. GTK 3 and GTK 4 cannot live in one process.

The port first went bottom-up with every ported directory building with
both GTK versions (that was libgnomecanvas, tag `doxmail-gtk3-last`).
After that it is **GTK 4 only**: keeping two code paths through e-util
cost more than it saved. `-DWITH_GTK4=ON` builds the directories listed in
`src/CMakeLists.txt` under `if(WITH_GTK4)`; a directory joins the list
when it is the next one to port, and the compiler tells what is left.

Most of the work is mechanical and done by tools in `doxmail/port/`, with
compat layers in `src/e-util` that keep GTK 3 call shapes working on GTK 4
(see below). What the tools cannot do is done by hand, file by file.

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

### 2. e-util

`libevolution-util` builds and links with GTK 4 (no unresolved symbols),
and `test-eutil` checks the compat layers, `EAssistant` and some ported
widgets in headless weston. Compile errors went 3803 → 0.

**Compat layers** (`src/e-util`):

- `e-gtk4-compat.[ch]`: containers (`e_gtk_container_add`, box packing,
  bin child, child properties onto `GtkNotebookPage`/`GtkStackPage`/grid
  layout children/paned), dialogs run in a nested loop, the current event,
  icons (`GtkIconInfo` as `GtkIconPaintable`, sizes, pixbufs), stock
  buttons, `GtkCalendar` with GTK 3 months, radio buttons as grouped check
  buttons, toggle getters that take check buttons, `GtkAccelGroup` as a
  shortcut controller, header bar title, popovers, a font override, entry
  text index at a position, the pointer position, GtkScrollable forwarding
  for wrapper widgets, accessible names and labelled-by;
- `e-gtk-event-compat.[ch]`: the GTK 3 widget event signals
  (`key-press-event`, `button-press-event`, ..., `popup-menu`,
  `style-updated`) registered on GtkWidget and emitted from controllers
  for widgets set up with `e_gtk_widget_legacy_events()`; event vfunc
  overrides become class handler overrides; `e_gdk_event_*()` getters
  replace `event->field` and give widget coordinates, like GTK 3;
- `e-gtk-dnd-compat.[ch]`: GTK 3 drag and drop, selection data and
  clipboard on top of GtkDragSource/GtkDropTarget/GdkClipboard;
- `e-gtk-file-chooser-compat.[ch]`: a file chooser button, filename
  based API, no-op previews, extra widgets below dialogs;
- `e-assistant.[ch]`: a derivable stand-in for the final `GtkAssistant`.

**Tools** (`doxmail/port/`): `gtk4-rewrite.py` (renames, call rewrites,
dropped calls; `rewrite-all.sh` runs it on the tree), `measure.py`
(get_preferred_* → measure), `canvas-events.py` and
`canvas-event-fields.py` (canvas code → GnomeCanvasEvent),
`gdk-event-fields.py` (GdkEvent fields → getters), `errs.sh`/`cc.sh`
(errors of one file, compile one file), `gdb-test.sh` (backtrace of a
failing test).

**Patterns done by hand**: subclasses of types that are final with GTK 4
became wrappers (EPaned, EHeaderBar, EAttachmentIconView, EPictureGallery,
EColorChooserWidget, ECollectionAccountWizard, EMonthWidget, EAlertBar,
EActivityBar) or plain instances (EEllipsizedComboBoxText); GtkMenu became
GMenu + actions + GtkPopoverMenu (context menus of entries go through
`gtk_entry_set_extra_menu`); popup windows with grabs became popovers
(EDateEdit, ECellPopup, ENameSelectorList); toolbars are boxes.

**Known gaps** (to check when the app runs):

- accessibility: the ATK factories (ea-*, gal-a11y-*) are not built;
- keyboard-opened context menus of ESpellEntry/ENameSelectorEntry miss the
  extra items (they are built on right-button press);
- `e_gtk_entry_get_index_at_position()` assumes unscrolled entry text;
- no window placement (`window-position` dropped);
- real mouse and keyboard input is not tested automatically.

### 3. shell, libemail-engine, smime, em-format, addressbook, composer

All build with GTK 4, and the `evolution` binary starts and shows its
window (`doxmail/port/run-isolated.sh 15 _build-gtk4/src/shell/evolution`
with `GSETTINGS_SCHEMA_DIR=_build/data` runs it with its own HOME and
D-Bus session; without modules it only knows no views yet). More compat:
`EGtkBin` (GtkBin), `EGtkLayout` (GtkLayout), GtkToolbarStyle,
`delete-event`/`owner-change` emulation, nested main loops; and the
`size-allocate.py` tool. **GtkBuilder `.ui` files are not converted yet**
(contact editor, contact list editor, mail and calendar dialogs):
`gtk4-builder-tool simplify --3to4`, then fix by hand.

## Next

mail → calendar → modules (they give the shell its views) → plugins;
then the `.ui` files and `data/` (schemas, icons) in the GTK 4 build.
For each: add it to the `WITH_GTK4` list, rerun `rewrite-all.sh`, run
`canvas-events.py` on canvas code and `gdk-event-fields.py` on the rest,
then fix what the compiler reports. Calendar views mix canvas and widget
events; they need care.

### DoxIA

- application ID and binary name (`org.doxia.DoxMail`, `doxmail`) once it
  runs on GTK 4 — renaming earlier would only make upstream merges harder;
- DoxIA theme and look (doxia-theme colors, maybe libadwaita);
- packaging in the DoxIA image.

## Keeping up with upstream

`upstream` is gitlab.gnome.org/GNOME/evolution. Stable fixes come from the
`gnome-50` branch (3.60.x); merge them when Fedora's EDS reaches the same
version, since Evolution requires the matching EDS version.
