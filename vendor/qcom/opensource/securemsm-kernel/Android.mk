# Android makefile for securemsm kernel modules

ENABLE_SECUREMSM_DLKM := true
ENABLE_SECUREMSM_QTEE_DLKM := true

ifeq ($(TARGET_KERNEL_DLKM_DISABLE), true)
  ifeq ($(TARGET_KERNEL_DLKM_SECURE_MSM_OVERRIDE),false)
    ENABLE_SECUREMSM_DLKM := false
  endif
  ifeq ($(TARGET_KERNEL_DLKM_SECUREMSM_QTEE_OVERRIDE),false)
    ENABLE_SECUREMSM_QTEE_DLKM := false
  endif
endif

LOCAL_PATH := $(call my-dir)
DLKM_DIR := $(TOP)/device/qcom/common/dlkm

SEC_KERNEL_DIR := $(TOP)/vendor/qcom/opensource/securemsm-kernel

LOCAL_EXPORT_KO_INCLUDE_DIRS := $(LOCAL_PATH)/include/ \
                                $(LOCAL_PATH)/include/uapi

SSG_SRC_FILES := \
	$(wildcard $(LOCAL_PATH)/*) \
 	$(wildcard $(LOCAL_PATH)/*/*) \
 	$(wildcard $(LOCAL_PATH)/*/*/*) \
 	$(wildcard $(LOCAL_PATH)/*/*/*/*)
LOCAL_MODULE_DDK_BUILD := true
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := SSG_ROOT=$(SEC_KERNEL_DIR)
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)

###################################################
include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := sec-module-symvers
LOCAL_MODULE_STEM         := Module.symvers
LOCAL_MODULE_KBUILD_NAME  := Module.symvers
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###################################################
###################################################

ifeq ($(ENABLE_SECUREMSM_QTEE_DLKM), true)
include $(CLEAR_VARS)
#LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := smcinvoke_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := smcinvoke_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_HEADER_LIBRARIES    := smcinvoke_kernel_headers
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###################################################
###################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := tz_log_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := tz_log_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk

include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := qseecom_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := qseecom_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
endif #ENABLE_SECUREMSM_QTEE_DLKM
###################################################
###################################################

ifeq ($(ENABLE_SECUREMSM_DLKM), true)
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := qce50_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := qce50_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###################################################
###################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := qcedev-mod_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := qcedev-mod_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###################################################
###################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := qcrypto-msm_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := qcrypto-msm_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###################################################
###################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := hdcp_qseecom_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := hdcp_qseecom_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###################################################
###################################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_MODULE              := qrng_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := qrng_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
###################################################
###################################################
ifeq ($(TARGET_USES_SMMU_PROXY), true)
include $(CLEAR_VARS)
#LOCAL_SRC_FILES           := $(SSG_SRC_FILES)
LOCAL_EXPORT_KO_INCLUDE_DIRS := $(LOCAL_PATH)/smmu-proxy/ $(LOCAL_PATH)/
LOCAL_MODULE              := smmu_proxy_dlkm.ko
LOCAL_MODULE_KBUILD_NAME  := smmu_proxy_dlkm.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
endif
###################################################
###################################################
endif #COMPILE_SECUREMSM_DLKM check
