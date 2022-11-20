obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_panel_init.o ../smcdsd_panel.o ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o

obj-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o

obj-$(CONFIG_SMCDSD_PANEL_A22EX)	+= a22ex_hx83102d_boe.o
obj-$(CONFIG_SMCDSD_PANEL_A13X)	+= a13x_nt36525c_boe.o a13x_nt36525b_sharp.o a13x_hx83108a_boe.o a13x_td4160_boe.o

