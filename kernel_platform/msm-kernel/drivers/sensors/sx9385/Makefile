#
# Makefile for grip sensors
#
ifneq (,$(filter m,$(CONFIG_SENSORS_SX9385) $(CONFIG_SENSORS_SX9385_SUB)))
	obj-m += sx9385.o
else ifneq (,$(filter y,$(CONFIG_SENSORS_SX9385) $(CONFIG_SENSORS_SX9385_SUB)))
	obj-y += sx9385.o
endif
