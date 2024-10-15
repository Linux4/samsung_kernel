# Copyright (C) 2023 The Android Open Source Project
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
# This file is the build configuration for a full Android
# build for erd9945 hardware. This cleanly combines a set of
# device-specific aspects (drivers) with a device-agnostic
# product configuration (apps). Except for a few implementation
# details, it only fundamentally contains two inherit-product
# lines, full and erd9945, hence its name.
#

# ifeq ($(TARGET_PRODUCT),full_erd9945_evt0_u)
TARGET_USE_EVT0 := true
# ifeq ($(TARGET_BUILD_VARIANT),eng)
# SANITIZE_TARGET := hwaddress
# endif

# Check System uses Virtual AB system
EXYNOS_USE_VIRTUAL_AB := false

# Check Exynos Product support dedicated recovery image
# For Non-AB  -> Recovery Image is mandatory
# For AB      -> Recovery Image is optional
ifneq ($(EXYNOS_USE_VIRTUAL_AB), true)
EXYNOS_USE_DEDICATED_RECOVERY_IMAGE := true
endif

# Do not build system image if WITH_ESSI_64 specified as true
ifeq ($(WITH_ESSI_64),true)
PRODUCT_BUILD_SYSTEM_IMAGE := false
PRODUCT_BUILD_SYSTEM_DLKM_IMAGE := true
PRODUCT_BUILD_SYSTEM_OTHER_IMAGE := false
PRODUCT_BUILD_VENDOR_IMAGE := true
PRODUCT_BUILD_PRODUCT_IMAGE := false
PRODUCT_BUILD_PRODUCT_SERVICES_IMAGE := false
PRODUCT_BUILD_ODM_IMAGE := true
PRODUCT_BUILD_CACHE_IMAGE := false
PRODUCT_BUILD_RAMDISK_IMAGE := true
PRODUCT_BUILD_USERDATA_IMAGE := false
PRODUCT_BUILD_BOOT_IMAGE := true
PRODUCT_BUILD_VBMETA_IMAGE := true
PRODUCT_BUILD_INIT_BOOT_IMAGE := false

# If Target Support dedicated reocvery image, set property to build recovery image
ifeq ($(EXYNOS_USE_DEDICATED_RECOVERY_IMAGE), true)
PRODUCT_BUILD_RECOVERY_IMAGE := true
endif

# Also, since we're going to skip building the system image, we also skip
# building the OTA package. We'll build this at a later step. We also don't
# need to build the OTA tools package (we'll use the one from the system build).
TARGET_SKIP_OTA_PACKAGE := true
TARGET_SKIP_OTATOOLS_PACKAGE := true
endif

# Inherit from those products. Most specific first.
$(call inherit-product, device/samsung/erd9945/device.mk)

PRODUCT_BOARD := erd9945
TARGET_DEVICE_NAME := erd9945
TARGET_SOC := s5e9945
TARGET_SOC_BASE := s5e9945
TARGET_BOOTLOADER_BOARD_NAME := s5e9945
TARGET_BOARD_PLATFORM := erd9945

PRODUCT_NAME := full_erd9945_evt0_u
PRODUCT_DEVICE := erd9945
PRODUCT_BRAND := Exynos
PRODUCT_MODEL := Full Android on S5E9945 ERD
PRODUCT_MANUFACTURER := Samsung Electronics Co., Ltd.

# endif
