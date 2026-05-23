#include <logman/json_formatter.hpp>
#include <logman/log_manager.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
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

TEST(JsonFormatter, EmitsParsableJsonWithExpectedFields) {
    auto sink = std::make_shared<CapturingSink>();
    sink->set_level(spdlog::level::trace);
    sink->set_formatter(std::make_unique<logman::JsonFormatter>());

    spdlog::logger lg("json.fmt.smoke", sink);
    lg.set_level(spdlog::level::trace);
    lg.info("hello-json");

    ASSERT_FALSE(sink->last.empty());
    ASSERT_EQ(sink->last.back(), '\n');
    const auto j = nlohmann::json::parse(sink->last);
    EXPECT_EQ(j.at("level").get<std::string>(), "info");
    EXPECT_EQ(j.at("channel").get<std::string>(), "json.fmt.smoke");
    EXPECT_EQ(j.at("message").get<std::string>(), "hello-json");
    EXPECT_TRUE(j.contains("timestamp"));
    EXPECT_TRUE(j.contains("thread_id"));
}

TEST(LogManager, StructuredJsonFlagSwitchesFormat) {
    logman::detail::reset_log_manager_for_testing();

    const auto sink = std::make_shared<CapturingSink>();
    sink->set_level(spdlog::level::trace);
    sink->set_formatter(std::make_unique<logman::JsonFormatter>());

    logman::InitConfig cfg;
    cfg.read_env = false;
    cfg.enable_console = false;
    cfg.structured_json = true;
    logman::LogManager::initialize(cfg);
    logman::LogManager::add_sink(sink);

    const auto lg = logman::LogManager::get("json.mgr.smoke");
    lg->set_level(spdlog::level::trace);
    lg->info("via-manager");
    lg->flush();

    ASSERT_FALSE(sink->last.empty());
    const auto j = nlohmann::json::parse(sink->last);
    EXPECT_EQ(j.at("channel").get<std::string>(), "json.mgr.smoke");
    EXPECT_EQ(j.at("message").get<std::string>(), "via-manager");

    logman::detail::reset_log_manager_for_testing();
}
