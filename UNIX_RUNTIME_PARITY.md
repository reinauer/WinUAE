# WinUAE Unix Runtime Parity Audit

This audit compares the current Unix CMake target against the Windows build surface in `od-win32/sysconfig.h`, `od-win32/winuae_msvc15/winuae_msvc.vcxproj`, the Unix feature flags in `od-unix/sysconfig.h`, the Unix CMake source list, and the remaining host stubs in `od-unix/stubs.cpp`.

Status values:

- **Enabled**: linked and backed by a Unix implementation.
- **Partial**: linked, but incomplete or not yet validated against Windows behavior.
- **Stubbed**: build symbols exist, but the Unix implementation returns no-op/empty results.
- **Deferred**: Windows supports it, but the Unix build intentionally does not enable it yet.
- **Windows-only**: not planned as a direct port; needs a native Unix equivalent or UI hiding.

## Core Emulator

| Windows feature area | Unix status | Evidence | Next action |
| --- | --- | --- | --- |
| 68000/68010/68020/68030/68040/68060 interpreter cores, MMU/FPU/softfloat | Enabled | `od-unix/sysconfig.h` defines `CPUEMU_0`, `CPUEMU_11`, `CPUEMU_13`, `CPUEMU_20`-`CPUEMU_24`, `CPUEMU_31`-`CPUEMU_35`, `CPUEMU_40`, `CPUEMU_50`, `FPUEMU`, `MMUEMU`, `FULLMMU`; CMake links the generated CPU files. | Keep covered by boot smoke tests. |
| x86/x64 JIT | Deferred | Windows defines `JIT` and links `jit/*.cpp`; Unix does not define `JIT` and CMake does not link JIT sources. | Treat x86_64 JIT and arm64 JIT as separate tracks after interpreter parity is stable. |
| Custom chipset, CIA, blitter, events, drawing, Action Replay, Arcadia, A-Max, CDTV/CD32 | Enabled | CMake links the shared core files and Unix defines `ECS_DENISE`, `AGA`, `CD32`, `CDTV`, `ACTION_REPLAY`, `ARCADIA`, `AMAX`. | Add smoke configs beyond A1200 as regressions appear. |
| Keyboard MCU emulation | Stubbed | Windows links `kbmcu/*`; Unix satisfies the symbols in `od-unix/stubs.cpp` with no-op save/restore/run functions. | Link the MCU sources or keep disabled in UI for models needing exact keyboard MCU behavior. |
| Enforcer | Enabled | Unix defines and links `enforcer.cpp`. | No immediate action. |
| Savestates | Partial | Unix defines `SAVESTATE` and links `savestate.cpp`; P96, SCSI, keyboard MCU, screenshot/log helpers in `od-unix/stubs.cpp` save empty host state. | Add targeted savestate smoke tests once RTG, SCSI, and host devices are enabled. |

## Storage And Media

