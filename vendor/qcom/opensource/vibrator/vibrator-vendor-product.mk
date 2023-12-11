#ifeq ($(HAPTICS_OFFLOAD_SUPPORT),true)
#QTI_VIBRATOR_HAL_SERVICE := \
#      vendor.qti.hardware.vibrator.offload.service
#else
#QTI_VIBRATOR_HAL_SERVICE := \
#      vendor.qti.hardware.vibrator.service
#endif
#PRODUCT_PACKAGES += $(QTI_VIBRATOR_HAL_SERVICE)

#PRODUCT_COPY_FILES += \
#      vendor/qcom/opensource/vibrator/excluded-input-devices.xml:vendor/etc/excluded-input-devices.xml
