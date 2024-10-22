LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

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
    libfmq \
    libhardware \
    libbase \
    vendor.qti.hardware.pal@1.0 \
    android.hidl.allocator@1.0 \
    android.hidl.memory@1.0 \
    libhidlmemory

LOCAL_EXPORT_HEADER_LIBRARY_HEADERS := libarpal_headers
LOCAL_HEADER_LIBRARIES := libarpal_headers
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
