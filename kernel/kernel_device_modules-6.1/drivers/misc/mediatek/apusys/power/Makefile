# common part
ccflags-y += -I$(srctree)/drivers/devfreq/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/soc/mediatek/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/debug/
# for mtk_sip_svc.h
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux/soc/mediatek/

# apusys power driver v2.5
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/2.5/inc/
SRC := 2.5/common/apu-clk.o 2.5/common/apu-common.o 2.5/common/apu-regulator.o 2.5/common/apu-rpc.o 2.5/common/apu-of.o 2.5/common/apu-plat.o  2.5/common/apu-driver.o 2.5/common/apu-dbg.o 2.5/common/apu-trace.o
SRC += 2.5/governor/gov-apusys.o 2.5/governor/gov-user.o 2.5/governor/common.o 2.5/governor/gov-constrain.o 2.5/governor/gov-apusys-pe.o
SRC += 2.5/devices/dev-freq-core.o 2.5/devices/dev-freq-conn.o 2.5/devices/dev-freq-vpu.o 2.5/devices/dev-freq-mdla.o 2.5/devices/dev-cb.o 2.5/devices/dev-freq-iommu.o

# apusys power driver v3.0
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/inc/platform/
SRC += 3.0/aputop_drv.o
SRC += 3.0/aputop_rpmsg.o
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6983_aputop.c),)
subdir-ccflags-y += -DMT6983_PLAT_DATA
SRC += 3.0/platform/mt6983_aputop.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6983_pwr_func.c),)
SRC += 3.0/platform/mt6983_pwr_func.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6879_aputop.c),)
subdir-ccflags-y += -DMT6879_PLAT_DATA
SRC += 3.0/platform/mt6879_aputop.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6879_pwr_func.c),)
SRC += 3.0/platform/mt6879_pwr_func.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6897_aputop.c),)
subdir-ccflags-y += -DMT6897_PLAT_DATA
SRC += 3.0/platform/mt6897_aputop.o
SRC += 3.0/platform/mt6897_ao.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6897_pwr_func.c),)
SRC += 3.0/platform/mt6897_pwr_func.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6878_aputop.c),)
subdir-ccflags-y += -DMT6878_PLAT_DATA
SRC += 3.0/platform/mt6878_aputop.o
SRC += 3.0/platform/mt6878_ao.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6878_pwr_func.c),)
SRC += 3.0/platform/mt6878_pwr_func.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6895_aputop.c),)
subdir-ccflags-y += -DMT6895_PLAT_DATA
SRC += 3.0/platform/mt6895_aputop.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6895_pwr_func.c),)
SRC += 3.0/platform/mt6895_pwr_func.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6985_aputop.c),)
subdir-ccflags-y += -DMT6985_PLAT_DATA
SRC += 3.0/platform/mt6985_aputop.o
SRC += 3.0/platform/mt6985_ao.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6985_pwr_func.c),)
SRC += 3.0/platform/mt6985_pwr_func.o
endif

ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6886_aputop.c),)
subdir-ccflags-y += -DMT6886_PLAT_DATA
SRC += 3.0/platform/mt6886_aputop.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6886_pwr_func.c),)
SRC += 3.0/platform/mt6886_pwr_func.o
endif

ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt8188_aputop.c),)
subdir-ccflags-y += -DMT8188_PLAT_DATA
SRC += 3.0/platform/mt8188_aputop.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt8188_pwr_func.c),)
SRC += 3.0/platform/mt8188_pwr_func.o
endif

ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6989_aputop.c),)
subdir-ccflags-y += -DMT6989_PLAT_DATA
SRC += 3.0/platform/mt6989_aputop.o
SRC += 3.0/platform/mt6989_ao.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/platform/mt6989_pwr_func.c),)
SRC += 3.0/platform/mt6989_pwr_func.o
endif

SRC += apu_top_entry.o

ifneq (,$(findstring aging,$(PROJECT_DTB_NAMES)))
subdir-ccflags-y += -DAGING_MARGIN
endif

obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apu_top.o
apu_top-objs = $(SRC)
