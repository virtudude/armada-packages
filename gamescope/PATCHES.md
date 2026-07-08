# Patches

Patches applied on top of BASE.env. Each entry's `source` is an upstream URL pinned
to a commit, or `armada` if it's original; a URL source with no `notes` is verbatim.
`notes` mean the file was modified.

- `patches/0001-cstdint.patch`
  source: https://src.fedoraproject.org/rpms/gamescope/blob/cc1a9bd6aad3992a1bdaff27219efc1744478d8c/f/0001-cstdint.patch
- `patches/Allow-to-use-system-wlroots.patch`
  source: https://src.fedoraproject.org/rpms/gamescope/blob/5566fcac324cb909fd49a2323816deefc445a5fa/f/Allow-to-use-system-wlroots.patch
- `patches/Use-system-stb-glm.patch`
  source: https://src.fedoraproject.org/rpms/gamescope/blob/b5a75d544d1f314ef0d86c4dc9142b1de62e1b8e/f/Use-system-stb-glm.patch
- `patches/0004-DRMBackend-Add-GAMESCOPE_FAKE_OUTPUT_MM-env-to-set-c.patch`
  source: https://github.com/ROCKNIX/distribution/blob/ff40ff1897fa5687bc0e50103e50acc9cd90d7d3/projects/ROCKNIX/packages/apps/gamescope/patches/0004-DRMBackend-Add-GAMESCOPE_FAKE_OUTPUT_MM-env-to-set-c.patch
- `patches/0005-feature-add-rotation-shader-for-rotating-output.patch`
  source: https://github.com/ROCKNIX/distribution/blob/d5991e155a1941c248c8bcb9b364723eec75fc61/projects/ROCKNIX/packages/apps/gamescope/patches/0005-feature-add-rotation-shader-for-rotating-output.patch
- `patches/0006-steamcompmgr-fix-gamepad-cursor-sprite-frozen-via-XTest.patch`
  source: https://github.com/ROCKNIX/distribution/blob/e108ad2b8971b4e332d7457b75dd21dadb666d19/projects/ROCKNIX/packages/apps/gamescope/patches/0006-steamcompmgr-fix-gamepad-cursor-sprite-frozen-via-XTest.patch
- `patches/0007-WaylandBackend-forward-wl_touch-input.patch`
  source: armada
  notes: nested (Wayland backend) gamescope never requested wl_touch, so
  touchscreens were dead when gamescope runs inside a desktop session
  (dual-screen nested gaming). Upstream candidate.
- `patches/0008-WaylandBackend-nested-refresh-rates-env.patch`
  source: armada
  notes: nested backend can't enumerate host output modes; read
  GAMESCOPE_NESTED_REFRESH_RATES so Steam gets the panel's real refresh
  range (nested gaming session applies the mode switches via the host).
- `patches/0009-WaylandBackend-fullscreen-on-preferred-output.patch`
  source: armada
  notes: nested fullscreen honors -O/--prefer-output so gaming mode lands
  on the primary panel instead of whichever output the host compositor
  considered active. Upstream candidate.
