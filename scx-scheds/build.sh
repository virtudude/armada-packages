#!/bin/bash
set -euxo pipefail
cd "$(dirname "$0")"; REPO=$PWD
source ./BASE.env

mkdir -p out; rm -f out/*
podman run --rm \
    -e VERSION="${VERSION}" \
    -v "${REPO}:/work:Z" \
    -w /work \
    --platform linux/aarch64 \
    "${BUILDER_IMAGE}" bash -euxc '
    dnf -y install --skip-unavailable \
        rpm-build rpmdevtools spectool \
        cargo rust clang llvm bpftool git-core \
        cmake make gcc gcc-c++ protobuf-compiler dwarves \
        libbpf-devel elfutils-libelf-devel zlib-devel libzstd-devel \
        openssl-devel libseccomp-devel pkgconf-pkg-config systemd-rpm-macros
    rpmdev-setuptree
    cat >/etc/rpm/macros.armada <<EOF
%_buildhost armada-builder
%packager Armada
%vendor Armada
EOF
    cp scx-scheds.spec ~/rpmbuild/SPECS/
    sed -i "s/^Version:.*/Version:        ${VERSION}/" ~/rpmbuild/SPECS/scx-scheds.spec
    cp scx.default ~/rpmbuild/SOURCES/
    spectool -g -R ~/rpmbuild/SPECS/scx-scheds.spec
    rpmbuild -bb ~/rpmbuild/SPECS/scx-scheds.spec
    cp ~/rpmbuild/RPMS/aarch64/scx-scheds-*.rpm /work/out/
'
