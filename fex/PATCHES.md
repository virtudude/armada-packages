# Patches

Patches applied on top of BASE.env. Each entry's `source` is an upstream URL pinned
to a commit, or `armada` if it's original; a URL source with no `notes` is verbatim.
`notes` mean the file was modified.

- `patches/0001-fexcore-aarch64-workaround-llvm18-ice.patch`
  source: https://github.com/ROCKNIX/distribution/blob/214b17f900c9bda705ed371262f6654ea2451958/projects/ROCKNIX/packages/compat/fex-emu/patches/0001-fexcore-aarch64-workaround-llvm18-ice.patch
- `patches/0005-host-thunks-aarch64-char-signed-char.patch`
  source: https://github.com/ROCKNIX/distribution/blob/e485495a942daba186d4a8543e18a1ad09c9a5d5/projects/ROCKNIX/packages/compat/fex-emu/patches/0005-host-thunks-aarch64-char-signed-char.patch
  notes: modified -- hunk offsets rebased for FEX a04b0241; applies clean on 2607 (1cc4b93)
