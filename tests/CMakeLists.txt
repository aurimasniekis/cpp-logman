include(GoogleTest)
include(FetchContent)

set(_logman_test_sources
    test_log_event.cpp
    test_listener_sink.cpp
    test_log_manager.cpp
    test_formatters.cpp
    test_env_levels.cpp
    test_threaded.cpp
)

# Tests-only dependency: nlohmann/json. The library itself stays
# json-agnostic; we fetch it here so the JSON test paths actually run in CI.
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    URL      https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    FIND_PACKAGE_ARGS 3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

if(TARGET nlohmann_json::nlohmann_json)
    list(APPEND _logman_test_sources
        test_json_formatter.cpp
        test_log_event_json.cpp
    )
endif()

add_executable(logman_tests ${_logman_test_sources})

target_link_libraries(logman_tests
    PRIVATE
        logman::logman
        GTest::gtest_main
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(logman_tests PRIVATE nlohmann_json::nlohmann_json)
endif()

logman_set_warnings(logman_tests PRIVATE)
logman_enable_sanitizers(logman_tests)
logman_enable_tsan(logman_tests)
logman_enable_coverage(logman_tests)

gtest_discover_tests(logman_tests)
