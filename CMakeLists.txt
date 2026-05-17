cmake_minimum_required(VERSION 3.20)

if(CMAKE_HOST_APPLE)
    set(WINUAE_UNIX_DEFAULT_MACOS_DEPLOYMENT_TARGET "14.0")
    set(WINUAE_UNIX_MACOS_DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/../winuae-macos-deps" CACHE PATH "Private macOS dependency prefix built by tools/macos-build-deps.sh")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${WINUAE_UNIX_DEFAULT_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "Minimum macOS version for WinUAE Unix builds")
    elseif(CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${WINUAE_UNIX_DEFAULT_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "Minimum macOS version for WinUAE Unix builds" FORCE)
    endif()
endif()

project(winuae_unix_port LANGUAGES C CXX)

option(WINUAE_UNIX_BUILD_EXECUTABLE "Link the initial Unix command-line executable" ON)
option(WINUAE_UNIX_WITH_SDL3 "Build the Unix SDL3 video/audio backend" ON)
option(WINUAE_UNIX_WITH_SLIRP "Build built-in SLIRP user-mode networking and A2065 Ethernet support" ON)
option(WINUAE_UNIX_WITH_NCR_SCSI "Build NCR/NCR9x SCSI controller emulation" ON)
option(WINUAE_UNIX_WITH_QT_UI "Build the optional Unix Qt launcher/config UI" ON)
option(WINUAE_UNIX_WITH_INTEGRATED_QT_UI "Allow winuae_unix to host the Unix Qt frontend" ON)

find_package(ZLIB REQUIRED)
find_package(PkgConfig QUIET)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_library(winuae_core OBJECT)

target_compile_definitions(winuae_core PRIVATE
    UAE_TARGET_UNIX=1
    UAE_NOGUI=1
    WINUAE_UNIX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(winuae_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/od-unix"
    "${CMAKE_CURRENT_SOURCE_DIR}/od-unix/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/archivers/dms"
    "${CMAKE_CURRENT_SOURCE_DIR}/archivers/lha"
    "${CMAKE_CURRENT_SOURCE_DIR}/archivers/lzx"
    "${CMAKE_CURRENT_SOURCE_DIR}/archivers/mp2"
    "${CMAKE_CURRENT_SOURCE_DIR}/archivers/wrp"
    "${CMAKE_CURRENT_SOURCE_DIR}/archivers/zip"
)

target_compile_options(winuae_core PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-register>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-format-security>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-multichar>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-value>
)

set(WINUAE_UNIX_EXTRA_SOURCES)
set(WINUAE_UNIX_EXTRA_LIBRARIES)
set(WINUAE_UNIX_EXTRA_LIBRARY_DIRS)

if(WINUAE_UNIX_WITH_SLIRP)
    target_compile_definitions(winuae_core PRIVATE
        A2065=1
        WITH_SLIRP=1
        WITH_BUILTIN_SLIRP=1
    )
    list(APPEND WINUAE_UNIX_EXTRA_SOURCES
        a2065.cpp
        slirp_uae.cpp
        slirp/bootp.cpp
        slirp/cksum.cpp
        slirp/if.cpp
        slirp/ip_icmp.cpp
        slirp/ip_input.cpp
        slirp/ip_output.cpp
        slirp/mbuf.cpp
        slirp/misc.cpp
        slirp/sbuf.cpp
        slirp/slirp.cpp
        slirp/slirpdebug.cpp
        slirp/socket.cpp
        slirp/tcp_input.cpp
        slirp/tcp_output.cpp
        slirp/tcp_subr.cpp
        slirp/tcp_timer.cpp
        slirp/tftp.cpp
        slirp/udp.cpp
    )
endif()

if(WINUAE_UNIX_WITH_NCR_SCSI)
    target_compile_definitions(winuae_core PRIVATE NCR=1 NCR9X=1)
    set_source_files_properties(ncr_scsi.cpp ncr9x_scsi.cpp PROPERTIES COMPILE_DEFINITIONS WITH_QEMU_CPU=1)
    list(APPEND WINUAE_UNIX_EXTRA_SOURCES
        ncr_scsi.cpp
        ncr9x_scsi.cpp
        qemuvga/esp.cpp
        qemuvga/lsi53c710.cpp
        qemuvga/lsi53c895a.cpp
        qemuvga/qemuuaeglue.cpp
    )
endif()

if(WINUAE_UNIX_WITH_SDL3)
    find_package(SDL3 CONFIG QUIET)
    if(TARGET SDL3::SDL3)
        set(WINUAE_UNIX_SDL3_TARGET SDL3::SDL3)
    elseif(TARGET SDL3::SDL3-shared)
        set(WINUAE_UNIX_SDL3_TARGET SDL3::SDL3-shared)
    elseif(TARGET SDL3-static)
        set(WINUAE_UNIX_SDL3_TARGET SDL3-static)
    elseif(TARGET SDL3)
        set(WINUAE_UNIX_SDL3_TARGET SDL3)
    endif()
    if(NOT WINUAE_UNIX_SDL3_TARGET AND PkgConfig_FOUND)
        pkg_check_modules(SDL3 QUIET sdl3)
    endif()
endif()

if(WINUAE_UNIX_SDL3_TARGET)
    target_compile_definitions(winuae_core PRIVATE UAE_UNIX_WITH_SDL3=1)
    target_link_libraries(winuae_core PRIVATE ${WINUAE_UNIX_SDL3_TARGET})
    list(APPEND WINUAE_UNIX_EXTRA_SOURCES od-unix/video_sdl.cpp)
    list(APPEND WINUAE_UNIX_EXTRA_LIBRARIES ${WINUAE_UNIX_SDL3_TARGET})
elseif(SDL3_FOUND)
    target_compile_definitions(winuae_core PRIVATE UAE_UNIX_WITH_SDL3=1)
    target_include_directories(winuae_core PRIVATE ${SDL3_INCLUDE_DIRS})
    target_compile_options(winuae_core PRIVATE ${SDL3_CFLAGS_OTHER})
    list(APPEND WINUAE_UNIX_EXTRA_SOURCES od-unix/video_sdl.cpp)
    list(APPEND WINUAE_UNIX_EXTRA_LIBRARIES ${SDL3_LIBRARIES})
    list(APPEND WINUAE_UNIX_EXTRA_LIBRARY_DIRS ${SDL3_LIBRARY_DIRS})
else()
    list(APPEND WINUAE_UNIX_EXTRA_SOURCES od-unix/video_null.cpp)
endif()

set(WINUAE_CORE_SOURCES
    a2091.cpp
    akiko.cpp
    amax.cpp
    ar.cpp
    arcadia.cpp
    aros.rom.cpp
    audio.cpp
    autoconf.cpp
    blitfunc.cpp
    blitops.cpp
    blittable.cpp
    blitter.cpp
    blkdev.cpp
    blkdev_cdimage.cpp
    bsdsocket.cpp
    calc.cpp
    cd32_fmv.cpp
    cd32_fmv_genlock.cpp
    cdrom.cpp
    cdtv.cpp
    cdtvcr.cpp
    cfgfile.cpp
    cia.cpp
    consolehook.cpp
    cpudefs.cpp
    cpustbl.cpp
    cpuemu_0.cpp
    cpuemu_11.cpp
    cpuemu_13.cpp
    cpuemu_20.cpp
    cpuemu_21.cpp
    cpuemu_22.cpp
    cpuemu_23.cpp
    cpuemu_24.cpp
    cpuemu_31.cpp
    cpuemu_32.cpp
    cpuemu_33.cpp
    cpuemu_34.cpp
    cpuemu_35.cpp
    cpuemu_40.cpp
    cpuemu_50.cpp
    cpummu.cpp
    cpummu30.cpp
    crc32.cpp
    custom.cpp
    debug.cpp
    debugmem.cpp
    def_icons.cpp
    devices.cpp
    disasm.cpp
    disk.cpp
    diskutil.cpp
    dlopen.cpp
    dongle.cpp
    drawing.cpp
    driveclick.cpp
    enforcer.cpp
    ethernet.cpp
    events.cpp
    expansion.cpp
    fdi2raw.cpp
    filesys.cpp
    flashrom.cpp
    fpp.cpp
    fpp_native.cpp
    fpp_softfloat.cpp
    framebufferboards.cpp
    fsdb.cpp
    fsdb_unix.cpp
    gayle.cpp
    gfxlib.cpp
    gfxutil.cpp
    hardfile.cpp
    hrtmon.rom.cpp
    ide.cpp
    idecontrollers.cpp
    identify.cpp
    ini.cpp
    inputdevice.cpp
    inputrecord.cpp
    isofs.cpp
    keybuf.cpp
    logging.cpp
    main.cpp
    memory.cpp
    missing.cpp
    mos6502.cpp
    native2amiga.cpp
    newcpu.cpp
    newcpu_common.cpp
    readcpu.cpp
    rommgr.cpp
    rtc.cpp
    savestate.cpp
    scp.cpp
    scsi.cpp
    scsitape.cpp
    statusline.cpp
    test_card.cpp
    tinyxml2.cpp
    traps.cpp
    uaeexe.cpp
    uaelib.cpp
    uaeresource.cpp
    vm.cpp
    writelog.cpp
    zfile.cpp
    archivers/mp2/kjmp2.cpp
    softfloat/softfloat.cpp
    softfloat/softfloat_decimal.cpp
    softfloat/softfloat_fpsp.cpp
    od-unix/charset.cpp
    od-unix/clipboard.cpp
    od-unix/config.cpp
    od-unix/driveclick.cpp
    od-unix/filesys_host.cpp
    od-unix/graphics.cpp
    od-unix/gui.cpp
    od-unix/hardfile_host.cpp
    od-unix/input.cpp
    od-unix/logging.cpp
    od-unix/mman.cpp
    od-unix/path_expand.cpp
    od-unix/parallel.cpp
    od-unix/rtg.cpp
    od-unix/serial.cpp
    od-unix/sound.cpp
    od-unix/stubs.cpp
    od-unix/support.cpp
    od-unix/threading.cpp
    od-unix/time.cpp
)

list(APPEND WINUAE_CORE_SOURCES ${WINUAE_UNIX_EXTRA_SOURCES})
target_sources(winuae_core PRIVATE ${WINUAE_CORE_SOURCES})

if(WINUAE_UNIX_BUILD_EXECUTABLE)
    add_executable(winuae_unix $<TARGET_OBJECTS:winuae_core>)
    if(WINUAE_UNIX_EXTRA_LIBRARY_DIRS)
        target_link_directories(winuae_unix PRIVATE ${WINUAE_UNIX_EXTRA_LIBRARY_DIRS})
    endif()
    target_link_libraries(winuae_unix PRIVATE pthread ${CMAKE_DL_LIBS} ZLIB::ZLIB ${WINUAE_UNIX_EXTRA_LIBRARIES})
endif()

add_executable(winuae_unix_path_expand_test EXCLUDE_FROM_ALL
    od-unix/path_expand.cpp
    od-unix/path_expand_test.cpp
)
target_include_directories(winuae_unix_path_expand_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/od-unix"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(WINUAE_UNIX_WITH_QT_UI OR WINUAE_UNIX_WITH_INTEGRATED_QT_UI)
    set(WINUAE_UNIX_VERSION_MAJOR 0)
    set(WINUAE_UNIX_VERSION_MINOR 0)
    set(WINUAE_UNIX_VERSION_REVISION 0)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/options.h" WINUAE_UNIX_VERSION_LINES REGEX "^#define UAE(MAJOR|MINOR|SUBREV) [0-9]+")
    foreach(WINUAE_UNIX_VERSION_LINE IN LISTS WINUAE_UNIX_VERSION_LINES)
        if(WINUAE_UNIX_VERSION_LINE MATCHES "^#define UAEMAJOR +([0-9]+)")
            set(WINUAE_UNIX_VERSION_MAJOR "${CMAKE_MATCH_1}")
        elseif(WINUAE_UNIX_VERSION_LINE MATCHES "^#define UAEMINOR +([0-9]+)")
            set(WINUAE_UNIX_VERSION_MINOR "${CMAKE_MATCH_1}")
        elseif(WINUAE_UNIX_VERSION_LINE MATCHES "^#define UAESUBREV +([0-9]+)")
            set(WINUAE_UNIX_VERSION_REVISION "${CMAKE_MATCH_1}")
        endif()
    endforeach()

    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        set(WINUAE_UNIX_QT_WIDGETS_TARGET Qt6::Widgets)
    else()
        find_package(Qt5 COMPONENTS Widgets QUIET)
        if(Qt5_FOUND)
            set(WINUAE_UNIX_QT_WIDGETS_TARGET Qt5::Widgets)
        endif()
    endif()

    if(WINUAE_UNIX_QT_WIDGETS_TARGET)
        add_library(winuae_unix_qt_frontend STATIC
            od-unix/qt/config.cpp
            od-unix/qt/launcher.cpp
            od-unix/qt/launcher_bridge.cpp
            od-unix/qt/launcher_backend.cpp
            od-unix/qt/mount_config.cpp
            od-unix/qt/prefs_adapter.cpp
        )
        target_include_directories(winuae_unix_qt_frontend PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/od-unix"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        )
        target_compile_definitions(winuae_unix_qt_frontend PRIVATE
            WINUAE_UNIX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            WINUAE_UNIX_BUILD_DIR="${CMAKE_BINARY_DIR}"
            WINUAE_UNIX_VERSION_MAJOR=${WINUAE_UNIX_VERSION_MAJOR}
            WINUAE_UNIX_VERSION_MINOR=${WINUAE_UNIX_VERSION_MINOR}
            WINUAE_UNIX_VERSION_REVISION=${WINUAE_UNIX_VERSION_REVISION}
        )
        target_link_libraries(winuae_unix_qt_frontend PUBLIC ${WINUAE_UNIX_QT_WIDGETS_TARGET})
        if(WINUAE_UNIX_SDL3_TARGET)
            target_compile_definitions(winuae_unix_qt_frontend PRIVATE UAE_UNIX_WITH_SDL3=1)
            target_link_libraries(winuae_unix_qt_frontend PUBLIC ${WINUAE_UNIX_SDL3_TARGET})
        elseif(SDL3_FOUND)
            target_compile_definitions(winuae_unix_qt_frontend PRIVATE UAE_UNIX_WITH_SDL3=1)
            target_include_directories(winuae_unix_qt_frontend PRIVATE ${SDL3_INCLUDE_DIRS})
            target_compile_options(winuae_unix_qt_frontend PRIVATE ${SDL3_CFLAGS_OTHER})
            target_link_libraries(winuae_unix_qt_frontend PUBLIC ${SDL3_LIBRARIES})
            if(SDL3_LIBRARY_DIRS)
                target_link_directories(winuae_unix_qt_frontend PUBLIC ${SDL3_LIBRARY_DIRS})
            endif()
        endif()

        if(WINUAE_UNIX_WITH_QT_UI)
            add_executable(winuae_unix_qt
                od-unix/qt/launcher_main.cpp
            )
            target_link_libraries(winuae_unix_qt PRIVATE winuae_unix_qt_frontend)
            if(TARGET winuae_unix)
                add_dependencies(winuae_unix_qt winuae_unix)
            endif()
        endif()

        add_executable(winuae_unix_qt_config_test EXCLUDE_FROM_ALL
            od-unix/qt/config.cpp
            od-unix/qt/config_roundtrip_test.cpp
        )
        target_link_libraries(winuae_unix_qt_config_test PRIVATE ${WINUAE_UNIX_QT_WIDGETS_TARGET})

        add_executable(winuae_unix_qt_mount_config_test EXCLUDE_FROM_ALL
            od-unix/qt/mount_config.cpp
            od-unix/qt/mount_config_test.cpp
        )
        target_link_libraries(winuae_unix_qt_mount_config_test PRIVATE ${WINUAE_UNIX_QT_WIDGETS_TARGET})

        add_executable(winuae_unix_qt_prefs_adapter_test EXCLUDE_FROM_ALL
            od-unix/qt/config.cpp
            od-unix/qt/prefs_adapter.cpp
            od-unix/qt/prefs_adapter_test.cpp
        )
        target_include_directories(winuae_unix_qt_prefs_adapter_test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/od-unix"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        )
        target_link_libraries(winuae_unix_qt_prefs_adapter_test PRIVATE ${WINUAE_UNIX_QT_WIDGETS_TARGET})

        if(WINUAE_UNIX_WITH_INTEGRATED_QT_UI)
            target_compile_definitions(winuae_core PRIVATE WINUAE_UNIX_WITH_INTEGRATED_QT_UI=1)
            if(TARGET winuae_unix)
                target_link_libraries(winuae_unix PRIVATE winuae_unix_qt_frontend)
            endif()
        endif()
    else()
        message(STATUS "Qt Widgets not found; skipping Unix Qt targets")
    endif()
endif()

if(APPLE)
    add_custom_target(winuae_unix_macos_deps
        COMMAND "${CMAKE_COMMAND}" -E env
            "WINUAE_MACOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos-build-deps.sh" "${WINUAE_UNIX_MACOS_DEPS_PREFIX}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building private macOS dependencies for deployment target ${CMAKE_OSX_DEPLOYMENT_TARGET}"
        VERBATIM
    )
endif()

if(APPLE AND TARGET winuae_unix)
    add_custom_target(winuae_unix_macos_app
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos-bundle.sh" "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/package"
        DEPENDS winuae_unix
        COMMENT "Creating WinUAE.app"
        VERBATIM
    )
    add_custom_target(winuae_unix_macos_dmg
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos-dmg.sh" "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/package"
        DEPENDS winuae_unix
        COMMENT "Creating WinUAE drag-install DMG"
        VERBATIM
    )
    if(TARGET winuae_unix_qt_frontend AND WINUAE_UNIX_WITH_INTEGRATED_QT_UI)
        add_custom_target(winuae_unix_macos_app_smoke
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos-smoke-app.sh" "${CMAKE_BINARY_DIR}/package/WinUAE.app"
            DEPENDS winuae_unix_macos_app
            COMMENT "Smoke testing packaged WinUAE.app Qt launch"
            VERBATIM
        )
        add_custom_target(winuae_unix_macos_release_check
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos-smoke-app.sh" "${CMAKE_BINARY_DIR}/package/WinUAE.app"
            DEPENDS winuae_unix_macos_dmg
            COMMENT "Release checking packaged WinUAE DMG and Qt app launch"
            VERBATIM
        )
    else()
        add_custom_target(winuae_unix_macos_release_check
            COMMAND "${CMAKE_COMMAND}" -E echo "Qt integrated UI is disabled; release check is limited to DMG verification"
            DEPENDS winuae_unix_macos_dmg
            COMMENT "Release checking packaged WinUAE DMG"
            VERBATIM
        )
    endif()
endif()
