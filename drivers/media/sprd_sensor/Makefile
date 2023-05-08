include drivers/media/sprd_dcam/common/Makefile.inc

common-file := csi2/csi_access.o csi2/csi_driver.o csi2/csi_log.o csi2/csi_system.o

sprd_sensor-objs := csi2/csi_api.o

ifeq ($(CONFIG_64BIT),y)
sprd_sensor-objs += compat_sensor_drv_k.o $(common-file)
endif

sprd_sensor-objs += sensor_drv_k.o $(common-file)
obj-y += sprd_sensor.o
obj-y += power/ otp/ identify/
obj-y += sensor_reloadinfo_thread.o
