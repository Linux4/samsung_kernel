#
# Makefile for the Imagis touch driver
#

# Each configuration option enables a list of files.
ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
	obj-$(CONFIG_TOUCHSCREEN_IST4050) += ist40xx.o ist40xx_misc.o ist40xx_sys.o ist40xx_update.o ist40xx_sec.o ist40xx_cmcs.o
else
	obj-$(CONFIG_TOUCHSCREEN_IST4050) += ist40xx.o ist40xx_misc.o ist40xx_sys.o ist40xx_update.o ist40xx_sec.o ist40xx_cmcs.o ist40xx_vendor.o
endif
