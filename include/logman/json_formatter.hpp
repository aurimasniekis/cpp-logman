#pragma once

/// @file
/// @brief spdlog formatter that emits one JSON object per log record. Gated
///        by `__has_include(<nlohmann/json.hpp>)` in the umbrella header.

#include <logman/listener_sink.hpp>
#include <logman/log_event.hpp>
#include <logman/log_event_json.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/formatter.h>
#include <spdlog/pattern_formatter.h>

#include <memory>
#include <string>

namespace logman {

/// spdlog formatter that builds a `LogEvent` from the record, dumps it as
/// compact JSON via `to_json`, and appends a trailing newline. Plays the role
/// of "single-line structured output" — one object per line, no pretty
/// printing.
class JsonFormatter final : public spdlog::formatter {
public:
    JsonFormatter() : message_formatter_("%v", spdlog::pattern_time_type::local, std::string{}) {}

    void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override {
        spdlog::memory_buf_t message_buf;
        message_formatter_.format(msg, message_buf);
        const LogEvent event =
            detail::make_log_event(msg, std::string_view(message_buf.data(), message_buf.size()));

        const nlohmann::json j = event;
        const std::string out = j.dump();
        spdlog::details::fmt_helper::append_string_view(out, dest);
        dest.push_back('\n');
    }

    [[nodiscard]] std::unique_ptr<spdlog::formatter> clone() const override {
        return std::make_unique<JsonFormatter>();
    }

private:
    spdlog::pattern_formatter message_formatter_;
};

}  // namespace logman
