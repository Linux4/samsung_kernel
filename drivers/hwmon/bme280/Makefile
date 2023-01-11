#
# Makefile for Bosch sensor driver.
#
obj-$(CONFIG_SENSORS_BME280)      += bme280_core.o bme280.o
obj-$(CONFIG_SENSORS_BME280_I2C)  += bme280_i2c.o
ifeq ($(CONFIG_SENSORS_BME280_I2C),y)
	EXTRA_CFLAGS += -DBME_USE_BASIC_I2C_FUNC
endif
