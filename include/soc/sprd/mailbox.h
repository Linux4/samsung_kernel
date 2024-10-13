/* mailbox.h */

#ifndef MAILBOX_H
#define MAILBOX_H

#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kfifo.h>


/*reg offset*/
#define MBOX_ID 0x00
#define MBOX_MSG_L 0x04
#define MBOX_MSG_H 0x08
#define MBOX_TRI 0x0c
#define MBOX_FIFO_RST 0x10
#define MBOX_FIFO_STS 0x14
#define MBOX_IRQ_STS 0x18
#define MBOX_IRQ_MSK 0x1c
#define MBOX_IRQ_CLR_BIT 0x1


#define MBOX_LOCK 0x20

#define FIFO_BLOCK_STS (0xfe)
#define FIFO_FULL_STS_MASK (0x4)

#define MBOX_UNLOCK_KEY 0x5a5a5a5a

#define MBOX_FIFO_SIZE 0x40
#define MBOX_MAX_CORE_CNT 8
#define MAX_SMSG_BAK   64

//#define MBOX_GET_FIFO_WR_PTR(val) ((val >> 16)& 0x3f)
//#define MBOX_GET_FIFO_RD_PTR(val) ((val >> 24)& 0x3f)
//#define MBOX_ENABLE_INDICATE_AND_FULL_IRQ  (~(BIT_4 | BIT_0))


#ifdef CONFIG_SPRD_MAILBOX_FIFO
typedef irqreturn_t (*MBOX_FUNCALL)(void* ptr, void *private);
int mbox_register_irq_handle(u8 target_id, MBOX_FUNCALL irq_handler,
	void *priv_data);
#else
int mbox_register_irq_handle(u8 target_id, irq_handler_t irq_handler,
	void *priv_data);
#endif

int mbox_unregister_irq_handle(u8 target_id);
int mbox_raw_sent(u8 target_id, u64 msg);
#ifdef CONFIG_SPRD_MAILBOX_FIFO
u32 mbox_core_fifo_full(int core_id);
#endif
#endif /* MAILBOX_H */
