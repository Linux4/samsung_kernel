###################################
## VIDEO
####################################
# MFC firmware
PRODUCT_COPY_FILES += \
	device/samsung/erd9945/firmware/mfc_fw_v18.x.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/mfc_fw.bin

# 1. Codec 2.0
ifeq ($(BOARD_USE_DEFAULT_SERVICE), true)
# dummy service
PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/exynos/c2service

DEVICE_MANIFEST_FILE += \
	device/samsung/erd9945/manifest_media_c2_default.xml

PRODUCT_COPY_FILES += \
	device/samsung/erd9945/media_codecs_performance_c2.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_performance_c2.xml

PRODUCT_PACKAGES += \
    samsung.hardware.media.c2@1.1-default-service
else
# exynos service
PRODUCT_SOONG_NAMESPACES += hardware/samsung_slsi/codec2

DEVICE_MANIFEST_FILE += \
	device/samsung/erd9945/manifest_media_c2.xml

PRODUCT_COPY_FILES += \
	device/samsung/erd9945/media_codecs_c2.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_c2.xml \
	device/samsung/erd9945/media_codecs_performance_c2.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_performance_c2.xml

PRODUCT_PACKAGES += \
    samsung.hardware.media.c2@1.2-service \
    codec2.vendor.base.policy \
    codec2.vendor.ext.policy \
    libc2filterplugin \
    libExynosC2ComponentStore \
    libExynosC2H264Dec \
    libExynosC2H264Enc \
    libExynosC2HevcDec \
    libExynosC2HevcEnc \
    libExynosC2Vp8Dec \
    libExynosC2Vp8Enc \
    libExynosC2Vp9Dec \
    libExynosC2Vp9Enc \
    libExynosC2Av1Dec

# HDR10+ Profile A/B MetaGenerator
ifeq ($(BOARD_USE_HDR10PLUS_STAT_ENC), true)
PRODUCT_PACKAGES += \
	libHDRMetaGenerator
endif

PRODUCT_PROPERTY_OVERRIDES += \
	vendor.media.omx=0 \
	debug.stagefright.c2inputsurface=-1 \
	debug.stagefright.ccodec_strict_type=true \
	debug.stagefright.ccodec_lax_type=true \
	debug.stagefright.c2-poolmask=458752 \
	media.c2.dmabuf.padding=512 \
	debug.c2.use_dmabufheaps=1 \
	debug.stagefright.ccodec_delayed_params=1 \
	ro.vendor.gpu.dataspace=1

ifeq ($(BOARD_USE_COMPRESSED_COLOR), true)
PRODUCT_PROPERTY_OVERRIDES += \
    vendor.debug.c2.sbwc.enable=true
endif

ifeq ($(BOARD_USE_MFC_HEADER), true)
PRODUCT_SOONG_NAMESPACES += vendor/samsung_slsi/exynos/video
endif

# 2. OpenMAX IL
PRODUCT_COPY_FILES += \
	device/samsung/erd9945/media_codecs_performance.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_performance.xml \
	device/samsung/erd9945/media_codecs.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs.xml
endif

# Camera
PRODUCT_COPY_FILES += \
	device/samsung/erd9945/media_profiles.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml

