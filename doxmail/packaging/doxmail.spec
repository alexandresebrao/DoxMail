# DoxMail: the GTK 4 port of Evolution, the mail, calendar and contacts
# app of DoxIA. Built by doxmail/package from a git archive of this tree;
# the release comes from it (--define 'doxmail_release ...').

%global eds_series 3.60

Name:           doxmail
Version:        3.60.2
Release:        %{?doxmail_release}%{!?doxmail_release:1}%{?dist}
Summary:        Mail, calendar and contacts for DoxIA (Evolution on GTK 4)

License:        LGPL-2.0-only OR LGPL-3.0-only
URL:            https://github.com/alexandresebrao/DoxMail
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  ninja-build
BuildRequires:  gettext
BuildRequires:  intltool
BuildRequires:  python3
BuildRequires:  desktop-file-utils
BuildRequires:  appstream
BuildRequires:  pkgconfig(gtk4) >= 4.12
BuildRequires:  pkgconfig(libadwaita-1) >= 1.4
BuildRequires:  pkgconfig(webkitgtk-6.0)
BuildRequires:  pkgconfig(webkitgtk-web-process-extension-6.0)
BuildRequires:  pkgconfig(libedataserver-1.2) >= %{eds_series}
BuildRequires:  pkgconfig(libedataserverui4-1.0) >= %{eds_series}
BuildRequires:  pkgconfig(camel-1.2) >= %{eds_series}
BuildRequires:  pkgconfig(libebook-1.2) >= %{eds_series}
BuildRequires:  pkgconfig(libecal-2.0) >= %{eds_series}
BuildRequires:  pkgconfig(libebackend-1.2) >= %{eds_series}
BuildRequires:  pkgconfig(cairo-gobject)
BuildRequires:  pkgconfig(gdk-pixbuf-2.0)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(gsettings-desktop-schemas)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(shared-mime-info)
BuildRequires:  pkgconfig(libsecret-1)
BuildRequires:  pkgconfig(libsoup-3.0)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(enchant-2)
BuildRequires:  pkgconfig(nss)
BuildRequires:  pkgconfig(nspr)
BuildRequires:  pkgconfig(libnotify)
BuildRequires:  pkgconfig(gweather4)
BuildRequires:  pkgconfig(geocode-glib-2.0)
BuildRequires:  pkgconfig(libpst)
BuildRequires:  pkgconfig(libcmark)
BuildRequires:  pkgconfig(libical-glib)
BuildRequires:  libical-devel
BuildRequires:  libytnef-devel
BuildRequires:  openldap-devel

# Evolution's data, settings and accounts are shared, so the same mail goes
# on; the installed files are Evolution's too (libraries, modules, schemas)
Requires:       evolution-data-server%{?_isa} >= %{eds_series}
Requires:       evolution-data-server%{?_isa} < 3.61
Requires:       highlight
Conflicts:      evolution
Conflicts:      evolution-langpacks
Conflicts:      evolution-devel

%description
DoxMail is a fork of GNOME Evolution ported to GTK 4, the mail, calendar,
contacts, tasks and memos app of DoxIA. It uses Evolution's mail, settings
and accounts, so moving from Evolution keeps everything.

%prep
%autosetup -n %{name}-%{version}

%build
# The alarm notify module would be loaded by the system (GTK 3)
# evolution-alarm-notify of evolution-data-server
%cmake -G Ninja \
	-DWITH_GTK4=ON \
	-DWITH_HELP=OFF \
	-DENABLE_ALARM_NOTIFY_MODULE=OFF \
	-DENABLE_INSTALLED_TESTS=OFF \
	-DENABLE_GTK_DOC=OFF
%cmake_build

%install
%cmake_install

# no development files: the libraries are private to the app
rm -rf %{buildroot}%{_includedir}
rm -rf %{buildroot}%{_libdir}/pkgconfig

%find_lang evolution

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/org.doxia.DoxMail.desktop
appstreamcli validate --no-net %{buildroot}%{_metainfodir}/org.doxia.DoxMail.metainfo.xml

%files -f evolution.lang
%license COPYING
%doc README.md
%{_bindir}/*
%{_libdir}/evolution
%{_libexecdir}/evolution
%{_libdir}/evolution-data-server/camel-providers/libcamelrss.*
%{_datadir}/evolution
%{_datadir}/applications/*.desktop
%{_metainfodir}/*.xml
%{_datadir}/icons/hicolor/*/apps/*
%{_datadir}/glib-2.0/schemas/*
%{_datadir}/GConf/gsettings/*

%changelog
* Mon Oct 19 2026 Alexandre Krumenauer Sebrao <xandizitxu@mac.com> - 3.60.2-1
- DoxMail on GTK 4: Evolution 3.60.2 ported, DoxIA theme, pt_BR as DoxMail
