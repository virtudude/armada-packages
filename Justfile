# armada-packages build orchestrator.
#
# Each package's build.sh produces its artifacts in <pkg>/out/ (most run in a
# fedora:44 podman container; kernel builds natively). `image` wraps those into
# the scratch carrier image armada bind-mounts at build time.
#
# Run on an aarch64 host — the kernel/mesa builds under x86 qemu emulation are
# unusably slow.

registry := env("REGISTRY", "localhost/armada-packages")
packages := "extest armada-splash inputplumber fex mesa mangohud gamescope networkmanager jupiter-hw-support kernel"

import? 'Justfile.local'

[private]
default:
    @just --list

# Build one package's artifacts into <pkg>/out/
[group('build')]
artifacts pkg:
    cd {{pkg}} && ./build.sh

# Build + wrap one package as {{registry}}/<pkg>:latest
[group('build')]
image pkg: (artifacts pkg)
    #!/usr/bin/env bash
    set -euo pipefail
    bash scripts/stage.sh {{pkg}}
    buildah build -f oci/Containerfile -t "{{registry}}/{{pkg}}:latest" .
    echo "==> {{registry}}/{{pkg}}:latest"

# Build artifacts for every package
[group('build')]
all:
    #!/usr/bin/env bash
    set -euo pipefail
    for p in {{packages}}; do just artifacts "$p"; done

# Build images for every package
[group('build')]
images:
    #!/usr/bin/env bash
    set -euo pipefail
    for p in {{packages}}; do just image "$p"; done

# Remove staging dir and all build outputs
[group('build')]
clean:
    rm -rf ctx */out
