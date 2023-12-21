LOCAL_PATH:= $(call my-dir)

# ---------------------------------------------------------------------------------
#             Populate ACDB data files to file system for audconf
# ---------------------------------------------------------------------------------

include $(CLEAR_VARS)

$(shell mkdir -p $(TARGET_OUT_VENDOR_ETC)/audconf)
$(shell mkdir -p $(TARGET_OUT_VENDOR_ETC)/audconf/OPEN)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/$(TARGET_PRODUCT)/OPEN/*.acdb) $(TARGET_OUT_VENDOR_ETC)/audconf/OPEN)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/$(TARGET_PRODUCT)/OPEN/*.qwsp) $(TARGET_OUT_VENDOR_ETC)/audconf/OPEN)

ifeq ($(TARGET_PRODUCT),$(filter $(TARGET_PRODUCT),gts9xxx gts9wifixx gts9pxxx gts9pwifixx gts9uxxx gts9uwifixx))
$(shell mkdir -p $(TARGET_OUT_VENDOR_ETC)/audconf/INS)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/$(TARGET_PRODUCT)/INS/*.acdb) $(TARGET_OUT_VENDOR_ETC)/audconf/INS)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/$(TARGET_PRODUCT)/INS/*.qwsp) $(TARGET_OUT_VENDOR_ETC)/audconf/INS)
endif

ifeq ($(TARGET_PRODUCT),$(filter $(TARGET_PRODUCT),gts9xxx gts9wifixx gts9uxxx gts9uwifixx))
$(shell mkdir -p $(TARGET_OUT_VENDOR_ETC)/audconf/SLK)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/$(TARGET_PRODUCT)/SLK/*.acdb) $(TARGET_OUT_VENDOR_ETC)/audconf/SLK)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/$(TARGET_PRODUCT)/SLK/*.qwsp) $(TARGET_OUT_VENDOR_ETC)/audconf/SLK)
endif

ifeq ($(shell if [ -d $(TARGET_OUT_VENDOR)/firmware ]; then echo true; fi), true)
    $(info "$(TARGET_OUT_VENDOR)/firmware exists")
else
    $(shell mkdir -p $(TARGET_OUT_VENDOR)/firmware)
endif

$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/*.wmfw) $(TARGET_OUT_VENDOR)/firmware)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/*.bin) $(TARGET_OUT_VENDOR)/firmware)
$(shell cp -rf $(wildcard $(LOCAL_PATH)/audconf/$(PROJECT_NAME)/*.cnt) $(TARGET_OUT_VENDOR)/firmware)

# ---------------------------------------------------------------------------------
#                     END
# ---------------------------------------------------------------------------------