| Windows feature area | Unix status | Evidence | Next action |
| --- | --- | --- | --- |
| ADF/floppy core, SCP, FDI2RAW | Enabled | Unix defines `SCP` and `FDI2RAW`; CMake links `disk.cpp`, `diskutil.cpp`, `scp.cpp`, `fdi2raw.cpp`. | Keep boot/install floppy smoke tests. |
| FloppyBridge and Catweasel hardware | Deferred | Windows defines `FLOPPYBRIDGE` and `CATWEASEL`; Unix does not, and CMake does not link those sources. | Keep disabled until a native hardware access backend is designed. |
| Image-backed hardfiles | Enabled | CMake links `hardfile.cpp` and `od-unix/hardfile_host.cpp`; the Unix host backend uses normal file I/O and `ftruncate`. | Validate read/write/resize with HDF smoke tests. |
| Directory filesystems | Enabled | Unix defines `FILESYS` and `UAE_FILESYS_THREADS`; CMake links `filesys.cpp`, `fsdb.cpp`, `fsdb_unix.cpp`, `od-unix/filesys_host.cpp`. | Add host filesystem round-trip tests for permissions, symlinks, case behavior, and timestamps. |
| Windows shortcut/shell-link behavior | Windows-only | `od-unix/filesys_host.cpp` intentionally returns false for Windows shortcut helpers. | Hide or document Windows-only shortcut behavior. |
| CD image support | Partial | CMake links `blkdev.cpp`, `blkdev_cdimage.cpp`, `cdrom.cpp`, `isofs.cpp`; native optical drive masks in `od-unix/stubs.cpp` return zero. | Keep image-backed CDs enabled; add native optical-device enumeration only if needed. |
| Native SCSI, SPTI, raw optical, tape/scanner media | Deferred/Stubbed | Windows defines `SCSIEMU`, `WITH_SCSI_IOCTL`, `WITH_SCSI_SPTI` and links Win32 SPTI/IOCTL code; Unix does not define `SCSIEMU`, and `scsi_get_cd_drive_mask`, `scsi_add_tape`, save/restore scsidev stubs return empty results. | Add POSIX/Linux/macOS native SCSI/media passthrough behind explicit feature flags. |
| A2091/A590 SCSI controller emulation | Enabled | Unix defines `A2091` and CMake links `a2091.cpp`, `scsi.cpp`, `scsitape.cpp`. | Test image-backed SCSI hardfiles separately from native device passthrough. |
| NCR/NCR9X SCSI controllers | Partial | CMake option `WINUAE_UNIX_WITH_NCR_SCSI` defaults on, defines `NCR` / `NCR9X`, and links `ncr_scsi.cpp`, `ncr9x_scsi.cpp`, and the required `qemuvga` SCSI helper sources. A headless A4000 smoke with a real A4091 ROM reaches valid Z3 autoconfig and maps the `NCR53C700/800` board, and `tools/unix-smoke-a4091-hdf.sh` covers the image-backed A4091 hardfile path. PPC CPU-board callbacks used by the same source remain stubbed in `od-unix/stubs.cpp`. | Validate Fastlane, Oktagon, and related NCR/NCR9x boards with real board ROMs; keep PPC CPU-board SCSI deferred until PPC support exists. |
| ArchiveAccess, ZIP/RAR/7z/LHA/LZX/DMS/WRP, CHD | Stubbed/Deferred | Windows defines `ARCHIVEACCESS`, `WITH_CHD`, and archive format flags; Unix does not define them, CMake does not link `zfile_archive.cpp` or most archive sources, and `od-unix/stubs.cpp` returns `NULL` for archive access. | Decide whether to link portable archive implementations or keep archive browsing disabled. |

## Graphics, Display, And RTG

| Windows feature area | Unix status | Evidence | Next action |
| --- | --- | --- | --- |
| Main native/OCS/ECS/AGA framebuffer output | Enabled | CMake uses `od-unix/video_sdl.cpp` when SDL3 is available, otherwise `video_null.cpp`; `od-unix/graphics.cpp` and SDL present the emulator frame. | Continue SDL3 path as the bring-up backend; later add Metal/Vulkan/OpenGL backend. |
| Bottom status/drive LED line | Partial | `od-unix/video_sdl.cpp` renders the shared `statusline` surface and supports first click actions. | Add Windows-style popup/menu behavior and polish runtime settings re-entry. |
| Display enumeration, multi-monitor, fullscreen modes, vblank scanline/beam racing | Stubbed/Partial | `od-unix/stubs.cpp` returns one `"Unix display"`; `od-unix/graphics.cpp` returns fixed vblank values and no scanline/fullscreen switching. Windows implements this in `od-win32/win32gfx.cpp` and Direct3D/GDI code. | Add a Unix display abstraction before exposing advanced display controls as active. |
| Direct3D 9/11 rendering, HDR, overlays, masks, shader presets | Windows-only/Deferred | Windows defines `D3D`, `WITH_DIRECT3D9`, `WITH_DIRECT3D11` and links Direct3D files; Unix does not. | Implement native shader/filter pipeline through Unix graphics instead of porting Direct3D. |
| Screenshots and video/audio capture | Partial | Unix has a first runtime screenshot action that writes a BMP from the active framebuffer to the configured Screenshots path, including the integrated Qt Output-page button when opened during emulation. The standalone launcher keeps the runtime-only button disabled. Windows still has fuller `screenshot.cpp` behavior: PNG, palette/indexed options, clipboard, autoclip, continuous screenshots, savestate thumbnails, and AVI/video capture. Unix `save_screenshot`, `save_p96`, and videograb helpers remain no-ops. | Add PNG/clipboard/autoclip parity, then savestate thumbnails and capture codec work. |
| Picasso96 / `uaegfx.card` Amiga-side entry points | Partial | Unix defines `PICASSO96`; `od-unix/rtg.cpp` installs a first `uaegfx.card` path and `od-unix/graphics.cpp` renders basic RTG buffers. `tools/unix-smoke-uaegfx.sh` verifies install-level `uaegfx.card` and P96 resolution-memory setup, and has optional stricter checks for `FindCard`, `InitCard`, and screen-switch logs when a P96-installed guest config is available. | Validate with guest Picasso96 monitor drivers, then require the stricter smoke mode for RTG-capable test images and expand accelerated operations. |
| UAE Zorro II/Zorro III RTG RAM | Partial | Unix `od-unix/rtg.cpp` advertises only `GFXBOARD_UAE_Z2` and `GFXBOARD_UAE_Z3`; the current smoke covers Z3 autoconfig and RTG RAM mapping. | Add Z2 coverage and guest mode-switch tests for CLUT/direct-color modes. |
| Hardware graphics boards, PCI graphics, Voodoo, QEMU VGA | Deferred | Windows defines `GFXBOARD`, `WITH_PCI`, `WITH_QEMU_CPU`, and links `gfxboard.cpp`, `pci.cpp`, `pcem/*`, `qemuvga/*`, `mame/*`; Unix only links the `qemuvga` SCSI helper subset needed by NCR/NCR9x SCSI. | Re-enable board families one at a time after RTG core is solid. |
| Picasso96 accelerated blits and write-watch | Partial/Stubbed | Unix routes unsupported P96 board functions to `unix_picasso_default_unsupported`; `picasso_getwritewatch` returns the whole VRAM dirty region. Windows has full `picasso96_win.cpp`. | Implement operations needed by common P96 drivers first, then optimize. |

