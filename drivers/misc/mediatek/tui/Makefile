#
# Makefile for the stui Module
#
#EXTRA_CFLAGS += -Idrivers/misc/tui
ccflags-y += -Idrivers/misc/tui
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk

obj-$(CONFIG_BLOWFISH_TUI_SUPPORT)	+= main.o
obj-$(CONFIG_BLOWFISH_TUI_SUPPORT)	+= stui_core.o
obj-$(CONFIG_BLOWFISH_TUI_SUPPORT)	+= stui_inf.o
obj-$(CONFIG_BLOWFISH_TUI_SUPPORT)	+= ./platforms/vendor/stui_hal_display.o
obj-$(CONFIG_BLOWFISH_TUI_SUPPORT)	+= ./platforms/vendor/stui_hal_touch.o

ccflags-y += -DTUI_ENABLE_DISPLAY
ccflags-y += -DTUI_ENABLE_TOUCH
ccflags-y += -DTUI_ENABLE_I2C
ccflags-y += -DTUI_ENABLE_MEMORY_SSVP