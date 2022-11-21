##############################################################################
#
#    Copyright (c) 2005 - 2012 by Vivante Corp.
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
GPU_TYPE ?= unified_reg

DEBUG := 0
ABI := aapcs-linux
ENABLE_ARM_L2_CACHE := 1
NO_DMA_COHERENT := 1
CONFIG_DOVE_GPU := 0
ANDROID := 1
ENABLE_GPU_CLOCK_BY_DRIVER := 1
BUILD_OPTION_USE_GPU_FREQ :=1
USE_GPU_FREQ :=${BUILD_OPTION_USE_GPU_FREQ}

AQROOT :=$(srctree)/drivers/marvell/graphics/galcore_4x
AQARCH :=$(AQROOT)/arch/${GPU_TYPE}

#
# Linux build file for kernel HAL driver.
#
ARCH_TYPE                         ?= arm
#SDK_DIR                           ?= $(AQROOT)/build/sdk
USE_3D_VG                         ?= 1
USE_POWER_MANAGEMENT              ?= 1
FORCE_ALL_VIDEO_MEMORY_CACHED     ?= 0
NONPAGED_MEMORY_CACHEABLE         ?= 0
NONPAGED_MEMORY_BUFFERABLE        ?= 1
CACHE_FUNCTION_UNIMPLEMENTED      ?= 0
VIVANTE_ENABLE_VG                 ?= 0
NO_USER_DIRECT_ACCESS_FROM_KERNEL ?= 1
VIVANTE_NO_3D                     ?= 0
ENABLE_OUTER_CACHE_PATCH          ?= 1
USE_BANK_ALIGNMENT                ?= 0
BANK_BIT_START                    ?= 0
BANK_BIT_END                      ?= 0
BANK_CHANNEL_BIT                  ?= 0

#DRIVER_OUT_DIR = hal/driver
#KERNEL_DIR ?= $(TOOL_DIR)/kernel

OS_KERNEL_DIR   := hal/os/linux/kernel
ARCH_KERNEL_DIR := arch/$(notdir $(AQARCH))/hal/kernel
ARCH_VG_KERNEL_DIR := arch/$(notdir $(AQVGARCH))/hal/kernel
HAL_KERNEL_DIR  := hal/kernel
GPUFREQ_DIR     := $(OS_KERNEL_DIR)/gpufreq

EXTRA_CFLAGS += -Werror
EXTRA_CFLAGS += -fno-pic

EXTRA_CFLAGS += -DGPU_DRV_SRC_ROOT=${AQROOT}

OBJS := $(OS_KERNEL_DIR)/gc_hal_kernel_device.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_driver.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_linux.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_math.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_os.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_sysfs.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_sysfs_test.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_debugfs.o

ifeq ($(USE_GPU_FREQ), 1)

OBJS += $(GPUFREQ_DIR)/gpufreq.o \
        $(GPUFREQ_DIR)/gpufreq-pxa988.o \
        $(GPUFREQ_DIR)/gpufreq_ondemand.o \
        $(GPUFREQ_DIR)/gpufreq_conservative.o \
        $(GPUFREQ_DIR)/gpufreq_userspace.o \
        $(GPUFREQ_DIR)/gpufreq_performance.o \
        $(GPUFREQ_DIR)/gpufreq_powersave.o
EXTRA_CFLAGS += -DUSE_GPU_FREQ=1
else
EXTRA_CFLAGS += -DUSE_GPU_FREQ=0
endif

ifeq ($(USE_3D_VG), 1)

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
OBJS += $(OS_KERNEL_DIR)/gc_hal_kernel_sync.o
endif

else

OBJS += $(HAL_KERNEL_DIR)/gc_hal_kernel.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_command.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_heap.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_interrupt.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_mmu.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_video_memory.o \
        $(OS_KERNEL_DIR)/gc_hal_kernel_debug.o \
        $(HAL_KERNEL_DIR)/gc_hal_kernel_power.o

OBJS += $(ARCH_KERNEL_DIR)/gc_hal_kernel_hardware.o \
        $(ARCH_KERNEL_DIR)/gc_hal_kernel_hardware_command.o

endif

#ifeq ($(KERNELRELEASE), )

#.PHONY: all clean install

## Define targets.
#all:
#	@mkdir -p $(DRIVER_OUT_DIR)
#	@make V=$(V) ARCH=$(ARCH_TYPE) -C $(KERNEL_DIR) SUBDIRS=`pwd` modules

