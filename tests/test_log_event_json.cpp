#include <logman/log_event.hpp>
#include <logman/log_event_json.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <string>

TEST(LogEventJson, SerializesRequiredFields) {
    logman::LogEvent e{};
    e.timestamp = std::chrono::system_clock::time_point{std::chrono::milliseconds{1700000000000}};
    e.level = "info";
    e.channel = "x.y";
    e.message = "hi";
    e.thread_id = 7;

    nlohmann::json j = e;
    EXPECT_EQ(j.at("timestamp").get<long long>(), 1700000000000LL);
    EXPECT_EQ(j.at("level").get<std::string>(), "info");
    EXPECT_EQ(j.at("channel").get<std::string>(), "x.y");
    EXPECT_EQ(j.at("message").get<std::string>(), "hi");
    EXPECT_EQ(j.at("thread_id").get<std::uint64_t>(), 7U);

    EXPECT_FALSE(j.contains("process_id")) << "negative pid should be omitted";
    EXPECT_FALSE(j.contains("file")) << "empty file should be omitted";
}

TEST(LogEventJson, IncludesOptionalFieldsWhenSet) {
    logman::LogEvent e{};
    e.process_id = 1234;
    e.file = "x.cpp";
    e.line = 7;
    e.function = "do_thing";

    nlohmann::json j = e;
    EXPECT_EQ(j.at("process_id").get<int>(), 1234);
    EXPECT_EQ(j.at("file").get<std::string>(), "x.cpp");
    EXPECT_EQ(j.at("line").get<int>(), 7);
    EXPECT_EQ(j.at("function").get<std::string>(), "do_thing");
}
