#!/bin/bash
set -euxo pipefail
cd "$(dirname "$0")"; REPO=$PWD
source ./BASE.env
source ../toolchain.env

CCACHE_DIR="${CCACHE_DIR:-${REPO}/.ccache}"; mkdir -p "${CCACHE_DIR}"
mkdir -p out; rm -f out/*

podman run --rm \
    -e KERNEL_VERSION="${VERSION}" \
    -v "${REPO}:/work:Z" -w /work \
    -v "${CCACHE_DIR}:/ccache:Z" \
    -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=4G \
    --platform linux/aarch64 \
    "${BUILDER_IMAGE}" bash -euxc '
        dnf -y install gcc binutils make bc bison flex openssl-devel \
            elfutils-libelf-devel dwarves zstd xz cpio patch curl perl-interpreter python3 \
            findutils diffutils gawk grep sed coreutils hostname gzip tar ccache
        WORK_DIR=/tmp/armada-kernel-build OUT_DIR=/work/out \
            bash scripts/build-kernel.sh
    '
