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
ifdef CONFIG_CAMERA_MMV_V53X
obj-y				+= imgsensor_eeprom_rear_s5khm6_v008.o
obj-y				+= imgsensor_eeprom_rear2_imx355_v008.o
obj-y				+= imgsensor_otp_rear3_gc02m1b_v008.o
obj-y				+= imgsensor_eeprom_rear4_gc02m1_v008.o
obj-y				+= imgsensor_eeprom_front_imx616_v007.o
endif

ifdef CONFIG_CAMERA_AAU_V13X
obj-y				+= imgsensor_eeprom_rear_s5kjn1_v008.o
obj-y				+= imgsensor_otp_front_gc5035_v008.o
obj-y				+= imgsensor_otp_rear2_gc02m1b_v008.o
obj-y				+= imgsensor_eeprom_rear3_gc02m1_v008.o
endif
ifdef CONFIG_CAMERA_AAU_V22
# imx582 without OIS
obj-y				+= imgsensor_eeprom_rear_imx582_gc5035_v001.o
# imx582 with OIS
obj-y				+= imgsensor_eeprom_rear_imx582_v008.o
obj-y				+= imgsensor_eeprom_front_imx258_v008.o
obj-y				+= imgsensor_eeprom_rear2_imx355_v008.o
obj-y				+= imgsensor_otp_rear3_gc02m1b_v008.o
obj-y				+= imgsensor_eeprom_rear4_gc02m1_v008_2.o
endif
ifdef CONFIG_CAMERA_AAT_V32X
obj-y				+= imgsensor_eeprom_rear_gm2_v008_2.o
obj-y				+= imgsensor_otp_front_3l6_v007_2.o
obj-y				+= imgsensor_otp_rear2_sr846d_v008.o
obj-y				+= imgsensor_otp_rear3_gc02m1b_v007.o
obj-y				+= imgsensor_otp_rear4_gc5035_v008.o
endif
ifdef CONFIG_CAMERA_AAT_V31
obj-y				+= imgsensor_eeprom_rear_gm2_gc5035_v001.o
obj-y				+= imgsensor_eeprom_front_hi2021_v001.o
obj-y				+= imgsensor_eeprom_rear2_4ha_v002.o
obj-y				+= imgsensor_eeprom_rear4_gc5035_v001.o
endif
ifdef CONFIG_CAMERA_AAT_V41
obj-y				+= imgsensor_eeprom_rear_imx582_gc5035_v001.o
obj-y				+= imgsensor_eeprom_front_imx576_v001.o
obj-y				+= imgsensor_eeprom_front_s5k2x5sp13_v001.o
obj-y				+= imgsensor_eeprom_rear2_4ha_v002.o
endif
ifdef CONFIG_CAMERA_AAU_V32
obj-y				+= imgsensor_eeprom_rear_gw3_v008.o
obj-y				+= imgsensor_eeprom_front_hi2021q_v008.o
obj-y				+= imgsensor_otp_rear2_sr846d_v008.o
obj-y				+= imgsensor_otp_rear3_gc5035b_v007.o
obj-y				+= imgsensor_otp_rear4_gc5035_v008.o
endif
ifdef CONFIG_CAMERA_MMU_V32
obj-y				+= imgsensor_eeprom_rear_gw3_v008.o
obj-y				+= imgsensor_otp_front_hi2021q_v008_001.o
obj-y				+= imgsensor_eeprom_rear2_imx355_v008.o
obj-y				+= imgsensor_otp_rear3_gc02m1b_v008.o
obj-y				+= imgsensor_eeprom_rear4_gc02m1_v008_2.o
endif
ifdef CONFIG_CAMERA_MMU_V22
obj-y				+= imgsensor_eeprom_rear_gm2_v008_3.o
obj-y				+= imgsensor_eeprom_front_imx258_v008.o
obj-y				+= imgsensor_eeprom_rear2_imx355_v008.o
obj-y				+= imgsensor_otp_rear3_gc02m1b_v008.o
obj-y				+= imgsensor_eeprom_rear4_gc02m1_v008_2.o
endif
ifdef CONFIG_CAMERA_AAU_V22EX
obj-y				+= imgsensor_eeprom_rear_imx258_v008.o
obj-y				+= imgsensor_otp_front_gc5035_v008.o
endif
