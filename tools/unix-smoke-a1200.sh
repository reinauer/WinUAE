#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${WINUAE_BUILD_DIR:-/tmp/winuae_cmake_build}
EXE=${WINUAE_EXE:-"$BUILD_DIR/winuae_unix"}
LOG=${WINUAE_SMOKE_LOG:-/tmp/winuae_unix_smoke.log}
RUN_SECONDS=${WINUAE_SMOKE_SECONDS:-5}

ROM=${WINUAE_KICKSTART_ROM:-}
ADF=${WINUAE_FLOPPY0:-}

if [ -z "$ROM" ] || [ -z "$ADF" ]; then
    echo "Set WINUAE_KICKSTART_ROM and WINUAE_FLOPPY0 before running this smoke test." >&2
    exit 2
fi

if [ ! -f "$ROM" ]; then
    echo "Kickstart ROM not found: $ROM" >&2
    exit 2
fi

if [ ! -f "$ADF" ]; then
    echo "ADF image not found: $ADF" >&2
    exit 2
fi

if [ ! -x "$EXE" ]; then
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "$BUILD_DIR" --target winuae_unix
fi

: > "$LOG"

SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy}
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy}
export SDL_AUDIODRIVER SDL_VIDEODRIVER

"$EXE" \
    -s kickstart_rom_file="$ROM" \
    -s floppy0="$ADF" \
    -s nr_floppies=1 \
    -s chipset=aga \
    -s chipset_compatible=A1200 \
    -s cpu_model=68020 \
    -s chipmem_size=4 \
    -s cachesize=0 \
    > "$LOG" 2>&1 &

pid=$!
trap 'kill -INT "$pid" 2>/dev/null || true' INT TERM EXIT

sleep "$RUN_SECONDS"
if kill -0 "$pid" 2>/dev/null; then
    kill -INT "$pid" 2>/dev/null || true
fi
wait "$pid" || true
trap - INT TERM EXIT

grep -q "Known ROM" "$LOG"
grep -q "SDL2: audio initialized" "$LOG"
grep -q "hardreset, memory cleared" "$LOG"
if grep -q "failed to load config" "$LOG" || grep -q "cfgfile_load_2 failed" "$LOG"; then
    echo "Unexpected config load failure in smoke log: $LOG" >&2
    exit 1
fi

echo "Unix smoke test passed. Log: $LOG"
