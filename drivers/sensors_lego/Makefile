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

#
# Makefile for grip sensors
#

ifneq (,$(filter m,$(CONFIG_SENSORS_A96T396) $(CONFIG_SENSORS_A96T396_SUB) $(CONFIG_SENSORS_A96T396_SUB2) $(CONFIG_SENSORS_A96T396_WIFI)))
	obj-m += a96t396.o
else ifneq (,$(filter y,$(CONFIG_SENSORS_A96T396) $(CONFIG_SENSORS_A96T396_SUB) $(CONFIG_SENSORS_A96T396_SUB2) $(CONFIG_SENSORS_A96T396_WIFI)))
	obj-y += a96t396.o
endif