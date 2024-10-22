# SPDX-License-Identifier: GPL-2.0

VERSION := v2

# common include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/adsp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek

# target object
obj-${CONFIG_MTK_ADSP_V2} += adsp-$(VERSION).o

adsp-$(VERSION)-objs += adsp_driver_$(VERSION).o
adsp-$(VERSION)-objs += adsp_clk_audio.o
