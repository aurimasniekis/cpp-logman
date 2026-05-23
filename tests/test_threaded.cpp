#include <logman/log_manager.hpp>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

TEST(Threaded, ListenerReceivesAllEventsAcrossThreads) {
    logman::LogManager::initialize(spdlog::level::trace);
    logman::LogManager::set_all_levels(spdlog::level::trace);

    constexpr int k_threads = 8;
    constexpr int k_per_thread = 200;
    std::atomic<int> events_seen{0};

    const auto id = logman::LogManager::add_listener(
        [&](const logman::LogEvent&) { events_seen.fetch_add(1, std::memory_order_relaxed); });

    std::vector<std::thread> workers;
    workers.reserve(k_threads);
    for (int t = 0; t < k_threads; ++t) {
        workers.emplace_back([t] {
            const auto lg = logman::LogManager::get("threaded.worker." + std::to_string(t));
            lg->set_level(spdlog::level::trace);
            for (int i = 0; i < k_per_thread; ++i) {
                lg->info("hit {} from {}", i, t);
            }
            lg->flush();
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    logman::LogManager::flush();

    EXPECT_EQ(events_seen.load(), k_threads * k_per_thread);
    logman::LogManager::remove_listener(id);
}

TEST(Threaded, GetChannelIsRaceFree) {
    logman::LogManager::initialize();
    constexpr std::size_t k_threads = 16;
    std::vector<std::thread> workers;
    workers.reserve(k_threads);
    std::vector<std::shared_ptr<spdlog::logger>> seen(k_threads);
    for (std::size_t t = 0; t < k_threads; ++t) {
        workers.emplace_back(
            [t, &seen] { seen[t] = logman::LogManager::get("threaded.shared.name"); });
    }
    for (auto& w : workers) {
        w.join();
    }
    // All threads should see the same instance.
    for (std::size_t t = 1; t < k_threads; ++t) {
        EXPECT_EQ(seen[0].get(), seen[t].get());
    }
}
