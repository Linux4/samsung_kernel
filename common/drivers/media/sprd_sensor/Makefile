# SC88XX Platform Support

obj-y += sprd_sensor.o
ccflags-y += -Idrivers/media/sprd_dcam/common

common-file := csi2/csi_access.o csi2/csi_driver.o csi2/csi_log.o csi2/csi_system.o
ifeq ($(CONFIG_ARCH_SCX35),y)
sprd_sensor-objs := csi2/csi_api.o
endif
ifeq ($(CONFIG_ARCH_SCX15),y)
sprd_sensor-objs := csi2/csi_api_null.o
endif
sprd_sensor-objs += sensor_drv_k.o $(common-file)
obj-y += power/
