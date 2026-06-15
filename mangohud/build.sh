#!/bin/bash
set -euxo pipefail
cd "$(dirname "$0")"; REPO=$PWD
source ./BASE.env

mkdir -p out; rm -f out/*
podman run --rm -e VERSION="${VERSION}" -v "${REPO}:/work:Z" -w /work --platform linux/aarch64 fedora:44 bash -euxc '
    export HOME=/tmp
    dnf -y install rpm-build rpmdevtools spectool "dnf-command(builddep)"
    rpmdev-setuptree
    cat >/etc/rpm/macros.armada <<EOF
%_buildhost armada-builder
%packager Armada
%vendor Armada
EOF
    cp /work/mangohud.spec ~/rpmbuild/SPECS/
    sed -i "s/^Version:.*/Version:        ${VERSION}/" ~/rpmbuild/SPECS/mangohud.spec
    cp /work/patches/*.patch ~/rpmbuild/SOURCES/
    # fetch the upstream tarball
    spectool -g -R ~/rpmbuild/SPECS/mangohud.spec
    dnf -y builddep ~/rpmbuild/SPECS/mangohud.spec
    # meson downloads subprojects from the wraps (__meson_wrap_mode=default + network)
    rpmbuild -bb ~/rpmbuild/SPECS/mangohud.spec
    cp ~/rpmbuild/RPMS/*/mangohud-[0-9]*.armada.*.rpm /work/out/
'
