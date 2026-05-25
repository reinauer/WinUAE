#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [qemu-uae-source] [output-plugin]

Builds the QEMU-UAE plugin from the sibling qemu-uae-v11.0 tree.

Environment:
  WINUAE_MACOS_DEPLOYMENT_TARGET  macOS deployment target for the plugin.
  QEMU_UAE_NINJA                  ninja executable. Defaults to ninja in PATH.
  QEMU_UAE_SKIP_CONFIGURE=1       Skip configure-qemu-uae. By default,
                                  configure only runs when build/build.ninja
                                  is missing.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$(cd "${script_dir}/.." && pwd)"
qemu_source="${1:-${WINUAE_QEMU_UAE_SOURCE_DIR:-${source_dir}/../qemu-uae-v11.0}}"
output_plugin="${2:-${WINUAE_QEMU_UAE_OUTPUT_PLUGIN:-}}"

if [[ ! -x "${qemu_source}/configure-qemu-uae" ]]; then
    echo "error: configure-qemu-uae not found in ${qemu_source}" >&2
    exit 1
fi
qemu_source="$(cd "${qemu_source}" && pwd)"

if [[ "$(uname -s)" == "Darwin" ]]; then
    export MACOSX_DEPLOYMENT_TARGET="${WINUAE_MACOS_DEPLOYMENT_TARGET:-${MACOSX_DEPLOYMENT_TARGET:-13.0}}"
fi

ninja_executable="${QEMU_UAE_NINJA:-$(command -v ninja || true)}"
if [[ -z "${ninja_executable}" && -x "${qemu_source}/build/pyvenv/bin/ninja" ]]; then
    ninja_executable="${qemu_source}/build/pyvenv/bin/ninja"
fi
if [[ -z "${ninja_executable}" ]]; then
    echo "error: ninja not found; set QEMU_UAE_NINJA" >&2
    exit 1
fi

if [[ "${QEMU_UAE_SKIP_CONFIGURE:-0}" != "1" && ! -f "${qemu_source}/build/build.ninja" ]]; then
    (cd "${qemu_source}" && ./configure-qemu-uae --ninja="${ninja_executable}")
fi

"${ninja_executable}" -C "${qemu_source}/build" qemu-uae.so

plugin="${qemu_source}/build/qemu-uae.so"
if [[ ! -f "${plugin}" ]]; then
    echo "error: qemu-uae.so was not produced" >&2
    exit 1
fi

if [[ -n "${output_plugin}" ]]; then
    mkdir -p "$(dirname "${output_plugin}")"
    cp "${plugin}" "${output_plugin}"
    plugin="${output_plugin}"
fi

echo "${plugin}"
