# WinUAE Unix Port

This is an early macOS/Linux port of the WinUAE source tree. The current Unix build is a native executable with SDL3 video/input support when SDL3 is available and an integrated Qt configuration UI when Qt Widgets is available.

## Current Status

- Builds with CMake as `winuae_unix`.
- Uses `od-unix/` host abstractions.
- SDL3 provides the current window, framebuffer presentation, mouse input, keyboard input, audio output, and playback-device selection.
- SDL3 gamepads and non-gamepad joysticks are exposed through the WinUAE input-device layer for game-port use; the Qt Game Ports/Input pages have first remap/test dialogs backed by SDL3 device enumeration and WinUAE config keys.
- Qt Widgets provides an initial Windows-style configuration UI. When Qt is available, it is integrated into `winuae_unix` and the standalone `winuae_unix_qt` launcher is also built.
- Host clipboard text paste is available through the same paste input event as Windows. The `clipboard_sharing` option has a first native text clipboard-device backend; image clipboard sharing is still incomplete.
- Floppy drive click sound config and sample loading are present, but audible click output still needs follow-up.
- The integrated Qt Output page can toggle the core Sample ripper; ripped WAV files use the configured Rips path. The standalone launcher keeps this runtime action disabled.
- The integrated Qt Output page can run Pro Wizard when the default `WINUAE_UNIX_WITH_PROWIZARD` build option is enabled. Save prompts use Qt warning dialogs with the same OK/Yes/No/Cancel return contract as Windows.
- When opened during emulation, the integrated Qt Output page can play, start/stop, and save core input re-recordings. The standalone launcher keeps these runtime actions disabled.
- The Qt Paths page now writes real Unix target path settings for configuration files, NVRAM, screenshots, videos, save images, rips, data, and ROMs, so runtime helpers use the configured directories. Older `unix.ui.*` path keys are still read for compatibility.
- Native Unix serial support is available for POSIX serial devices and TCP listener endpoints.
- A2065 Ethernet can use the built-in SLIRP user-mode NAT backend.
- UAE Zorro II/Zorro III RTG RAM can be configured and autoconfigured, with an initial Unix `uaegfx.card` install path; guest Picasso96 monitor-driver testing and accelerated RTG operations are still incomplete.
- The Qt Expansions page can enable common Zorro/expansion board ROM entries using the same `*_rom_file` and `*_rom_options` keys as WinUAE.
- Full UI parity with the Windows configuration dialogs and platform packaging are still incomplete.
- If SDL3 is not found, CMake currently builds a headless/null-video target.

See `UNIX_RUNTIME_PARITY.md` for the current Windows-vs-Unix runtime feature matrix.

## Requirements

- CMake 3.20 or newer
- C and C++ compiler with C++17 support
- zlib development headers
- pkg-config or pkgconf
- SDL3 development headers and libraries, recommended for a usable windowed emulator
- Qt 6 or Qt 5 Widgets, recommended for the native Unix configuration UI

### macOS

Install Xcode Command Line Tools and Homebrew dependencies:

```sh
xcode-select --install
brew install cmake pkg-config sdl3
```

For the Qt frontend:

```sh
brew install qt
```

The system zlib is normally enough on macOS.

The macOS build defaults to `CMAKE_OSX_DEPLOYMENT_TARGET=13.0` so the app is not accidentally tied to the build machine's current macOS release. Bundled libraries and frameworks must support the same or an older deployment target. The packaging script checks every bundled Mach-O file and fails if, for example, Homebrew SDL3 was built with a newer `minos` than the app target. The current Homebrew Qt on this machine reports `minos 14.0`, so packaged Qt builds made with it require macOS 14 or newer unless you use the private dependency build below.

For repeatable release builds, build SDL3, QtBase, and optionally libpng into a private prefix with the same deployment target instead of using Homebrew bottles. The helper defaults QtBase to bundled third-party libraries so Homebrew dylibs with newer deployment targets are not pulled into the release app. Supplying `WINUAE_LIBPNG_SOURCE` gives the Unix screenshot backend a deployment-target-compatible PNG library; without it, CMake may skip a too-new Homebrew libpng and fall back to BMP screenshots:

