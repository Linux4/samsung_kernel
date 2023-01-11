##############################################################################
#
#    Copyright (c) 2005 - 2014 by Vivante Corp.
#    
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the license, or
#    (at your option) any later version.
#    
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#    GNU General Public License for more details.
#    
#    You should have received a copy of the GNU General Public License
#    along with this program; if not write to the Free Software
#    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#
##############################################################################
GPU_TYPE ?= XAQ2

DEBUG := 0
ABI := aapcs-linux
ENABLE_ARM_L2_CACHE := 1
NO_DMA_COHERENT := 1
CONFIG_DOVE_GPU := 0
ANDROID := 1
ENABLE_GPU_CLOCK_BY_DRIVER := 1
USE_ARCH_REG := 0
USE_GPU_FREQ := 1
FPGA_BUILD := 0

AQROOT := $(srctree)/$(src)
AQARCH := $(AQROOT)/arch/${GPU_TYPE}

#
# Linux build file for kernel HAL driver.
#
ARCH_TYPE                         ?= arm64
VIVANTE_ENABLE_3D                 ?= 1
VIVANTE_ENABLE_2D                 ?= 1
VIVANTE_ENABLE_VG                 ?= 0
FORCE_ALL_VIDEO_MEMORY_CACHED     ?= 0
NONPAGED_MEMORY_CACHEABLE         ?= 0
NONPAGED_MEMORY_BUFFERABLE        ?= 1
CACHE_FUNCTION_UNIMPLEMENTED      ?= 0
ENABLE_OUTER_CACHE_PATCH          ?= 1
USE_BANK_ALIGNMENT                ?= 0
BANK_BIT_START                    ?= 0
BANK_BIT_END                      ?= 0
BANK_CHANNEL_BIT                  ?= 0
PLATFORM                          ?= default/gc_hal_kernel_platform_default

OS_KERNEL_DIR   := hal/os/linux/kernel
ifeq ($(USE_ARCH_REG), 1)
ARCH_KERNEL_DIR := hal/kernel/arch_reg
ARCH_VG_KERNEL_DIR := hal/kernel/archvg_reg
else
ARCH_KERNEL_DIR := hal/kernel/arch
ARCH_VG_KERNEL_DIR := hal/kernel/archvg
endif
HAL_KERNEL_DIR  := hal/kernel
GPUFREQ_DIR     := $(OS_KERNEL_DIR)/gpufreq

# Check and include platform config.
ifneq ($(PLATFORM),)

# Get platform config path.
PLATFORM_CONFIG ?= $(AQROOT)/$(OS_KERNEL_DIR)/platform/$(PLATFORM).config

# Check whether it exists.
PLATFORM_CONFIG := $(wildcard $(PLATFORM_CONFIG))

# Include it if exists.
ifneq ($(PLATFORM_CONFIG),)
include $(PLATFORM_CONFIG)
endif

endif

MODULE_NAME ?= galcore
CUSTOMER_ALLOCATOR_OBJS    ?=
ALLOCATOR_ARRAY_H_LOCATION ?= $(OS_KERNEL_DIR)/allocator/default/

EXTRA_CFLAGS += -Werror
EXTRA_CFLAGS += -fno-pic

EXTRA_CFLAGS += -DGPU_DRV_SRC_ROOT=${AQROOT}

ifneq ($(USE_MULTI_GPU), )
    EXTRA_CFLAGS += -DgcdMULTI_GPU=$(USE_MULTI_GPU)
endif

OBJS := $(OS_KERNEL_DIR)/gc_hal_kernel_device.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_linux.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_math.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_os.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_sysfs.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_sysfs_test.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_debugfs.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_allocator.o \

ifneq ($(CONFIG_IOMMU_SUPPORT),)
OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_iommu.o
endif

ifneq ($(PLATFORM),)
OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_probe.o
OBJS += $(OS_KERNEL_DIR)/platform/$(PLATFORM).o
else
OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_driver.o
endif

OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_plat.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_common.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_adir.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_eden.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_plat_pxa988.o \

ifeq ($(USE_GPU_FREQ), 1)

OBJS += $(GPUFREQ_DIR)/gpufreq.o \
        $(GPUFREQ_DIR)/gpufreq-pxa988.o \
        $(GPUFREQ_DIR)/gpufreq-eden.o \
        $(GPUFREQ_DIR)/gpufreq_ondemand.o \
        $(GPUFREQ_DIR)/gpufreq_conservative.o \
        $(GPUFREQ_DIR)/gpufreq_userspace.o \
        $(GPUFREQ_DIR)/gpufreq_performance.o \
        $(GPUFREQ_DIR)/gpufreq_powersave.o

