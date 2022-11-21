obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_panel_init.o ../smcdsd_panel.o ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o

obj-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o

obj-$(CONFIG_SMCDSD_PANEL_A32X)	+= ili9882n_a32x.o nt36525b_a32x.o
obj-$(CONFIG_SMCDSD_PANEL_SHOOTING)	+= td4150_shooting.o
obj-$(CONFIG_SMCDSD_PANEL_A32XU)	+= ili9882n_a32x.o nt36675_a32x.o ili7806s_a32x.o

