#pragma once

/// @file
/// @brief `ListenerSink` — spdlog sink that dispatches each log record to
///        registered listener callbacks as `LogEvent` values.

#include <logman/log_event.hpp>

#if defined(_WIN32)
#include <process.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <ranges>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace logman {

namespace detail {

/// Platform-portable PID accessor. Defined `inline` (not `static`) so the
/// linker dedups across translation units in this header-only library.
inline int current_pid() noexcept {
#if defined(_WIN32)
    return _getpid();
#elif defined(__unix__) || defined(__APPLE__)
    return static_cast<int>(::getpid());
#else
    return -1;
#endif
}

/// Build a `LogEvent` from a spdlog `log_msg`. The `formatted_message` view
/// becomes the event's `message` field — callers format with their own
/// formatter and pass the rendered output here.
inline LogEvent make_log_event(const spdlog::details::log_msg& msg,
                               const std::string_view formatted_message) {
    LogEvent event{};
    event.timestamp = msg.time;
    const auto level_sv = spdlog::level::to_string_view(msg.level);
    event.level.assign(level_sv.data(), level_sv.size());
    event.channel.assign(msg.logger_name.data(), msg.logger_name.size());
    event.thread_id = static_cast<std::uint64_t>(msg.thread_id);
    if (const int pid = current_pid(); pid >= 0) {
        event.process_id = pid;
    }
    if (msg.source.filename != nullptr && msg.source.filename[0] != '\0') {
        event.file = msg.source.filename;
        event.line = static_cast<int>(msg.source.line);
        if (msg.source.funcname != nullptr) {
            event.function = msg.source.funcname;
        }
    }
    event.message.assign(formatted_message.data(), formatted_message.size());
    return event;
}

}  // namespace detail

/// spdlog sink that dispatches every record to a list of user-registered
/// listener callbacks as `LogEvent` values. Listeners run *outside* the sink
/// lock (snapshot-then-invoke) and each invocation is guarded by a try/catch
/// so one misbehaving listener can't crash or stall the pipeline.
class ListenerSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    using listener_t = std::function<void(const LogEvent&)>;
    using listener_id = std::uint64_t;

    ~ListenerSink() override = default;

    listener_id add_listener(listener_t listener) {
        const std::scoped_lock lock(listeners_mutex_);
        const auto id = ++listener_id_gen_;
        listeners_.emplace(id, std::move(listener));
        return id;
    }

    bool remove_listener(const listener_id id) {
        const std::scoped_lock lock(listeners_mutex_);
        return listeners_.erase(id) > 0;
    }

    void clear_listeners() {
        const std::scoped_lock lock(listeners_mutex_);
        listeners_.clear();
    }

    /// Snapshot of current listener count — primarily for tests.
    std::size_t listener_count() const {
        const std::scoped_lock lock(listeners_mutex_);
        return listeners_.size();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        const LogEvent event =
            detail::make_log_event(msg, std::string_view(formatted.data(), formatted.size()));

        std::vector<listener_t> snapshot;
        {
            const std::scoped_lock lock(listeners_mutex_);
            snapshot.reserve(listeners_.size());
            for (const auto& l : listeners_ | std::views::values) {
                snapshot.push_back(l);
            }
        }

        for (auto& listener : snapshot) {
            try {
                listener(event);
            } catch (const std::exception& e) {
#ifndef NDEBUG
                std::cerr << "logman: listener threw " << typeid(e).name() << ": " << e.what()
                          << '\n';
#else
                (void)e;
#endif
            } catch (...) {
#ifndef NDEBUG
                std::cerr << "logman: listener threw non-std::exception\n";
#endif
            }
        }
    }

    void flush_() override {}

private:
    std::atomic<listener_id> listener_id_gen_{0};
    std::unordered_map<listener_id, listener_t> listeners_;
    mutable std::mutex listeners_mutex_;
};

}  // namespace logman