EXTRA_CFLAGS += -DUSE_GPU_FREQ=1

else

EXTRA_CFLAGS += -DUSE_GPU_FREQ=0

endif


OBJS += $(HAL_KERNEL_DIR)/gc_hal_kernel.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_command.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_db.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_debug.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_event.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_heap.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_mmu.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_video_memory.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_power.o

OBJS += $(ARCH_KERNEL_DIR)/gc_hal_kernel_context.o \
        $(ARCH_KERNEL_DIR)/gc_hal_kernel_hardware.o

ifeq ($(VIVANTE_ENABLE_3D), 1)
OBJS += $(ARCH_KERNEL_DIR)/gc_hal_kernel_recorder.o
endif

ifeq ($(VIVANTE_ENABLE_VG), 1)
OBJS +=\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_vg.o\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_command_vg.o\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_interrupt_vg.o\
          $(HAL_KERNEL_DIR)/gc_hal_kernel_mmu_vg.o\
          $(ARCH_VG_KERNEL_DIR)/gc_hal_kernel_hardware_command_vg.o\
          $(ARCH_VG_KERNEL_DIR)/gc_hal_kernel_hardware_vg.o
endif

ifneq ($(CONFIG_SYNC),)
EXTRA_CFLAGS += -Idrivers/staging/android

OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_sync.o
endif

ifeq ($(SECURITY), 1)
OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_security_channel.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_security.o
endif

ifneq ($(CUSTOMER_ALLOCATOR_OBJS),)
OBJS += $(CUSTOMER_ALLOCATOR_OBJS)
endif

#ifeq ($(KERNELRELEASE), )

#.PHONY: all clean install

# Define targets.
#all:
#	@mkdir -p $(DRIVER_OUT_DIR)
#	@$(MAKE) V=$(V) ARCH=$(ARCH_TYPE) -C $(KERNEL_DIR) SUBDIRS=`pwd` modules

#clean:
#	@rm -rf $(OBJS)
#	@rm -rf $(DRIVER_OUT_DIR)
#	@rm -rf modules.order Module.symvers
#	@find $(AQROOT) -name ".gc_*.cmd" | xargs rm -f
#	@find $(OS_KERNEL_DIR) -name ".gc_*.cmd" | xargs rm -f
#	@find $(HAL_KERNEL_DIR) -name ".gc_*.cmd" | xargs rm -f
#	@find $(GPUFREQ_DIR) -name ".gpufreq*.cmd" | xargs rm -f

#install: all
#	@mkdir -p $(SDK_DIR)/drivers
#	@cp $(DRIVER_OUT_DIR)/$(MODULE_NAME).ko $(SDK_DIR)/drivers

#else


EXTRA_CFLAGS += -DLINUX -DDRIVER

ifeq ($(FLAREON),1)
EXTRA_CFLAGS += -DFLAREON
endif

ifeq ($(DEBUG), 1)
EXTRA_CFLAGS += -DDBG=1 -DDEBUG -D_DEBUG
else
EXTRA_CFLAGS += -DDBG=0
endif

ifeq ($(NO_DMA_COHERENT), 1)
EXTRA_CFLAGS += -DNO_DMA_COHERENT
endif

ifeq ($(CONFIG_DOVE_GPU), 1)
EXTRA_CFLAGS += -DCONFIG_DOVE_GPU=1
endif

ifneq ($(USE_PLATFORM_DRIVER), 0)
EXTRA_CFLAGS += -DUSE_PLATFORM_DRIVER=1
else
EXTRA_CFLAGS += -DUSE_PLATFORM_DRIVER=0
endif

ifeq ($(USE_PROFILER), 1)
EXTRA_CFLAGS += -DVIVANTE_PROFILER=1
EXTRA_CFLAGS += -DVIVANTE_PROFILER_CONTEXT=1
else
EXTRA_CFLAGS += -DVIVANTE_PROFILER=0
EXTRA_CFLAGS += -DVIVANTE_PROFILER_CONTEXT=0
endif

ifeq ($(ANDROID), 1)
EXTRA_CFLAGS += -DANDROID=1
endif

