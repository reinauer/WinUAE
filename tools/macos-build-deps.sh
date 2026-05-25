#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 [prefix]

Build macOS dependencies into a private prefix with a fixed deployment target.
Use the resulting prefix as CMAKE_PREFIX_PATH when configuring WinUAE.

Arguments:
  prefix  Install prefix for SDL3, Qt, and optional libpng/libFLAC. Defaults to
          WINUAE_DEPS_PREFIX or <repo>/../winuae-macos-deps.

Environment:
  WINUAE_MACOS_DEPLOYMENT_TARGET  Minimum macOS version. Defaults to 13.0.
  WINUAE_DEPS_BUILD_DIR           Build directory. Defaults to <prefix>/build.
  WINUAE_DEPS_JOBS                Parallel build jobs. Defaults to hw.ncpu.
  WINUAE_SDL3_SOURCE              SDL3 source tree. Required unless
                                  WINUAE_SKIP_SDL3=1.
  WINUAE_QT_SOURCE                Qt 6 source tree, or a qtbase CMake source
                                  tree. Required unless WINUAE_SKIP_QT=1.
  WINUAE_LIBPNG_SOURCE            libpng source tree. Optional unless
                                  WINUAE_REQUIRE_LIBPNG=1.
  WINUAE_FLAC_SOURCE              FLAC source tree. Optional unless
                                  WINUAE_REQUIRE_FLAC=1.
  WINUAE_SKIP_SDL3=1              Do not build SDL3.
  WINUAE_SKIP_QT=1                Do not build Qt.
  WINUAE_SKIP_LIBPNG=1            Do not build libpng.
  WINUAE_SKIP_FLAC=1              Do not build libFLAC.
  WINUAE_REQUIRE_LIBPNG=1         Fail if WINUAE_LIBPNG_SOURCE is missing.
  WINUAE_REQUIRE_FLAC=1           Fail if WINUAE_FLAC_SOURCE is missing.
  WINUAE_SDL3_CMAKE_ARGS          Extra arguments passed to SDL3 CMake.
  WINUAE_QT_CONFIGURE_ARGS        Extra arguments passed to Qt configure,
                                  in addition to the release-safe defaults.
  WINUAE_QT_CMAKE_ARGS            Extra arguments passed to Qt CMake.
  WINUAE_LIBPNG_CMAKE_ARGS        Extra arguments passed to libpng CMake.
  WINUAE_FLAC_CMAKE_ARGS          Extra arguments passed to FLAC CMake.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$(cd "${script_dir}/.." && pwd)"
target="${WINUAE_MACOS_DEPLOYMENT_TARGET:-13.0}"
prefix="${1:-${WINUAE_DEPS_PREFIX:-${source_dir}/../winuae-macos-deps}}"
build_dir="${WINUAE_DEPS_BUILD_DIR:-${prefix}/build}"
sdl_source="${WINUAE_SDL3_SOURCE:-}"
qt_source="${WINUAE_QT_SOURCE:-}"
libpng_source="${WINUAE_LIBPNG_SOURCE:-}"
flac_source="${WINUAE_FLAC_SOURCE:-}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: macOS dependency builds require Darwin/macOS" >&2
    exit 1
fi

jobs="${WINUAE_DEPS_JOBS:-}"
if [[ -z "${jobs}" ]]; then
    jobs="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"
fi
export MACOSX_DEPLOYMENT_TARGET="${target}"

require_source() {
    local name="$1"
    local path="$2"
    if [[ -z "${path}" ]]; then
        echo "error: ${name} source path is required" >&2
        usage >&2
        exit 1
    fi
    if [[ ! -d "${path}" ]]; then
        echo "error: ${name} source directory not found: ${path}" >&2
        exit 1
    fi
}

run_cmake_build() {
    local src="$1"
    local bld="$2"
    shift 2
    cmake -S "${src}" -B "${bld}" "$@"
    cmake --build "${bld}" -j "${jobs}"
    cmake --install "${bld}"
}

