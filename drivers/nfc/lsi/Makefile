# SPDX-License-Identifier: GPL-2.0
#
# Makefile for nfc devices
#

ifeq ($(CONFIG_SAMSUNG_NFC),m)
obj-$(CONFIG_SAMSUNG_NFC)		+= nfc_slsi_sec.o
nfc_slsi_sec-$(CONFIG_SEC_NFC)		+= sec_nfc.o
nfc_slsi_sec-$(CONFIG_SEC_MX_NFC)	+= sec_nfc.o
nfc_slsi_sec-$(CONFIG_ESE_P3_LSI)	+= ese_p3.o
nfc_slsi_sec-$(CONFIG_SEC_NFC_LOGGER)	+= ../nfc_logger/nfc_logger.o
else
obj-$(CONFIG_SEC_NFC)			+= sec_nfc.o
obj-$(CONFIG_SEC_MX_NFC)		+= sec_nfc.o
obj-$(CONFIG_ESE_P3_LSI)		+= ese_p3.o
obj-$(CONFIG_SEC_NFC_LOGGER)		+= ../nfc_logger/nfc_logger.o
endif
ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG

