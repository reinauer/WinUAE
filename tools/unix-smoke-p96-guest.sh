#!/usr/bin/env bash
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${WINUAE_BUILD_DIR:-/tmp/winuae_cmake_build}
EXE=${WINUAE_EXE:-"$BUILD_DIR/winuae_unix"}
LOG=${WINUAE_SMOKE_LOG:-/tmp/winuae_unix_p96_guest_smoke.log}
RUN_SECONDS=${WINUAE_SMOKE_SECONDS:-45}
Z3=${WINUAE_P96_Z3:-0}
USER_ARGS=("$@")

if [ "$Z3" = "1" ]; then
    ROM=${WINUAE_A4000_KICKSTART_ROM:-${WINUAE_KICKSTART_ROM:-}}
else
    ROM=${WINUAE_KICKSTART_ROM:-}
fi
WORKBENCH=${WINUAE_P96_WORKBENCH_DIR:-}
SCREENMODE=${WINUAE_P96_SCREENMODE:-}
EXPECT_MODE_MASK=${WINUAE_P96_EXPECT_MODE_MASK:-}
EXPECT_RESINFO_BYTES=${WINUAE_P96_EXPECT_RESINFO_BYTES:-}

if [ -z "$ROM" ] || [ -z "$WORKBENCH" ]; then
    echo "Set WINUAE_KICKSTART_ROM and WINUAE_P96_WORKBENCH_DIR before running this smoke test." >&2
    exit 2
fi

if [ ! -f "$ROM" ]; then
    echo "Kickstart ROM not found: $ROM" >&2
    exit 2
fi

if [ ! -d "$WORKBENCH" ]; then
    echo "P96 Workbench directory not found: $WORKBENCH" >&2
    exit 2
fi

if [ ! -f "$WORKBENCH/Devs/Monitors/uaegfx" ] || [ ! -f "$WORKBENCH/Libs/Picasso96API.library" ]; then
    echo "P96 Workbench directory must contain Devs/Monitors/uaegfx and Libs/Picasso96API.library: $WORKBENCH" >&2
    exit 2
fi

if [ ! -x "$EXE" ]; then
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "$BUILD_DIR" --target winuae_unix
fi

WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/winuae_p96_guest.XXXXXX")
pid=
trap 'kill -INT "$pid" 2>/dev/null || true; rm -rf "$WORKDIR"' INT TERM EXIT

cp -R "$WORKBENCH" "$WORKDIR/Workbench"

write_screenmode_prefs() {
    mode=$1
    file=$2
    case "$mode" in
        640x480x8)
            display_id='\120\003\020\000'
            depth='\000\010'
            ;;
        800x600x8)
            display_id='\120\004\020\000'
            depth='\000\010'
            ;;
        1024x768x8)
            display_id='\120\005\020\000'
            depth='\000\010'
            ;;
        640x480x15)
            display_id='\120\003\020\000'
            depth='\000\017'
            ;;
        800x600x15)
            display_id='\120\004\020\000'
            depth='\000\017'
            ;;
        1024x768x15)
            display_id='\120\005\020\000'
            depth='\000\017'
            ;;
        640x480x16)
            display_id='\120\003\020\000'
            depth='\000\020'
            ;;
        800x600x16)
            display_id='\120\004\020\000'
            depth='\000\020'
            ;;
        1024x768x16)
            display_id='\120\005\020\000'
            depth='\000\020'
            ;;
        *)
            echo "Unsupported WINUAE_P96_SCREENMODE: $mode" >&2
            exit 2
            ;;
    esac

    mkdir -p "$(dirname "$file")"
    {
        printf 'FORM\000\000\000\066PREFPRHD\000\000\000\006\000\000\000\000\000\000'
        printf 'SCRM\000\000\000\034'
        printf '\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000'
        printf "$display_id"
        printf '\377\377\377\377'
        printf "$depth"
        printf '\000\001'
    } > "$file"
}

