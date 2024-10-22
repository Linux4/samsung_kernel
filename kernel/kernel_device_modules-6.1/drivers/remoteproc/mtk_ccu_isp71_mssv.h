/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __RPOC_MTK_CCU_IPS7_H
#define __RPOC_MTK_CCU_IPS7_H

#include <linux/kernel.h>
#include <linux/remoteproc.h>
#include <linux/wait.h>
#include <linux/types.h>

#include "mtk_ccu_common_mssv.h"

#define MTK_CCU_CORE_PMEM_BASE  (0x00000000)
#define MTK_CCU_CORE_DMEM_BASE  (0x00020000)
#define MTK_CCU_CORE_DMEM_BASE_ISP7SP  (0x00040000)
#define MTK_CCU_PMEM_BASE  (0x1B000000)
#define MTK_CCU_DMEM_BASE  (0x1B020000)
#define MTK_CCU_PMEM_SIZE  (0x20000)
#define MTK_CCU_DMEM_SIZE  (0x20000)
#define MTK_CCU_ISR_LOG_SIZE  (0x400)
#define MTK_CCU_LOG_SIZE  (0x800)
#define MTK_CCU_CACHE_SIZE  (0x100000)
#define MTK_CCU_CACHE_BASE (0x40000000)
#define MTK_CCU_SHARED_BUF_OFFSET 0 //at DCCM start
#define MTK_CCU_BASE_MASK  (0xFFF00000)

#define MTK_CCU_REG_RESET    (0x0)
#define MTK_CCU_HW_RESET_BIT (0x000d0100)
#define MTK_CCU_HW_RESET_BIT_ISP7SP (0x000d0300)
#define MTK_CCU_REG_CTRL     (0x0c)
#define MTK_CCU_REG_AXI_REMAP  (0x24)
#define MTK_CCU_REG_CORE_CTRL  (0x28)
#define MTK_CCU_RUN_BIT      (0x00000010)
#define MTK_CCU_REG_CORE_STATUS     (0x28)
#define MTK_CCU_INT_TRG         (0x8010)
#define MTK_CCU_INT_TRG_ISP7SP  (0x80C0)
#define MTK_CCU_INT_CLR         (0x5C)
#define MTK_CCU_INT_CLR_EXCH    (0x80A4)
#define MTK_CCU_INT_ST          (0x60)
#define MTK_CCU_MON_ST          (0x78)
#define HALT_MASK_RV33          (0x30)
#define HALT_MASK_RV55          (0x100)

#define SPM_BASE              (0x1C001000)
#define SPM_SIZE              (0x1000)
#define CCU_SLEEP_SRAM_CON    (0xF54)
#define CCU_SLEEP_SRAM_PDN    (0x1 << 8)

#define MTK_CCU_SPARE_REG00   (0x0110)
#define MTK_CCU_SPARE_REG01   (0x0114)
#define MTK_CCU_SPARE_REG02   (0x0118)
#define MTK_CCU_SPARE_REG03   (0x011C)
#define MTK_CCU_SPARE_REG04   (0x0120)
#define MTK_CCU_SPARE_REG05   (0x0124)
#define MTK_CCU_SPARE_REG06   (0x0128)
#define MTK_CCU_SPARE_REG07   (0x012C)
#define MTK_CCU_SPARE_REG08   (0x0130)
#define MTK_CCU_SPARE_REG09   (0x0134)
#define MTK_CCU_SPARE_REG10   (0x0138)
#define MTK_CCU_SPARE_REG11   (0x013C)
#define MTK_CCU_SPARE_REG12   (0x0140)
#define MTK_CCU_SPARE_REG13   (0x0144)
#define MTK_CCU_SPARE_REG14   (0x0148)
#define MTK_CCU_SPARE_REG15   (0x014C)
#define MTK_CCU_SPARE_REG16   (0x0150)
#define MTK_CCU_SPARE_REG17   (0x0154)
#define MTK_CCU_SPARE_REG18   (0x0158)
#define MTK_CCU_SPARE_REG19   (0x015C)
#define MTK_CCU_SPARE_REG20   (0x0160)
#define MTK_CCU_SPARE_REG21   (0x0164)
#define MTK_CCU_SPARE_REG22   (0x0168)
#define MTK_CCU_SPARE_REG23   (0x016C)
#define MTK_CCU_SPARE_REG24   (0x0170)
#define MTK_CCU_SPARE_REG25   (0x0174)
#define MTK_CCU_SPARE_REG26   (0x0178)
#define MTK_CCU_SPARE_REG27   (0x017C)
#define MTK_CCU_SPARE_REG28   (0x0180)
#define MTK_CCU_SPARE_REG29   (0x0184)
#define MTK_CCU_SPARE_REG30   (0x0188)
#define MTK_CCU_SPARE_REG31   (0x018C)

#define CCU_STATUS_INIT_DONE              0xffff0000
#define CCU_STATUS_INIT_DONE_2            0xffff00a5
#define CCU_GO_TO_LOAD                    0x10AD10AD
#define CCU_GO_TO_RUN                     0x17172ACE
#define CCU_GO_TO_STOP                    0x8181DEAD
#define CCU_DMA_DOMAIN                    0x30340C0C

#define STRESS_TEST    0xFFFFFF03

#define ERR_INFRA        1
#define ERR_SW_SW        2
#define ERR_LW_LW        3
#define ERR_SW_LW        4
#define ERR_SRAM_RW      5
#define ERR_WDMA_TOUT    6
#define ERR_WDMA_DRAM    7
#define ERR_RDMA_TOUT    8
#define ERR_RDMA_DRAM    9
#define ERR_CCU_MAX      10

#endif //__RPOC_MTK_CCU_IPS7_H
