#
# Copyright (C) 2022 MediaTek Inc.
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
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/security/selinux/include/
# flask.h is created in runtime when building gki_kernel, so we have
# to specially set the path for it. (ugly hack)
ccflags-y += -I$(OUT_DIR)/../gki_kernel/$(KERNEL_DIR)/security/selinux/
# https://android-review.googlesource.com/c/kernel/build/+/2512040
ccflags-y += -Isecurity/selinux/
obj-$(CONFIG_MTK_SELINUX_AEE_WARNING) += mtk_selinux_aee_warning.o

ifeq ($(strip $(MGK_INTERNAL)),true)
mtk_selinux_aee_warning-$(CONFIG_MTK_SELINUX_AEE_WARNING) += mtk_selinux_warning.o
else
mtk_selinux_aee_warning-$(CONFIG_MTK_SELINUX_AEE_WARNING) += dummy.o
endif
