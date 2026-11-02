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
TARGET_SENSORS_HAL := iio
TARGET_HAS_BATTERY := false
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
    e2fsck_ramdisk \
    fstab.thinksmart \
    fstab.thinksmart.ramdisk \
    init.thinksmart.rc \
    init.recovery.thinksmart.rc

PRODUCT_PACKAGES_DEBUG += \
    fsverity \
    tinymix \
    tinycap \
    tinyplay

DEVICE_PACKAGE_OVERLAYS += $(DEVICE_PATH)/overlays/overlay

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/audio/audio.thinksmart.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio.thinksmart.xml \
    $(DEVICE_PATH)/audio/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml \
    $(DEVICE_PATH)/config/media_codecs_c2.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_c2.xml \
    $(DEVICE_PATH)/config/thermal.json:$(TARGET_COPY_OUT_VENDOR)/etc/thermal.json \
    $(DEVICE_PATH)/permissions/thinksmart-core-hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/thinksmart-core-hardware.xml \
    $(DEVICE_PATH)/config/ueventd.thinksmart.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.thinksmart.rc \
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
    ro.vendor.qcom.soc.enable_modem_services=0 \
    ro.vendor.v4l2_codec2.decoder.supported.h264=true \
    ro.vendor.v4l2_codec2.decoder.supported.hevc=true \
    ro.vendor.v4l2_codec2.decode_concurrent_instances=1 \
    debug.stagefright.c2-poolmask=0xfe0000

PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.iio.illuminance.name=VCNL4200_ALS \
    ro.vendor.iio.illuminance.vendor=Vishay \
    ro.vendor.iio.illuminance.max_range=1572.84 \
    ro.vendor.iio.illuminance.resolution=0.024 \
    ro.vendor.iio.illuminance.max_freq=5 \
    ro.vendor.iio.illuminance.quirks=no-event,no-trig \
    ro.vendor.iio.proximity.name=VCNL4200_Proximity \
    ro.vendor.iio.proximity.vendor=Vishay \
    ro.vendor.iio.proximity.max_range=5 \
    ro.vendor.iio.proximity.resolution=5 \
    ro.vendor.iio.proximity.max_freq=2 \
    ro.vendor.iio.proximity.near_level=3 \
    ro.vendor.iio.proximity.far_level=2 \
    ro.vendor.iio.proximity.quirks=no-event,no-trig

include $(DEVICE_PATH)/config/firmware-files.mk
