# SC88XX Platform Support

ifeq ($(CONFIG_ARCH_SCX30G2),y)
ccflags-y += -Idrivers/media/sprd_dcam/tshark2
DEFS = -DVERSION3D=1
else ifeq ($(CONFIG_ARCH_SCX30G),y)
ifeq ($(CONFIG_ARCH_SCX20),y)
ccflags-y += -Idrivers/media/sprd_dcam/pike
else
ccflags-y += -Idrivers/media/sprd_dcam/tshark
endif
DEFS = -DVERSION3T=1
else ifeq ($(CONFIG_ARCH_SCX35L),y)
ccflags-y += -Idrivers/media/sprd_dcam/sharkl
DEFS = -DVERSION3L=1
endif

ccflags-y += $(DEFS)
ccflags-y += -Idrivers/media/sprd_dcam/common
