#
# Copyright (C) 2023 The Android Open-Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


include $(LOCAL_PATH)/BoardConfig.mk

PRODUCT_ENFORCE_VINTF_MANIFEST := true
PRODUCT_SHIPPING_API_LEVEL := 34
BOARD_BOOT_HEADER_VERSION := 4
BOARD_INIT_BOOT_HEADER_VERSION := 4
BOARD_GKI_NONAB_COMPAT := true
PRODUCT_USE_DYNAMIC_PARTITIONS := true
BOARD_BUILD_SUPER_IMAGE_BY_DEFAULT := true
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 0x04000000
CONF_PATH_BASE := device/samsung/erd9945/conf
BOARD_USES_SW_GATEKEEPER := true
USE_SWIFTSHADER := false
BOARD_USES_GENERIC_KERNEL_IMAGE := true
BOARD_MOVE_GSI_AVB_KEYS_TO_VENDOR_BOOT := true

#BOARD_PREBUILTS := device/samsung/$(TARGET_PRODUCT:full_%=%)-prebuilts
BOARD_PREBUILTS := device/samsung/$(TARGET_SOC)-prebuilts
TARGET_NO_KERNEL := false
PRODUCT_BUILD_RECOVERY_IMAGE := true
INSTALLED_KERNEL_TARGET := $(BOARD_PREBUILTS)/kernel
BOARD_PREBUILT_BOOTIMAGE := $(BOARD_PREBUILTS)/boot.img
BOARD_PREBUILT_DTBOIMAGE := $(BOARD_PREBUILTS)/dtbo.img
BOARD_PREBUILT_DTB := $(BOARD_PREBUILTS)/dtb.img
#BOARD_PREBUILT_BOOTLOADER_IMG := $(BOARD_PREBUILTS)/bootloader.img
INSTALLED_DTBIMAGE_TARGET := $(BOARD_PREBUILT_DTB)

BOARD_PREBUILT_KERNEL_MODULES := $(BOARD_PREBUILTS)/modules
BOARD_PREBUILT_KERNEL_MODULES_SYMBOLS := $(BOARD_PREBUILTS)/modules_symbols
BOARD_VENDOR_RAMDISK_FRAGMENTS := dlkm
BOARD_VENDOR_RAMDISK_FRAGMENT.dlkm.KERNEL_MODULE_DIRS := top

# Reordered modules list
BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(foreach module, $(notdir $(shell cat $(BOARD_PREBUILTS)/modules/modules.order | sed 's/^.*\///')), $(BOARD_PREBUILT_KERNEL_MODULES)/$(module))

