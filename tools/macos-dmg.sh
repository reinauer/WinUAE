#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [build-dir] [output-dir]

Creates a drag-install WinUAE DMG from an existing macOS build tree.

Arguments:
  build-dir   CMake build directory containing winuae_unix.
              Defaults to WINUAE_BUILD_DIR or the current directory.
  output-dir  Directory that will receive WinUAE.app and the final DMG.
              Defaults to <build-dir>/package.

Environment:
  WINUAE_SKIP_FINDER_LAYOUT=1  Skip Finder window layout customization.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$(cd "${script_dir}/.." && pwd)"
build_dir="${1:-${WINUAE_BUILD_DIR:-$(pwd)}}"
output_dir="${2:-${build_dir}/package}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: macOS DMG creation requires Darwin/macOS" >&2
    exit 1
fi

major="$(awk '/^#define UAEMAJOR / { print $3; exit }' "${source_dir}/include/options.h")"
minor="$(awk '/^#define UAEMINOR / { print $3; exit }' "${source_dir}/include/options.h")"
revision="$(awk '/^#define UAESUBREV / { print $3; exit }' "${source_dir}/include/options.h")"
version="${major:-0}.${minor:-0}.${revision:-0}"

app_dir="$("${script_dir}/macos-bundle.sh" "${build_dir}" "${output_dir}")"
staging_dir="${output_dir}/dmg-root"
volume_name="WinUAE"
rw_dmg="${output_dir}/WinUAE-${version}.rw.dmg"
final_dmg="${output_dir}/WinUAE-${version}.dmg"
mount_dir=""

cleanup() {
    if [[ -n "${mount_dir}" && -d "${mount_dir}" ]]; then
        hdiutil detach "${mount_dir}" -quiet >/dev/null 2>&1 || true
        rmdir "${mount_dir}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

rm -rf "${staging_dir}" "${rw_dmg}" "${final_dmg}"
mkdir -p "${staging_dir}/.background"
cp -R "${app_dir}" "${staging_dir}/WinUAE.app"
ln -s /Applications "${staging_dir}/Applications"

volume_icon="${app_dir}/Contents/Resources/WinUAE.icns"
apply_volume_icon() {
    local target_dir="$1"

    if [[ ! -f "${volume_icon}" ]]; then
        return
    fi

    cp "${volume_icon}" "${target_dir}/.VolumeIcon.icns"
    if command -v SetFile >/dev/null 2>&1; then
        SetFile -a C "${target_dir}" || true
        SetFile -a V "${target_dir}/.VolumeIcon.icns" || true
    fi
}

apply_volume_icon "${staging_dir}"

background_ppm="${staging_dir}/.background/background.ppm"
background_png="${staging_dir}/.background/background.png"
awk '
BEGIN {
    w = 640; h = 400;
    print "P3";
    print w " " h;
    print 255;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            r = 246; g = 248; b = 250;
            dx1 = x - 178; dy1 = y - 200;
            dx2 = x - 462; dy2 = y - 200;
            if (dx1 * dx1 + dy1 * dy1 < 74 * 74 || dx2 * dx2 + dy2 * dy2 < 74 * 74) {
                r = 238; g = 241; b = 245;
            }
            if (x >= 264 && x <= 374 && y >= 195 && y <= 205) {
                r = 78; g = 86; b = 96;
            }
            if (x >= 370 && x <= 398 && (y - 200 <= (398 - x) / 2) && (200 - y <= (398 - x) / 2)) {
                r = 78; g = 86; b = 96;
            }
            printf "%d %d %d ", r, g, b;
        }
        print "";
    }
}' > "${background_ppm}"
sips -s format png "${background_ppm}" --out "${background_png}" >/dev/null
rm -f "${background_ppm}"

hdiutil create -volname "${volume_name}" -srcfolder "${staging_dir}" -fs HFS+ -format UDRW -ov "${rw_dmg}" >/dev/null
mount_dir="$(hdiutil attach "${rw_dmg}" -readwrite -noverify -noautoopen | awk -F '\t' '/\/Volumes\// { print $NF; exit }')"
if [[ -z "${mount_dir}" || ! -d "${mount_dir}" ]]; then
    echo "error: failed to mount ${rw_dmg}" >&2
    exit 1
fi

apply_volume_icon "${mount_dir}"

if [[ "${WINUAE_SKIP_FINDER_LAYOUT:-0}" != "1" ]] && command -v osascript >/dev/null 2>&1; then
    osascript <<EOF
tell application "Finder"
    set dmgFolder to POSIX file "${mount_dir}" as alias
    set backgroundPicture to POSIX file "${mount_dir}/.background/background.png" as alias
    open dmgFolder
    delay 1
    set dmgWindow to container window of dmgFolder
    set current view of dmgWindow to icon view
    set toolbar visible of dmgWindow to false
    set statusbar visible of dmgWindow to false
    set bounds of dmgWindow to {100, 100, 740, 500}
    set viewOptions to icon view options of dmgWindow
    set arrangement of viewOptions to not arranged
    set icon size of viewOptions to 96
    set background picture of viewOptions to backgroundPicture
    set position of item "WinUAE.app" of dmgFolder to {178, 200}
    set position of item "Applications" of dmgFolder to {462, 200}
    update dmgFolder
    delay 1
    close dmgWindow
end tell
EOF
    for _ in {1..20}; do
        if [[ -f "${mount_dir}/.DS_Store" ]]; then
            break
        fi
        sleep 0.5
    done
    if [[ ! -f "${mount_dir}/.DS_Store" ]]; then
        echo "error: Finder did not write ${mount_dir}/.DS_Store; DMG background layout was not saved" >&2
        exit 1
    fi
fi

apply_volume_icon "${mount_dir}"
if [[ -f "${volume_icon}" ]] && command -v SetFile >/dev/null 2>&1 && command -v GetFileInfo >/dev/null 2>&1; then
    volume_attrs="$(GetFileInfo -a "${mount_dir}" 2>/dev/null || true)"
    case "${volume_attrs}" in
        *C*) ;;
        *)
            echo "error: custom volume icon attribute was not set on ${mount_dir}" >&2
            exit 1
            ;;
    esac
fi

sync
hdiutil detach "${mount_dir}" -quiet
mount_dir=""
hdiutil convert "${rw_dmg}" -format UDZO -imagekey zlib-level=9 -o "${final_dmg}" -ov >/dev/null
hdiutil verify "${final_dmg}" >/dev/null
"${script_dir}/macos-verify-dmg.sh" "${final_dmg}" >/dev/null
rm -rf "${rw_dmg}" "${staging_dir}"

echo "${final_dmg}"
