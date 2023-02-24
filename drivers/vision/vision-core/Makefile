obj-$(CONFIG_VISION_CORE)	+= vision-dev.o
obj-$(CONFIG_VISION_CORE)	+= vision-compat-ioctl32.o
obj-$(CONFIG_VISION_CORE)	+= vision-ioctl.o
obj-$(CONFIG_VISION_CORE)	+= vision-buffer.o

ccflags-y += -Idrivers/vision/vision-core/include -Idrivers/vision/npu/core/include -Wformat
subdir-ccflags-y := -Wformat