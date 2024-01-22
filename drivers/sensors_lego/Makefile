#
# Makefile for grip sensors
#

obj-$(CONFIG_SENSORS_ISG6320)	    += isg6320.o


#
# Makefile for grip sensors
#
ifneq (,$(filter m,$(CONFIG_SENSORS_ISG5320A) $(CONFIG_SENSORS_ISG5320A_SUB)))
	obj-m += isg5320a.o
else ifneq (,$(filter y,$(CONFIG_SENSORS_ISG5320A) $(CONFIG_SENSORS_ISG5320A_SUB)))
	obj-y += isg5320a.o
endif
