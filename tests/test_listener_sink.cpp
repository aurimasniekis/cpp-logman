#include <logman/listener_sink.hpp>

#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::shared_ptr<logman::ListenerSink> make_sink() {
    auto sink = std::make_shared<logman::ListenerSink>();
    sink->set_level(spdlog::level::trace);
    sink->set_formatter(std::make_unique<spdlog::pattern_formatter>(
        "%v", spdlog::pattern_time_type::local, std::string{}));
    return sink;
}

spdlog::logger make_logger(const std::string& name, const std::shared_ptr<spdlog::sinks::sink>& s) {
    spdlog::logger lg(name, s);
    lg.set_level(spdlog::level::trace);
    return lg;
}

}  // namespace

TEST(ListenerSink, ReceivesAddedListener) {
    const auto sink = make_sink();
    logman::LogEvent captured{};
    bool fired = false;
    sink->add_listener([&](const logman::LogEvent& e) {
        captured = e;
        fired = true;
    });

    auto lg = make_logger("listener.smoke", sink);
    lg.info("hello-world");

    EXPECT_TRUE(fired);
    EXPECT_EQ(captured.channel, "listener.smoke");
    EXPECT_EQ(captured.level, "info");
    EXPECT_NE(captured.message.find("hello-world"), std::string::npos);
}

TEST(ListenerSink, RemoveListenerStopsDelivery) {
    const auto sink = make_sink();
    int calls = 0;
    const auto id = sink->add_listener([&](const logman::LogEvent&) { ++calls; });

    auto lg = make_logger("listener.remove", sink);
    lg.info("first");
    EXPECT_TRUE(sink->remove_listener(id));
    lg.info("second");
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(sink->remove_listener(id));
}

TEST(ListenerSink, ClearRemovesAll) {
    const auto sink = make_sink();
    sink->add_listener([](const logman::LogEvent&) {});
    sink->add_listener([](const logman::LogEvent&) {});
    EXPECT_EQ(sink->listener_count(), 2U);
    sink->clear_listeners();
    EXPECT_EQ(sink->listener_count(), 0U);
}

TEST(ListenerSink, ThrowingListenerDoesNotBreakPipeline) {
    const auto sink = make_sink();
    std::atomic<int> after_calls{0};
    sink->add_listener([](const logman::LogEvent&) { throw std::runtime_error("boom"); });
    sink->add_listener([&](const logman::LogEvent&) { ++after_calls; });

    auto lg = make_logger("listener.throw", sink);
    lg.info("first");
    lg.info("second");

    EXPECT_EQ(after_calls.load(), 2)
        << "Later listeners must still receive events when an earlier one throws";

    // Sink remains usable: register one more and watch it fire.
    std::atomic<int> tail_calls{0};
    sink->add_listener([&](const logman::LogEvent&) { ++tail_calls; });
    lg.info("third");
    EXPECT_EQ(tail_calls.load(), 1);
}