```sh
WINUAE_MACOS_DEPLOYMENT_TARGET=13.0 \
WINUAE_SDL3_SOURCE=/path/to/SDL3-source \
WINUAE_QT_SOURCE=/path/to/qtbase-source \
WINUAE_LIBPNG_SOURCE=/path/to/libpng-source \
tools/macos-build-deps.sh /opt/winuae-macos-13

source /opt/winuae-macos-13/winuae-macos-deps-env.sh
cmake -S . -B /tmp/winuae_cmake_build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_PREFIX_PATH=/opt/winuae-macos-13
```

The same helper is available as a CMake target after configure; pass source paths through the environment:

```sh
WINUAE_SDL3_SOURCE=/path/to/SDL3-source \
WINUAE_QT_SOURCE=/path/to/qtbase-source \
WINUAE_LIBPNG_SOURCE=/path/to/libpng-source \
cmake --build /tmp/winuae_cmake_build --target winuae_unix_macos_deps
```

To target a different macOS release, configure with an explicit deployment target and build/bundle SDL3 and Qt with a matching target:

```sh
cmake -S . -B /tmp/winuae_cmake_build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
```

### Debian/Ubuntu

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config zlib1g-dev libsdl3-dev
```

For the Qt frontend:

```sh
sudo apt install qt6-base-dev
```

### Fedora

```sh
sudo dnf install gcc gcc-c++ cmake pkgconf-pkg-config zlib-devel SDL3-devel
```

For the Qt frontend:

```sh
sudo dnf install qt6-qtbase-devel
```

## Build

From the `WinUAE/` directory:

```sh
cmake -S . -B /tmp/winuae_cmake_build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/winuae_cmake_build --target winuae_unix -j
```

The executable will be:

```sh
/tmp/winuae_cmake_build/winuae_unix
```

On Linux, install the executable, shared resources, documentation, and desktop/MIME metadata into a prefix with:

```sh
cmake --install /tmp/winuae_cmake_build --prefix /opt/winuae
```

This installs the desktop entry, `.uae` MIME type, and hicolor icons. On macOS, use the `.app` and DMG targets below instead of installing the raw executable.

On Linux, CPack can create packages from the same install rules:

```sh
cmake --build /tmp/winuae_cmake_build --target package
```

This currently produces a `.tar.gz` package and, when Debian packaging tools are available, a `.deb` package with shared-library dependencies inferred by `dpkg-shlibdeps`.

If Qt Widgets is available, CMake links the Windows-style configuration UI into `winuae_unix` by default and also builds the standalone launcher:

```sh
cmake --build /tmp/winuae_cmake_build --target winuae_unix_qt -j
/tmp/winuae_cmake_build/winuae_unix_qt
```

On macOS, a local `WinUAE.app` bundle can be created from the same build:

```sh
cmake --build /tmp/winuae_cmake_build --target winuae_unix_macos_app -j
```

The app bundle is written to:

```sh
/tmp/winuae_cmake_build/package/WinUAE.app
```

The bundling script copies the Qt UI resources and runs `macdeployqt` when it is available. It does not bundle Kickstart ROMs, disks, or other Amiga system media.

For a GUI launch smoke test of the packaged app, run:

```sh
cmake --build /tmp/winuae_cmake_build --target winuae_unix_macos_app_smoke
```

This starts `WinUAE.app` from an isolated temporary home directory and uses an opt-in smoke-test environment variable to verify that the Qt configuration window reaches a visible state.

To create a drag-install DMG with `WinUAE.app`, an `/Applications` link, a volume icon, and a Finder background arrow:

```sh
cmake --build /tmp/winuae_cmake_build --target winuae_unix_macos_dmg -j
```

The DMG is written next to the app bundle in `/tmp/winuae_cmake_build/package/`.
The DMG target verifies the generated image by mounting it, checking the app bundle, `/Applications` link, Finder layout metadata, volume icon, background image, and `.uae` document declaration, then running the bundled executable with `-h` from an isolated temporary home directory. You can rerun the verification directly:

```sh
tools/macos-verify-dmg.sh /tmp/winuae_cmake_build/package/WinUAE-6.1.0.dmg
```

For a single local release gate, build:

```sh
cmake --build /tmp/winuae_cmake_build --target winuae_unix_macos_release_check -j
```

This produces and verifies the DMG, then launches the packaged Qt app from an isolated temporary home directory when the integrated Qt UI is enabled. A macOS 13-targeted private-dependency DMG has been smoke-tested on macOS 13; keep using the private dependency prefix path for release artifacts instead of newer Homebrew Qt/SDL bottles.

To force a configure from scratch:

```sh
rm -rf /tmp/winuae_cmake_build
cmake -S . -B /tmp/winuae_cmake_build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/winuae_cmake_build --target winuae_unix -j
```

## Run

The port accepts normal WinUAE command-line `-s` configuration overrides. A minimal A1200 example:

```sh
/tmp/winuae_cmake_build/winuae_unix \
  -s kickstart_rom_file=/path/to/A1200.rom \
  -s floppy0=/path/to/disk.adf \
  -s nr_floppies=1 \
  -s chipset=aga \
  -s chipset_compatible=A1200 \
  -s cpu_model=68020 \
  -s chipmem_size=4 \
  -s cachesize=0
