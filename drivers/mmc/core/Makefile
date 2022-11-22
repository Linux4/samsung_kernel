# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the kernel mmc core.
#

obj-$(CONFIG_MMC)		+= mmc_core.o
mmc_core-y			:= core.o bus.o host.o \
				   mmc.o mmc_ops.o sd.o sd_ops.o \
				   sdio.o sdio_ops.o sdio_bus.o \
				   sdio_cis.o sdio_io.o sdio_irq.o \
				   slot-gpio.o
mmc_core-$(CONFIG_OF)		+= pwrseq.o
obj-$(CONFIG_PWRSEQ_SIMPLE)	+= pwrseq_simple.o
obj-$(CONFIG_PWRSEQ_SD8787)	+= pwrseq_sd8787.o
obj-$(CONFIG_PWRSEQ_EMMC)	+= pwrseq_emmc.o
mmc_core-$(CONFIG_DEBUG_FS)	+= debugfs.o
obj-$(CONFIG_MMC_BLOCK)		+= mmc_block.o
mmc_block-objs			:= block.o queue.o
obj-$(CONFIG_MMC_TEST)		+= mmc_test.o
obj-$(CONFIG_SDIO_UART)		+= sdio_uart.o
mmc_core-$(CONFIG_MMC_CRYPTO)	+= crypto.o

ifeq ($(CONFIG_KUNIT),y)
GCOV_PROFILE_core.o := y
GCOV_PROFILE_bus.o := y
GCOV_PROFILE_host.o := y
GCOV_PROFILE_mmc.o := y
GCOV_PROFILE_sd.o := y
GCOV_PROFILE_mmc_ops.o := y
GCOV_PROFILE_sd_ops.o := y
GCOV_PROFILE_slot-gpio.o := y
GCOV_PROFILE_block.o := y
GCOV_PROFILE_queue.o := y
endif
