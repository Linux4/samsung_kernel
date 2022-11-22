#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_MIPI_DSIM) += timenval.o panel.o panel_bl.o panel_drv.o decon_board.o
obj-$(CONFIG_PANEL_AID_DIMMING) += dimming.o panel_irc.o
obj-$(CONFIG_EXYNOS_DECON_MDNIE) += mdnie.o
obj-$(CONFIG_EXYNOS_DECON_LCD_SYSFS) += sysfs.o
obj-$(CONFIG_EXYNOS_DECON_LCD_SPI) += spi.o
obj-$(CONFIG_EXYNOS_DECON_LCD_COPR) += copr.o
obj-$(CONFIG_DISPLAY_USE_INFO) += dpui.o
obj-$(CONFIG_PANEL_NOTIFY) += panel_notify.o
obj-$(CONFIG_SUPPORT_DDI_FLASH) += panel_poc.o
obj-$(CONFIG_EXTEND_LIVE_CLOCK) += aod/aod_drv.o
obj-$(CONFIG_SUPPORT_POC_SPI) += panel_spi.o
obj-$(CONFIG_SUPPORT_I2C) += panel_i2c.o
obj-$(CONFIG_DYNAMIC_FREQ) += df/dynamic_freq.o

//obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA9) += s6e3ha9/s6e3ha9.o s6e3ha9/s6e3ha9_aod.o
obj-$(CONFIG_EXYNOS_DECON_LCD_ANA6705_A71X) += ana6705/ana6705.o ana6705/ana6705_aod.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FA9_A71X) += s6e3fa9/s6e3fa9.o s6e3fa9/s6e3fa9_aod.o
obj-$(CONFIG_EXYNOS_DECON_LCD_EA8076_A51X) += ea8076/ea8076.o ea8076/ea8076_aod.o

ifeq ($(CONFIG_SUPPORT_DISPLAY_PROFILER),y)
obj-y += display_profiler/display_profiler.o display_profiler/maskgen.o

ifeq ($(CONFIG_EXYNOS_DECON_LCD_S6E3FA9_A71X),y)
obj-y += s6e3fa9/s6e3fa9_profiler.o
endif
endif

