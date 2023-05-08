/* mailbox.h */

#ifndef MAILBOX_H
#define MAILBOX_H

#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kfifo.h>
enum {
	AP_CA7 = 0x0,
	ARM7,
	CP0_DSP0,
	CP0_DSP1,
	CP0_ARM0,
	CP0_ARM1,
	CP1_DSP,
	CP1_CA5,
	MBOX_NR,
};

#define MBOX_CORE_GGE CP0_ARM0
#define MBOX_CORE_LTE CP1_CA5

int mbox_register_irq_handle(u8 target_id, irq_handler_t irq_handler, void *priv_data);
int mbox_unregister_irq_handle(u8 target_id);
int mbox_raw_sent(u8 target_id, u64 msg);

#endif /* MAILBOX_H */
