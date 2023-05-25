#
# Makefile for Invensense inv-mpu-iio/inv-mpu-iio-spi device.
#

obj-$(CONFIG_ICM_MPU_IIO) += inv-mpu-iio.o
obj-$(CONFIG_INV_MPU_IIO_SPI) += inv-mpu-iio-spi.o

inv-mpu-iio-objs := inv_mpu_i2c.o
inv-mpu-iio-objs += inv_mpu_core.o
inv-mpu-iio-objs += inv_mpu_init.o
inv-mpu-iio-objs += inv_mpu_ring.o
inv-mpu-iio-objs += inv_mpu_parsing.o
inv-mpu-iio-objs += inv_mpu_misc.o
inv-mpu-iio-objs += inv_mpu_load_dmp.o
inv-mpu-iio-objs += inv_mpu_common.o
inv-mpu-iio-objs += inv_mpu_selftest.o
inv-mpu-iio-objs += inv_mpu_setup.o

inv-mpu-iio-spi-objs := inv_mpu_spi.o
inv-mpu-iio-spi-objs += inv_mpu_core.o
inv-mpu-iio-spi-objs += inv_mpu_init.o
inv-mpu-iio-spi-objs += inv_mpu_ring.o
inv-mpu-iio-spi-objs += inv_mpu_parsing.o
inv-mpu-iio-spi-objs += inv_mpu_misc.o
inv-mpu-iio-spi-objs += inv_mpu_load_dmp.o
inv-mpu-iio-spi-objs += inv_mpu_common.o
inv-mpu-iio-spi-objs += inv_mpu_selftest.o
inv-mpu-iio-spi-objs += inv_mpu_setup.o

# compile Invensense MPU IIO driver as DTS
ifeq ($(CONFIG_DTS_INV_MPU_IIO), y)
inv-mpu-iio-objs += inv_mpu_dts.o
endif
ccflags-y := -Idrivers/staging/iio/
