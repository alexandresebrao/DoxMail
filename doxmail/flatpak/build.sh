#!/bin/sh
# build.sh [--install]: builds the DoxMail Flatpak from the committed
# doxmail branch into _flatpak/ (repo in _flatpak/repo); --install also
# installs it for the user from that repo.
cd "$(dirname "$0")/../.." || exit 1
set -e
builder="flatpak run --user org.flatpak.Builder"
$builder --user --force-clean --ccache --repo=_flatpak/repo \
	--state-dir=_flatpak/state _flatpak/build doxmail/flatpak/org.doxia.DoxMail.json
if [ "$1" = "--install" ]; then
	flatpak --user remote-add --no-gpg-verify --if-not-exists doxmail-local "$PWD/_flatpak/repo"
	flatpak --user install -y --reinstall doxmail-local org.doxia.DoxMail
fi
