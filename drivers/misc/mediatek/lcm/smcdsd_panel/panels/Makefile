obj-$(CONFIG_SMCDSD_PANEL) += ../smcdsd_panel_init.o ../smcdsd_panel.o ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o

obj-$(CONFIG_SMCDSD_DPUI) += dpui.o
obj-$(CONFIG_SMCDSD_MDNIE) += mdnie.o

obj-$(and $(CONFIG_SMCDSD_MDNIE),$(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_mdnie.o
obj-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o

obj-$(CONFIG_SMCDSD_PANEL_A32)	+= s6e3fc3_a32.o
obj-$(CONFIG_SMCDSD_PANEL_M32)	+= s6e3fc3_m32.o
obj-$(CONFIG_SMCDSD_PANEL_A22)	+= s6e3fc3_a22.o s6e3fc3_a32.o
obj-$(CONFIG_SMCDSD_PANEL_M22)	+= s6e3fc3_m22.o
obj-$(CONFIG_SMCDSD_PANEL_A31)	+= ea8076_a31.o timenval.o
obj-$(CONFIG_SMCDSD_PANEL_A41)	+= ea8076_a31.o timenval.o
obj-$(CONFIG_SMCDSD_PANEL_A41_JPN)	+= ea8076_a31.o timenval.o
obj-$(CONFIG_SMCDSD_PANEL_A01CORE)	+= a01core_td4150.o a01core_nt36525b.o
obj-$(CONFIG_SMCDSD_PANEL_A03CORE)	+= a03core_nt36525b_ctc.o a03core_nt36525b_boe.o a03core_nt36525b_sharp.o a03core_ili9882q_mantix.o
obj-$(CONFIG_SMCDSD_PANEL_A02)	+= a02_nt36525b_ctc.o a02_ili7806s_tianma.o a02_nt36675_csot.o a02_nt36525b_boe.o a02_td4150.o a02_ili9882n_boe.o a02_ili9882q_boe.o

