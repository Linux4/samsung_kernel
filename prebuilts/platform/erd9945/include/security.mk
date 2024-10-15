## Enable Camellia OS
#BOARD_CAMELLIA_OS_ENABLE := false
#
#Weaver config
#BOARD_USES_WEAVER := false
#
# AVB2.0, to tell PackageManager that the system supports Verified Boot(PackageManager.FEATURE_VERIFIED_BOOT)
ifeq ($(BOARD_AVB_ENABLE), true)
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.software.verified_boot.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.verified_boot.xml
endif

USE_BLOWFISH := true
TEEGRIS_VERSION := 5

PRODUCT_PACKAGES += \
    tzdaemon \
    tzts_daemon \
    libteecl \
    libuuid \
    libteecl.vendor \
    libuuid.vendor \
    tzapp

TEEGRIS_GATEKEEPER_ENABLE := true
PRODUCT_TRUSTZONE_ENABLED := true
PRODUCT_TRUSTZONE_TYPE := exynos9xxx

PRODUCT_TEEGRIS_TUI_ENABLED := true
PRODUCT_TEEGRIS_TUILL_ENABLED := true

ifeq ($(shell secgetspf SEC_PRODUCT_FEATURE_SECURITY_SUPPORT_MDFPP_KEYMASTER), TRUE)
#Gatekeeper
    USE_SEC_FEATURE_MDFPP_KEYMASTER_VER := $(shell secgetspf SEC_PRODUCT_FEATURE_SECURITY_CONFIG_MDFPP_KEYMASTER_VERSION)
    SKEYMASTER_VER := $(shell expr $(USE_SEC_FEATURE_MDFPP_KEYMASTER_VER) / 10)
    ifeq ($(call math_gt_or_eq,$(SKEYMASTER_VER),30),true)
        PRODUCT_PACKAGES += \
            android.hardware.gatekeeper-service \
            android.hardware.gatekeeper-service.rc \
            gatekeeper.$(TARGET_SOC)
    else
        PRODUCT_PACKAGES += \
            android.hardware.gatekeeper@1.0-impl \
            android.hardware.gatekeeper@1.0-service \
            gatekeeper.$(TARGET_SOC)
    endif
endif

# Gatekeeper
#ifeq ($(BOARD_TEE_GATEKEEPER_ENABLE), false)
#PRODUCT_PACKAGES += \
#	android.hardware.gatekeeper@1.0_dummy-service
#else # TEE GATEKEEPER
#PRODUCT_PACKAGES += \
#	android.hardware.gatekeeper@1.0_tee-service

#PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/$(TARGET_SOC)/TlcTeeGatekeeper
# GateKeeper TA
#PRODUCT_COPY_FILES += \
#        vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/07061000000000000000000000000000.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/07061000000000000000000000000000.tlbin

#endif
#
#CryptoManager
#PRODUCT_PACKAGES += \
#	tlcmdrv \

# CryptoManager test app for eng build
#ifneq (,$(filter eng, $(TARGET_BUILD_VARIANT)))
#PRODUCT_PACKAGES += \
#	tlcmtest \
#	cm_test
#endif

# CryptoManager - tlcmdrv
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/FFFFFFFFD00000000000000000000016.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/FFFFFFFFD00000000000000000000016.tlbin

# CryptoManager test app for eng build
#ifneq (,$(filter eng, $(TARGET_BUILD_VARIANT)))
# tlcmtest / cm_test
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/04010000000000000000000000000000.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/04010000000000000000000000000000.tlbin \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/cm_test:$(TARGET_COPY_OUT_VENDOR)/app/cm_test
#endif

# RPMB_TA
#PRODUCT_PACKAGES += \
	tlrpmb

# RPMB test application
#PRODUCT_PACKAGES += \
	bRPMB

# RPMB TA - tlrpmb
#PRODUCT_COPY_FILES += \
	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/09090000070100010000000000000000.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/09090000070100010000000000000000.tlbin

# RPMB Unit Test - bRPMB
#PRODUCT_COPY_FILES += \
	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/bRPMB:$(TARGET_COPY_OUT_VENDOR)/app/bRPMB

