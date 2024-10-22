#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
MFD_TOP = $(DEVICE_MODULES_PATH)/drivers/mfd
ccflags-y += -I$(MFD_TOP)/

obj-$(CONFIG_MTK_CLKBUF_CTRL) += clkbuf.o
clkbuf-y += src/clkbuf-ctrl.o
clkbuf-y += src/clkbuf-pmic.o
clkbuf-y += src/srclken.o
clkbuf-y += src/clkbuf-pmif.o
clkbuf-y += src/platform-hdlr.o
clkbuf-y += src/clkbuf-debug.o
#platform data files
clkbuf-y += src/dcxo-6685.o
clkbuf-y += src/dcxo-6685-tb.o
clkbuf-y += src/pmif-data-v1.o
clkbuf-y += src/src-data-v1.o
