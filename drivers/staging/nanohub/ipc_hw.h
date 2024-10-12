/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Common IPC Driver Mailbox HW Control
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *
 */

#ifndef _MAILBOX_CHUB_IPC_HW_H
#define _MAILBOX_CHUB_IPC_HW_H

#include "ipc_type.h"

/*  mailbox Registers */
#define REG_MAILBOX_MCUCTL (0x000)
#define REG_MAILBOX_INTGR0 (0x008)
#define REG_MAILBOX_INTCR0 (0x00C)
#define REG_MAILBOX_INTMR0 (0x010)
#define REG_MAILBOX_INTSR0 (0x014)
#define REG_MAILBOX_INTMSR0 (0x018)
#define REG_MAILBOX_INTGR1 (0x01C)
#define REG_MAILBOX_INTCR1 (0x020)
#define REG_MAILBOX_INTMR1 (0x024)
#define REG_MAILBOX_INTSR1 (0x028)
#define REG_MAILBOX_INTMSR1 (0x02C)

#define IPC_HW_IRQ_MAX (16)

enum ipc_hw_dir {
	IPC_DST,
	IPC_SRC,
};

struct ipc_owner_ctrl {
	enum ipc_hw_dir src;
	void *base;
};

#define IRQ_HW_MAX (16)
#define MB0_BIT_OFFSET (IRQ_HW_MAX)

enum ipc_mb_id {
	IPC_SRC_MB0,
	IPC_DST_MB1,
};

int ipc_hw_start_bit(enum ipc_mb_id which_mb);
void ipc_hw_set_mcuctrl(void *base, unsigned int val);
void ipc_hw_write_shared_reg(void *base, unsigned int val, int num);
unsigned int ipc_hw_read_shared_reg(void *base, int num);

unsigned int ipc_hw_read_int_gen_reg(void *base, enum ipc_mb_id which_mb);
void ipc_hw_gen_interrupt(void *base, enum ipc_mb_id which_mb, int irq);
void ipc_hw_clear_all_int_pend_reg(void *base, enum ipc_mb_id which_mb);
void ipc_hw_clear_int_pend_reg(void *base, enum ipc_mb_id which_mb, int irq);
unsigned int ipc_hw_read_int_status_reg_all(void *base,
					    enum ipc_mb_id which_mb);
unsigned int ipc_hw_read_int_status_reg(void *base, enum ipc_mb_id which_mb,
					int irq);
#endif
