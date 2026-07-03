#!/usr/bin/env bash
# Stage a package's build outputs into ctx/ in the layout oci/Containerfile
# copies into the carrier image. Single source for both the Justfile `image`
# recipe and CI. Usage: stage.sh <package>
set -euo pipefail
cd "$(dirname "$0")/.."
pkg="${1:?usage: stage.sh <package>}"

rm -rf ctx && mkdir -p ctx
case "${pkg}" in
    extest)       cp extest/out/libextest.so ctx/ ;;
    kernel)       mkdir -p ctx/kernel && cp kernel/out/armada-kernel-*.tar.zst kernel/out/armada-kernel-*.tar.zst.sha256 ctx/kernel/ ;;
    fex|mesa|mangohud|gamescope|inputplumber|networkmanager|jupiter-hw-support) mkdir -p ctx/rpms && cp "${pkg}"/out/*.rpm ctx/rpms/ ;;
    *) echo "unknown package: ${pkg}" >&2; exit 1 ;;
esac
