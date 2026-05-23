#include <logman/log_manager.hpp>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>

TEST(EnvLevels, EnvSuffixToChannelPrefix) {
    EXPECT_EQ(logman::env_suffix_to_channel_prefix("ORG_FOO_MANAGER"), "org.foo.manager");
    EXPECT_EQ(logman::env_suffix_to_channel_prefix("NET"), "net");
    EXPECT_EQ(logman::env_suffix_to_channel_prefix("HTTP_CLIENT"), "http.client");
    EXPECT_EQ(logman::env_suffix_to_channel_prefix("_LEADING"), ".leading");
    EXPECT_EQ(logman::env_suffix_to_channel_prefix("TRAILING_"), "trailing.");
    EXPECT_EQ(logman::env_suffix_to_channel_prefix(""), "");
}

TEST(EnvLevels, ParseLevelValueIsCaseInsensitive) {
    EXPECT_EQ(logman::parse_level_value("trace"), spdlog::level::trace);
    EXPECT_EQ(logman::parse_level_value("TRACE"), spdlog::level::trace);
    EXPECT_EQ(logman::parse_level_value("Debug"), spdlog::level::debug);
    EXPECT_EQ(logman::parse_level_value("info"), spdlog::level::info);
    EXPECT_EQ(logman::parse_level_value("warn"), spdlog::level::warn);
    EXPECT_EQ(logman::parse_level_value("warning"), spdlog::level::warn);
    EXPECT_EQ(logman::parse_level_value("ERR"), spdlog::level::err);
    EXPECT_EQ(logman::parse_level_value("error"), spdlog::level::err);
    EXPECT_EQ(logman::parse_level_value("critical"), spdlog::level::critical);
    EXPECT_EQ(logman::parse_level_value("off"), spdlog::level::off);
}

TEST(EnvLevels, ParseLevelValueReturnsNulloptOnUnknown) {
    EXPECT_FALSE(logman::parse_level_value("").has_value());
    EXPECT_FALSE(logman::parse_level_value("verbose").has_value());
    EXPECT_FALSE(logman::parse_level_value("INFOO").has_value());
}

TEST(EnvLevels, MultiPrefixLaterOverridesEarlier) {
    // Set both LOGMAN_ and FOO_ env vars; expect FOO_ to win because it
    // appears later in env_prefixes.
    ::setenv("LOGMAN_LEVEL_INTEG_MP", "info", 1);
    ::setenv("FOO_LEVEL_INTEG_MP", "trace", 1);

    logman::detail::reset_log_manager_for_testing();

    logman::InitConfig cfg;
    cfg.env_prefixes = {"LOGMAN_", "FOO_"};
    logman::LogManager::initialize(cfg);

    const auto lg = logman::LogManager::get("integ.mp.x");
    EXPECT_EQ(lg->level(), spdlog::level::trace) << "FOO_ prefix should override LOGMAN_";

    ::unsetenv("LOGMAN_LEVEL_INTEG_MP");
    ::unsetenv("FOO_LEVEL_INTEG_MP");
    logman::detail::reset_log_manager_for_testing();
}
