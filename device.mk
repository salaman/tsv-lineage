DEVICE_PATH := device/lenovo/thinksmart

TARGET_QCOM_SOC_FAMILY := msm8953
TARGET_QCOM_SOC := apq8053
TARGET_INITIAL_BRINGUP := false
TARGET_CONSOLE_AS_ROOT := false
TARGET_ENABLE_FBKEYBOARD := false
TARGET_GRAPHICS := mesa
TARGET_GRAPHICS_EGL := mesa
TARGET_GRAPHICS_VULKAN := none
TARGET_GRAPHICS_ALLOCATOR_HAL := minigbm-upstream
TARGET_GRAPHICS_COMPOSER_HAL := drm_hwcomposer
TARGET_AUDIO_HAL := tinyhal
TARGET_THERMAL_HAL := linaro-libpm
TARGET_POWER_HAL := perfmgr-lineage
TARGET_SENSORS_HAL := iio
TARGET_HAS_BATTERY := false
TARGET_HEALTH_HAL := none
TARGET_HAS_IR := false
TARGET_HAS_VIBRATOR := false
TARGET_SUPPORTS_SUSPEND := false
TARGET_MEDIA_C2_HAL := v4l2_codec2
TARGET_V4L2_CODEC2_USE_EXAMPLE_CONFIGURATION := false

include device/mainline/qcom-common/optional/options.mk
$(call inherit-product, device/mainline/qcom-common/mainline_qcom-common.mk)
$(call inherit-product, frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)
$(call soong_config_set,mainline_common_libinit,set_properties_from,devicetree)

PRODUCT_SHIPPING_API_LEVEL := 27
PRODUCT_AAPT_CONFIG := normal large xlarge hdpi xhdpi
PRODUCT_AAPT_PREF_CONFIG := hdpi
TARGET_SCREEN_WIDTH := 800
TARGET_SCREEN_HEIGHT := 1280
TARGET_BOOTANIMATION_HALF_RES := true

PRODUCT_PACKAGES += \
    android.hardware.health-service.thinksmart \
    android.hardware.health-service.thinksmart_recovery \
    e2fsck_ramdisk \
    fstab.thinksmart \
    fstab.thinksmart.ramdisk \
    init.thinksmart.rc \
    init.recovery.thinksmart.rc \
    watchdogd \
    watchdogd.recovery

# Keep IPowerStats when replacing the APEX that bundled it with example IPower.
PRODUCT_PACKAGES += android.hardware.power.stats-service.example

# The common libcamera recipe also declares unqualified FULL and RAW features.
PRODUCT_PACKAGES += \
    com.android.hardware.camera.libcamera \
    android.hardware.camera.front.prebuilt.xml \
    libcamera_camera_hal.yaml \
    libcamera_ipa_simple_s5kc505a.yaml \
    libcamera_ipa_simple_uncalibrated.yaml

PRODUCT_PACKAGES_DEBUG += \
    cam \
    fsverity \
    media-ctl \
    tinymix \
    tinycap \
    tinyplay \
    v4l2-ctl

DEVICE_PACKAGE_OVERLAYS += $(DEVICE_PATH)/overlays/overlay

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/audio/audio.thinksmart.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio.thinksmart.xml \
    $(DEVICE_PATH)/audio/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml \
    $(DEVICE_PATH)/config/media_codecs_c2.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_c2.xml \
    $(DEVICE_PATH)/config/thermal.json:$(TARGET_COPY_OUT_VENDOR)/etc/thermal.json \
    $(DEVICE_PATH)/permissions/thinksmart-core-hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/thinksmart-core-hardware.xml \
    $(DEVICE_PATH)/config/ueventd.thinksmart.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.thinksmart.rc \
    $(DEVICE_PATH)/config/generic_ft5x06__09_.idc:$(TARGET_COPY_OUT_VENDOR)/usr/idc/generic_ft5x06__09_.idc \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml

PRODUCT_SOONG_NAMESPACES += \
    $(DEVICE_PATH) \
    kernel/mainline/configs

PRODUCT_SYSTEM_PROPERTIES += \
    debug.renderengine.backend=skiagl \
    ro.sf.lcd_density=160 \
    persist.sys.zram_enabled=1 \
    sys.use_memfd=true

PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.camera=libcamera \
    ro.vendor.qcom.soc.enable_modem_services=0 \
    vendor.powerhal.disp.idle_support=0 \
    vendor.powerhal.interaction.min=1000 \
    vendor.powerhal.interaction.max=2000 \
    vendor.powerhal.interaction.offset=0 \
    ro.vendor.v4l2_codec2.decoder.supported.h264=true \
    ro.vendor.v4l2_codec2.decoder.supported.hevc=true \
    ro.vendor.v4l2_codec2.decode_concurrent_instances=1 \
    debug.stagefright.c2-poolmask=0xfe0000

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.iio.accel.name=BMA253_Accelerometer \
    ro.vendor.iio.accel.vendor=Bosch \
    ro.vendor.iio.accel.max_range=39.2266 \
    ro.vendor.iio.accel.resolution=0.019154 \
    ro.vendor.iio.accel.max_freq=125 \
    ro.vendor.iio.accel.quirks=continuous \
    ro.vendor.iio.temp.bma253.quirks=no-poll,no-trig,no-event \
    ro.vendor.iio.illuminance.name=VCNL4200_ALS \
    ro.vendor.iio.illuminance.vendor=Vishay \
    ro.vendor.iio.illuminance.opt_scale=12.5 \
    ro.vendor.iio.illuminance.max_range=19660.5 \
    ro.vendor.iio.illuminance.resolution=0.3 \
    ro.vendor.iio.illuminance.max_freq=5 \
    ro.vendor.iio.illuminance.quirks=no-event,no-trig \
    ro.vendor.iio.proximity.name=VCNL4200_Proximity \
    ro.vendor.iio.proximity.vendor=Vishay \
    ro.vendor.iio.proximity.max_range=5 \
    ro.vendor.iio.proximity.resolution=5 \
    ro.vendor.iio.proximity.max_freq=2 \
    ro.vendor.iio.proximity.near_level=0.875 \
    ro.vendor.iio.proximity.far_level=0.625 \
    ro.vendor.iio.proximity.quirks=no-event,no-trig

include $(DEVICE_PATH)/config/firmware-files.mk
