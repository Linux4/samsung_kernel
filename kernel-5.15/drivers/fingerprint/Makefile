#
# Makefile for the Fingerprint sensor driver.
#

# Each configuration option enables a list of files.

ccflags-y := -Wformat

obj-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint.o fingerprint_sysfs.o

fingerprint-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint_common.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_MTK)	+= fingerprint_common_mtk.o

fingerprint-$(CONFIG_SENSORS_GW3X)		+= gw3x/gw3x_common.o gw3x/gw3x_spidev.o
