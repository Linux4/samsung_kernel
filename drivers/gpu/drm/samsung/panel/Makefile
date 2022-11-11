#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

subdir-ccflags-y := -I$(srctree) -I$(srctree)/$(src)

ifneq ($(filter y m,$(CONFIG_DRM_SAMSUNG_DPU)),)
subdir-ccflags-y += \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/include \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/cal_common
subdir-ccflags-$(CONFIG_UML) += \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/exynos_p1 \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/exynos_p1/cal_pm
subdir-ccflags-$(CONFIG_SOC_S5E9925) += \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/exynos_p1 \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/exynos_p1/cal_pm
subdir-ccflags-$(CONFIG_SOC_S5E8825) += \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/exynos_p2 \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/exynos_p2/cal_py
subdir-ccflags-$(CONFIG_SOC_S5E9935) += \
	-I$(srctree)/drivers/gpu/drm/samsung/dpu/cal_qd
endif

ifneq ($(filter y m,$(CONFIG_EXYNOS_DPU30)),)
subdir-ccflags-y += \
	-I$(srctree)/drivers/video/fbdev/exynos/dpu30
endif

mcd-panel-y += timenval.o \
				util.o \
				panel_lib.o \
				maptbl.o \
				panel.o \
				panel_bl.o \
				dimming.o \
				panel_gpio.o \
				panel_regulator.o \
				panel_drv_ioctl.o \
				panel_drv.o \
				panel_irc.o \
				panel_vrr.o \
				panel_power_ctrl.o \
				panel_obj.o

mcd-panel-$(CONFIG_PANEL_DISPLAY_MODE) += panel_modes.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_SYSFS) += sysfs.o
mcd-panel-$(CONFIG_EXYNOS_DECON_MDNIE_LITE) += mdnie.o
mcd-panel-$(CONFIG_PANEL_DEBUG) += panel_debug.o
mcd-panel-$(CONFIG_EXTEND_LIVE_CLOCK) += aod/aod_drv.o
mcd-panel-$(CONFIG_DYNAMIC_MIPI) += dynamic_mipi/dynamic_mipi.o
mcd-panel-$(CONFIG_SUPPORT_MAFPC) += mafpc/mafpc_drv.o
mcd-panel-$(CONFIG_PANEL_NOTIFY) += panel_notify.o
mcd-panel-$(CONFIG_DISPLAY_USE_INFO) += dpui.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_COPR) += copr.o
mcd-panel-$(CONFIG_SUPPORT_POC_SPI) += panel_poc.o panel_spi.o
mcd-panel-$(CONFIG_MCD_PANEL_I2C) += panel_i2c.o
mcd-panel-$(CONFIG_MCD_PANEL_BLIC) += panel_blic.o
obj-$(CONFIG_MCD_PANEL) += mcd-panel.o

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
    -Wframe-larger-than=8192 \
    -Wno-unused-variable \
    -Wno-unused-function \
    -Wno-missing-braces \
    -Wno-implicit-fallthrough \
    -Wno-format

