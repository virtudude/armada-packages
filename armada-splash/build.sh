#!/bin/bash
# Build the armada boot splash for aarch64 in a pinned Fedora container. Emits
# out/armada-splash: one binary with fbdev (early boot) + x11 (gamescope phase)
# backends. Links libX11.
set -euxo pipefail
cd "$(dirname "$0")"; REPO=$PWD
source ../toolchain.env   # BUILDER_IMAGE; no BASE.env (first-party, no upstream pin)

mkdir -p out; rm -f out/*
podman run --rm -v "${REPO}:/work:Z" -w /work --platform linux/aarch64 "${BUILDER_IMAGE}" bash -euxc '
    dnf -y install gcc libX11-devel

    # stb_truetype in its own TU (third-party; warnings suppressed).
    gcc -O2 -w -c stb_impl.c -o stb_impl.o

    gcc -O2 -Wall -Wextra -DHAVE_X11 -o out/armada-splash \
        armada-splash.c stb_impl.o $(pkg-config --cflags --libs x11) -lm

    strip out/armada-splash
    rm -f stb_impl.o
'
echo "built: ${REPO}/out/armada-splash"
