#
# Copyright (c) 2024 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_SEC_DISPLAYPORT) += sec_displayport_mtk.o
sec_displayport_mtk-y += sec_dp_mtk.o \
			sec_dp_sysfs.o \
			sec_dp_edid.o

sec_displayport_mtk-$(CONFIG_SEC_DISPLAYPORT_LOGGER)	+= dp_logger.o
sec_displayport_mtk-$(CONFIG_SEC_DISPLAYPORT_BIGDATA)	+= displayport_bigdata.o

# from MTK Makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/			\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mminfra/			\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/			\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/			\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/			\
		-I$(DEVICE_MODULES_PATH)/drivers/iommu/				\
		-I$(DEVICE_MODULES_PATH)/drivers/devfreq/				\
		-I$(srctree)/drivers/gpu/drm/				\
		-I$(srctree)/drivers/gpu/drm/display/				\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/				\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/$(MTK_PLATFORM)/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/        \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/		\
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/        \
		-I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common/ \
		-I$(srctree)/drivers/gpu/drm/mediatek/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/v1/inc/ \
		-I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/tinysys_scmi/ \
		-I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps/ \
		-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/mml/ \
		-I$(DEVICE_MODULES_PATH)/drivers/gpu/drm/mediatek/dpc/ \
		-I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
