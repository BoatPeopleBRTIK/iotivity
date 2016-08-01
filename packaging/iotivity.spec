Name: iotivity
Version: 1.1.0
Release: 0%{?dist}
Summary: IoT Connectivity sponsored by the OIC
Group: Network & Connectivity/Other
License: Apache-2.0
URL: https://www.iotivity.org/
Source: %{name}-%{version}.tar.gz
BuildRequires:  gettext, expat-devel
BuildRequires:  python, libcurl-devel
BuildRequires:  scons
BuildRequires:  openssl-devel
BuildRequires:  boost-devel
BuildRequires:  boost-thread
BuildRequires:  boost-system
BuildRequires:  boost-filesystem
BuildRequires:  pkgconfig(uuid)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(sqlite3)
Requires(postun): /sbin/ldconfig
Requires(post): /sbin/ldconfig


#%define RELEASE False
%define RELEASE True

%{!?TARGET_TRANSPORT: %define TARGET_TRANSPORT IP}
%{!?SECURED: %define SECURED 0}
%{!?LOGGING: %define LOGGING True}
%{!?ROUTING: %define ROUTING GW}
%{!?ES_TARGET_ENROLLEE: %define ES_TARGET_ENROLLEE linux}
%{!?ES_ROLE: %define ES_ROLE enrollee}
%{!?ES_SOFTAP_MODE: %define ES_SOFTAP_MODE MEDIATOR_SOFTAP}

%description
An open source reference implementation of the OIC standard specifications
IoTivity Base Libraries are included.


%package service
Summary: Development files for %{name}
Group: Network & Connectivity/Service
Requires: %{name} = %{version}-%{release}

%description service
The %{name}-service package contains service libraries files for
developing applications that use %{name}-service.

%package test
Summary: Development files for %{name}
Group: Network & Connectivity/Testing
Requires: %{name} = %{version}-%{release}

%description test
The %{name}-test package contains example files to show
how the iotivity works using %{name}-test

%package devel
Summary: Development files for %{name}
Group: Network & Connectivity/Development
Requires: %{name} = %{version}-%{release}
Requires: pkgconfig

%description devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%prep
%setup -q
chmod g-w %_sourcedir/*

cp LICENSE.md LICENSE.APLv2

%build
%define RPM_ARCH %{_arch}

%ifarch armv7l armv7hl armv7nhl armv7tnhl armv7thl
%define RPM_ARCH "armeabi-v7a-hard"
%endif

%ifarch aarch64
%define RPM_ARCH "arm64"
%endif

%ifarch x86_64
%define RPM_ARCH "x86_64"
%endif

%ifarch %{ix86}
%define RPM_ARCH "x86"
%endif

export PKG_CONFIG_SYSROOT_DIR=%{_sysrootdir}
export PKG_CONFIG_PATH=${PKG_CONFIG_SYSROOT_DIR}/usr/lib/pkg-config
VERBOSE=1
scons %{?_smp_mflags} --prefix=%{_prefix} \
	TARGET_OS=linux TARGET_ARCH=%{RPM_ARCH} \
	TARGET_TRANSPORT=%{TARGET_TRANSPORT} \
	RELEASE=%{RELEASE} SECURED=%{SECURED}\
	ES_TARGET_ENROLLEE=%{ES_TARGET_ENROLLEE} ES_ROLE=%{ES_ROLE} \
	VERBOSE=yes TC_PREFIX=arm-linux-gnueabihf- TC_PATH=%{_sysrootdir}/usr \
	LIB_INSTALL_DIR=%{_libdir}

#SECURED=%{SECURED} LOGGING=%{LOGGING} ROUTING=%{ROUTING} \

%install
rm -rf %{buildroot}
CFLAGS="${CFLAGS:-%optflags}" ; export CFLAGS ;
scons install --install-sandbox=%{buildroot} --prefix=%{_prefix} \
	TARGET_OS=linux TARGET_ARCH=%{RPM_ARCH} \
	TARGET_TRANSPORT=%{TARGET_TRANSPORT} \
	RELEASE=%{RELEASE} SECURED=%{SECURED}\
	ES_TARGET_ENROLLEE=%{ES_TARGET_ENROLLEE} ES_ROLE=%{ES_ROLE} \
	VERBOSE=yes TC_PREFIX=arm-linux-gnueabihf- TC_PATH=%{_sysrootdir}/usr \
	LIB_INSTALL_DIR=%{_libdir}

#SECURED=%{SECURED} LOGGING=%{LOGGING} ROUTING=%{ROUTING} \

# For Example
%if %{RELEASE} == "True"
%define build_mode release
%else
%define build_mode debug
%endif
%define ex_install_dir %{buildroot}%{_bindir}
mkdir -p %{ex_install_dir}

cp out/linux/*/%{build_mode}/lib*.so %{buildroot}%{_libdir}
cp out/linux/*/%{build_mode}/lib*.a %{buildroot}%{_libdir}

cp out/linux/*/%{build_mode}/examples/OICMiddle/OICMiddle %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/devicediscoveryclient %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/devicediscoveryserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/fridgeclient %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/fridgeserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/garageclient %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/garageserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/groupclient %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/groupserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/lightserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/presenceclient %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/presenceserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/roomclient %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/roomserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/simpleclient %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/simpleclientHQ %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/simpleclientserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/simpleserver %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/simpleserverHQ %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/threadingsample %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/oic_svr_db_server.dat %{ex_install_dir}
cp out/linux/*/%{build_mode}/resource/examples/oic_svr_db_client.dat %{ex_install_dir}
cp out/linux/*/%{build_mode}/libcoap.a %{buildroot}%{_libdir}
%if 0%{?SECURED} == 1
mkdir -p %{ex_install_dir}/provisioning
mkdir -p %{ex_install_dir}/provision-sample

