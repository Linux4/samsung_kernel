# Makefile for the Himax touchscreen drivers.

ifeq ($(CONFIG_EXYNOS_DECON_LCD_HX83112F_BOE_A13), y)
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INCELL_CORE)   	+=	himax_ic_incell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_ONCELL_CORE)   	+=	himax_ic_oncell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xH)   	+=	himax_ic_HX852xH.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX852xG)   	+=	himax_ic_HX852xG.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)   	+=	himax_ic_HX83102.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83108)   	+=	himax_ic_HX83108.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191)   	+=	himax_ic_HX83191.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)   	+=	himax_ic_HX83112.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83111)   	+=	himax_ic_HX83111.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83103)   	+=	himax_ic_HX83103.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121)   	+=	himax_ic_HX83121.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_COMMON)   		+=	himax_common.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_I2C) 			+= himax_platform.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_SPI) 			+= himax_platform_SPI.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)   		+=	himax_debug.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)   		+=	himax_inspection.o
endif

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui
