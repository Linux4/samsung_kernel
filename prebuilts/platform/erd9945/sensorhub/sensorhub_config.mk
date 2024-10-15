ifneq ($(filter mu1s% mu2s% mu3s%, e1s%, e2s%, $(TARGET_PRODUCT)),)
SHUB_FIRMWARE_NAME := muse
endif

# firmware signing
ifeq ($(SEC_BUILD_OPTION_KNOX_CSB),true)
type=${VBMETA_TYPE}
project=${SEC_BUILD_CONF_MODEL_SIGNING_NAME}
SECURE_SCRIPT=../buildscript/tools/signclient.jar

UNSIGN_SENSORHUB := device/samsung/$(TARGET_BOARD_PLATFORM)/firmware/sensorhub/shub_root_$(SHUB_FIRMWARE_NAME)_unchecked.bin
SIGN_SENSORHUB := device/samsung/$(TARGET_BOARD_PLATFORM)/firmware/sensorhub/shub_root_$(SHUB_FIRMWARE_NAME).bin
RENAMING_SENSORHUB := $(shell mv $(SIGN_SENSORHUB) $(UNSIGN_SENSORHUB))
SIGNNING_SENSORHUB := $(shell java -jar $(SECURE_SCRIPT) -runtype ${type} -model ${project} -input $(UNSIGN_SENSORHUB) -output $(SIGN_SENSORHUB))
endif

SENSOR_HAL := SENSOR_HUB
SENSOR_MCU := LSI

USE_SENSOR_MULTI_HAL := true
SENSOR_INPUT_VIRTUAL_HAL := true