cp ./resource/csdk/security/include/pinoxmcommon.h %{buildroot}%{_includedir}
cp ./resource/csdk/security/provisioning/include/oxm/*.h %{buildroot}%{_includedir}
cp ./resource/csdk/security/provisioning/include/internal/*.h %{buildroot}%{_includedir}
cp ./resource/csdk/security/provisioning/include/*.h %{buildroot}%{_includedir}
cp ./resource/csdk/security/provisioning/sample/oic_svr_db_server_justworks.dat %{buildroot}%{_libdir}/oic_svr_db_server.dat

%endif


%if 0%{?tizen_version_major} < 3
mkdir -p %{buildroot}/%{_datadir}/license
cp LICENSE.APLv2 %{buildroot}/%{_datadir}/license/%{name}
cp LICENSE.APLv2 %{buildroot}/%{_datadir}/license/%{name}-service
cp LICENSE.APLv2 %{buildroot}/%{_datadir}/license/%{name}-test
%endif
cp resource/c_common/*.h %{buildroot}%{_includedir}
cp resource/csdk/stack/include/*.h %{buildroot}%{_includedir}

cp service/things-manager/sdk/inc/*.h %{buildroot}%{_includedir}
cp service/easy-setup/inc/*.h %{buildroot}%{_includedir}
cp service/easy-setup/enrollee/inc/*.h %{buildroot}%{_includedir}


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/liboc.so
%{_libdir}/liboc_logger.so
%{_libdir}/liboc_logger_core.so
%{_libdir}/liboctbstack.so
#%{_libdir}/libconnectivity_abstraction.so
%if 0%{?tizen_version_major} < 3
%{_datadir}/license/%{name}
%else
%license LICENSE.APLv2
%endif

%files service
%defattr(-,root,root,-)
%{_libdir}/libBMISensorBundle.so
%{_libdir}/libDISensorBundle.so
#%{_libdir}/libresource_hosting.so
%{_libdir}/libTGMSDKLibrary.so
%{_libdir}/libHueBundle.so
%{_libdir}/librcs_client.so
%{_libdir}/librcs_common.so
%{_libdir}/librcs_container.so
%{_libdir}/librcs_server.so
%{_libdir}/libESEnrolleeSDK.so
%if 0%{?SECURED} == 1
%{_libdir}/libocpmapi.so
%{_libdir}/libocprovision.so
%{_libdir}/oic_svr_db_server.dat
%endif
%if 0%{?tizen_version_major} < 3
%{_datadir}/license/%{name}-service
%else
%license LICENSE.APLv2
%endif

%files test
%defattr(-,root,root,-)
%{_bindir}/*
%if 0%{?tizen_version_major} < 3
%{_datadir}/license/%{name}-test
%else
%license LICENSE.APLv2
%endif

%files devel
%defattr(-,root,root,-)
%{_libdir}/lib*.a
%{_libdir}/pkgconfig/%{name}.pc
%{_includedir}/*
