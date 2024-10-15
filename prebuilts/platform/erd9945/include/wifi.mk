# WLAN configuration
# device specific wpa_supplicant.conf
PRODUCT_COPY_FILES += \
    device/samsung/erd9945/wpa_supplicant.conf:vendor/etc/wifi/wpa_supplicant.conf \
    device/samsung/erd9945/wifi/p2p_supplicant.conf:vendor/etc/wifi/p2p_supplicant.conf \
    device/samsung/erd9945/wifi/init.insmod.wifi.cfg:/vendor/etc/wifi/init.insmod.wifi.cfg \
    device/samsung/erd9945/wifi/init.exynos.wifi.rc:/vendor/etc/init/init.exynos.wifi.rc
#device/samsung/erd9945/temp.sh:vendor/etc/wifi/temp.sh

PRODUCT_PROPERTY_OVERRIDES += \
        wifi.interface=wlan0

# Packages needed for WLAN
# Note android.hardware.wifi@1.0-service is used by HAL interface 1.2
PRODUCT_PACKAGES += \
    android.hardware.wifi-service \
    android.hardware.wifi.supplicant@1.1-service \
    android.hardware.wifi.offload@1.0-service \
    dhcpcd.conf \
    hostapd \
    wificond \
    wifilog \
    wpa_supplicant.conf \
    wpa_supplicant \
    wpa_cli \
    hostapd_cli

PRODUCT_PACKAGES += \
    libwifi-hal \
    libwifi-system \
    libwifikeystorehal \
    android.hardware.wifi.supplicant@1.2 \
    android.hardware.wifi@1.3

#$(warning #### [WIFI] WLAN_VENDOR = $(WLAN_VENDOR))
#$(warning #### [WIFI] WLAN_CHIP = $(WLAN_CHIP))
#$(warning #### [WIFI] WLAN_CHIP_TYPE = $(WLAN_CHIP_TYPE))
#$(warning #### [WIFI] WIFI_NEED_CID = $(WIFI_NEED_CID))
#$(warning #### [WIFI] ARGET_BOARD_PLATFORM = $(ARGET_BOARD_PLATFORM))
#$(warning #### [WIFI] TARGET_BOOTLOADER_BOARD_NAME = $(TARGET_BOOTLOADER_BOARD_NAME))

PRODUCT_COPY_FILES += device/samsung/erd9945/wpa_supplicant.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant.conf

#supplicant configs
ifneq ($(BOARD_WPA_SUPPLICANT_DRIVER),)
CONFIG_DRIVER_$(BOARD_WPA_SUPPLICANT_DRIVER) := y
endif
WPA_SUPPL_DIR = external/wpa_supplicant_8
include $(WPA_SUPPL_DIR)/wpa_supplicant/wpa_supplicant_conf.mk
include $(WPA_SUPPL_DIR)/wpa_supplicant/android.config


