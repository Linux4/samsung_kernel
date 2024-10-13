#############################################################################
#
# Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
#
#############################################################################

# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
clean-files := *.o *.ko

ifneq ($(CONFIG_SCSC_CORE),n)

ccflags-y +=  -Iinclude/scsc
ccflags-y += $(CONFIG_SAMSUNG_MAXWELL_EXTRA)

ifneq ($(filter $(CONFIG_SCSC_MXLOGGER), y m),)
ccflags-y += -DCONFIG_SCSC_MXLOGGER
endif

ifneq ($(filter $(CONFIG_SCSC_LOG_COLLECTION), y m),)
ccflags-y += -DCONFIG_SCSC_LOG_COLLECTION
endif

ifneq ($(filter $(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM), y m),)
ccflags-y += -DCONFIG_SCSC_INDEPENDENT_SUBSYSTEM
endif

obj-$(CONFIG_SCSC_LOG_COLLECTION) += scsc_log_collection.o
scsc_log_collection-$(CONFIG_SCSC_LOG_COLLECTION) += logs/scsc_log_collector.o logs/scsc_log_collector_proc.o logs/scsc_log_collector_mmap.o

obj-$(CONFIG_SCSC_LOGRING) += scsc_logring.o
scsc_logring-$(CONFIG_SCSC_LOGRING) += logs/scsc_logring_main.o logs/scsc_logring_ring.o logs/scsc_logring_debugfs.o

##
## The following cumbersome ifeq/else is the only way to make
## this CONFIG_SCSC_PRINTK propagate correctly in any scenario (y,m,n)
## Otherwise using ONLY the else branch when CONFIG_SCSC_DEBUG evaluates
## to m ==>> ccflags-m does NOT work correctly in every build scenario:
## - kernel debug modules : builtin with scsc_logring support
## - kernel nodebug modules : builtins without scsc_logring support
## - kernel debug|nodebug nomodules : no builtin / no scsclogring
##
ccflags-y += -DCONFIG_SCSC_PRINTK

ifneq ($(CONFIG_SCSC_DEBUG),n)
ccflags-$(CONFIG_SCSC_STATIC_RING)	+= -DCONFIG_SCSC_STATIC_RING_SIZE=$(CONFIG_SCSC_STATIC_RING_SIZE)
endif

#obj-$(CONFIG_SCSC_WIFILOGGER) +=  scsc_wifilogger.o
#scsc_wifilogger-y += scsc_wifilogger_api.o \
#		     scsc_wifilogger_internal.o \
#		     scsc_wifilogger_core.o \
#		     scsc_wifilogger_module.o \
#		     scsc_wifilogger_ring_connectivity.o \
#		     scsc_wifilogger_ring_wakelock.o \
#		     scsc_wifilogger_ring_pktfate.o

#scsc_wifilogger-$(CONFIG_SCSC_WIFILOGGER_DEBUGFS) += scsc_wifilogger_debugfs.o
#scsc_wifilogger-$(CONFIG_SCSC_WIFILOGGER_TEST) += scsc_wifilogger_ring_test.o

# GDB channels for MoreDump
ccflags-$(CONFIG_SCSC_MX450_GDB_SUPPORT) += -DCONFIG_SCSC_MX450_GDB_SUPPORT

ifeq ($(CONFIG_SCSC_PCIE), m)
ccflags-y += -DCONFIG_SCSC_PCIE
ifeq ($(CONFIG_SCSC_PCIE_PAEAN_X86), y)
ccflags-y += -DCONFIG_SCSC_PCIE_PAEAN_X86
ccflags-y += -DCONFIG_SCSC_COMMON_HCF
else
ccflags-y += -DCONFIG_SCSC_PCIE_SNOW
endif
endif

# PCI-E MIF
obj-$(CONFIG_SCSC_PCIE) += scsc_pcie_mif.o
ifeq ($(CONFIG_SCSC_PCIE_PAEAN_X86), y)
scsc_pcie_mif-$(CONFIG_SCSC_PCIE) += \
    pcie_mif_module.o \
	pcie_mif_paean.o \
	pcie_proc.o
else
scsc_pcie_mif-$(CONFIG_SCSC_PCIE) += \
    pcie_mif_module.o \
	pcie_mif.o \
	pcie_proc.o
endif

obj-$(CONFIG_SCSC_PLATFORM) += scsc_platform_mif.o

scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += \
	platform_mif_module.o

ifeq ($(CONFIG_WLBT_REFACTORY), y)

ifeq ($(CONFIG_SCSC_PCIE_CHIP), y)
	SUB_API_PATH := pcie
scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += \
	${SUB_API_PATH}/platform_mif_pcie_api.o
else
	SUB_API_PATH := modap
scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += \
	${SUB_API_PATH}/platform_mif_memlog_api.o \
	${SUB_API_PATH}/platform_mif_regmap_api.o
endif

scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += \
	${SUB_API_PATH}/platform_mif_memory_api.o \
	${SUB_API_PATH}/platform_mif.o \
	${SUB_API_PATH}/platform_mif_irq_api.o \
	${SUB_API_PATH}/platform_mif_pm_api.o \
	${SUB_API_PATH}/platform_mif_intr_handler.o

scsc_platform_mif-$(CONFIG_WLBT_PROPERTY_READ) += \
	common/platform_mif_property_api.o

scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += ${SUB_API_PATH}/pmu_cal.o

ifeq ($(CONFIG_SOC_S5E5515), y)
	CHIP_PATH := s5e5515
endif
ifeq ($(CONFIG_SOC_S5E8535), y)
	CHIP_PATH := s5e8535
endif
ifeq ($(CONFIG_SOC_S5E8825), y)
	CHIP_PATH := s5e8825
endif
ifeq ($(CONFIG_SOC_S5E8835), y)
	CHIP_PATH := s5e8835
endif
ifeq ($(CONFIG_SOC_S5E8845), y)
	CHIP_PATH := s5e8845
endif
ifeq ($(CONFIG_SCSC_BB_PAEAN), y)
	CHIP_PATH := s6165
endif
ifeq ($(CONFIG_SCSC_BB_REDWOOD), y)
	CHIP_PATH := s6175
endif
ifeq ($(CONFIG_SOC_S5E5535), y)
	CHIP_PATH := s5e5535
endif

scsc_platform_mif-$(CONFIG_WLBT_ITMON_NOTIFIER) += \
	${SUB_API_PATH}/${CHIP_PATH}/platform_mif_itmon_api.o

ifeq ($(CONFIG_SCSC_PCIE_CHIP), y)
scsc_platform_mif-y += \
	${SUB_API_PATH}/pcie.o \
	pcie_proc.o \
	${SUB_API_PATH}/${CHIP_PATH}/pcie_remapper.o
else
scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += \
	${SUB_API_PATH}/${CHIP_PATH}/platform_mif_legacy.o
endif

scsc_platform_mif-$(CONFIG_SCSC_QOS) += \
	${SUB_API_PATH}/${CHIP_PATH}/platform_mif_qos_api.o

scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += \
	${SUB_API_PATH}/${CHIP_PATH}/pmu_cal_data.o

endif

