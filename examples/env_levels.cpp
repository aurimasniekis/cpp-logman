#include <logman/logman.hpp>

#include <cstdlib>

int main() {
    // Set the env vars from inside the program for demonstration. In real use
    // these come from the shell / process supervisor.
    ::setenv("LOGMAN_LEVEL", "info", 1);
    ::setenv("LOGMAN_LEVEL_ORG_FOO", "trace", 1);
    ::setenv("LOGMAN_LEVEL_NET", "warn", 1);

    logman::LogManager::initialize();  // reads env once

    logman::LogManager::get("org.foo")->trace("org.foo at trace");
    logman::LogManager::get("org.foo.bar")->trace("org.foo.bar at trace (inherits)");
    logman::LogManager::get("net.http")->info("net.http info — suppressed (level=warn)");
    logman::LogManager::get("net.http")->warn("net.http warn — printed");
    logman::LogManager::get("other")->debug("other debug — suppressed (default=info)");
    logman::LogManager::get("other")->info("other info — printed");

    logman::LogManager::flush();
    return 0;
}
