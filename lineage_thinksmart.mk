$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, vendor/lineage/config/common_mini_tablet_wifionly.mk)
$(call inherit-product, device/lenovo/thinksmart/device.mk)

ifeq ($(TARGET_BUILD_VARIANT),userdebug)
# Make serializes any nonempty value (including "false") as true here.
PRODUCT_NOT_DEBUGGABLE_IN_USERDEBUG :=
endif

PRODUCT_NAME := lineage_thinksmart
PRODUCT_DEVICE := thinksmart
PRODUCT_BRAND := Lenovo
PRODUCT_MANUFACTURER := Lenovo
PRODUCT_MODEL := ThinkSmart View
