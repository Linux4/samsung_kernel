/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/sprdmux.h>
#include <linux/kfifo.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>


#include <spi_gpio_ctrl.h>
#include <ipc_spi.h>

#define DRV_NAME "ipc-spi"

#define  IPC_READ_DISABLE                    0x01
#define  IPC_WRITE_DISABLE                  0x02

#define IPC_RX_ACK_NONE 0
#define IPC_RX_ACK_HOLD 1
#define IPC_RX_ACK_SEND 2

#define MAX_MIPC_TX_FRAME_NUM    5
#define MAX_MIPC_TX_FRAME_SIZE    (16*1024)
#define MAX_MIPC_RX_FRAME_SIZE    (64*1024)
#define MAX_MIPC_RX_CACHE_SIZE   MAX_MIPC_RX_FRAME_SIZE*2

#define IPC_DRIVER_NAME "IPC_SPI"
#define IPC_DBG(f, x...) 	pr_debug(IPC_DRIVER_NAME " [%s()]: " f, __func__,## x)

static struct ipc_spi_dev *ipc_dev;

static __cacheline_aligned_in_smp u8 receive_buf[MAX_RECEIVER_SIZE];
static u8 send_buf[SETUP_PACKET_SIZE];
static u8 dummy_buf[4] = {0x5a, 0x5a, 0x0,0x0};
static struct ipc_transfer_frame tx_transfer_frame[MAX_MIPC_TX_FRAME_NUM];

static u8 read_buf[MAX_RECEIVER_SIZE];
static u8 write_buf[MAX_MIPC_TX_FRAME_SIZE];

struct ipc_spi_dev *g_ipc_spi_dev = NULL;

static int ipc_freelist_Init(struct frame_list* frame_list_ptr)
{
	u32 t = 0;
	struct ipc_transfer_frame* frame_ptr = NULL;

	INIT_LIST_HEAD(&frame_list_ptr->frame_list_head);
	mutex_init(&frame_list_ptr->list_mutex);
	frame_list_ptr->counter = 0;

	for(t = 0; t < MAX_MIPC_TX_FRAME_NUM;  t++) {
		frame_ptr = &tx_transfer_frame[t];
		frame_ptr->buf_size =  MAX_MIPC_TX_FRAME_SIZE;
		frame_ptr->buf_ptr = (u8*)kmalloc(frame_ptr->buf_size , GFP_KERNEL);
		if(!frame_ptr->buf_ptr)
			return -ENOMEM;
		frame_ptr->pos = 0;
		frame_ptr->flag = 0;
		list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
		frame_list_ptr->counter++;
	}
	return 0;

}

static void ipc_transferinit(struct ipc_transfer* transfer_ptr)
{
	INIT_LIST_HEAD(&transfer_ptr->frame_fifo);
	mutex_init(&transfer_ptr->transfer_mutex);
	transfer_ptr->counter = 0;
	transfer_ptr->cur_frame_ptr = NULL;
}


static int ipc_frame_init(struct ipc_spi_dev *dev)
{
	int ret;
	ret = kfifo_alloc(&dev->rx_fifo,MAX_MIPC_RX_CACHE_SIZE, GFP_KERNEL);
	if(ret) {
		printk(KERN_ERR "kfifo_alloc failed \n");
		return ret;
	}
	ret = ipc_freelist_Init(&dev->tx_free_lst);
	if(ret) {
		printk(KERN_ERR "ipc free list init failed \n");
		return ret;
	}
	ipc_transferinit(&dev->tx_transfer);
	return 0;
}

/*static*/ int ipc_IsTransferFifoEmpty(struct ipc_transfer* transfer_ptr)
{
	u32  ret = 0;
	mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
	ret = list_empty(&transfer_ptr->frame_fifo);
	mutex_unlock(&transfer_ptr->transfer_mutex);/*set  lock */
	return ret;
}

static void ipc_AddFrameToTxTransferFifo(struct ipc_transfer_frame* frame_ptr, struct ipc_transfer*  transfer_ptr)
{
	list_add_tail(&frame_ptr->link,  &transfer_ptr->frame_fifo);
}


/*static*/  u32 ipc_FlushTxTransfer(struct ipc_transfer* transfer_ptr)
{
	u32 ret = 0;
	mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
	do {
		if((transfer_ptr->counter == 0) || !transfer_ptr->cur_frame_ptr ||
		(transfer_ptr->cur_frame_ptr->pos == sizeof( struct data_packet_header))) {
			ret = 1;
			break;
		}
		ipc_AddFrameToTxTransferFifo(transfer_ptr->cur_frame_ptr, transfer_ptr);
		transfer_ptr->cur_frame_ptr = NULL;
	}while(0);
	mutex_unlock(&transfer_ptr->transfer_mutex);/*set	lock */
	return  ret;
}

/*static*/ u32 ipc_GetFrameFromTxTransfer(struct ipc_transfer* transfer_ptr,  struct ipc_transfer_frame* *out_frame_ptr)
{
	struct ipc_transfer_frame* frame_ptr = NULL;
	mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */
	if(!list_empty(&transfer_ptr->frame_fifo)) {
		frame_ptr = list_entry(transfer_ptr->frame_fifo.next, struct ipc_transfer_frame, link);
		list_del(&frame_ptr->link);
	}
	mutex_unlock(&transfer_ptr->transfer_mutex);/*set  lock */

	if(!frame_ptr) {
		return 1;
	}

	*out_frame_ptr = frame_ptr;
	return 0;
}

