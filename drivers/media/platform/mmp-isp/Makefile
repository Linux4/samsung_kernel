# Makefile for MARVELL MMP ISP driver

# TODO: Add Camera/ISP IP driver below
obj-$(CONFIG_VIDEO_MMPISP_B52SENSOR)	+= b52-sensor.o
obj-$(CONFIG_SUBDEV_VCM)		+= b52-vcm.o
obj-$(CONFIG_SUBDEV_FLASH)		+= b52-flash.o
obj-$(CONFIG_VIDEO_MMPISP_B52)		+= b52-reg.o b52isp-ctrl.o b52isp.o
obj-$(CONFIG_VIDEO_MMPISP_CCICV2)	+= ccicv2.o

# TODO: Add platform Camera/ISP driver below
obj-$(CONFIG_HOST_SUBDEV)		+= host_isd.o
obj-$(CONFIG_VIDEO_MMPISP_PLATFORM)	+= plat_cam.o
obj-$(CONFIG_VIDEO_MMPISP)		+= b52socisp/