patch_qtbase_source() {
    local header="${qt_source}/src/corelib/thread/qyieldcpu.h"
    if [[ -f "${header}" ]] && grep -q "__yield();" "${header}" && ! grep -q "arm_acle.h" "${header}"; then
        perl -0pi -e 's/(#include <QtCore\/qtconfigmacros\.h>\n)/$1\n#if defined(__has_include)\n#  if __has_include(<arm_acle.h>)\n#    include <arm_acle.h>\n#  endif\n#endif\n/' "${header}"
    fi

    local simd="${qt_source}/src/corelib/global/qsimd.cpp"
    if [[ -f "${simd}" ]] && grep -q 'sysctlbyname("hw.optional.neon"' "${simd}" && ! grep -q "AArch64 includes Advanced SIMD" "${simd}"; then
        perl -0pi -e 's/#elif defined\(Q_OS_DARWIN\) && defined\(Q_PROCESSOR_ARM\)\n    unsigned feature;\n    size_t len = sizeof\(feature\);\n    if \(sysctlbyname\("hw\.optional\.neon", &feature, &len, nullptr, 0\) == 0\)\n        features \|= feature \? CpuFeatureNEON : 0;/#elif defined(Q_OS_DARWIN) \&\& defined(Q_PROCESSOR_ARM)\n    unsigned feature;\n    size_t len = sizeof(feature);\n#  if defined(Q_PROCESSOR_ARM_64)\n    \/\/ AArch64 includes Advanced SIMD; some macOS versions no longer\n    \/\/ expose the legacy hw.optional.neon sysctl that Qt probes here.\n    features |= CpuFeatureNEON;\n#  else\n    if (sysctlbyname("hw.optional.neon", \&feature, \&len, nullptr, 0) == 0)\n        features |= feature ? CpuFeatureNEON : 0;\n#  endif/' "${simd}"
    fi
}

split_extra_args() {
    if [[ -n "${1:-}" ]]; then
        # Intentionally split user-provided extra flags the same way a shell would.
        # shellcheck disable=SC2206
        extra_args=($1)
    else
        extra_args=()
    fi
}

mkdir -p "${prefix}" "${build_dir}"

if [[ "${WINUAE_SKIP_LIBPNG:-0}" != "1" ]]; then
    if [[ -n "${libpng_source}" ]]; then
        require_source "libpng" "${libpng_source}"
        run_cmake_build "${libpng_source}" "${build_dir}/libpng" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="${prefix}" \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="${target}" \
            -DPNG_SHARED=ON \
            -DPNG_STATIC=OFF \
            -DPNG_TESTS=OFF \
            -DPNG_TOOLS=OFF \
            ${WINUAE_LIBPNG_CMAKE_ARGS:-}
    elif [[ "${WINUAE_REQUIRE_LIBPNG:-0}" == "1" ]]; then
        echo "error: libpng source path is required when WINUAE_REQUIRE_LIBPNG=1" >&2
        usage >&2
        exit 1
    else
        echo "note: WINUAE_LIBPNG_SOURCE not set; skipping private libpng build" >&2
    fi
fi

if [[ "${WINUAE_SKIP_FLAC:-0}" != "1" ]]; then
    if [[ -n "${flac_source}" ]]; then
        require_source "FLAC" "${flac_source}"
        run_cmake_build "${flac_source}" "${build_dir}/flac" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="${prefix}" \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="${target}" \
            -DBUILD_SHARED_LIBS=ON \
            -DBUILD_CXXLIBS=OFF \
            -DBUILD_PROGRAMS=OFF \
            -DBUILD_EXAMPLES=OFF \
            -DBUILD_DOCS=OFF \
            -DBUILD_TESTING=OFF \
            -DINSTALL_MANPAGES=OFF \
            -DINSTALL_PKGCONFIG_MODULES=ON \
            -DWITH_OGG=OFF \
            ${WINUAE_FLAC_CMAKE_ARGS:-}
    elif [[ "${WINUAE_REQUIRE_FLAC:-0}" == "1" ]]; then
        echo "error: FLAC source path is required when WINUAE_REQUIRE_FLAC=1" >&2
        usage >&2
        exit 1
    else
        echo "note: WINUAE_FLAC_SOURCE not set; skipping private libFLAC build" >&2
    fi
fi