static int spi_read_write(struct spi_device *spi, void *rbuf,
		const void *wbuf, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf     = rbuf,
			.tx_buf		= wbuf,
			.len		= len,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}


static u16 ipc_calculate_len(u16 length)
{
	if(length % 32)
		return (length/32 + 1) * 32;
	else
		return length;
}

/*just use for calulate time when debug */
static void ipc_recordaptime(struct ipc_spi_dev* dev)
{
	dev->ap_info[dev->ap_infocnt%100].time = cpu_clock(0);
	dev->ap_info[dev->ap_infocnt%100].status = dev->task_status;
	dev->ap_infocnt++;
}

static size_t ipc_read_write_data(struct ipc_spi_dev* dev, u8* rbuf, u32 rlen, u8* wbuf, u32 wlen)
{
	size_t ret = 0;
	//wait_event(dev->wait, dev->transfer_status == TRANSFER_IDLE);
	rlen = ipc_calculate_len(rlen);
	wlen = ipc_calculate_len(wlen);
	ap2cp_enable();
	dev->tx_irqennum++;
	dev->task_status = 5;
	ipc_recordaptime(dev);
	wait_event(dev->wait, (dev->rx_ctl == CP2AP_RASING) || !dev->ipc_enable);
	if(!dev->ipc_enable)
		return -1;
	dev->task_status = 6;
	dev->rx_ctl = CP2AP_NOIRQ;
	if(wlen == 0)
		ret = spi_read_write(dev->spi,rbuf, dummy_buf, rlen);
	else
		ret = spi_read_write(dev->spi,rbuf, wbuf, (rlen > wlen)? rlen : wlen);
	ap2cp_disable();
	dev->tx_irqdisnum++;
	dev->task_status = 7;
	wait_event(dev->wait, dev->rx_ctl || !dev->ipc_enable);
	if(!dev->ipc_enable)
		return -1;
	dev->task_status = 8;
	dev->rwnum++;
	return ret;

}

static bool ipc_rxfifo_full(struct ipc_spi_dev* dev)
{
	bool ret = false;
	int rx_avail_size = kfifo_avail(&dev->rx_fifo);
	if(dev->rx_status == SETUP_STATUS) {
		if(dev->rx_needdata) {
			if(dev->rx_size > rx_avail_size) {
				ret = true;
			}
		} else {
			if(rx_avail_size < SETUP_PACKET_SIZE - sizeof(struct setup_packet_header)) {
				ret = true;
			}
		}
	} else if(dev->rx_status == DATA_STATUS) {
		if(rx_avail_size < SETUP_PACKET_SIZE - sizeof(struct setup_packet_header)) {
				ret = true;
		}
	}
	return ret;
}

/* FLOW CONTROL */
static bool ipc_cansendack(struct ipc_spi_dev* dev)
{
	bool ret = true;
	spin_lock(&dev->rx_fifo_lock);
	if(ipc_rxfifo_full(dev)) {
		dev->rx_ack_hold = IPC_RX_ACK_HOLD;
		ret = false;
	} else {
		dev->rx_ack_hold = IPC_RX_ACK_SEND;
	}
	spin_unlock(&dev->rx_fifo_lock);
	return ret;
}

static bool ipc_needsendack(struct ipc_spi_dev* dev)
{
	return (dev->rx_status == SETUP_STATUS) || (dev->rx_status == DATA_STATUS);
}

static bool ipc_needsendnak(struct ipc_spi_dev* dev)
{
	return (dev->rx_status == ERROR_STATUS);
}

/*static bool ipc_needsendacksuc(struct ipc_spi_dev* dev)
{
	return (dev->rx_status == ACK_SUCCESS_STATUS);
} */

static void ipc_ack(u8* buf, u16* len)
{
	struct ack_packet ack;
	ack.magic = ACK_MAGIC;
	ack.type = TYPE_ACK;
	memcpy(buf, &ack, sizeof(struct ack_packet));
	*len = sizeof(struct ack_packet);
}

static void ipc_nak(u8* buf, u16* len)
{
	struct ack_packet ack;
	ack.magic = ACK_MAGIC;
	ack.type = TYPE_NAK;
	memcpy(buf, &ack, sizeof(struct ack_packet));
	*len = sizeof(struct ack_packet);
}

static void ipc_acksuc(u8* buf, u16* len)
{
	struct ack_packet ack;
	ack.magic = ACK_MAGIC;
	ack.type = TYPE_SUCCESS;
	memcpy(buf, &ack, sizeof(struct ack_packet));
	*len = sizeof(struct ack_packet);
}

static u16 ipc_checksum(const unsigned char *src, int len)
{
	unsigned long sum =0;
	int size1 = len >> 1;
	int size2 = len & 0x01;
	unsigned short *data = (unsigned short *)src;

	if(len){
		while (size1-- > 0)
		{
				sum += *data++;
		}

		if (size2)
		{
				sum +=src[len - 1];
		}
	}
	sum = (sum >> 16)+(sum & 0xFFFF);
	sum += (sum >> 16);
	return ~sum;
}


