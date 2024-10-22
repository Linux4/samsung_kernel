# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2021 MediaTek Inc.
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

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mediatek_v2 \
             -I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc \
             -I$(DEVICE_MODULES_PATH)/drivers/devfreq \
             -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat \
             -I$(DEVICE_MODULES_PATH)/drivers/iommu \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi \
             -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3

obj-$(CONFIG_MTK_MML_LEGACY) := mtk-mml-legacy.o
obj-$(CONFIG_MTK_MML) := mtk-mml.o

mtk-mml-legacy-objs := mtk-mml.o

mtk-mml-objs += mtk-mml-monitor.o

mtk-mml-objs += mtk-mml-driver.o
mtk-mml-objs += mtk-mml-core.o
mtk-mml-objs += mtk-mml-tile.o
mtk-mml-objs += mtk-mml-pq-core.o
mtk-mml-objs += mtk-mml-drm-adaptor.o
mtk-mml-objs += mtk-mml-dle-adaptor.o
mtk-mml-objs += mtk-mml-buf.o
mtk-mml-objs += mtk-mml-sys.o
mtk-mml-objs += tile_core.o
mtk-mml-objs += tile_mdp_func.o
mtk-mml-objs += DpTileScaler.o
# device components
mtk-mml-objs += mtk-mml-aal.o
mtk-mml-objs += mtk-mml-color.o
mtk-mml-objs += mtk-mml-fg.o
mtk-mml-objs += mtk-mml-fg-fw.o
mtk-mml-objs += mtk-mml-hdr.o
mtk-mml-objs += mtk-mml-mutex.o
mtk-mml-objs += mtk-mml-rdma.o
mtk-mml-objs += mtk-mml-rsz.o
mtk-mml-objs += mtk-mml-rsz-fw.o
mtk-mml-objs += mtk-mml-pq-rdma.o
mtk-mml-objs += mtk-mml-pq-birsz.o
mtk-mml-objs += mtk-mml-pq-birsz-fw.o
mtk-mml-objs += mtk-mml-tcc.o
mtk-mml-objs += mtk-mml-tdshp.o
mtk-mml-objs += mtk-mml-wrot.o
mtk-mml-objs += mtk-mml-rrot.o
mtk-mml-objs += mtk-mml-merge.o
mtk-mml-objs += mtk-mml-c3d.o
mtk-mml-objs += mtk-mml-dpc.o

# mml call sspm support
mtk-mml-objs += mtk-mml-scmi.o

ifeq ($(strip $(CONFIG_MTK_MML_DEBUG)),y)
ifneq (,$(strip $(CONFIG_MMPROFILE)))
mtk-mml-objs += mtk-mml-mmp.o
endif
endif

ifneq ($(wildcard $(srctree)/$(src)/mtk-mml-tp-mt6983.c),)
obj-$(CONFIG_MTK_MML) += mtk-mml-mt6983.o
mtk-mml-mt6983-objs := mtk-mml-tp-mt6983.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mtk-mml-tp-mt6985.c),)
obj-$(CONFIG_MTK_MML) += mtk-mml-mt6985.o
mtk-mml-mt6985-objs := mtk-mml-tp-mt6985.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mtk-mml-tp-mt6886.c),)
obj-$(CONFIG_MTK_MML) += mtk-mml-mt6886.o
mtk-mml-mt6886-objs := mtk-mml-tp-mt6886.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mtk-mml-tp-mt6897.c),)
obj-$(CONFIG_MTK_MML) += mtk-mml-mt6897.o
mtk-mml-mt6897-objs := mtk-mml-tp-mt6897.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mtk-mml-tp-mt6989.c),)
obj-$(CONFIG_MTK_MML) += mtk-mml-mt6989.o
mtk-mml-mt6989-objs := mtk-mml-tp-mt6989.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mtk-mml-tp-mt6878.c),)
obj-$(CONFIG_MTK_MML) += mtk-mml-mt6878.o
mtk-mml-mt6878-objs := mtk-mml-tp-mt6878.o
endif

# for unit test
ifeq ($(strip $(CONFIG_MTK_MML_DEBUG)),y)
mtk-mml-objs += mtk-mml-test.o
endif

# for FPGA early porting
ifeq ($(strip $(CONFIG_FPGA_EARLY_PORTING)),y)
ccflags-y += -DMML_FPGA
endif
