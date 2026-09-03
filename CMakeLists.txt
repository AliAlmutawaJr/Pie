# Bundled libffi is intentionally built outside the project tree because
# libffi's configure script rejects source directories containing spaces.

cmake_minimum_required(VERSION 3.21)

project(Pie LANGUAGES C CXX)

# for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(ExternalProject)

# ============================================================================
# Header-only dependencies
# ============================================================================

# These are header-only dependencies for Pie. We only want FetchContent to
# download them. We deliberately do NOT configure their own CMake projects.
#
# cpp-std-extensions has its own dependency manager and pins MP11 internally.
# Configuring it as a subproject causes a dependency-version conflict.
FetchContent_Declare(
    mp11
    GIT_REPOSITORY https://github.com/boostorg/mp11.git
    GIT_TAG boost-1.83.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR __pie_do_not_configure__
)

FetchContent_Declare(
    cpp_std_extensions
    GIT_REPOSITORY https://github.com/intel/cpp-std-extensions.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR __pie_do_not_configure__
)

FetchContent_MakeAvailable(mp11 cpp_std_extensions)

# ============================================================================
# libffi
# ============================================================================

set(LIBFFI_VERSION "3.7.1")

# libffi is built outside CMake, so on macOS we must explicitly make its
# architecture match the architecture CMake uses for Pie.
set(LIBFFI_CONFIGURE_ARCH_ARGS)
set(LIBFFI_ARCH_ENV)

if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES)
        list(LENGTH CMAKE_OSX_ARCHITECTURES PIE_OSX_ARCH_COUNT)
        if(NOT PIE_OSX_ARCH_COUNT EQUAL 1)
            message(FATAL_ERROR
                "Bundled libffi currently supports one macOS architecture per build. "
                "CMAKE_OSX_ARCHITECTURES='${CMAKE_OSX_ARCHITECTURES}'")
        endif()
        list(GET CMAKE_OSX_ARCHITECTURES 0 PIE_OSX_ARCH)
    else()
        execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE PIE_OSX_ARCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    if(PIE_OSX_ARCH STREQUAL "arm64")
        set(LIBFFI_HOST "aarch64-apple-darwin")
    elseif(PIE_OSX_ARCH STREQUAL "x86_64")
        set(LIBFFI_HOST "x86_64-apple-darwin")
    else()
        message(FATAL_ERROR "Unsupported macOS architecture for bundled libffi: ${PIE_OSX_ARCH}")
    endif()

    set(LIBFFI_CONFIGURE_ARCH_ARGS "--host=${LIBFFI_HOST}")
    set(LIBFFI_ARCH_ENV
        "CFLAGS=-arch ${PIE_OSX_ARCH}"
        "CXXFLAGS=-arch ${PIE_OSX_ARCH}"
        "LDFLAGS=-arch ${PIE_OSX_ARCH}"
    )

    message(STATUS "Pie macOS architecture: ${PIE_OSX_ARCH}")
endif()

# Include architecture/compiler target in the key so an arm64 libffi archive
# can never be accidentally reused by an x86_64 Pie build (or vice versa).
string(SHA256 LIBFFI_BUILD_HASH
    "${CMAKE_SOURCE_DIR}-${LIBFFI_VERSION}-${CMAKE_OSX_ARCHITECTURES}-${CMAKE_C_COMPILER_TARGET}-${CMAKE_CXX_COMPILER_TARGET}-${CMAKE_SYSTEM_PROCESSOR}")
string(SUBSTRING "${LIBFFI_BUILD_HASH}" 0 16 LIBFFI_BUILD_ID)

set(LIBFFI_ROOT        "/tmp/pie-libffi-${LIBFFI_BUILD_ID}")
set(LIBFFI_DOWNLOAD_DIR "${LIBFFI_ROOT}/download")
set(LIBFFI_SOURCE_DIR   "${LIBFFI_ROOT}/source")
set(LIBFFI_BINARY_DIR   "${LIBFFI_ROOT}/build")
set(LIBFFI_INSTALL_DIR  "${LIBFFI_ROOT}/install")
set(LIBFFI_STAMP_DIR    "${LIBFFI_ROOT}/stamp")
set(LIBFFI_TMP_DIR      "${LIBFFI_ROOT}/tmp")

