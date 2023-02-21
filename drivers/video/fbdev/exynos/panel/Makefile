#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_PANEL_DISPLAY_MODE) += panel_modes.o
obj-$(CONFIG_EXYNOS_MIPI_DSIM) += timenval.o panel.o panel_bl.o \
	dimming.o panel_drv.o panel_irc.o panel_vrr.o
obj-$(CONFIG_PANEL_DEBUG) += panel_debug.o
obj-$(CONFIG_EXYNOS_DECON_MDNIE_LITE) += mdnie.o
obj-$(CONFIG_EXYNOS_DECON_LCD_SYSFS) += sysfs.o
obj-$(CONFIG_EXYNOS_DECON_LCD_SPI) += spi.o
obj-$(CONFIG_EXYNOS_DECON_LCD_COPR) += copr.o
obj-$(CONFIG_DISPLAY_USE_INFO) += dpui.o
obj-$(CONFIG_PANEL_NOTIFY) += panel_notify.o
obj-$(CONFIG_SUPPORT_DDI_FLASH) += panel_poc.o
obj-$(CONFIG_EXTEND_LIVE_CLOCK) += aod/aod_drv.o
obj-$(CONFIG_SUPPORT_POC_SPI) += panel_spi.o
obj-$(CONFIG_DYNAMIC_FREQ) += df/dynamic_freq.o
obj-$(CONFIG_SUPPORT_DISPLAY_PROFILER) += \
	display_profiler/display_profiler.o \
	display_profiler/maskgen.o
obj-$(CONFIG_SUPPORT_MAFPC) += mafpc/mafpc_drv.o
obj-$(CONFIG_PANEL_DECON_BOARD) += decon_board.o

#
# Panel and DDI(display driver ic) Device Drivers
#
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3XA0) += s6e3xa0/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FA7) += s6e3fa7/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FA9) += s6e3fa9/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FAB) += s6e3fab/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA8) += s6e3ha8/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA9) += s6e3ha9/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HAB) += s6e3hab/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HAC) += s6e3hac/
obj-$(CONFIG_EXYNOS_DECON_LCD_EA8076) += ea8076/
obj-$(CONFIG_EXYNOS_DECON_LCD_EA8079) += ea8079/
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FC3) += s6e3fc3/
