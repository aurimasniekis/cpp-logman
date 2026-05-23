#pragma once

/// @file
/// @brief Umbrella header. Including this pulls in the full public logman API.
///        The nlohmann/json adapter is conditional on `<nlohmann/json.hpp>`
///        being on the include path at compile time.

#include <logman/env_prefixes.hpp>
#include <logman/formatters.hpp>
#include <logman/init_config.hpp>
#include <logman/listener_sink.hpp>
#include <logman/log_event.hpp>
#include <logman/log_manager.hpp>
#include <logman/version.hpp>

#if __has_include(<nlohmann/json.hpp>)
#include <logman/json_formatter.hpp>
#include <logman/log_event_json.hpp>
#endif
