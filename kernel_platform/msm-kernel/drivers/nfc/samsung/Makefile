#
# Makefile for Samsung NRNx NFC driver
#

obj-$(CONFIG_SEC_NFC)		+= slsi_nfc.o
slsi_nfc-y		+= sec_nfc.o
slsi_nfc-$(CONFIG_ESE_P3_LSI)		+= ese_p3.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
