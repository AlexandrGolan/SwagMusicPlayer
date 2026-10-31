cmake_minimum_required(VERSION 3.20)

project(
    smp
    VERSION 2.0.0
    DESCRIPTION "SMP - a small terminal music player"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SMP_BUILD_TESTS "Build the unit tests" ON)
option(SMP_WERROR "Treat compiler warnings as errors" OFF)
option(SMP_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(SMP_STRICT "Enable extra strict warnings" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------
function(smp_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(SMP_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(
            ${target}
            PRIVATE -Wall
                    -Wextra
                    -Wpedantic
                    -Wshadow
                    -Wstrict-prototypes
                    -Wmissing-prototypes
                    -Wcast-qual
                    -Wvla
                    -Wundef
                    -Wformat=2
        )
        if(SMP_STRICT)
            target_compile_options(${target} PRIVATE -Wconversion -Wsign-conversion)
        endif()
        if(SMP_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(SMP_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Third party single header libraries
# ---------------------------------------------------------------------------
add_library(smp_vendor STATIC src/vendor_impl.c)
target_include_directories(smp_vendor SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

# ---------------------------------------------------------------------------
# Platform sources
# ---------------------------------------------------------------------------
set(SMP_PLATFORM_SOURCES)

if(WIN32)
    list(
        APPEND
        SMP_PLATFORM_SOURCES
        src/plat/thread_win.c
        src/plat/term_win.c
        src/audio/audio_wasapi.c
    )
else()
    list(APPEND SMP_PLATFORM_SOURCES src/plat/thread_posix.c src/plat/term_posix.c)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND SMP_PLATFORM_SOURCES src/audio/audio_alsa.c)
    endif()
endif()

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(
    smp_core
    STATIC
    src/common.c
    src/fs.c
    src/str.c
    src/pcm.c
    src/tags.c
    src/media.c
    src/cover.c
    src/frame.c
    src/playlist.c
    src/player.c
    src/ui.c
    src/audio/audio.c
    src/audio/audio_null.c
    ${SMP_PLATFORM_SOURCES}
)

target_include_directories(
    smp_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
)

target_link_libraries(smp_core PUBLIC smp_vendor)

if(WIN32)
    target_link_libraries(smp_core PUBLIC ole32)
else()
    target_compile_definitions(smp_core PRIVATE _POSIX_C_SOURCE=200809L)
    find_package(Threads REQUIRED)
    target_link_libraries(smp_core PUBLIC Threads::Threads m)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
    target_link_libraries(smp_core PRIVATE PkgConfig::ALSA)
endif()

smp_apply_warnings(smp_core)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(smp src/main.c)
target_link_libraries(smp PRIVATE smp_core)
smp_apply_warnings(smp)

if(WIN32)
    set_target_properties(smp PROPERTIES WIN32_EXECUTABLE OFF)
    if(MINGW)
        # wmain() needs the Unicode startup object supplied by -municode.
        target_link_options(smp PRIVATE -municode)
    endif()
endif()

install(TARGETS smp RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SMP_BUILD_TESTS)
    enable_testing()

    add_executable(
        smp_tests
        tests/test_main.c
        tests/test_support.c
        tests/test_common.c
        tests/test_str.c
        tests/test_fs.c
        tests/test_pcm.c
        tests/test_tags.c
        tests/test_media.c
        tests/test_playlist.c
        tests/test_frame.c
        tests/test_cover.c
        tests/test_player.c
    )
    target_include_directories(smp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(smp_tests PRIVATE smp_core)
    smp_apply_warnings(smp_tests)

    add_test(NAME unit COMMAND smp_tests)
endif()
