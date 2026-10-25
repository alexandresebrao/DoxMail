#!/usr/bin/python3
# Copies the Evolution settings the user changed on the host (dconf,
# read-only in the sandbox) into the app's own keyfile, once, on the first
# run of the DoxMail Flatpak. Flatpak's X-DConf migrate-path only takes a
# path under the app ID, and DoxMail keeps Evolution's schemas.
#
# migrate-settings.py KEYFILE

import os
import sys

keyfile = sys.argv[1]

# the dconf GIO module is private to this script; it reads the host
# database under the host config dir
os.environ["GIO_EXTRA_MODULES"] = "/app/lib/doxmail-migrate/gio-modules"
os.environ["GSETTINGS_BACKEND"] = "dconf"
os.environ["XDG_CONFIG_HOME"] = os.path.join(os.environ["HOME"], ".config")

import gi
gi.require_version("Gio", "2.0")
from gi.repository import Gio

if not os.path.exists(os.path.join(os.environ["XDG_CONFIG_HOME"], "dconf", "user")):
    sys.exit(0)

host = Gio.SettingsBackend.get_default()
if type(host).__name__ != "DConfSettingsBackend":
    print("migrate-settings: no dconf backend, settings not copied", file=sys.stderr)
    sys.exit(0)

os.makedirs(os.path.dirname(keyfile), exist_ok=True)
app = Gio.keyfile_settings_backend_new(keyfile, "/", None)

source = Gio.SettingsSchemaSource.get_default()
schemas, _relocatable = source.list_schemas(True)
copied = 0

for schema_id in schemas:
    if not schema_id.startswith("org.gnome.evolution"):
        continue
    schema = source.lookup(schema_id, True)
    from_host = Gio.Settings.new_full(schema, host, None)
    to_app = Gio.Settings.new_full(schema, app, None)
    to_app.delay()
    for key in schema.list_keys():
        value = from_host.get_user_value(key)
        if value is not None and schema.get_key(key).range_check(value):
            to_app.set_value(key, value)
            copied += 1
    to_app.apply()

Gio.Settings.sync()
print(f"migrate-settings: {copied} settings copied from the host", file=sys.stderr)
