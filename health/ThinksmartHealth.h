// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <health-impl/Health.h>

namespace aidl::android::hardware::health {

class ThinksmartHealth final : public Health {
  public:
    using Health::Health;

    ndk::ScopedAStatus getBatteryHealthData(BatteryHealthData* out) override {
        *out = {};
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

  protected:
    void UpdateHealthInfo(HealthInfo* info) override {
        // The fixed DC input has no online sensor; without it the device cannot run.
        info->chargerAcOnline = true;
        info->batteryHealthData.reset();
    }
};

}  // namespace aidl::android::hardware::health