/*static*/ void ipc_setup(struct ipc_spi_dev* dev, u8* buf, u16* len, packet_type *type)
{
	struct setup_packet_header *header = (struct setup_packet_header*)buf;
	struct ipc_transfer_frame*  frame_ptr = NULL;
	if(dev->tx_transfer.counter) {
		if(dev->tx_status == NAK_STATUS)
			frame_ptr = dev->curframe;
		else {
			if(ipc_IsTransferFifoEmpty(&(dev->tx_transfer))) {
				if(ipc_FlushTxTransfer(&(dev->tx_transfer))) {
					*len = 0; /* no data to send */
					return;
				}
			}

			ipc_GetFrameFromTxTransfer(&dev->tx_transfer, &frame_ptr);
			dev->curframe = frame_ptr;
		}
		if(frame_ptr->pos - sizeof(struct data_packet_header) > SETUP_PACKET_SIZE - sizeof(struct setup_packet_header))	{
			header->magic = SETUP_MAGIC;
			header->type = TYPE_SETUP_WITHOUT_DATA;
			header->length = frame_ptr->pos - sizeof(struct data_packet_header);
			if(dev->tx_status == NAK_STATUS)
				header->seqnum = dev->tx_seqnum -1;
			else
				header->seqnum = dev->tx_seqnum++;
			header->checksum = ipc_checksum((const unsigned char *)&header->type, 8);
			*len = sizeof(struct setup_packet_header);
			*type = TYPE_SETUP_WITHOUT_DATA;
			dev->tx_needdata = true;
		} else {
			header->magic = SETUP_MAGIC;
			header->type = TYPE_SETUP_WITH_DATA;
			header->length = frame_ptr->pos - sizeof(struct data_packet_header);
			if(dev->tx_status == NAK_STATUS)
				header->seqnum = dev->tx_seqnum -1;
			else
				header->seqnum = dev->tx_seqnum++;
			memcpy(buf + sizeof(struct setup_packet_header), frame_ptr->buf_ptr + sizeof(struct data_packet_header), header->length);
			header->checksum = ipc_checksum((const unsigned char *)&header->type, 8 + header->length);
			*len = sizeof(struct setup_packet_header) + header->length;
			*type = TYPE_SETUP_WITH_DATA;
			dev->tx_needdata = false;
		}

	} else {
		*len = 0;
	}
}

static void ipc_data(u16 length, u8* buf, u16* len)
{
	u32 offset = sizeof(struct data_packet_header);
	struct data_packet_header header;
	header.magic = DATA_MAGIC;
	header.checksum = ipc_checksum(&buf[offset], length - offset);
	memcpy(buf, &header, sizeof(header));
	*len = length;
}

static void ipc_sendcheck(struct ipc_spi_dev* dev)
{
	struct ipc_transfer *transfer_ptr = &dev->tx_transfer;
	mutex_lock(&transfer_ptr->transfer_mutex);
	if(transfer_ptr->counter > 0)
		dev->bneedsnd = true;
	mutex_unlock(&transfer_ptr->transfer_mutex);
}


/*static*/ void ipc_prepare_send(struct ipc_spi_dev* dev, u8* buf,
		u16* len, packet_type *type)
{
	if(ipc_needsendack(dev) && ipc_cansendack(dev)) {
		*type = TYPE_ACK;
		return ipc_ack(buf, len);
	}
	if(ipc_needsendnak(dev)) {
		*type = TYPE_NAK;
		return ipc_nak(buf, len);
	}
	/*if(ipc_needsendacksuc(dev))
		return ipc_acksuc(buf, len);*/
	if(dev->tx_status == IDLE_STATUS || dev->tx_status == NAK_STATUS
		|| (dev->tx_status == ACK_STATUS && !dev->tx_needdata)) {
		return ipc_setup(dev, buf, len, type);
	}
	if(dev->tx_status == ACK_STATUS && dev->tx_needdata) {
		*type = TYPE_DATA;
		return ipc_data(dev->curframe->pos, dev->curframe->buf_ptr, len);
	}
	if(dev->tx_status == SETUP_STATUS || dev->tx_status == DATA_STATUS) {
		*len = 0;
		return ;
	}

}

static void ipc_prepare_read(struct ipc_spi_dev* dev, u16* len)
{
	if(dev->rx_status == ACK_STATUS && dev->rx_needdata)
		*len = dev->rx_size + sizeof(struct data_packet_header);
	else
		*len = SETUP_PACKET_SIZE;
}


struct ipc_transfer_frame* ipc_AllocFrame(struct frame_list*  frame_list_ptr)
{
	struct ipc_transfer_frame *frame_ptr = NULL;

	if(!frame_list_ptr) {
		panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
	}

	mutex_lock(&frame_list_ptr->list_mutex);/*get  lock */
	if(!list_empty(&frame_list_ptr->frame_list_head)) {
		frame_ptr = list_entry(frame_list_ptr->frame_list_head.next, struct ipc_transfer_frame, link);
		list_del(&frame_ptr->link);
		frame_ptr->pos = sizeof(struct data_packet_header);
		frame_ptr->flag = 1;
		frame_list_ptr->counter--;
	}
	mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
	pr_debug("_AllocFrame:0x%X\r\n", (u32)frame_ptr);
	return  frame_ptr;
}

static int ipc_txfreelist_empty(struct ipc_spi_dev *dev)
{
	struct frame_list *frame_lst = &dev->tx_free_lst;
	return list_empty(&frame_lst->frame_list_head);
}


