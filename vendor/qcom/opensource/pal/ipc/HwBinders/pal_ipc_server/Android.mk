LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := vendor.qti.hardware.pal@1.0-impl
LOCAL_MODULE_OWNER := qti
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -v
LOCAL_SRC_FILES := \
    src/pal_server_wrapper.cpp

LOCAL_C_INCLUDES := \
    $(TOP)/vendor/qcom/opensource/pal/utils/inc

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/inc

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
    libar-pal \
    android.hidl.allocator@1.0 \
    android.hidl.memory@1.0 \
    libhidlmemory

LOCAL_HEADER_LIBRARIES := \
    libspf-headers \
    libarosal_headers \
    libacdb_headers

include $(BUILD_SHARED_LIBRARY)

