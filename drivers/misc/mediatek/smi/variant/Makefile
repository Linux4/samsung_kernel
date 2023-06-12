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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/

obj-y += smi_variant.o
obj-y += smi_debug.o

ifeq ($(CONFIG_ARCH_MT8173),y)
obj-y += smi_variant_config_8173.o
ccflags-y += -DMT73
endif

ifeq ($(CONFIG_ARCH_MT8127),y)
ccflags-y += -DMT27
obj-y += smi_variant_config_8127.o
endif