static u32 ipc_AddDataToTxTransfer(struct ipc_spi_dev *dev, u8* data_ptr, u32 len)
{
	u32 ret = 0;
	u32 index = 0;
	u32 remain_size = 0;
	struct ipc_transfer_frame *frame_ptr = NULL;
	struct ipc_transfer_frame *new_frame_ptr = NULL;
	struct ipc_transfer *transfer_ptr = &dev->tx_transfer;

	IPC_DBG("Enter ipc_AddDataToTxTransfer %d \n", len);
	mutex_lock(&transfer_ptr->transfer_mutex);/*get  lock */

	do {
		frame_ptr = transfer_ptr->cur_frame_ptr;
		if(!frame_ptr) {
			frame_ptr =  ipc_AllocFrame(&dev->tx_free_lst);
			if(!frame_ptr) {
				break;
			}
		}
		remain_size = frame_ptr->buf_size - frame_ptr->pos;
		if(len > remain_size) {
			memcpy(&frame_ptr->buf_ptr[frame_ptr->pos], data_ptr + index, remain_size);
			frame_ptr->pos  = frame_ptr->buf_size;
			transfer_ptr->counter += remain_size;
			transfer_ptr->cur_frame_ptr = NULL;
			ipc_AddFrameToTxTransferFifo(frame_ptr, transfer_ptr);
			ret += remain_size;
			index += remain_size;
			len -= remain_size;
			new_frame_ptr =  ipc_AllocFrame(&dev->tx_free_lst);
			if(!new_frame_ptr) {
				break;
			}

			frame_ptr = new_frame_ptr;
			transfer_ptr->cur_frame_ptr = frame_ptr;
		} else {
			memcpy(&frame_ptr->buf_ptr[frame_ptr->pos], data_ptr + index, len);
			frame_ptr->pos  += len;
			transfer_ptr->counter += len;
			transfer_ptr->cur_frame_ptr = frame_ptr;
			ret += len;
			break;
		}
	}while(len > 0);

	mutex_unlock(&transfer_ptr->transfer_mutex);/*set	lock */
	IPC_DBG("Enter ipc_AddDataToTxTransfer %d - \n", ret);
	return ret;
}

static void ipc_FreeFrame(struct ipc_transfer_frame *frame_ptr, struct frame_list*  frame_list_ptr)
{
	if(!frame_list_ptr || !frame_ptr) {
		panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
	}
	pr_debug("_FreeFrame:0x%x\r\n", (u32)frame_ptr);
	mutex_lock(&frame_list_ptr->list_mutex);/*get	lock */
	frame_ptr->pos = 0;
	frame_ptr->flag = 0;
	list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
	frame_list_ptr->counter++;
	mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
}

static u32 ipc_DelDataFromTxTransfer(struct ipc_spi_dev *dev,
		struct ipc_transfer_frame *frame_ptr)
{
	struct ipc_transfer* transfer_ptr  = &dev->tx_transfer;
	mutex_lock(&transfer_ptr->transfer_mutex);
	transfer_ptr->counter -=  frame_ptr->pos - sizeof(struct data_packet_header);
	mutex_unlock(&transfer_ptr->transfer_mutex);
	ipc_FreeFrame(frame_ptr, &dev->tx_free_lst);
	wake_up(&dev->tx_frame_wait);
	return 0;
}

static u32 ipc_FreeAllTxTransferFrame(struct ipc_spi_dev *dev)
{
	struct ipc_transfer_frame *frame_ptr = NULL;
	while(!ipc_GetFrameFromTxTransfer(&dev->tx_transfer, &frame_ptr)) {
		ipc_DelDataFromTxTransfer(dev, frame_ptr);
	}
    return 0;
}

static void ipc_print_errdata(u8 *buf, u16 len)
{
	int i;
	for(i = 0; i < len;) {

		printk("data: 0x%x, 0x%x, 0x%x, 0x%x \n", buf[i], buf[i+1], buf[i+2], buf[i+3]);
		i+=4;
	}
}