#clean:
#	@rm -rf $(OBJS)
#	@rm -rf $(DRIVER_OUT_DIR)
#	@rm -rf modules.order Module.symvers
#	@find $(AQROOT) -name ".gc_*.cmd" | xargs rm -f

#install: all
#	@mkdir -p $(SDK_DIR)/drivers
#	@cp $(DRIVER_OUT_DIR)/galcore.ko $(SDK_DIR)/drivers

#else


EXTRA_CFLAGS += -DLINUX -DDRIVER

ifeq ($(ENUM_WORKAROUND), 1)
EXTRA_CFLAGS += -DENUM_WORKAROUND=1
else
EXTRA_CFLAGS += -DENUM_WORKAROUND=0
endif

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

ifeq ($(USE_POWER_MANAGEMENT), 1)
EXTRA_CFLAGS += -DgcdPOWER_MANAGEMENT=1
else
EXTRA_CFLAGS += -DgcdPOWER_MANAGEMENT=0
endif

ifneq ($(USE_PLATFORM_DRIVER), 0)
EXTRA_CFLAGS += -DUSE_PLATFORM_DRIVER=1
else
EXTRA_CFLAGS += -DUSE_PLATFORM_DRIVER=0
endif

#ifeq ($(USE_PROFILER), 1)
#EXTRA_CFLAGS += -DVIVANTE_PROFILER=1
#else
#EXTRA_CFLAGS += -DVIVANTE_PROFILER=0
#endif

ifeq ($(ANDROID), 1)
EXTRA_CFLAGS += -DANDROID=1
endif

ifeq ($(X11), 1)
EXTRA_CFLAGS += -DX11=1
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

ifeq ($(NO_USER_DIRECT_ACCESS_FROM_KERNEL), 1)
EXTRA_CFLAGS += -DNO_USER_DIRECT_ACCESS_FROM_KERNEL=1
else
EXTRA_CFLAGS += -DNO_USER_DIRECT_ACCESS_FROM_KERNEL=0
endif

ifeq ($(FORCE_ALL_VIDEO_MEMORY_CACHED), 1)
EXTRA_CFLAGS += -DgcdPAGED_MEMORY_CACHEABLE=1
else
EXTRA_CFLAGS += -DgcdPAGED_MEMORY_CACHEABLE=0
endif

ifeq ($(ENABLE_GPUTEX), 1)
CFLAGS += -I$(ANDROID_HOME)/vendor/marvell/generic/gpu-engine/tex/include
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

ifeq ($(VIVANTE_ENABLE_VG), 1)
EXTRA_CFLAGS += -DgcdENABLE_VG=1
else
EXTRA_CFLAGS += -DgcdENABLE_VG=0
endif

ifeq ($(CONFIG_SMP), y)
EXTRA_CFLAGS += -DgcdSMP=1
else
EXTRA_CFLAGS += -DgcdSMP=0
endif

ifeq ($(VIVANTE_NO_3D),1)
EXTRA_CFLAGS += -DVIVANTE_NO_3D
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

ifneq ($(CONFIG_SYNC),)
EXTRA_CFLAGS += -DgcdANDROID_NATIVE_FENCE_SYNC=0
endif

EXTRA_CFLAGS += -I$(AQROOT)/hal/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/kernel
EXTRA_CFLAGS += -I$(AQARCH)/hal/kernel
#EXTRA_CFLAGS += -I$(AQARCH)/cmodel/inc
EXTRA_CFLAGS += -I$(AQROOT)/hal/user
EXTRA_CFLAGS += -I$(AQROOT)/hal/os/linux/kernel

#$(warning $(AQROOT))
#$(warning $(AQARCH))
#$(warning $(EXTRA_CFLAGS))



ifeq ($(VIVANTE_ENABLE_VG), 1)
EXTRA_CFLAGS += -I$(AQVGARCH)/hal/kernel
endif

# kernel3.10+kk4.4 move ion.h/pxa_ion.h to drivers/staging folder
ifneq ($(srctree),)
EXTRA_CFLAGS += -I$(srctree)/drivers/staging/android/ion
endif

ifeq ($(CONFIG_MRVL_GPU_GALCORE),)
obj-m = galcore.o
else
obj-${CONFIG_MRVL_GPU_GALCORE} = galcore.o
endif

galcore-objs  = $(OBJS)

#endif
