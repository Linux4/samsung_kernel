# SPDX-License-Identifier: GPL-2.0
#
# Makefile for regulator drivers.
ccflags-y += -I$(srctree)/drivers/regulator
CFLAGS_mtk-vmm-isp71-regulator.o := -I$(DEVICE_MODULES_PATH)/drivers/regulator
CFLAGS_mtk-dvfsrc-regulator.o := -I$(DEVICE_MODULES_PATH)/drivers/regulator

obj-$(CONFIG_DEVICE_MODULES_REGULATOR_MT6315) += mt6315-regulator.o
obj-$(CONFIG_DEVICE_MODULES_REGULATOR_MT6315) += mtk-extbuck-debug.o
obj-$(CONFIG_DEVICE_MODULES_REGULATOR_MT6316) += mt6316-regulator.o
obj-$(CONFIG_REGULATOR_MT6359P) += mt6359p-regulator.o
obj-$(CONFIG_DEVICE_MODULES_REGULATOR_MT6360) += mt6360-regulator.o
obj-$(CONFIG_REGULATOR_MT6362) += mt6362-regulator.o
obj-$(CONFIG_REGULATOR_MT6363) += mt6363-regulator.o
obj-$(CONFIG_REGULATOR_MT6368) += mt6368-regulator.o
obj-$(CONFIG_REGULATOR_MT6369) += mt6369-regulator.o
obj-$(CONFIG_REGULATOR_MT6373) += mt6373-regulator.o
obj-$(CONFIG_REGULATOR_MT6681) += mt6681-regulator.o
obj-$(CONFIG_DEVICE_MODULES_REGULATOR_MTK_DVFSRC) += mtk-dvfsrc-regulator.o
obj-$(CONFIG_REGULATOR_MTK_VMM_ISP71) += mtk-vmm-isp71-regulator.o
obj-$(CONFIG_REGULATOR_RT4803)	+= rt4803.o
obj-$(CONFIG_REGULATOR_RT5133)  += rt5133-regulator.o
obj-$(CONFIG_REGULATOR_MAX77816) += max77816.o
obj-$(CONFIG_DEVICE_MODULES_REGULATOR_RT6160)	+= rt6160-regulator.o
ifneq ($(wildcard $(srctree)/$(src)/k6878v1_64_alpha),)
obj-$(CONFIG_REGULATOR_MT6878_ALPHA_SUPPORT) += k6878v1_64_alpha/
endif
ccflags-$(CONFIG_REGULATOR_DEBUG) += -DDEBUG
