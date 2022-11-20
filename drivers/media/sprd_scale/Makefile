ccflags-y += -Idrivers/media/sprd_scale/common -Idrivers/media/sprd_dcam/common

ifeq ($(CONFIG_ARCH_SCX35),y)
ccflags-y += -Idrivers/media/sprd_scale
sprd_scale-objs := common/sin_cos.o common/gen_scale_coef.o common/img_scale.o scale_drv.o
ifeq ($(CONFIG_64BIT),y)
sprd_scale-objs += common/compat_img_scale.o
endif
obj-y += sprd_scale.o
endif

include drivers/media/sprd_dcam/common/Makefile.inc
