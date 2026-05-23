#pragma once

/// @file
/// @brief Plain-old-data structure describing one log record. Decoupled from
///        any JSON dependency — see `log_event_json.hpp` for the optional
///        nlohmann/json adapter.

#include <chrono>
#include <cstdint>
#include <string>

namespace logman {

/// One log record, captured by `ListenerSink` and (optionally) emitted as JSON
/// by `JsonFormatter`. Fields mirror what spdlog gives us in `log_msg`.
struct LogEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string level;
    std::string channel;
    std::string message;
    std::uint64_t thread_id = 0;
    int process_id = -1;
    std::string file;
    int line = 0;
    std::string function;
};

}  // namespace logman
