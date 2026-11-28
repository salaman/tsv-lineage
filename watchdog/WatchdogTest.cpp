// SPDX-License-Identifier: Apache-2.0

#include "Watchdog.h"

#include <gtest/gtest.h>
#include <string>

namespace thinksmart {
namespace {

using namespace std::chrono_literals;
constexpr Watchdog::Clock::time_point kStart{};
constexpr std::string_view kPrefix = "[drm:hangcheck_handler [msm]] *ERROR* 05000600: ";

TEST(KernelLogTimestampTest, AcceptsLiveClockSkewWithoutAcceptingStaleReports) {
    constexpr auto now = 1700000000s;
    EXPECT_TRUE(IsRecentKernelLog(now + 9ms, now));
    EXPECT_TRUE(IsRecentKernelLog(now - 2s, now));
    EXPECT_FALSE(IsRecentKernelLog(now - 3001ms, now));
    EXPECT_FALSE(IsRecentKernelLog(now + 1s, now));
}

TEST(HangcheckReaderTest, ReadsRealReportAndRejectsMissingOrInvalidFences) {
    HangcheckReader reader;
    const auto read = [&](std::string_view line, auto when) {
        return reader.Read(std::string(kPrefix) + std::string(line), kStart + when);
    };
    EXPECT_FALSE(read("submitted fence: 22683936", 0ms));
    EXPECT_FALSE(read("hangcheck detected gpu lockup rb 2!", 1ms));
    EXPECT_FALSE(read("     completed fence: 22683742", 2ms));
    const auto sample = read("     submitted fence: 22683936", 3ms);
    ASSERT_TRUE(sample);
    EXPECT_EQ(sample->ring, 2u);
    EXPECT_EQ(sample->completed, 22683742u);
    EXPECT_EQ(sample->submitted, 22683936u);

    EXPECT_FALSE(read("hangcheck detected gpu lockup rb 2!", 1s));
    EXPECT_FALSE(read("completed fence: 99ba", 1001ms));
    EXPECT_FALSE(read("submitted fence: 100", 1002ms));
    EXPECT_FALSE(read("hangcheck detected gpu lockup rb 2!", 2s));
    EXPECT_FALSE(read("completed fence: 20", 2001ms));
    EXPECT_FALSE(read("submitted fence: 20", 2002ms));
    EXPECT_FALSE(read("hangcheck detected gpu lockup rb 2!", 3s));
    EXPECT_FALSE(read("completed fence: 20", 3001ms));
    EXPECT_FALSE(read("submitted fence: 40", 4s));
}

TEST(HangcheckReaderTest, SupportsHardwareFenceWrap) {
    HangcheckReader reader;
    reader.Read(std::string(kPrefix) + "hangcheck detected gpu lockup rb 2!", kStart);
    reader.Read(std::string(kPrefix) + "completed fence: 4294967294", kStart);
    EXPECT_TRUE(reader.Read(std::string(kPrefix) + "submitted fence: 3", kStart));
}

TEST(WatchdogTest, RebootsOnlyAfterBootGraceAndSustainedStall) {
    Watchdog watchdog(kStart);
    for (auto elapsed = 0s; elapsed < 60s; elapsed += 1s) {
        EXPECT_FALSE(watchdog.Stalled({2, 50, 90}, kStart + elapsed));
    }
    EXPECT_TRUE(watchdog.Stalled({2, 50, 90}, kStart + 60s));

    Watchdog afterBoot(kStart);
    for (auto elapsed = 60s; elapsed < 85s; elapsed += 1s) {
        EXPECT_FALSE(afterBoot.Stalled({2, 50, 90}, kStart + elapsed));
    }
    EXPECT_TRUE(afterBoot.Stalled({2, 50, 90}, kStart + 85s));
}

TEST(WatchdogTest, IdleGapsAndRecoveredOrChangedRingsDoNotAccumulateStallTime) {
    Watchdog watchdog(kStart);
    for (auto elapsed = 60s; elapsed < 85s; elapsed += 1s) {
        EXPECT_FALSE(watchdog.Stalled({2, 50, 90}, kStart + elapsed));
    }
    EXPECT_FALSE(watchdog.Stalled({2, 51, 90}, kStart + 85s));
    EXPECT_FALSE(watchdog.Stalled({2, 51, 91}, kStart + 86s));
    EXPECT_FALSE(watchdog.Stalled({1, 51, 91}, kStart + 87s));
    EXPECT_FALSE(watchdog.Stalled({1, 51, 91}, kStart + 120s));
    EXPECT_FALSE(watchdog.Quiet(kStart + 120s + 14min));
    EXPECT_TRUE(watchdog.Quiet(kStart + 120s + 15min));
}

}  // namespace
}  // namespace thinksmart
