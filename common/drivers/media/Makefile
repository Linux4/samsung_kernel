#
# Makefile for the kernel multimedia device drivers.
#

media-objs	:= media-device.o media-devnode.o media-entity.o

#
# I2C drivers should come before other drivers, otherwise they'll fail
# when compiled as builtin drivers
#
obj-y += i2c/ tuners/
obj-$(CONFIG_DVB_CORE)  += dvb-frontends/

#
# Now, let's link-in the media core
#
ifeq ($(CONFIG_MEDIA_CONTROLLER),y)
  obj-$(CONFIG_MEDIA_SUPPORT) += media.o
endif

obj-$(CONFIG_VIDEO_DEV) += v4l2-core/
obj-$(CONFIG_VIDEO_DCAM_SPRD) += sprd_dcam/
obj-$(CONFIG_VIDEO_DCAM_SPRD) += sprd_scale/
obj-$(CONFIG_VIDEO_DCAM_SPRD) += sprd_rotation/
obj-$(CONFIG_VIDEO_GSP_SPRD)  += sprd_gsp/
obj-$(CONFIG_VIDEO_DCAM_SPRD) += sprd_sensor/
obj-$(CONFIG_VIDEO_DCAM_SPRD) += sprd_isp/
obj-$(CONFIG_VIDEO_DCAM_SPRD) += sprd_dma_copy/
obj-$(CONFIG_DVB_CORE)  += dvb-core/
obj-$(CONFIG_MTV) += mtv/

# There are both core and drivers at RC subtree - merge before drivers
obj-y += rc/

#
# Finally, merge the drivers that require the core
#

obj-y += common/ platform/ pci/ usb/ mmc/ firewire/ parport/
obj-$(CONFIG_VIDEO_DEV) += radio/

