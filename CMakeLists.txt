cmake_minimum_required(VERSION 3.20)

if(CMAKE_HOST_APPLE)
    set(WINUAE_UNIX_DEFAULT_MACOS_DEPLOYMENT_TARGET "13.0")
    set(WINUAE_UNIX_MACOS_DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/../winuae-macos-deps" CACHE PATH "Private macOS dependency prefix built by tools/macos-build-deps.sh")
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${WINUAE_UNIX_DEFAULT_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "Minimum macOS version for WinUAE Unix builds")
    elseif(CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${WINUAE_UNIX_DEFAULT_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "Minimum macOS version for WinUAE Unix builds" FORCE)
    endif()
endif()

project(winuae_unix_port LANGUAGES C CXX)

include(GNUInstallDirs)
include(CTest)

set(WINUAE_UNIX_TEST_TARGETS)
function(winuae_unix_register_test target)
    list(APPEND WINUAE_UNIX_TEST_TARGETS "${target}")
    set(WINUAE_UNIX_TEST_TARGETS "${WINUAE_UNIX_TEST_TARGETS}" PARENT_SCOPE)
    if(BUILD_TESTING)
        add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
        set_tests_properties(${target} PROPERTIES LABELS "unix")
    endif()
endfunction()

option(WINUAE_UNIX_BUILD_EXECUTABLE "Link the initial Unix command-line executable" ON)
option(WINUAE_UNIX_WITH_SDL3 "Build the Unix SDL3 video/audio backend" ON)
option(WINUAE_UNIX_WITH_SLIRP "Build built-in SLIRP user-mode networking and A2065 Ethernet support" ON)
option(WINUAE_UNIX_WITH_NCR_SCSI "Build NCR/NCR9x SCSI controller emulation" ON)
option(WINUAE_UNIX_WITH_PROWIZARD "Build Pro-Wizard module ripper support" ON)
option(WINUAE_UNIX_WITH_QT_UI "Build the optional Unix Qt launcher/config UI" ON)
option(WINUAE_UNIX_WITH_INTEGRATED_QT_UI "Allow winuae_unix to host the Unix Qt frontend" ON)
option(WINUAE_UNIX_WITH_LIBPNG "Build optional PNG screenshot support when libpng is available" ON)

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
set(WINUAE_UNIX_VERSION "${WINUAE_UNIX_VERSION_MAJOR}.${WINUAE_UNIX_VERSION_MINOR}.${WINUAE_UNIX_VERSION_REVISION}")

find_package(ZLIB REQUIRED)
if(WINUAE_UNIX_WITH_LIBPNG)
    find_package(PNG QUIET)
endif()
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
    WINUAE_UNIX_INSTALL_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/winuae"
    WINUAE_UNIX_INSTALL_DATADIR_RELATIVE="${CMAKE_INSTALL_DATADIR}/winuae"
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

set(WINUAE_UNIX_USE_LIBPNG OFF)
if(WINUAE_UNIX_WITH_LIBPNG AND PNG_FOUND)
    set(WINUAE_UNIX_USE_LIBPNG ON)
    if(APPLE)
        set(WINUAE_UNIX_PNG_LIBRARY "")
        get_target_property(WINUAE_UNIX_PNG_LIBRARY_RELEASE PNG::PNG IMPORTED_LOCATION_RELEASE)
        get_target_property(WINUAE_UNIX_PNG_LIBRARY_DEFAULT PNG::PNG IMPORTED_LOCATION)
        if(WINUAE_UNIX_PNG_LIBRARY_RELEASE AND EXISTS "${WINUAE_UNIX_PNG_LIBRARY_RELEASE}")
            set(WINUAE_UNIX_PNG_LIBRARY "${WINUAE_UNIX_PNG_LIBRARY_RELEASE}")
        elseif(WINUAE_UNIX_PNG_LIBRARY_DEFAULT AND EXISTS "${WINUAE_UNIX_PNG_LIBRARY_DEFAULT}")
            set(WINUAE_UNIX_PNG_LIBRARY "${WINUAE_UNIX_PNG_LIBRARY_DEFAULT}")
        elseif(PNG_LIBRARY_RELEASE AND EXISTS "${PNG_LIBRARY_RELEASE}")
            set(WINUAE_UNIX_PNG_LIBRARY "${PNG_LIBRARY_RELEASE}")
        endif()
        if(WINUAE_UNIX_PNG_LIBRARY)
            execute_process(
                COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/macos-check-deployment-target.sh" "${WINUAE_UNIX_PNG_LIBRARY}" "${CMAKE_OSX_DEPLOYMENT_TARGET}"
                RESULT_VARIABLE WINUAE_UNIX_PNG_DEPLOYMENT_RESULT
                OUTPUT_VARIABLE WINUAE_UNIX_PNG_DEPLOYMENT_OUTPUT
                ERROR_VARIABLE WINUAE_UNIX_PNG_DEPLOYMENT_ERROR
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_STRIP_TRAILING_WHITESPACE
            )
            if(NOT WINUAE_UNIX_PNG_DEPLOYMENT_RESULT EQUAL 0)
                message(STATUS "libpng is newer than CMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}; PNG screenshots disabled. ${WINUAE_UNIX_PNG_DEPLOYMENT_ERROR}")
                set(WINUAE_UNIX_USE_LIBPNG OFF)
            endif()
        else()
            message(WARNING "libpng was found but its imported library path was not available; PNG screenshots disabled")
            set(WINUAE_UNIX_USE_LIBPNG OFF)
        endif()
    endif()
endif()

if(WINUAE_UNIX_USE_LIBPNG)
    target_compile_definitions(winuae_core PRIVATE WINUAE_UNIX_WITH_LIBPNG=1)
    target_link_libraries(winuae_core PRIVATE PNG::PNG)
    list(APPEND WINUAE_UNIX_EXTRA_LIBRARIES PNG::PNG)
endif()

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

if(WINUAE_UNIX_WITH_PROWIZARD)
    target_compile_definitions(winuae_core PRIVATE PROWIZARD=1)
    target_include_directories(winuae_core PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/prowizard/include"
    )
    list(APPEND WINUAE_UNIX_EXTRA_SOURCES
        moduleripper.cpp
        prowizard/misc/misc.c
        prowizard/prowiz.c
        prowizard/misc/testbag.c
        prowizard/rippers/AC1D_packer.c
        prowizard/rippers/AMOS-MusicBank.c
        prowizard/rippers/AMOS-PowerPackerBank.c
        prowizard/rippers/BP-Soundmonitor.c
        prowizard/rippers/BSI-FutureComposer.c
        prowizard/rippers/Bytekiller.c
        prowizard/rippers/ChipTracker.c
        prowizard/rippers/Crunchmania-Address.c
        prowizard/rippers/Crunchmania-Simple.c
        prowizard/rippers/datacrunchers.c
        prowizard/rippers/DefjamCruncher32-pro.c
        prowizard/rippers/DigiBooster17.c
        prowizard/rippers/DigitalIllusion.c
        prowizard/rippers/DoubleAction.c
        prowizard/rippers/Dragpack100.c
        prowizard/rippers/Dragpack252.c
        prowizard/rippers/EurekaPacker.c
        prowizard/rippers/FastTracker2.c
        prowizard/rippers/FC-M-Packer.c
        prowizard/rippers/FuchsTracker.c
        prowizard/rippers/FutureComposer13.c
        prowizard/rippers/FutureComposer14.c
        prowizard/rippers/FuzzacPacker.c
        prowizard/rippers/GMC.c
        prowizard/rippers/GNUPacker12.c
        prowizard/rippers/GnuPlayer.c
        prowizard/rippers/GPMO.c
        prowizard/rippers/HeatseekerMC10.c
        prowizard/rippers/HornetPacker.c
        prowizard/rippers/HQC2.c
        prowizard/rippers/JamCracker.c
        prowizard/rippers/KefrensSoundMachine.c
        prowizard/rippers/MasterCruncher30-Address.c
        prowizard/rippers/MaxPacker12.c
        prowizard/rippers/MED-Octamed.c
        prowizard/rippers/MegaCruncher-Object.c
        prowizard/rippers/MegaCruncher.c
        prowizard/rippers/MOD-compatible.c
        prowizard/rippers/ModuleProtector.c
        prowizard/rippers/Mugician.c
        prowizard/rippers/Newtron.c
        prowizard/rippers/NewtronOld.c
        prowizard/rippers/NoiseFromHeaven.c
        prowizard/rippers/NoisePacker1.c
        prowizard/rippers/NoisePacker2.c
        prowizard/rippers/NoiseRunner.c
        prowizard/rippers/NovoTrade.c
        prowizard/rippers/NP3.c
        prowizard/rippers/PhaPacker.c
        prowizard/rippers/PolkaPacker.c
        prowizard/rippers/PowerMusic.c
        prowizard/rippers/PowerPacker23.c
        prowizard/rippers/PowerPacker30.c
        prowizard/rippers/PowerPacker40-library.c
        prowizard/rippers/PowerPacker40.c
        prowizard/rippers/Promizer01.c
        prowizard/rippers/Promizer10c.c
        prowizard/rippers/Promizer18a.c
        prowizard/rippers/Promizer20.c
        prowizard/rippers/Promizer40.c
        prowizard/rippers/ProPacker10.c
        prowizard/rippers/ProPacker21.c
        prowizard/rippers/ProPacker30.c
        prowizard/rippers/Prorunner10.c
        prowizard/rippers/Prorunner20.c
        prowizard/rippers/QuadraComposer.c
        prowizard/rippers/RelokIt10.c
        prowizard/rippers/SGT-Packer.c
        prowizard/rippers/skizzo.c
        prowizard/rippers/SkytPacker.c
        prowizard/rippers/SoundFX.c
        prowizard/rippers/SoundTracker.c
        prowizard/rippers/SoundTracker26.c
        prowizard/rippers/SpikeCruncher.c
        prowizard/rippers/StartrekkerPack.c
        prowizard/rippers/StimPacker.c
        prowizard/rippers/StoneArtsPlayer.c
        prowizard/rippers/StoneCracker270.c
        prowizard/rippers/StoneCracker299.c
        prowizard/rippers/StoneCracker299b.c
        prowizard/rippers/StoneCracker299d.c
        prowizard/rippers/StoneCracker300.c
        prowizard/rippers/StoneCracker310.c
        prowizard/rippers/SuperCruncher27.c
        prowizard/rippers/SyncroPacker46.c
        prowizard/rippers/TetraPack102.c
        prowizard/rippers/TetraPack21.c
        prowizard/rippers/TetraPack22.c
        prowizard/rippers/TheDarkDemon.c
        prowizard/rippers/ThePlayer22a.c
        prowizard/rippers/ThePlayer30a.c
        prowizard/rippers/ThePlayer40.c
        prowizard/rippers/ThePlayer41a.c
        prowizard/rippers/ThePlayer50a.c
        prowizard/rippers/ThePlayer60a.c
        prowizard/rippers/ThePlayer61a.c
        prowizard/rippers/TimeCruncher17.c
        prowizard/rippers/TitanicsPlayer.c
        prowizard/rippers/TMK.c
        prowizard/rippers/TNMCruncher.c
        prowizard/rippers/TrackerPacker1.c
        prowizard/rippers/TrackerPacker2.c
        prowizard/rippers/TrackerPacker3.c
        prowizard/rippers/TryItCruncher101.c
        prowizard/rippers/TurboSqueezer61.c
        prowizard/rippers/UnicTracker.c
        prowizard/rippers/UnicTracker2.c
        prowizard/rippers/WantonPacker.c
        prowizard/rippers/XannPlayer.c
        prowizard/rippers/ZenPacker.c
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
    od-unix/screenshot.cpp
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

    function(add_winuae_unix_smoke_target target script)
        add_custom_target(${target}
            COMMAND "${CMAKE_COMMAND}" -E env
                "WINUAE_BUILD_DIR=${CMAKE_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/${script}"
            DEPENDS winuae_unix
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Running ${script}"
            VERBATIM
        )
    endfunction()

    add_winuae_unix_smoke_target(winuae_unix_smoke_a1200 unix-smoke-a1200.sh)
    add_winuae_unix_smoke_target(winuae_unix_smoke_a2065 unix-smoke-a2065.sh)
    add_winuae_unix_smoke_target(winuae_unix_smoke_rtg_z3 unix-smoke-rtg-z3.sh)
    add_winuae_unix_smoke_target(winuae_unix_smoke_uaegfx unix-smoke-uaegfx.sh)
    add_winuae_unix_smoke_target(winuae_unix_smoke_p96_guest unix-smoke-p96-guest.sh)
    add_winuae_unix_smoke_target(winuae_unix_smoke_a4091_hdf unix-smoke-a4091-hdf.sh)
    add_winuae_unix_smoke_target(winuae_unix_smoke_serial_tcp unix-smoke-serial-tcp.sh)
    add_winuae_unix_smoke_target(winuae_unix_smoke_path_config unix-smoke-path-config.sh)
    add_custom_target(winuae_unix_smoke_basic
        DEPENDS winuae_unix_smoke_a1200 winuae_unix_smoke_path_config
    )
endif()

add_executable(winuae_unix_path_expand_test EXCLUDE_FROM_ALL
    od-unix/path_expand.cpp
    od-unix/path_expand_test.cpp
)
target_include_directories(winuae_unix_path_expand_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/od-unix"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
winuae_unix_register_test(winuae_unix_path_expand_test)

add_executable(winuae_unix_threading_test EXCLUDE_FROM_ALL
    od-unix/threading.cpp
    od-unix/threading_test.cpp
)
target_include_directories(winuae_unix_threading_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/od-unix"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(winuae_unix_threading_test PRIVATE pthread)
winuae_unix_register_test(winuae_unix_threading_test)

if(WINUAE_UNIX_WITH_QT_UI OR WINUAE_UNIX_WITH_INTEGRATED_QT_UI)
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
            od-unix/qt/path_utils.cpp
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
            WINUAE_UNIX_INSTALL_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/winuae"
            WINUAE_UNIX_INSTALL_DATADIR_RELATIVE="${CMAKE_INSTALL_DATADIR}/winuae"
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
        winuae_unix_register_test(winuae_unix_qt_config_test)

        add_executable(winuae_unix_qt_mount_config_test EXCLUDE_FROM_ALL
            od-unix/qt/mount_config.cpp
            od-unix/qt/mount_config_test.cpp
        )
        target_link_libraries(winuae_unix_qt_mount_config_test PRIVATE ${WINUAE_UNIX_QT_WIDGETS_TARGET})
        winuae_unix_register_test(winuae_unix_qt_mount_config_test)

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
        winuae_unix_register_test(winuae_unix_qt_prefs_adapter_test)

        add_executable(winuae_unix_qt_path_utils_test EXCLUDE_FROM_ALL
            od-unix/qt/path_utils.cpp
            od-unix/qt/path_utils_test.cpp
        )
        target_link_libraries(winuae_unix_qt_path_utils_test PRIVATE ${WINUAE_UNIX_QT_WIDGETS_TARGET})
        winuae_unix_register_test(winuae_unix_qt_path_utils_test)

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

if(WINUAE_UNIX_TEST_TARGETS)
    add_custom_target(winuae_unix_tests
        DEPENDS ${WINUAE_UNIX_TEST_TARGETS}
    )
    if(BUILD_TESTING)
        add_custom_target(winuae_unix_check
            COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
            DEPENDS winuae_unix_tests
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Running Unix host tests"
            VERBATIM
        )
    endif()
endif()

if(TARGET winuae_unix AND NOT APPLE)
    install(TARGETS winuae_unix
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(FILES README_unix.md UNIX_RUNTIME_PARITY.md
        DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    )
    install(DIRECTORY od-win32/resources/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/winuae/od-win32/resources"
    )
    if(NOT APPLE)
        install(FILES od-unix/share/applications/net.winuae.WinUAE.desktop
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
        )
        install(FILES od-unix/share/mime/packages/net.winuae.WinUAE.xml
            DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages"
        )
        install(FILES od-win32/resources/amiga.png
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
            RENAME winuae.png
        )
        install(FILES od-win32/resources/amiga.png
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/mimetypes"
            RENAME application-x-winuae-config.png
        )
    endif()
endif()

if(TARGET winuae_unix_qt AND NOT APPLE)
    install(TARGETS winuae_unix_qt
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
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

if(NOT APPLE)
    set(CPACK_PACKAGE_NAME "WinUAE")
    set(CPACK_PACKAGE_VENDOR "WinUAE")
    set(CPACK_PACKAGE_VERSION "${WINUAE_UNIX_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WinUAE Unix port")
    set(CPACK_PACKAGE_CONTACT "https://www.winuae.net/")
    set(CPACK_PACKAGE_FILE_NAME "WinUAE-${WINUAE_UNIX_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README_unix.md")
    set(CPACK_GENERATOR "TGZ")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND CPACK_GENERATOR "DEB")
        set(CPACK_DEBIAN_PACKAGE_NAME "winuae")
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://www.winuae.net/")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "WinUAE")
        set(CPACK_DEBIAN_PACKAGE_SECTION "emulators")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    endif()
    include(CPack)
endif()