static void ipc_process_rcvdata(struct ipc_spi_dev* dev, u8 *buf, u16 len)
{
	struct setup_packet_header *setup_header;
	u16 *magic = (u16*)buf;
	IPC_DBG("ipc_process_rcvdata 0x%x \n", *magic);
	if(SETUP_MAGIC == *magic) {
		setup_header = (struct setup_packet_header*)buf;
		if(setup_header->type == TYPE_SETUP_WITH_DATA) {
			if(setup_header->checksum == ipc_checksum((const unsigned char *)&setup_header->type, setup_header->length + 8)) {
				if(kfifo_avail(&dev->rx_fifo) < setup_header->length) {
					printk(KERN_ERR "ipc RX_FIFO is full \n");
					BUG_ON(1);
				}
				if(dev->rx_seqnum == setup_header->seqnum) {
					printk(KERN_WARNING "ipc receive the same packet, need to discard \n");
				} else {
					kfifo_in(&dev->rx_fifo, buf + sizeof(struct setup_packet_header), setup_header->length);
					wake_up_interruptible(&dev->rx_read_wait);
					dev->rx_seqnum = setup_header->seqnum;
				}
				dev->rx_needdata = false;
				dev->rx_status = SETUP_STATUS;
				ipc_cansendack(dev);
			} else {
				ipc_print_errdata(buf, len);
				dev->rx_status = ERROR_STATUS;
				dev->rx_ack_hold = IPC_RX_ACK_SEND;
			}
		} else if(setup_header->type == TYPE_SETUP_WITHOUT_DATA) {
			if(setup_header->checksum == ipc_checksum((const unsigned char *)&setup_header->type, 8)) {
				dev->rx_size = setup_header->length;
				if(dev->rx_seqnum == setup_header->seqnum) {
					printk(KERN_WARNING "ipc receive the same packet, need to discard11 \n");
					dev->rx_data_discard = true;
				}else {
					dev->rx_tmpnum = setup_header->seqnum;
					dev->rx_data_discard = false;
				}
				dev->rx_needdata = true;
				dev->rx_status = SETUP_STATUS;
				ipc_cansendack(dev);
			} else {
				ipc_print_errdata(buf, len);
				dev->rx_status = ERROR_STATUS;
				dev->rx_ack_hold = IPC_RX_ACK_SEND;
			}
		} else {
			ipc_print_errdata(buf, len);
			dev->rx_status = ERROR_STATUS;
			dev->rx_ack_hold = IPC_RX_ACK_SEND;
		}
	}else if(DATA_MAGIC == *magic) {
		struct data_packet_header *header = (struct data_packet_header*)buf;
		if(header->checksum == ipc_checksum(buf + sizeof(struct data_packet_header), dev->rx_size)) {
			if(!dev->rx_data_discard) {
				if(kfifo_avail(&dev->rx_fifo) < dev->rx_size) {
					printk(KERN_ERR "ipc RX_FIFO is full11 \n");
					BUG_ON(1);
				}
				kfifo_in(&dev->rx_fifo, buf + sizeof(struct data_packet_header), dev->rx_size);
				wake_up_interruptible(&dev->rx_read_wait);
				dev->rx_seqnum = dev->rx_tmpnum;
			}
			dev->rx_status = DATA_STATUS;
			ipc_cansendack(dev);
		} else {
			ipc_print_errdata(buf, len);
			dev->rx_status = ERROR_STATUS;
			dev->rx_ack_hold = IPC_RX_ACK_SEND;
		}
	} else if(ACK_MAGIC == *magic) {
		struct ack_packet *header = (struct ack_packet*)buf;
		if(header->type == TYPE_ACK) {
			if((dev->tx_status == DATA_STATUS) || ((dev->tx_status == SETUP_STATUS) && !dev->tx_needdata)) {
				ipc_DelDataFromTxTransfer(dev, dev->curframe);
				dev->curframe = NULL;
				ipc_sendcheck(dev);
				dev->tx_status = IDLE_STATUS;
				return ;
			} else if (dev->tx_status == SETUP_STATUS){
				printk(KERN_ERR "setup without data receive ack \n");
				ipc_sendcheck(dev);
				dev->tx_status = ACK_STATUS;
			} else
				printk(KERN_ERR "ipc in wrong status when receive ack %d\n", dev->tx_status);
		} else if(header->type == TYPE_NAK) {
			printk(KERN_WARNING "ipc receive nak status %d \n", dev->tx_status);
			dev->bneedsnd = true;
			dev->tx_status = NAK_STATUS;
		} else if(header->type == TYPE_SUCCESS) {
			ipc_sendcheck(dev);
			dev->tx_status = ACK_STATUS;
			dev->tx_needdata = false;
		} else {
			dev->bneedsnd = true;
			dev->tx_status = NAK_STATUS;
		}
	} else if(DUMMY_MAGIC == *magic) {
		IPC_DBG("ipc dump data just discard \n");
	} else {
		printk(KERN_ERR "ipc wrong data need to tell remote peer \n");
		printk(KERN_ERR "ipc tx_status: %d, rx_status %d \n", dev->tx_status, dev->rx_status);
		ipc_print_errdata(buf, len);
		if(dev->tx_status == SETUP_STATUS || dev->tx_status == DATA_STATUS) {
			/* when tx wait ack , then need to resend setup */
			dev->bneedsnd = true;
			dev->tx_status = NAK_STATUS;
		}
		if(dev->rx_status != SETUP_STATUS && dev->rx_status != DATA_STATUS) {
			dev->rx_status = ERROR_STATUS;
			dev->rx_ack_hold = IPC_RX_ACK_SEND;
		}
	}
}

static void ipc_process_send(struct ipc_spi_dev* dev, packet_type tx_type)
{
	if(tx_type == TYPE_SETUP_WITHOUT_DATA || tx_type == TYPE_SETUP_WITH_DATA) {
		dev->bneedsnd = false;
		dev->tx_status = SETUP_STATUS;
	}
	else if(tx_type == TYPE_ACK) {
		dev->rx_status = ACK_STATUS;
		dev->rx_ack_hold = IPC_RX_ACK_NONE;
	}
	else if(tx_type == TYPE_NAK || tx_type == TYPE_SUCCESS) {
		dev->rx_status = IDLE_STATUS;
		dev->rx_ack_hold = IPC_RX_ACK_NONE;
	}
	else if(tx_type == TYPE_DATA) {
		dev->bneedsnd = false;
		dev->tx_status = DATA_STATUS;
	}
}

static int mux_ipc_spi_read(char *buf, size_t  count)
{
	int ret = 0;
	bool bwake = false;
	struct ipc_spi_dev *dev = ipc_dev;

    wait_event_interruptible(dev->rx_read_wait, !kfifo_is_empty(&dev->rx_fifo)
		|| (dev->rwctrl & IPC_READ_DISABLE));

	if(dev->rwctrl & IPC_READ_DISABLE) {
		printk(KERN_ERR "read was disabled \n");
		return -1;
	}
    IPC_DBG("[mipc]mux_ipc_spi_read read len:%d\r\n", count);
	spin_lock(&dev->rx_fifo_lock);
    ret = kfifo_out(&dev->rx_fifo,buf,count);
	if(dev->rx_ack_hold == IPC_RX_ACK_HOLD) {
		if(!ipc_rxfifo_full(dev)) {
			dev->rx_ack_hold = IPC_RX_ACK_SEND;
			bwake = true;
		}
	}
	spin_unlock(&dev->rx_fifo_lock);
	if(bwake)
		wake_up(&dev->wait);
    return ret;
}

