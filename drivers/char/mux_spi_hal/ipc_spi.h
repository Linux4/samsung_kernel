/*
 *  linux/drivers/mmc/host/sdio_hal.h - Secure Digital Host Controller Interface driver
 *
 *  Copyright (C) 2005-2008 Pierre Ossman, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */
#ifndef __IPC_SPI_H
#define __IPC_SPI_H

#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>
#include <mach/modem_interface.h>


//#define SETUP_PACKET_SIZE (128)
#define MAX_RECEIVER_SIZE (8*1024)

//#define SETUP_MAGIC 0x1a2b
#define DATA_MAGIC 0x3c4d
#define ACK_MAGIC 0x5e6f
//#define SETUP_ACK_MAGIC 0x789a
#define DATA_ACK_MAGIC 0x9abc
#define DUMMY_MAGIC 0x5a5a

#define CP2AP_NOIRQ 0x0
#define CP2AP_FALLING 0x1
#define CP2AP_RASING 0x2

//#define REMOTE_ALIVE_STATUS 0x1
//#define REMOTE_ASSERT_STATUS 0x2

typedef enum {
	TYPE_ACK = 0x1,
	TYPE_NAK,
	TYPE_NAKALL, //if receive this ack , cancel the data you want to resend
	TYPE_DATA,
	TYPE_MAX
}packet_type;

struct ack_packet {
	u16 magic;
	u16 checksum;
	u16 type;
	u16 dummy;
	u32 seq_begin;
	u32 seq_end;
};

// data packet type like below

struct data_packet_header {
	u16 magic;
	u16 checksum;
	union {
			struct ack_packet ack;
		};
	u32 seqnum;
	u32 len;
};


enum ipc_status {
	IDLE_STATUS,
	ACK_STATUS,
	NAK_STATUS,
	NAKALL_STATUS,
	MAX_STATUS
};

struct frame_list {
	struct list_head   frame_list_head;
	struct mutex       list_mutex;
	u32    counter;
};

struct ipc_transfer_frame {
	u8*  buf_ptr;
	u32  buf_size;
	u16  pos;
	u16  flag;
	u32  status;
	u32  seq;
	struct list_head  link;
};


struct ipc_transfer
{
   struct mutex transfer_mutex;
   struct list_head frame_fifo;
   struct ipc_transfer_frame* cur_frame_ptr;
   u32 counter;
};

struct timer_info {
	unsigned long long time;
	u32 status;
};

struct ipc_spi_dev {
	bool bneedrcv;
	bool ipc_enable;
	bool bneedread;
	bool bneedsend;
	u16 rx_ctl; // gpio control no irq: 0 , falling: 1, rasing: 2
	u16 rwctrl; /* read write controlled by mux */
	u16 rx_status; // rx status
	u32 rx_seqnum; /* received num */
	u32 tx_seqnum;
	u32 remote_status;
	struct frame_list tx_free_lst;
	struct ipc_transfer tx_transfer;
	struct frame_list rx_free_lst;
	struct frame_list rx_recv_lst;
	struct ipc_transfer_frame* rx_curframe;
	struct ipc_transfer_frame *tx_curframe;
	wait_queue_head_t wait;
	wait_queue_head_t rx_read_wait;
	wait_queue_head_t tx_frame_wait; // tx frame wait
	struct task_struct *task;
	struct spi_device *spi;
	struct miscdevice		miscdev;
	struct wake_lock wake_lock;
	struct modemsts_chg modemsts;
#ifdef CONFIG_PM
	bool bsuspend;
#endif
	u32 irq_num; /* use for debug cp side irq num */
	u32 task_count; /* use for debug task run count*/
	u32 task_status; /* use for debug record the status of task */
	u32 rwnum;
	u32 tx_irqennum; /* use for debug enable gpio num */
	u32 tx_irqdisnum; /* usefor debug disable gpio num */
	u32 ap_infocnt;
	struct timer_info cp2ap[100];
	struct timer_info ap_info[100];
};




#endif /* __SDHCI_H */
