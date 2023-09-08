#
# Makefile for the kernel flashlight device drivers.
#

flashlight-y := flashlight-core.o
flashlight-y += flashlight-device.o

obj-$(CONFIG_MTK_FLASHLIGHT)             += flashlight.o
obj-$(CONFIG_MTK_FLASHLIGHT_LED191)      += flashlights-led191.o
obj-$(CONFIG_MTK_FLASHLIGHT_LM3642)      += flashlights-lm3642.o
obj-$(CONFIG_MTK_FLASHLIGHT_LM3643)      += flashlights-lm3643.o
obj-$(CONFIG_MTK_FLASHLIGHT_LM3644)      += flashlights-lm3644.o
obj-$(CONFIG_MTK_FLASHLIGHT_RT4505)      += flashlights-rt4505.o
obj-$(CONFIG_MTK_FLASHLIGHT_RT5081)      += flashlights-rt5081.o
obj-$(CONFIG_MTK_FLASHLIGHT_MT6336)      += flashlights-mt6336.o
obj-$(CONFIG_MTK_FLASHLIGHT_MT6370)      += flashlights-mt6370.o
obj-$(CONFIG_MTK_FLASHLIGHT_MT6360)      += flashlights-mt6360.o
obj-$(CONFIG_MTK_FLASHLIGHT_AW3644)      += flashlights-aw3644.o
obj-$(CONFIG_MTK_FLASHLIGHT_SYWT78)      += flashlights-sywt78.o
obj-$(CONFIG_MTK_FLASHLIGHT_DUMMY)       += flashlights-dummy.o
obj-$(CONFIG_MTK_FLASHLIGHT_DUMMY_GPIO)  += flashlights-dummy-gpio.o
#hs03s code add for flashlights_aw36518 by chengzhi at 2021.06.13 begin
#can't change this order,iic device must be first,otherwise can't compatible device
obj-$(CONFIG_MTK_FLASHLIGHT_AW36518)  += flashlights-aw36518.o
#hs03s code add for flashlights_aw36518 by chengzhi at 2021.06.13 end

#hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/18 start
ifeq ($(CONFIG_HQ_PROJECT_HS04),y)
obj-$(CONFIG_MTK_FLASHLIGHT_O21_SGM3785_GPIO)  += flashlights-o21-sgm3785-gpio.o
endif
#hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/18 end

# hs14 code for SR-AL5628-01-161 by lisizhou at 2022-9-26 start
ifeq ($(CONFIG_HQ_PROJECT_O22), y)
obj-$(CONFIG_MTK_FLASHLIGHT_SGM3785_GPIO)  += flashlights-sgm3785-gpio-hq.o
else
obj-$(CONFIG_MTK_FLASHLIGHT_SGM3785_GPIO)  += flashlights-sgm3785-gpio.o
endif
# hs14 code for SR-AL5628-01-161 by lisizhou at 2022-9-26 end

ccflags-$(CONFIG_MTK_FLASHLIGHT_DEBUG)     := -DDEBUG
ccflags-$(CONFIG_MTK_FLASHLIGHT_SYWT78)    += -DCONFIG_MTK_FLASHLIGHT_SYWT78
ccflags-$(CONFIG_MTK_FLASHLIGHT_PT)        += -DCONFIG_MTK_FLASHLIGHT_PT
ccflags-$(CONFIG_MTK_FLASHLIGHT_PT_STRICT) += -DCONFIG_MTK_FLASHLIGHT_PT_STRICT
ccflags-$(CONFIG_MTK_FLASHLIGHT_DLPT)      += -DCONFIG_MTK_FLASHLIGHT_DLPT

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek

ccflags-y += -D$(MTK_PLATFORM) -D$(MTK_PROJECT)
ifeq ($(CONFIG_MTK_FLASHLIGHT_AW3644),y)
ccflags-y += -DCONFIG_MTK_FLASHLIGHT_AW3644
endif
subdir-ccflags-y += \
	-I$(MTK_CUSTOM_PATH)/include/mt-plat \
	-I$(MTK_CUSTOM_PATH)/include/mt-plat/$(MTK_PLATFORM)/include/mach \
	-I$(MTK_CUSTOM_PATH)/pmic/include \
	-I$(MTK_CUSTOM_PATH)/pmic \
	-I$(MTK_CUSTOM_PATH)/power_throttling

