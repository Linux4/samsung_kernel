obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_panel_init.o ../smcdsd_panel.o

obj-$(CONFIG_SMCDSD_PANEL) += ../../../../../gpu/drm/panel/smcdsd_panel/smcdsd_regmap.o ../../../../../gpu/drm/panel/smcdsd_panel/smcdsd_supply.o

obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o

obj-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o

obj-$(CONFIG_SMCDSD_PANEL_A12)	+= ili9882n_a12.o nt36525b_a12.o nt36525b_dtc_a12.o nt36525b_boe_a12.o nt36675_a12.o

obj-$(CONFIG_SMCDSD_PANEL_A12U)	+= ili9882n_a12.o nt36525b_a12.o nt36525b_dtc_a12.o nt36525b_boe_a12.o nt36675_a12.o

obj-$(CONFIG_SMCDSD_PANEL_A13VE)	+= a13ve_nt36672c_csot.o a13ve_hx83112f_boe.o