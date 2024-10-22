# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019 MediaTek Inc.

KERNEL_ENV_PATH := $(call my-dir)
KERNEL_ROOT_DIR := $(PWD)

define touch-kernel-image-timestamp
if [ -e $(1) ] && [ -e $(2) ] && cmp -s $(1) $(2); then \
 echo $(2) has no change;\
 mv -f $(1) $(2);\
else \
 rm -f $(1);\
fi
endef

# '\\' in command is wrongly replaced to '\\\\' in kernel/out/arch/arm/boot/compressed/.piggy.xzkern.cmd
define fixup-kernel-cmd-file
if [ -e $(1) ]; then cp $(1) $(1).bak; sed -e 's/\\\\\\\\/\\\\/g' < $(1).bak > $(1); rm -f $(1).bak; fi
endef

  mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
  KERNEL_DIR := $(KERNEL_ENV_PATH)
  REL_ACK_DIR := $(LINUX_KERNEL_VERSION)
  REL_KERNEL_DIR := $(shell bash $(dir $(mkfile_path))scripts/get_rel_path.sh $(KERNEL_DIR) $(KERNEL_ROOT_DIR)/kernel)
  REL_KERNEL_DIR_FROM_ACK := $(shell bash $(dir $(mkfile_path))scripts/get_rel_path.sh $(KERNEL_DIR) $(KERNEL_ROOT_DIR)/kernel/$(REL_ACK_DIR))
ifndef KERNEL_BUILD_VARIANT
  KERNEL_BUILD_VARIANT := $(TARGET_BUILD_VARIANT)
endif

  ifeq ($(wildcard $(TARGET_PREBUILT_KERNEL)),)
    KERNEL_OUT ?= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/$(REL_ACK_DIR)
    REL_KERNEL_OUT := $(shell bash $(dir $(mkfile_path))scripts/get_rel_path.sh $(patsubst %/,%,$(dir $(KERNEL_OUT))) $(KERNEL_ROOT_DIR)/kernel)
    KERNEL_ROOT_OUT := $(if $(filter /% ~%,$(KERNEL_OUT)),,$(KERNEL_ROOT_DIR)/)$(KERNEL_OUT)
    KERNEL_GKI_CONFIG :=
    ifeq (yes,$(strip $(BUILD_KERNEL)))
    ifeq ($(KERNEL_TARGET_ARCH), arm64)
      ifeq (,$(strip $(MTK_KERNEL_COMPRESS_FORMAT)))
        MTK_KERNEL_COMPRESS_FORMAT := gz
      endif
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/Image.$(MTK_KERNEL_COMPRESS_FORMAT)-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/Image.$(MTK_KERNEL_COMPRESS_FORMAT)
      endif

     ifneq ($(KERNEL_USE_BAZEL),yes)
      ifdef MTK_GKI_PREBUILTS_DIR
          KERNEL_ZIMAGE_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/Image.$(MTK_KERNEL_COMPRESS_FORMAT)
      else
        ifdef MTK_GKI_BUILD_CONFIG
          KERNEL_ZIMAGE_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/dist/Image.$(MTK_KERNEL_COMPRESS_FORMAT)
        endif
      endif
      TARGET_KERNEL_CONFIG := $(KERNEL_OUT)/.config
     else
          KERNEL_BAZEL_BUILD_OUT ?= $(TARGET_OUT_INTERMEDIATES)/KLEAF_OBJ
          KERNEL_BAZEL_DIST_OUT := $(KERNEL_BAZEL_BUILD_OUT)/dist
          KERNEL_ZIMAGE_OUT := $(KERNEL_BAZEL_DIST_OUT)/$(REL_KERNEL_DIR)/$(my_kernel_target)_kernel_aarch64.$(KERNEL_BUILD_VARIANT)/Image.$(MTK_KERNEL_COMPRESS_FORMAT)
	  TARGET_KERNEL_CONFIG := $(KERNEL_BAZEL_DIST_OUT)/$(REL_KERNEL_DIR)/$(my_kernel_target).$(KERNEL_BUILD_VARIANT)/.config
     endif
    else
      ifeq ($(MTK_APPENDED_DTB_SUPPORT), yes)
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/zImage-dtb
      else
        KERNEL_ZIMAGE_OUT := $(KERNEL_OUT)/arch/$(KERNEL_TARGET_ARCH)/boot/zImage
      endif
    endif
    endif#BUILD_KERNEL

    ifneq ($(KERNEL_USE_BAZEL),yes)
    BUILT_KERNEL_TARGET := $(KERNEL_ZIMAGE_OUT).bin
    else
    BUILT_KERNEL_TARGET := $(KERNEL_ZIMAGE_OUT)
    endif
    ifneq ($(strip $(TARGET_NO_KERNEL)),true)
    INSTALLED_KERNEL_TARGET := $(PRODUCT_OUT)/kernel
    endif
    GEN_KERNEL_BUILD_CONFIG := $(patsubst %/,%,$(dir $(KERNEL_OUT)))/build.config
    REL_GEN_KERNEL_BUILD_CONFIG := $(REL_KERNEL_OUT)/$(notdir $(GEN_KERNEL_BUILD_CONFIG))
    GEN_MTK_SETUP_ENV_SH := $(patsubst %/,%,$(dir $(KERNEL_OUT)))/mtk_setup_env.sh
    ifneq (gki_defconfig,$(strip $(KERNEL_DEFCONFIG)))
      KERNEL_CONFIG_FILE := $(KERNEL_DIR)/arch/$(KERNEL_TARGET_ARCH)/configs/$(word 1,$(KERNEL_DEFCONFIG))
    else
      KERNEL_CONFIG_FILE := kernel/$(REL_ACK_DIR)/arch/$(KERNEL_TARGET_ARCH)/configs/$(KERNEL_DEFCONFIG)
    endif

  else
    BUILT_KERNEL_TARGET := $(TARGET_PREBUILT_KERNEL)
  endif #TARGET_PREBUILT_KERNEL is empty

