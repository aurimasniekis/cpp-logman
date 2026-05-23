#include <logman/logman.hpp>

int main() {
    logman::LogManager::initialize(spdlog::level::info);

    const auto net = logman::get("net.http");
    const auto db = logman::get("db.postgres");
    const auto worker = logman::get("worker.queue");

    net->info("listening on :8080");
    db->info("connected to postgres");
    worker->info("queue depth = 0");

    // Per-prefix tuning: shut up everything in net.* without touching db.* / worker.*
    logman::LogManager::set_levels_by_prefix("net.", spdlog::level::warn);
    net->info("this won't print — net.* is now warn");
    db->info("but db.* still talks");

    logman::LogManager::flush();
    return 0;
}