set(LIBFFI_INCLUDE_DIR "${LIBFFI_INSTALL_DIR}/include")
set(LIBFFI_LIBRARY     "${LIBFFI_INSTALL_DIR}/lib/libffi.a")

# Imported targets require their include directory to exist during configure.
file(MAKE_DIRECTORY "${LIBFFI_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${LIBFFI_INSTALL_DIR}/lib")

find_program(MAKE_EXECUTABLE NAMES gmake make REQUIRED)

message(STATUS "PIE_CMAKE_V6")
message(STATUS "libffi source:  ${LIBFFI_SOURCE_DIR}")
message(STATUS "libffi build:   ${LIBFFI_BINARY_DIR}")
message(STATUS "libffi install: ${LIBFFI_INSTALL_DIR}")

ExternalProject_Add(
    libffi_external

    URL
        "https://github.com/libffi/libffi/releases/download/v${LIBFFI_VERSION}/libffi-${LIBFFI_VERSION}.tar.gz"

    DOWNLOAD_EXTRACT_TIMESTAMP TRUE

    # IMPORTANT: none of these paths may be under CMAKE_BINARY_DIR because
    # the user's project path may contain spaces.
    PREFIX       "${LIBFFI_ROOT}"
    DOWNLOAD_DIR "${LIBFFI_DOWNLOAD_DIR}"
    SOURCE_DIR   "${LIBFFI_SOURCE_DIR}"
    BINARY_DIR   "${LIBFFI_BINARY_DIR}"
    INSTALL_DIR  "${LIBFFI_INSTALL_DIR}"
    STAMP_DIR    "${LIBFFI_STAMP_DIR}"
    TMP_DIR      "${LIBFFI_TMP_DIR}"

    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -E env
            "CC=${CMAKE_C_COMPILER}"
            "CXX=${CMAKE_CXX_COMPILER}"
            ${LIBFFI_ARCH_ENV}
            <SOURCE_DIR>/configure
                ${LIBFFI_CONFIGURE_ARCH_ARGS}
                --prefix=<INSTALL_DIR>
                --includedir=<INSTALL_DIR>/include
                --libdir=<INSTALL_DIR>/lib
                --disable-shared
                --enable-static
                --disable-docs

    BUILD_COMMAND
        "${MAKE_EXECUTABLE}"

    INSTALL_COMMAND
        "${MAKE_EXECUTABLE}" install

    BUILD_BYPRODUCTS
        "${LIBFFI_LIBRARY}"
)

add_library(ffi STATIC IMPORTED GLOBAL)
set_target_properties(ffi PROPERTIES
    IMPORTED_LOCATION "${LIBFFI_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBFFI_INCLUDE_DIR}"
)
add_dependencies(ffi libffi_external)

# ============================================================================
# Pie sources
# ============================================================================

