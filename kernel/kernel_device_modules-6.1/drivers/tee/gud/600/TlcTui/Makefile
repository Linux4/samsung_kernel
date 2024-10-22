# Copyright (c) 2013-2021 TRUSTONIC LIMITED
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

#
# Makefile for the Kinibi trusted UI driver
#

#GUD_ROOT_FOLDER := drivers/gud/

# add our modules to kernel.
obj-$(CONFIG_TRUSTONIC_TRUSTED_UI) += t-base-tui.o

t-base-tui-y := main.o tlcTui.o trustedui.o tui-hal_mt.o

# Release mode by default
ccflags-y += -DNDEBUG
ccflags-y += -Wno-declaration-after-statement

ccflags-$(CONFIG_TRUSTONIC_TEE_DEBUG) += -DDEBUG

# MobiCore Driver includes
ccflags-y += -I$(GUD_ROOT_FOLDER)/MobiCoreDriver/public

# MobiCore TlcTui required includes
ccflags-y += -I$(GUD_ROOT_FOLDER)/TlcTui/inc \
             -I$(GUD_ROOT_FOLDER)/TlcTui/public

ccflags-y += -DTUI_ENABLE_DISPLAY
ccflags-y += -DTUI_ENABLE_TOUCH

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/dma-buf/heaps/