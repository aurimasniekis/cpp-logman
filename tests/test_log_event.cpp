#include <logman/log_event.hpp>

#include <gtest/gtest.h>

#include <chrono>

TEST(LogEvent, DefaultsAreSensible) {
    const logman::LogEvent e{};
    EXPECT_EQ(e.thread_id, 0U);
    EXPECT_EQ(e.process_id, -1);
    EXPECT_EQ(e.line, 0);
    EXPECT_TRUE(e.level.empty());
    EXPECT_TRUE(e.message.empty());
}

TEST(LogEvent, FieldsRoundTrip) {
    logman::LogEvent e{};
    e.timestamp = std::chrono::system_clock::now();
    e.level = "info";
    e.channel = "test.chan";
    e.message = "hello";
    e.thread_id = 42;
    e.process_id = 1234;
    e.file = "foo.cpp";
    e.line = 7;
    e.function = "do_thing";

    EXPECT_EQ(e.level, "info");
    EXPECT_EQ(e.channel, "test.chan");
    EXPECT_EQ(e.message, "hello");
    EXPECT_EQ(e.thread_id, 42U);
    EXPECT_EQ(e.process_id, 1234);
    EXPECT_EQ(e.file, "foo.cpp");
    EXPECT_EQ(e.line, 7);
    EXPECT_EQ(e.function, "do_thing");
}
