# Copy WLBT fw and other related files from vendor/samsung_slsi/mx140/firmware/
$(call inherit-product-if-exists, vendor/samsung_slsi/mx140/firmware/wlbt_firmware.mk)

# PRODUCT_PACKAGES for vendor/samsung_slsi/scsc_tools/dev_tools/
$(call inherit-product-if-exists, vendor/samsung_slsi/scsc_tools/dev_tools/dev_tools.mk)

# PRODUCT_PACKAGES for vendor/samsung_slsi/scsc_tools/wifi-bt/
#$(call inherit-product-if-exists, vendor/samsung_slsi/scsc_tools/wifi-bt/wifi-bt.mk)

# PRODUCT_PACKAGES for vendor/samsung_slsi/scsc_tools/test_engine
#$(call inherit-product-if-exists, vendor/samsung_slsi/scsc_tools/test_engine/test_engine.mk)

# PRODUCT_PACKAGES for vendor/samsung_slsi/scsc_tool/kic
#$(call inherit-product-if-exists, vendor/samsung_slsi/scsc_tools/kic/kic.mk)