```

When the integrated Qt UI is built, `winuae_unix` opens the configuration UI by default. To boot directly from a config or command-line settings, disable the GUI:

```sh
/tmp/winuae_cmake_build/winuae_unix \
  -f /path/to/config.uae \
  -s use_gui=no
```

The executable also tries to load `~/default.uae` by default. A missing default config is ignored silently; explicit `-config` / `-f` load failures are still reported.

Unix path expansion is supported for `~/`, `$VAR`, and `${VAR}` in core config paths and Qt file/config boundaries. Relative config and media paths are resolved against the process working directory, matching the non-relative Windows save mode; the Windows-style relative-path save option is not enabled on Unix yet. The Qt Paths page saves runtime-visible target path settings such as `unix.screenshot_path`, `unix.rip_path`, `unix.video_path`, and `statefile_path`; legacy `unix.ui.*` path settings are still accepted when loading older configs. `~user` expansion is not implemented; use an absolute path for another user's home directory.

There is also a minimal example config at:

```sh
configs/unix-a1200-install32.uae.example
```

For A2065 SLIRP networking, use:

```sh
configs/unix-a1200-install32-a2065.uae.example
```

For Zorro III RTG RAM autoconfig bring-up testing, use:

```sh
configs/unix-a1200-install32-rtg-z3.uae.example
```

Copy an example to a writable location and replace the ROM and ADF paths before using it with `-config` or `-f`.

## Audio

SDL3 audio uses the WinUAE `sound_output`, `sound_frequency`, `sound_channels`, `sound_volume*`, and floppy drive sound config keys. Unix playback device selection follows the same target-prefixed style as Windows:

```sh
/tmp/winuae_cmake_build/winuae_unix \
  -s unix.soundcard=0 \
  -s 'unix.soundcardname=SDL:Default Audio Device'
```

The Qt Sound page lists SDL playback devices and writes both `unix.soundcard` and `unix.soundcardname` so saved configs can recover by name if the device index changes. Floppy click sounds are off by default, matching the shared WinUAE defaults; enable them per drive with `floppy0sound=1` through `floppy3sound=1`.

## Clipboard

Host clipboard text can be pasted into the emulated keyboard through `SPC_PASTE`. The default keyboard mapping follows Windows: the special qualifier plus Insert triggers paste. On macOS the Unix backend reads text with `pbpaste`; on Linux it tries `wl-paste`, `xclip`, then `xsel` if available.

Transparent Amiga clipboard-device sharing can also be enabled with `clipboard_sharing=true`. The first Unix backend supports text in both directions using `pbpaste`/`pbcopy` on macOS or `wl-paste`/`wl-copy`, `xclip`, or `xsel` on Linux. Bitmap/image clipboard sharing remains deferred.

## Serial

The Unix serial backend follows the same target-prefixed config style as Windows. Use `unix.serial_port` for command-line overrides and saved configs:

```sh
# Real serial device
/tmp/winuae_cmake_build/winuae_unix \
  -s unix.serial_port=/dev/cu.usbserial-0001 \
  -s serial_hardware_ctsrts=true

# Telnet-style TCP listener on all local interfaces, port 1234
/tmp/winuae_cmake_build/winuae_unix \
  -s unix.serial_port=TCP://0.0.0.0:1234
```

`TCP:host:port`, `TCP://host:port`, and `TCP:port` are accepted. Add `/wait` to delay startup until a client connects, for example `TCP://0.0.0.0:1234/wait`. Connect locally with `telnet 127.0.0.1 1234` or `nc 127.0.0.1 1234`.

## Smoke Test

