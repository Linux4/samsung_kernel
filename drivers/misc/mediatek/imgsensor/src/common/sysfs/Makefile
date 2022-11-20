SAMSUNG_MODEL_NAME  := $(subst ",,$(CONFIG_CAMERA_SAMSUNG_MODEL_NAME))

SYSFS_PATH          := $(srctree)/drivers/misc/mediatek/imgsensor/src/common/sysfs

# Build common sysfs files
obj-y				+= imgsensor_sysfs.o
obj-y				+= imgsensor_vendor.o
obj-y				+= crc32.o
obj-y				+= imgsensor_vendor_rom_config.o

# Select imgsensor_vendor_rom_config by SAMSUNG_MODEL_NAME
ROM_CONFIG_FILE     := imgsensor_vendor_rom_config_$(SAMSUNG_MODEL_NAME).o

ifneq ($(wildcard $(SYSFS_PATH)/$(subst .o,.c,$(ROM_CONFIG_FILE))),)
obj-y				+= ROM_CONFIG_FILE
else
$(info Build default file: imgsensor_vendor_rom_config_default.o)
obj-y				+= imgsensor_vendor_rom_config_default.o
endif

# Build cal map by project
ifdef CONFIG_CAMERA_AAU_V22EX
obj-y				+= imgsensor_eeprom_rear_imx258_v008.o
obj-y				+= imgsensor_otp_front_gc5035_v008.o
endif

ifdef CONFIG_CAMERA_AAU_V13X
obj-y				+= imgsensor_eeprom_rear_s5kjn1_v008.o
obj-y				+= imgsensor_otp_front_gc5035_v008.o
obj-y				+= imgsensor_otp_rear2_gc02m1b_v008.o
obj-y				+= imgsensor_eeprom_rear3_gc02m1_v008.o
endif
