#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_DPU30) += mcd-panel.o

mcd-panel-y += mcd_panel_register.o
mcd-panel-$(CONFIG_EXYNOS_MIPI_DSIM) += timenval.o panel.o panel_bl.o dimming.o panel_drv.o panel_irc.o decon_board.o decon_notify.o decon_void_regulator.o decon_assist.o
mcd-panel-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o dd_assist.o
mcd-panel-$(CONFIG_EXYNOS_DECON_MDNIE) += mdnie.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_SYSFS) += sysfs.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_SPI) += spi.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_COPR) += copr.o
mcd-panel-$(CONFIG_DISPLAY_USE_INFO) += dpui.o
mcd-panel-$(CONFIG_PANEL_NOTIFY) += panel_notify.o
mcd-panel-$(CONFIG_SUPPORT_DDI_FLASH) += panel_poc.o
mcd-panel-$(CONFIG_EXTEND_LIVE_CLOCK) += aod/aod_drv.o
mcd-panel-$(CONFIG_SUPPORT_POC_SPI) += panel_spi.o
mcd-panel-$(CONFIG_SUPPORT_I2C) += panel_i2c.o
mcd-panel-$(CONFIG_DYNAMIC_FREQ) += df/dynamic_freq.o


mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_DUMMY_LCD_V320) += dummy_lcd/dummy_lcd.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_NT36672C_TIANMA_A14) += nt36672c_tianma_a14/nt36672c_tianma.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_ILI7807_BOE_A14) += ili7807_boe_a14/ili7807_boe.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_NT36672C_CSOT_A14) += nt36672c_csot_a14/nt36672c_csot.o
mcd-panel-$(CONFIG_EXYNOS_DECON_LCD_FT8720_SKYWORTH_A14) += ft8720_skyworth_a14/ft8720_skyworth.o
