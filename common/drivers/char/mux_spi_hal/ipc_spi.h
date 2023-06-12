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



#define SETUP_PACKET_SIZE 128
#define MAX_RECEIVER_SIZE (16*1024)

#define SETUP_MAGIC 0x1a2b
#define DATA_MAGIC 0x3c4d
#define ACK_MAGIC 0x5e6f
#define DUMMY_MAGIC 0x5a5a

#define CP2AP_NOIRQ 0x0
#define CP2AP_FALLING 0x1
#define CP2AP_RASING 0x2

typedef enum {
	TYPE_SETUP_WITH_DATA = 0x1,
	TYPE_SETUP_WITHOUT_DATA,
	TYPE_ACK,
	TYPE_NAK,
	TYPE_SUCCESS, //if receive this ack , cancel the data you want to resend
	TYPE_DATA,
	TYPE_MAX
}packet_type;

struct setup_packet_header {
	u16 magic;
	u16 checksum;
	u16 type;
	u16 length; // raw data length
	u32 seqnum;
};

struct ack_packet {
	u16 magic;
	u16 type;
};

// data packet type like below

struct data_packet_header {
	u16 magic;
	u16 checksum;
};


enum ipc_status {
	IDLE_STATUS,
	SETUP_STATUS,
	ERROR_STATUS, /* only RECEIVE has this status */
	ACK_STATUS,
	NAK_STATUS,
	DATA_STATUS,
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
	struct list_head  link;
};


struct ipc_transfer
{
   struct mutex transfer_mutex;
   struct list_head frame_fifo;
   struct ipc_transfer_frame* cur_frame_ptr;
   u32 counter;
};
enum transfer_status {
	TRANSFER_IDLE,
	TRANSFERRING,
};

struct timer_info {
	unsigned long long time;
	u32 status;
};

struct ipc_spi_dev {
	bool bneedrcv;
	bool bneedsnd;
	bool ipc_enable;
	bool rx_needdata;
	bool rx_data_discard;
	u16 rx_ctl; // gpio control no irq: 0 , falling: 1, rasing: 2
	u16 rx_ack_hold; /* 0 idle, 1 hold ack, 2 need to send ack */
	u16 rwctrl; /* read write controlled by mux */
	u16 rx_status; // rx status
	u16 rx_size; // need to receive's data size
	u32 rx_tmpnum; /* templete number of packet */
	u32 rx_seqnum; /* received num */
	struct kfifo rx_fifo;
	struct frame_list tx_free_lst;
	struct ipc_transfer tx_transfer;
	struct ipc_transfer_frame *curframe;
	u16 tx_status;
	u16 tx_size;
	bool tx_needdata;
	u32 tx_seqnum;
	u32 transfer_status;
	wait_queue_head_t wait;
	wait_queue_head_t rx_read_wait;
	wait_queue_head_t tx_frame_wait; // tx frame wait
	spinlock_t		rx_fifo_lock;
	struct task_struct *task;
	struct spi_device *spi;
	struct miscdevice		miscdev;
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
