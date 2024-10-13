ifneq ($(AUDIO_USE_STUB_HAL), true)
ifeq ($(TARGET_USES_QCOM_MM_AUDIO),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE             := arm
LOCAL_MODULE               := sound_trigger.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS          := optional
LOCAL_MODULE_OWNER         := qti
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MULTILIB             := $(AUDIOSERVER_MULTILIB)
LOCAL_VENDOR_MODULE        := true

LOCAL_CFLAGS += -Wall -Werror
LOCAL_CFLAGS += -DSOUND_TRIGGER_PLATFORM=$(TARGET_BOARD_PLATFORM)

LOCAL_C_INCLUDES += \
    system/media/audio_utils/include \
    external/expat/lib \
    vendor/qcom/opensource/core-utils/fwk-detect \
    vendor/qcom/opensource/pal \
    vendor/qcom/opensource/audio-hal/primary-hal/hal/audio_extn \
    vendor/qcom/opensource/audio-hal/primary-hal/hal

# { SEC_AUDIO_COMMON
SEC_AUDIO_VARS := vendor/samsung/variant/audio/sec_audioreach_vars.mk
include $(SEC_AUDIO_VARS)
# } SEC_AUDIO_COMMON

LOCAL_SRC_FILES := \
    SoundTriggerDevice.cpp \
    SoundTriggerSession.cpp

LOCAL_HEADER_LIBRARIES := \
    libhardware_headers \
    libsystem_headers

LOCAL_SHARED_LIBRARIES := \
    libbase \
    liblog \
    libcutils \
    libdl \
    libaudioutils \
    libexpat \
    libhidlbase \
    libprocessgroup \
    libutils \
    libar-pal

ifeq ($(strip $(AUDIO_FEATURE_ENABLED_LSM_HIDL)),true)
    LOCAL_HEADER_LIBRARIES += liblisten_headers

    LOCAL_SHARED_LIBRARIES += \
        vendor.qti.hardware.ListenSoundModel@1.0-impl \
        vendor.qti.hardware.ListenSoundModel@1.0

    LOCAL_CFLAGS += -DLSM_HIDL_ENABLED
endif

include $(BUILD_SHARED_LIBRARY)
endif #TARGET_USES_QCOM_MM_AUDIO
endif #AUDIO_USE_STUB_HAL
