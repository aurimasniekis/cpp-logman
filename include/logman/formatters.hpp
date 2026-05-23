#pragma once

/// @file
/// @brief Custom spdlog pattern flag formatters used by the default logman
///        pattern. Public so consumers can reuse them in their own patterns.

#include <spdlog/details/fmt_helper.h>
#include <spdlog/pattern_formatter.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace logman {

/// `%L` — uppercase level name, right-aligned to 8 characters.
/// Pattern uses bare `%L` (no width digit) because spdlog applies padding
/// *before* dispatch to custom flag formatters; doubling it produces extra
/// spaces.
class UpperLevelFormatter final : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg,
                const std::tm& /*tm*/,
                spdlog::memory_buf_t& dest) override {
        const auto level_sv = spdlog::level::to_string_view(msg.level);
        std::string level_name(level_sv.data(), level_sv.size());
        std::ranges::transform(
            level_name, level_name.begin(), [](const unsigned char c) { return std::toupper(c); });
        if (level_name.size() < 8) {
            const std::size_t pad = 8U - level_name.size();
            level_name.insert(std::size_t{0}, pad, ' ');
        }
        spdlog::details::fmt_helper::append_string_view(level_name, dest);
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<UpperLevelFormatter>();
    }
};

/// `%n` — channel name abbreviated/padded to a fixed width (20 chars).
/// Uses dotted-namespace abbreviation: `foo.bar.baz` → `f.b.baz` when
/// truncation is needed.
class ChannelNameFormatter final : public spdlog::custom_flag_formatter {
public:
    static std::string abbreviate(std::string input, const std::size_t size) {
        if (size == 0) {
            return {};
        }

        if (input.size() <= size) {
            input.append(size - input.size(), ' ');
            return input;
        }

        std::vector<std::string_view> parts;
        {
            std::string_view s(input);
            std::size_t start = 0;
            while (true) {
                const std::size_t pos = s.find('.', start);
                if (pos == std::string_view::npos) {
                    parts.emplace_back(s.substr(start));
                    break;
                }
                parts.emplace_back(s.substr(start, pos - start));
                start = pos + 1;
            }
        }

        auto build = [&](const std::size_t abbreviated_prefix_count,
                         const bool leading_dot) -> std::string {
            std::string out;
            if (leading_dot) {
                out.push_back('.');
            }
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (i != 0) {
                    out.push_back('.');
                }
                if (const bool is_last = (i + 1 == parts.size());
                    !is_last && i < abbreviated_prefix_count) {
                    if (!parts[i].empty()) {
                        out.push_back(parts[i].front());
                    }
                } else {
                    out.append(parts[i]);
                }
            }
            return out;
        };

        for (std::size_t abbr = 1; abbr < parts.size(); ++abbr) {
            if (std::string candidate = build(abbr, false); candidate.size() <= size) {
                candidate.append(size - candidate.size(), ' ');
                return candidate;
            }
        }

        while (parts.size() > 1) {
            parts.erase(parts.begin());
            if (std::string candidate = build(parts.size() - 1, true); candidate.size() <= size) {
                candidate.append(size - candidate.size(), ' ');
                return candidate;
            }
        }

        return input.substr(input.size() - size);
    }

    void format(const spdlog::details::log_msg& msg,
                const std::tm& /*tm*/,
                spdlog::memory_buf_t& dest) override {
        const std::string name =
            abbreviate(std::string(msg.logger_name.data(), msg.logger_name.size()), 20);
        spdlog::details::fmt_helper::append_string_view(name, dest);
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<ChannelNameFormatter>();
    }
};

/// Default Ghostframe-style pattern. `%L` is the bare custom-flag form (no
/// width digit) so `UpperLevelFormatter` controls padding alone.
inline constexpr std::string_view default_pattern =
    "%Y-%m-%dT%H:%M:%S.%e%z %^%L%$ %P --- [%6t] %n : %v";

}  // namespace logman
