obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_panel_init.o ../smcdsd_panel.o ../smcdsd_regmap.o ../smcdsd_supply.o ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o ../smcdsd_dsi_msg.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o
obj-$(CONFIG_SMCDSD_MDNIE) += mdnie.o

obj-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_ddp.o dd_param.o

obj-$(CONFIG_SMCDSD_PANEL_A23EX)	+= a23ex_hx83102d_boe.o a23ex_nt36525b_boe.o
obj-$(CONFIG_SMCDSD_PANEL_M53X)	+= m53x_sw83109_boe.o
obj-$(CONFIG_SMCDSD_PANEL_BIRDIE)	+= birdie_hx83102j_boe.o
obj-$(CONFIG_SMCDSD_PANEL_A34X)	+= a34x_s6e3fc5_sdc.o


