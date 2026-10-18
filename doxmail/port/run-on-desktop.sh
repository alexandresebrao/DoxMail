#!/bin/sh
# run-on-desktop.sh [EVOLUTION ARGS...]: runs the installed GTK 4 build
# (see run-evolution.sh) on the current Wayland desktop, with its own HOME
# (_home-gtk4, kept between runs) and D-Bus session, away from the user's
# real mail data and accounts.  The desktop look comes along: the themes,
# icons, fonts and GTK 4 CSS are linked into that HOME, and the
# org.gnome.desktop.interface settings are copied into its session.
cd "$(dirname "$0")/../.." || exit 1
home="$PWD/_home-gtk4"
mkdir -p "$home/run" "$home/.local/share" "$home/.config"
chmod 700 "$home/run"

for dir in .local/share/themes .local/share/icons .local/share/fonts .themes .icons .config/gtk-4.0 .config/fontconfig; do
	if [ -e "$HOME/$dir" ] && [ ! -e "$home/$dir" ]; then
		ln -s "$HOME/$dir" "$home/$dir"
	fi
done

settings=""
for key in gtk-theme icon-theme cursor-theme color-scheme font-name document-font-name monospace-font-name; do
	value=$(gsettings get org.gnome.desktop.interface "$key" 2>/dev/null) &&
		settings="$settings gsettings set org.gnome.desktop.interface $key \"$value\";"
done

display=${WAYLAND_DISPLAY:-wayland-0}
case "$display" in /*) ;; *) display="$XDG_RUNTIME_DIR/$display" ;; esac

exec env -i PATH="$PATH" HOME="$home" XDG_RUNTIME_DIR="$home/run" \
	WAYLAND_DISPLAY="$display" GDK_BACKEND=wayland LANG="${LANG:-C.UTF-8}" \
	GSETTINGS_SCHEMA_DIR="$PWD/_inst-gtk4/share/glib-2.0/schemas" \
	XDG_DATA_DIRS="$PWD/_inst-gtk4/share:/usr/local/share:/usr/share" \
	dbus-run-session -- sh -c "$settings exec _inst-gtk4/bin/doxmail \"\$@\"" evolution "$@"
