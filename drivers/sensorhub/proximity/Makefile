#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.

obj-y += proximity_sensor.o
obj-$(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725) += prox_autocal_ams.o
obj-$(CONFIG_SENSORS_SSP_PROXIMITY_STK3X3X) += prox_stk3x3x.o
obj-$(CONFIG_SENSORS_SSP_PROXIMITY_STK3X6X) += prox_stk3x6x.o
obj-$(CONFIG_SENSORS_SSP_PROXIMITY_GP2AP110S) += prox_gp2ap110s.o
GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/
