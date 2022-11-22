obj-$(CONFIG_EXYNOS_DECON_LCD) += dsim_panel.o
obj-$(CONFIG_EXYNOS_DECON_MDNIE_LITE) += mdnie_lite.o
obj-$(CONFIG_DISPLAY_USE_INFO) += dpui.o

obj-$(and $(CONFIG_EXYNOS_DECON_MDNIE_LITE),$(CONFIG_SEC_GPIO_DVS),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_mdnie.o
obj-$(and $(CONFIG_SEC_GPIO_DVS),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_dpu.o dd_param.o

obj-$(CONFIG_PANEL_S6E8AA5X01)	+= s6e8aa5x01_lcd_ctrl.o dimming_core.o
obj-$(CONFIG_PANEL_TD4100)	+= td4100_lcd_ctrl.o
obj-$(CONFIG_PANEL_S6D7AA0_XCOVER4)	+= s6d7aa0_xcover4_lcd_ctrl.o
obj-$(CONFIG_PANEL_TD4100_J3POP)	+= td4100_j3pop_lcd_ctrl.o
obj-$(CONFIG_PANEL_TD4100_J3Y17)	+= td4100_j3y17_lcd_ctrl.o
obj-$(CONFIG_PANEL_S6D7AA0_GTESVE)	+= s6d7aa0_gtesve_lcd_ctrl.o
obj-$(CONFIG_PANEL_TD4100_J3TOP)	+= td4100_j3top_lcd_ctrl.o
obj-$(CONFIG_PANEL_EA8061S_J4LTE)	+= ea8061s_j4lte_lcd_ctrl.o dynamic_aid.o
obj-$(CONFIG_PANEL_TD4101_J2CORELTE)	+= td4101_j2corelte_lcd_ctrl.o
