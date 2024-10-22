# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for kernel SPMI framework.
#

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
obj-$(CONFIG_DEVICE_MODULES_SPMI_MTK_PMIF)	+= spmi-mtk-pmif.o spmi-mtk-mpu.o
spmi-mtk-pmif-objs := spmi-mtk-pmif-core.o spmi-mtk-debug.o
