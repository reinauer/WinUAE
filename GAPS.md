# Windows-to-macOS/Unix feature gaps

Reviewed: 2026-07-08
Updated: 2026-07-12

This note tracks WinUAE Windows features that are absent, partially present, or
misrepresented in the macOS/Unix port. It focuses on parity gaps that can affect
users, especially places where the Unix build links through stubs or advertises
formats/features without the matching backend.

## Fixed in this branch

### FloppyBridge real-drive support

Status: Core, packaging, and Qt configuration UI added

Unix and macOS builds now compile the shared WinUAE FloppyBridge path by
default through `WINUAE_UNIX_WITH_FLOPPYBRIDGE=ON`. This restores the core
real-drive interface used by DrawBridge, Greaseweazle, and SuperCard Pro.
The independently maintained FloppyBridge runtime is built from a pinned
upstream revision and included in the standard macOS and Linux packages. macOS
follows the existing plugin convention and loads it from the application's
`PlugIns` directory; Linux installs it in WinUAE's runtime plugin directory.
`WINUAE_FLOPPYBRIDGE_LIBRARY` can name an explicit library for development and
nonstandard installations.

The Qt floppy-drive selectors now match the Windows flow: they list the
available FloppyBridge profiles, store the selected profile in the WinUAE
configuration, prevent one profile from being assigned to multiple emulated
drives, and expose a profile manager in both the full configuration and
Quickstart paths. The profile editor covers driver selection, bridge mode,
density, serial-port selection and auto-detection, track caching, smart speed,
and IBM PC or Shugart drive selection when supported by the driver. This makes
DrawBridge selectable and configurable without editing configuration files.

This was missing from the original parity review because FloppyBridge was
entirely compiled out on Unix rather than exposed through a host stub or a
misleading file association. The review concentrated on the latter two
classes and did not compare the Windows-only feature defines exhaustively.

Verification:

- The macOS Qt `winuae` target builds and links with FloppyBridge enabled.
- The bundled `FloppyBridge.so` loads and reports the DrawBridge,
  Greaseweazle, and SuperCard Pro drivers.
- All seven Unix CTest targets pass with the Qt profile UI enabled.

### Unix debugger hooks and Qt debugger state pane

Status: Fixed by `3168ae07`

The Unix debugger is no longer a hidden stub. The Unix logging layer can use
either terminal or Qt debugger I/O, the Qt console can receive command output
and input, and `update_debug_info()` now publishes live CPU, data/address
register, CCR/status, prefetch, control-register, FPU, and FPSR state to the Qt
debugger pane.

Verification:

- `cmake --build /private/tmp/winuae_release_build --target winuae_unix`
  passed after the debugger changes.
- The debugger state pane is listed under current Unix feature coverage below.

### IPF/CAPS image support

Status: Fixed by `cdd12ec6`

The default Unix build now compiles CAPS support through
`WINUAE_UNIX_WITH_CAPS=ON` and the existing dynamic CAPS bridge. The bridge
searches the SoftPres macOS `CAPSImage.framework` locations and Linux
`libcapsimage.so.4`/`.4.2` runtime names, matching the SoftPres distribution
model where the user installs the IPF User Library separately.

The Unix UI and file classifiers are now tied to the build feature:

- Default builds define `CAPS` and keep `.ipf` associations and filters.
- `-DWINUAE_UNIX_WITH_CAPS=OFF` removes `.ipf` from Qt associations/filters
  and from core `.ipf`/CAPS-header file classification.
- A CAPS-header image in a non-CAPS build is rejected explicitly instead of
  falling through as a generic floppy image.

Verification:

- `cmake --build /private/tmp/winuae_release_build --target winuae_unix`
  passed with CAPS enabled.
- `/private/tmp/winuae_caps_off_build` configured with
  `-DWINUAE_UNIX_WITH_CAPS=OFF`, and `disk.cpp`, `zfile.cpp`, and the Qt
  launcher object compiled.
- The OFF build had no `caps_win32` target and no `-DCAPS=1`.

### Global ROM write protection

Status: Fixed by `273cccf1`

Unix no longer satisfies the ROM protection hooks with empty stubs. The Unix
memory manager now tracks ROM-like banks and uses `mprotect()` to switch their
host pages between writable and read-only states, matching the Windows
`VirtualProtect()` model for strict JIT ROM protection.

The implementation keeps Unix non-direct ROMs on page-aligned heap allocations
instead of forcing them into the direct-map allocation model. That gives
`mprotect()` page-aligned spans while preserving the non-direct memory layout
expected by the Unix port. `unprotect_maprom()` now restores writable access for
MAPROM windows that need temporary mutation.

The fix also updates ARM64 JIT PC-state storage for Unix non-direct memory.
Generated code now derives `regs.pc` from the previous `pc/pc_oldp` pair when
`natmem_offset` is zero, while keeping the direct `host_pc - natmem_offset`
path for Windows-style direct mappings. This avoids the crash exposed by
protectable ROM mappings on macOS/ARM64.

Verification:

