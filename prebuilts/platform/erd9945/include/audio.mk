#  AUDIO & VOICE
ifneq ($(filter full_erd9945_s5400_u,$(TARGET_PRODUCT)),)
$(call inherit-product-if-exists, device/samsung/audio/erd9945/device-vendor-audio.mk)
else
$(call inherit-product-if-exists, device/samsung/audio/erd9945_sec/device-vendor-audio.mk)
endif
