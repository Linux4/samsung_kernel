#
# Makefile for nfc devices
#

obj-$(CONFIG_NFC_PN544)		+= pn544/
obj-$(CONFIG_NFC_WILINK)		+= nfcwilink.o
obj-$(CONFIG_NFC_MEI_PHY)	+= mei_phy.o
obj-$(CONFIG_NFC_SIM)		+= nfcsim.o
obj-$(CONFIG_NFC_PORT100)	+= port100.o
obj-$(CONFIG_NFC_MRVL)		+= nfcmrvl/
obj-$(CONFIG_NFC_SEC)		+= nfcsec/
obj-$(CONFIG_ST21NFCA)		+= st21nfca.o
obj-$(CONFIG_SEC_NFC)	+= sec_nfc.o
obj-$(CONFIG_NFC_PN547)		+= pn547.o
obj-$(CONFIG_BCM2079X_NFC_I2C)	+= bcm2079x-i2c.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
