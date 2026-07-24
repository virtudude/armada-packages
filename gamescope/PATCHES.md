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
- `patches/0007-steamcompmgr-fallback-appid-focus.patch`
  source: armada
- `patches/0008-drm-allow-explicit-edidless-internal-panel-profiles.patch`
  source: armada
  notes: Requires an internal DSI connector, `GAMESCOPE_INTERNAL_DISPLAY_ID`, a matching Lua profile with `allow_no_edid=true`, and complete validated physical size plus Gamma 2.2 HDR calibration before exposing support. Qualified whole-millimetre geometry overrides unreliable connector dimensions only in the generated client EDID.
- `patches/0009-drm-compose-gamma22-hdr-without-hardware-color-management.patch`
  source: armada
  notes: Prevents a single PQ layer from bypassing the Vulkan PQ-to-Gamma-2.2 transform on DRM drivers without hardware color management, with a Catch2 regression test.
- `patches/0010-wsi-filter-hdr-formats-by-underlying-support.patch`
  source: armada
  notes: Filters synthetic PQ/scRGB WSI entries by the VkFormats reported for the selected underlying surface, so Turnip clients cannot select an A2B10G10R10 format the driver will reject while AMD retains every format it actually supports. Covers both surface-format entry points, Vulkan count/VK_INCOMPLETE behavior, and preservation of application-owned VkSurfaceFormat2KHR output headers with Catch2 tests.
- `patches/0011-color-scale-sdr-white-on-gamma22-hdr-output.patch`
  source: armada
  notes: Applies the compositor's SDR-on-HDR white level to traditional Gamma 2.2 HDR outputs by scaling SDR input against the qualified panel peak. Ordinary SDR and external PQ output retain their existing paths; Catch2 covers the guarded gain calculation.
- `patches/0012-expose-client-sampleable-formats.patch`
  source: armada
  notes: Adds the opt-in `--expose-client-sampleable-formats` switch so the inner Wayland server can advertise DMA-BUF formats and modifiers that Vulkan can import and sample even when KMS planes cannot scan them out. The default backend intersection is unchanged. Direct scanout remains protected by the DRM backend framebuffer-import check, so a client buffer outside the KMS plane set falls back to Vulkan composition into a scanout-capable output buffer. Catch2 covers both the default policy and the opt-in bypass.
