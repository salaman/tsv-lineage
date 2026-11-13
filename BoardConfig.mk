DEVICE_PATH := device/lenovo/thinksmart
include device/mainline/qcom-common/BoardConfigMainlineQcomCommon.mk
$(call soong_config_set,mainline_qcom_common_soc,primary_audio_policy_configuration_variant,none)

# Resolve only the fallback, not the namespace's duplicate Cuttlefish binaries.
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := //external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant:lib_driver_cmd_fallback

TARGET_BOOTLOADER_BOARD_NAME := CD-18781Y
AB_OTA_UPDATER := false
BOARD_AVB_ENABLE := false
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_PRODUCT := system/product
TARGET_COPY_OUT_SYSTEM_EXT := system/system_ext
TARGET_USERIMAGES_USE_EXT4 := true

BOARD_BOOT_HEADER_VERSION := 0
BOARD_MKBOOTIMG_ARGS += --header_version 0
BOARD_RAMDISK_USE_LZ4 := true
# Existing lk2nd uses a legacy header and a DTB appended after the gzip stream.
BOARD_INCLUDE_DTB_IN_BOOTIMG :=
BOARD_KERNEL_APPEND_DTBS := qcom/apq8053-lenovo-cd-18781y.dtb
# Reserve the existing lk2nd prefix; boot.img itself is only the Android payload.
BOARD_BOOTIMAGE_PARTITION_SIZE := 33030144
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 33554432
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1610612736
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_PARTITION_SIZE := 402653184
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_CACHEIMAGE_PARTITION_SIZE := 132120576
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_USES_METADATA_PARTITION := true
BOARD_USERDATAIMAGE_PARTITION_SIZE := 4290223616
BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := ext4

# UART5 belongs to Bluetooth; do not inherit the Qualcomm serial console.
MAINLINE_QCOM_KERNEL_PARAMS :=
MAINLINE_COMMON_ANDROIDBOOT_PARAMS := $(filter-out androidboot.boot_devices=any,$(MAINLINE_COMMON_ANDROIDBOOT_PARAMS))
MAINLINE_COMMON_KERNEL_PARAMS := $(filter-out binder.impl=rust panic=-1,$(MAINLINE_COMMON_KERNEL_PARAMS))
BOARD_KERNEL_CMDLINE := \
    $(MAINLINE_COMMON_ANDROIDBOOT_PARAMS) \
    $(MAINLINE_COMMON_KERNEL_PARAMS) \
    $(MAINLINE_QCOM_SOC_ANDROIDBOOT_PARAMS) \
    $(MAINLINE_QCOM_SOC_KERNEL_PARAMS) \
    androidboot.hardware=thinksmart \
    androidboot.verifiedbootstate=orange \
    panic=0 \
    lk2nd.pass-simplefb=relocate,autorefresh

THINKSMART_BOOT_CONSOLE ?= false
ifeq ($(THINKSMART_BOOT_CONSOLE),true)
BOARD_KERNEL_CMDLINE += androidboot.serialconsole=1 console=tty0
else
BOARD_KERNEL_CMDLINE += androidboot.serialconsole=0 console=null
endif

BOARD_MESA3D_VULKAN_DRIVERS :=
$(call soong_config_set,minigbm_upstream,platform,msm)

TARGET_KERNEL_SOURCE := kernel/mainline/msm8953-mainline
TARGET_KERNEL_CONFIG_EXT := \
    $(DEVICE_PATH)/upstream/xiaomi-mainline/mi8953_a/kconfigs/config-postmarketos-qcom-msm8953.aarch64 \
    kernel/mainline/configs/fragments/android-base-pre/common.config \
    kernel/mainline/configs/fragments/android-base-pre/arm64.config \
    kernel/configs/b/android-6.12/android-base.config \
    kernel/mainline/configs/fragments/android-base-conditional/CONFIG_ARM64-y.config \
    kernel/mainline/configs/fragments/common.config \
    $(DEVICE_PATH)/kconfigs/hardware.config \
    $(DEVICE_PATH)/kconfigs/thinksmart.config
MERGE_ALL_KERNEL_CONFIGS_AT_ONCE := true
TARGET_KERNEL_ADDITIONAL_FLAGS := LLVM=1
TARGET_KERNEL_DTB := qcom/apq8053-lenovo-cd-18781y.dtb
TARGET_DTB_LIST_WILDCARD := qcom/apq8053-lenovo-cd-18781y
TARGET_AUTO_COLLECT_KERNEL_MODULE_DEPS := true
BOARD_VENDOR_KERNEL_MODULES_LOAD := \
    qcom-pon.ko \
    rtc-pm8xxx.ko \
    qrtr.ko \
    qrtr-smd.ko \
    qcom_q6v5_pas.ko \
    q6afe-clocks.ko \
    snd-soc-apq8016-sbc.ko \
    snd-soc-msm8916-analog.ko \
    snd-soc-msm8916-digital.ko \
    q6afe-dai.ko \
    q6asm-dai.ko \
    q6routing.ko \
    q6voice-dai.ko \
    snd-soc-tas5782m.ko \
    msm.ko \
    panel-lenovo-cd-18781y-ft8201.ko \
    panel-lenovo-cd-18781y-hx83100a.ko \
    edt-ft5x06.ko \
    himax_hx83112b.ko \
    vcnl4000.ko \
    bmc150-accel-i2c.ko \
    venus-core.ko \
    venus-dec.ko \
    venus-enc.ko \
    i2c-qcom-cci.ko \
    s5kc505a.ko \
    qcom-camss.ko \
    ath10k_sdio.ko \
    hci_uart.ko

BOARD_RECOVERY_KERNEL_MODULES_LOAD := \
    qcom-pon.ko \
    msm.ko \
    panel-lenovo-cd-18781y-ft8201.ko \
    panel-lenovo-cd-18781y-hx83100a.ko \
    edt-ft5x06.ko \
    himax_hx83112b.ko
RECOVERY_KERNEL_MODULES := $(BOARD_RECOVERY_KERNEL_MODULES_LOAD)

TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/config/fstab.thinksmart
TARGET_RECOVERY_DENSITY := hdpi
DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/config/manifest.xml
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/private
