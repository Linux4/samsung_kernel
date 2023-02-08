/**
@file		link_device_memory.h
@brief		header file for all types of memory interface media
@date		2014/02/05
@author		Hankook Jang (hankook.jang@samsung.com)
*/

/*
 * Copyright (C) 2010 Samsung Electronics.
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

#ifndef __MODEM_LINK_DEVICE_MEMORY_H__
#define __MODEM_LINK_DEVICE_MEMORY_H__

#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/netdevice.h>

#include "modem_prj.h"
#include "link_device_memory_config.h"
#include "link_device_memory_define.h"
#include "include/circ_queue.h"

#ifdef GROUP_MEM_TYPE
/**
@addtogroup group_mem_type
@{
*/

/**
@brief		check the type of a memory interface medium
@retval true	if @b @@type indicates a type of shared memory media
@retval false	otherwise
*/
static inline bool mem_type_shmem(enum mem_iface_type type)
{
	return (type & MEM_SHMEM_TYPE_MASK) ? true : false;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_INTERRUPT
/**
@addtogroup group_mem_link_interrupt
@{
*/

static inline bool int_valid(u16 x)
{
	return (x & MASK_INT_VALID) ? true : false;
}

static inline u16 mask2int(u16 mask)
{
	return mask | MASK_INT_VALID;
}

/**
@remark		This must be invoked after validation with int_valid().
*/
static inline bool cmd_valid(u16 x)
{
	return (x & MASK_CMD_VALID) ? true : false;
}

static inline u16 int2cmd(u16 x)
{
	return x & MASK_CMD_FIELD;
}

static inline u16 cmd2int(u16 cmd)
{
	return mask2int(cmd | MASK_CMD_VALID);
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_IPC_DEVICE
/**
@addtogroup group_mem_ipc_device
@{
*/

/**
@brief		get a circ_queue (CQ) instance in an IPC device

@param dev	the pointer to a mem_ipc_device instance
@param dir	the direction of communication (TX or RX)

@return		the pointer to the @b @@dir circ_queue (CQ) instance in @b @@dev
*/
static inline struct circ_queue *cq(struct mem_ipc_device *dev,
				    enum direction dir)
{
	return (dir == TX) ? &dev->txq : &dev->rxq;
}

/**
@brief		get the value of the @e head (in) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e head pointer in the TXQ of @b @@dev
*/
static inline unsigned int get_txq_head(struct mem_ipc_device *dev)
{
	return get_head(&dev->txq);
}

/**
@brief		set the value of the @e head (in) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance
@param in	the value to be written to the @e head pointer in the TXQ of
		@b @@dev
*/
static inline void set_txq_head(struct mem_ipc_device *dev, unsigned int in)
{
	set_head(&dev->txq, in);
}

/**
@brief		get the value of the @e tail (out) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e tail pointer in the TXQ of @b @@dev

@remark		It is useless for AP to read the tail pointer in a TX queue
		twice to verify whether or not the value in the pointer is
		valid, because it can already have been updated by CP after the
		first access from AP.
*/
static inline unsigned int get_txq_tail(struct mem_ipc_device *dev)
{
	return get_tail(&dev->txq);
}

/**
@brief		set the value of the @e tail (out) pointer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance
@param out	the value to be written to the @e tail pointer in the TXQ of
		@b @@dev
*/
static inline void set_txq_tail(struct mem_ipc_device *dev, unsigned int out)
{
	set_tail(&dev->txq, out);
}

/**
@brief		get the start address of the data buffer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the start address of the data buffer in the TXQ of @b @@dev
*/
static inline char *get_txq_buff(struct mem_ipc_device *dev)
{
	return get_buff(&dev->txq);
}

/**
@brief		get the size of the data buffer in a circular TXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the size of the data buffer in the TXQ of @e @@dev
*/
static inline unsigned int get_txq_buff_size(struct mem_ipc_device *dev)
{
	return get_size(&dev->txq);
}

/**
@brief		get the value of the @e head (in) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e head pointer in the RXQ of @b @@dev

@remark		It is useless for AP to read the head pointer in an RX queue
		twice to verify whether or not the value in the pointer is
		valid, because it can already have been updated by CP after the
		first access from AP.
*/
static inline unsigned int get_rxq_head(struct mem_ipc_device *dev)
{
	return get_head(&dev->rxq);
}

/**
@brief		set the value of the @e head (in) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance
@param in	the value to be written to the @e head pointer in the RXQ of
		@b @@dev
*/
static inline void set_rxq_head(struct mem_ipc_device *dev, unsigned int in)
{
	set_head(&dev->rxq, in);
}

/**
@brief		get the value of the @e tail (out) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the value of the @e tail pointer in the RXQ of @b @@dev
*/
static inline unsigned int get_rxq_tail(struct mem_ipc_device *dev)
{
	return get_tail(&dev->rxq);
}

/**
@brief		set the value of the @e tail (out) pointer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance
@param out	the value to be written to the @e tail pointer of the RXQ of
		@b @@dev
*/
static inline void set_rxq_tail(struct mem_ipc_device *dev, unsigned int out)
{
	set_tail(&dev->rxq, out);
}

/**
@brief		get the start address of the data buffer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the start address of the data buffer in the RXQ of @b @@dev
*/
static inline char *get_rxq_buff(struct mem_ipc_device *dev)
{
	return get_buff(&dev->rxq);
}

/**
@brief		get the size of the data buffer in a circular RXQ

@param dev	the pointer to a mem_ipc_device instance

@return		the size of the data buffer in the RXQ of @b @@dev
*/
static inline unsigned int get_rxq_buff_size(struct mem_ipc_device *dev)
{
	return get_size(&dev->rxq);
}

/**
@brief		get the MSG_SEND mask value for an IPC device

@param dev	the pointer to a mem_ipc_device instance

@return		the MSG_SEND mask of @b @@dev
*/
static inline u16 msg_mask(struct mem_ipc_device *dev)
{
	return dev->msg_mask;
}

/**
@brief		get the REQ_ACK mask value for an IPC device

@param dev	the pointer to a mem_ipc_device instance

@return		the REQ_ACK mask of @b @@dev
*/
static inline u16 req_ack_mask(struct mem_ipc_device *dev)
{
	return dev->req_ack_mask;
}

/**
@brief		get the RES_ACK mask value for an IPC device

@param dev	the pointer to a mem_ipc_device instance

@return		the RES_ACK mask of @b @@dev
*/
static inline u16 res_ack_mask(struct mem_ipc_device *dev)
{
	return dev->res_ack_mask;
}

/**
@brief		check whether or not @b @@val matches up with the REQ_ACK mask
		of an IPC device

@param dev	the pointer to a mem_ipc_device instance
@param val	the value to be checked

@return		"true" if @b @@val matches up with the REQ_ACK mask of @b @@dev
*/
static inline bool req_ack_valid(struct mem_ipc_device *dev, u16 val)
{
	if (!cmd_valid(val) && (val & req_ack_mask(dev)))
		return true;
	else
		return false;
}

/**
@brief		check whether or not @b @@val matches up with the RES_ACK mask
		of an IPC device

@param dev	the pointer to a mem_ipc_device instance
@param val	the value to be checked

@return		"true" if @b @@val matches up with the RES_ACK mask of @b @@dev
*/
static inline bool res_ack_valid(struct mem_ipc_device *dev, u16 val)
{
	if (!cmd_valid(val) && (val & res_ack_mask(dev)))
		return true;
	else
		return false;
}

/**
@brief		check whether or not an RXQ is empty

@param dev	the pointer to a mem_ipc_device instance

@return		"true" if the RXQ of @b @@dev is empty, i.e. HEAD == TAIL
*/
static inline bool rxq_empty(struct mem_ipc_device *dev)
{
	u32 head;
	u32 tail;
	unsigned long flags;

	spin_lock_irqsave(&dev->rxq.lock, flags);

	head = get_rxq_head(dev);
	tail = get_rxq_tail(dev);

	spin_unlock_irqrestore(&dev->rxq.lock, flags);

	return circ_empty(head, tail);
}

/**
@brief		check whether or not an TXQ is empty

@param dev	the pointer to a mem_ipc_device instance

@return		"true" if the TXQ of @b @@dev is empty, i.e. HEAD == TAIL
*/
static inline bool txq_empty(struct mem_ipc_device *dev)
{
	u32 head;
	u32 tail;
	unsigned long flags;

	spin_lock_irqsave(&dev->txq.lock, flags);

	head = get_txq_head(dev);
	tail = get_txq_tail(dev);

	spin_unlock_irqrestore(&dev->txq.lock, flags);

	return circ_empty(head, tail);
}

/**
@brief		get the ID of the IPC device for a SIPC channel ID

@param ch	the channel ID

@return		the ID of the IPC device for @b @@ch
*/
static inline enum dev_format dev_id(enum sipc_ch_id ch)
{
	return sipc5_fmt_ch(ch) ? IPC_FMT : IPC_RAW;
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_DEVICE
/**
@addtogroup group_mem_link_device
@{
*/

static inline unsigned int get_magic(struct mem_link_device *mld)
{
	return ioread32(mld->magic);
}

static inline unsigned int get_access(struct mem_link_device *mld)
{
	return ioread32(mld->access);
}

static inline void set_magic(struct mem_link_device *mld, unsigned int val)
{
	iowrite32(val, mld->magic);
}

static inline void set_access(struct mem_link_device *mld, unsigned int val)
{
	iowrite32(val, mld->access);
}

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SNAPSHOT
/**
@addtogroup group_mem_link_snapshot
@{
*/

int msb_init(void);

struct mst_buff *msb_alloc(void);
void msb_free(struct mst_buff *msb);

void msb_queue_head_init(struct mst_buff_head *list);
void msb_queue_tail(struct mst_buff_head *list, struct mst_buff *msb);
void msb_queue_head(struct mst_buff_head *list, struct mst_buff *msb);

struct mst_buff *msb_dequeue(struct mst_buff_head *list);

void msb_queue_purge(struct mst_buff_head *list);

/**
@brief		take a snapshot of the current status of a memory interface

Takes a snapshot of current @b @@dir memory status

@param mld	the pointer to a mem_link_device instance
@param dir	the direction of a communication (TX or RX)

@return		the pointer to a mem_snapshot instance
*/
struct mst_buff *mem_take_snapshot(struct mem_link_device *mld,
				   enum direction dir);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_INTERRUPT
/**
@addtogroup group_mem_link_interrupt
@{
*/

static inline void send_ipc_irq(struct mem_link_device *mld, u16 val)
{
	if (likely(mld->send_ap2cp_irq))
		mld->send_ap2cp_irq(mld, val);
}

void mem_irq_handler(struct mem_link_device *mld, struct mst_buff *msb);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_SETUP
/**
@addtogroup group_mem_link_setup
@{
*/

struct mem_link_device *mem_create_link_device(enum mem_iface_type type,
					       struct modem_data *modem);

int mem_setup_boot_map(struct mem_link_device *mld, unsigned long start,
		       unsigned long size);

int mem_setup_ipc_map(struct mem_link_device *mld, unsigned long start,
		      unsigned long size);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_COMMAND
/**
@addtogroup group_mem_link_command
@{
*/

int mem_reset_ipc_link(struct mem_link_device *mld);
void mem_cmd_handler(struct mem_link_device *mld, u16 cmd);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_FLOW_CONTROL
/**
@addtogroup group_mem_flow_control
@{
*/

void start_tx_flow_ctrl(struct mem_link_device *mld,
			struct mem_ipc_device *dev);
void stop_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev);
int under_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev);
int check_tx_flow_ctrl(struct mem_link_device *mld, struct mem_ipc_device *dev);

void send_req_ack(struct mem_link_device *mld, struct mem_ipc_device *dev);
void recv_res_ack(struct mem_link_device *mld, struct mem_ipc_device *dev,
		  struct mem_snapshot *mst);

void recv_req_ack(struct mem_link_device *mld, struct mem_ipc_device *dev,
		  struct mem_snapshot *mst);
void send_res_ack(struct mem_link_device *mld, struct mem_ipc_device *dev);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_CP_CRASH
/**
@addtogroup group_mem_cp_crash
@{
*/

void mem_handle_cp_crash(struct mem_link_device *mld, enum modem_state state);
void mem_forced_cp_crash(struct mem_link_device *mld);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef GROUP_MEM_LINK_DEBUG
/**
@addtogroup group_mem_link_debug
@{
*/

void print_req_ack(struct mem_link_device *mld, struct mem_snapshot *mst,
		   struct mem_ipc_device *dev, enum direction dir);
void print_res_ack(struct mem_link_device *mld, struct mem_snapshot *mst,
		   struct mem_ipc_device *dev, enum direction dir);

void print_mem_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst);
void print_dev_snapshot(struct mem_link_device *mld, struct mem_snapshot *mst,
			struct mem_ipc_device *dev);

void save_mem_dump(struct mem_link_device *mld);
void mem_dump_work(struct work_struct *ws);

/**
@}
*/
#endif

/*============================================================================*/

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
extern int is_rndis_use(void);
#endif

#endif

