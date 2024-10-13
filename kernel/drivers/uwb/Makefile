# SPDX-License-Identifier: GPL-2.0
#
# Makefile for UWB devices
#
ccflags-y := -I$(srctree)/$(src)/libqmrom -I$(srctree)/$(src)/libfwupdater -I$(srctree)/$(src)/uwb_logger -DC0_WRITE_STATS

ifeq ($(CONFIG_SAMSUNG_UWB),m)
obj-$(CONFIG_SAMSUNG_UWB) := qm35.o
qm35-$(CONFIG_UWB_QM35) := \
	qm35-spi.o \
	qm35_rb.o \
	qmrom_spi.o \
	libqmrom/qmrom_common.o \
	libqmrom/qm357xx_rom_common.o \
	libqmrom/qm357xx_rom_b0.o \
	libqmrom/qm357xx_rom_c0.o \
	libqmrom/qmrom_log.o \
	libfwupdater/fwupdater.o \
	hsspi.o \
	hsspi_uci.o \
	hsspi_log.o \
	hsspi_coredump.o \
	debug.o \
	hsspi_test.o

qm35-$(CONFIG_EVENT_TRACING)	+= qm35-trace.o
qm35-$(CONFIG_SEC_UWB_LOGGER)	+= uwb_logger/uwb_logger.o

endif
