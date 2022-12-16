#
# Makefile for MT6853 platform
#

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -DTUI_ENABLE_DISPLAY
ccflags-y += -Idrivers/misc/tui
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-ssmr

obj-y	+= stui_hal_display.o
obj-y	+= stui_hal_touch.o
