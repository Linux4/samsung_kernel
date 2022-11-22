#
# Makefile for nfc devices
#

obj-$(CONFIG_PN544_NFC)		+= pn544.o
obj-$(CONFIG_NFC_PN533)		+= pn533.o
obj-$(CONFIG_PN547_NFC)		+= pn547.o
obj-$(CONFIG_SEC_NFC_I2C)	+= sec_nfc.o
obj-$(CONFIG_SEC_NFC_COMMON)	+= sec_nfc_com.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