static int mux_ipc_spi_write(const char *buf, size_t  count)
{
	ssize_t ret = 0;
	struct ipc_spi_dev *dev = ipc_dev;

	if(dev->rwctrl & IPC_WRITE_DISABLE) {
		printk(KERN_ERR "write was disabled \n");
		return -1;
	}
	IPC_DBG("Enter mux_ipc_spi_write %d \n", count);
	ret = ipc_AddDataToTxTransfer(dev, (u8*)buf, count);
	if(!ret) {
		wait_event(dev->tx_frame_wait, !ipc_txfreelist_empty(dev));
	}

	/* if tx transfer is empty so need to wake up thread */
	if(dev->tx_transfer.counter == ret) {
		IPC_DBG("ipc write wake up \n");
		dev->bneedsnd = true;
		wake_up(&dev->wait);
	}
	IPC_DBG("Enter mux_ipc_spi_write end %d \n", ret);
	return ret;
}

int mux_ipc_spi_stop(int mode)
{
	struct ipc_spi_dev *dev = ipc_dev;
	if(mode & SPRDMUX_READ)
	{
		dev->rwctrl |=  IPC_READ_DISABLE;
	}

	if(mode & SPRDMUX_WRITE)
	{
		dev->rwctrl |=  IPC_WRITE_DISABLE;
	}

    wake_up_interruptible(&dev->rx_read_wait);

    return 0;
}


static struct sprdmux sprd_iomux = {
	.id		= SPRDMUX_ID_SPI,
	.io_read	= mux_ipc_spi_read,
	.io_write	= mux_ipc_spi_write,
	.io_stop  =  mux_ipc_spi_stop,
};

void  spi_ipc_enable(u8  is_enable)
{
	struct ipc_spi_dev *dev = ipc_dev;
	if(is_enable) {
		dev->rwctrl = 0;
		dev->ipc_enable = true;
	}
	else {
		ap2cp_disable();
		dev->ipc_enable = false;
	}
	wake_up(&dev->wait);
	IPC_DBG("[mipc]mux ipc enable:0x%x, status:0x%x\r\n", is_enable, dev->ipc_enable);
}

static void ipc_reset(struct ipc_spi_dev *dev)
{
	int i;
	dev->rx_ctl = false;
	dev->bneedrcv = false;
	dev->bneedsnd = false;
	dev->rx_status = IDLE_STATUS;
	dev->tx_status = IDLE_STATUS;
	dev->rx_size = SETUP_PACKET_SIZE;
	dev->tx_size = SETUP_PACKET_SIZE;
	dev->tx_seqnum = 0;
	dev->rx_seqnum = -1;
	dev->irq_num = 0;
	dev->rwnum = 0;
	dev->tx_irqennum = 0;
	dev->tx_irqdisnum = 0;
	dev->ap_infocnt = 0;
	for(i = 0; i < MAX_RECEIVER_SIZE; i++)
		receive_buf[i] = 0x11;
}


static int mux_ipc_thread(void *data)
{
	int rval = 0;
	struct ipc_spi_dev *dev = (struct ipc_spi_dev*)data;
	struct sched_param	 param = {.sched_priority = 30};

	IPC_DBG("mux_ipc_tx_thread");
	sched_setscheduler(current, SCHED_FIFO, &param);

	spi_hal_gpio_init();

	IPC_DBG("%s() spi channel opened %d\n", __func__, rval);
	spi_hal_gpio_irq_init(dev);
	dev->task_count = 0;
	dev->task_status = 0;

	while (!kthread_should_stop()) {
		dev->task_count++;
		dev->task_status = 1;
		wait_event(dev->wait, dev->ipc_enable);
		dev->task_status = 2;
		wait_event(dev->wait,  dev->bneedsnd || dev->bneedrcv
				|| (dev->rx_ack_hold == IPC_RX_ACK_SEND));
		dev->task_status = 3;
		ipc_recordaptime(dev);

		IPC_DBG("Enter mux_ipc_thread %d %d %d \n",
				dev->tx_transfer.counter, dev->bneedrcv, dev->rx_ack_hold);
		while(dev->bneedsnd || dev->bneedrcv || dev->rx_ack_hold == IPC_RX_ACK_SEND) {
			u16 tx_len = 0;
			u16 rx_len = 0;
			packet_type tx_type = TYPE_MAX;
			ipc_prepare_send(dev, send_buf, &tx_len, &tx_type);
			ipc_prepare_read(dev, &rx_len);
			IPC_DBG("ipc done prepare %d, %d, %d \n", tx_len, tx_type, rx_len);
			dev->task_status = 4;
			if(tx_type == TYPE_DATA) {
				rval = ipc_read_write_data(dev, receive_buf, rx_len, dev->curframe->buf_ptr, tx_len);
			} else
				rval = ipc_read_write_data(dev, receive_buf, rx_len, send_buf, tx_len);
			IPC_DBG("ipc_read_write_data rval %d \n", rval);
			if(rval < 0)
				break;
			dev->task_status = 9;
			ipc_process_rcvdata(dev, receive_buf, rx_len);
			dev->task_status = 10;
			ipc_process_send(dev, tx_type);
			dev->task_status = 11;
		}
		if(!dev->ipc_enable) {
			printk(KERN_ERR "ipc was disabled , there must something wrong! \n");
			ipc_reset(dev);
			kfifo_reset(&dev->rx_fifo);
			ipc_FreeAllTxTransferFrame(dev);
			if(dev->curframe)
				ipc_DelDataFromTxTransfer(dev, dev->curframe);
			ap2cp_disable();
		}
	}
	spi_hal_gpio_exit();
	dev->task_status = 12;

	return 0;
}