The repository includes a headless A1200 smoke test. It uses SDL dummy video/audio by default and checks that ROM loading, audio initialization, and hard reset reached the expected log points:

```sh
export WINUAE_KICKSTART_ROM=/path/to/A1200.47.115.rom
export WINUAE_FLOPPY0=/path/to/Install3.2.adf
tools/unix-smoke-a1200.sh
```

The same scripts are also available as CMake targets. The targets set `WINUAE_BUILD_DIR` to the active build directory and still read the same ROM/disk environment variables:

```sh
cmake --build "$WINUAE_BUILD_DIR" --target winuae_unix_smoke_a1200
cmake --build "$WINUAE_BUILD_DIR" --target winuae_unix_smoke_path_config
```

`winuae_unix_smoke_basic` runs the A1200 boot smoke and the path/config smoke.

The host-side unit tests can be built and run through one CMake target:

```sh
cmake --build "$WINUAE_BUILD_DIR" --target winuae_unix_check
```

`winuae_unix_tests` only builds the test executables. `ctest --output-on-failure` can be run directly from the build directory after that.

For the Unix host semaphore/event primitive test:

```sh
cmake --build "$WINUAE_BUILD_DIR" --target winuae_unix_threading_test
"$WINUAE_BUILD_DIR/winuae_unix_threading_test"
```

To include A2065 SLIRP autoconfig in the same smoke path:

```sh
tools/unix-smoke-a2065.sh
```

To include Zorro III RTG RAM autoconfig in the same smoke path:

```sh
tools/unix-smoke-rtg-z3.sh
```

For manual A4091 autoconfig smoke tests, use an A4000/A4000T-style config, disable 24-bit CPU addressing, and provide a real A4091 ROM:

```sh
/tmp/winuae_cmake_build/winuae_unix \
  -s use_gui=no \
  -s kickstart_rom_file=/path/to/A4000.rom \
  -s a4091_rom_file=/path/to/a4091.rom \
  -s chipset=aga \
  -s chipset_compatible=A4000 \
  -s cpu_model=68030 \
  -s cpu_24bit_addressing=false
```

For image-backed hardfiles on the A4091, choose `A4091 (SCSI)` in the Qt hardfile dialog or use a hardfile tail such as `scsi0_a4091`. A manual A4091 ROM plus HDF boot has been validated; other NCR/NCR9x boards still need real-ROM validation.

To automate the A4091 HDF smoke path:

```sh
export WINUAE_A4000_KICKSTART_ROM=/path/to/A4000.rom
export WINUAE_A4091_ROM=/path/to/a4091.rom
export WINUAE_A4091_HDF=/path/to/disk.hdf
tools/unix-smoke-a4091-hdf.sh
```

RIPPLE and AlfaPower/AT-Bus 2008 can be enabled without an external ROM path. In the Qt UI, enable the board on the Expansions page first, then choose `RIPPLE (IDE)` or `AlfaPower/AT-Bus 2008 (IDE)` in the hardfile dialog. The matching config tails are `ide0_ripple` and `ide0_alfapower`.

The IDE expansion smoke targets create a temporary blank HDF if `WINUAE_IDE_EXPANSION_HDF` or `WINUAE_HARDFILE0` is not set:

```sh
export WINUAE_A1200_KICKSTART_ROM=/path/to/A1200.rom
tools/unix-smoke-alfapower-hdf.sh
tools/unix-smoke-ripple-hdf.sh
```

To automate the TCP serial listener path, including the Windows-style `/wait` startup behavior:

```sh
export WINUAE_KICKSTART_ROM=/path/to/A1200.47.115.rom
export WINUAE_FLOPPY0=/path/to/Install3.2.adf
tools/unix-smoke-serial-tcp.sh
```

The default port is `51234`. Override it with `WINUAE_SERIAL_TCP_PORT` if that port is already in use.

To exercise Unix path expansion through a real config file and command-line override:

```sh
export WINUAE_KICKSTART_ROM=/path/to/A1200.47.115.rom
export WINUAE_FLOPPY0=/path/to/Install3.2.adf
tools/unix-smoke-path-config.sh
```

To let the boot continue long enough to verify Zorro III RTG RAM autoconfig and `uaegfx.card` installation:

```sh
tools/unix-smoke-uaegfx.sh
```

