LOCAL_PATH := $(call my-dir)
# Build libagm_headers
include $(CLEAR_VARS)
LOCAL_MODULE                := libagm_headers
LOCAL_VENDOR_MODULE         := true
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc/public
include $(BUILD_HEADER_LIBRARY)

# Build libagm
include $(CLEAR_VARS)

LOCAL_MODULE        := libagm
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS        := -D_ANDROID_
LOCAL_CFLAGS        += -Wno-tautological-compare -Wno-macro-redefined -Wall
LOCAL_CFLAGS        += -D_GNU_SOURCE -DACDB_PATH=\"/vendor/etc/acdbdata/\"
$(info [ZS]LOCAL_CFLAGS ACDB_PATH is $(LOCAL_CFLAGS))
LOCAL_CFLAGS        += -D_GNU_SOURCE_USER -DACDB_PATH_USER=\"/vendor/etc/acdbdata/user/\"
$(info [ZS]LOCAL_CFLAGS ACDB_PATH_USER is $(LOCAL_CFLAGS))
LOCAL_CFLAGS        += -D_GNU_SOURCE_FACTORY -DACDB_PATH_FACTORY=\"/vendor/etc/acdbdata/factory/\"
$(info [ZS]LOCAL_CFLAGS ACDB_PATH_FACTORY is $(LOCAL_CFLAGS))
LOCAL_CFLAGS        += -DACDB_DELTA_FILE_PATH="/data/vendor/audio/acdbdata/delta"

LOCAL_C_INCLUDES    := $(LOCAL_PATH)/inc/public
LOCAL_C_INCLUDES    += $(LOCAL_PATH)/inc/private

#if android version is R, use qtitinyalsa headers otherwise use upstream ones
#This assumes we would be using AR code only for Android R and subsequent versions.
ifneq ($(filter 11 R, $(PLATFORM_VERSION)),)
LOCAL_C_INCLUDES    += $(TOP)/vendor/qcom/opensource/tinyalsa/include
endif

ifeq (FACTORY, $(SEC_SAMSUNG_EXPERIENCE_PLATFORM_CATEGORY))
LOCAL_CFLAGS        += -DACDB_FACTORY
$(info [ZS]ACDB_FACTORY is $(LOCAL_CFLAGS))
else
LOCAL_CFLAGS        += -DACDB_USER
$(info [ZS]ACDB_USER is $(LOCAL_CFLAGS))
endif

ifeq (sep_lite_new, $(SEC_SAMSUNG_EXPERIENCE_PLATFORM_CATEGORY))
LOCAL_CFLAGS        += -DSEC_AUDIO_COMMON
endif

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc/public

LOCAL_SRC_FILES  := \
    src/agm.c\
    src/graph.c\
    src/graph_module.c\
    src/metadata.c\
    src/session_obj.c\
    src/device.c \
    src/utils.c \
    src/device_hw_ep.c

LOCAL_HEADER_LIBRARIES := \
    libspf-headers \
    libutils_headers \
    libacdb_headers

LOCAL_SHARED_LIBRARIES := \
    libar-gsl \
    liblog \
    liblx-osal \
    libaudioroute \
    libcutils \
    libats

#if android version is R, use qtitinyalsa lib otherwise use upstream ones
#This assumes we would be using AR code only for Android R and subsequent versions.
ifneq ($(filter R 11,$(PLATFORM_VERSION)),)
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif


ifeq ($(strip $(AUDIO_FEATURE_ENABLED_DYNAMIC_LOG)), true)
LOCAL_CFLAGS           += -DDYNAMIC_LOG_ENABLED
LOCAL_C_INCLUDES       += $(TOP)/external/expat/lib/expat.h
LOCAL_SHARED_LIBRARIES += libaudio_log_utils
LOCAL_SHARED_LIBRARIES += libexpat
LOCAL_HEADER_LIBRARIES += libaudiologutils_headers
endif

include $(BUILD_SHARED_LIBRARY)

