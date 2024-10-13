#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.
#include $(srctree)/drivers/misc/mediatek/Makefile.custom

#obj-y  := et300.o

obj-$(CONFIG_FINGERPRINT_ET300) := \
	et300-spi.o \
	et300-spi_data_transfer.o \
	sprd_fps_trigger.o
