// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <health/utils.h>

#include <memory>
#include <utility>

#include "ThinksmartHealth.h"

namespace aidl::android::hardware::health {

class ThinksmartHealthTest : public testing::Test {
  protected:
    void SetUp() override {
        auto config = std::make_unique<healthd_config>();
        ::android::hardware::health::InitHealthdConfig(config.get());
        health_ = ndk::SharedRefBase::make<ThinksmartHealth>("default", std::move(config));
    }

    std::shared_ptr<ThinksmartHealth> health_;
};

TEST_F(ThinksmartHealthTest, ReportsAcWithoutBatteryOrCharging) {
    HealthInfo info;
    ASSERT_TRUE(health_->getHealthInfo(&info).isOk());
    EXPECT_TRUE(info.chargerAcOnline);
    EXPECT_FALSE(info.chargerUsbOnline);
    EXPECT_FALSE(info.chargerWirelessOnline);
    EXPECT_FALSE(info.chargerDockOnline);
    EXPECT_FALSE(info.batteryPresent);
    EXPECT_EQ(BatteryStatus::UNKNOWN, info.batteryStatus);
    EXPECT_EQ(BatteryHealth::UNKNOWN, info.batteryHealth);
    EXPECT_EQ(BatteryCapacityLevel::UNSUPPORTED, info.batteryCapacityLevel);
    EXPECT_FALSE(info.batteryHealthData.has_value());
}

TEST_F(ThinksmartHealthTest, DoesNotInventMeasurements) {
    HealthInfo info;
    ASSERT_TRUE(health_->getHealthInfo(&info).isOk());
    EXPECT_EQ(0, info.batteryLevel);
    EXPECT_EQ(0, info.batteryVoltageMillivolts);
    EXPECT_EQ(0, info.batteryTemperatureTenthsCelsius);
    EXPECT_EQ(0, info.batteryCurrentMicroamps);
    EXPECT_EQ(0, info.batteryCurrentAverageMicroamps);
    EXPECT_EQ(0, info.batteryChargeCounterUah);
    EXPECT_EQ(0, info.batteryFullChargeUah);
    EXPECT_EQ(0, info.batteryFullChargeDesignCapacityUah);
    EXPECT_EQ(0, info.batteryCycleCount);
    EXPECT_TRUE(info.batteryTechnology.empty());
    EXPECT_EQ(0, info.maxChargingCurrentMicroamps);
    EXPECT_EQ(0, info.maxChargingVoltageMicrovolts);
    EXPECT_EQ(HealthInfo::BATTERY_CHARGE_TIME_TO_FULL_NOW_SECONDS_UNSUPPORTED,
              info.batteryChargeTimeToFullNowSeconds);
}

TEST_F(ThinksmartHealthTest, UnavailableBatteryPropertiesStayUnsupported) {
    int32_t value = -1;
    EXPECT_EQ(EX_UNSUPPORTED_OPERATION, health_->getCapacity(&value).getExceptionCode());
    EXPECT_EQ(EX_UNSUPPORTED_OPERATION, health_->getChargeCounterUah(&value).getExceptionCode());
    EXPECT_EQ(EX_UNSUPPORTED_OPERATION, health_->getCurrentNowMicroamps(&value).getExceptionCode());
    EXPECT_EQ(EX_UNSUPPORTED_OPERATION,
              health_->getCurrentAverageMicroamps(&value).getExceptionCode());
    int64_t energy = -1;
    EXPECT_EQ(EX_UNSUPPORTED_OPERATION, health_->getEnergyCounterNwh(&energy).getExceptionCode());
}

TEST_F(ThinksmartHealthTest, ChargeStatusRemainsUnknown) {
    BatteryStatus status = BatteryStatus::CHARGING;
    ASSERT_TRUE(health_->getChargeStatus(&status).isOk());
    EXPECT_EQ(BatteryStatus::UNKNOWN, status);
}

TEST_F(ThinksmartHealthTest, DoesNotInventBatteryIdentityOrHealthData) {
    BatteryHealthData data;
    data.batterySerialNumber = "stale";
    EXPECT_EQ(EX_UNSUPPORTED_OPERATION, health_->getBatteryHealthData(&data).getExceptionCode());
    EXPECT_FALSE(data.batterySerialNumber.has_value());
    EXPECT_FALSE(data.batteryManufacturer.has_value());
    EXPECT_FALSE(data.batteryModelName.has_value());
}

TEST_F(ThinksmartHealthTest, UpdatesWithoutAChargingSimulation) {
    ASSERT_TRUE(health_->update().isOk());
    HealthInfo info;
    ASSERT_TRUE(health_->getHealthInfo(&info).isOk());
    EXPECT_TRUE(info.chargerAcOnline);
    EXPECT_FALSE(info.batteryPresent);
    EXPECT_EQ(BatteryStatus::UNKNOWN, info.batteryStatus);
    EXPECT_FALSE(info.batteryHealthData.has_value());
}

}  // namespace aidl::android::hardware::health
