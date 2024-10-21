#
# Copyright (C) 2019 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# assign value
UTIL_DIR = util

# declare apusys version
APUSYS_VERSION_MIDWARE = 2.0
APUSYS_VERSION_SAMPLE = 2.0
APUSYS_VERSION_DEBUG = 1.0
APUSYS_VERSION_MNOC = 1.5
APUSYS_VERSION_VPU = 4.5
APUSYS_VERSION_REVISER = 2.0

# target object
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apusys.o

# common include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/geniezone/public/
# for mtk_sip_svc.h
ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux/soc/mediatek/

ccflags-y += -I$(DEVICE_MODULES_PATH)/include/

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/

# core makefile
apusys-objs += core/apusys_core.o

# middleware makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/staging/android/ion
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/debug/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/ap/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/rv/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/ext/
#ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/ap/dummy/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/$(UTIL_DIR)/

apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_drv.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_rsc.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_drv.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_ctx.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_dev.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_ioctl.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_dma.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_aram.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_mem_pool.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_cmd.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_cmn.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_hs.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_sysfs.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_dbg.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_import.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/mdw_util.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/rv/mdw_rv.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/rv/mdw_rv_cmd.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/rv/mdw_rv_cmd_v3.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/rv/mdw_rv_cmd_v4.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/rv/mdw_rv_dev.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/rv/mdw_rv_tag.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/rv/mdw_rv_pb.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/ext/mdw_ext.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/ext/mdw_ext_ioctl.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/ext/mdw_ext_cmd.o
apusys-objs += midware/$(APUSYS_VERSION_MIDWARE)/ext/mdw_ext_hs.o

# support trace
CFLAGS_mtk_apu_events.o	:= -I$(src)
apusys-$(CONFIG_FTRACE) += $(UTIL_DIR)/apusys_trace.o
apusys-objs += $(UTIL_DIR)/apu_bmap.o

# edma driver makefile
apusys-objs += edma/edma_driver.o
apusys-objs += edma/edma_dbgfs.o
apusys-objs += edma/edma_cmd_hnd.o
apusys-objs += edma/edma_plat_device.o
apusys-objs += edma/edma_v20.o
apusys-objs += edma/edma_v30.o
apusys-objs += edma/edma_rv.o
#apusys-objs += edma/apusys_power_dummy.o


# reviser driver makefile
REVISER=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/
REVISER_PLAT=reviser/$(APUSYS_VERSION_REVISER)/platform
REVISER_COMN=reviser/$(APUSYS_VERSION_REVISER)/common

ccflags-y += -I$(REVISER)/$(REVISER_PLAT)
ccflags-y += -I$(REVISER)/$(REVISER_COMN)
ccflags-y += -I$(REVISER)/$(REVISER_PLAT)/v_1_0

apusys-objs += $(REVISER_COMN)/reviser_drv.o
apusys-objs += $(REVISER_COMN)/reviser_dbg.o
apusys-objs += $(REVISER_COMN)/reviser_export.o
apusys-objs += $(REVISER_COMN)/reviser_import.o
apusys-objs += $(REVISER_COMN)/reviser_hw_mgt.o
apusys-objs += $(REVISER_COMN)/reviser_mem.o
apusys-objs += $(REVISER_COMN)/reviser_power.o
apusys-objs += $(REVISER_COMN)/reviser_table_mgt.o
apusys-objs += $(REVISER_COMN)/reviser_remote.o
apusys-objs += $(REVISER_COMN)/reviser_remote_cmd.o

apusys-objs += $(REVISER_PLAT)/reviser_device.o
apusys-objs += $(REVISER_PLAT)/reviser_plat.o

apusys-objs += $(REVISER_PLAT)/v_1_0/reviser_hw_v1_0.o
apusys-objs += $(REVISER_PLAT)/v_1_0/reviser_plat_v1_0.o
apusys-objs += $(REVISER_PLAT)/v_1_0/reviser_reg_v1_0.o
apusys-objs += $(REVISER_PLAT)/v_rv/reviser_hw_vrv.o
apusys-objs += $(REVISER_PLAT)/v_rv/reviser_plat_vrv.o

