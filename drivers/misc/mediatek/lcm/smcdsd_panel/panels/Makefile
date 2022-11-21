obj-y += ../smcdsd_notify.o ../smcdsd_panel.o ../smcdsd_abd.o ../smcdsd_board.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o
obj-$(CONFIG_SMCDSD_MDNIE) += mdnie.o

obj-$(and $(CONFIG_SMCDSD_MDNIE),$(CONFIG_SMCDSD_ENG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_mdnie.o
obj-$(and $(CONFIG_SMCDSD_ENG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o

obj-$(CONFIG_SMCDSD_EA8076_A31)	+= ea8076_a31_lcd_ctrl.o timenval.o
obj-$(CONFIG_SMCDSD_EA8076_A41)	+= ea8076_a31_lcd_ctrl.o timenval.o
obj-$(CONFIG_SMCDSD_TD4150_A01CORE)	+= td4150_a01core_lcd_ctrl.o
obj-$(CONFIG_SMCDSD_NT36525B_A01CORE)	+= nt36525b_a01core_lcd_ctrl.o

