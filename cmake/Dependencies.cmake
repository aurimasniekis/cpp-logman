include_guard(GLOBAL)
include(FetchContent)

set(SPDLOG_BUILD_EXAMPLE   OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTS     OFF CACHE INTERNAL "")
set(SPDLOG_INSTALL         OFF CACHE INTERNAL "")
set(SPDLOG_FMT_EXTERNAL    OFF CACHE INTERNAL "")
FetchContent_Declare(
    spdlog
    URL      https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    FIND_PACKAGE_ARGS 1.17.0
)
FetchContent_MakeAvailable(spdlog)

if(LOGMAN_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE INTERNAL "")
    set(BUILD_GMOCK   OFF CACHE INTERNAL "")
    FetchContent_Declare(
        googletest
        URL      https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        FIND_PACKAGE_ARGS NAMES GTest
    )
    FetchContent_MakeAvailable(googletest)
endif()
