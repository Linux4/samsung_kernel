#
# Makefile for nanohub
#

obj-$(CONFIG_NANOHUB) += nanohub.o
nanohub-y := chub.o chub_bootup.o chub_dbg.o chub_log.o chub_exynos.o
nanohub-y += chub_ipc_if.o ipc_chub.o ipc_chub_ap.o ipc_common.o ipc_hw.o
nanohub-$(CONFIG_SENSOR_DRV) += main.o comms.o

#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.

nanohub-$(CONFIG_SENSORS_SSP) += ssp_dev.o ssp_i2c.o ssp_data.o ssp_sysfs.o\
						ssp_firmware.o ssp_debug.o ssp_dump.o\
						factory/sensors.o\
						factory/accelerometer.o factory/gyroscope.o\
						factory/magnetometer.o factory/barometer.o\
						factory/light.o factory/proximity.o factory/magnetometer.o \
						ssp_iio.o ssp_iio_kfifo.o \
						factory/accel_icm42632m.o factory/gyro_icm42632m.o \
						factory/accel_lsm6dso.o factory/gyro_lsm6dso.o \
						factory/light_stk33910.o factory/prox_stk33910.o \
						factory/light_stk33911.o factory/prox_stk33911.o \
						factory/light_stk33915.o factory/prox_stk33915.o \
						factory/light_tmd4912.o factory/prox_tmd4912.o \
						factory/light_tmd4907.o factory/prox_tmd4907.o \
						factory/thermistor.o factory/flip_cover_detector.o \
						factory/magnetic_ak09918c.o factory/baro_lps22hhtr.o factory/baro_lps25hhtr.o \
						ssp_sensorhub.o ssp_comm.o factory/mcu_chub.o \
						ssp_platform.o

obj-m += sensors_core.o
