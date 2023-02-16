#
# Makefile for exynos9820 platform
#

ccflags-$(CONFIG_TEEGRIS_TUI)	+= -DTUI_ENABLE_DISPLAY
ccflags-y += -Idrivers/misc/mediatek/tui
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-ssmr

obj-y	+= stui_hal_display.o
obj-y	+= stui_hal_touch.o
