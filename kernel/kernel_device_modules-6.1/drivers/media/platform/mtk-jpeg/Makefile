# SPDX-License-Identifier: GPL-2.0-only
mtk_jpeg-objs := mtk_jpeg_core.o \
		 mtk_jpeg_dec_hw.o \
		 mtk_jpeg_dec_parse.o \
		 mtk_jpeg_enc_hw.o
obj-$(CONFIG_DEVICE_MODULES_VIDEO_MEDIATEK_JPEG) += mtk_jpeg.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-jpeg
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3