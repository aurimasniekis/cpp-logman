#pragma once

/// @file
/// @brief nlohmann/json adapter for `LogEvent`. Included unconditionally —
///        the umbrella header gates it via `__has_include`.

#include <logman/log_event.hpp>

#include <nlohmann/json.hpp>

#include <chrono>

namespace logman {

inline void to_json(nlohmann::json& j, const LogEvent& e) {
    const auto ts =
        std::chrono::duration_cast<std::chrono::milliseconds>(e.timestamp.time_since_epoch())
            .count();
    j = nlohmann::json{
        {"timestamp", ts},
        {"level", e.level},
        {"channel", e.channel},
        {"message", e.message},
        {"thread_id", e.thread_id},
    };
    if (e.process_id >= 0) {
        j["process_id"] = e.process_id;
    }
    if (!e.file.empty()) {
        j["file"] = e.file;
        j["line"] = e.line;
        j["function"] = e.function;
    }
}

}  // namespace logman
