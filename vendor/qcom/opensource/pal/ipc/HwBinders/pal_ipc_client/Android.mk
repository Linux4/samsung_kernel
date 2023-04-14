LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES += vendor/qcom/opensource/pal
LOCAL_MODULE := libpalclient
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := \
    src/pal_client_wrapper.cpp

LOCAL_SHARED_LIBRARIES := \
    libhidlbase \
    libhidltransport \
    libutils \
    liblog \
    libcutils \
    libhardware \
    libbase \
    vendor.qti.hardware.pal@1.0

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
#LOCAL_MODULE := paltest1
#LOCAL_MODULE_OWNER := qti
#LOCAL_VENDOR_MODULE := true
#LOCAL_SRC_FILES := \
    paltest/pal_test.cpp

#LOCAL_CFLAGS += -v


#LOCAL_SHARED_LIBRARIES := \
    libutils \
    libpalclient\
    liblog

#include $(BUILD_EXECUTABLE)
