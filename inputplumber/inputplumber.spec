# armada InputPlumber RPM, built from the ShadowBlip fork at a pinned commit
%global appname InputPlumber
%global forgeurl https://github.com/ShadowBlip/InputPlumber
%global shortcommit %(c=%{commit}; echo ${c:0:7})
%global dbusname org.shadowblip.InputPlumber
# %changelog is intentionally empty; don't derive SOURCE_DATE_EPOCH from it.
%global source_date_epoch_from_changelog 0

Name:           inputplumber
# overwritten from BASE.env by build.sh
Version:        0
Release:        1%{?dist}.armada
Summary:        Input router and remapper daemon for handheld gaming devices

License:        GPL-3.0-or-later
URL:            %{forgeurl}
Source0:        %{forgeurl}/archive/%{commit}/%{name}-%{commit}.tar.gz
Patch1:         0001-fix-CapabilityMap-preserve-signed-axis-button-mappin.patch
Patch2:         0002-fix-gamepad-honor-passthrough-config-skip-exclusive-grab.patch

BuildRequires:  cargo
BuildRequires:  rust
BuildRequires:  clang
BuildRequires:  clang-devel
BuildRequires:  lld
BuildRequires:  make
BuildRequires:  cmake
BuildRequires:  pkgconf-pkg-config
BuildRequires:  systemd-devel
BuildRequires:  systemd-rpm-macros
BuildRequires:  libevdev-devel
BuildRequires:  libiio-devel
BuildRequires:  openssl-devel
BuildRequires:  dbus-devel
BuildRequires:  git-core

Requires:       dbus
%{?systemd_requires}

%description
InputPlumber detects, manages, and routes input from handheld gaming devices,
including combining devices into a single virtual gamepad. armada fork: carries
the dpad signed-axis-button mapping fix, and honors 'passthrough' for gamepad
source devices (upstream only wired it up for keyboard sources).

%prep
%autosetup -n %{appname}-%{commit} -p1

%build
make build BUILD_TYPE=release

%install
make install PREFIX=%{buildroot}%{_prefix}

%post
%systemd_post inputplumber.service

%preun
%systemd_preun inputplumber.service

%postun
%systemd_postun_with_restart inputplumber.service

%files
%license LICENSE
%doc README.md
%{_bindir}/inputplumber
%{_datadir}/dbus-1/system.d/%{dbusname}.conf
%{_datadir}/polkit-1/actions/%{dbusname}.policy
%{_datadir}/polkit-1/rules.d/%{dbusname}.rules
%{_prefix}/lib/systemd/system/inputplumber.service
%{_prefix}/lib/systemd/system/inputplumber-suspend.service
%{_prefix}/lib/udev/hwdb.d/*.hwdb
%{_prefix}/lib/udev/rules.d/*.rules
%dir %{_datadir}/inputplumber
%{_datadir}/inputplumber/devices/
%{_datadir}/inputplumber/schema/
%{_datadir}/inputplumber/capability_maps/
%{_datadir}/inputplumber/profiles/

%changelog
