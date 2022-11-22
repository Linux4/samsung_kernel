# SC88XX Platform Support

ifeq ($(CONFIG_ARCH_SCX35),y)
ifeq ($(CONFIG_ARCH_SCX15),y)
sprd_isp-objs := isp_drv_null.o
obj-y += sprd_isp.o
else
sprd_isp-objs := isp_drv_kernel.o
obj-y += sprd_isp.o
endif
endif

ccflags-y += -Idrivers/media/sprd_dcam/common

ifeq ($(CONFIG_ARCH_SC8825),y)
sprd_isp-objs := isp_drv_kernel.o
obj-y += sprd_isp.o
endif

