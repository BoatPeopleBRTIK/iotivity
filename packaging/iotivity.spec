Name: iotivity
Version: 1.3.1
Release: 0%{?dist}
Summary: IoT Connectivity sponsored by the OCF
Group: Development/Libraries
License: Apache-2.0
URL: https://www.iotivity.org/

%define _unpackaged_files_terminate_build 0

%if %{undefined _srcdir}
Source: %{name}-%{version}.tar.gz
%endif

%if %{undefined _sysrootdir}
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  chrpath
BuildRequires:  expat-devel
BuildRequires:  python
BuildRequires:  scons
BuildRequires:  libcurl-devel
BuildRequires:  boost-devel
BuildRequires:  boost-thread
BuildRequires:  boost-system
BuildRequires:  boost-filesystem
BuildRequires:  gettext
BuildRequires:  pkgconfig(uuid)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(sqlite3)
%endif

Requires(postun): /sbin/ldconfig
Requires(post): /sbin/ldconfig

%description
IoTivity is an open source software framework enabling seamless device-to-device connectivity to address the emerging needs of the Internet of Things.

%package service
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description service
The %{name}-service package contains service libraries files for
developing applications that use %{name}-service.

%package test
Summary: Development files for %{name}
Group: Development/Tools
Requires: %{name} = %{version}-%{release}

%description test
The %{name}-test package contains example files to show
how the iotivity works using %{name}-test

%package devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%ifarch armv7l armv7hl armv7nhl armv7tnhl armv7thl
%define BUILD_ARCH "armeabi-v7a"
%endif

%ifarch aarch64
%define BUILD_ARCH "arm64"
%endif

%ifarch x86_64
%define BUILD_ARCH "x86_64"
%endif

%ifarch %{ix86}
%define BUILD_ARCH "x86"
%endif

%if %{defined _sysrootdir}
%define SYSROOT %{_sysrootdir}
%define TC_PATH "TC_PATH=%{SYSROOT}/usr"
%else
%define SYSROOT ""
%define TC_PATH ""
%endif

%if %{defined _cross}
%define TC_PREFIX "TC_PREFIX=%{_cross}-"
%define __strip "%{_cross}-strip"
%else
%define TC_PREFIX ""
%endif

%define SCONS_OPTS TARGET_OS=linux TARGET_ARCH=%{BUILD_ARCH} RELEASE=True SECURED=1 TARGET_TRANSPORT=IP LOG_LEVEL=ERROR VERBOSE=yes %{TC_PREFIX} %{TC_PATH}

%prep
%if %{undefined _srcdir}
%setup -q
%else
cd %{_srcdir}
%endif
find . \
     -iname "LICEN*E*"  \
     -o -name "*BSD*" \
     -o -name "*COPYING*" \
     -o -name "*GPL*" \
     -o -name "*MIT*" \
     | sort | uniq \
     | grep -v 'libcoap-4.1.1/LICENSE.GPL'  \
     | while read file ; do \
          dir=$(dirname -- "$file")
          echo "Files: ${dir}/*"
          echo "License: ${file}"
          sed 's/^/ /' "${file}"
          echo ""
          echo ""
%if %{undefined _srcdir}
     done > tmp.tmp && mv tmp.tmp LICENSE
%else
     done > tmp.tmp && mv tmp.tmp %_builddir/LICENSE
%endif

%build
%if %{defined _srcdir}
cd %{_srcdir}
%endif
%if %{defined _sysrootdir}
export PKG_CONFIG_SYSROOT_DIR=%{_sysrootdir}
export PKG_CONFIG_PATH=%{SYSROOT}/usr/lib/pkgconfig
%endif
scons %{?_smp_mflags} --prefix=%{_prefix} %{SCONS_OPTS}

%install
%if %{defined _srcdir}
cd %{_srcdir}
%endif
rm -rf %{buildroot}
%if %{defined _sysrootdir}
export PKG_CONFIG_SYSROOT_DIR=%{_sysrootdir}
export PKG_CONFIG_PATH=%{SYSROOT}/usr/lib/pkgconfig
%endif
scons install --install-sandbox=%{buildroot} --prefix=%{_prefix} %{SCONS_OPTS}
rm -rf %{buildroot}/${HOME}
find %{buildroot} -type f -executable -exec chrpath -d "{}" \;
find %{buildroot} -type f -iname "lib*.so" -exec chrpath -d "{}" \;

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libconnectivity_abstraction.so
%{_libdir}/libipca.so
%{_libdir}/liboc.so
%{_libdir}/liboc_logger.so
%{_libdir}/liboc_logger_core.so
%{_libdir}/libocpmapi.so
%{_libdir}/libocprovision.so
%{_libdir}/liboctbstack.so
%{_libdir}/libresource_directory.so
%license LICENSE

%files service
%defattr(-,root,root,-)
%{_libdir}/libBMISensorBundle.so
%{_libdir}/libDISensorBundle.so
%{_libdir}/libESEnrolleeSDK.so
%{_libdir}/libESMediatorRich.so
%{_libdir}/libcoap_http_proxy.so
%{_libdir}/libHueBundle.so
%{_libdir}/librcs*.so
%{_libdir}/lib*plugin.so
%{_libdir}/libnotification*.so
%license LICENSE

%files test
%defattr(-,root,root,-)
%{_libdir}/iotivity/examples/*
%{_libdir}/iotivity/resource/csdk/security/provisioning/*
%{_libdir}/iotivity/resource/examples/*
%{_libdir}/iotivity/service/*
%license LICENSE

%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/pkgconfig/iotivity.pc
%{_libdir}/lib*.a
%{_libdir}/iotivity/resource/csdk/security/tool/*
%license LICENSE
