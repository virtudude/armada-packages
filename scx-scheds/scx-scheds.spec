# armada sched_ext scheduler RPM, built from a pinned upstream scx release.
%global forgeurl https://github.com/sched-ext/scx
%global source_date_epoch_from_changelog 0
%global debug_package %{nil}

Name:           scx-scheds
# overwritten from BASE.env by build.sh
Version:        0
Release:        1%{?dist}.armada
Summary:        sched_ext userspace schedulers for Armada

License:        GPL-2.0-only
URL:            %{forgeurl}
Source0:        %{forgeurl}/archive/refs/tags/v%{version}/scx-%{version}.tar.gz
Source1:        scx.default

BuildRequires:  cargo
BuildRequires:  rust
BuildRequires:  clang
BuildRequires:  bpftool
BuildRequires:  cmake
BuildRequires:  make
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  protobuf-compiler
BuildRequires:  dwarves
BuildRequires:  libbpf-devel
BuildRequires:  elfutils-libelf-devel
BuildRequires:  zlib-devel
BuildRequires:  libzstd-devel
BuildRequires:  openssl-devel
BuildRequires:  libseccomp-devel
BuildRequires:  pkgconf-pkg-config
BuildRequires:  systemd-rpm-macros

Requires:       bpftool
Requires:       libbpf
%{?systemd_requires}

%description
sched_ext allows Linux schedulers to be implemented in BPF and loaded from
userspace. Armada initially ships scx_lavd for gaming-focused scheduling,
scx_bpfland and scx_rusty as fallback schedulers, and scxtop for diagnostics.

%prep
%autosetup -n scx-%{version}

%build
cargo build --release --locked \
    -p scx_lavd \
    -p scx_bpfland \
    -p scx_rusty \
    -p scxtop

%install
install -Dpm 0755 target/release/scx_lavd %{buildroot}%{_bindir}/scx_lavd
install -Dpm 0755 target/release/scx_bpfland %{buildroot}%{_bindir}/scx_bpfland
install -Dpm 0755 target/release/scx_rusty %{buildroot}%{_bindir}/scx_rusty
install -Dpm 0755 target/release/scxtop %{buildroot}%{_bindir}/scxtop

install -Dpm 0644 services/scx.service %{buildroot}%{_unitdir}/scx.service
install -Dpm 0644 %{SOURCE1} %{buildroot}%{_sysconfdir}/default/scx

%post
%systemd_post scx.service

%preun
%systemd_preun scx.service

%postun
%systemd_postun_with_restart scx.service

%files
%license LICENSE
%doc README.md
%{_bindir}/scx_lavd
%{_bindir}/scx_bpfland
%{_bindir}/scx_rusty
%{_bindir}/scxtop
%{_unitdir}/scx.service
%config(noreplace) %{_sysconfdir}/default/scx

%changelog
