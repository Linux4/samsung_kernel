obj-$(CONFIG_DECON_MDNIE_LITE)		+= mdnie_lite.o mdnie_tuning.o
obj-$(CONFIG_DECON_MDNIE_LITE_CHAGALL)		+= mdnie_lite_chagall.o mdnie_tuning.o
obj-$(CONFIG_DECON_LCD_S6E3HA0K)	+= s6e3ha0k_mipi_lcd.o s6e3ha0k_lcd_ctrl.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_S6E3HA2K)	+= s6e3ha2k_mipi_lcd.o s6e3ha2k_lcd_ctrl.o
obj-$(CONFIG_DECON_LCD_S6E3FA0)		+= s6e3fa0_mipi_lcd.o s6e3fa0_lcd_ctrl.o
obj-$(CONFIG_DECON_LCD_S6E3FA2)		+= s6e3fa2_mipi_lcd.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_S6E3HA0)		+= s6e3ha0_mipi_lcd.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_EA8064G)		+= ea8064g_mipi_lcd.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_S6E3HA2)		+= s6e3ha2_mipi_lcd.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_S6E3HF2)		+= s6e3hf2_mipi_lcd.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_S6TNMR7)		+= s6tnmr7_mipi_lcd.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_ANA38401)	+= s6tnmr7_tabs2_mipi_lcd.o ana38401_mipi_lcd.o dynamic_aid.o
obj-$(CONFIG_DECON_LCD_S6E3HA2_TABS2)	+= s6e3ha1_mipi_lcd.o s6e3ha2_tabs2_mipi_lcd.o dynamic_aid.o

ifeq ($(CONFIG_EXYNOS_DECON_DUAL_DISPLAY),y)
obj-$(CONFIG_DECON_MDNIE_LITE)		+= mdnie_lite_ext.o
obj-$(CONFIG_DECON_LCD_S6E3FA2)	+= s6e3fa2_mipi_lcd_ext.o
endif
