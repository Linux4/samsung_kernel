/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_HW_H_
#define APU_HW_H_

/* reviser register definition */
#define UP_NORMAL_DOMAIN_NS (0x0)
#define UP_PRI_DOMAIN_NS (0x4)
#define UP_IOMMU_CTRL (0X8)
#define UP_CORE0_VABASE0 (0xc)
#define UP_CORE0_MVABASE0 (0x10)
#define UP_CORE0_VABASE1 (0x14)
#define UP_CORE0_MVABASE1 (0x18)
#define USERFW_CTXT (0x1000)
#define SECUREFW_CTXT (0x1004)

/* md32_sysctrl register definition */
#define MD32_SYS_CTRL (0x0)
#define DBG_BUS_SEL (0x98)
#define APU_UP_SYS_DBG_EN (1UL << 16)
#define MD32_CLK_EN (0xb8)
#define UP_WAKE_HOST_MASK0 (0xbc)

/* apu_rcx_ao_ctrl register definition */
#define MD32_PRE_DEFINE (0x0)
#define MD32_BOOT_CTRL (0x4)
#define MD32_RUNSTALL (0x8)

/* apu_mbox register definition */
#define MBOX_HOST_CONFIG_ADDR (0x48)
#define MBOX_WKUP_CFG (0x80)
#define APU_SEMA_CTRL0 (0x800)
#define APU_SEMA_DATA0 (0x840)
#define APU_SEMA_MPU_CTRL00 (0x880)
#define APU_SEMA_CTRL1 (0x900)
#define APU_SEMA_DATA1 (0x940)
#define APU_SEMA_MPU_CTRL1 (0x980)

#define MBOX_RV_PWR_STA_FLG (0x50)
#define MBOX_RV_TIMESYNC_FLG (0x54)

/* apu_rpc register definition */
#define APU_RPC_STATUS_1 (0x34)
#define APU_RPC_INTF_PWR_RDY (0x44)

#endif /* APU_HW_H_ */