# set system dlkm source directory and remove it in copy file list
BOARD_SYSTEM_DLKM_SRC := $(BOARD_PREBUILTS)/system_dlkm_staging
BOARD_SYSTEM_KERNEL_MODULES := $(wildcard $(BOARD_SYSTEM_DLKM_SRC)/lib/modules/*.ko)
BOARD_SYSTEM_KERNEL_MODULES_LOAD := $(shell cat $(BOARD_SYSTEM_DLKM_SRC)/lib/modules/modules.load)

BOARD_VENDOR_DLKM_SRC := $(BOARD_PREBUILTS)/vendor_dlkm_staging
BOARD_VENDOR_KERNEL_MODULES := $(wildcard $(BOARD_VENDOR_DLKM_SRC)/lib/modules/*.ko)
BOARD_VENDOR_KERNEL_MODULES_LOAD := $(shell cat $(BOARD_VENDOR_DLKM_SRC)/lib/modules/modules.load)

BOARD_RECOVERY_KERNEL_MODULES += $(BOARD_VENDOR_RAMDISK_KERNEL_MODULES)

PRODUCT_COPY_FILES += $(foreach image,\
	$(filter-out $(BOARD_SYSTEM_DLKM_SRC) $(BOARD_PREBUILT_DTBOIMAGE) \
		     $(BOARD_PREBUILT_BOOTLOADER_IMG) $(BOARD_PREBUILT_DTB) \
		     $(BOARD_PREBUILT_KERNEL_MODULES) $(BOARD_PREBUILT_KERNEL_MODULES_SYMBOLS) \
		     $(BOARD_PREBUILT_BOOTIMAGE) $(BOARD_VENDOR_DLKM_SRC), \
		     $(wildcard $(BOARD_PREBUILTS)/*)),\
	$(image):$(PRODUCT_OUT)/$(notdir $(image)))

PRODUCT_COPY_FILES += device/samsung/erd9945/conf/init.insmod.sh:/vendor/bin/init.insmod.sh
PRODUCT_COPY_FILES += $(foreach modules, $(wildcard $(BOARD_PREBUILT_KERNEL_MODULES_SYMBOLS)/*), $(modules):$(PRODUCT_OUT)/modules_symbols/$(notdir $(modules)))
PRODUCT_COPY_FILES += $(BOARD_PREBUILTS)/init.insmod.vendor_dlkm.cfg:$(TARGET_COPY_OUT_VENDOR)/etc/init.insmod.vendor_dlkm.cfg

PRODUCT_VENDOR_PROPERTIES += \
	ro.soc.manufacturer=$(TARGET_SOC_MANUFACTURE) \
	ro.soc.model=$(TARGET_SOC_MODEL)

# Configuration for AB update
ifeq ($(EXYNOS_USE_VIRTUAL_AB),true)
AB_OTA_UPDATER := true
EXYNOS_VIRTUAL_AB_COMPRESSION := true
EXYNOS_KERNEL_DM_SNAPSHOT_AS_MODULE := false		# Current Kernel uses CONFIG_DM_SNAPSHOT=y
AB_OTA_PARTITIONS := \
	vendor \
	boot \
	init_boot \
	vendor_boot \
	dtbo \
	vbmeta \
	vbmeta_system \
	vbmeta_vendor \
	vendor_dlkm \
	system_dlkm \
	system_ext \
	product \

ifneq ($(WITH_ESSI_64), true)
AB_OTA_PARTITIONS += \
	system \

endif

# Boot Contorl HAL to support AB system
EXYNOS_BOOT_CONTROL_HAL_INTERFACE_TYPE := AIDL
ifeq ($(EXYNOS_BOOT_CONTROL_HAL_INTERFACE_TYPE), HIDL)
EXYNOS_BOOT_CONTROL_HAL_NAME := android.hardware.boot@1.2
PRODUCT_PACKAGES += \
	$(EXYNOS_BOOT_CONTROL_HAL_NAME)-impl-exynos \
	$(EXYNOS_BOOT_CONTROL_HAL_NAME)-impl-exynos.recovery \
	$(EXYNOS_BOOT_CONTROL_HAL_NAME)-service \
	$(EXYNOS_BOOT_CONTROL_HAL_NAME)-service.recovery \

else
PRODUCT_PACKAGES += \
	android.hardware.boot-service.exynos \
	android.hardware.boot-service.exynos_recovery \

endif

PRODUCT_PACKAGES += \
	update_engine \
	update_verifier \
	update_engine_client \

# Recovery Configuration for Virtual AB
# Ignore case for applying AB update with Recovery image
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE :=
BOARD_USES_FULL_RECOVERY_IMAGE :=
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT := true
NORMAL_TARGET_COPY_OUT_VENDOR_RAMDISK := $(TARGET_COPY_OUT_VENDOR_RAMDISK)/first_stage_ramdisk
RECOVERY_TARGET_COPY_OUT_VENDOR_RADISK := $(TARGET_COPY_OUT_VENDOR_RAMDISK)
TARGET_NO_RECOVERY := true

# Compression option for OTA Package
ifeq ($(EXYNOS_VIRTUAL_AB_COMPRESSION), true)
# Select Compression Package Type
# Option: gz, brotli
PRODUCT_VIRTUAL_AB_COMPRESSION_METHOD := gz

# To use CONFIG_DM_SNAPSHOT=y in kernel, assign kernel module ko file for dm snapshot can be loaded in first stage ramdisk
ifeq ($(EXYNOS_KERNEL_DM_SNAPSHOT_AS_MODULE), true)
BOARD_GENERIC_RAMDISK_KERNEL_MODULES_LOAD := dm-user.ko
endif
endif

# Userspace Snapshot Merge Feature
PRODUCT_VENDOR_PROPERTIES += ro.virtual_ab.userspace.snapshots.enabled=true
else
# If Non-AB, then recovery must be exist
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE :=
BOARD_USES_FULL_RECOVERY_IMAGE := true
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT :=
NORMAL_TARGET_COPY_OUT_VENDOR_RAMDISK := $(TARGET_COPY_OUT_VENDOR_RAMDISK)
RECOVERY_TARGET_COPY_OUT_VENDOR_RADISK := $(TARGET_COPY_OUT_VENDOR_RAMDISK)
TARGET_NO_RECOVERY :=
endif

BOARD_INCLUDE_DTB_IN_BOOTIMG := true
BOARD_RAMDISK_OFFSET := 0
BOARD_KERNEL_TAGS_OFFSET := 0
BOARD_DTB_OFFSET := 0

# recovery mode
ifeq ($(EXYNOS_USE_DEDICATED_RECOVERY_IMAGE), true)
BOARD_INCLUDE_RECOVERY_DTBO := true
BOARD_RECOVERY_CMDLINE := $(BOARD_BOOTCONFIG)
BOARD_RECOVERY_CMDLINE += $(BOARD_KERNEL_CMDLINE)
BOARD_RECOVERY_MKBOOTIMG_ARGS := \
  --ramdisk_offset $(BOARD_RAMDISK_OFFSET) \
  --tags_offset $(BOARD_KERNEL_TAGS_OFFSET) \
  --header_version 2 \
  --dtb $(INSTALLED_DTBIMAGE_TARGET) \
  --dtb_offset $(BOARD_DTB_OFFSET) \
  --pagesize $(BOARD_FLASH_BLOCK_SIZE) \
  --recovery_dtbo $(BOARD_PREBUILT_DTBOIMAGE) \
  --cmdline "$(BOARD_RECOVERY_CMDLINE)" \

endif
BOARD_MKBOOTIMG_INIT_ARGS := \
  --header_version $(BOARD_INIT_BOOT_HEADER_VERSION)

BOARD_MKBOOTIMG_ARGS := \
  --ramdisk_offset $(BOARD_RAMDISK_OFFSET) \
  --tags_offset $(BOARD_KERNEL_TAGS_OFFSET) \
  --header_version $(BOARD_BOOT_HEADER_VERSION) \
  --dtb $(INSTALLED_DTBIMAGE_TARGET) \
  --dtb_offset 0 \
  --pagesize $(BOARD_FLASH_BLOCK_SIZE)

PRODUCT_COPY_FILES += \
		$(INSTALLED_KERNEL_TARGET):$(PRODUCT_OUT)/kernel

# init_boot partition size is recommended to be 8MB, it can be larger.
# When this variable is set, init_boot.img will be built with the generic
# ramdisk, and that ramdisk will no longer be included in boot.img.
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 16777216
PRODUCT_BUILD_INIT_BOOT_IMAGE = true

# Partitions options
BOARD_BOOTIMAGE_PARTITION_SIZE := 0x04000000
BOARD_DTBOIMG_PARTITION_SIZE := 0x00800000

BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
# BOARD_SYSTEMIMAGE_PARTITION_SIZE := 3145728000
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
# BOARD_VENDORIMAGE_PARTITION_SIZE := 838860800

BOARD_USERDATAIMAGE_FILE_SYSTEM_TYPE := f2fs
BOARD_USERDATAIMAGE_PARTITION_SIZE := 11796480000
# Recovery is optional for AB
ifeq ($(EXYNOS_USE_DEDICATED_RECOVERY_IMAGE), true)
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 99614720
endif
# If AB system is applied, cache partition does not used
ifneq ($(AB_OTA_UPDATER), true)
BOARD_CACHEIMAGE_PARTITION_SIZE := 69206016
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
endif

BOARD_SUPER_PARTITION_GROUPS := group_basic

# Dynamic Partitions options
ifeq ($(EXYNOS_USE_VIRTUAL_AB), true)
# Size of Super Partition in AB must be at lease 200% compared to its of Non-AB
BOARD_SUPER_PARTITION_SIZE := 9848225792
BOARD_GROUP_BASIC_SIZE := 9839837184
else
BOARD_SUPER_PARTITION_SIZE := 4924112896
BOARD_GROUP_BASIC_SIZE := 4919918592
endif
ifeq ($(WITH_ESSI_64),true)
BOARD_GROUP_BASIC_PARTITION_LIST := vendor vendor_dlkm system_dlkm odm

TARGET_COPY_OUT_ODM := odm
BOARD_ODMIMAGE_FILE_SYSTEM_TYPE := ext4
else
BOARD_GROUP_BASIC_PARTITION_LIST := system vendor vendor_dlkm system_dlkm system_ext product
endif

ifeq ($(TARGET_BUILD_VARIANT),eng)
PRODUCT_SET_DEBUGFS_RESTRICTIONS := false
endif

# For incremental filesystem feature
PRODUCT_PROPERTY_OVERRIDES += ro.incremental.enable=yes

# control_privapp_permissions
PRODUCT_PROPERTY_OVERRIDES += \
	ro.control_privapp_permissions=enforce

# Use FUSE passthrough
PRODUCT_PRODUCT_PROPERTIES += \
      persist.sys.fuse.passthrough.enable=true

# Enable fuse-bpf
PRODUCT_PRODUCT_PROPERTIES += \
      ro.fuse.bpf.enabled=true

PRODUCT_PRODUCT_PROPERTIES += \
	ro.arm64.memtag.bootctl_supported=1

# VRR
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += ro.surface_flinger.enable_frame_rate_override=false

PRODUCT_ENFORCE_RRO_TARGETS := \
	framework-res

# Device Manifest, Device Compatibility Matrix for Treble
ifeq ($(AB_OTA_UPDATER), true)
ifneq ($(filter full_erd9945_s5400_u,$(TARGET_PRODUCT)),)
DEVICE_MANIFEST_FILE := device/samsung/erd9945/manifest_ab_s5400.xml
else ifneq ($(filter full_erd9945_s6375_u,$(TARGET_PRODUCT)),)
DEVICE_MANIFEST_FILE := device/samsung/erd9945/manifest_ab_s6375.xml
else
DEVICE_MANIFEST_FILE := device/samsung/erd9945/manifest_ab.xml
endif
else
ifneq ($(filter full_erd9945_s5400_u,$(TARGET_PRODUCT)),)
DEVICE_MANIFEST_FILE := device/samsung/erd9945/manifest_s5400.xml
else ifneq ($(filter full_erd9945_s6375_u,$(TARGET_PRODUCT)),)
DEVICE_MANIFEST_FILE := device/samsung/erd9945/manifest_s6375.xml
else
DEVICE_MANIFEST_FILE := device/samsung/erd9945/manifest.xml
endif
endif
DEVICE_MATRIX_FILE := device/samsung/erd9945/compatibility_matrix.xml
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE := device/samsung/erd9945/framework_compatibility_matrix.xml
DEVICE_PACKAGE_OVERLAYS += device/samsung/erd9945/overlay

# Init files
ifneq ($(filter full_erd9945_s5400_u,$(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += \
	$(CONF_PATH_BASE)/init.s5e9945_s5400.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.$(TARGET_SOC).rc
else ifneq ($(filter full_erd9945_s6375_u,$(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += \
	$(CONF_PATH_BASE)/init.s5e9945_s6375.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.$(TARGET_SOC).rc
else
PRODUCT_COPY_FILES += \
	$(CONF_PATH_BASE)/init.s5e9945.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.$(TARGET_SOC).rc
endif

PRODUCT_COPY_FILES += \
	$(CONF_PATH_BASE)/init.s5e9945.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.$(TARGET_SOC).usb.rc \
	$(CONF_PATH_BASE)/ueventd.s5e9945.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.rc \
	$(CONF_PATH_BASE)/init.recovery.s5e9945.rc:root/init.recovery.$(TARGET_SOC).rc \
	device/samsung/erd9945/task_profiles.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json

# [ SystemPerformance - Power Hal
PRODUCT_PROPERTY_OVERRIDES += \
    debug.sf.enable_adpf_cpu_hint=true \
    debug.hwui.use_hint_manager=true
# SystemPerformance - Power Hal ]

BOOTUP_STORAGE := $(BOARD_USES_BOOT_STORAGE)
ifeq ($(AB_OTA_UPDATER), true)
FSTAB_FILE := fstab.s5e9945.$(BOOTUP_STORAGE).ab
else
FSTAB_FILE := fstab.s5e9945.$(BOOTUP_STORAGE)
endif
PRODUCT_COPY_FILES += \
	$(CONF_PATH_BASE)/$(FSTAB_FILE):$(TARGET_COPY_OUT_VENDOR)/etc/fstab.$(TARGET_SOC) \
	$(CONF_PATH_BASE)/$(FSTAB_FILE):$(NORMAL_TARGET_COPY_OUT_VENDOR_RAMDISK)/fstab.$(TARGET_SOC) \

TARGET_RECOVERY_FSTAB := $(CONF_PATH_BASE)/$(FSTAB_FILE)

PRODUCT_COPY_FILES += \
	$(CONF_PATH_BASE)/init.recovery.s5e9945.rc:$(RECOVERY_TARGET_COPY_OUT_VENDOR_RAMDISK)/init.recovery.$(TARGET_SOC).rc \
	$(CONF_PATH_BASE)/init.s5e9945.usb.rc:$(RECOVERY_TARGET_COPY_OUT_VENDOR_RAMDISK)/etc/init/init.$(TARGET_SOC).usb.rc \
	$(CONF_PATH_BASE)/$(FSTAB_FILE):$(RECOVERY_TARGET_COPY_OUT_VENDOR_RAMDISK)/etc/recovery.fstab

# Support devtools
ifeq ($(TARGET_BUILD_VARIANT),eng)
PRODUCT_PACKAGES += \
	Development
endif

# Filesystem management tools
PRODUCT_PACKAGES += \
	e2fsck

TARGET_RO_FILE_SYSTEM_TYPE ?= erofs

PRODUCT_SOONG_NAMESPACES += vendor/samsung_slsi/s5e9945/libs
ifneq ($(filter full_erd9945_s5400_u,$(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += \
	device/samsung/erd9945/handheld_core_hardware_s5400.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml
else ifneq ($(filter full_erd9945_s6375_u,$(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += \
	device/samsung/erd9945/handheld_core_hardware_s6375.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml
else
PRODUCT_COPY_FILES += \
	device/samsung/erd9945/handheld_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/handheld_core_hardware.xml
endif
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \

ifneq ($(filter e1s% e2s% e3s% r12s%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.wifi.aware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.aware.xml \
	frameworks/native/data/etc/android.hardware.wifi.rtt.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.rtt.xml

# wifi Aware definition
INCLUDE_AWARE_FEATURE := 1
WIFI_HIDL_FEATURE_AWARE := true
BOARD_HAS_AWARE := true

# Enable vendor properties.
PRODUCT_PROPERTY_OVERRIDES += \
	wifi.aware.interface=wifi-aware0
endif

PRODUCT_PACKAGES += \
	android.hardware.health-service.example \
	android.hardware.thermal-service.exynos

# Include additional device.mk
include $(LOCAL_PATH)/include/audio.mk
include $(LOCAL_PATH)/include/camera.mk
include $(LOCAL_PATH)/include/gpu.mk
include $(LOCAL_PATH)/include/security.mk
include $(LOCAL_PATH)/include/video.mk
include $(LOCAL_PATH)/include/npu.mk
include $(LOCAL_PATH)/include/hwc.mk
include $(LOCAL_PATH)/include/wfd.mk
include $(LOCAL_PATH)/include/sf.mk
#include $(LOCAL_PATH)/include/sensor.mk
#ifeq ($(filter full_erd9945_s5400_u,$(TARGET_PRODUCT)),)
#include $(LOCAL_PATH)/include/bluetooth.mk
#endif
include $(LOCAL_PATH)/include/memtrack.mk
include $(LOCAL_PATH)/include/m2m.mk
#include $(LOCAL_PATH)/include/wifi.mk

# Use 64-bit dex2oat for better dexopt time.
PRODUCT_PROPERTY_OVERRIDES += \
	dalvik.vm.dex2oat64.enabled=true

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_base.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/updatable_apex.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)

$(call inherit-product-if-exists, vendor/samsung_slsi/common/exynos-vendor.mk)
$(call inherit-product-if-exists, hardware/samsung_slsi/s5e9945/s5e9945.mk)

# GMS Applications
ifeq ($(WITH_GMS),true)
GMS_ENABLE_OPTIONAL_MODULES := true
USE_GMS_STANDARD_CONFIG := true
$(call inherit-product-if-exists, vendor/partner_gms/products/gms_64bit_only.mk)
endif

PRODUCT_PACKAGES += \
    vndservicemanager

PRODUCT_COPY_FILES += \
  device/samsung/erd9945/public.libraries.txt:$(TARGET_COPY_OUT_VENDOR)/etc/public.libraries.txt

ifeq ($(EXYNOS_USE_VIRTUAL_AB),true)
# Previous AOSP guide is to use lunch default make file, it changed from android S
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/android_t_baseline.mk)
endif

# Generic Ramdisk
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_ramdisk.mk)

ifeq ($(TARGET_BUILD_VARIANT),eng)
PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS = false
endif

## PowerHAL
#PRODUCT_SOONG_NAMESPACES += device/samsung/erd9945/power
#PRODUCT_PACKAGES += \
#	android.hardware.power-service.exynos9945 \
#	android.hardware.power.stats-service.exynos9945 \
#	com.android.hardware.power.exynos9945

# EPIC
PRODUCT_SOONG_NAMESPACES += device/samsung/erd9945/epic
PRODUCT_PACKAGES += \
       epic libems_service libgmc libepic_helper

PRODUCT_COPY_FILES += \
       device/samsung/erd9945/epic/epic.json:$(TARGET_COPY_OUT_VENDOR)/etc/epic.json \
       device/samsung/erd9945/epic/ems.json:$(TARGET_COPY_OUT_VENDOR)/etc/ems.json \
       device/samsung/erd9945/epic/ems_parameter.json:$(TARGET_COPY_OUT_VENDOR)/etc/ems_parameter.json \

# Enabled only in ERD and not in SEP
ifneq (,$(findstring erd9945, $(TARGET_PRODUCT)))
PRODUCT_VENDOR_PROPERTIES += \
	vendor.power.touch=1 \
	vendor.power.applaunch=1 \
	vendor.power.applaunchrelease=1 \
	vendor.epic.feature=1
endif

# Profiler
PRODUCT_SOONG_NAMESPACES += device/samsung/erd9945/profiler
PRODUCT_PACKAGES += \
       gflow gperf libexynos_profiler libexynos_logger
PRODUCT_COPY_FILES += \
       device/samsung/erd9945/profiler/gperf_version.txt:$(TARGET_COPY_OUT_VENDOR)/etc/gperf_version.txt \
       device/samsung/erd9945/profiler/gflow_version.txt:$(TARGET_COPY_OUT_VENDOR)/etc/gflow_version.txt \

#Epic AIDL
SOONG_CONFIG_NAMESPACES += epic
SOONG_CONFIG_epic := vendor_hint
SOONG_CONFIG_epic_vendor_hint := true

PRODUCT_PACKAGES += \
	vendor.samsung_slsi.hardware.epic \
	vendor.samsung_slsi.hardware.epic-service \

# ENN
$(call inherit-product-if-exists, vendor/samsung_slsi/exynos/enn/enn.mk)

#ENN HAL
PRODUCT_PACKAGES +=\
    android.hardware.neuralnetworks-service-enn
PRODUCT_SOONG_NAMESPACES +=\
    vendor/samsung_slsi/exynos/enn_saidl_driver

# MODULES : libexynosgdc, libHDRMetaGenerator, libvra
$(call inherit-product-if-exists, vendor/samsung_slsi/exynos/modules/modules.mk)

# VPL
PRODUCT_COPY_FILES += \
    device/samsung/erd9945/firmware/camera/libvpl/default_configuration.flm.cfg.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/default_configuration.flm.cfg.bin \
    device/samsung/erd9945/firmware/camera/libvpl/secure_configuration.flm.cfg.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/secure_configuration.flm.cfg.bin \
    device/samsung/erd9945/firmware/camera/libvpl/OD_V3.4.10_11_13_QVGA_ROOT_ENN_BGRA.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/OD_V3.4.10_11_13_QVGA_ROOT_ENN_BGRA.nnc \
    device/samsung/erd9945/firmware/camera/libvpl/OD_V3.5.3_09_26_VGA_ROOT_ENN_BGRA.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/OD_V3.5.3_09_26_VGA_ROOT_ENN_BGRA.nnc

# CAC
PRODUCT_COPY_FILES += \
    device/samsung/erd9945/firmware/camera/libsemseg/MSL.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MSL.nnc

# CAV
PRODUCT_COPY_FILES += \
    device/samsung/erd9945/firmware/camera/libsemseg/video_cnn.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/video_cnn.nnc \

# DOF
PRODUCT_COPY_FILES += \
    device/samsung/erd9945/firmware/camera/libvpl_dof/default_configuration.dof.front.cfg.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/default_configuration.dof.front.cfg.bin \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_FE_O2_ROOT_ENN_Front.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_FE_O2_ROOT_ENN_Front.nnc \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_MV_B2_O2_ROOT_ENN_Front.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_MV_B2_O2_ROOT_ENN_Front.nnc \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_MV_B3_O2_ROOT_ENN_Front.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_MV_B3_O2_ROOT_ENN_Front.nnc \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_MV_B4_O2_ROOT_ENN_Front.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_MV_B4_O2_ROOT_ENN_Front.nnc \
    device/samsung/erd9945/firmware/camera/libvpl_dof/default_configuration.dof.rear.cfg.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/default_configuration.dof.rear.cfg.bin \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_FE_O2_ROOT_ENN_Rear.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_FE_O2_ROOT_ENN_Rear.nnc \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_MV_B2_O2_ROOT_ENN_Rear.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_MV_B2_O2_ROOT_ENN_Rear.nnc \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_MV_B3_O2_ROOT_ENN_Rear.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_MV_B3_O2_ROOT_ENN_Rear.nnc \
    device/samsung/erd9945/firmware/camera/libvpl_dof/MFC_DOF_MV_B4_O2_ROOT_ENN_Rear.nnc:$(TARGET_COPY_OUT_VENDOR)/firmware/MFC_DOF_MV_B4_O2_ROOT_ENN_Rear.nnc \


# USB HAL
PRODUCT_PACKAGES += \
	android.hardware.usb-service.exynos \
	com.android.future.usb.accessory

PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml

ifeq ($(TARGET_BUILD_VARIANT),eng)
PRODUCT_COPY_FILES += \
	$(CONF_PATH_BASE)/usb_connection.sh:$(TARGET_COPY_OUT_VENDOR)/bin/usb_connection.sh
endif

PRODUCT_PROPERTY_OVERRIDES += \
    dev.usbsetting.embedded=on

# Lights HAL
PRODUCT_PACKAGES += \
    android.hardware.lights-service.exynos

# Fastboot HAL
PRODUCT_PACKAGES += \
    fastbootd \
    android.hardware.fastboot@1.1\
    android.hardware.fastboot@1.1-impl-mock.s5e9945

# Exynos RIL and telephony
$(call inherit-product-if-exists, vendor/samsung_slsi/telephony/$(BOARD_USES_SHARED_VENDOR_TELEPHONY)/common/device-vendor.mk)

# dmd
$(call inherit-product-if-exists, vendor/samsung_slsi/telephony/samsungmobile/modem_logging.mk)

## IMSService ##
# IMSService build for both types (source build, prebulit apk/so build).
# Please make sure that only one of both ShannonIms(apk git) shannon-ims(src git) is present in the repo.
# This will be called only if IMSService is building with prebuilt binary for stable branches.
$(call inherit-product-if-exists, packages/apps/ShannonIms/device-vendor.mk)
# This will be called only if IMSService is building with source code for dev branches.
$(call inherit-product-if-exists, vendor/samsung_slsi/ims/shannon-ims/device-vendor.mk)

#RCSService#
$(call inherit-product-if-exists, packages/apps/ShannonRcs/device-vendor.mk)
$(call inherit-product-if-exists, vendor/samsung_slsi/ims/ShannonRcs/device-vendor.mk)

#RCSClient#
$(call inherit-product-if-exists, packages/apps/RcsClient/device-vendor.mk)
$(call inherit-product-if-exists, vendor/samsung_slsi/ims/rcs-client/device-vendor.mk)

# SecureElement HAL
$(call inherit-product-if-exists, vendor/samsung/hal-server/secure_element/seHal.mk)

# SKPM 1.0 HAL
$(call inherit-product-if-exists, vendor/samsung/hal-server/skpm/skpmHal.mk)

# SEM 1.0 HAL
$(call inherit-product-if-exists, vendor/samsung/hal-server/sem/semHal.mk)

# DQE xml
ifneq ($(BOARD_DQE_VENDOR_XML_PATH),)
ifneq ($(BOARD_DQE_VENDOR_XML_SUBDIR),)
PRODUCT_COPY_FILES += $(foreach xml,\
       $(wildcard $(BOARD_DQE_VENDOR_XML_PATH)/$(BOARD_DQE_VENDOR_XML_SUBDIR)/*.xml),\
       $(xml):$(TARGET_COPY_OUT_VENDOR)/etc/dqe/$(notdir $(xml)))
endif
PRODUCT_COPY_FILES += $(foreach xml,\
       $(wildcard $(BOARD_DQE_VENDOR_XML_PATH)/*.xml),\
       $(xml):$(TARGET_COPY_OUT_VENDOR)/etc/dqe/$(notdir $(xml)))
endif

PRODUCT_PACKAGES += \
	libhwjsqz

PRODUCT_PACKAGES += linker.vendor_ramdisk shell_and_utilities_vendor_ramdisk

# HTS
PRODUCT_SOONG_NAMESPACES += device/samsung/erd9945/hts
PRODUCT_PACKAGES += \
	htsd

PRODUCT_COPY_FILES += \
	device/samsung/erd9945/hts/hts.json:$(TARGET_COPY_OUT_VENDOR)/etc/hts.json

# Ramplus
ifeq ($(shell secgetspf SEC_PRODUCT_FEATURE_SYSTEM_SUPPORT_RAMPLUS_MODEL_FSTAB), FALSE)
PRODUCT_COPY_FILES += \
    device/samsung/common/fstab.ramplus:vendor/etc/fstab.ramplus
endif

PRODUCT_COPY_FILES += \
    device/samsung/common/init.ramplus.rc:vendor/etc/init/init.ramplus.rc

#mptool
$(call inherit-product-if-exists, $(SSCR_MP_TOOL_DIR)/device-mptool.mk)


ifneq ($(filter r12s%, $(TARGET_PRODUCT)),)
PRODUCT_PACKAGES += \
	libTsAwb_front_3j1 \
	libTsPrc_front_3j1 \
	libTsAwb_front_imx374 \
	libTsPrc_front_imx374
endif