#
# Makefile for Bosch sensor driver.
#

obj-$(CONFIG_SENSORS_BMP280)      += bmp280_core.o bmp280.o
obj-$(CONFIG_SENSORS_BMP280_I2C)  += bmp280_i2c.o
obj-$(CONFIG_SENSORS_BMP280_SPI)  += bmp280_spi.o
ifeq ($(CONFIG_SENSORS_BMP280_I2C),y)
	EXTRA_CFLAGS += -DBMP_USE_BASIC_I2C_FUNC
endif

