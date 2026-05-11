#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 dmg-path

Verifies a WinUAE macOS drag-install DMG.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

dmg_path="${1:-}"
mount_dir=""

cleanup() {
    if [[ -n "${mount_dir}" && -d "${mount_dir}" ]]; then
        hdiutil detach "${mount_dir}" -quiet >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: macOS DMG verification requires Darwin/macOS" >&2
    exit 1
fi

if [[ -z "${dmg_path}" ]]; then
    usage >&2
    exit 1
fi

if [[ ! -f "${dmg_path}" ]]; then
    echo "error: DMG not found: ${dmg_path}" >&2
    exit 1
fi

hdiutil verify "${dmg_path}" >/dev/null
mount_dir="$(hdiutil attach "${dmg_path}" -readonly -noverify -noautoopen | awk -F '\t' '/\/Volumes\// { print $NF; exit }')"
if [[ -z "${mount_dir}" || ! -d "${mount_dir}" ]]; then
    echo "error: failed to mount ${dmg_path}" >&2
    exit 1
fi

app_dir="${mount_dir}/WinUAE.app"
info_plist="${app_dir}/Contents/Info.plist"

require_path() {
    local path="$1"
    local description="$2"
    if [[ ! -e "${path}" ]]; then
        echo "error: missing ${description}: ${path}" >&2
        exit 1
    fi
}

require_file() {
    local path="$1"
    local description="$2"
    if [[ ! -f "${path}" ]]; then
        echo "error: missing ${description}: ${path}" >&2
        exit 1
    fi
}

require_path "${app_dir}" "app bundle"
require_file "${info_plist}" "Info.plist"
require_file "${app_dir}/Contents/MacOS/WinUAE" "bundle executable"
if [[ ! -x "${app_dir}/Contents/MacOS/WinUAE" ]]; then
    echo "error: bundle executable is not executable: ${app_dir}/Contents/MacOS/WinUAE" >&2
    exit 1
fi
require_file "${app_dir}/Contents/Resources/WinUAE.icns" "application icon"
require_file "${app_dir}/Contents/Resources/README_unix.md" "bundled README"

if [[ ! -L "${mount_dir}/Applications" ]]; then
    echo "error: missing /Applications symlink" >&2
    exit 1
fi
if [[ "$(readlink "${mount_dir}/Applications")" != "/Applications" ]]; then
    echo "error: Applications symlink does not point to /Applications" >&2
    exit 1
fi

require_file "${mount_dir}/.DS_Store" "Finder layout metadata"
require_file "${mount_dir}/.background/background.png" "Finder background image"
require_file "${mount_dir}/.VolumeIcon.icns" "volume icon"

if command -v GetFileInfo >/dev/null 2>&1; then
    volume_attrs="$(GetFileInfo -a "${mount_dir}" 2>/dev/null || true)"
    case "${volume_attrs}" in
        *C*) ;;
        *)
            echo "error: custom volume icon attribute is not set on ${mount_dir}" >&2
            exit 1
            ;;
    esac
fi

plist_get() {
    /usr/libexec/PlistBuddy -c "Print $1" "${info_plist}" 2>/dev/null || true
}

if [[ "$(plist_get ':CFBundleExecutable')" != "WinUAE" ]]; then
    echo "error: CFBundleExecutable is not WinUAE" >&2
    exit 1
fi
if [[ "$(plist_get ':CFBundleIconFile')" != "WinUAE.icns" ]]; then
    echo "error: CFBundleIconFile is not WinUAE.icns" >&2
    exit 1
fi
if [[ "$(plist_get ':CFBundleDocumentTypes:0:CFBundleTypeExtensions:0')" != "uae" ]]; then
    echo "error: .uae document type is not registered in Info.plist" >&2
    exit 1
fi

echo "verified ${dmg_path}"
