# Libacryl
SOONG_CONFIG_NAMESPACES += libacryl
SOONG_CONFIG_libacryl += default_scaler
ifneq ($(TARGET_USE_EVT0), true)
SOONG_CONFIG_libacryl_default_scaler := mscl_sbwc_v2_8
else
SOONG_CONFIG_libacryl_default_scaler := mscl_sbwc_v2_7
endif

# LIBSBWCWRAPPER
SOONG_CONFIG_NAMESPACES += sbwcwrapper
SOONG_CONFIG_sbwcwrapper += sbwcwrapper_priority
SOONG_CONFIG_sbwcwrapper_sbwcwrapper_priority := mscl

# Sbwc Decomp
PRODUCT_PACKAGES += \
    vendor.samsung_slsi.hardware.SbwcDecompService@1.0-service

# JPEG HAL
PRODUCT_PACKAGES += \
    libhwjpeg