if [[ "${WINUAE_SKIP_SDL3:-0}" != "1" ]]; then
    require_source "SDL3" "${sdl_source}"
    run_cmake_build "${sdl_source}" "${build_dir}/sdl3" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${target}" \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_EXAMPLES=OFF \
        -DSDL_INSTALL_TESTS=OFF \
        ${WINUAE_SDL3_CMAKE_ARGS:-}
fi

if [[ "${WINUAE_SKIP_QT:-0}" != "1" ]]; then
    require_source "Qt" "${qt_source}"
    qt_build="${build_dir}/qt"
    mkdir -p "${qt_build}"
    patch_qtbase_source

    qt_configure_args=(
        -force-bundled-libs
        -no-dbus
        -no-openssl
        -no-glib
        -no-icu
        -no-cups
        -no-fontconfig
        -no-gtk
        -qt-doubleconversion
        -qt-pcre
        -qt-zlib
        -qt-libpng
        -qt-libjpeg
        -qt-freetype
        -qt-harfbuzz
    )
    split_extra_args "${WINUAE_QT_CONFIGURE_ARGS:-}"
    qt_configure_args+=("${extra_args[@]}")

    qt_cmake_args=()
    if ! xcodebuild -version >/dev/null 2>&1 && xcrun --show-sdk-path >/dev/null 2>&1; then
        qt_cmake_args+=(-DQT_NO_XCODE_MIN_VERSION_CHECK=ON)
    fi
    split_extra_args "${WINUAE_QT_CMAKE_ARGS:-}"
    qt_cmake_args+=("${extra_args[@]}")

    qt_submodule_args=()
    if [[ -d "${qt_source}/qtbase" || -f "${qt_source}/init-repository" ]]; then
        qt_submodule_args=(-submodules qtbase)
    fi

    if [[ -x "${qt_source}/configure" && ! -d "${qt_source}/src/corelib" ]]; then
        (
            cd "${qt_build}"
            "${qt_source}/configure" \
                -prefix "${prefix}" \
                -release \
                -opensource \
                -confirm-license \
                -nomake examples \
                -nomake tests \
                "${qt_submodule_args[@]}" \
                "${qt_configure_args[@]}" \
                -- \
                -DCMAKE_OSX_DEPLOYMENT_TARGET="${target}" \
                "${qt_cmake_args[@]}"
        )
        cmake --build "${qt_build}" -j "${jobs}"
        cmake --install "${qt_build}"
    else
        run_cmake_build "${qt_source}" "${qt_build}" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="${prefix}" \
            -DCMAKE_OSX_DEPLOYMENT_TARGET="${target}" \
            -DQT_BUILD_EXAMPLES=OFF \
            -DQT_BUILD_TESTS=OFF \
            -DQT_FEATURE_dbus=OFF \
            -DQT_FEATURE_openssl=OFF \
            -DQT_FEATURE_glib=OFF \
            -DQT_FEATURE_icu=OFF \
            -DQT_FEATURE_cups=OFF \
            -DQT_FEATURE_fontconfig=OFF \
            -DQT_FEATURE_gtk=OFF \
            -DQT_FEATURE_opengl=OFF \
            -DQT_FEATURE_opengles2=OFF \
            "${qt_cmake_args[@]}"
    fi
fi

"${script_dir}/macos-check-deployment-target.sh" "${prefix}" "${target}"

env_file="${prefix}/winuae-macos-deps-env.sh"
cat > "${env_file}" <<EOF
export CMAKE_PREFIX_PATH="${prefix}\${CMAKE_PREFIX_PATH:+:\${CMAKE_PREFIX_PATH}}"
export PKG_CONFIG_PATH="${prefix}/lib/pkgconfig:${prefix}/share/pkgconfig\${PKG_CONFIG_PATH:+:\${PKG_CONFIG_PATH}}"
export PATH="${prefix}/bin:\${PATH}"
export WINUAE_MACOS_DEPLOYMENT_TARGET="${target}"
EOF

cat <<EOF
macOS dependencies installed to: ${prefix}
Deployment target verified: ${target}

Use:
  source "${env_file}"
  cmake -S "${source_dir}" -B /tmp/winuae_cmake_macos \\
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \\
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${target}" \\
    -DCMAKE_PREFIX_PATH="${prefix}"
EOF
