#  (C) Copyright 2010 Marvell International Ltd.
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

ifeq ($(CONFIG_CPU_PXA1L88), y)
CPU_TYPE = A7
SOC_TYPE = PXA1088
endif

ifeq ($(CONFIG_CPU_EDEN), y)
CPU_TYPE = A7
SOC_TYPE = EDEN
endif

ifeq ($(CONFIG_CPU_PXA1088), y)
CPU_TYPE = A7
SOC_TYPE = PXA1088
endif

ifeq ($(CONFIG_CPU_PXA988), y)
CPU_TYPE = A9
SOC_TYPE = PXA988 
endif

PXALINUX = $(KERNELDIR)
#CROSS_COMPILE = $(CROSS_COMPILE)  

# add this to make the default value of CONFIG_PIXIU is m in pixiu build enviroment
# CONFIG_PIXIU ?= m 
# export CONFIG_PIXIU

export CPU_TYPE SOC_TYPE
export PXALINUX CROSS_COMPILE  

obj-y += CMProfiler/

obj-y += CSSProfiler/

obj-y += HSProfiler/

obj-y += TPProfiler/

.NOTPARALLEL:

