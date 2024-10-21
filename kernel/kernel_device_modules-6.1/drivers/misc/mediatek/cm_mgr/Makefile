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

ccflags-y += \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cm_mgr/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM) \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi/ \
	-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/qos \
	-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2/ \
	-I$(srctree)/include \

ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/sspm/v2
endif

obj-${CONFIG_MTK_CM_MGR} += mtk_cm_mgr.o
mtk_cm_mgr-y = mtk_cm_mgr_common.o

obj-${CONFIG_MTK_CM_IPI} += mtk_cm_ipi.o

ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6853.c),)
obj-${CONFIG_MTK_CM_MGR_MT6853} += mtk_cm_mgr_mt6853.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6855.c),)
obj-${CONFIG_MTK_CM_MGR_MT6855} += mtk_cm_mgr_mt6855.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6873.c),)
obj-${CONFIG_MTK_CM_MGR_MT6873} += mtk_cm_mgr_mt6873.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6878.c),)
obj-${CONFIG_MTK_CM_MGR_MT6878} += mtk_cm_mgr_mt6878.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6879.c),)
obj-${CONFIG_MTK_CM_MGR_MT6879} += mtk_cm_mgr_mt6879.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6886.c),)
obj-${CONFIG_MTK_CM_MGR_MT6886} += mtk_cm_mgr_mt6886.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6893.c),)
obj-${CONFIG_MTK_CM_MGR_MT6893} += mtk_cm_mgr_mt6893.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6895.c),)
obj-${CONFIG_MTK_CM_MGR_MT6895} += mtk_cm_mgr_mt6895.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6897.c),)
obj-${CONFIG_MTK_CM_MGR_MT6897} += mtk_cm_mgr_mt6897.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6983.c),)
obj-${CONFIG_MTK_CM_MGR_MT6983} += mtk_cm_mgr_mt6983.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6985.c),)
obj-${CONFIG_MTK_CM_MGR_MT6985} += mtk_cm_mgr_mt6985.o
endif
ifneq ($(wildcard $(srctree)/$(src)/mtk_cm_mgr_mt6989.c),)
obj-${CONFIG_MTK_CM_MGR_MT6989} += mtk_cm_mgr_mt6989.o
endif
