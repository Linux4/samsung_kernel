# SC88XX Platform Support

ifeq ($(CONFIG_ARCH_SCX15),y)
ccflags-y += -Idrivers/media/sprd_dcam/scx15 -Idrivers/media/sprd_dcam/common
else
ccflags-y += -Idrivers/media/sprd_dcam/sc8830 -Idrivers/media/sprd_dcam/common
endif

ifeq ($(CONFIG_ARCH_SCX35),y)
sprd_rotation-objs := sc8830/rot_drv.o sc8830/img_rot.o
obj-y += sprd_rotation.o
endif

