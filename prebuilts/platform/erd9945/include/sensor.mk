#copy sensor xml
PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.sensor.barometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.barometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/native/data/etc/android.hardware.sensor.hifi_sensors.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.hifi_sensors.xml \
	device/samsung/erd9945/init.exynos.nanohub.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.exynos.sensorhub.rc \
	frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepcounter.xml \
	frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepdetector.xml \
# Copy CHUB os binary files
ifneq ("$(wildcard device/samsung/erd9945/firmware/os.checked*.bin)", "")
PRODUCT_COPY_FILES += $(foreach image,\
    $(wildcard device/samsung/erd9945/firmware/os.checked*.bin),\
	$(image):$(TARGET_COPY_OUT_VENDOR)/firmware/$(notdir $(image)))
else
# PRODUCT_COPY_FILES += \
	device/samsung/erd9945/firmware/chub_nanohub_0.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/os.checked_0.bin \
	device/samsung/erd9945/firmware/chub_nanohub_1.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/os.checked_1.bin
endif

# Copy CHUB os elf file
ifneq ("$(wildcard device/samsung/erd9945/firmware/os.checked*.elf)", "")
PRODUCT_COPY_FILES += $(foreach image,\
    $(wildcard device/samsung/erd9945/firmware/os.checked*.elf),\
	$(image):$(PRODUCT_OUT)/firmware_symbol/$(notdir $(image)))
endif

# SENSORHAL
NANOHUB_SENSORHAL_SENSORLIST := $(LOCAL_PATH)/sensorhal/sensorlist.cpp
NANOHUB_SENSORHAL_EXYNOS_CONTEXTHUB := true
NANOHUB_SENSORHAL_NAME_OVERRIDE := sensors.$(TARGET_BOARD_PLATFORM)

ifneq ($(BOARD_USES_EXYNOS_SENSORS_DUMMY), true)
ifeq ($(NANOHUB_SENSORHAL2), true)
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/sensors.hals.conf:$(TARGET_COPY_OUT_VENDOR)/etc/sensors/hals.conf

PRODUCT_PACKAGES += \
	$(NANOHUB_SENSORHAL_NAME_OVERRIDE) \
	android.hardware.sensors-service.aidl_exynos \
	libexynossensor_aidl_impl
else
TARGET_USES_NANOHUB_SENSORHAL := true
PRODUCT_PACKAGES += \
	$(NANOHUB_SENSORHAL_NAME_OVERRIDE) \
	android.hardware.sensors@1.0-impl \
	android.hardware.sensors@1.0-service
endif # NANOHUB_SENSORHAL2

#sensor utilities (only for userdebug and eng builds)
ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
PRODUCT_PACKAGES += \
	samsung_nanotool \
	nanoap_cmd \
	sensortest \
	sensortest_rand
endif

ifeq ($(NANOHUB_TESTSUITE), true)
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/sensorhal/chub_test_pr.sh:$(TARGET_COPY_OUT_VENDOR)/bin/chub_test_pr.sh \
	$(LOCAL_PATH)/sensorhal/chub_test_reboot.sh:$(TARGET_COPY_OUT_VENDOR)/bin/chub_test_reboot.should

PRODUCT_PACKAGES += \
	chub_test_pr
endif

else
#Sensor HAL
PRODUCT_PACKAGES += \
	android.hardware.sensors@1.0-impl \
	android.hardware.sensors@1.0-service \
	sensors.$(TARGET_SOC)
endif

