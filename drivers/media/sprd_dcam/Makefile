# SC88XX Platform Support

obj-y += sprd_dcam.o
ccflags-y += -Idrivers/media/sprd_dcam/common
common-file := common/dcam_v4l2.o  common/gen_scale_coef.o common/sin_cos.o

ifeq ($(CONFIG_ARCH_SCX15),y)
ccflags-y += -Idrivers/media/sprd_dcam/scx15
sprd_dcam-objs := scx15/dcam_drv.o $(common-file)
else
ifeq ($(CONFIG_ARCH_SCX35),y)
ccflags-y += -Idrivers/media/sprd_dcam/sc8830
sprd_dcam-objs := sc8830/dcam_drv.o $(common-file)
endif
endif
obj-y += flash/
