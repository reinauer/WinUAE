# WinUAE Unix Port

This is an early macOS/Linux port of the WinUAE source tree. The current Unix build is a native executable with SDL2 video/input support when SDL2 is available and an integrated Qt configuration UI when Qt Widgets is available.

## Current Status

- Builds with CMake as `winuae_unix`.
- Uses `od-unix/` host abstractions.
- SDL2 provides the current window, framebuffer presentation, mouse input, keyboard input, and audio output.
- Qt Widgets provides an initial Windows-style configuration UI. When Qt is available, it is integrated into `winuae_unix` and the standalone `winuae_unix_qt` launcher is also built.
- A2065 Ethernet can use the built-in SLIRP user-mode NAT backend.
- UAE Zorro II/Zorro III RTG RAM can be configured and autoconfigured, with an initial Unix `uaegfx.card` install path; guest Picasso96 monitor-driver testing and accelerated RTG operations are still incomplete.
- Full UI parity with the Windows configuration dialogs and platform packaging are still incomplete.
- If SDL2 is not found, CMake currently builds a headless/null-video target.

## Requirements

- CMake 3.20 or newer
- C and C++ compiler with C++17 support
- zlib development headers
- pkg-config or pkgconf
- SDL2 development headers and libraries, recommended for a usable windowed emulator
- Qt 6 or Qt 5 Widgets, recommended for the native Unix configuration UI

### macOS

Install Xcode Command Line Tools and Homebrew dependencies:

```sh
xcode-select --install
brew install cmake pkg-config sdl2
```

For the Qt frontend:

```sh
brew install qt
```

The system zlib is normally enough on macOS.

### Debian/Ubuntu

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config zlib1g-dev libsdl2-dev
```

For the Qt frontend:

```sh
sudo apt install qt6-base-dev
```

### Fedora

```sh
sudo dnf install gcc gcc-c++ cmake pkgconf-pkg-config zlib-devel SDL2-devel
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

If Qt Widgets is available, CMake links the Windows-style configuration UI into `winuae_unix` by default and also builds the standalone launcher:

```sh
cmake --build /tmp/winuae_cmake_build --target winuae_unix_qt -j
/tmp/winuae_cmake_build/winuae_unix_qt
```

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

## Smoke Test

The repository includes a headless A1200 smoke test. It uses SDL dummy video/audio by default and checks that ROM loading, audio initialization, and hard reset reached the expected log points:

```sh
export WINUAE_KICKSTART_ROM=/path/to/A1200.47.115.rom
export WINUAE_FLOPPY0=/path/to/Install3.2.adf
tools/unix-smoke-a1200.sh
```

To include A2065 SLIRP autoconfig in the same smoke path:

```sh
tools/unix-smoke-a2065.sh
```

To include Zorro III RTG RAM autoconfig in the same smoke path:

```sh
tools/unix-smoke-rtg-z3.sh
```

To let the boot continue long enough to verify `uaegfx.card` installation:

```sh
tools/unix-smoke-uaegfx.sh
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

## Useful CMake Options

```sh
-DWINUAE_UNIX_BUILD_EXECUTABLE=ON
-DWINUAE_UNIX_WITH_SDL2=ON
-DWINUAE_UNIX_WITH_SLIRP=ON
-DWINUAE_UNIX_WITH_QT_UI=ON
-DWINUAE_UNIX_WITH_INTEGRATED_QT_UI=ON
```

`WINUAE_UNIX_WITH_SDL2` is enabled by default. If SDL2 is not found through pkg-config, the build currently falls back to the null video presenter.
`WINUAE_UNIX_WITH_SLIRP` is enabled by default and builds the bundled SLIRP backend plus A2065 emulation.
`WINUAE_UNIX_WITH_QT_UI` is enabled by default, but the `winuae_unix_qt` target is skipped when Qt Widgets is not installed.
`WINUAE_UNIX_WITH_INTEGRATED_QT_UI` is enabled by default. When Qt Widgets is not installed, the build continues without the integrated UI.
