#pragma once

/// @file
/// @brief Configuration struct consumed by `LogManager::initialize()`.

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace logman {

/// Settings applied during `LogManager::initialize(InitConfig)`. All fields
/// have sensible defaults; the no-arg `initialize()` overload uses the level
/// argument and leaves the rest at the defaults below.
struct InitConfig {
    /// Default level applied to the root logger and every channel created
    /// without an explicit prefix rule. Env var `<PREFIX>LEVEL` overrides it
    /// when `read_env` is true.
    spdlog::level::level_enum default_level = spdlog::level::info;

    /// Install the colour console sink. Leave on for applications; libraries
    /// embedding logman may want to turn this off and add their own sinks via
    /// `LogManager::add_sink`.
    bool enable_console = true;

    /// spdlog pattern string. Empty selects the built-in pattern:
    ///   `%Y-%m-%dT%H:%M:%S.%e%z %^%L%$ %P --- [%6t] %n : %v`.
    /// Ignored when `structured_json` is true (JSON output is unstructured by
    /// definition of "structured").
    std::string pattern;

    /// Install the "main" logger as `spdlog::default_logger()`. Preserved
    /// from Ghostframe behaviour — this *is* the global logger facility for
    /// the consuming program. Libraries embedding logman can opt out.
    bool set_as_spdlog_default = true;

    /// When true, scan the process environment under `env_prefixes` for
    /// `<PREFIX>LEVEL`, `<PREFIX>LEVEL_<NAMESPACE>`, and `<PREFIX>FORMAT`.
    bool read_env = true;

    /// Prefixes to scan when `read_env` is true. Defaults to the compile-time
    /// list baked in via the CMake cache variable `LOGMAN_ENV_PREFIXES`,
    /// which always contains `LOGMAN_` and may contain additional consumer
    /// prefixes. Later entries override earlier ones.
    std::vector<std::string> env_prefixes;

    /// Emit one JSON object per line via `JsonFormatter`. Requires
    /// `<nlohmann/json.hpp>` on the include path at compile time; otherwise
    /// the request is ignored and a stderr warning is emitted.
    bool structured_json = false;
};

}  // namespace logman
