#include <logman/log_event.hpp>
#include <logman/log_event_json.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>

int main() {
    logman::LogEvent e{};
    e.timestamp = std::chrono::system_clock::now();
    e.level = "info";
    e.channel = "example.json_event";
    e.message = "hand-built event";
    e.thread_id = 1;
    e.process_id = 4242;
    e.file = __FILE__;
    e.line = __LINE__;
    e.function = "main";

    const nlohmann::json j = e;
    std::cout << j.dump(2) << '\n';
    return 0;
}
