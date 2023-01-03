#
# Makefile for the stui Module
#

obj-$(CONFIG_SAMSUNG_TUI)	+= main.o
obj-$(CONFIG_SAMSUNG_TUI)	+= stui_core.o
obj-$(CONFIG_SAMSUNG_TUI)	+= stui_inf.o

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -DUSE_TEE_CLIENT_API
ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tzdev/include/tzdev
ccflags-y += -Idrivers/misc/tui

ifeq ($(CONFIG_MACH_MT6877), y)
obj-$(CONFIG_SAMSUNG_TUI)	+= platforms/mt6877/
else
obj-$(CONFIG_SAMSUNG_TUI)	+= platforms/$(CONFIG_MTK_PLATFORM)/
endif

