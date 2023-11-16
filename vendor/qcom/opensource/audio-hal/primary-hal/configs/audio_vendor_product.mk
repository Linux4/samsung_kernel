#Audio product definitions 
include vendor/qcom/opensource/audio-hal/primary-hal/configs/audio-generic-modules.mk
PRODUCT_PACKAGES += $(AUDIO_GENERIC_MODULES)

PRODUCT_PACKAGES_DEBUG += $(MM_AUDIO_DBG)

#----------------------------------------------------------------------
# audio specific
#----------------------------------------------------------------------
TARGET_USES_AOSP := false
TARGET_USES_AOSP_FOR_AUDIO := false
ifeq ($(TARGET_USES_QMAA_OVERRIDE_AUDIO), false)
ifeq ($(TARGET_USES_QMAA),true)
AUDIO_USE_STUB_HAL := true
TARGET_USES_AOSP_FOR_AUDIO := true
-include $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/common/default.mk
else
# Audio hal configuration file
-include $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/$(TARGET_BOARD_PLATFORM)/$(TARGET_BOARD_PLATFORM).mk
endif
else
# Audio hal configuration file
-include $(TOPDIR)vendor/qcom/opensource/audio-hal/primary-hal/configs/$(TARGET_BOARD_PLATFORM)/$(TARGET_BOARD_PLATFORM).mk
endif

ifeq ($(AUDIO_USE_STUB_HAL), true)
PRODUCT_COPY_FILES += \
    frameworks/av/services/audiopolicy/config/audio_policy_configuration_generic.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/primary_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/surround_sound_configuration_5_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio/surround_sound_configuration_5_0.xml
endif

# Pro Audio feature
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.audio.pro.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.audio.pro.xml

SOONG_CONFIG_qtiaudio_var00 := false
SOONG_CONFIG_qtiaudio_var11 := false
SOONG_CONFIG_qtiaudio_var22 := false

ifneq ($(BUILD_AUDIO_TECHPACK_SOURCE), true)
    SOONG_CONFIG_qtiaudio_var00 := true
    SOONG_CONFIG_qtiaudio_var11 := true
    SOONG_CONFIG_qtiaudio_var22 := true
endif
ifeq (,$(wildcard $(QCPATH)/mm-audio-noship))
    SOONG_CONFIG_qtiaudio_var11 := true
endif
ifeq (,$(wildcard $(QCPATH)/mm-audio))
    SOONG_CONFIG_qtiaudio_var22 := true
endif