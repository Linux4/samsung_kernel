# WFD property related on video codec
PRODUCT_PROPERTY_OVERRIDES += \
    ro.hw.wfd_use_single_plane_in_drm=0 \
    ro.hw.wfd_use_c2_encoder=1

# To build 64bit binary on 32bit/64bit mode system
PRODUCT_PACKAGES += \
    libtsmux \
    librepeater

# WFD BoardConfigs
BOARD_USES_VIRTUAL_DISPLAY := true
BOARD_USES_DISABLE_COMPOSITIONTYPE_GLES := true
BOARD_USES_SECURE_ENCODER_ONLY := true