static int ipc_fopen(struct inode *inode, struct file *filp)
{
	struct ipc_spi_dev *dev = container_of(filp->private_data,
			struct ipc_spi_dev, miscdev);
	IPC_DBG("enter ipc_fopen \n");
	filp->private_data = dev;
	return 0;
}

static ssize_t ipc_fread(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	IPC_DBG("enter ipc_fread \n");
	ret = mux_ipc_spi_read(read_buf, count);
	if(ret > 0) {
		if (copy_to_user(buf, read_buf, ret)) {
			printk(KERN_ERR "ipc_fread copy data to user error !\n");
			return -EFAULT;
		}
	}
	*ppos += ret;
	return ret;
}

static ssize_t ipc_fwrite(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	IPC_DBG("enter ipc_fwrite \n");
	if (copy_from_user(write_buf, buf, count)) {
		return -EFAULT;
	}
	ret = mux_ipc_spi_write(write_buf, count);
	if(ret > 0)
		*ppos += ret;
	return ret;
}


static const struct file_operations ipc_fops = {
	.owner = THIS_MODULE,
	.open = ipc_fopen,
	.read = ipc_fread,
	.write = ipc_fwrite,
};


#if defined(CONFIG_DEBUG_FS)
static int ipc_debug_show(struct seq_file *m, void *private)
{
	u32 *sdata = NULL;
	struct ipc_spi_dev *dev = ipc_dev;
	seq_printf(m, "======================================================================\n");
	seq_printf(m, "ipc_enable: %d: rwctrl: %d\n", dev->ipc_enable, dev->rwctrl);
	seq_printf(m, "bneedrcv: %d: rx_needdata: %d\n", dev->bneedrcv, dev->rx_needdata);
	seq_printf(m, "rx_ctl: %d: rx_ack_hold: %d\n", dev->rx_ctl, dev->rx_ack_hold);
	seq_printf(m, "rx status: %d: rx_size: %d rx_seqnum: %d\n",
			dev->rx_status, dev->rx_size, dev->rx_seqnum);
	seq_printf(m, "tx status: %d: tx_seqnum: %d\n", dev->tx_status, dev->tx_seqnum);
	seq_printf(m, "tx_transfer.counter: %d: \n", dev->tx_transfer.counter);
	seq_printf(m, "tx_free_lst.counter: %d: \n", dev->tx_free_lst.counter);
	seq_printf(m, "bneedsnd: %d : tx_needdata: %d\n", dev->bneedsnd, dev->tx_needdata);
	seq_printf(m, "ap2cp_sts: %d: cp2ap_sts: %d\n", ap2cp_sts(), cp2ap_sts());
	seq_printf(m, "task_count: %d: task_status: %d\n", dev->task_count, dev->task_status);
	seq_printf(m, "irq_num: %d rwnum: %d\n", dev->irq_num, dev->rwnum);
	seq_printf(m, "tx_irqennum: %d tx_irqdisnum: %d\n", dev->tx_irqennum, dev->tx_irqdisnum);

	seq_printf(m, "======================================================================\n");

	//spi_read_write(dev->spi, receive_buf, send_buf, SETUP_PACKET_SIZE);

	seq_printf(m, "*******send data: *********\n");
	sdata = (u32*)send_buf;
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[0], sdata[1], sdata[2], sdata[3], sdata[4], sdata[5], sdata[6], sdata[7]);
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[8], sdata[9], sdata[10], sdata[11], sdata[12], sdata[13], sdata[14], sdata[15]);
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[16], sdata[17], sdata[18], sdata[19], sdata[20], sdata[21], sdata[22], sdata[23]);
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[24], sdata[25], sdata[26], sdata[27], sdata[28], sdata[29], sdata[30], sdata[31]);
	seq_printf(m, "*******receive data: *********\n");
	sdata = (u32*)receive_buf;
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[0], sdata[1], sdata[2], sdata[3], sdata[4], sdata[5], sdata[6], sdata[7]);
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[8], sdata[9], sdata[10], sdata[11], sdata[12], sdata[13], sdata[14], sdata[15]);
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[16], sdata[17], sdata[18], sdata[19], sdata[20], sdata[21], sdata[22], sdata[23]);
	seq_printf(m, "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			sdata[24], sdata[25], sdata[26], sdata[27], sdata[28], sdata[29], sdata[30], sdata[31]);

	seq_printf(m, "======================================================================\n");

	seq_printf(m, "will show mipc_thread stack in kernel log \n");
	show_stack(dev->task,NULL);

	return 0;
}

static int ipc_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, ipc_debug_show, inode->i_private);
}

