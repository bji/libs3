Summary: C Library and Tools for Amazon S3 Access
Name: globus-s3
%global _name %(tr - _ <<< %{name})
Version: 0.0
Release: 3
License: LGPL
Group: Networking/Utilities
URL: http://github.com/globus/globus-s3
Source0: %{_name}-%{version}.tar.gz
Buildroot: %{_tmppath}/%{name}-%{version}-%{release}-root

%if %{?fedora}%{!?fedora:0} >= 19 || %{?rhel}%{!?rhel:0} >= 7
BuildRequires:  automake >= 1.11
BuildRequires:  autoconf >= 2.60
BuildRequires:  libtool >= 2.2
%endif

%if %{?rhel}%{!?rhel:0} == 5
Buildrequires: curl-devel
%else
Buildrequires: libcurl-devel
%endif

BuildRequires: libxml2-devel
BuildRequires: openssl-devel

%if %{?rhel}%{!?rhel:0} == 5 || %{?rhel}%{!?rhel:0} == 6
Requires: openssl
%else
%if %{?suse_version}%{!?suse_version:0} > 0
Requires: libopenssl0_9_8
%else
Requires: openssl-libs
%endif
%endif

%if %{?rhel}%{!?rhel:0} == 5
Requires: curl
%else
%if %{?suse_version}%{!?suse_version:0} > 0
Requires: libcurl4
%else
Requires: libcurl
%endif
%endif

Requires: libxml2

%description
This package is a modified version of libs3 which adds support for
accessing the Ceph Rados Gateway Admin endpoint to access user information.

%package devel
Summary: Headers and documentation for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
This package is a modified version of libs3 which adds support for
accessing the Ceph Rados Gateway Admin endpoint to access user information.
This package includes the development header files and libraries.
%prep

%setup -q -n %{_name}-%{version}

%build
%if %{?fedora}%{!?fedora:0} >= 19 || %{?rhel}%{!?rhel:0} >= 7
# Remove files that should be replaced during bootstrap
rm -rf autom4te.cache

autoreconf -if
%endif

%configure \
           --disable-static \
           --docdir=%{_docdir}/%{name}-%{version} \
           --includedir=%{_includedir}/globus/%{name} \
           --datadir=%{_datadir}/globus \
           --libexecdir=%{_datadir}/globus

make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

# Remove libtool archives (.la files)
find $RPM_BUILD_ROOT%{_libdir} -name 'lib*.la' -exec rm -v '{}' \;

# Remove the s3 executable which is not needed
rm -f $RPM_BUILD_ROOT%{_bindir}/s3

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libdir}/libglobus_s3.so.*

%files devel
%defattr(-,root,root,-)
%{_includedir}/globus/%{name}/*.h
%{_libdir}/libglobus_s3.so
%{_libdir}/pkgconfig/globus-s3.pc

%changelog
* Tue Feb 23 2016 Globus Toolkit <support@globus.org> - 0.0-3
- Adjust some dependencies for SLES 11SP3

* Tue Feb 23 2016 Globus Toolkit <support@globus.org> - 0.0-2
- Adjust some dependencies for older centos

* Fri Feb 19 2016  <bester@mcs.anl.gov> Joseph Bester - 0.0-1
- Based on Bryan Ischo's libs3 with radosgw modifications

* Sat Aug 09 2008  <bryan@ischo,com> Bryan Ischo
- Split into regular and devel packages.

* Tue Aug 05 2008  <bryan@ischo,com> Bryan Ischo
- Initial build.