ifeq ($(ENABLE_GPU_CLOCK_BY_DRIVER), 1)
EXTRA_CFLAGS += -DENABLE_GPU_CLOCK_BY_DRIVER=1
else
EXTRA_CFLAGS += -DENABLE_GPU_CLOCK_BY_DRIVER=0
endif

ifeq ($(USE_NEW_LINUX_SIGNAL), 1)
EXTRA_CFLAGS += -DUSE_NEW_LINUX_SIGNAL=1
else
EXTRA_CFLAGS += -DUSE_NEW_LINUX_SIGNAL=0
endif

ifeq ($(FORCE_ALL_VIDEO_MEMORY_CACHED), 1)
EXTRA_CFLAGS += -DgcdPAGED_MEMORY_CACHEABLE=1
else
EXTRA_CFLAGS += -DgcdPAGED_MEMORY_CACHEABLE=0
endif

ifeq ($(NONPAGED_MEMORY_CACHEABLE), 1)
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_CACHEABLE=1
else
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_CACHEABLE=0
endif

ifeq ($(NONPAGED_MEMORY_BUFFERABLE), 1)
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_BUFFERABLE=1
else
EXTRA_CFLAGS += -DgcdNONPAGED_MEMORY_BUFFERABLE=0
endif

ifeq ($(CACHE_FUNCTION_UNIMPLEMENTED), 1)
EXTRA_CFLAGS += -DgcdCACHE_FUNCTION_UNIMPLEMENTED=1
else
EXTRA_CFLAGS += -DgcdCACHE_FUNCTION_UNIMPLEMENTED=0
endif

ifeq ($(CONFIG_SMP), y)
EXTRA_CFLAGS += -DgcdSMP=1
else
EXTRA_CFLAGS += -DgcdSMP=0
endif

ifeq ($(VIVANTE_ENABLE_3D),0)
EXTRA_CFLAGS += -DgcdENABLE_3D=0
else
EXTRA_CFLAGS += -DgcdENABLE_3D=1
endif

ifeq ($(VIVANTE_ENABLE_2D),0)
EXTRA_CFLAGS += -DgcdENABLE_2D=0
else
EXTRA_CFLAGS += -DgcdENABLE_2D=1
endif

ifeq ($(VIVANTE_ENABLE_VG),0)
EXTRA_CFLAGS += -DgcdENABLE_VG=0
else
EXTRA_CFLAGS += -DgcdENABLE_VG=1
endif

ifeq ($(ENABLE_OUTER_CACHE_PATCH), 1)
EXTRA_CFLAGS += -DgcdENABLE_OUTER_CACHE_PATCH=1
else
EXTRA_CFLAGS += -DgcdENABLE_OUTER_CACHE_PATCH=0
endif

ifeq ($(USE_BANK_ALIGNMENT), 1)
    EXTRA_CFLAGS += -DgcdENABLE_BANK_ALIGNMENT=1
    ifneq ($(BANK_BIT_START), 0)
	        ifneq ($(BANK_BIT_END), 0)
	            EXTRA_CFLAGS += -DgcdBANK_BIT_START=$(BANK_BIT_START)
	            EXTRA_CFLAGS += -DgcdBANK_BIT_END=$(BANK_BIT_END)
	        endif
    endif

    ifneq ($(BANK_CHANNEL_BIT), 0)
        EXTRA_CFLAGS += -DgcdBANK_CHANNEL_BIT=$(BANK_CHANNEL_BIT)
    endif
endif

ifeq ($(FPGA_BUILD), 1)
EXTRA_CFLAGS += -DgcdFPGA_BUILD=1
else
EXTRA_CFLAGS += -DgcdFPGA_BUILD=0
endif

ifeq ($(SECURITY), 1)
EXTRA_CFLAGS += -DgcdSECURITY=1
endif

EXTRA_CFLAGS += -I$(AQROOT)/hal/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel
ifeq ($(USE_ARCH_REG), 1)
    EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/arch_reg
else
    EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/arch
endif
EXTRA_CFLAGS += -I$(AQARCH)/cmodel/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/os/linux/kernel
EXTRA_CFLAGS += -I$(AQROOT)/$(ALLOCATOR_ARRAY_H_LOCATION)

ifeq ($(VIVANTE_ENABLE_VG), 1)
    ifeq ($(USE_ARCH_REG), 1)
        EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/archvg_reg
    else
        EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel/archvg
    endif
endif

obj-$(CONFIG_MRVL_GPU_GALCORE) = galcore.o

galcore-objs  = $(OBJS)

#endif