file(GLOB PIE_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/Lex/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/Parser/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/Analysis/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/Interp/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/Utils/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/CLI/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/Preprocessor/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/Type/*.cxx"
    "${PROJECT_SOURCE_DIR}/src/Value/*.cxx"
)

set(PIE_INCLUDE_DIRS
    "${PROJECT_SOURCE_DIR}/src"
    "${mp11_SOURCE_DIR}/include"
    "${cpp_std_extensions_SOURCE_DIR}/include"
)

function(configure_pie_target target)
    target_include_directories(${target} PRIVATE ${PIE_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ffi)

    # Make the ordering explicit: libffi must be installed before this target
    # can link against the imported static library.
    add_dependencies(${target} libffi_external)

    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wextra>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wpedantic>
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-missing-braces>
    )
endfunction()

function(enable_sanitizers target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -fsanitize=undefined
            -fsanitize=address
        )
        target_link_options(${target} PRIVATE
            -fsanitize=undefined
            -fsanitize=address
        )
    endif()
endfunction()

# ============================================================================
# Default / release build
# ============================================================================

add_executable(Pie
    src/main.cc
    ${PIE_SOURCES}
)

configure_pie_target(Pie)
target_compile_definitions(Pie PRIVATE NO_ERR_LOC)
target_compile_options(Pie PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O2>
)

# Compatibility with the old Makefile target name.
add_custom_target(main DEPENDS Pie)

# ============================================================================
# Debug: make debug
# ============================================================================

add_executable(Pie_debug EXCLUDE_FROM_ALL
    src/main.cc
    ${PIE_SOURCES}
)

configure_pie_target(Pie_debug)
target_compile_options(Pie_debug PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O0>
)
enable_sanitizers(Pie_debug)

add_custom_target(debug DEPENDS Pie_debug)

# ============================================================================
# Tests: make test
# ============================================================================

add_executable(run_tests EXCLUDE_FROM_ALL
    Tests/Test.cc
    Tests/catch.cpp
    ${PIE_SOURCES}
)

configure_pie_target(run_tests)
target_compile_options(run_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O0>
)
enable_sanitizers(run_tests)

add_custom_target(test
    COMMAND $<TARGET_FILE:run_tests>
    DEPENDS run_tests
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL
)

# ============================================================================
# Web: make web
# emcc flags intentionally kept exactly as in the original Makefile.
# ============================================================================

find_program(EMCC_EXECUTABLE emcc)

if(EMCC_EXECUTABLE)
    add_custom_target(web
        COMMAND "${EMCC_EXECUTABLE}"
            -sWASM=1
            -sFORCE_FILESYSTEM
            "-sEXPORTED_RUNTIME_METHODS=[\"callMain\", \"ccall\"]"
            -sASSERTIONS
            -sENVIRONMENT=web
            -sSTACK_SIZE=8388608
            -INVOKE_RUN_AT_START=0
            -sEXIT_RUNTIME=0
            -sNO_DISABLE_EXCEPTION_CATCHING
            -std=c++23
            "-I${PROJECT_SOURCE_DIR}/src"
            "-I${mp11_SOURCE_DIR}/include/"
            "-I${cpp_std_extensions_SOURCE_DIR}/include/"
            -O2
            "${PROJECT_SOURCE_DIR}/src/main.cc"
            ${PIE_SOURCES}
            -o "${CMAKE_BINARY_DIR}/Pie.js"
            -DWEB_PIE
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM
    )
else()
    add_custom_target(web
        COMMAND ${CMAKE_COMMAND} -E echo "emcc was not found in PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        USES_TERMINAL
    )
endif()

# ============================================================================
# Test FFI dynamic library + GitHub Actions
# ============================================================================

# The tests open "../Tests/dylib" directly, so the library must be emitted
# exactly as Tests/dylib (no "lib" prefix and no .dylib/.so suffix).
add_library(ffi_test_dylib SHARED EXCLUDE_FROM_ALL
    Tests/ffi_test.cpp
)

set_target_properties(ffi_test_dylib PROPERTIES
    PREFIX ""
    SUFFIX ""
    OUTPUT_NAME "dylib"

    # LIBRARY_OUTPUT_DIRECTORY is used by macOS/Linux shared libraries.
    # RUNTIME_OUTPUT_DIRECTORY also keeps the runtime artifact in Tests on
    # platforms where shared libraries are treated as runtime targets.
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/Tests"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/Tests"
)

if(APPLE)
    # Match the original Makefile:
    #   -dynamiclib -arch arm64 -arch x86_64
    #
    # SHARED already makes CMake use -dynamiclib on macOS. This property makes
    # only the test dylib universal; Pie/libffi can still use one architecture.
    set_target_properties(ffi_test_dylib PROPERTIES
        OSX_ARCHITECTURES "arm64;x86_64"
    )

    add_custom_target(test_dylib_mac
        DEPENDS ffi_test_dylib
    )

    set(PIE_TEST_DYLIB_TARGET test_dylib_mac)

elseif(UNIX)
    # SHARED already uses -shared; make PIC explicit to match the Makefile's "-fPIC -shared"
    set_target_properties(ffi_test_dylib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    add_custom_target(test_dylib_lnx
        DEPENDS ffi_test_dylib
    )

    set(PIE_TEST_DYLIB_TARGET test_dylib_lnx)

else()
    # Keep the library buildable on other platforms
    set(PIE_TEST_DYLIB_TARGET ffi_test_dylib)
endif()

add_executable(run_tests_gh EXCLUDE_FROM_ALL
    Tests/Test.cc
    Tests/catch.cpp
    ${PIE_SOURCES}
)

configure_pie_target(run_tests_gh)
target_compile_options(run_tests_gh PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O0>
)

add_custom_target(gh-actions
    COMMAND $<TARGET_FILE:run_tests_gh>
    DEPENDS ${PIE_TEST_DYLIB_TARGET} run_tests_gh
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    USES_TERMINAL
)
