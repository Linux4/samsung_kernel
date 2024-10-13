# SC88XX Platform Support

ifeq ($(CONFIG_ARCH_SCX30G2),y)
ccflags-y += -Idrivers/media/sprd_dcam/tshark
DEFS = -DVERSION3D=1
else ifeq ($(CONFIG_ARCH_SCX30G),y)
ccflags-y += -Idrivers/media/sprd_dcam/tshark
DEFS = -DVERSION3T=1
else ifeq ($(CONFIG_ARCH_SCX35L),y)
ifeq ($(CONFIG_MACH_SP9830I),y)
ccflags-y += -Idrivers/media/sprd_dcam/tshark2
else
ccflags-y += -Idrivers/media/sprd_dcam/sharkl
endif
DEFS = -DVERSION3L=1
endif

ccflags-y += $(DEFS)
ccflags-y += -Idrivers/media/sprd_dcam/common
