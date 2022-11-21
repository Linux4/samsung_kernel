#
# Makefile for the Fingerprint sensor driver.
#

# Each configuration option enables a list of files.

ccflags-y := -Wformat

ifeq ($(CONFIG_SENSORS_FINGERPRINT),m)
ifeq ($(CONFIG_SENSORS_FINGERPRINT_LSI),y)
ifeq ($(CONFIG_SENSORS_QBT2000),m)
obj-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint.o fingerprint_sysfs.o
obj-$(CONFIG_SENSORS_QBT2000)			+= qbt2000_spidev.o
fingerprint-$(CONFIG_SENSORS_QBT2000)	+= fingerprint_common.o fingerprint_common_lsi.o qbt2000_common.o qbt2000_platform.o
endif
endif
ifeq ($(CONFIG_SENSORS_FINGERPRINT_QCOM),y)
ifeq ($(CONFIG_SENSORS_QBT2000),m)
obj-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint.o fingerprint_sysfs.o
obj-$(CONFIG_SENSORS_QBT2000)			+= qbt2000_spidev.o
fingerprint-$(CONFIG_SENSORS_QBT2000)	+= fingerprint_common.o fingerprint_common_qcom.o qbt2000_common.o qbt2000_platform.o
endif
endif
ifeq ($(CONFIG_SENSORS_FINGERPRINT_MTK),y)
ifeq ($(CONFIG_SENSORS_QBT2000),m)
obj-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint.o fingerprint_sysfs.o
obj-$(CONFIG_SENSORS_QBT2000)			+= qbt2000_spidev.o
fingerprint-$(CONFIG_SENSORS_QBT2000)	+= fingerprint_common.o fingerprint_common_mtk.o qbt2000_common.o qbt2000_platform.o
endif
endif
endif
ifeq ($(CONFIG_SENSORS_FINGERPRINT),y)

obj-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint.o fingerprint_sysfs.o

fingerprint-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint_common.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_LSI)	+= fingerprint_common_lsi.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_QCOM)	+= fingerprint_common_qcom.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_MTK)	+= fingerprint_common_mtk.o

fingerprint-$(CONFIG_SENSORS_QBT2000)	+= qbt2000_common.o qbt2000_platform.o
obj-$(CONFIG_SENSORS_QBT2000)			+= qbt2000_spidev.o
fingerprint-$(CONFIG_SENSORS_ET5XX)		+= et5xx-spi.o et5xx-spi_data_transfer.o et5xx-spi_platform.o
fingerprint-$(CONFIG_SENSORS_ET7XX)		+= et7xx-spi.o et7xx-spi_data_transfer.o et7xx-spi_platform.o
fingerprint-$(CONFIG_SENSORS_GW3X)		+= gw3x_common.o gw3x_platform.o gw3x_spidev.o
fingerprint-$(CONFIG_SENSORS_GW9558X)	+= gw9558x_common.o gw9558x_platform.o gw9558x_spidev.o

endif
