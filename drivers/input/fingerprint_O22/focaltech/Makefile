# Makefile for the focaltech fingerprint driver.

obj-$(CONFIG_FINGERPRINT_FOCALTECH) := focaltech_fp.o
focaltech_fp-objs  := ff_core.o

ifeq ($(CONFIG_FINGERPRINT_FOCALTECH_SPI_SUPPORT),y)
focaltech_fp-objs  += ff_spi.o
endif