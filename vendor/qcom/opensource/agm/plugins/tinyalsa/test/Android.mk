LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE        := libagmmixer
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS        += -Wno-unused-parameter -Wno-unused-result
LOCAL_SRC_FILES     := agmmixer.c

LOCAL_HEADER_LIBRARIES := \
    libagm_headers \
    libacdb_headers

#if android version is R, refer to qtitinyxx otherwise use upstream ones
#This assumes we would be using AR code only for Android R and subsequent versions.
ifneq ($(filter 11 R, $(PLATFORM_VERSION)),)
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif


LOCAL_SHARED_LIBRARIES += \
     libexpat

include $(BUILD_SHARED_LIBRARY)

# Build agmplay
include $(CLEAR_VARS)

LOCAL_MODULE        := agmplay
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS        += -Wno-unused-parameter -Wno-unused-result
LOCAL_CFLAGS        += -DBACKEND_CONF_FILE=\"/vendor/etc/backend_conf.xml\"
LOCAL_SRC_FILES     := agmplay.c

LOCAL_HEADER_LIBRARIES := \
    libagm_headers \
    libacdb_headers

#if android version is R, refer to qtitinyxx otherwise use upstream ones
#This assumes we would be using AR code only for Android R and subsequent versions.
ifneq ($(filter 11 R, $(PLATFORM_VERSION)),)
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif

LOCAL_SHARED_LIBRARIES += \
    libagmmixer

include $(BUILD_EXECUTABLE)
include $(CLEAR_VARS)

LOCAL_MODULE        := agmcap
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS        += -Wno-unused-parameter -Wno-unused-result
LOCAL_CFLAGS        += -DBACKEND_CONF_FILE=\"/vendor/etc/backend_conf.xml\"
LOCAL_SRC_FILES     := agmcap.c

LOCAL_HEADER_LIBRARIES := \
    libagm_headers \
    libacdb_headers

#if android version is R, refer to qtitinyxx otherwise use upstream ones
#This assumes we would be using AR code only for Android R and subsequent versions.
ifneq ($(filter 11 R, $(PLATFORM_VERSION)),)
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif

LOCAL_SHARED_LIBRARIES += \
    libagmmixer

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE        := agmcompressplay
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS        += -Wno-unused-parameter -Wno-unused-result
LOCAL_CFLAGS        += -DBACKEND_CONF_FILE=\"/vendor/etc/backend_conf.xml\"

LOCAL_C_INCLUDES    += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_SRC_FILES     := agmcompressplay.c

LOCAL_HEADER_LIBRARIES := \
    libagm_headers \
    libacdb_headers

#if android version is R, refer to qtitinyxx otherwise use upstream ones
#This assumes we would be using AR code only for Android R and subsequent versions.
ifneq ($(filter 11 R, $(PLATFORM_VERSION)),)
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinycompress/include
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa\
                          libqti-tinycompress
else
LOCAL_C_INCLUDES += $(TOP)/external/tinycompress/include
LOCAL_SHARED_LIBRARIES += libtinyalsa\
                          libtinycompress
endif

LOCAL_SHARED_LIBRARIES += \
    libagmmixer

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE        := agmvoiceui
LOCAL_MODULE_OWNER  := qti
LOCAL_MODULE_TAGS   := optional
LOCAL_VENDOR_MODULE := true

LOCAL_CFLAGS        += -Wno-unused-parameter -Wno-unused-result
LOCAL_CFLAGS        += -DBACKEND_CONF_FILE=\"/vendor/etc/backend_conf.xml\"
LOCAL_SRC_FILES     := agm_voiceui.c

LOCAL_HEADER_LIBRARIES := \
    libagm_headers \
    libacdb_headers

#if android version is R, refer to qtitinyxx otherwise use upstream ones
#This assumes we would be using AR code only for Android R and subsequent versions.
ifneq ($(filter 11 R, $(PLATFORM_VERSION)),)
LOCAL_C_INCLUDES += $(TOP)/vendor/qcom/opensource/tinyalsa/include
LOCAL_SHARED_LIBRARIES += libqti-tinyalsa
else
LOCAL_SHARED_LIBRARIES += libtinyalsa
endif

LOCAL_SHARED_LIBRARIES += \
    libagmmixer

include $(BUILD_EXECUTABLE)
