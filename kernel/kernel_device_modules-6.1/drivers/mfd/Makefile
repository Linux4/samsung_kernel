# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_MFD_MT6338)	+= mt6338-core.o
obj-$(CONFIG_DEVICE_MODULES_MFD_MT6360)	+= mt6360-core.o
mt6397-objs			:= mt6397-core.o mt6397-irq.o mt6358-irq.o
obj-$(CONFIG_DEVICE_MODULES_MFD_MT6397)	+= mt6397.o
obj-$(CONFIG_DEVICE_MODULES_MFD_MT6397)        += mt63xx-debug.o
obj-$(CONFIG_MFD_MT6375)	+= mt6375.o
obj-$(CONFIG_MFD_MT6681)	+= mt6681-core.o
obj-$(CONFIG_MT6685_AUDCLK)	+= mt6685-audclk.o
obj-$(CONFIG_MFD_MT6685)	+= mt6685-core.o
obj-$(CONFIG_MFD_MTK_SPMI_PMIC)	+= mtk-spmi-pmic.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_MFD_MTK_SPMI_PMIC)	+= mtk-spmi-pmic-debug.o
endif
obj-$(CONFIG_MFD_RT9490)	+= rt9490.o
