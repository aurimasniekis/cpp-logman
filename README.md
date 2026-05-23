# LogMan


[![CI](https://github.com/aurimasniekis/cpp-logman/actions/workflows/ci.yml/badge.svg)](https://github.com/aurimasniekis/cpp-logman/actions/workflows/ci.yml)
[![Docs](https://github.com/aurimasniekis/cpp-logman/actions/workflows/docs.yml/badge.svg)](https://aurimasniekis.github.io/cpp-logman/)

A header-only C++23 logging facade that wraps [spdlog](https://github.com/gabime/spdlog) with a
single `LogManager` registry. It gives you lazily-created named channels (`net.http`, `db.postgres`,
…), per-channel and prefix-based level control, a callback-style listener hook for shipping records
into your own pipeline as structured `LogEvent` values, and an opt-in JSON output format.

The underlying loggers are plain `spdlog::logger` shared pointers — anything you can do with spdlog
you can still do here.

## Why use this library?

- **Channels for free.** `logman::get("net.http")` lazily creates and caches a logger; calling
  `get` with the same name later returns the same instance.
- **Namespace-prefix levels.** `set_levels_by_prefix("net.", warn)` tunes every existing and future
  channel under `net.*`. The same rule applies to env vars: `LOGMAN_LEVEL_NET_HTTP=trace` lights up
  `net.http.*` without code changes.
- **Listener capture.** Register a `std::function<void(const LogEvent&)>` and receive structured
  records (timestamp, level, channel, message, thread id, PID, source location). Listeners run
  *outside* the sink lock and exceptions thrown from one listener can't break the pipeline for
  others.
- **Structured JSON, opt-in.** One JSON object per console line via `InitConfig::structured_json`
  or `LOGMAN_FORMAT=json`. Requires `<nlohmann/json.hpp>` on the include path at compile time;
  otherwise the request is ignored with a one-line stderr warning.
- **Header-only.** No separate library to build or link beyond spdlog's own headers.
- **Not ideal for** ultra-hot paths that can't tolerate a `shared_mutex` lookup on the first
  `get()` per channel, or projects that need to fully replace spdlog rather than wrap it.

## Quick example

```cpp
#include <logman/logman.hpp>

int main() {
    logman::initialize(spdlog::level::debug);

    auto net = logman::get("net.http");
    auto db  = logman::get("db.postgres");

    net->info("listening on :8080");
    db->info("connected");

    // Tap into every record from anywhere in the program.
    auto id = logman::add_listener([](const logman::LogEvent& e) {
        // e.timestamp, e.level, e.channel, e.message, e.thread_id, ...
        // Ship to telemetry, an in-app log viewer, etc.
    });

    // Quiet a whole namespace without touching the rest.
    logman::set_levels_by_prefix("net.", spdlog::level::warn);
    net->info("suppressed by prefix rule");
    db->info("still printed");

    logman::flush();
    logman::remove_listener(id);
}
```

The free functions (`logman::initialize`, `logman::get`, `logman::set_level`, …) are thin shortcuts
that forward to `logman::LogManager::*`. Use either style — same behaviour, same singleton
underneath.

Why this example works:

- `initialize(level)` sets up the singleton, installs a colour console sink, and parses
  `LOGMAN_*` env vars. It is `std::call_once`-guarded, so calling it again is a no-op (you must
  use `shutdown()` to reinitialise).
- `get(name)` returns a `std::shared_ptr<spdlog::logger>`. The first call for a new name takes a
  write lock to create the channel; subsequent calls take a shared lock and return the cached
  logger.
- `set_levels_by_prefix` updates every already-created channel whose name starts with the prefix
  *and* caches the rule so any channel created later under that prefix inherits it.

## Installation

The repository's only required dependency is spdlog v1.17.0, fetched automatically via CMake's
`FetchContent` (and reused via `find_package` if already present, thanks to `FIND_PACKAGE_ARGS`).
Tests pull GoogleTest 1.17.0; the JSON examples pull nlohmann/json 3.12.0. All optional.

### CMake `FetchContent` (recommended)

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    logman
    URL      https://github.com/aurimasniekis/cpp-logman/archive/refs/tags/v0.1.0.tar.gz
    URL_HASH SHA256=0000000000000000000000000000000000000000000000000000000000000000
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(logman)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE logman::logman)
```

### CMake `add_subdirectory`

If you vendor the repository inside your tree:

```cmake
add_subdirectory(third_party/logman)
target_link_libraries(my_app PRIVATE logman::logman)
```

### Install + `find_package`

`make install` (or the equivalent `cmake --install`) produces an installable package only when
spdlog is provided by `find_package` rather than fetched (the build emits a warning and disables
install rules if it had to fetch spdlog itself, because a fetched spdlog cannot be re-exported).
With a system spdlog:

```cmake
find_package(logman CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE logman::logman)
```

### Manual include path

Because the library is header-only, you can also add `include/` (and the generated `version.hpp`
/ `env_prefixes.hpp`) to your compiler's include path and link spdlog yourself. CMake is still the
expected workflow — there is no separate hand-rolled install script.

## Requirements

- **C++ standard:** C++23. `CMAKE_CXX_STANDARD 23` is required; `cxx_std_23` is propagated via the
  `logman` INTERFACE target.
- **CMake:** 3.25 or newer.
- **Dependency:** spdlog 1.17.0 (header-only target `spdlog::spdlog_header_only`).
- **Optional dependency:** nlohmann/json 3.12.0. Detected at compile time via
  `__has_include(<nlohmann/json.hpp>)`; without it, the JSON adapter and `JsonFormatter` are
  excluded and `logman::has_json_support` is `false`.
- **Test dependency:** GoogleTest 1.17.0 (only when `LOGMAN_BUILD_TESTS` is ON).
- **Platforms:** the listener sink probes the PID via `_getpid` on Windows and `getpid` on
  `__unix__`/`__APPLE__`. The env-var scanner uses `GetEnvironmentStringsW` on Windows and the
  POSIX `environ` everywhere else. No other platform-specific code.

## Core concepts

### `LogManager`

Singleton that owns the channel map, the default console sink, and the listener sink. All public
operations are accessed through static methods or the matching free functions. Thread-safe: a
`std::shared_mutex` guards channel and level lookups.

```cpp
logman::LogManager::initialize();          // or logman::initialize()
auto lg = logman::LogManager::get("svc.x"); // or logman::get("svc.x")
```

`get` lazily registers the channel with spdlog under the same name and copies the manager's sinks
into it, so the channel inherits the console sink, the listener sink, and any sinks you added with
`add_sink` *before* the call.

### `InitConfig`

POD config struct passed to `LogManager::initialize(const InitConfig&)`. All fields have defaults;
override only what you need.

```cpp
logman::InitConfig cfg;
cfg.default_level        = spdlog::level::debug;
cfg.enable_console       = true;         // install the colour console sink
cfg.pattern              = "";           // empty => built-in pattern
cfg.set_as_spdlog_default = true;        // install "main" as spdlog::default_logger
cfg.read_env             = true;         // scan env vars under env_prefixes
cfg.env_prefixes         = {};           // empty => compile-time default ("LOGMAN_" + extras)
cfg.structured_json      = false;        // true => JSON console output (requires nlohmann/json)
logman::LogManager::initialize(cfg);
```

`initialize` is `std::call_once`-guarded. To apply a *different* `InitConfig` in the same process
(typically in tests), call `LogManager::shutdown()` first — or
`logman::detail::reset_log_manager_for_testing()` from `<logman/log_manager.hpp>`.

### `LogEvent`

The structured form of one log record, defined in `<logman/log_event.hpp>` and produced by
`ListenerSink`:

```cpp
struct LogEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string level;        // "trace" .. "critical"
    std::string channel;      // logger name
    std::string message;      // already-formatted message body
    std::uint64_t thread_id = 0;
    int process_id = -1;      // -1 if probing the PID failed
    std::string file;         // empty when source location is unavailable
    int line = 0;
    std::string function;
};
```

Listeners receive a `const LogEvent&`. The struct is trivially copyable for storage.

### `ListenerSink`

The spdlog sink that powers `add_listener`. It formats each record with its own bare `"%v"`
formatter, packages the result into a `LogEvent`, takes a snapshot of the listener list under a
short lock, then dispatches *outside* the lock. Each call is guarded by a try/catch — exceptions
are swallowed in release builds and logged to stderr in debug builds.

```cpp
const auto id = logman::add_listener([](const logman::LogEvent& e) { /* ... */ });
// later
logman::remove_listener(id);   // true if it existed, false otherwise
logman::clear_listeners();     // remove all listeners
```

### Formatters

`UpperLevelFormatter` (`%L`) — uppercase level name, right-aligned to 8 characters. Use the bare
`%L` token; the padding is applied inside the custom flag (doubling it produces extra spaces).

`ChannelNameFormatter` (`%n`) — channel name in a 20-character column. Long dotted names are
abbreviated segment-by-segment: `alpha.beta.gamma.delta.epsilon` → e.g. `a.b.g.d.epsilon`. If it
cannot fit even after abbreviation, leading segments are dropped with a leading `.`.

Default pattern (composed automatically when `InitConfig::pattern` is empty):

```
%Y-%m-%dT%H:%M:%S.%e%z %^%L%$ %P --- [%6t] %n : %v
```

To change it:

```cpp
logman::set_pattern("[%H:%M:%S.%e] [%^%L%$] [%n] %v");
```

Setting a pattern reformats both the console sink and every already-registered channel. The
listener sink keeps its own bare `%v` formatter — `LogEvent::message` always carries just the
formatted message body, not the prefix.

## Common usage patterns

### Tune levels by channel namespace

```cpp
#include <logman/logman.hpp>

int main() {
    logman::initialize(spdlog::level::info);

    auto net    = logman::get("net.http");
    auto db     = logman::get("db.postgres");
    auto worker = logman::get("worker.queue");

    // Silence everything under net.* (existing + future).
    logman::set_levels_by_prefix("net.", spdlog::level::warn);

    net->info("dropped");      // suppressed
    db->info("printed");       // unaffected
    worker->info("printed");   // unaffected

    // A channel created after the rule still inherits it.
    auto api = logman::get("net.api.v1");
    api->info("also dropped");
}
```

Why this works: `set_levels_by_prefix` updates every matching channel *and* records the rule in an
internal table that `get` consults when creating new channels.

### Capture records into your own pipeline

```cpp
#include <logman/logman.hpp>
#include <iostream>

int main() {
    logman::LogManager::initialize(spdlog::level::trace);
    logman::LogManager::set_all_levels(spdlog::level::trace);

    const auto id = logman::add_listener([](const logman::LogEvent& e) {
        std::cout << "[listener] " << e.level << ' ' << e.channel
                  << " thread=" << e.thread_id << " : " << e.message << '\n';
    });

    auto lg = logman::get("example.listener");
    lg->info("event one");
    lg->warn("event two");
    logman::flush();

    logman::remove_listener(id);
}
```

Common pitfalls:

- Listeners run on the thread that emitted the log record, not on a dedicated worker. If you do
  heavy work inside the callback, do it on your own queue.
- A listener that throws a `std::exception` (or anything else) does not stop the other listeners
  from running. The exception is logged to stderr in debug builds and silently dropped in release.
- The `message` field is the formatted output of the bare `%v` formatter — no timestamp, no
  channel, no level prefix.

### Add a rotating file sink alongside the console

```cpp
#include <logman/logman.hpp>
#include <spdlog/sinks/rotating_file_sink.h>

#include <filesystem>

int main() {
    logman::LogManager::initialize(spdlog::level::info);

    const auto path = std::filesystem::temp_directory_path() / "my_app.log";
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        path.string(), /*max_size*/ 1024 * 1024, /*max_files*/ 3);
    file_sink->set_level(spdlog::level::trace);
    logman::LogManager::add_sink(file_sink);

    logman::get("svc.boot")->info("written to console and {}", path.string());
    logman::flush();
}
```

Why this works: `add_sink` appends the sink to the manager *and* to every channel that already
exists. New channels created after the call also receive it because `get()` copies the current
sink list when constructing the underlying `spdlog::logger`. `remove_sink` reverses both.

### Configure from environment variables

`LOGMAN_LEVEL`, `LOGMAN_LEVEL_<NAMESPACE>`, and `LOGMAN_FORMAT` are parsed once during
`initialize()` when `InitConfig::read_env` is true (the default).

```sh
LOGMAN_LEVEL=info                     # global default
LOGMAN_LEVEL_ORG_FOO_MANAGER=debug    # applies to channel prefix "org.foo.manager"
LOGMAN_LEVEL_NET_HTTP=trace           # applies to channel prefix "net.http"
LOGMAN_FORMAT=json                    # switch the console sink to structured JSON
```

`<NAMESPACE>` is lowercased and underscores become dots, so `ORG_FOO_MANAGER` addresses
`org.foo.manager.*`. Unknown level names or unknown `FORMAT` values emit a one-line stderr warning
and are skipped — they never abort initialisation.

Demonstration (test-only — you would normally set these in the shell):

```cpp
#include <logman/logman.hpp>
#include <cstdlib>

int main() {
    ::setenv("LOGMAN_LEVEL",          "info",  1);
    ::setenv("LOGMAN_LEVEL_ORG_FOO",  "trace", 1);
    ::setenv("LOGMAN_LEVEL_NET",      "warn",  1);

    logman::LogManager::initialize();      // reads env once

    logman::get("org.foo.bar")->trace("printed — inherits org.foo.* trace");
    logman::get("net.http")->info("suppressed — net.* is warn");
    logman::get("other")->debug("suppressed — falls back to LOGMAN_LEVEL=info");
}
```

### Custom env-var prefix (for rebranding)

Applications integrating logman via FetchContent can register additional prefixes at configure
time. `LOGMAN_` is always included unconditionally.

```cmake
set(LOGMAN_ENV_PREFIXES "FOO_" CACHE STRING "")
FetchContent_MakeAvailable(logman)
```

Later prefixes override earlier ones at runtime, so an application using both `LOGMAN_LEVEL=info`
and `FOO_LEVEL=debug` ends up with `debug`. You can also override `env_prefixes` at runtime:

```cpp
logman::InitConfig cfg;
cfg.env_prefixes = {"LOGMAN_", "FOO_"};   // FOO_ wins on conflict
logman::LogManager::initialize(cfg);
```

### Emit structured JSON

```cpp
#include <logman/logman.hpp>

int main() {
    logman::InitConfig cfg;
    cfg.default_level   = spdlog::level::trace;
    cfg.structured_json = true;
    logman::LogManager::initialize(cfg);
    logman::LogManager::set_all_levels(spdlog::level::trace);

    auto lg = logman::get("example.json");
    lg->info("structured json line one");
    lg->warn("second line, opaque to the formatter");
    logman::flush();
}
```

Each console line is a single compact JSON object terminated by `\n`:

```json
{"timestamp":1700000000000,"level":"info","channel":"example.json","message":"hello","thread_id":7}
```

`process_id`, `file`, `line`, and `function` are included only when present (negative PID or empty
file path → omitted). The schema is defined by `to_json(nlohmann::json&, const LogEvent&)` in
`<logman/log_event_json.hpp>`.

### Serialise a hand-built `LogEvent`

`LogEvent` is independent of spdlog, so you can build and serialise one yourself — useful for
tests or for replaying records from another source.

```cpp
#include <logman/log_event.hpp>
#include <logman/log_event_json.hpp>

#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>

int main() {
    logman::LogEvent e{};
    e.timestamp  = std::chrono::system_clock::now();
    e.level      = "info";
    e.channel    = "example.json_event";
    e.message    = "hand-built event";
    e.thread_id  = 1;
    e.process_id = 4242;
    e.file       = __FILE__;
    e.line       = __LINE__;
    e.function   = "main";

    const nlohmann::json j = e;
    std::cout << j.dump(2) << '\n';
}
```

### Detect optional JSON support at compile time

```cpp
#include <logman/logman.hpp>

if constexpr (logman::has_json_support) {
    // safe to include <logman/json_formatter.hpp> and use JsonFormatter
}
```

### Inspect or enumerate channels

```cpp
// Snapshot of all currently-registered channels and their levels.
for (const auto& [name, lvl] : logman::channels()) {
    // ...
}

// Lookup without creating a channel.
if (auto lg = logman::get_or_null("does.not.exist"); !lg) {
    // nothing was created
}
```

### Flush on demand or at a chosen level

```cpp
logman::flush();                                 // flush every channel now
logman::set_flush_on(spdlog::level::warn);       // auto-flush at warn or above
```

`set_flush_on` is recorded by the manager so newly-created channels inherit it.

## Error handling

logman does not throw on the hot path. The way it reports problems:

| Situation                                                              | Behaviour                                                                                              |
|------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------|
| Unknown level name in env var                                          | One-line warning to `std::cerr`, env entry skipped                                                     |
| Unknown `LOGMAN_FORMAT` value                                          | Warning to `std::cerr`, defaults to text                                                               |
| `structured_json = true` but `<nlohmann/json.hpp>` not on include path | Warning to `std::cerr`, falls back to text formatter                                                   |
| Listener throws an exception                                           | Caught; logged to `std::cerr` in debug builds, silently dropped in release. Other listeners still run. |
| `set_level(channel, …)` for an unknown channel                         | Returns `false` (no exception)                                                                         |
| `remove_listener(id)` / `remove_sink(sink)` for unknown id/sink        | Returns `false`                                                                                        |
| `get_or_null(name)` for unknown channel                                | Returns `nullptr`                                                                                      |
| `get(name)` before `initialize()`                                      | Auto-initialises with defaults, then proceeds                                                          |
| Calling `initialize()` twice without `shutdown()`                      | Second call is a no-op (`std::call_once`)                                                              |

spdlog's own logging operations may still throw `spdlog::spdlog_ex` for sink-level failures (for
example, file-sink IO errors). logman does not wrap those.

## Edge cases and pitfalls

- **Init is once-only.** `LogManager::initialize` is guarded by `std::call_once`. To swap
  `InitConfig` at runtime, call `LogManager::shutdown()` first (and rebuild any sinks/listeners
  you owned). Tests should use `logman::detail::reset_log_manager_for_testing()`.
- **`set_pattern` does not affect JSON.** When `InitConfig::structured_json` was true at init time,
  the console sink uses `JsonFormatter`; `set_pattern` will replace the *pattern* used by the
  per-channel loggers, but the console sink keeps its JSON formatter until you replace the sink.
- **`get(name)` snapshots sinks at creation time.** Sinks added with `add_sink` *after* a channel
  was first fetched are still propagated by `add_sink` itself (it iterates every existing logger).
  Sinks created via your own `std::make_shared<spdlog::logger>` outside the manager won't pick
  this up.
- **Source location may be empty.** `LogEvent::file` is non-empty only when spdlog received a
  source location (typically via the `SPDLOG_*` macros). Calls like `lg->info("…")` do not include
  it, so `file`, `line`, `function` will be defaults and the JSON adapter will omit them.
- **`process_id == -1`** when the platform-specific PID probe is unavailable. The JSON adapter
  omits the field in that case.
- **`thread_id` is spdlog's `os::thread_id()`**, not a `std::thread::id`. It is suitable for log
  correlation but not for joining a `std::thread`.
- **Prefix rules don't handle wildcards.** `set_levels_by_prefix("net.", warn)` uses plain
  `starts_with`. Pass an exact prefix (usually ending in `.`) to avoid matching `network_*`.
- **Env-var namespace mapping is mechanical.** `_LEADING` becomes `.leading`, `TRAILING_` becomes
  `trailing.`, an empty namespace becomes the empty string (which matches every channel). Be
  deliberate with naming.
- **`set_all_levels(lvl)` overwrites per-channel and prefix tuning** for every existing channel
  and the default level. Newly-created channels still pick up cached prefix rules.
- **Channel-level vs. sink-level.** Each channel and each sink has its own level. `set_level` /
  `set_levels_by_prefix` / `set_all_levels` operate on channels. Sinks default to `trace`; tighten
  them with `sink->set_level(...)` if you want a per-sink filter (see the rotating-file example).
- **Listener exceptions** are swallowed silently in release builds. If you need diagnostics, add a
  guard inside the listener and log/store the failure yourself, or build with `NDEBUG` undefined.
- **Install rules are disabled when spdlog is fetched.** `LOGMAN_INSTALL` flips off automatically
  if `Dependencies.cmake` fetched spdlog instead of finding it, because the fetched target can't
  be re-exported. Install spdlog via a system package or `find_package` if you need install rules.

## API overview

Only the most commonly used public symbols. Everything is under `namespace logman`.

| API                                                              | Purpose                                                      | Notes                                     |
|------------------------------------------------------------------|--------------------------------------------------------------|-------------------------------------------|
| `initialize(level)` / `initialize(InitConfig)`                   | Bring up the singleton                                       | `std::call_once`-guarded                  |
| `shutdown()`                                                     | Tear down sinks, listeners, channels                         | Required before re-initialising           |
| `get(name)`                                                      | Get-or-create a channel                                      | Returns `std::shared_ptr<spdlog::logger>` |
| `get_or_null(name)`                                              | Lookup without create                                        | Returns `nullptr` if absent               |
| `channels()`                                                     | Snapshot of name → level map                                 | For introspection / debugging             |
| `set_level(name, lvl)`                                           | Tune one channel                                             | Returns `false` if name unknown           |
| `set_levels_by_prefix(pfx, lvl)`                                 | Tune every channel under a prefix (existing + future)        | Cached for late-bound channels            |
| `set_all_levels(lvl)`                                            | Force one level on every channel and the default             | Overwrites prior tuning                   |
| `set_default_level(lvl)` / `default_level()`                     | Default for newly-created channels                           |                                           |
| `set_pattern(p)`                                                 | Change spdlog pattern globally                               | Ignored by JSON console sink              |
| `flush()` / `set_flush_on(lvl)`                                  | Manual flush / auto-flush trigger                            |                                           |
| `add_sink(sink)` / `remove_sink(sink)`                           | Attach/detach an spdlog sink to every channel                |                                           |
| `add_listener(fn)` / `remove_listener(id)` / `clear_listeners()` | Manage `LogEvent` callbacks                                  | Snapshot-then-invoke pattern              |
| `listener_sink()`                                                | Access the underlying `ListenerSink`                         | For advanced wiring                       |
| `LogEvent`                                                       | Structured record passed to listeners                        | Defined in `<logman/log_event.hpp>`       |
| `InitConfig`                                                     | Initialisation settings                                      | All fields have sensible defaults         |
| `JsonFormatter`                                                  | spdlog formatter emitting one JSON object per line           | Requires `<nlohmann/json.hpp>`            |
| `UpperLevelFormatter` / `ChannelNameFormatter`                   | Custom pattern flags (`%L`, `%n`)                            | Reusable in your own patterns             |
| `has_json_support`                                               | `constexpr bool` — true iff JSON adapter is compiled in      |                                           |
| `default_env_prefixes`                                           | `std::span<const std::string_view>` of compile-time prefixes |                                           |

The `LogManager::` static methods mirror the free functions one-to-one — pick whichever style you
prefer.

## Examples

All examples live under `examples/` and are built when `LOGMAN_BUILD_EXAMPLES` is ON.

| Example                          | Demonstrates                                                     |
|----------------------------------|------------------------------------------------------------------|
| `examples/basic_usage.cpp`       | Minimal `initialize` + `get` + each level call                   |
| `examples/multi_channel.cpp`     | Multiple channels and per-prefix level tuning                    |
| `examples/listener_callback.cpp` | Registering a listener and reading `LogEvent` fields             |
| `examples/env_levels.cpp`        | Driving levels from `LOGMAN_LEVEL[_NAMESPACE]` env vars          |
| `examples/custom_sink.cpp`       | Attaching a `rotating_file_sink_mt` alongside the console        |
| `examples/structured_json.cpp`   | Switching the console to JSON via `InitConfig::structured_json`  |
| `examples/json_event.cpp`        | Hand-building a `LogEvent` and serialising it with nlohmann/json |

Build and run them all (the Makefile wraps the same CMake calls):

```bash
make examples       # build + run every logman_* example, fail on any non-zero exit
make example        # just logman_basic_usage
```

## Testing

GoogleTest 1.17.0 is fetched automatically when `LOGMAN_BUILD_TESTS` is ON (the default when the
project is built top-level).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Or via the convenience Makefile:

```bash
make test       # configure + build + test in build/
make sanitize   # Debug + ASan + UBSan in build-san/
make tsan       # Debug + ThreadSanitizer in build-tsan/
make release    # Release build + test in build-release/
make tidy       # configure + build with clang-tidy in build-tidy/
make coverage   # Clang source-based coverage in build-coverage/
make ci         # the full pre-push gate (format-check + tidy + test + sanitize + tsan + release)
```

The tests cover channel registry semantics, prefix-based level tuning (including late-bound
channels), listener add/remove/clear, listener exceptions, env-var parsing, multi-prefix override
order, formatter padding/abbreviation, JSON serialisation (with and without optional fields), and
thread-safety smoke tests (8 threads × 200 events; 16 threads racing for the same channel name).

CMake presets are available via `CMakePresets.json` (`cmake --preset <name>` / `--list-presets`).

## Performance notes

- **Channel lookup:** `O(1)` average via `std::unordered_map`. The first `get(name)` for a new
  channel takes a write lock; repeat calls take a shared lock.
- **Listener dispatch:** copies the listener list under a short mutex, then invokes outside the
  lock. This avoids holding the sink mutex during user callbacks but allocates per record (the
  snapshot vector). Avoid listeners on the hottest paths if that allocation matters to you.
- **Logging:** delegates to `spdlog::logger`, so spdlog's compile-time level macros
  (`SPDLOG_ACTIVE_LEVEL`) still apply if you want to strip records at compile time.

## FAQ

**Do I need to link a library, or is it header-only?**
Header-only. Link `logman::logman`; CMake handles the rest. spdlog's header-only target is pulled
in transitively.

**Can I use it in multiple threads?**
Yes. The channel/level maps are guarded by a `std::shared_mutex`; the listener list has its own
mutex and listeners are dispatched outside the sink lock. Loggers themselves are `spdlog`'s
multi-threaded variants (`spdlog::logger` with a console-color `_mt` sink by default).

**What if `nlohmann/json` isn't installed?**
The umbrella header skips the JSON adapter via `__has_include`, `logman::has_json_support` is
`false`, and asking for `structured_json = true` falls back to the text formatter after a one-line
stderr warning. Nothing else breaks.

**Can I disable the console sink?**
Set `InitConfig::enable_console = false` and add your own sinks via `LogManager::add_sink`.

**How do I integrate with my own log viewer or telemetry pipeline?**
Use `add_listener` to receive every record as a `LogEvent`. Marshal those into your queue/IPC of
choice. Listeners run on the calling thread, so push to a queue rather than blocking inside the
callback.

**Why are levels still printing after I called `set_levels_by_prefix`?**
`set_levels_by_prefix` is a plain `starts_with` check. `"net"` matches `"network.api"` too;
prefer `"net."` to scope to the dotted namespace.

**How do I reset state between tests?**
Use `logman::detail::reset_log_manager_for_testing()` — it drops the singleton state, clears
listeners and sinks, and resets the once-flag so the next `initialize()` re-runs.

**How do I bypass logman entirely and reach into spdlog?**
`logman::get(name)` returns the underlying `std::shared_ptr<spdlog::logger>`; do what you like
with it. `spdlog::default_logger()` is set to the manager's `"main"` logger unless you turned that
off with `InitConfig::set_as_spdlog_default = false`.

**How do I build the API docs?**
`make docs` (or `cmake -S . -B build-docs -DLOGMAN_BUILD_DOCS=ON && cmake --build build-docs
--target logman_docs`). Requires Doxygen.

## Contributing

Contributions to the library are welcome! If you encounter any issues or have suggestions for
improvements,
please feel free to submit a pull request or open an issue on the project's repository.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