That smoke verifies the current Unix install-level RTG path: Z3 RTG RAM is mapped, `uaegfx.card` is installed, and P96 resolution memory is allocated. It does not prove a guest Picasso96 monitor driver has opened the board or switched to an RTG screen. For a Workbench/Picasso96 setup that should exercise those paths, enable stricter log checks and append any config or override arguments needed by that setup:

```sh
export WINUAE_SMOKE_UAEGFX_DRIVER=1
export WINUAE_SMOKE_UAEGFX_SCREEN=1
tools/unix-smoke-uaegfx.sh -f /path/to/p96-workbench.uae
```

Optional overrides:

```sh
export WINUAE_BUILD_DIR=/tmp/winuae_cmake_build
export WINUAE_EXE=/tmp/winuae_cmake_build/winuae_unix
export WINUAE_SMOKE_SECONDS=5
export WINUAE_SMOKE_LOG=/tmp/winuae_unix_smoke.log
```

## SDL Input

- Click inside the window to grab mouse input.
- Press `Esc` while grabbed to release the mouse.
- Press `Ctrl+G` or `Cmd+G` to release the mouse.
- Press `Ctrl+Q` or `Cmd+Q` to quit.
- SDL3 gamepads use the standard SDL layout: left stick and D-pad map to joystick directions, South/East/West/North map to the first four buttons, and CD32 mode follows the Windows default button order where possible.
- Non-gamepad SDL joysticks expose their native axes, hats, and buttons; hats also map to joystick directions by default.
- Press `F12` to open the integrated Qt settings UI during emulation.
- The screenshot file input event and integrated runtime Qt Output-page button save under the configured Screenshots directory. Unix uses PNG when libpng is found at configure time and falls back to BMP otherwise. On macOS, libpng must also pass the configured deployment-target check; a newer Homebrew libpng is skipped so release builds stay compatible with the selected minimum macOS. The standalone Qt launcher keeps this runtime-only button disabled. Clipboard screenshots, palette options, autoclip, and continuous screenshots are still pending Unix parity work.
- Hold `End` and press `F1`-`F4` to change DF0:-DF3:, `F5` to change the CD image, and `F6` to restore a state. Hold `Shift` with those shortcuts to eject the matching floppy/CD image or save state, matching the Windows key map.
- On MacBook or compact Apple keyboards, `End` is usually `Fn`/Globe + `Right Arrow`. Depending on macOS keyboard settings, function keys may also need `Fn`/Globe, so the MacBook form is `Fn`/Globe + `Right Arrow`, then `F1`-`F6` or `Shift` + `F1`-`F6`. Enabling "Use F1, F2, etc. keys as standard function keys" in macOS makes these closer to the Windows chords.
- The SDL status strip at the bottom of the emulation window mirrors the Windows basics: left-click DF0:-DF3: or CD to choose media, right-click them to eject, left-click the power area for settings, right-click it for soft reset, and left-click the paused FPS area to resume.

## Useful CMake Options

```sh
-DWINUAE_UNIX_BUILD_EXECUTABLE=ON
-DWINUAE_UNIX_WITH_SDL3=ON
-DWINUAE_UNIX_WITH_SLIRP=ON
-DWINUAE_UNIX_WITH_NCR_SCSI=ON
-DWINUAE_UNIX_WITH_PROWIZARD=ON
-DWINUAE_UNIX_WITH_QT_UI=ON
-DWINUAE_UNIX_WITH_INTEGRATED_QT_UI=ON
```

`WINUAE_UNIX_WITH_SDL3` is enabled by default. If SDL3 is not found through CMake package discovery or pkg-config, the build currently falls back to the null video presenter.
`WINUAE_UNIX_WITH_SLIRP` is enabled by default and builds the bundled SLIRP backend plus A2065 emulation.
`WINUAE_UNIX_WITH_NCR_SCSI` is enabled by default and builds the NCR/NCR9x SCSI controller emulation used by boards such as A4091. ROM-backed controller boards still need a valid board ROM path in the config.
`WINUAE_UNIX_WITH_PROWIZARD` is enabled by default and builds the same Pro Wizard source set used by the Windows project.
`WINUAE_UNIX_WITH_QT_UI` is enabled by default, but the `winuae_unix_qt` target is skipped when Qt Widgets is not installed.
`WINUAE_UNIX_WITH_INTEGRATED_QT_UI` is enabled by default. When Qt Widgets is not installed, the build continues without the integrated UI.
