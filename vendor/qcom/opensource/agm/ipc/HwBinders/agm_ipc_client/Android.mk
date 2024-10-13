LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libagmclient_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc
LOCAL_VENDOR_MODULE := true

include $(BUILD_HEADER_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libagmclient
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES := \
    src/agm_client_wrapper.cpp\
    src/AGMCallback.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/inc

LOCAL_SHARED_LIBRARIES := \
    libhidlbase \
    libutils \
    liblog \
    libcutils \
    libhardware \
    libbase \
    vendor.qti.hardware.AGMIPC@1.0

LOCAL_HEADER_LIBRARIES := libagm_headers

include $(BUILD_SHARED_LIBRARY)
