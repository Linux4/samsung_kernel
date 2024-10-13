# SC88XX Platform Support
include drivers/media/sprd_dcam/common/Makefile.inc

obj-y += sprd_dcam.o
#ccflags-y += -Idrivers/media/sprd_dcam/common
common-file := common/dcam_img.o common/parse_hwinfo.o common/gen_scale_coef.o common/sin_cos.o common/flash_test.o


ifeq ($(CONFIG_ARCH_SCX30G),y)

ifeq ($(CONFIG_ARCH_SCX20),y)
sprd_dcam-objs := pike/dcam_drv.o $(common-file)
else ifeq ($(CONFIG_ARCH_SCX30G2),y)
sprd_dcam-objs := tshark2/dcam_drv.o $(common-file)
else
sprd_dcam-objs := tshark/dcam_drv.o $(common-file)
endif

else ifeq ($(CONFIG_ARCH_SCX35L),y)
ifeq ($(CONFIG_ARCH_SCX35LT8),y)
sprd_dcam-objs := tshark2/dcam_drv.o $(common-file)
else ifeq ($(CONFIG_MACH_SP9830I),y)
sprd_dcam-objs := tshark2/dcam_drv.o $(common-file)
else
sprd_dcam-objs := sharkl/dcam_drv.o $(common-file)
endif
endif
obj-y += flash/
