/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * MCU IPC driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef __EXYNOS_MCU_IPC_H__
#define __EXYNOS_MCU_IPC_H__

struct mcu_ipc_irq_handler {
	void *data;
	void (*handler)(void *);
};

struct mcu_ipc_drv_data {
	void __iomem *ioaddr;
	u32 registered_irq;
	struct device *mcu_ipc_dev;
	struct mcu_ipc_irq_handler hd[16];
};

#define MAX_MBOX_NUM	64

extern int mbox_request_irq(u32 int_num, void (*handler)(void *), void *data);
extern int mbox_free_irq(u32 int_num, void *data);
extern void mbox_set_interrupt(u32 int_num);
extern void mbox_send_command(u32 int_num, u16 cmd);
extern u32 mbox_get_value(u32 mbx_num);
extern void mbox_set_value(u32 mbx_num, u32 msg);

#endif
