#
# Makefile for Samsung NRNx NFC driver
#

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/base/power/include \

obj-$(CONFIG_SEC_NFC)		+= sec_nfc.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
