#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#
# Makefile for misc devices that really don't fit anywhere else.
#

# HS03S code added for SR-AL5625-01-310 by gaozhengwei at 20210423 start.
obj-$(CONFIG_MTK_LCM) += mt65xx_lcm_list.o        \
                         lcm_common.o             \
                         lcm_gpio.o               \
                         lcm_i2c.o                \
                         lcm_pmic.o               \
                         lcm_util.o               

ifeq ($(strip $(CONFIG_HQ_PROJECT_HS03S)),y)
obj-$(CONFIG_MTK_LCM) += lcm_drv.o
endif

ifeq ($(strip $(CONFIG_HQ_PROJECT_HS04)),y)
obj-$(CONFIG_MTK_LCM) += lcm_drv.o
endif

ifeq ($(strip $(CONFIG_HQ_PROJECT_OT8)),y)
obj-$(CONFIG_MTK_LCM) += mtk_lcd_gpio_ctrl.o
else
obj-$(CONFIG_MTK_LCM) += lcm_drv.o
endif
ifeq ($(strip $(CONFIG_HQ_PROJECT_O22)),y)
obj-$(CONFIG_MTK_LCM) += lcm_drv.o
endif

#ifeq ($(MTK_TARGET_PROJECT), ot8)
#      LOCAL_CFLAGS += -DHQ_PROJECT_OT8
#endif

# HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 start
obj-$(CONFIG_HQ_SET_LCD_BIAS) += lcm_bias.o
# HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 end
LCM_LISTS := $(subst ",,$(CONFIG_CUSTOM_KERNEL_LCM))
#hs04 code for DEAL6398A-1875 by zhawei at 20221017 start
ifdef CONFIG_HQ_PROJECT_O22
obj-$(CONFIG_MTK_LCM) += $(foreach LCM,$(LCM_LISTS),o22/)
else ifdef CONFIG_HQ_PROJECT_HS04
obj-$(CONFIG_MTK_LCM) += $(foreach LCM,$(LCM_LISTS),M04/)
else ifdef CONFIG_HQ_PROJECT_HS03S
obj-$(CONFIG_MTK_LCM) += $(foreach LCM,$(LCM_LISTS),$(LCM)/)
else ifdef CONFIG_HQ_PROJECT_OT8
obj-$(CONFIG_MTK_LCM) += $(foreach LCM,$(LCM_LISTS),$(LCM)/)
else
obj-$(CONFIG_MTK_LCM) += $(foreach LCM,$(LCM_LISTS),$(LCM)/)
endif
#hs04 code for DEAL6398A-1875 by zhawei at 20221017 end

ifeq (y, $(filter y, $(CONFIG_MACH_MT6761)))
    MTK_PLATFORM:= $(subst ",,mt6765)
else
    MTK_PLATFORM:= $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

subdir-ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc
subdir-ccflags-$(CONFIG_MTK_FB) += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
                                       -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/mt6735 \
                                       -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
                                       -I$(srctree)/drivers/misc/mediatek/video/include

LCM_DEFINES := $(shell echo $(CONFIG_CUSTOM_KERNEL_LCM) | tr a-z A-Z)
DEFINES += $(foreach LCM,$(LCM_DEFINES),$(LCM))
DEFINES += MTK_LCM_PHYSICAL_ROTATION=\"$(MTK_LCM_PHYSICAL_ROTATION)\"
ccflags-$(CONFIG_MTK_LCM) += $(addprefix -D, $(DEFINES))
ccflags-$(CONFIG_MTK_LCM_DEVICE_TREE_SUPPORT) += -DMTK_LCM_DEVICE_TREE_SUPPORT

