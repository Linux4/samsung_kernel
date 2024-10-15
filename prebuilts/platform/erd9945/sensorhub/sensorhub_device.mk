# Sensor Hub HAL
ifneq ($(filter mu1s% mu2s% mu3s% e1s% e2s%, $(TARGET_PRODUCT)),)
SHUB_FIRMWARE_NAME := muse
endif

ifneq ($(filter r12s%, $(TARGET_PRODUCT)),)
SHUB_FIRMWARE_NAME := r12s
endif

PRODUCT_PACKAGES += \
	android.hardware.sensors-service.multihal \
	libsensorndkbridge

PRODUCT_PACKAGES += \
	sensors.sensorhub \
	sensors.inputvirtual

PRODUCT_COPY_FILES += \
	device/samsung/erd9945/sensorhub/init.sensorhub.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.sensorhub.rc

PRODUCT_COPY_FILES += \
	device/samsung/erd9945/firmware/sensorhub/shub_root_$(SHUB_FIRMWARE_NAME).bin:$(TARGET_COPY_OUT_VENDOR)/firmware/os.checked.bin