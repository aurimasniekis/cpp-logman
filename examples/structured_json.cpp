#include <logman/logman.hpp>

int main() {
    logman::InitConfig cfg;
    cfg.default_level = spdlog::level::trace;
    cfg.structured_json = true;
    logman::LogManager::initialize(cfg);
    logman::LogManager::set_all_levels(spdlog::level::trace);

    const auto lg = logman::LogManager::get("example.json");
    lg->info("structured json line one");
    lg->warn("second line, key=value style messages stay opaque to the formatter");
    logman::LogManager::flush();
    return 0;
}
