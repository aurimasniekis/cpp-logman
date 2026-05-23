include(FetchContent)

# Examples opt into nlohmann/json if it's already available via the tests
# subdirectory (FetchContent_MakeAvailable is cached) or via a system package.
if(NOT TARGET nlohmann_json::nlohmann_json)
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
endif()

function(logman_add_example target src)
    add_executable(${target} ${src})
    target_link_libraries(${target} PRIVATE logman::logman)
    if(TARGET nlohmann_json::nlohmann_json)
        target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json)
    endif()
    logman_set_warnings(${target} PRIVATE)
    logman_enable_sanitizers(${target})
    logman_enable_tsan(${target})
endfunction()

logman_add_example(logman_basic_usage       basic_usage.cpp)
logman_add_example(logman_multi_channel     multi_channel.cpp)
logman_add_example(logman_listener_callback listener_callback.cpp)
logman_add_example(logman_env_levels        env_levels.cpp)
logman_add_example(logman_custom_sink       custom_sink.cpp)

if(TARGET nlohmann_json::nlohmann_json)
    logman_add_example(logman_structured_json structured_json.cpp)
    logman_add_example(logman_json_event      json_event.cpp)
endif()
