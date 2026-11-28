// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <charconv>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace thinksmart {

struct HungRing {
    uint32_t ring;
    uint32_t completed;
    uint32_t submitted;

    bool operator==(const HungRing&) const = default;
};

constexpr auto kMaxReportGap = std::chrono::seconds(3);

constexpr bool IsRecentKernelLog(std::chrono::nanoseconds timestamp,
                                 std::chrono::nanoseconds now) {
    // logd's kernel-to-realtime conversion can put a live record slightly ahead of the clock.
    constexpr auto kMaxFutureSkew = std::chrono::milliseconds(250);
    const auto age = now - timestamp;
    return age >= -kMaxFutureSkew && age <= kMaxReportGap;
}

// Accept only complete DRM hangcheck reports from this device's Adreno 506.
class HangcheckReader {
public:
    using Clock = std::chrono::steady_clock;

    std::optional<HungRing> Read(std::string_view message, Clock::time_point now) {
        constexpr std::string_view prefix = "[drm:hangcheck_handler [msm]] *ERROR* 05000600:";
        if (!message.starts_with(prefix)) return {};
        message.remove_prefix(prefix.size());
        const auto first = message.find_first_not_of(' ');
        if (first == std::string_view::npos) return {};
        message.remove_prefix(first);

        constexpr std::string_view detected = "hangcheck detected gpu lockup rb ";
        constexpr std::string_view completed = "completed fence: ";
        constexpr std::string_view submitted = "submitted fence: ";
        if (message.starts_with(detected)) {
            ring_ = message.ends_with('!') ? Number(message.substr(detected.size(),
                                                      message.size() - detected.size() - 1))
                                           : std::nullopt;
            completed_.reset();
            start_ = now;
        } else if (ring_ && now - start_ <= std::chrono::milliseconds(500)) {
            if (message.starts_with(completed)) {
                completed_ = Number(message.substr(completed.size()));
            } else if (message.starts_with(submitted)) {
                auto tail = Number(message.substr(submitted.size()));
                const auto ring = *ring_;
                ring_.reset();
                if (completed_ && tail) {
                    // Hardware fences are wrapping 32-bit sequence numbers.
                    const uint32_t outstanding = *tail - *completed_;
                    if (outstanding && outstanding < (uint32_t{1} << 31)) {
                        return HungRing{ring, *completed_, *tail};
                    }
                }
            }
        } else {
            ring_.reset();
        }
        return {};
    }

private:
    static std::optional<uint32_t> Number(std::string_view text) {
        uint32_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size()) return {};
        return value;
    }

    std::optional<uint32_t> ring_;
    std::optional<uint32_t> completed_;
    Clock::time_point start_;
};

// Require fresh, repeated reports on the same ring and no completed work.
class Watchdog {
public:
    using Clock = HangcheckReader::Clock;

    explicit Watchdog(Clock::time_point start) : start_(start), lastHang_(start) {}

    bool Stalled(HungRing sample, Clock::time_point now) {
        if (last_ != sample || now - lastHang_ > kMaxReportGap || now < lastHang_) {
            stuckSince_ = now;
        }
        last_ = sample;
        lastHang_ = now;
        return now - start_ >= std::chrono::minutes(1) &&
               now - stuckSince_ >= std::chrono::seconds(25);
    }

    // Rearm after a quiet boot, not merely because the device rebooted.
    bool Quiet(Clock::time_point now) const {
        return now - lastHang_ >= std::chrono::minutes(15);
    }

private:
    Clock::time_point start_;
    Clock::time_point lastHang_;
    Clock::time_point stuckSince_;
    std::optional<HungRing> last_;
};

}  // namespace thinksmart
