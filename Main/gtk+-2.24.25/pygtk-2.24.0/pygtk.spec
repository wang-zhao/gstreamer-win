# -*- mode: rpm-spec -*-
%define buildglade %(pkg-config libglade-2.0 && echo 1 || echo 0)
%define python python2

Summary: Python bindings for the GTK+ widget set.
Name: pygtk2
Version: 2.24.0
Release: 1
Copyright: LGPL
Group: Development/Languages
Source: ftp://ftp.gtk.org/pub/gtk/python/pygtk-%{version}.tar.gz
BuildRoot: /var/tmp/pygtk-root
Packager: James Henstridge <james@daa.com.au>
Requires: gtk2 >= 2.0.0
Requires: %{python} >= 2.2
Buildrequires: %{python}-devel >= 2.2
Buildrequires: libglade2-devel

%description
PyGTK is an extension module for python that gives you access to the GTK+
widget set.  Just about anything you can write in C with GTK+ you can write
in python with PyGTK (within reason), but with all the benefits of python.

%package libglade
Summary: A wrapper for the libglade library for use with PyGTK
Group: Development/Languages
Requires: pygtk2 = %{version}

%description libglade
This module contains a wrapper for the libglade library.  Libglade allows
a program to construct its user interface from an XML description, which
allows the programmer to keep the UI and program logic separate.

%package devel
Summary: files needed to build wrappers for GTK+ addon libraries
Group: Development/Languages
Requires: pygtk2 = %{version}

%description devel
This package contains files required to build wrappers for GTK+ addon
libraries so that they interoperate with pygtk.

%changelog
* Thu Oct 31 2002 Matt Wilson <msw@redhat.com>
- use libdir for .pc files

* Wed Feb 27 2002 Matt Wilson <msw@redhat.com>
- 1.99.8

* Mon Jan 28 2002 Matt Wilson <msw@redhat.com>
- added atkmodule.so to file list

* Thu Oct 18 2001 Matt Wilson <msw@redhat.com>
- fix devel filelist to match new header location

* Mon Oct 15 2001 Matt Wilson <msw@redhat.com>
- get the headers from their new version-specific location

* Thu Oct 11 2001 Matt Wilson <msw@redhat.com>
- fixed typo in devel filelist
- added macro that tests to see if we have libglade2, make the
  filelist a condition of that
- changed name to 'pygtk2' to avoid name conflict with pygtk

%prep
%setup -q -n pygtk-%{version}
[ -x /usr/bin/python2.2 ] && export PYTHON=/usr/bin/python2.2
./configure --prefix=%{_prefix}

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
find $RPM_BUILD_ROOT -type f -name "*.la" -exec rm -f {} ';'

%files
%defattr(644, root, root, 755)
%{_prefix}/lib/python?.?/site-packages/pygtk.py*
%dir %{_prefix}/lib/python?.?/site-packages/gtk-2.0
%dir %{_prefix}/lib/python?.?/site-packages/gtk-2.0/gtk
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/gtk/__init__.py*
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/gtk/compat.*
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/gtk/keysyms.py*
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/dsextras*

%defattr(755, root, root, 755)
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/atk.so
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/gobject.so
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/pango.so
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/gtk/_gtk.so

%doc AUTHORS NEWS README MAPPING ChangeLog
%doc examples

%if %{buildglade}
%files libglade
%defattr(755, root, root, 755)
%{_prefix}/lib/python?.?/site-packages/gtk-2.0/gtk/glade.so
%endif

%files devel
%defattr(755, root, root, 755)
%defattr(644, root, root, 755)
%dir %{_prefix}/include/pygtk-2.0
%{_prefix}/include/pygtk-2.0/*.h
%dir %{_prefix}/include/pygtk-2.0/pygtk
%{_prefix}/include/pygtk-2.0/pygtk/*.h
%{_libdir}/pkgconfig/pygtk-2.0.pc
%dir %{_prefix}/share/pygtk
%dir %{_prefix}/share/pygtk/2.0
%dir %{_prefix}/share/pygtk/2.0/defs
%{_prefix}/share/pygtk/2.0/defs/atk.defs
%{_prefix}/share/pygtk/2.0/defs/atk-types.defs
%{_prefix}/share/pygtk/2.0/defs/pango.defs
%{_prefix}/share/pygtk/2.0/defs/pango-types.defs
%{_prefix}/share/pygtk/2.0/defs/gdk.defs
%{_prefix}/share/pygtk/2.0/defs/gdk-types.defs
%{_prefix}/share/pygtk/2.0/defs/gtk.defs
%{_prefix}/share/pygtk/2.0/defs/gtk-types.defs
%{_prefix}/share/pygtk/2.0/defs/gtk-extrafuncs.defs
%{_prefix}/share/pygtk/2.0/defs/libglade.defs