# apummu driver makefile
APUMMU_ROOT=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/
APUMMU_PLAT=apummu/platform
APUMMU_COMN=apummu/common

ccflags-y += -I$(APUMMU_ROOT)/$(APUMMU_PLAT)
ccflags-y += -I$(APUMMU_ROOT)/$(APUMMU_COMN)

# apummu boot api
apusys-objs += $(APUMMU_COMN)/apummu_boot.o

apusys-objs += $(APUMMU_COMN)/apummu_drv.o
apusys-objs += $(APUMMU_COMN)/apummu_dbg.o
apusys-objs += $(APUMMU_COMN)/apummu_export.o
apusys-objs += $(APUMMU_COMN)/apummu_import.o
apusys-objs += $(APUMMU_COMN)/apummu_mem.o
apusys-objs += $(APUMMU_COMN)/apummu_remote.o
apusys-objs += $(APUMMU_COMN)/apummu_remote_cmd.o
apusys-objs += $(APUMMU_COMN)/apummu_mgt.o

apusys-objs += $(APUMMU_PLAT)/apummu_device.o
apusys-objs += $(APUMMU_PLAT)/apummu_plat.o

# mnoc makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/common/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/platform/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/utilities/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/mnoc/$(APUSYS_VERSION_MNOC)/interface/

apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_drv.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_pmu.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_tag.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_rv.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/mnoc_plat_device.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/v1_50/mnoc_hw_v1_50.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/v1_51/mnoc_hw_v1_51.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/platform/v1_52/mnoc_hw_v1_52.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/utilities/mnoc_util.o
ifneq (, $(CONFIG_MTK_QOS_FRAMEWORK))
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/qos/
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_dbg.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_qos.o
apusys-objs += mnoc/$(APUSYS_VERSION_MNOC)/common/mnoc_qos_sys.o
endif

# debug makefile
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/debug/$(APUSYS_VERSION_DEBUG)/common/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/debug/$(APUSYS_VERSION_DEBUG)/interface/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/debug/$(APUSYS_VERSION_DEBUG)/platform/

ifeq ($(CONFIG_MTK_APUSYS_DEBUG),y)
apusys-objs += debug/apu_tags.o
apusys-objs += debug/apu_tp.o

apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/common/debug_drv.o
apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/platform/debug_plat_device.o
apusys-objs += debug/$(APUSYS_VERSION_DEBUG)/platform/debug_platform.o
endif

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/debug/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/include/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/midware/$(APUSYS_VERSION_MIDWARE)/

apusys-objs += power/2.5/common/apu-tag.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/2.5/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/inc/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/inc/platform/
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += power/

# apusys power budget
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/pbm/
apusys-objs += pbm/apu_power_budget.o

# mdla
MDLA=$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys
ccflags-y += -I$(srctree)/$(src)/mdla/
apusys-objs += $(shell cd $(MDLA) && find mdla -name "*.c" | sed 's/\.c/\.o/g')

# vpu
VPU = vpu/$(APUSYS_VERSION_VPU)
ccflags-y += -I$(srctree)/$(src)/$(VPU)
ifeq ($(CONFIG_MTK_APUSYS_VPU_DEBUG),y)
apusys-objs += $(VPU)/vpu_tag.o
apusys-objs += $(VPU)/vpu_met.o
endif
ifeq ($(CONFIG_MTK_APUSYS_VPU),y)
apusys-objs += $(VPU)/vpu_main.o
apusys-objs += $(VPU)/vpu_algo.o
apusys-objs += $(VPU)/vpu_cmd.o
apusys-objs += $(VPU)/vpu_hw.o
apusys-objs += $(VPU)/vpu_mem.o
apusys-objs += $(VPU)/vpu_power.o
apusys-objs += $(VPU)/vpu_cfg.o
apusys-objs += $(VPU)/vpu_debug.o
apusys-objs += $(VPU)/vpu_dump.o
endif

