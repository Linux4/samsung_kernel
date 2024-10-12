#
# Makefile for the stui Module
#

ccflags-y += -I$(srctree)/drivers/misc/tui/
ccflags-y += -I$(srctree)/drivers/misc/tzdev/include/tzdev
ccflags-y += -DUSE_TEE_CLIENT_API

obj-$(CONFIG_SAMSUNG_TUI)	+= main.o
obj-$(CONFIG_SAMSUNG_TUI)	+= stui_core.o
obj-$(CONFIG_SAMSUNG_TUI)	+= stui_inf.o


ifeq ($(CONFIG_MACH_MT6877), y)
obj-$(CONFIG_SAMSUNG_TUI)	+= platforms/mt6877/
else
obj-$(CONFIG_SAMSUNG_TUI)	+= platforms/$(CONFIG_MTK_PLATFORM)/
endif

ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
obj-$(CONFIG_SAMSUNG_TUI) += iwd_agent.o

ccflags-y += -I$(srctree)/drivers/misc
ccflags-y += -I$(srctree)/drivers/misc/tzdev/
ccflags-y += -DCONFIG_SAMSUNG_TUI_LOWLEVEL
endif
