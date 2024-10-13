# Android makefile for display kernel modules

TOUCH_DLKM_ENABLE := true
ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
	ifeq ($(TARGET_KERNEL_DLKM_TOUCH_OVERRIDE), false)
		TOUCH_DLKM_ENABLE := false
	endif
endif

ifeq ($(TOUCH_DLKM_ENABLE),  true)
	TOUCH_SELECT := CONFIG_MSM_TOUCH=m

	LOCAL_PATH := $(call my-dir)
	include $(CLEAR_VARS)

	# This makefile is only for DLKM
	ifneq ($(findstring vendor,$(LOCAL_PATH)),)

	ifneq ($(findstring opensource,$(LOCAL_PATH)),)
		TOUCH_BLD_DIR := $(shell pwd)/vendor/qcom/opensource/touch-drivers
	endif # opensource

	DLKM_DIR := $(TOP)/device/qcom/common/dlkm

	LOCAL_ADDITIONAL_DEPENDENCIES := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)

	# Build
	###########################################################
	# This is set once per LOCAL_PATH, not per (kernel) module
	KBUILD_OPTIONS := TOUCH_ROOT=$(TOUCH_BLD_DIR)

	KBUILD_OPTIONS += MODNAME=touch_dlkm
	KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
	KBUILD_OPTIONS += $(TOUCH_SELECT)
	ifeq ($(SEC_SAMSUNG_EXPERIENCE_PLATFORM_CATEGORY), FACTORY)
		KBUILD_OPTIONS += KCPPFLAGS=-DFACTORY_MODE_GKI_DISABLE
	endif

	###########################################################
	include $(CLEAR_VARS)
	LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := nt36xxx-i2c.ko
	LOCAL_MODULE_KBUILD_NAME  := nt36xxx-i2c.ko
	LOCAL_MODULE_TAGS         := optional
	#LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
	###########################################################

	###########################################################
	include $(CLEAR_VARS)
	LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := ilitek.ko
	LOCAL_MODULE_KBUILD_NAME  := ilitek.ko
	LOCAL_MODULE_TAGS         := optional
	#LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
	###########################################################

	###########################################################
	include $(CLEAR_VARS)
	LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := icnl9922c.ko
	LOCAL_MODULE_KBUILD_NAME  := icnl9922c.ko
	LOCAL_MODULE_TAGS         := optional
	#LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
	###########################################################


	###########################################################
	include $(CLEAR_VARS)
	LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := hx83112f.ko
	LOCAL_MODULE_KBUILD_NAME  := hx83112f.ko
	LOCAL_MODULE_TAGS         := optional
	#LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
	###########################################################

        ###########################################################
        include $(CLEAR_VARS)
        LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
        LOCAL_MODULE              := lct_tp.ko
        LOCAL_MODULE_KBUILD_NAME  := lct_tp.ko
        LOCAL_MODULE_TAGS         := optional
        #LOCAL_MODULE_DEBUG_ENABLE := true
        LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
        include $(DLKM_DIR)/Build_external_kernelmodule.mk
        ###########################################################

    ###########################################################
    include $(CLEAR_VARS)
    LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
    LOCAL_MODULE              := tp_info.ko
    LOCAL_MODULE_KBUILD_NAME  := tp_info.ko
    LOCAL_MODULE_TAGS         := optional
    #LOCAL_MODULE_DEBUG_ENABLE := true
    LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
    include $(DLKM_DIR)/Build_external_kernelmodule.mk
    ###########################################################

ifneq ($(SEC_SAMSUNG_EXPERIENCE_PLATFORM_CATEGORY), sep_lite_new)
    ###########################################################
    include $(CLEAR_VARS)
    LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
    LOCAL_MODULE              := sec.ko
    LOCAL_MODULE_KBUILD_NAME  := sec.ko
    LOCAL_MODULE_TAGS         := optional
    #LOCAL_MODULE_DEBUG_ENABLE := true
    LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
    include $(DLKM_DIR)/Build_external_kernelmodule.mk
    ###########################################################
endif
    ###########################################################
    include $(CLEAR_VARS)
    LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
    LOCAL_MODULE              := cmd.ko
    LOCAL_MODULE_KBUILD_NAME  := cmd.ko
    LOCAL_MODULE_TAGS         := optional
    #LOCAL_MODULE_DEBUG_ENABLE := true
    LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
    include $(DLKM_DIR)/Build_external_kernelmodule.mk
    ###########################################################

	###########################################################
	include $(CLEAR_VARS)
	LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := goodix_ts.ko
	LOCAL_MODULE_KBUILD_NAME  := goodix_ts.ko
	LOCAL_MODULE_TAGS         := optional
	#LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
	###########################################################

	###########################################################
	include $(CLEAR_VARS)
	LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := atmel_mxt_ts.ko
	LOCAL_MODULE_KBUILD_NAME  := atmel_mxt_ts.ko
	LOCAL_MODULE_TAGS         := optional
	#LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
	###########################################################

	###########################################################
	include $(CLEAR_VARS)
	LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
	LOCAL_MODULE              := synaptics_tcm_ts.ko
	LOCAL_MODULE_KBUILD_NAME  := synaptics_tcm_ts.ko
	LOCAL_MODULE_TAGS         := optional
	#LOCAL_MODULE_DEBUG_ENABLE := true
	LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
	include $(DLKM_DIR)/Build_external_kernelmodule.mk
	###########################################################

	ifneq ($(TARGET_BOARD_PLATFORM), pineapple)
		###########################################################
		include $(CLEAR_VARS)
		LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
		LOCAL_MODULE              := pt_ts.ko
		LOCAL_MODULE_KBUILD_NAME  := pt_ts.ko
		LOCAL_MODULE_TAGS         := optional
		#LOCAL_MODULE_DEBUG_ENABLE := true
		LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
		include $(DLKM_DIR)/Build_external_kernelmodule.mk
		###########################################################

		###########################################################
		include $(CLEAR_VARS)
		LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
		LOCAL_MODULE              := pt_i2c.ko
		LOCAL_MODULE_KBUILD_NAME  := pt_i2c.ko
		LOCAL_MODULE_TAGS         := optional
		#LOCAL_MODULE_DEBUG_ENABLE := true
		LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
		include $(DLKM_DIR)/Build_external_kernelmodule.mk
		###########################################################

		###########################################################
		include $(CLEAR_VARS)
		LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
		LOCAL_MODULE              := pt_device_access.ko
		LOCAL_MODULE_KBUILD_NAME  := pt_device_access.ko
		LOCAL_MODULE_TAGS         := optional
		#LOCAL_MODULE_DEBUG_ENABLE := true
		LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
		include $(DLKM_DIR)/Build_external_kernelmodule.mk
		###########################################################

		###########################################################
		include $(CLEAR_VARS)
		LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)
		LOCAL_MODULE              := raydium_ts.ko
		LOCAL_MODULE_KBUILD_NAME  := raydium_ts.ko
		LOCAL_MODULE_TAGS         := optional
		#LOCAL_MODULE_DEBUG_ENABLE := true
		LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
		include $(DLKM_DIR)/Build_external_kernelmodule.mk
		###########################################################
	endif # pineapple
	endif # DLKM check
endif
