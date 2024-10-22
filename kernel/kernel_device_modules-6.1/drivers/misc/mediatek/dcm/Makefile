ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/dcm/include/

# Core
obj-$(CONFIG_MTK_DCM_COMMON)	+= mtk_dcm.o

# SoC Drivers
ifneq ($(wildcard $(srctree)/$(src)/mt6985_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6985) += mt6985_dcm.o
mt6985_dcm-objs := mt6985_dcm_internal.o mt6985_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6878_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6878) += mt6878_dcm.o
mt6878_dcm-objs := mt6878_dcm_internal.o mt6878_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6897_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6897) += mt6897_dcm.o
mt6897_dcm-objs := mt6897_dcm_internal.o mt6897_dcm_autogen.o
endif

ifneq ($(wildcard $(srctree)/$(src)/mt6989_dcm_internal.c),)
obj-$(CONFIG_MTK_DCM_MT6989) += mt6989_dcm.o
mt6989_dcm-objs := mt6989_dcm_internal.o mt6989_dcm_autogen.o
endif