## Audio And Input

| Windows feature area | Unix status | Evidence | Next action |
| --- | --- | --- | --- |
| Paula audio output | Enabled | `od-unix/sound.cpp` uses SDL3 `SDL_AudioStream`, enumerates SDL playback devices, and honors `unix.soundcard` / `unix.soundcardname` in the same index-plus-name pattern as Windows. The A1200 smoke test checks SDL dummy audio initialization. | Test real host output devices on macOS/Linux and keep advanced backends disabled until implemented. |
| Floppy drive sounds | Partial | Unix defines `DRIVESOUND`; `od-unix/sound.cpp` calls `driveclick_init()` and CMake links `od-unix/driveclick.cpp`. The Qt Sound page round-trips `floppyNsound` and per-drive empty/disk volumes, but real-host testing has not produced audible drive clicks yet. | Investigate the drive-click mixer path after higher-priority UI and packaging work. |
| Sample ripper | Partial | The core `audio_sampleripper()` path is linked on Unix and the integrated Qt Output page now toggles `sampleripper_enabled` like the Windows Output dialog. Ripped WAV files use `fetch_ripperpath()`, which now honors the Qt Paths page's `unix.rip_path` setting. | Validate with a real sample-ripping workload. |
| Pro Wizard module ripper | Partial | CMake option `WINUAE_UNIX_WITH_PROWIZARD` defaults on, defines `PROWIZARD`, compiles the same Pro Wizard source list as the Windows project, and enables the integrated runtime Qt Output-page button. Unix now routes `gui_message_multibutton()` through Qt warning dialogs when the integrated Qt UI is available, preserving Windows' OK/Yes/No/Cancel return values. Non-Qt builds still log and fall back to the old automatic Yes result. | Validate real module-ripping output and consider a non-Qt console prompt if Pro Wizard is exposed outside the integrated Qt UI. |
| AHI sound emulation | Deferred | Windows defines `AHI` and links `od-win32/ahidsound_*`; Unix does not define `AHI`. | Defer until core audio output and MIDI/sound-board work are stable. |
| Sound boards, Toccata, DSP, MIDI emulation | Deferred | Windows defines `WITH_SNDBOARD`, `WITH_TOCCATA`, `WITH_DSP`, `WITH_MIDIEMU`; Unix does not link those sources. | Add native audio/MIDI backends before enabling UI controls. |
| Native MIDI | Deferred | Windows defines `WITH_MIDI`, `WITH_PORTAUDIO`, `WITH_OPENAL` and links `od-win32/midi.cpp`; Unix does not. | Choose CoreMIDI on macOS and ALSA/PipeWire/PortMIDI on Linux. |
| Sampler input | Stubbed | `od-unix/stubs.cpp` implements `sampler_init` and `sampler_getsample` as no-op/zero. | Keep UI disabled until a native capture backend exists. |
| Keyboard and mouse input | Enabled | `od-unix/input.cpp` exposes one Unix keyboard and mouse; `od-unix/video_sdl.cpp` feeds SDL3 key, mouse, wheel, and grab events. | Continue matching Windows shortcuts and runtime grab behavior. |
| Input recording / re-recorder | Partial | The core `inputrecord.cpp` path is linked on Unix and the integrated Qt Output page now exposes runtime play, start/stop re-recording, and save actions using the same core state variables and functions as the Windows Output dialog. | Validate with real recordings and polish file-extension/state-file handling against Windows. |
| Joystick/gamepad input, remapping/test UI | Partial | `od-unix/input.cpp` now exposes SDL3 gamepads and non-gamepad joysticks through `inputdevicefunc_joystick`, maps left stick plus D-pad/hat axes to Amiga directions, and follows the Windows default button/CD32 ordering where possible. Remap/test UI is still disabled. | Test with real controllers on macOS/Linux, add SDL haptics later if useful, then enable remap/test UI once the dialog has a working backend. |
| Tablet/lightpen/raw input | Stubbed/Deferred | `is_tablet`, `input_get_default_lightpen`, and `is_touch_lightpen` return disabled values. Windows uses RawInput/tablet paths. | Native tablet/lightpen support can wait until basic input parity is complete. |

