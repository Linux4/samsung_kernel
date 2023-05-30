ifeq ($(CONFIG_MTK_SENSOR_SUPPORT),y)
obj-$(CONFIG_CUSTOM_KERNEL_SENSORHUB)	+= sensorHub/
obj-$(CONFIG_MTK_HWMON)	    += hwmon/
obj-$(CONFIG_CUSTOM_KERNEL_ALSPS) += alsps/
ifeq ($(CONFIG_CUSTOM_KERNEL_ACCELGYRO),y)
obj-y += accelgyro/
else
obj-$(CONFIG_CUSTOM_KERNEL_ACCELEROMETER) += accelerometer/
obj-$(CONFIG_CUSTOM_KERNEL_GYROSCOPE) += gyroscope/
endif
obj-$(CONFIG_CUSTOM_KERNEL_MAGNETOMETER) += magnetometer/
obj-$(CONFIG_CUSTOM_KERNEL_BAROMETER) += barometer/
obj-$(CONFIG_CUSTOM_KERNEL_STEP_COUNTER) += step_counter/
obj-$(CONFIG_CUSTOM_KERNEL_SIGNIFICANT_MOTION_SENSOR) += step_counter/
obj-$(CONFIG_CUSTOM_KERNEL_SITUATION) += situation/
obj-$(CONFIG_CUSTOM_KERNEL_SENSOR_FUSION) += sensorfusion/
else
obj-y += dummy.o
endif
#TabA7 Lite code for SR-AX3565-01-101 by Hujincan at 20201123 start
# hs03s code for SR-AL5625-01-168 by xiongxiaoliang at 2021/06/09 start
#hs14 code for SR-AL6528A-01-376 by xiongxiaoliang at 2022/09/02 start
ifeq ($(CONFIG_HQ_PROJECT_O22), y)
obj-$(CONFIG_CUSTOM_KERNEL_SAR_SENSOR) += o22_sar/
else
obj-$(CONFIG_CUSTOM_KERNEL_SAR_SENSOR) += sar/
endif
#hs14 code for SR-AL6528A-01-376 by xiongxiaoliang at 2022/09/02 end
# hs03s code for SR-AL5625-01-168 by xiongxiaoliang at 2021/06/09 end
#TabA7 Lite code for SR-AX3565-01-101 by Hujincan at 20201123 end
