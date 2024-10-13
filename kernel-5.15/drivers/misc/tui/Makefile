#
# Makefile for the stui Module
#

obj-$(CONFIG_SAMSUNG_TUI) += tuihw.o tuihw-inf.o

ccflags-y += -DUSE_TEE_CLIENT_API
ccflags-y += -DTUI_ENABLE_DISPLAY
ccflags-y += -DDEBUG

ccflags-y += -I$(srctree)/drivers/misc/tui/
ccflags-y += -I$(srctree)/drivers/misc/tzdev/include/tzdev
ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2
ccflags-y += -I$(srctree)/drivers/misc/tui/platforms/

# secure memory
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem
ccflags-y += -I$(srctree)/drivers/dma-buf/heaps/

tuihw-inf-y = stui_inf.o

tuihw-y += main.o
tuihw-y += stui_core.o
tuihw-y += platforms/stui_hal_display.o
tuihw-y += platforms/stui_hal_touch.o

ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
tuihw-y += iwd_agent.o

ccflags-y += -I$(srctree)/drivers/misc/tzdev/
ccflags-y += -DCONFIG_SAMSUNG_TUI_LOWLEVEL
endif