## Networking, Serial, And Ports

| Windows feature area | Unix status | Evidence | Next action |
| --- | --- | --- | --- |
| A2065 Ethernet with built-in SLIRP | Partial | CMake option `WINUAE_UNIX_WITH_SLIRP` defaults on and defines `A2065`, `WITH_SLIRP`, `WITH_BUILTIN_SLIRP`; CMake links `a2065.cpp` and `slirp/*`. | Validate guest driver behavior with a real setup and add smoke coverage for autoconfig. |
| SANA-II `uaenet.device` | Deferred | Windows defines `SANA2`; Unix does not, and CMake does not link `sana2.cpp`. | Enable after A2065/SLIRP is validated or when pcap/tap/tun backend exists. |
| pcap/tap/tun native Ethernet | Deferred | Windows defines `WITH_UAENET_PCAP`; Unix does not. | Add native pcap/tap/tun behind explicit CMake options. |
| `bsdsocket.library` | Deferred | CMake links `bsdsocket.cpp`, but the implementation is inactive because Unix does not define `BSDSOCKET` in `od-unix/sysconfig.h`; `od-unix/stubs.cpp` supplies the fallback interrupt symbols. | Decide whether to define and validate it, or remove it from the Unix source list until ready. |
| Native serial device | Partial | Unix defines `SERIAL_PORT`; `od-unix/serial.cpp` supports POSIX termios devices with CTS/RTS and modem-status handling, and `unix.serial_port` is now saved through the target config path. | Test with a real device. |
| TCP/telnet serial listener | Partial | `od-unix/serial.cpp` supports `TCP:host:port`, `TCP://host:port`, and `/wait` with basic telnet IAC filtering. The Qt UI now offers the Windows-style `TCP://0.0.0.0:1234` listener entries, config round-trip coverage normalizes legacy `serial_port` to `unix.serial_port`, and a manual localhost smoke accepted an `nc` connection. | Automate the socket smoke and consider pseudo-terminal convenience endpoints. |
| Windows serial UDP transport | Deferred | Windows defines `SERIAL_ENET`; Unix `enet_*` serial helpers return empty results. | Keep deferred unless a concrete compatibility need appears. |
| `uaeserial.device` | Deferred | Windows defines and links `UAESERIAL`; Unix does not. | Decide after base serial testing. |
| Parallel port and printer | Stubbed | Unix defines `PARALLEL_PORT`, but `od-unix/parallel.cpp` returns no printer/parallel data; `PARALLEL_DIRECT` is not enabled. | Add CUPS/file-printer backend if printer support matters; direct parallel can remain deferred. |

## Expansion Boards And Advanced Hardware

| Windows feature area | Unix status | Evidence | Next action |
| --- | --- | --- | --- |
| Accelerator/CPU boards, MapROM, board flash | Stubbed | Windows defines `WITH_CPUBOARD` and links `cpuboard.cpp`; Unix stubs all `cpuboard_*` functions in `od-unix/stubs.cpp`. | Link and validate board families one at a time, keeping unsupported UI controls disabled. |
| PPC accelerators and QEMU CPU | Deferred | Windows defines `WITH_PPC`, `WITH_QEMU_CPU` and links `ppc/*`; Unix does not. The PPC IRQ/RAM callbacks referenced by NCR SCSI CPU-board variants are no-op stubs. | Separate future task; not needed for first native 68k parity. |
| PCI, bridgeboards, PCem x86 hardware | Deferred | Windows defines `WITH_PCI`, `WITH_X86` and links `pci.cpp`, `x86.cpp`, and `pcem/*`; Unix does not. | Defer until core/RTG/networking are stable. |
| Additional Ethernet boards: Ariadne II, Hydra, LanRover, X-Surf, X-Surf 100 | Stubbed | `od-unix/stubs.cpp` returns false for each board init. | Enable only with a shared Ethernet backend and UI coverage. |
| Special monitors, BeamRacer, Draco | Deferred | Windows defines `WITH_SPECIALMONITORS`, `WITH_BEAMRACER`, `WITH_DRACO`; Unix does not. | Keep deferred and hide advanced controls until backend exists. |
| Lua scripting, uaenative, tablet.library, RetroPlatform | Deferred | Windows defines `WITH_LUA`, `WITH_UAENATIVE`, `WITH_TABLETLIBRARY`, `RETROPLATFORM`; Unix does not. | Revisit after runtime/UI parity basics. |

