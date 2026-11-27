// SPDX-License-Identifier: Apache-2.0

#include "Watchdog.h"

#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <log/log.h>
#include <log/log_read.h>

namespace {

constexpr const char* kStateDir = "/data/vendor/thinksmart_gpu_watchdog";
constexpr const char* kRebootGuard = "last_reboot";
constexpr std::string_view kKernelTag = "msm_dpu 1a01000.display-controller";

std::string_view HangcheckMessage(log_msg& record, int bytes) {
    if (record.id() != LOG_ID_KERNEL || record.entry.hdr_size < sizeof(logger_entry) ||
        bytes < record.entry.hdr_size ||
        record.entry.len > bytes - record.entry.hdr_size || record.entry.len < 3) {
        return {};
    }
    const char* payload = record.msg();
    if (!payload || payload[0] != ANDROID_LOG_ERROR) return {};
    std::string_view text(payload + 1, record.entry.len - 1);
    const auto tagEnd = text.find('\0');
    if (tagEnd == std::string_view::npos || text.substr(0, tagEnd) != kKernelTag) return {};
    text.remove_prefix(tagEnd + 1);
    if (!text.ends_with('\0')) return {};
    text.remove_suffix(1);
    return text;
}

bool IsRecent(const log_msg& record) {
    using namespace std::chrono;
    const auto now = system_clock::now().time_since_epoch();
    const auto timestamp = nanoseconds(record.nsec());
    // The log buffer uses realtime; use it only to reject stale queued reports.
    return timestamp <= now && now - timestamp <= thinksmart::kMaxReportGap;
}

bool StoreRebootGuard(int dir, const std::string& reason) {
    android::base::unique_fd guard(
            openat(dir, kRebootGuard, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600));
    return guard.ok() && android::base::WriteStringToFd(reason + "\n", guard) &&
           fsync(guard) == 0 && fsync(dir) == 0;
}

}  // namespace

int main(int argc, char** argv) {
    android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));
    if (argc != 1) {
        LOG(ERROR) << "Usage: thinksmart_gpu_watchdog";
        return EXIT_FAILURE;
    }
    android::base::unique_fd state(open(kStateDir, O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (!state.ok()) {
        PLOG(ERROR) << "Could not open the GPU watchdog state directory";
        return EXIT_FAILURE;
    }
    struct stat guard;
    bool guarded = fstatat(state, kRebootGuard, &guard, AT_SYMLINK_NOFOLLOW) == 0;
    if (!guarded && errno != ENOENT) {
        PLOG(ERROR) << "Could not check the GPU reboot guard";
        return EXIT_FAILURE;
    }

    using Logger = std::unique_ptr<logger_list, decltype(&android_logger_list_free)>;
    Logger logs(android_logger_list_open(LOG_ID_KERNEL, 0, 1, 0), android_logger_list_free);
    if (!logs) {
        LOG(ERROR) << "Could not open the kernel log";
        return EXIT_FAILURE;
    }
    using thinksmart::Watchdog;
    Watchdog watchdog(Watchdog::Clock::now());
    thinksmart::HangcheckReader reader;
    bool reading = false;
    while (true) {
        log_msg record{};
        const int bytes = android_logger_list_read(logs.get(), &record);
        if (bytes <= 0) {
            LOG(ERROR) << "Kernel log reader failed: " << bytes;
            return EXIT_FAILURE;
        }
        if (!reading) {
            LOG(INFO) << "Monitoring Adreno fence progress; reboot guard " << guarded;
            reading = true;
        }
        const auto now = Watchdog::Clock::now();
        if (guarded && watchdog.Quiet(now)) {
            if (unlinkat(state, kRebootGuard, 0) || fsync(state)) {
                PLOG(ERROR) << "Could not clear the GPU reboot guard";
                return EXIT_FAILURE;
            }
            guarded = false;
            LOG(INFO) << "Rearmed the GPU watchdog after a quiet interval";
        }
        if (!IsRecent(record)) continue;
        const auto sample = reader.Read(HangcheckMessage(record, bytes), now);
        if (!sample || !watchdog.Stalled(*sample, now) || guarded) continue;

        const auto reason = android::base::StringPrintf(
                "Sustained GPU stall: ring=%u completed=%u submitted=%u",
                sample->ring, sample->completed, sample->submitted);
        if (!StoreRebootGuard(state, reason)) {
            PLOG(ERROR) << "Not rebooting: could not persist the GPU reboot guard";
            return EXIT_FAILURE;
        }
        guarded = true;
        LOG(ERROR) << "Requesting warm reboot: " << reason;
        if (!android::base::SetProperty("sys.powerctl", "reboot,gpu_watchdog")) {
            LOG(ERROR) << "Could not request the GPU watchdog reboot";
            return EXIT_FAILURE;
        }
    }
}
