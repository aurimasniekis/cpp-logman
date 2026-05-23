#include <logman/log_manager.hpp>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace gml = logman;

TEST(LogManager, GetReturnsDistinctLoggerPerName) {
    gml::LogManager::initialize();
    const auto a = gml::LogManager::get("smoke.alpha");
    const auto b = gml::LogManager::get("smoke.beta");
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(a->name(), "smoke.alpha");
    EXPECT_EQ(b->name(), "smoke.beta");
    EXPECT_NE(a.get(), b.get());

    const auto a_again = gml::LogManager::get("smoke.alpha");
    EXPECT_EQ(a.get(), a_again.get()) << "Same channel name must return same logger";
}

TEST(LogManager, GetOrNullDoesNotCreate) {
    EXPECT_EQ(gml::LogManager::get_or_null("never.created.channel"), nullptr);
    (void)gml::LogManager::get("now.created");
    EXPECT_NE(gml::LogManager::get_or_null("now.created"), nullptr);
}

TEST(LogManager, SetLevelsByPrefixTunesMatchingChannelsAndCachesForLater) {
    const auto x_y = gml::LogManager::get("prefix_test.x.y");
    const auto x_z = gml::LogManager::get("prefix_test.x.z");
    const auto other = gml::LogManager::get("prefix_test.other");

    gml::LogManager::set_level("prefix_test.x.y", spdlog::level::warn);
    gml::LogManager::set_level("prefix_test.x.z", spdlog::level::warn);
    gml::LogManager::set_level("prefix_test.other", spdlog::level::warn);

    gml::LogManager::set_levels_by_prefix("prefix_test.x.", spdlog::level::trace);

    EXPECT_EQ(x_y->level(), spdlog::level::trace);
    EXPECT_EQ(x_z->level(), spdlog::level::trace);
    EXPECT_EQ(other->level(), spdlog::level::warn);

    const auto x_late = gml::LogManager::get("prefix_test.x.late");
    EXPECT_EQ(x_late->level(), spdlog::level::trace)
        << "Channels created after the prefix tuning should pick up the cached level";
}

TEST(LogManager, ListenerReceivesEvent) {
    gml::LogManager::initialize(spdlog::level::trace);
    gml::LogManager::set_all_levels(spdlog::level::trace);

    const auto logger = gml::LogManager::get("listener_test.channel");
    logger->set_level(spdlog::level::trace);

    std::shared_ptr<gml::LogEvent> captured;
    const auto id = gml::LogManager::add_listener(
        [&captured](const gml::LogEvent& e) { captured = std::make_shared<gml::LogEvent>(e); });

    logger->info("hello-listener");
    logger->flush();

    ASSERT_TRUE(captured) << "Listener never invoked";
    EXPECT_EQ(captured->channel, "listener_test.channel");
    EXPECT_EQ(captured->level, "info");
    EXPECT_NE(captured->message.find("hello-listener"), std::string::npos);

    gml::LogManager::remove_listener(id);
}

TEST(LogManager, ResetForTestingClearsState) {
    (void)gml::LogManager::get("reset.alpha");
    gml::detail::reset_log_manager_for_testing();
    EXPECT_EQ(gml::LogManager::get_or_null("reset.alpha"), nullptr);

    // Subsequent initialize must work again.
    gml::LogManager::initialize();
    const auto fresh = gml::LogManager::get("reset.alpha");
    ASSERT_TRUE(fresh);
    EXPECT_EQ(fresh->name(), "reset.alpha");
}

TEST(LogManager, AddSinkAppliesToAllLoggers) {
    gml::LogManager::initialize();
    const auto sink = std::make_shared<gml::ListenerSink>();
    sink->set_level(spdlog::level::trace);
    sink->set_formatter(std::make_unique<spdlog::pattern_formatter>(
        "%v", spdlog::pattern_time_type::local, std::string{}));

    int hits = 0;
    sink->add_listener([&](const gml::LogEvent&) { ++hits; });

    const auto pre = gml::LogManager::get("sink.pre");
    pre->set_level(spdlog::level::trace);

    gml::LogManager::add_sink(sink);

    const auto post = gml::LogManager::get("sink.post");
    post->set_level(spdlog::level::trace);

    pre->info("a");
    post->info("b");

    EXPECT_EQ(hits, 2);
    EXPECT_TRUE(gml::LogManager::remove_sink(sink));
}

TEST(LogManagerShortcuts, FreeFunctionsDelegateToManager) {
    logman::initialize(spdlog::level::trace);
    logman::set_all_levels(spdlog::level::trace);

    auto lg = logman::get("shortcuts.alpha");
    ASSERT_TRUE(lg);
    EXPECT_EQ(lg->name(), "shortcuts.alpha");
    EXPECT_EQ(logman::get("shortcuts.alpha").get(), lg.get())
        << "same name must return same logger via shortcut";

    EXPECT_NE(logman::get_or_null("shortcuts.alpha"), nullptr);
    EXPECT_EQ(logman::get_or_null("shortcuts.does.not.exist"), nullptr);

    EXPECT_TRUE(logman::set_level("shortcuts.alpha", spdlog::level::warn));
    EXPECT_EQ(lg->level(), spdlog::level::warn);

    logman::set_levels_by_prefix("shortcuts.", spdlog::level::debug);
    EXPECT_EQ(lg->level(), spdlog::level::debug);
    auto late = logman::get("shortcuts.beta");
    EXPECT_EQ(late->level(), spdlog::level::debug);

    logman::set_default_level(spdlog::level::info);
    EXPECT_EQ(logman::default_level(), spdlog::level::info);

    int hits = 0;
    auto id = logman::add_listener([&](const logman::LogEvent&) { ++hits; });
    lg->info("via shortcuts");
    logman::flush();
    EXPECT_GE(hits, 1);
    EXPECT_TRUE(logman::remove_listener(id));

    EXPECT_NE(logman::listener_sink(), nullptr);
    EXPECT_FALSE(logman::channels().empty());
}
