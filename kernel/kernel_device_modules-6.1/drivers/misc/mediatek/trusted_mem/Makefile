# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2018 MediaTek Inc.
#

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

# Modules Feature Options
include $(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/Makefile.configs

# Force strictly compiler check
ccflags-y += -Werror

#==========================================================================
# Trusted Memory Subsystem Driver
#==========================================================================
# Ensure module is built only when option is enabled
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) = trusted_mem.o
trusted_mem-y += dev_mgr.o
trusted_mem-y += entry.o
trusted_mem-y += region_mgr.o
trusted_mem-y += ssmr_mgr.o
trusted_mem-y += peer_mgr.o
trusted_mem-y += proc.o
trusted_mem-y += ssheap_proc.o
trusted_mem-y += alloc_api.o
trusted_mem-y += ssmr/memory_ssmr.o
trusted_mem-y += ssheap/ssheap_lib.o
ifneq ($(filter y m,$(CONFIG_ALLOC_TMEM_WITH_HIGH_FREQ)),)
trusted_mem-y += tmemperf/tmemperf.o
endif # !CONFIG_ALLOC_TMEM_WITH_HIGH_FREQ

ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
obj-m += tmem_ffa.o
tmem_ffa-y += dev_mgr.o
tmem_ffa-y += entry.o
tmem_ffa-y += region_mgr.o
tmem_ffa-y += ssmr_mgr.o
tmem_ffa-y += peer_mgr.o
tmem_ffa-y += proc.o
tmem_ffa-y += ssheap_proc.o
tmem_ffa-y += alloc_api.o
tmem_ffa-y += ssmr/memory_ssmr.o
tmem_ffa-y += ssheap/ssheap_lib.o
ifneq ($(filter y m,$(CONFIG_ALLOC_TMEM_WITH_HIGH_FREQ)),)
tmem_ffa-y += tmemperf/tmemperf.o
endif # !CONFIG_ALLOC_TMEM_WITH_HIGH_FREQ
endif # CONFIG_ARM_FFA_TRANSPORT

ifeq ($(TCORE_PROFILING_SUPPORT),y)
trusted_mem-y += profiler.o
ccflags-y += -DTCORE_PROFILING_SUPPORT
ifeq ($(TCORE_PROFILING_AUTO_DUMP),y)
ccflags-y += -DTCORE_PROFILING_AUTO_DUMP
endif # !TCORE_PROFILING_AUTO_DUMP
endif # !TCORE_PROFILING_SUPPORT

ifeq ($(TCORE_MEMORY_LEAK_DETECTION_SUPPORT),y)
trusted_mem-y += mld_helper.o
ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
tmem_ffa-y += mld_helper.o
endif
ccflags-y += -DTCORE_MEMORY_LEAK_DETECTION_SUPPORT
endif # !TCORE_KERN_MEMORY_LEAK_DETECTION_SUPPORT

ifeq ($(CONFIG_TEST_MTK_TRUSTED_MEMORY),m)
trusted_mem-y += tests/ut_common.o
trusted_mem-y += tests/ut_server.o
trusted_mem-y += tests/ut_cases.o
trusted_mem-y += tests/ut_ssheap.o
endif # !CONFIG_TEST_MTK_TRUSTED_MEMORY

ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
ifeq ($(CONFIG_TEST_MTK_TRUSTED_MEMORY),m)
tmem_ffa-y += tests/ut_common.o
tmem_ffa-y += tests/ut_server.o
tmem_ffa-y += tests/ut_cases.o
tmem_ffa-y += tests/ut_ssheap.o
endif
endif

ifneq ($(filter y m,$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)),)
#==========================================================================
# TEE Seecure Memory Devices & Trustzone Peer Driver
#==========================================================================
ifneq ($(filter y m,$(CONFIG_MTK_SECURE_MEM_SUPPORT)\
				  $(CONFIG_MTK_WFD_SMEM_SUPPORT)\
				  $(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE)),)
trusted_mem-y += tee_devices.o
ccflags-y += -DTEE_DEVICES_SUPPORT
trusted_mem-y += tee_impl/tee_invoke.o

ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
tmem_ffa-y += tee_devices.o
tmem_ffa-y += tee_impl/tee_invoke.o
endif

ifneq ($(filter y m,$(CONFIG_MICROTRUST_TEE_SUPPORT)\
				  $(CONFIG_TRUSTONIC_TEE_SUPPORT)\
				  $(CONFIG_TEEGRIS_TEE_SUPPORT)),)
trusted_mem-y += tee_impl/tee_ops.o
ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
tmem_ffa-y += tee_impl/tee_ops.o
endif
endif

endif

#==========================================================================
# MTEE Multiple Chunk Devices & Geniezone Peer Driver
#==========================================================================
ifneq ($(filter y m,$(CONFIG_MTK_PROT_MEM_SUPPORT)\
					$(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)\
					$(CONFIG_MTK_SECURE_MEM_SUPPORT)),)
trusted_mem-y += mtee_devices.o
ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
tmem_ffa-y += mtee_devices.o
endif
ccflags-y += -DMTEE_DEVICES_SUPPORT
endif
ifneq ($(filter y m,$(CONFIG_MTK_GZ_KREE)),)
trusted_mem-y += mtee_impl/mtee_ops.o
trusted_mem-y += mtee_impl/mtee_invoke.o
ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
tmem_ffa-y += mtee_impl/mtee_ops.o
tmem_ffa-y += mtee_impl/mtee_invoke.o
endif
endif
ifneq ($(filter y m,$(CONFIG_ARM_FFA_TRANSPORT)),)
tmem_ffa-y += mtee_impl/tmem_ffa.o
endif
trusted_mem-y += mtee_impl/tmem_carveout_heap.o
endif # !CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM

#==========================================================================
# Trusted Memory Subsystem Common Headers
#==========================================================================
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/public/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/ssmr
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/ssheap
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/

#==========================================================================
# Other Modules Include Header Paths
#==========================================================================
ifneq ($(filter y m,$(CONFIG_MTK_GZ_KREE)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/gz/
endif # !CONFIG_MTK_GZ_KREE

ifneq ($(filter y m,$(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/GP
endif # !CONFIG_TRUSTONIC_TEE_SUPPORT

ifneq ($(filter y m,$(CONFIG_MICROTRUST_TEE_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/teei/$(CONFIG_MICROTRUST_TEE_VERSION)/common/include
endif # !CONFIG_MICROTRUST_TEE_SUPPORT

ifneq ($(filter y m,$(CONFIG_BLOWFISH_TEE_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/tzdev
endif # !CONFIG_BLOWFISH_TEE_SUPPORT

ifneq ($(filter y m,$(CONFIG_TEEGRIS_TEE_SUPPORT)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/tee/tzdev/include/tzdev
endif # !CONFIG_TEEGRIS_TEE_SUPPORT

ifneq ($(filter y m,$(CONFIG_ALLOC_TMEM_WITH_HIGH_FREQ)),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/tmemperf
endif # !CONFIG_ALLOC_TMEM_WITH_HIGH_FREQ