if [ -n "$SCREENMODE" ]; then
    write_screenmode_prefs "$SCREENMODE" "$WORKDIR/Workbench/Prefs/Env-Archive/Sys/ScreenMode.prefs"
    mkdir -p "$WORKDIR/Workbench/Prefs/Env-Archive/Picasso96"
    printf 'All' > "$WORKDIR/Workbench/Prefs/Env-Archive/Picasso96/ShowModes"
fi

cat > "$WORKDIR/Workbench/S/Startup-Sequence" <<'EOF'
C:MakeDir RAM:T RAM:ENV RAM:ENV/Sys RAM:Clipboards
C:Copy >NIL: ENVARC: RAM:ENV ALL NOREQ
Assign ENV: RAM:ENV
Assign T: RAM:T
Assign CLIPS: RAM:Clipboards
Assign LIBS: SYS:Classes ADD
Echo "before uaegfx smoke" >DH0:unix-p96-before-uaegfx-smoke
DEVS:Monitors/uaegfx
C:IPrefs
C:LoadWB
Echo "after uaegfx smoke" >DH0:unix-p96-after-uaegfx-smoke
Wait 20
EndCLI >NIL:
EOF

: > "$LOG"

SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-dummy}
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy}
export SDL_AUDIODRIVER SDL_VIDEODRIVER

set -- \
    -s use_gui=no \
    -s kickstart_rom_file="$ROM" \
    -s filesystem2="rw,DH0:System:$WORKDIR/Workbench,10" \
    -s nr_floppies=0 \
    -s chipset=aga \
    -s cachesize=0

if [ "$Z3" = "1" ]; then
    set -- "$@" \
        -s chipset_compatible=A4000 \
        -s cpu_model=68040 \
        -s fpu_model=68040 \
        -s cpu_24bit_addressing=false \
        -s chipmem_size=4 \
        -s a3000mem_size=8 \
        -s gfxcard_size=16 \
        -s gfxcard_type=ZorroIII
else
    set -- "$@" \
        -s chipset_compatible=A1200 \
        -s cpu_model=68020 \
        -s chipmem_size=2 \
        -s fastmem_size=4 \
        -s gfxcard_size=4 \
        -s gfxcard_type=ZorroII
fi

"$EXE" "$@" "${USER_ARGS[@]}" > "$LOG" 2>&1 &

pid=$!

sleep "$RUN_SECONDS"
if kill -0 "$pid" 2>/dev/null; then
    kill -INT "$pid" 2>/dev/null || true
fi
wait "$pid" || true
pid=
trap 'rm -rf "$WORKDIR"' EXIT

test -f "$WORKDIR/Workbench/unix-p96-before-uaegfx-smoke"
test -f "$WORKDIR/Workbench/unix-p96-after-uaegfx-smoke"
grep -q "Known ROM" "$LOG"
grep -q "FS: mounted virtual unit DH0" "$LOG"
grep -q "Unix uaegfx.card" "$LOG"
grep -q "Unix RTG P96 RESINFO" "$LOG"
grep -q "Unix RTG FindCard:" "$LOG"
grep -q "Unix RTG InitCard:" "$LOG"
grep -q "Unix RTG SetGC:" "$LOG"
grep -q "Unix RTG SetPanning:" "$LOG"
grep -q "Unix RTG SetSwitch:" "$LOG"
if [ -n "$EXPECT_MODE_MASK" ]; then
    grep -qi "Unix RTG mode mask: $EXPECT_MODE_MASK" "$LOG"
fi
if [ -n "$EXPECT_RESINFO_BYTES" ]; then
    grep -Eq "Unix RTG P96 RESINFO: .*\($EXPECT_RESINFO_BYTES bytes\)" "$LOG"
fi
if [ -n "$SCREENMODE" ]; then
    grep -q "Unix RTG SetGC: $SCREENMODE" "$LOG"
fi
if grep -q "not executable" "$LOG" || grep -q "failed to load config" "$LOG" || grep -q "cfgfile_load_2 failed" "$LOG"; then
    echo "Unexpected failure in P96 guest smoke log: $LOG" >&2
    exit 1
fi

echo "Unix P96 guest smoke test passed. Log: $LOG"