static const struct file_operations ipc_debug_fops = {
	.open = ipc_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static u8 test_buf[128] = {0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,
	0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,
	0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,
	0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,
	0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,0x5a, 0x5a, 0x5a,0x5a,};

static int ipc_debug_show1(struct seq_file *m, void *private)
{
	u32 max ;
	u32 i;
	static u32 count = 0;
	struct ipc_spi_dev *dev = ipc_dev;
	if(count % 2) {
		seq_printf(m, "**no receiver just send buf **\n");
		spi_write(dev->spi, test_buf, 128);

	} else {
		seq_printf(m, "**spi read and write **\n");
		spi_read_write(dev->spi, receive_buf, test_buf, 128);
	}
	count++;
	seq_printf(m, "**********************************************************************\n");
	seq_printf(m, "show cp2ap irq time:\n");
	max = (dev->irq_num > 100) ? 100: dev->irq_num;
	for(i = 0; i < max; i++) {
		unsigned long long t;
		unsigned long nanosec_rem;

		t = dev->cp2ap[i].time;
		nanosec_rem = do_div(t, 1000000000);
		seq_printf(m, "[%5lu.%06lu], status: %d\n", (unsigned long) t, nanosec_rem / 1000,
				dev->cp2ap[i].status);
	}

	seq_printf(m, "**********************************************************************\n");
	seq_printf(m, "show ap2cp irq time:\n");
	max = (dev->ap_infocnt > 100) ? 100: dev->ap_infocnt;
	for(i = 0; i < max; i++) {
		unsigned long long t;
		unsigned long nanosec_rem;

		t = dev->ap_info[i].time;
		nanosec_rem = do_div(t, 1000000000);
		seq_printf(m, "[%5lu.%06lu], status: %d\n", (unsigned long) t, nanosec_rem / 1000,
				dev->ap_info[i].status);
	}

	seq_printf(m, "**********************************************************************\n");

	return 0;
}


static int ipc_debug_open1(struct inode *inode, struct file *file)
{
	return single_open(file, ipc_debug_show1, inode->i_private);
}

static const struct file_operations ipc_debug_fops1 = {
	.open = ipc_debug_open1,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ipc_init_debugfs(void)
{
	struct dentry *root = debugfs_create_dir("ipc-spi", NULL);
	if (!root)
		return -ENXIO;
	debugfs_create_file("status", S_IRUGO, (struct dentry *)root, NULL, &ipc_debug_fops);
	debugfs_create_file("timeinfo", S_IRUGO, (struct dentry *)root, NULL, &ipc_debug_fops1);
	return 0;
}

#endif /* CONFIG_DEBUG_FS */



static int __init ipc_spi_probe(struct spi_device *spi)
{
	struct ipc_spi_dev *dev;
	int ret = 0;
	int i;
	printk("Enter ipc_spi_probe ..\n");
	dev = kzalloc(sizeof(struct ipc_spi_dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto error_ret;
	}
	spi_set_drvdata(spi, dev);
	dev->spi = spi;

	ret = ipc_frame_init(dev);
	if(ret) {
		printk(KERN_ERR "frame init failed \n");
		goto error_free_dev;
	}
	init_waitqueue_head(&dev->wait);
	init_waitqueue_head(&dev->rx_read_wait);
	init_waitqueue_head(&dev->tx_frame_wait);
	spin_lock_init(&dev->rx_fifo_lock);
	ipc_reset(dev);

	dev->ipc_enable = true; /* need tobe change later false;*/

	/* register misc devices */
	dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	dev->miscdev.name = "ipc_spi";
	dev->miscdev.fops = &ipc_fops;
	dev->miscdev.parent = NULL;
	ret = misc_register(&dev->miscdev);
	if(ret) {
		printk(KERN_ERR "failed to register spi ipc miscdev!\n");
		goto error_free_dev;
	}

	#ifdef CONFIG_DEBUG_FS
	ret = ipc_init_debugfs();
	if(ret) {
		printk(KERN_ERR "init debugfs error %d !.\n", ret);
		goto error_unregister_dev;
	}
	#endif

	spi->max_speed_hz = 8000000;
	spi->mode = SPI_MODE_3;
	spi->bits_per_word = 32;
	spi_setup(spi);

	ipc_dev = dev;
	dev->task = kthread_create(mux_ipc_thread, (void*)dev, "mipc_thread");
	if (IS_ERR(dev->task)) {
		printk(KERN_ERR "mux_ipc_thread error!.\n");
		goto error_unregister_dev;
	}

	wake_up_process(dev->task);

	sprdmux_register(&sprd_iomux);

	return 0;
error_unregister_dev:
	misc_deregister(&dev->miscdev);

error_free_dev:
	kfifo_free(&dev->rx_fifo);
	for(i = 0; i < MAX_MIPC_TX_FRAME_NUM; i++) {
		if(tx_transfer_frame[i].buf_ptr)
			kfree(tx_transfer_frame[i].buf_ptr);
	}
	kfree(dev);

error_ret:
	return ret;
}

static int __exit ipc_spi_remove(struct spi_device *spi)
{
	int i;
	struct ipc_spi_dev *dev = dev_get_drvdata(&spi->dev);
	dev->rwctrl |= IPC_WRITE_DISABLE | IPC_READ_DISABLE;
	dev->ipc_enable = false;
	sprdmux_unregister(SPRDMUX_ID_SPI);
	kfifo_free(&dev->rx_fifo);
	for(i = 0; i < MAX_MIPC_TX_FRAME_NUM; i++) {
		if(tx_transfer_frame[i].buf_ptr)
			kfree(tx_transfer_frame[i].buf_ptr);
	}

	kfree(dev);
	return 0;
}

static struct spi_driver ipc_spi_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = ipc_spi_probe,
	.remove = __exit_p(ipc_spi_remove),
};
module_spi_driver(ipc_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhongping.tan<zhongping.tan@spreadtrum.com>");
MODULE_DESCRIPTION("MUX spi Driver");
