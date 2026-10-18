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

### 4. mail

`src/mail` (libevolution-mail, mail importers) builds with GTK 4.
Config pages and both config notebooks were subclasses of the now final
GtkScrolledWindow/GtkNotebook: they are `EGtkBin` subclasses now, the
page interface puts a scrolled window in the bin
(`e_mail_config_page_set_content()`), and the notebooks wrap a GtkNotebook
(`e_mail_config_notebook_get_notebook()`); modules' pages (backup-restore,
startup-wizard) are switched too. `e_mail_reader_get_popup_menu()` returns
a GtkPopoverMenu shown with `e_util_popup_menu_at_event()`; small menus
(remote content, subscription editor) are popovers with flat buttons;
message-list cut/copy/paste uses the clipboard compat instead of a
GtkInvisible; label colors are a GtkColorChooserWidget and their icons
PNG files in `$XDG_RUNTIME_DIR/evolution/label-icons` (no built-in icons).
Not verified yet: account reordering by drag (hooked to GtkTreeView's own
GtkDragSource), anything interactive.

### 5. calendar

`src/calendar` (libevolution-calendar, importers) builds with GTK 4; the
ATK `ea-*` files are out of the build like in e-util. The day and week
views share code between canvas item handlers and canvas widget signal
handlers: the shared code takes GnomeCanvasEvent, and the widget handlers
convert their GdkEvent with `e_gdk_event_to_canvas_event()` (canvas
window coordinates, like the GTK 3 bin window; `doxmail/port/canvas-entry.py`
rewrites such handlers). Explicit pointer grabs are gone (GTK 4 grabs for
the pressed widget implicitly; item grabs use `gnome_canvas_item_grab()`).
`draw` became `snapshot` (ECalDayColumn, EMeetingTimeSelector);
`style_updated` overrides became "style-updated" handlers
(`doxmail/port/style-updated.py`); menus are GMenu/GtkPopoverMenu
(to-do pane, day view time column) or popovers with check buttons
(meeting time selector Options/Autopick). More compat: `GdkPoint`,
`e_gtk_widget_get_modifier_state()`. Not verified at runtime: any of the
calendar views' mouse handling.

### 6. modules, web extensions, data: Evolution runs

`src/modules`, `src/web-extensions` (WebKitGTK 6 web process extensions)
and `data/` build with GTK 4, and `evolution` starts with the mail view
and the first-run assistant (`doxmail/screenshot-gtk4-first-run.png`).
`doxmail/port/run-evolution.sh [--gdb] [SECONDS]` installs the build into
`_inst-gtk4` (configure `_build-gtk4` with
`-DCMAKE_INSTALL_PREFIX=$PWD/_inst-gtk4` and `camel_providerdir`,
`edsuimoduledir` inside it, so nothing goes to system directories) and runs
it isolated; `SHOT=file.png` takes a screenshot.

