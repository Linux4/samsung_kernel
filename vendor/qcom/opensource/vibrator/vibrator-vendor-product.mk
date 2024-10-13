#QTI_VIBRATOR_HAL_SERVICE := \
#      vendor.qti.hardware.vibrator.service

#PRODUCT_PACKAGES += $(QTI_VIBRATOR_HAL_SERVICE)

#PRODUCT_COPY_FILES += \
#      vendor/qcom/opensource/vibrator/excluded-input-devices.xml:vendor/etc/excluded-input-devices.xml

#ifeq ($(PLATFORM_VERSION), $(filter $(PLATFORM_VERSION),S 12))
#  SOONG_CONFIG_NAMESPACES += vibrator
#  SOONG_CONFIG_vibrator += vibratortargets
#  SOONG_CONFIG_vibrator_vibratortargets := vibratoraidlV2platformtarget
#endif

#ifeq ($(PLATFORM_VERSION), $(filter $(PLATFORM_VERSION),T 13))
#  $(call soong_config_set, vibrator, vibratortargets, vibratoraidlV2target)
#endif

#ifeq ($(PLATFORM_VERSION), $(filter $(PLATFORM_VERSION),U 14))
#  $(call soong_config_set, vibrator, vibratortargets, vibratoraidlV2target)
#endif
