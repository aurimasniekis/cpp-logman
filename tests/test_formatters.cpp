#include <logman/formatters.hpp>

#include <gtest/gtest.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace {

class CapturingSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    std::string last;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t buf;
        this->formatter_->format(msg, buf);
        last.assign(buf.data(), buf.size());
    }
    void flush_() override {}
};

}  // namespace

TEST(Formatters, UpperLevelFormatterPadsRightToEight) {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<logman::UpperLevelFormatter>('L');
    formatter->set_pattern("[%L]");

    const auto sink = std::make_shared<CapturingSink>();
    sink->set_level(spdlog::level::trace);
    sink->set_formatter(std::move(formatter));
    spdlog::logger lg("fmt.level", sink);
    lg.set_level(spdlog::level::trace);

    lg.info("msg");
    // 4 leading spaces + "INFO" = 8 chars
    EXPECT_NE(sink->last.find("[    INFO]"), std::string::npos) << "got: " << sink->last;

    lg.warn("msg");
    // spdlog::level::to_string_view(warn) is "warning" (7 chars + 1 pad).
    EXPECT_NE(sink->last.find("[ WARNING]"), std::string::npos) << "got: " << sink->last;

    lg.critical("msg");
    EXPECT_NE(sink->last.find("[CRITICAL]"), std::string::npos) << "got: " << sink->last;
}

TEST(Formatters, ChannelNameFormatterFitsWithinTwenty) {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<logman::ChannelNameFormatter>('n');
    formatter->set_pattern("[%n]");

    auto sink = std::make_shared<CapturingSink>();
    sink->set_level(spdlog::level::trace);
    sink->set_formatter(std::move(formatter));
    spdlog::logger lg("very.long.channel.name.that.exceeds.twenty.chars", sink);
    lg.set_level(spdlog::level::trace);

    lg.info("msg");
    const auto open = sink->last.find('[');
    const auto close = sink->last.find(']');
    ASSERT_NE(open, std::string::npos);
    ASSERT_NE(close, std::string::npos);
    const auto inside = sink->last.substr(open + 1, close - open - 1);
    EXPECT_EQ(inside.size(), 20U) << "got: " << sink->last;
}

TEST(Formatters, ChannelNameAbbreviateFitsAndPads) {
    // Short name -> right-padded
    EXPECT_EQ(logman::ChannelNameFormatter::abbreviate("foo", 10), "foo       ");
    // Length-equal name -> unchanged
    EXPECT_EQ(logman::ChannelNameFormatter::abbreviate("0123456789", 10), "0123456789");
    // Long dotted name -> shortened to fit
    const auto out = logman::ChannelNameFormatter::abbreviate("alpha.beta.gamma.delta.epsilon", 10);
    EXPECT_EQ(out.size(), 10U);
}

TEST(Formatters, DefaultPatternRendersExpectedShape) {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<logman::UpperLevelFormatter>('L');
    formatter->add_flag<logman::ChannelNameFormatter>('n');
    formatter->set_pattern(std::string(logman::default_pattern));

    const auto sink = std::make_shared<CapturingSink>();
    sink->set_level(spdlog::level::trace);
    sink->set_formatter(std::move(formatter));
    spdlog::logger lg("pattern.smoke", sink);
    lg.set_level(spdlog::level::trace);

    lg.info("hello");
    // Should contain the padded INFO and the ` --- ` ghostframe separator
    EXPECT_NE(sink->last.find(" --- "), std::string::npos) << "got: " << sink->last;
    EXPECT_NE(sink->last.find("    INFO"), std::string::npos) << "got: " << sink->last;
    EXPECT_NE(sink->last.find("hello"), std::string::npos) << "got: " << sink->last;
}
