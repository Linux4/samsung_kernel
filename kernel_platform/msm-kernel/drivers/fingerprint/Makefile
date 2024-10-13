#
# Makefile for the Fingerprint sensor driver.
#

# Each configuration option enables a list of files.

ccflags-y := -Wformat

obj-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint.o fingerprint_sysfs.o

fingerprint-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint_common.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_QCOM)	+= fingerprint_common_qcom.o
fingerprint-$(CONFIG_SENSORS_GW9558X)	+= gw9558x_common.o gw9558x_spidev.o
