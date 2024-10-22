# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2018 MediaTek Inc.
#
# export MTK_IMGSYS_VER := $(subst ",,$(CONFIG_MTK_IMGSYS_VER))
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-isp/mtk-hcp
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-isp/imgsys/platforms/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-isp/imgsys/platforms/isp71
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-isp/imgsys
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-isp/mtk-ipesys-me/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/soc/mediatek

ccflags-y += -ferror-limit=0

ifneq ($(wildcard $(DEVICE_MODULES_PATH)/include/dt-bindings/memory/mt6983-larb-port.h),)
  subdir-ccflags-y += -DWPE_TF_DUMP_71_1
else ifneq ($(wildcard $(DEVICE_MODULES_PATH)/include/dt-bindings/memory/mt6879-larb-port.h),)
  subdir-ccflags-y += -DWPE_TF_DUMP_71_2
  subdir-ccflags-y += -DONEPIXEL_MODE
endif
$(info "imgsys, subdir-ccflags-y = $(subdir-ccflags-y)")

ifneq ($(wildcard $(DEVICE_MODULES_PATH)/include/dt-bindings/memory/mt6985-larb-port.h),)
  subdir-ccflags-y += -DWPE_TF_DUMP_7S_1
  subdir-ccflags-y += -DPQDIP_7S_1
else ifneq ($(wildcard $(DEVICE_MODULES_PATH)/include/dt-bindings/memory/mt6886-larb-port.h),)
  subdir-ccflags-y += -DWPE_TF_DUMP_7S_2
endif
$(info "imgsys, subdir-ccflags-y = $(subdir-ccflags-y)")

# Utilities to provide frame-based streaming model
# with v4l2 user interfaces and alloc context managing
# memory shared between ISP and coprocessor
mtk_imgsys_v4l2-objs := \
mtk_imgsys-v4l2.o \
mtk_imgsys-dev.o \
mtk_imgsys-of.o \
mtk_imgsys-sys.o \
mtk_imgsys-worker.o \
mtk_imgsys-trace.o \
platforms/isp71/mtk_imgsys-debug.o \
platforms/isp71/modules/mtk_imgsys-dip.o \
platforms/isp71/modules/mtk_imgsys-traw.o \
platforms/isp71/modules/mtk_imgsys-pqdip.o \
platforms/isp71/modules/mtk_imgsys-wpe.o \
platforms/isp71/modules/mtk_imgsys-adl.o

#mtk_imgsys_v4l2-objs += mtk_imgsys-cmdq.o

#mtk_imgsys_v4l2-objs := mtk_imgsys_hw_isp71.o mtk_imgsys_util.o

obj-${CONFIG_VIDEO_MTK_ISP_IMGSYS} += mtk_imgsys_v4l2.o

mtk_imgsys_isp7s-objs := \
mtk_imgsys-isp7s.o \
platforms/isp7s/mtk_imgsys-debug.o \
platforms/isp7s/modules/mtk_imgsys-dip.o \
platforms/isp7s/modules/mtk_imgsys-traw.o \
platforms/isp7s/modules/mtk_imgsys-pqdip.o \
platforms/isp7s/modules/mtk_imgsys-wpe.o \
platforms/isp7s/modules/mtk_imgsys-me.o \
platforms/isp7s/modules/mtk_imgsys-adl.o

obj-${CONFIG_VIDEO_MTK_ISP_IMGSYS_7S} += mtk_imgsys_isp7s.o

mtk_imgsys_isp7sp-objs := \
mtk_imgsys-isp7sp.o \
platforms/isp7sp/mtk_imgsys-debug.o \
platforms/isp7sp/modules/mtk_imgsys-dip.o \
platforms/isp7sp/modules/mtk_imgsys-traw.o \
platforms/isp7sp/modules/mtk_imgsys-pqdip.o \
platforms/isp7sp/modules/mtk_imgsys-wpe.o \
platforms/isp7sp/modules/mtk_imgsys-me.o \
platforms/isp7sp/modules/mtk_imgsys-adl.o

obj-${CONFIG_VIDEO_MTK_ISP_IMGSYS_7SP} += mtk_imgsys_isp7sp.o
#
#  imgsys-cmdq
#

#ifeq ($(CONFIG_VIDEO_MTK_ISP_IMGSYS_7SP),m)
#  subdir-ccflags-y += -DMTK_IOVA_SINK2KERNEL
#ifeq ($(CONFIG_VIDEO_MTK_ISP_IMGSYS_7S),m)
subdir-ccflags-y += -DMTK_IOVA_SINK2KERNEL
#endif
$(info "imgsys cmdq, subdir-ccflags-y = $(subdir-ccflags-y)")

mtk_imgsys_cmdq-objs := \
mtk_imgsys-cmdq.o \
mtk_imgsys-trace.o \
platforms/isp71/mtk_imgsys-cmdq-plat.o \
platforms/isp7s/mtk_imgsys-cmdq-plat.o \
platforms/isp7sp/mtk_imgsys-cmdq-plat.o

obj-$(CONFIG_VIDEO_MTK_ISP_IMGSYS_CMDQ) += mtk_imgsys_cmdq.o
