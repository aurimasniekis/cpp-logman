#pragma once

/// @file
/// @brief `LogManager` singleton — central registry of named spdlog channels
///        with lazy creation, per-channel and prefix-based level control,
///        listener-based capture, and structured `LogEvent` metadata.

#include <logman/env_prefixes.hpp>
#include <logman/formatters.hpp>
#include <logman/init_config.hpp>
#include <logman/listener_sink.hpp>
#include <logman/log_event.hpp>

#if __has_include(<nlohmann/json.hpp>)
#include <logman/json_formatter.hpp>
#include <logman/log_event_json.hpp>
#define LOGMAN_HAS_JSON 1
#else
#define LOGMAN_HAS_JSON 0
#endif

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
extern "C" char** environ;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

namespace logman {

inline constexpr bool has_json_support = LOGMAN_HAS_JSON == 1;

namespace detail {

/// Iterate the process environment, calling `cb(name, value)` for each entry.
template <typename Cb>
inline void enumerate_env(Cb&& cb) {
#if defined(_WIN32)
    auto* block = ::GetEnvironmentStringsW();
    if (block == nullptr) {
        return;
    }
    auto* cursor = block;
    while (*cursor != L'\0') {
        const std::wstring_view entry{cursor};
        const auto eq = entry.find(L'=');
        if (eq != std::wstring_view::npos && eq != 0) {
            std::string name;
            std::string value;
            name.reserve(eq);
            value.reserve(entry.size() - eq - 1);
            for (auto wc : entry.substr(0, eq)) {
                name.push_back(static_cast<char>(wc));
            }
            for (auto wc : entry.substr(eq + 1)) {
                value.push_back(static_cast<char>(wc));
            }
            cb(std::string_view{name}, std::string_view{value});
        }
        cursor += entry.size() + 1;
    }
    ::FreeEnvironmentStringsW(block);
#else
    if (environ == nullptr) {
        return;
    }
    for (char** env = environ; *env != nullptr; ++env) {
        const std::string_view entry{*env};
        if (const auto eq = entry.find('='); eq != std::string_view::npos && eq != 0) {
            cb(entry.substr(0, eq), entry.substr(eq + 1));
        }
    }
#endif
}

inline std::string ascii_lower(const std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

}  // namespace detail

/// Parse a level name (case-insensitive). Accepts the standard spdlog set:
/// `trace, debug, info, warn, warning, err, error, critical, off`. Returns
/// `nullopt` for anything else.
inline std::optional<spdlog::level::level_enum> parse_level_value(const std::string_view value) {
    const std::string lower = detail::ascii_lower(value);
    if (lower == "trace") {
        return spdlog::level::trace;
    }
    if (lower == "debug") {
        return spdlog::level::debug;
    }
    if (lower == "info") {
        return spdlog::level::info;
    }
    if (lower == "warn" || lower == "warning") {
        return spdlog::level::warn;
    }
    if (lower == "err" || lower == "error") {
        return spdlog::level::err;
    }
    if (lower == "critical") {
        return spdlog::level::critical;
    }
    if (lower == "off") {
        return spdlog::level::off;
    }
    return std::nullopt;
}

/// Convert the `<NAMESPACE>` part of an env var name (e.g. `ORG_FOO_MANAGER`)
/// to a channel-prefix string (e.g. `org.foo.manager`). Lowercases everything
/// and replaces underscores with dots; leading and trailing underscores
/// collapse to empty segments (acceptable: the result is still a valid
/// `starts_with` prefix).
inline std::string env_suffix_to_channel_prefix(const std::string_view suffix) {
    std::string out;
    out.reserve(suffix.size());
    for (const char c : suffix) {
        if (c == '_') {
            out.push_back('.');
        } else {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return out;
}

/// Central registry of named spdlog channels. Singleton — `instance()` is a
/// Meyer's static, ODR-safe across translation units in this header-only
/// build. Thread-safe: a `std::shared_mutex` guards the channel/level maps,
/// and listener invocation runs outside the lock via the snapshot pattern in
/// `ListenerSink`.
class LogManager {
public:
    using Listener = std::function<void(const LogEvent&)>;
    using ListenerId = std::uint64_t;

    static void initialize(const spdlog::level::level_enum default_level = spdlog::level::info) {
        InitConfig cfg;
        cfg.default_level = default_level;
        instance().init(cfg);
    }

    static void initialize(const InitConfig& cfg) {
        instance().init(cfg);
    }

    static void shutdown() {
        instance().shutdown_impl();
    }

    static std::shared_ptr<spdlog::logger> get(const std::string_view channel) {
        return instance().get_channel(channel);
    }

    static std::shared_ptr<spdlog::logger> get_or_null(const std::string_view channel) {
        auto& self = instance();
        const std::shared_lock lock(self.mutex_);
        if (const auto it = self.loggers_.find(std::string(channel)); it != self.loggers_.end()) {
            return it->second;
        }
        return nullptr;
    }

    static std::unordered_map<std::string, spdlog::level::level_enum> channels() {
        auto& self = instance();
        const std::shared_lock lock(self.mutex_);
        std::unordered_map<std::string, spdlog::level::level_enum> out;
        out.reserve(self.loggers_.size());
        for (const auto& [name, logger] : self.loggers_) {
            out.emplace(name, logger->level());
        }
        return out;
    }

    static bool set_level(const std::string_view channel, const spdlog::level::level_enum lvl) {
        auto& self = instance();
        const std::shared_lock lock(self.mutex_);
        const auto it = self.loggers_.find(std::string(channel));
        if (it == self.loggers_.end()) {
            return false;
        }
        it->second->set_level(lvl);
        return true;
    }

    static void set_levels_by_prefix(const std::string_view prefix,
                                     const spdlog::level::level_enum lvl) {
        auto& self = instance();
        const std::unique_lock lock(self.mutex_);
        for (auto& [name, lg] : self.loggers_) {
            if (name.starts_with(prefix)) {
                lg->set_level(lvl);
            }
        }
        self.prefixed_levels_[std::string(prefix)] = lvl;
    }

    static void set_all_levels(const spdlog::level::level_enum lvl) {
        auto& self = instance();
        const std::unique_lock lock(self.mutex_);
        for (const auto& lg : self.loggers_ | std::views::values) {
            lg->set_level(lvl);
        }
        self.default_level_ = lvl;
        spdlog::set_level(lvl);
    }

    static void set_default_level(const spdlog::level::level_enum lvl) {
        auto& self = instance();
        const std::unique_lock lock(self.mutex_);
        self.default_level_ = lvl;
        spdlog::set_level(lvl);
    }

    static spdlog::level::level_enum default_level() {
        const auto& self = instance();
        const std::shared_lock lock(self.mutex_);
        return self.default_level_;
    }

    static void set_pattern(std::string pattern) {
        auto& self = instance();
        const std::unique_lock lock(self.mutex_);
        self.pattern_ = std::move(pattern);
        const auto formatter = self.make_console_formatter();
        if (self.console_sink_) {
            self.console_sink_->set_formatter(formatter->clone());
        }
        for (const auto& lg : self.loggers_ | std::views::values) {
            lg->set_pattern(self.pattern_);
        }
    }

    static void flush() {
        auto& self = instance();
        const std::shared_lock lock(self.mutex_);
        for (const auto& lg : self.loggers_ | std::views::values) {
            lg->flush();
        }
    }

    static void set_flush_on(spdlog::level::level_enum lvl) {
        auto& self = instance();
        const std::shared_lock lock(self.mutex_);
        for (const auto& lg : self.loggers_ | std::views::values) {
            lg->flush_on(lvl);
        }
        self.flush_on_ = lvl;
    }

    static void add_sink(const spdlog::sink_ptr& sink) {
        auto& self = instance();
        const std::unique_lock lock(self.mutex_);
        self.sinks_.push_back(sink);
        for (const auto& lg : self.loggers_ | std::views::values) {
            lg->sinks().push_back(sink);
        }
    }

    static bool remove_sink(const spdlog::sink_ptr& sink) {
        auto& self = instance();
        const std::unique_lock lock(self.mutex_);
        const auto before = self.sinks_.size();
        std::erase(self.sinks_, sink);
        if (self.sinks_.size() == before) {
            return false;
        }
        for (const auto& lg : self.loggers_ | std::views::values) {
            auto& s = lg->sinks();
            std::erase(s, sink);
        }
        return true;
    }

    static ListenerId add_listener(Listener l) {
        const auto& self = instance();
        return self.listener_sink_->add_listener(std::move(l));
    }

    static bool remove_listener(const ListenerId id) {
        return instance().listener_sink_->remove_listener(id);
    }

    static void clear_listeners() {
        instance().listener_sink_->clear_listeners();
    }

    static std::shared_ptr<ListenerSink> listener_sink() {
        return instance().listener_sink_;
    }

private:
    LogManager() = default;

    static LogManager& instance() {
        static LogManager inst;
        return inst;
    }

    friend void detail_reset_for_testing();

    std::unique_ptr<spdlog::pattern_formatter> make_console_formatter() const {
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<UpperLevelFormatter>('L');
        formatter->add_flag<ChannelNameFormatter>('n');
        formatter->set_pattern(pattern_);
        return formatter;
    }

    void apply_env(InitConfig& cfg) {
        if (!cfg.read_env) {
            return;
        }
        std::vector<std::string> prefixes = cfg.env_prefixes;
        if (prefixes.empty()) {
            for (auto p : default_env_prefixes) {
                prefixes.emplace_back(p);
            }
        }
        for (const auto& prefix : prefixes) {
            detail::enumerate_env([&](const std::string_view name, const std::string_view value) {
                if (!name.starts_with(prefix)) {
                    return;
                }
                if (const std::string_view rest = name.substr(prefix.size()); rest == "LEVEL") {
                    if (const auto lvl = parse_level_value(value)) {
                        cfg.default_level = *lvl;
                    } else {
                        std::cerr << "logman: ignoring " << name << '=' << value
                                  << " (unknown level)\n";
                    }
                } else if (rest.starts_with("LEVEL_")) {
                    const std::string_view ns = rest.substr(6);
                    if (const auto lvl = parse_level_value(value)) {
                        env_prefix_overrides_[env_suffix_to_channel_prefix(ns)] = *lvl;
                    } else {
                        std::cerr << "logman: ignoring " << name << '=' << value
                                  << " (unknown level)\n";
                    }
                } else if (rest == "FORMAT") {
                    if (const std::string lower = detail::ascii_lower(value); lower == "text") {
                        cfg.structured_json = false;
                    } else if (lower == "json") {
                        cfg.structured_json = true;
                    } else {
                        std::cerr << "logman: ignoring " << name << '=' << value
                                  << " (unknown format; want text|json)\n";
                    }
                }
            });
        }
    }

    void init(InitConfig cfg) {
        std::call_once(init_flag_, [&] {
            apply_env(cfg);
            pattern_ = cfg.pattern.empty() ? std::string(default_pattern) : cfg.pattern;
            default_level_ = cfg.default_level;

            listener_sink_ = std::make_shared<ListenerSink>();
            listener_sink_->set_level(spdlog::level::trace);
            listener_sink_->set_formatter(std::make_unique<spdlog::pattern_formatter>(
                "%v", spdlog::pattern_time_type::local, std::string{}));

            if (cfg.enable_console) {
                console_sink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink_->set_level(spdlog::level::trace);

                if (cfg.structured_json) {
#if LOGMAN_HAS_JSON
                    console_sink_->set_formatter(std::make_unique<JsonFormatter>());
#else
                    std::cerr << "logman: structured_json requested but library compiled "
                                 "without <nlohmann/json.hpp>; falling back to text\n";
                    console_sink_->set_formatter(make_console_formatter());
#endif
                } else {
                    console_sink_->set_formatter(make_console_formatter());
                }
                sinks_.push_back(console_sink_);
            }
            sinks_.push_back(listener_sink_);

            for (const auto& [pfx, lvl] : env_prefix_overrides_) {
                prefixed_levels_[pfx] = lvl;
            }

            auto main = std::make_shared<spdlog::logger>("main", sinks_.begin(), sinks_.end());
            main->set_level(default_level_);
            for (const auto& [pfx, lvl] : prefixed_levels_) {
                if (std::string_view("main").starts_with(pfx)) {
                    main->set_level(lvl);
                }
            }
            spdlog::register_logger(main);
            loggers_.emplace("main", main);
            if (cfg.set_as_spdlog_default) {
                spdlog::set_default_logger(main);
            }
            spdlog::set_level(default_level_);

            initialized_ = true;
        });
    }

    void shutdown_impl() {
        const std::unique_lock lock(mutex_);
        loggers_.clear();
        sinks_.clear();
        prefixed_levels_.clear();
        env_prefix_overrides_.clear();
        listener_sink_.reset();
        console_sink_.reset();
        initialized_ = false;
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::logger> get_channel(const std::string_view channel) {
        if (!initialized_) {
            initialize();
        }

        const std::string name(channel);
        {
            const std::shared_lock lock(mutex_);
            if (const auto it = loggers_.find(name); it != loggers_.end()) {
                return it->second;
            }
        }

        const std::unique_lock lock(mutex_);
        if (const auto it = loggers_.find(name); it != loggers_.end()) {
            return it->second;
        }

        auto logger = std::make_shared<spdlog::logger>(name, sinks_.begin(), sinks_.end());
        logger->set_level(default_level_);
        if (flush_on_) {
            logger->flush_on(*flush_on_);
        }
        for (const auto& [pfx, lvl] : prefixed_levels_) {
            if (name.starts_with(pfx)) {
                logger->set_level(lvl);
            }
        }
        spdlog::register_logger(logger);
        loggers_.emplace(name, logger);
        return logger;
    }

    mutable std::shared_mutex mutex_;
    std::once_flag init_flag_;
    bool initialized_ = false;
    std::string pattern_;
    std::shared_ptr<ListenerSink> listener_sink_;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink_;
    std::vector<spdlog::sink_ptr> sinks_;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
    std::unordered_map<std::string, spdlog::level::level_enum> prefixed_levels_;
    std::unordered_map<std::string, spdlog::level::level_enum> env_prefix_overrides_;
    std::optional<spdlog::level::level_enum> flush_on_;
    spdlog::level::level_enum default_level_ = spdlog::level::info;
};

// -----------------------------------------------------------------------------
// Free-function shortcuts. Each forwards to the corresponding LogManager
// static method — same arguments, same return type. Lets consumers write
// `logman::get("net")` instead of `logman::LogManager::get("net")`.
// -----------------------------------------------------------------------------

inline void initialize(const spdlog::level::level_enum lvl = spdlog::level::info) {
    LogManager::initialize(lvl);
}

inline void initialize(const InitConfig& cfg) {
    LogManager::initialize(cfg);
}

inline void shutdown() {
    LogManager::shutdown();
}

inline std::shared_ptr<spdlog::logger> get(const std::string_view channel) {
    return LogManager::get(channel);
}

inline std::shared_ptr<spdlog::logger> get_or_null(const std::string_view channel) {
    return LogManager::get_or_null(channel);
}

inline std::unordered_map<std::string, spdlog::level::level_enum> channels() {
    return LogManager::channels();
}

inline bool set_level(const std::string_view channel, const spdlog::level::level_enum lvl) {
    return LogManager::set_level(channel, lvl);
}

inline void set_levels_by_prefix(const std::string_view prefix,
                                 const spdlog::level::level_enum lvl) {
    LogManager::set_levels_by_prefix(prefix, lvl);
}

inline void set_all_levels(const spdlog::level::level_enum lvl) {
    LogManager::set_all_levels(lvl);
}

inline void set_default_level(const spdlog::level::level_enum lvl) {
    LogManager::set_default_level(lvl);
}

inline spdlog::level::level_enum default_level() {
    return LogManager::default_level();
}

inline void set_pattern(std::string pattern) {
    LogManager::set_pattern(std::move(pattern));
}

inline void flush() {
    LogManager::flush();
}

inline void set_flush_on(const spdlog::level::level_enum lvl) {
    LogManager::set_flush_on(lvl);
}

inline void add_sink(const spdlog::sink_ptr& sink) {
    LogManager::add_sink(sink);
}

inline bool remove_sink(const spdlog::sink_ptr& sink) {
    return LogManager::remove_sink(sink);
}

inline LogManager::ListenerId add_listener(LogManager::Listener l) {
    return LogManager::add_listener(std::move(l));
}

inline bool remove_listener(const LogManager::ListenerId id) {
    return LogManager::remove_listener(id);
}

inline void clear_listeners() {
    LogManager::clear_listeners();
}

inline std::shared_ptr<ListenerSink> listener_sink() {
    return LogManager::listener_sink();
}

namespace detail {

/// Tests-only: tear down the singleton state. Drops registered loggers, clears
/// sinks and listeners, and resets the once-flag so the next `initialize()`
/// re-runs.
inline void reset_log_manager_for_testing() {
    LogManager::shutdown();
    spdlog::drop_all();
}

}  // namespace detail

}  // namespace logman