- `cmake --build /private/tmp/winuae_release_build --target winuae_unix`
  passed after the ROM protection and ARM64 JIT fix.
- Strict ARM64 JIT with ROM write protection enabled survived 8-second and
  30-second smoke runs with `ROM mprotect ... WPROT` logged.
- Forced indirect trust mode survived 8-second and 30-second ARM64 JIT smoke
  runs.
- `git diff --check` passed.

Remaining useful validation:

- Exercise MAPROM and CD32 ROM patching paths in an interactive runtime
  configuration.
- Keep ARM64 JIT long-run testing separate from ROM protection parity, because
  Unix non-direct JIT had a latent PC-state hazard that ROM protection exposed.

### Missing-floppy recovery during state restore

Status: Fixed by `f3026691`

Unix no longer satisfies the state-restore replacement-disk hook with a stub.
When restored disk state references a missing floppy image, the shared disk
code now passes the restore buffer length to `gui_ask_disk()`, and the Unix
implementation opens the integrated Qt runtime file dialog for the affected
DFx: drive.

The Unix dialog path reuses the launcher/runtime image picker, including the
macOS symlink-preserving file dialog options. It pauses emulation-side input and
sound around the modal prompt, returns false on cancel or UI error, and copies
the selected replacement path back only when it fits the restore buffer. The
Windows implementation was updated to the same length-aware callback contract so
overlong replacement paths are rejected instead of truncated before
`drive_insert()`.

Verification:

- `cmake --build /private/tmp/winuae_release_build --target winuae_unix`
  passed after the recovery-dialog changes.
- `git diff --check` passed before commit.

### Video-grab and capture-backed genlock controls have no Unix backend

Status: Fixed by `44f8d4d1` and `0ab7cc0b`

Default Unix builds now enable `WINUAE_UNIX_WITH_VIDEOGRAB=ON`, define
`VIDEOGRAB=1`, and compile the new `od-unix/videograb.cpp` backend. Default
builds also enable `WINUAE_UNIX_WITH_FFMPEG=ON`; the FFmpeg/libav path becomes
active automatically when compatible libav 5.0-or-newer development packages
are available. The macOS release dependency helper builds FFmpeg for deployment
target 13.0 and the app bundler requires the libav dylibs whenever CMake enables
the FFmpeg backend.

The shared video-grab control actions are no longer wired to Unix no-ops:

- `AKS_VIDEOGRAB_RESTART` seeks to frame zero and resumes playback.
- `AKS_VIDEOGRAB_PAUSE` toggles the backend pause state.
- `AKS_VIDEOGRAB_PREV` and `AKS_VIDEOGRAB_NEXT` seek by frame.

The Unix backend implements the full `videograb.h` surface used by genlock and
LaserDisc paths, including initialization, frame retrieval, pause state,
position get/set, duration, and status updates. It currently supports:

- FFmpeg/libav-backed video-file and LaserDisc-backed genlock input for common
  container/codec files such as AVI, MP4, MOV, and MKV.
- SDL3 playback of source audio decoded from FFmpeg-backed video files.
- 24-bit uncompressed AVI input as the no-FFmpeg fallback.
- SDL3 camera capture for the `stream`/capture-device genlock mode.
- Tight BGR24 bottom-up frame buffers matching the layout expected by
  `specialmonitors.cpp`.

The old Unix stubs remain only for builds that explicitly disable the feature.
The Qt launcher gates video/capture/LaserDisc genlock choices on the build
feature. Builds with FFmpeg enabled expose a broader video-file picker; builds
without FFmpeg still narrow the picker to supported AVI input. The macOS bundle
now includes `NSCameraUsageDescription` so packaged builds can request camera
access.

Remaining limitation:

- SDL3 camera capture is video-only and does not yet attempt Windows
  DirectShow-style capture-device audio graph parity. File-backed genlock audio
  is handled by the FFmpeg path.

Verification:

- `cmake --build /private/tmp/winuae_release_build --target winuae_unix`
  passed after enabling VIDEOGRAB and adding the Unix backend.
- FFmpeg 8.1.2 was built into `/private/tmp/winuae_ffmpeg_macos13` with
  deployment target 13.0 and passed `tools/macos-check-deployment-target.sh`.
- `/private/tmp/winuae_ffmpeg_dmg_build` configured with
  `WINUAE_UNIX_WITH_FFMPEG=ON` and linked against the private libav dylibs.
- `/private/tmp/winuae_ffmpeg_dmg_build/package/WinUAE-6.1.0.dmg` was built
  and `tools/macos-verify-dmg.sh` passed.
- The built app bundle contains `libavformat.62.dylib`,
  `libavcodec.62.dylib`, `libavutil.60.dylib`, `libswscale.9.dylib`, and
  `libswresample.6.dylib` under `Contents/Frameworks`.
- `git diff --check` passed before commit.

## Hidden or misleading gaps

No remaining hidden stub-style gaps from this review are currently listed here.
Keep this section for future Windows-to-Unix parity findings where the UI or
shared core advertises a feature while Unix still supplies only a stub.

## Visible or intentionally disabled Unix UI gaps

