#
# Makefile for PN544 HCI based NFC driver
#
#obj-$(CONFIG_NFC_PN557)            += nq-nci.o
#obj-y        +=nq-nci.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
obj-y += nfc.o pn8xt.o
#ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
ccflags-y := -DDEBUG
ccflags-$(CONFIG_NXP_NFC_SN1XX)  := -DNFC_PLATFORM=sn1xx
#ccflags-$(CONFIG_NXP_NFC_PN8XT)  := -DNFC_PLATFORM=pn8xt
ccflags-y  := -DNFC_PLATFORM=pn8xt

