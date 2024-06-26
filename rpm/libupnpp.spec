Summary:        C++ wrapper for libupnp
Name:           libupnpp
Version:        0.26.4
Release:        1%{?dist}
Group:          Applications/Multimedia
License:        GPLv2+
URL:            http://www.lesbonscomptes.com/updmpdcli
Source0:        http://www.lesbonscomptes.com/upmpdcli/downloads/libupnpp-%{version}.tar.gz
BuildRequires:  meson
BuildRequires:  libnpupnp
BuildRequires:  libcurl-devel
# OpenSUSE:
#BuildRequires:  libexpat-devel
# Fedora:
Requires(pre):  shadow-utils
BuildRequires:  expat-devel

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
libupnpp is a C++ wrapper and higher level API over libnpupnp.

%prep
%autosetup

%build
%meson
%meson_build

%install
%{__rm} -rf %{buildroot}
%meson_install

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, -)
%{_includedir}/libupnpp
%{_libdir}/libupnpp.so*
%{_libdir}/pkgconfig/*.pc

%changelog
* Tue Sep 19 2017 J.F. Dockes <jf@dockes.org> - 0.16.0
- New interfaces
* Fri May 19 2017 J.F. Dockes <jf@dockes.org> - 0.15.2
- Linn Receiver issue
* Sun Oct 09 2016 J.F. Dockes <jf@dockes.org> - 0.15.1
- Added pkgconfig data
* Sun Sep 11 2016 J.F. Dockes <jf@dockes.org> - 0.15.0
- Add code to support embedded device. Small interface changes.
* Sun Feb 07 2016 J.F. Dockes <jf@dockes.org> - 0.14.1
- Fix bug leading to possible upmpdcli crash when radio service failed to
  init (e.g.: because of no python2)
* Fri Feb 05 2016 J.F. Dockes <jf@dockes.org> - 0.14.0
- No systematic broadcasting of state info
* Wed Dec 22 2015 J.F. Dockes <jf@dockes.org> - 0.13.1
- Help managing upmpdcli in SenderReceiver mode
* Mon Dec 14 2015 J.F. Dockes <jf@dockes.org> - 0.13.0
- Support more OpenHome services (Sender, Radio, Info). Songcast utils.
* Mon Aug 17 2015 J.F. Dockes <jf@dockes.org> - 0.12.1
- Make it easier to create pure openhome devices
* Tue May 05 2015 J.F. Dockes <jf@dockes.org> - 0.11.0
- Control side interface to the OpenHome receiver service
- API cleanups for a more stable ABI
- More complete implementation of OpenHome services to support pure
  OpenHome devices
* Sun Oct 12 2014 J.F. Dockes <jf@dockes.org> - 0.8.4
- Separation from upmpdcli
