#  (C) Copyright 2011 Marvell International Ltd.
#             All Rights Reserved
#  This software file (the "File") is distributed by Marvell International Ltd. 
#  under the terms of the GNU General Public License Version 2, June 1991 (the "License"). 
#  You may use, redistribute and/or modify this File in accordance with the terms and 
#  conditions of the License, a copy of which is available along with the File in the 
#  license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place, 
#  Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
#  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES 
#  OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.  
#  The License provides additional details about this warranty disclaimer.

THIS_MAKEFILE=$(abspath $(lastword $(MAKEFILE_LIST)))
CURR_DIR=$(shell dirname $(THIS_MAKEFILE))

#if VFP is configed, include the vfp directory
ifeq ($(CONFIG_VFPv3), y)
VFPDIR := $(PXALINUX)/arch/arm/vfp/

ifeq ($(SOC_TYPE), MMP3)
EXTRA_CFLAGS += -I$(VFPDIR)
endif

ifeq ($(SOC_TYPE), ARMADA610)
EXTRA_CFLAGS += -I$(VFPDIR)
endif
endif

ifeq ($(CONFIG_VFP), y)
VFPDIR := $(PXALINUX)/arch/arm/vfp/

ifeq ($(SOC_TYPE), MMP3)
EXTRA_CFLAGS += -I$(VFPDIR)
endif

ifeq ($(SOC_TYPE), ARMADA610)
EXTRA_CFLAGS += -I$(VFPDIR)
endif
endif
                    
ifneq ($(KERNELRELEASE),)
        obj-y += mpdc_tp.o

        mpdc_tp-objs := oshooks.o \
                    tp_drv.o funchooks.o

ifeq ($(CONFIG_PIXIU),m)
        mpdc_tp-objs += ../common/common.o
        mpdc_tp-objs += ../common/ring_buffer.o
endif

		mpdc_tp-objs += ../common/ufunc_hook.o ../common/displaced_decode.o


# set flags for PJ4	
ifeq ($(CPU_TYPE), PJ4)
	EXTRA_CFLAGS += -DPX_CPU_PJ4
ifeq ($(SOC_TYPE), ARMADA610)
	mpdc_tp-objs += ../common/getTargetInfo_pj4.o
endif
endif

# set flags for A7	
ifeq ($(CPU_TYPE), A7)
	EXTRA_CFLAGS += -DPX_CPU_A7

ifeq ($(SOC_TYPE), PXA1088)
	EXTRA_CFLAGS += -DPX_SOC_PXA1088 -fno-pic
endif

ifeq ($(SOC_TYPE), EDEN)
	EXTRA_CFLAGS += -DPX_SOC_PXAEDEN -fno-pic
endif

endif

# set flags for A9	
ifeq ($(CPU_TYPE), A9)
	EXTRA_CFLAGS += -DPX_CPU_A9
#	mpdc_tp-objs += ../common/pmu_a9.o
#	mpdc_tp-objs += a9/isr.o a9/timer.o

ifeq ($(SOC_TYPE), PXA988)
	EXTRA_CFLAGS += -DPX_SOC_PXA988 -fno-pic
endif

ifeq ($(SOC_TYPE), OMAP)
	EXTRA_CFLAGS += -DPX_SOC_OMAP
endif
ifeq ($(SOC_TYPE), TEGRA2)
	EXTRA_CFLAGS += -DPX_SOC_TEGRA2
endif

ifeq ($(SOC_TYPE), NEVO)
	EXTRA_CFLAGS += -DPX_SOC_NEVO
endif

endif


# set flags for PJ4B	
ifeq ($(CPU_TYPE), PJ4B)
	EXTRA_CFLAGS += -DPX_CPU_PJ4B
	
ifneq ($(SOC_TYPE), ARMADAXP)
	mpdc_tp-objs += ../common/getTargetInfo_pj4b.o
endif

ifeq ($(SOC_TYPE), MMP3)
	mpdc_tp-objs += ../common/getTargetInfo_pj4b_mmp3.o
endif
ifeq ($(SOC_TYPE), BG2)
	EXTRA_CFLAGS += -DPX_SOC_BG2
	mpdc_tp-objs += ../common/getTargetInfo_pj4b_bg2.o
endif
endif

# set flags for PJ1
ifeq ($(CPU_TYPE), PJ1)
	EXTRA_CFLAGS += -DPX_CPU_PJ1
ifeq ($(SOC_TYPE), PXA920)
	mpdc_tp-objs += ../common/getTargetInfo_pj1.o
endif
endif

	EXTRA_CFLAGS += -I$(CURR_DIR) -I$(CURR_DIR)/inc -I$(CURR_DIR)/../common -DSYS_CALL_TABLE_ADDRESS=$(SYS_CALL_TABLE_ADDRESS) 
	EXTRA_CFLAGS += -fno-omit-frame-pointer -mapcs -mno-sched-prolog -fno-schedule-insns -fno-schedule-insns2 -fno-toplevel-reorder
else
ifneq ($(MAKERULES),)
include $(MAKERULES)
endif
	KDIR := $(PXALINUX)
	PWD := $(shell pwd)
all:
	@export SYS_CALL_TABLE_ADDRESS=0;\
	if [ -f $(KDIR)/System.map ]; then \
                SYS_CALL_TABLE_ADDRESS=0x`awk '/sys_call_table/{print $$1}' $(KDIR)/System.map`; \
        fi;\
	if [ -f $(KDIR)/vmlinux ]; then \
                SYS_CALL_TABLE_ADDRESS=0x`nm $(KDIR)/vmlinux | awk '/sys_call_table/{print $$1}'`; \
        fi; \
        if [ "$$SYS_CALL_TABLE_ADDRESS" = "0" ]; then \
                echo "warning: couldn't find sys_call_table address in file System.map and vmlinux!";\
        fi; \
        $(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CROSS_COMPILE=$$CROSS_COMPILE ARCH=arm modules
	$(CROSS_COMPILE)strip -g *.ko
	$(hide)mkdir -p ../out
	$(hide)cp *.ko ../out

.PHONY: clean
clean:
	@rm -f *.o *.ko *.mod.c \.*.cmd
	@rm -rf .tmp_versions
	@rm -f *.ko
	@rm -f Module.symvers modules.order
	@rm -f $(PWD)/../common/*.o $(PWD)/../common/*.mod.c $(PWD)/../common/.*.o.cmd
endif

.NOTPARALLEL:

