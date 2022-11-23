ifeq ($(BOARD_WLAN_DEVICE),qcwcn)
LOCAL_PATH := $(call my-dir)

ifneq ($(filter q4% b4% e4% v4%, $(TARGET_PRODUCT)), )
    include $(LOCAL_PATH)/cape/qcacld-3.0/Android.mk
else
    include $(LOCAL_PATH)/waipio/qcacld-3.0/Android.mk
endif
endif