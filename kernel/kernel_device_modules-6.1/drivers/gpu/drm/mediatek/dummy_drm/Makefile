# SPDX-License-Identifier: GPL-2.0-only
$(warning build_test dummy_drm)

subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/


mediatek-dummy-drm-y := \
	mtk_vkms_composer.o \
	mtk_vkms_crtc.o \
	mtk_vkms_drv.o \
	mtk_vkms_helper.o \
	mtk_vkms_ioctls.o \
	mtk_vkms_output.o \
	mtk_vkms_plane.o \
	mtk_vkms_debug.o \
	mtk_vkms_mmp.o \
	mtk_vkms_writeback.o

obj-$(CONFIG_DRM_MEDIATEK_DUMMY) += mediatek-dummy-drm.o


obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_sync.o
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_panel_ext.o
obj-$(CONFIG_DEVICE_MODULES_DRM_MEDIATEK) += mtk_disp_notify.o


ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)	\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmp/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mmdvfs/			\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pmic/			\
             -I$(DEVICE_MODULES_PATH)/drivers/iommu/				\
             -I$(DEVICE_MODULES_PATH)/drivers/devfreq/				\
             -I$(srctree)/drivers/gpu/drm/				\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/				\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/leds/$(MTK_PLATFORM) \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/        \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/		\
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/        \
             -I$(srctree)/drivers/gpu/drm/mediatek \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq/mailbox/ \
             -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/v1/inc/ \
