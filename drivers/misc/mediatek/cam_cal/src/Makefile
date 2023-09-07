$(info cam_cal: makefile start)
MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(CONFIG_MTK_PLATFORM)/camera_hw \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc
subdir-ccflags-y += -I$(srctree)/drivers/i2c/busses/
ifeq ($(CONFIG_ARCH_MTK_PROJECT), "titan6757_phone_o")
ccflags-y += -DTITAN_OTP_SUPPORT
$(info cam_cal: macro TITAN_OTP_SUPPORT defined)
endif

ifeq ($(CONFIG_ARCH_MTK_PROJECT), "titan6757_c10_n")
ccflags-y += -DC10_OTP_SUPPORT
$(info cam_cal: macro C10_OTP_SUPPORT defined)
endif

ifeq ($(CONFIG_ARCH_MTK_PROJECT), "titan6757_jade_o")
ccflags-y += -DJADE_OTP_SUPPORT
$(info cam_cal: macro JADE_OTP_SUPPORT defined)
endif

LEGACY_PLATFORM = mt6572 mt6580 mt6582 mt6735 mt6752 mt6795 mt6755
CURRENT_PLATFORM = $(subst ",,$(CONFIG_MTK_PLATFORM))

ifneq (, $(findstring $(CURRENT_PLATFORM), $(LEGACY_PLATFORM)))
#$(info cam_cal: use legacy folder)
obj-y += legacy/$(subst ",,$(CONFIG_MTK_PLATFORM))/
else
$(info cam_cal: use common folder)
# obj-y += eeprom_driver.o
obj-y += cam_cal_drv.o
obj-y += cam_cal_list.o
obj-y += common/
endif
$(info cam_cal: makefile end)

