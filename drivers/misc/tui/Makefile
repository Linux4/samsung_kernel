#
# Makefile for the stui Module
#

ccflags-y += -Idrivers/video/fbdev/exynos/dpu30
ccflags-$(CONFIG_SAMSUNG_TUI) += -I$(srctree)/drivers/misc/tui/
ccflags-$(CONFIG_SAMSUNG_TUI) += -I$(srctree)/drivers/misc/tui/platforms/exynos2100

tuihw-inf-objs := stui_inf.o

tuihw-objs := main.o stui_core.o platforms/exynos2100/stui_hal_display.o platforms/exynos2100/stui_hal_touch.o
obj-$(CONFIG_SAMSUNG_TUI) += tuihw.o tuihw-inf.o