These are already exposed as disabled controls or known missing integrations in
the Qt launcher. They are less risky than hidden stubs because the UI generally
does not claim they work.

### Host drive automount

- `od-unix/qt/launcher.cpp`: disables "Add PC drives", removable-drive,
  network-drive, and CD automount options with a tooltip.
- `od-unix/stubs.cpp`: `filesys_addexternals()` and
  `target_get_volume_name()` are stubs.
- `od-win32/win32_filesys.cpp`: implements Windows volume discovery and external
  filesystem mounting.

### Multi-monitor RTG placement

- `od-unix/qt/launcher.cpp`: disables single-RTG-monitor and initial-monitor
  override controls.
- Windows has a deeper monitor-selection and placement model through the Win32
  display backend.

### Native tablet input

- `od-unix/qt/launcher.cpp`: disables tablet-library and tablet-emulation
  controls when no Unix tablet backend is available.
- `od-unix/input.cpp`: `is_tablet()` returns false.
- `od-win32/dinput.cpp` and `od-win32/win32.cpp`: implement Wintab/touch tablet
  handling.

### Relative-path configuration mode

- `od-unix/qt/launcher.cpp`: relative paths are disabled.
- This is a launcher/config parity gap rather than an emulator-core gap.

### Windowed style and video API selection

- `od-unix/qt/launcher.cpp`: windowed style/video API controls are disabled or
  constrained by the Unix backend.
- The Windows UI exposes Windows-specific display and API options that do not map
  directly to SDL/Qt/macOS.

### OSD font/list customization

- `od-unix/qt/launcher.cpp`: OSD font/list customization is disabled.
- The core OSD path exists, but the Windows-specific customization surface is not
  fully represented in the Unix launcher.

### Host keyboard LED output

- `od-unix/qt/launcher.cpp`: host keyboard LED output is disabled.
- Windows can drive host keyboard LEDs through platform APIs.

### File association install/uninstall

- `od-unix/qt/launcher.cpp`: file association install/uninstall controls are
  disabled.
- The launcher still lists association choices for display/config purposes, but
  it does not install macOS Launch Services associations.

### State-file page save button

- `od-unix/qt/launcher.cpp`: the state-file page "Save state..." button is
  disabled.
- Integrated runtime shortcut paths can save and restore state, so this is a UI
  surface mismatch rather than a missing core feature.

## Current Unix feature coverage to avoid misclassifying as gaps

The recent macOS release build has many non-Windows features enabled. Do not
count these as missing without checking the active CMake cache and runtime path:

- JIT
- AVI/video output
- CHD and FLAC-backed CHD
- Native CD
- Native hard drives
- Native SCSI and UAESCSI
- bsdsocket
- SLIRP
- pcap-backed UAENET
- uaeserial
- MIDI and MIDI emulation
- libpng/image support
- ProWizard
- IPF/CAPS floppy images through the SoftPres runtime library
- ROM write protection for strict JIT ROM/MAPROM windows
- Missing-floppy replacement prompt during state restore
- Video-grab/genlock video input through FFmpeg/libav-backed files, raw 24-bit
  AVI fallback, SDL3 file audio playback, and SDL3 camera capture
- PPC QEMU plugin support
- OpenGL shader pipeline
- SDL3 and integrated Qt UI
- Debugger command console with live CPU, register, prefetch, control-register,
  FPU, and FPSR state updates

## Review notes

This was a static Windows-to-macOS/Unix parity review. The highest-risk class of
gaps is not "feature absent" by itself, but "feature appears available while the
Unix implementation is a stub or the required compile-time backend is absent."

The debugger issue fell into that class before the recent fixes: the core could
enter debugger paths, but Unix host debugger hooks were empty. Command-console
debugger support is now present, and the Unix Qt debugger consumes
`update_debug_info()` to show the same live CPU/FPU/register state that the
Windows debugger refreshes.

IPF/CAPS was another gap in that class: Unix advertised `.ipf` files while the
CAPS backend was absent. Default Unix builds now enable the CAPS bridge, and
non-CAPS builds no longer advertise or silently classify IPF/CAPS images.

Video-grab/genlock controls also used to fall into that class: shared input and
genlock code exposed video/capture-backed control paths while Unix only supplied
minimal no-op hooks. Default Unix builds now compile `VIDEOGRAB`, provide a Unix
backend, add FFmpeg/libav file decoding and file-audio playback when compatible
libav packages are present, and gate the launcher choices on those build
features. macOS release builds now build and bundle FFmpeg for macOS 13 so the
default DMG has the codec-backed path enabled.

Global ROM write protection also fell into this hidden-stub class. Unix now
tracks protectable ROM banks and toggles their host page protections with
`mprotect()`, while the ARM64 JIT PC-state path handles Unix non-direct memory
without relying on a nonzero `natmem_offset`.

Missing-floppy state-restore recovery was another hidden-stub case. The shared
disk code already had a host callback, but Unix returned false unconditionally.
Unix now uses the integrated Qt runtime disk picker and keeps the callback
length-aware on both Unix and Windows.
