// SPDX-License-Identifier: Apache-2.0

#include <android-base/logging.h>
#include <android/binder_interface_utils.h>
#include <health/utils.h>

#include <memory>
#include <utility>

#include "ThinksmartHealth.h"

int main(int, [[maybe_unused]] char** argv) {
#ifdef __ANDROID_RECOVERY__
    android::base::InitLogging(argv, android::base::KernelLogger);
#endif
    using aidl::android::hardware::health::HalHealthLoop;
    using aidl::android::hardware::health::ThinksmartHealth;

    auto config = std::make_unique<healthd_config>();
    android::hardware::health::InitHealthdConfig(config.get());
    auto health = ndk::SharedRefBase::make<ThinksmartHealth>("default", std::move(config));
    LOG(INFO) << "Starting batteryless AC-powered health HAL.";
    auto loop = std::make_shared<HalHealthLoop>(health, health);
    return loop->StartLoop();
}
