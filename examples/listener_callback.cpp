#include <logman/logman.hpp>

#include <iostream>

int main() {
    logman::LogManager::initialize(spdlog::level::trace);
    logman::LogManager::set_all_levels(spdlog::level::trace);

    const auto id = logman::LogManager::add_listener([](const logman::LogEvent& e) {
        std::cout << "[listener] level=" << e.level << " channel=" << e.channel
                  << " message=" << e.message << " thread_id=" << e.thread_id << '\n';
    });

    const auto lg = logman::LogManager::get("example.listener");
    lg->info("event one");
    lg->warn("event two");
    logman::LogManager::flush();

    logman::LogManager::remove_listener(id);
    return 0;
}
