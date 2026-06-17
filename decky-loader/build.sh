#!/bin/bash
set -euxo pipefail
cd "$(dirname "$0")"; REPO=$PWD
source ./BASE.env
source ../toolchain.env

mkdir -p out; rm -rf out/*
podman run --rm -e VERSION="${VERSION}" -v "${REPO}:/work:Z" -w /work --platform linux/aarch64 "${BUILDER_IMAGE}" bash -euxc '
    export HOME=/tmp
    dnf -y install git nodejs npm python3.11 python3.11-devel gcc binutils make
    npm i -g pnpm

    git clone --branch "'"${VERSION}"'" --depth 1 https://github.com/SteamDeckHomebrew/decky-loader /tmp/decky
    git config --global --add safe.directory /tmp/decky

    # --ignore-scripts skips the deprecated husky `prepare` (fails on husky 9.1) and the
    # gated dep build scripts; `pnpm rebuild` then runs them (esbuild needs its binary).
    ( cd /tmp/decky/frontend && pnpm i --ignore-scripts && pnpm rebuild && pnpm run build )

    python3.11 -m ensurepip --upgrade
    python3.11 -m pip install --upgrade pip poetry
    cd /tmp/decky/backend
    poetry self add "poetry-dynamic-versioning[plugin]"
    poetry env use python3.11
    poetry install --no-interaction
    poetry run pyinstaller pyinstaller.spec

    cp -r dist/PluginLoader* /work/out/
'