Found at runtime: signals with a `GdkEvent` parameter must declare it as
`G_TYPE_POINTER` (GdkEvent is a fundamental type the generic marshaller
cannot pass), and handlers of legacy widget event signals must take
GdkEvent, not GnomeCanvasEvent (`doxmail/port/legacy-audit.py` lists them).
Also fixed at runtime: `size-allocate` handlers get the signal through
`e_gtk_widget_size_allocations()` (it wraps the class' size_allocate);
`map-event` became `map`; `GTK_TOGGLE_BUTTON()` is an unchecked cast, as
check buttons are no toggle buttons with GTK 4 and the
`e_gtk_toggle_button_*()` functions take both; WebKitGTK 6 names
(`initialize-web-process-extensions`, no `enable-java`); the private
library directory is added to the web process sandbox; GTK 4 widgets are
visible by default, so widgets GTK 3 kept hidden until needed must hide
themselves (alert and activity bars so far). `G_DEBUG=fatal-criticals
doxmail/port/run-evolution.sh --gdb` stops at the first critical.

Menus: GtkPopoverMenu does not drop the pages of submenus removed from its
model, so EUIMenu, which rebuilt its content from scratch, left stale
submenus ("duplicate child name in GtkStack"); it now fills a new menu and
syncs the shown one to it, keeping unchanged items and the submenu and
section GMenus. The compat `size-allocate` signal is emitted right after
the layout (GTK 3 handlers resize widgets, which GTK 4 forbids while
allocating). Minor: two "Trying to snapshot ECanvas without a current
allocation" warnings on the first frame.

**Layout managers**: GTK 4 measures and allocates widgets that have a
layout manager (GtkBox, GtkGrid, GtkFixed, GtkButton, ... and their
subclasses) through it, never calling the `measure`/`size_allocate`
methods of their classes. Classes that override those (the day and week
views, ECalDayColumn, ECalComponentWidget, EComposerHeaderTable,
EShellTaskbar, MessageList, EColorCombo) call
`e_gtk_widget_class_layout_methods()` in their class_init: the layout
manager class gets wrapped so that their methods run, and their chain-ups
reach the layout manager. Also: `style-updated` is emitted on realize, as
GTK 3 did; GtkScrollbar is no GtkRange. The calendar day view renders
(`doxmail/screenshot-gtk4-calendar.png`).

Buttons: GTK 3 buttons kept a label and an image together and showed only
the label when they had one (gtk-button-images was off); the
`e_gtk_button_set_image()` compat keeps the image and rebuilds the child
on label changes, the same way. The mailto handler prompt waits for the
shell window (it ran a nested loop inside GApplication::activate, which
kept the window from being created). Open: Adwaita for GTK 4 has no
full-color `dialog-*` icons (`image-missing` shows instead).

### 8. plugins

`src/plugins` builds with GTK 4; publish-calendar's GtkStatusIcon became a
GNotification of the application (unless libnotify shows the message).
The contacts view also runs.

Composer fixes from the first desktop run: the layout hook kept calling
the GtkWidget `measure` of classes overriding only `size_allocate` (the
header table got no height); GtkHeaderBar does not list packed widgets
as children, so EUIManager now remembers them (buttons were packed again
on each change); `margin` became the four margins; entry clipboard and
preedit signals are on the GtkText delegate; EUIMenu sync aligns items,
so an item shown or hidden in between does not recreate the rest.
Menus: GtkPopoverMenu names submenu pages by label and leaves the pages
of nested submenus behind when an item goes, so the EUIMenu sync empties
removed submenus first and drops a submenu that moved (hidden items merge
sections: the composer's Paragraph Style and Alignment) before adding it
again (`/eutil/menu-sync`).
`doxmail/port/run-on-desktop.sh` runs the build on the desktop with its
own HOME and D-Bus session, bringing the desktop theme along;
`doxmail/port/sample-stacks.sh` samples stacks with eu-stack.

### 7. GtkBuilder files

All 13 `.ui` files are GTK 4 files: `gtk4-builder-tool simplify --3to4
--replace`, then `doxmail/port/ui-leftovers.py` (GtkAlignment and button
boxes to GtkBox, ATK blocks and relations to GTK 4 accessibility, leftover
`<packing>`, empty list store cells) and `doxmail/port/ui-properties.py`
(checks every property against the real GTK 4 class through PyGObject:
translates border-width/xpad/ypad and icon sizes, drops the unknown and
read-only ones). By hand: the contact editor's config GtkMenu became a
popover of check buttons, the contact list editor sets a GtkBuilderCScope
for its handlers (GTK 4 connects them while building) and a shortcut for
its former `<accelerator>`, handlers that GTK 3 never connected went.
`test-ui-files` loads every file. Code that set GTK 3 only properties with
`g_object_set()`/`g_object_new()` (always-show-image, can-default,
draw-indicator, no-show-all, border-width) is fixed too.

## Next

Runtime: the other dialogs, real mouse/keyboard use; then the DoxIA items
below. The composer (`mailto:`), the contact, event, task and memo
editors, the preferences, the accounts window and the account assistant
(all its pages, up to a created account) run clean. GTK 4 entries emit
"changed" before "notify::text", which updates the bound extensions:
`e_mail_config_page_changed()` emits in a high priority idle, so the pages
check the new values. The assistant pages are taller than the 480 px
default with GTK 4 entries and scroll (overlay scrollbars).
Typing reaches the GtkText delegate of a GtkEntry, which alone emits
"insert-text"/"delete-text" then: handlers connected to the entry never
ran (the To field lost its address on focus-out, the widget undo recorded
nothing). ENameSelectorEntry (`NSE_EDIT_SIGNALS`), EWidgetUndo and the
event summary connect to the delegate (`/eutil/entry-undo`); grep for
these signals on new GTK 4 code. The delegate also has the focus, so
`gtk_widget_has_focus/is_focus (entry)` were FALSE while typing (the To
field closed each typed letter as an address): all the checks go through
`e_gtk_widget_has_focus/is_focus()`, which count the delegate.
GTK 4 GtkCheckButton is no GtkButton: `gtk_check_button_set_label()`;
GtkFontButton binds `font`, not `font-name`. `e_gtk_box_pack_start/end()`
apply expand/fill/padding along the orientation of the box layout manager
(EMailView's own "orientation" property is the preview orientation, the
message list stayed at its minimum width) and move them when it changes
(`/eutil/box-pack-orientation`); an expanding `pack_end()` child fills
the free space, the end box no longer sits at its natural size at the end
(the "Go to Folder" tree was under an empty gap, `/eutil/box-pack-end-expand`).
The other dialogs of the five views (folder, source, filter, search folder,
category, label, import, collection account, page setup, purge, jump-to,
manage groups, new/properties of each source) open without warnings.
Toolbars (EUIManager boxes) get an EToolbarLayout (src/e-util/
e-toolbar-layout.c, private): items at their natural width while they
fit, the rest hidden behind an overflow button whose menu, built when it
opens, has their actions (`/eutil/toolbar-overflow`). The shell window
can now get down to the header bar width (mail 518 px, was 931).

Headless runs reach dialogs through `DOXMAIL_ACTIVATE=action[,action...]`
(e-shell.c: activates actions of the first window or its active view once
they are enabled; `@sizes` prints the widgets at least `DOXMAIL_SIZES_MIN`
(400) px wide, `@click:Label` clicks a button, `@entry:Label:=text` fills
the entry of a mnemonic label, `@type:text` types into the focus through
its delegate, `@tab` moves the focus, in any window, `@popup:Name` opens
the context menu of the sidebar row with that name as Shift+F10 does, which
enables the actions of the clicked source or folder), e.g.
`DOXMAIL_ACTIVATE=contact-new SHOT=x.png doxmail/port/run-evolution.sh 18
-c contacts`. `run-evolution.sh` turns off the startup wizard and the
default mailer prompt (`FIRST_RUN=1` keeps them).

### DoxIA

- done: application ID `org.doxia.DoxMail`, binary `doxmail`, the
  `.desktop`/metainfo files generated from Evolution's in `data/CMakeLists.txt`,
  the icon also installed under the ID, "DoxMail" in window titles and About
  (CMake `DOXMAIL_APPLICATION_ID`/`DOXMAIL_BINARY`/`DOXMAIL_DISPLAY_NAME`,
  `E_APPLICATION_ID` in the config header). Data, settings, the keyring
  schema, the gresource prefix and the texts that say "Evolution" stay, so
  Evolution users keep their data; DoxMail refuses to start while
  `org.gnome.Evolution` is on the session bus (`src/shell/main.c`);
- DoxIA theme and look (doxia-theme colors, maybe libadwaita);
- packaging in the DoxIA image.

## Keeping up with upstream

`upstream` is gitlab.gnome.org/GNOME/evolution. Stable fixes come from the
`gnome-50` branch (3.60.x); merge them when Fedora's EDS reaches the same
version, since Evolution requires the matching EDS version.