ifeq ($(CONFIG_SOC_EXYNOS9610),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_9610.o
endif
ifeq ($(CONFIG_SOC_EXYNOS9630),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_9630.o
endif
ifeq ($(CONFIG_SOC_EXYNOS3830),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_3830.o
endif
ifeq ($(CONFIG_SOC_EXYNOS7570),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif.o
endif
ifeq ($(CONFIG_SOC_EXYNOS7872),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif.o
endif
ifeq ($(CONFIG_SOC_EXYNOS7885),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_7885.o
endif
ifeq ($(CONFIG_SOC_S5E9815),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_9815.o
endif
ifeq ($(CONFIG_SOC_S5E5515),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_s5e5515.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal_data_5515.o
endif
ifeq ($(CONFIG_SOC_S5E8825),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_s5e8825.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal_data_8825.o
endif
ifeq ($(CONFIG_SOC_S5E8535),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_s5e8535.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal_data_8535.o
endif
ifeq ($(CONFIG_SOC_S5E8835),y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_s5e8835.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal.o
	scsc_platform_mif-$(CONFIG_WLBT_AUTOGEN_PMUCAL) += pmu_cal_data_8835.o
endif

ifeq ($(CONFIG_SCSC_PCIE_CHIP),y)
ifneq ($(CONFIG_WLBT_REFACTORY), y)
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += platform_mif_s6165.o
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += pcie_s6165.o
	scsc_platform_mif-$(CONFIG_SCSC_PLATFORM) += pcie_proc.o
endif
endif

#core_module for static functions and registration client module (all the service driver modules -wlan, bt,...-)
#core for instance
obj-$(CONFIG_SCSC_CORE_CM) += scsc_mx.o
scsc_mx-y += \
	mxlog_transport.o \
	fw_panic_record.o \
	panicmon.o \
	suspendmon.o \
	mxlog.o \
	mxproc.o \
	scsc_service.o \
	scsc_mx_module.o \
	scsc_mx_impl.o \
	miframman.o \
	mifmboxman.o \
	mifproc.o \
	mifintrbit.o \
	fwimage.o \
	cpacket_buffer.o \
	mifstream.o \
	mxmgmt_transport.o \
	gdb_transport.o \
	scsc_lerna.o \
	mxsyserr.o \
	mxfwconfig.o \
	mx140_file.o \
	wlbt_ramsd.o

scsc_mx-$(CONFIG_SCSC_SMAPPER) += mifsmapper.o

scsc_mx-$(CONFIG_SCSC_WLBTD) += scsc_wlbtd.o

scsc_mx-$(CONFIG_SCSC_QOS) += mifqos.o

scsc_mx-$(CONFIG_SCSC_LAST_PANIC_IN_DRAM) += scsc_log_in_dram.o

scsc_mx-$(CONFIG_EXYNOS_SYSTEM_EVENT) += mxman_sysevent.o

ifneq ($(filter $(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM), y m),)
ccflags-y += -DCONFIG_SCSC_WLBTD
endif

ifneq ($(filter $(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM), y m),)
scsc_mx-$(CONFIG_SCSC_MXLOGGER) += mxlogger_split.o
scsc_mx-y += mxman_split.o
scsc_mx-y += mxman_res.o
scsc_mx-y += mxman_if.o
scsc_mx-y += whdr.o
scsc_mx-y += fhdr.o
scsc_mx-y += bhdr.o
scsc_mx-y += mifpmuman.o
scsc_mx-y += fw_obj_index.o
else
scsc_mx-$(CONFIG_SCSC_MXLOGGER) += mxlogger.o
scsc_mx-y += mxman.o
scsc_mx-y += fwhdr.o
endif

ifneq ($(CONFIG_SCSC_CM_MX_CLIENT_TEST),n)
obj-$(CONFIG_SCSC_CM_MX_CLIENT_TEST) += mx_client_test.o
mx_client_test-y += client_test.o
endif

ifneq ($(CONFIG_SCSC_FLASH_SERVICE),n)
obj-$(CONFIG_SCSC_FLASH_SERVICE) += scsc_flash_service.o
scsc_flash_service-y += flash_service.o
ccflags-y += -DCONFIG_SCSC_FLASH_SERVICE
endif

ifneq ($(CONFIG_SCSC_MMAP),n)
obj-$(CONFIG_SCSC_MMAP) += scsc_mmap.o
scsc_mmap-y += mx_mmap.o
endif

ifneq ($(CONFIG_SCSC_CLK20MHZ),n)
obj-$(CONFIG_SCSC_CLK20MHZ) += scsc_mx140_clk.o
scsc_mx140_clk-y += mx140_clk.o
ifeq ($(CONFIG_SCSC_CLK20MHZ_TEST),y)
scsc_mx140_clk-y += mx140_clk_test.o
endif
endif

ifneq ($(CONFIG_SCSC_FM), n)
obj-$(CONFIG_SCSC_FM) += scsc_mx250_fm.o
scsc_mx250_fm-y += mx250_fm.o
endif

obj-$(CONFIG_SCSC_DBG_SAMPLER) += scsc_dbg_sampler.o
scsc_dbg_sampler-y += mx_dbg_sampler.o

ifeq ($(CONFIG_KUNIT),y)
obj-$(CONFIG_WLBT_KUNIT)	+= kunit/
ifeq ($(CONFIG_WLBT_KUNIT), y)
export GCOV_PROFILE := y
endif
endif

endif

ifneq ($(CONFIG_SCSC_SEP_VERSION),0)
ccflags-y += -DSCSC_SEP_VERSION=$(CONFIG_SCSC_SEP_VERSION)
endif

# The below section is only for DTE purpose. It is controlled by the flag SCSC_DTE_BUILD which is set only by the
# DTE top level Makefile. Hence, no risk.
# In the below include dir the KERNEL_DIR comes from the DTE top level makefile.
ifeq ($(SCSC_DTE_BUILD),y)
obj-m := scsc_mx.o
obj-m += mx_client_test.o
mx_client_test-y += client_test.o

ccflags-y +=  -I$(KERNEL_DIR)/include/scsc/
ccflags-y +=  -I$(KERNEL_DIR)/include
ccflags-y +=  -I$(KERNEL_DIR)/drivers/misc/samsung/scsc
ccflags-y +=  -I$(KERNEL_DIR)/drivers/net/wireless/scsc

endif
