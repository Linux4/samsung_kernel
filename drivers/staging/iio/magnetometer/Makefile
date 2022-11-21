#
# Makefile for industrial I/O Magnetometer sensors
#

obj-$(CONFIG_SENSORS_AK8975)	+= ak8975.o
obj-$(CONFIG_SENSORS_HMC5843)	+= hmc5843.o
obj-$(CONFIG_INV_MMC328X_IIO) += inv-mmc328x-iio.o

inv-mmc328x-iio-objs := inv_mmc328x_core.o
inv-mmc328x-iio-objs += inv_mmc328x_ring.o
inv-mmc328x-iio-objs += inv_mmc328x_trigger.o

CFLAGS_inv_mmc328x_core.o    += -Idrivers/staging/iio
CFLAGS_inv_mmc328x_ring.o    += -Idrivers/staging/iio
CFLAGS_inv_mmc328x_trigger.o += -Idrivers/staging/iio
