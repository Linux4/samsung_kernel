obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_panel_init.o ../smcdsd_panel.o ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o ../smcdsd_dsi_msg.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o
obj-$(CONFIG_SMCDSD_MDNIE) += mdnie.o

obj-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_ddp.o dd_param.o

obj-$(CONFIG_SMCDSD_PANEL_M53X)	+= m53x_sw83109_boe.o
obj-$(CONFIG_SMCDSD_PANEL_A32X)	+= a32x_ili9882n_mantix.o a32x_nt36525b_sharp.o
obj-$(CONFIG_SMCDSD_PANEL_A32XU)	+= a32x_ili9882n_mantix.o a32x_nt36675_csot.o a32x_ili7806s_tianma.o
obj-$(CONFIG_SMCDSD_PANEL_A22EX)	+= a22ex_hx83102d_boe.o
obj-$(CONFIG_SMCDSD_PANEL_A13X)	+= a13x_td4160_boe.o a13x_nt36525c_sharp.o
obj-$(CONFIG_SMCDSD_PANEL_A13XU)	+= a13x_nt36525c_boe.o a13x_nt36525b_sharp.o a13x_hx83108a_boe.o a13x_td4160_boe.o


