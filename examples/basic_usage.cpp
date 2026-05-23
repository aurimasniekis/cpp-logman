#include <logman/logman.hpp>

int main() {
    // Free-function shortcuts forward to LogManager — pick whichever style
    // reads better in your code.
    logman::initialize(spdlog::level::debug);
    const auto lg = logman::get("example.basic");
    lg->trace("trace — filtered out at level=debug");
    lg->debug("debug message");
    lg->info("info message");
    lg->warn("warn message");
    lg->error("error message");
    logman::flush();
    return 0;
}
