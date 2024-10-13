/*
 * Copyright (C) 2014-2019, Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MCU_IPC_H__
#define __MCU_IPC_H__

/* Shared register with 64 * 32 words */
#define MAX_MBOX_NUM	64

enum mcu_ipc_region {
	MCU_CP,
	MCU_MAX,
};

extern int mbox_request_irq(enum mcu_ipc_region id, u32 int_num, irq_handler_t handler, void *data);
extern int mbox_unregister_irq(enum mcu_ipc_region id, u32 int_num, irq_handler_t handler);
extern int mbox_enable_irq(enum mcu_ipc_region id, u32 int_num);
extern int mbox_check_irq(enum mcu_ipc_region id, u32 int_num);
extern int mbox_disable_irq(enum mcu_ipc_region id, u32 int_num);
extern void mbox_set_interrupt(enum mcu_ipc_region id, u32 int_num);
extern void mcu_ipc_send_command(enum mcu_ipc_region id, u32 int_num, u16 cmd);
extern u32 mbox_get_value(enum mcu_ipc_region id, u32 mbx_num);
extern void mbox_set_value(enum mcu_ipc_region id, u32 mbx_num, u32 msg);
extern void mbox_update_value(enum mcu_ipc_region id, u32 mbx_num, u32 msg, u32 mask, u32 pos);
extern u32 mbox_extract_value(enum mcu_ipc_region id, u32 mbx_num, u32 mask, u32 pos);
extern void mbox_sw_reset(enum mcu_ipc_region id);
extern void mcu_ipc_reg_dump(enum mcu_ipc_region id);
extern int mcu_ipc_set_affinity(enum mcu_ipc_region id, int affinity);
#endif /* __MCU_IPC_H__ */
