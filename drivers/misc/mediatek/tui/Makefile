#
# Makefile for the stui Module
#

obj-$(CONFIG_TEEGRIS_TUI)	+= main.o
obj-$(CONFIG_TEEGRIS_TUI)	+= stui_core.o
obj-$(CONFIG_TEEGRIS_TUI)	+= stui_inf.o

ccflags-$(CONFIG_TEEGRIS_TUI)	+= -DUSE_TEE_CLIENT_API
ccflags-$(CONFIG_TEEGRIS_TUI)	+= -Idrivers/misc/tzdev/$(CONFIG_TEEGRIS_VERSION_STRING)/include/tzdev
#ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/video/fbdev/...
obj-$(CONFIG_TEEGRIS_TUI)	+= platforms/$(CONFIG_MTK_PLATFORM)/