# devapc
apusys-objs += devapc/dapc.o
apusys-objs += devapc/dapc_cfg.o

# apusys rv
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/
apusys-objs += apusys_rv/apu_config.o
apusys-objs += apusys_rv/apu_coredump.o
apusys-objs += apusys_rv/apu_excep.o
apusys-objs += apusys_rv/apu_ce_excep.o
apusys-objs += apusys_rv/apu_ipi.o
apusys-objs += apusys_rv/apu_mbox.o
apusys-objs += apusys_rv/apu_mem.o
apusys-objs += apusys_rv/apu_rproc.o
apusys-objs += apusys_rv/apu_procfs.o
apusys-objs += apusys_rv/apu_timesync.o
apusys-objs += apusys_rv/apu_deepidle.o
apusys-objs += apusys_rv/apu_regdump.o
apusys-objs += apusys_rv/apu_debug.o
apusys-objs += apusys_rv/apusys_rv_tag.o
apusys-objs += apusys_rv/aov_recovery.o

ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt6878_plat.c),)
subdir-ccflags-y += -DMT6878_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt6878_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt6879_plat.c),)
subdir-ccflags-y += -DMT6879_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt6879_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt6886_plat.c),)
subdir-ccflags-y += -DMT6886_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt6886_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/1.0/mt6893_plat.c),)
subdir-ccflags-y += -DMT6893_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/1.0/mt6893_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt6895_plat.c),)
subdir-ccflags-y += -DMT6895_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt6895_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt6897_plat.c),)
subdir-ccflags-y += -DMT6897_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt6897_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt6983_plat.c),)
subdir-ccflags-y += -DMT6983_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt6983_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt6985_plat.c),)
subdir-ccflags-y += -DMT6985_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt6985_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/2.0/mt8188_plat.c),)
subdir-ccflags-y += -DMT8188_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/2.0/mt8188_plat.o
endif
ifneq ($(wildcard $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/apusys_rv/3.0/mt6989_plat.c),)
subdir-ccflags-y += -DMT6989_APUSYS_RV_PLAT_DATA
apusys-objs += apusys_rv/3.0/mt6989_plat.o
endif

# ipi
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/ipi/
ccflags-y += -I$(srctree)/drivers/rpmsg/
apusys-objs += ipi/mtk_apu_rpmsg.o

# sw logger
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/sw_logger/
apusys-objs += sw_logger/sw_logger.o

# mvpu
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/mvpu/
apusys-objs += mvpu/mvpu_driver.o
apusys-objs += mvpu/mvpu_plat_device.o
apusys-objs += mvpu/mvpu_ipi.o
apusys-objs += mvpu/mvpu_sysfs.o
apusys-objs += mvpu/mvpu_sec.o

# aps
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/aps/
apusys-objs += aps/aps_driver.o
apusys-objs += aps/aps_ipi.o
apusys-objs += aps/aps_sysfs.o

# hw logger
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/hw_logger/
apusys-objs += hw_logger/hw_logger.o

# sapu
obj-$(CONFIG_MTK_APUSYS_SUPPORT) += sapu/

# hw_sema
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/power/3.0/inc/

# smmu device driver
apusys-objs += secure_mem/smmu_device.o

# aov
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/aov
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/apusys/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/slbc
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3

AOV_SRC := aov/aov_init.o
AOV_SRC += aov/plat/v1/aov_rpmsg_v1.o
AOV_SRC += aov/plat/v1/aov_recovery_v1.o
AOV_SRC += aov/plat/v2/aov_mem_service_v2.o
AOV_SRC += aov/plat/v2/aov_recovery_v2.o
AOV_SRC += aov/plat/aov_plat.o

obj-$(CONFIG_MTK_APUSYS_SUPPORT) += apu_aov.o
apu_aov-objs = $(AOV_SRC)
