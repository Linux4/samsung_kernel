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

subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/aee
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ifeq ($(CONFIG_MTK_ION),y)
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/mtk_ion/
endif

subdir-ccflags-y += -I$(srctree)/arch/arm64/include/
subdir-ccflags-y += -I$(srctree)/kernel/
subdir-ccflags-y += -I$(srctree)/include/

obj-$(CONFIG_MTK_HANG_DETECT) += monitor_hang.o
monitor_hang-objs += hang_detect.o hang_unwind.o
