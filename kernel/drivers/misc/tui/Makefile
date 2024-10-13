#
# Makefile for the stui Module
#

ccflags-y += -I$(srctree)/drivers/misc/tui/
ccflags-y += -I$(srctree)/drivers/misc/tzdev/include/tzdev
ccflags-y += -DUSE_TEE_CLIENT_API

tuihw-inf-objs := stui_inf.o

tuihw-objs := main.o
tuihw-objs += stui_core.o

ifeq ($(CONFIG_SOC_S5E8845),y)
ccflags-y += -DCONFIG_SOC_S5E8845
ccflags-y += -I$(srctree)/drivers/misc/tui/platforms/exynos8845
ccflags-y += -I$(srctree)/drivers/gpu/drm/samsung/dpu
tuihw-objs += platforms/exynos8845/stui_hal_display.o
tuihw-objs += platforms/exynos8845/stui_hal_touch.o
endif

ifeq ($(CONFIG_SOC_S5E9945),y)
ccflags-y += -DCONFIG_SOC_S5E9945
ccflags-y += -I$(srctree)/drivers/misc/tui/platforms/exynos9945
ccflags-y += -I$(srctree)/drivers/gpu/drm/samsung/dpu
tuihw-objs += platforms/exynos9945/stui_hal_display.o
tuihw-objs += platforms/exynos9945/stui_hal_touch.o
endif

ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
tuihw-objs += iwd_agent.o

ccflags-y += -I$(srctree)/drivers/misc
ccflags-y += -I$(srctree)/drivers/misc/tzdev/
ccflags-y += -DCONFIG_SAMSUNG_TUI_LOWLEVEL
endif

obj-$(CONFIG_SAMSUNG_TUI) += tuihw.o tuihw-inf.o
