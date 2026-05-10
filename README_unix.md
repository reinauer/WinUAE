# WinUAE Unix Port

This is an early macOS/Linux port of the WinUAE source tree. The current Unix build is a command-line executable with SDL2 video/input support when SDL2 is available.

## Current Status

- Builds with CMake as `winuae_unix`.
- Uses `od-unix/` host abstractions.
- SDL2 provides the current window, framebuffer presentation, mouse input, and keyboard input.
- Sound, full UI/configuration dialogs, native file pickers, and platform packaging are still incomplete.
- If SDL2 is not found, CMake currently builds a headless/null-video target.

## Requirements

- CMake 3.20 or newer
- C and C++ compiler with C++17 support
- zlib development headers
- pkg-config or pkgconf
- SDL2 development headers and libraries, recommended for a usable windowed emulator

### macOS

Install Xcode Command Line Tools and Homebrew dependencies:

```sh
xcode-select --install
brew install cmake pkg-config sdl2
```

The system zlib is normally enough on macOS.

### Debian/Ubuntu

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config zlib1g-dev libsdl2-dev
```

### Fedora

```sh
sudo dnf install gcc gcc-c++ cmake pkgconf-pkg-config zlib-devel SDL2-devel
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

The executable also tries to load `~/default.uae` by default. It is harmless if that file does not exist when command-line `-s` options provide the needed settings.

## SDL Input

- Click inside the window to grab mouse input.
- Press `Esc` while grabbed to release the mouse.
- Press `Ctrl+G` or `Cmd+G` to release the mouse.
- Press `Ctrl+Q` or `Cmd+Q` to quit.

## Useful CMake Options

```sh
-DWINUAE_UNIX_BUILD_EXECUTABLE=ON
-DWINUAE_UNIX_WITH_SDL2=ON
```

`WINUAE_UNIX_WITH_SDL2` is enabled by default. If SDL2 is not found through pkg-config, the build currently falls back to the null video presenter.
