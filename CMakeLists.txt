cmake_minimum_required(VERSION 3.25)

project(logman
    VERSION     0.1.0
    DESCRIPTION "Modern C++23 header-only logging manager wrapping spdlog with channels, listeners, and structured events"
    LANGUAGES   CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(LOGMAN_BUILD_TESTS         "Build logman tests"                  ${PROJECT_IS_TOP_LEVEL})
option(LOGMAN_BUILD_EXAMPLES      "Build logman examples"               ${PROJECT_IS_TOP_LEVEL})
option(LOGMAN_BUILD_DOCS          "Build Doxygen API documentation"     OFF)
option(LOGMAN_ENABLE_CLANG_TIDY   "Run clang-tidy during build"         OFF)
option(LOGMAN_ENABLE_SANITIZERS   "Build with ASan + UBSan (Debug)"     OFF)
option(LOGMAN_ENABLE_TSAN         "Build with ThreadSanitizer (Debug)"  OFF)
option(LOGMAN_ENABLE_COVERAGE     "Build with Clang source-based coverage" OFF)
option(LOGMAN_WARNINGS_AS_ERRORS  "Treat compiler warnings as errors"   ${PROJECT_IS_TOP_LEVEL})
option(LOGMAN_INSTALL             "Generate install rules for logman"   ${PROJECT_IS_TOP_LEVEL})

set(LOGMAN_ENV_PREFIXES "" CACHE STRING
    "Extra env-var prefixes to scan during LogManager::initialize() (semicolon list). \
LOGMAN_ is always included unconditionally.")

# Compose the compile-time prefix list: LOGMAN_ first, then any user-supplied
# entries in declaration order (later entries override earlier ones at runtime).
set(_logman_prefixes "LOGMAN_")
foreach(_p IN LISTS LOGMAN_ENV_PREFIXES)
    if(NOT _p STREQUAL "" AND NOT _p STREQUAL "LOGMAN_")
        list(APPEND _logman_prefixes "${_p}")
    endif()
endforeach()
string(JOIN ";" LOGMAN_DEFAULT_ENV_PREFIXES_RAW ${_logman_prefixes})
message(STATUS "logman: env prefixes = ${LOGMAN_DEFAULT_ENV_PREFIXES_RAW}")

include(CompilerWarnings)
include(Sanitizers)
include(ThreadSanitizer)
include(Coverage)
include(Dependencies)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/logman/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/logman/version.hpp"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/logman/env_prefixes.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/logman/env_prefixes.hpp"
    @ONLY
)

add_library(logman INTERFACE)
add_library(logman::logman ALIAS logman)

target_include_directories(logman
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(logman INTERFACE cxx_std_23)

# Use spdlog's header-only target to keep the chain header-only. Consumers
# who prefer the compiled `spdlog::spdlog` target can swap it via their own
# target_link_libraries.
target_link_libraries(logman INTERFACE spdlog::spdlog_header_only)

if(LOGMAN_ENABLE_CLANG_TIDY)
    find_program(LOGMAN_CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
    set(_logman_tidy_cmd
        "${LOGMAN_CLANG_TIDY_EXE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy"
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -print-search-dirs
            OUTPUT_VARIABLE _logman_gcc_search_dirs
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_logman_gcc_search_dirs MATCHES "install: ([^\n]+)")
            list(APPEND _logman_tidy_cmd
                "--extra-arg-before=--gcc-install-dir=${CMAKE_MATCH_1}")
            message(STATUS
                "logman: pinning clang-tidy to gcc install dir ${CMAKE_MATCH_1}")
        endif()
    endif()
    set(CMAKE_CXX_CLANG_TIDY "${_logman_tidy_cmd}")
    message(STATUS "logman: clang-tidy enabled (${LOGMAN_CLANG_TIDY_EXE})")
endif()

if(LOGMAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOGMAN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LOGMAN_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    set(LOGMAN_DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )
    add_custom_target(logman_docs
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating Doxygen documentation"
        VERBATIM
    )
endif()

if(LOGMAN_INSTALL AND DEFINED spdlog_SOURCE_DIR)
    message(STATUS
        "logman: LOGMAN_INSTALL disabled — a fetched spdlog cannot be re-exported. "
        "Install spdlog via system package or find_package to re-enable install rules."
    )
    set(LOGMAN_INSTALL OFF)
endif()

if(LOGMAN_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS logman
        EXPORT  logmanTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/logman
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/include/logman/version.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/include/logman/env_prefixes.hpp"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/logman
    )

    install(EXPORT logmanTargets
        FILE        logmanTargets.cmake
        NAMESPACE   logman::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logman
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/logmanConfigVersion.cmake"
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/logmanConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/logmanConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logman
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/logmanConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/logmanConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logman
    )
endif()