#ifneq (,$(filter eng, $(TARGET_BUILD_VARIANT)))
## SNVM
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/snvm/4e564d00-1234-4234-89ab-000000000002:$(TARGET_COPY_OUT_VENDOR)/app/snvm/4e564d00-1234-4234-89ab-000000000002 \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/snvm/camellia_tc_nvm_update_tool:$(TARGET_COPY_OUT_VENDOR)/app/snvm/camellia_tc_nvm_update_tool
#endif
#
# TEE Keymint
#ifeq ($(BOARD_TEE_KEYMINT_ENABLE), false)
#PRODUCT_PACKAGES += \
#	android.hardware.security.keymint-service
#else # TEE KEYMINT
#PRODUCT_PACKAGES += \
#       android.hardware.security.keymint@3.0_tee-service

#PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/exynos/tee/TlcKeymint_v3_14

# CTS require keystore attest key flag in package manager
#PRODUCT_COPY_FILES += \
#	frameworks/native/data/etc/android.hardware.keystore.app_attest_key.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.keystore.app_attest_key.xml \
#	frameworks/native/data/etc/android.software.device_id_attestation.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.device_id_attestation.xml \

#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/0706000000000000000000000000004d.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/0706000000000000000000000000004d.tlbin

# Dice Sample Driver
#PRODUCT_COPY_FILES += \
#        vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/07210000000000000000000000000000.drbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/07210000000000000000000000000000.drbin

# SPL for AVB, KeyMint
VENDOR_SECURITY_PATCH := $(PLATFORM_SECURITY_PATCH)
#BOOT_SECURITY_PATCH := $(PLATFORM_SECURITY_PATCH)
#endif

# strongbox_keymint_ta
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/00000000000000000000534258505859.tabin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/00000000000000000000534258505859.tabin

# strongbox_keymint_sa
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/keymint_sa.pkg:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/keymint_sa.pkg

# strongbox_keymint_service
#PRODUCT_PACKAGES += \
#	android.hardware.security.keymint@1.0_strongbox-service

# strongbox_keymint_ca
#PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/exynos/ssp/strongbox_keymint

##PRODUCT_PACKAGES += \
#	wait_for_dual_keymint
#
#PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/$(TARGET_SOC_BASE)/see/strongbox_keymint
## PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/exynos/ssp/wait_for_dual_keymint
#
#ifeq ($(BOARD_USES_WEAVER),true)
#Weaver
#PRODUCT_PACKAGES += \
#	android.hardware.weaver@2.0-service

# weaver sa
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/05010000000000000000000000000000.tabin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/05010000000000000000000000000000.tabin
#
#PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/exynos/weaver
#endif
#
# Storage: for factory reset protection feature
#PRODUCT_PROPERTY_OVERRIDES += \
#	ro.frp.pst=/dev/block/by-name/frp
#
## Camellia OS
#ifeq ($(BOARD_CAMELLIA_OS_ENABLE), true)
## STRONG persistent data daemon
#PRODUCT_PACKAGES += \
#	pdataDaemon
#
#PRODUCT_PACKAGES += \
#	libCamellia
#
#PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/$(TARGET_SOC_BASE)/see/camellia
#
## Camellia tool for eng build
#ifneq (,$(filter eng, $(TARGET_BUILD_VARIANT)))
#PRODUCT_COPY_FILES += \
#        vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/camellia/cadb:$(TARGET_COPY_OUT_VENDOR)/app/cadb
#endif
#
## Camellia Interface driver
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/ffffffffd000000000000000000001ca.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/ffffffffd000000000000000000001ca.tlbin
#
#endif # BOARD_CAMELLIA_OS_ENABLE

#PRODUCT_PACKAGES += \
	android.hardware.drm@latest-service.clearkey

# WideVine modules
TARGET_BUILD_WIDEVINE := nonupdatable
TARGET_BUILD_WIDEVINE_USE_PREBUILT := false
-include vendor/widevine/libwvdrmengine/apex/device/device.mk

# SecureDRM modules
#PRODUCT_PACKAGES += \
	tlwvdrm \
	tlsecdrm \
	liboemcrypto_modular

# tlwvdrm
#PRODUCT_COPY_FILES += \
	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/00060308060501020000000000000000.tabin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/00060308060501020000000000000000.tabin
#
## TUI
#PRODUCT_COPY_FILES += \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/FFFF0000000000000000000000000014.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/FFFF0000000000000000000000000014.tlbin \
#	vendor/samsung_slsi/$(TARGET_SOC_BASE)/secapp/070c0000000000000000000000000000.tlbin:$(TARGET_COPY_OUT_VENDOR)/app/mcRegistry/070c0000000000000000000000000000.tlbin
