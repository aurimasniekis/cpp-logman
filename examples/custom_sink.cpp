#include <logman/logman.hpp>

#include <spdlog/sinks/rotating_file_sink.h>

#include <cstdio>
#include <filesystem>
#include <print>

int main() {
    logman::LogManager::initialize(spdlog::level::info);

    const auto path = std::filesystem::temp_directory_path() / "logman_example.log";
    const auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        path.string(), /*max_size*/ 1024 * 1024, /*max_files*/ 3);
    file_sink->set_level(spdlog::level::trace);
    logman::LogManager::add_sink(file_sink);

    const auto lg = logman::LogManager::get("example.custom_sink");
    lg->info("written to both console and {}", path.string());
    logman::LogManager::flush();

    std::println("rotating file sink wrote to {}", path.string());

    return 0;
}
