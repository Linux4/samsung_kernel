LOCAL_PATH := $(call my-dir)

################# wpa_supplicant_config #################
include $(CLEAR_VARS)
LOCAL_MODULE := wpa_supplicant_config
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES :=   \
    p2p_supplicant.conf  \
    wpa_supplicant_overlay.conf
include $(BUILD_PHONY_PACKAGE)

#################################################

include $(CLEAR_VARS)
LOCAL_MODULE := p2p_supplicant.conf
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/wifi
LOCAL_MODULE_OWNER := samsung
LOCAL_SRC_FILES := p2p_supplicant.conf
include $(BUILD_PREBUILT)

#################################################