# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Unisoc WCN sdio configuration
#

obj-$(CONFIG_SDIOHAL) += sdiohal_main.o
obj-$(CONFIG_SDIOHAL) += sdiohal_common.o
obj-$(CONFIG_SDIOHAL) += sdiohal_tx.o
obj-$(CONFIG_SDIOHAL) += sdiohal_rx.o
obj-$(CONFIG_SDIOHAL) += sdiohal_if.o

ifdef CONFIG_DEBUG_FS
obj-$(CONFIG_SDIOHAL) += sdiohal_ctl.o
endif