## Host Integration And Tooling

| Windows feature area | Unix status | Evidence | Next action |
| --- | --- | --- | --- |
| Threads, timers, memory mapping, dynamic loading | Enabled | CMake links `od-unix/threading.cpp`, `od-unix/time.cpp`, `od-unix/mman.cpp`, and shared `dlopen.cpp`; Unix defines `SUPPORT_THREADS`. | Keep covered by normal boot and filesystem tests. |
| Logging and console | Partial | `od-unix/logging.cpp` logs to stderr/file paths and keeps a rolling in-memory buffer for `save_log()` / savestate log chunks. Console input helpers are still no-ops. | Add log-open/export polish through Qt; debugger console input remains later work. |
| Clipboard | Partial | `od-unix/clipboard.cpp` implements host text paste-to-keyboard through `keybuf_inject` and a first text-only Amiga clipboard-device backend using `pbpaste`/`pbcopy` on macOS and `wl-paste`/`wl-copy`, `xclip`, or `xsel` on Linux. The Qt Clipboard sharing option is enabled for text. | Add bitmap/image clipboard sharing and cleaner event-driven host clipboard notifications. |
| Caps Lock / keyboard LED host state | Partial | `od-unix/input.cpp` tracks SDL Caps/Num/Scroll lock modifier state, mirrors the Windows `target_checkcapslock` behavior for normal keyboard mode, and updates the WinUAE Caps LED. It does not drive native host keyboard LEDs. | Add native keyboard LED output only if compatibility requires it. |
| Config path and Unix path expansion | Enabled/Partial | `od-unix/config.cpp` and Qt expand `~/`, `$VAR`, and `${VAR}`. The Qt Paths page now saves runtime-visible target settings for configuration files, NVRAM, screenshots, videos, save images, rips, data, ROMs, and `statefile_path`; older `unix.ui.*` path keys still load for compatibility. `od-unix/path_expand_test.cpp` covers the core helper, and `od-unix/qt/path_utils_test.cpp` covers the Qt boundary helper used by launcher file/config paths. `~user` is documented as unsupported; relative-path policy is still open. | Define relative-path base directories consistently. |
| Qt integrated configuration UI | Partial | CMake defaults `WINUAE_UNIX_WITH_QT_UI` and `WINUAE_UNIX_WITH_INTEGRATED_QT_UI` on; Qt source lives under `od-unix/qt`. | Continue Windows behavior comparisons for each page/action. |
| macOS app and DMG packaging | Partial | CMake targets `winuae_unix_macos_app`, `winuae_unix_macos_dmg`, and `winuae_unix_macos_release_check` run the packaging scripts, DMG verifier, and packaged Qt app smoke test when Qt is enabled. A macOS 13-targeted private-dependency DMG has been smoke-tested on macOS 13. | Add signing/notarization and repeat clean-machine validation for each supported deployment target. |
| Windows registry, Recycle Bin, SPTI selectors, Win32 display backend selectors | Windows-only | These are Win32 host features with no direct Unix equivalent. | Keep hidden/disabled in Unix UI. |

## Highest-Risk Gaps

1. RTG/Picasso96 is visible in the UI and partially active, but the Unix backend only covers UAE Z2/Z3 RAM and a first `uaegfx.card` path. This needs guest-driver validation before it should be considered Windows-equivalent.
2. A2065/SLIRP is compiled by default, but SANA-II, pcap/tap/tun, and `bsdsocket.library` are not yet validated/enabled as a coherent networking story.
3. Audio output exists through SDL3, but Windows has many more audio/MIDI/board paths. The Unix UI should keep advanced sound/MIDI controls disabled until native backends exist.
4. Host integration stubs are still broad: full clipboard sharing, screenshots/capture, sampler, printer/parallel, archive browsing, native media passthrough, CPU boards, and hardware graphics boards. Joystick/gamepad support has a first SDL3 backend, but still needs real-device validation and remap/test UI wiring.
5. The Unix build defines some user-visible features whose backend is only partial or stubbed, especially `PARALLEL_PORT`, `PICASSO96`, and A2065/SLIRP. These should either become fully backed or be clearly reflected in the UI/status text.
