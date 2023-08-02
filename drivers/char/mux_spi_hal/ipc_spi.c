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

#define MAX_MIPC_RX_FRAME_NUM    8
#define MAX_MIPC_TX_FRAME_NUM    5
#define MAX_MIPC_TX_FRAME_SIZE    (8*1024)
#define MAX_MIPC_RX_FRAME_SIZE    (8*1024)

#define IPC_DRIVER_NAME "IPC_SPI"
#define IPC_DBG(f, x...) 	pr_debug(IPC_DRIVER_NAME " [%s()]: " f, __func__,## x)

static struct ipc_spi_dev *ipc_dev;

static u8 send_buf[128];
static u8 dummy_buf[4] = {0x5a, 0x5a, 0x0,0x0};
static struct ipc_transfer_frame tx_transfer_frame[MAX_MIPC_TX_FRAME_NUM];
static struct ipc_transfer_frame rx_transfer_frame[MAX_MIPC_RX_FRAME_NUM];


static u8 read_buf[MAX_RECEIVER_SIZE];
static u8 write_buf[MAX_MIPC_TX_FRAME_SIZE];

struct ipc_spi_dev *g_ipc_spi_dev = NULL;

extern int modemsts_notifier_register(struct modemsts_chg *handler);

static int ipc_freelist_Init(struct frame_list* tx_ptr, struct frame_list* rx_ptr)
{
	u32 t = 0;
	struct ipc_transfer_frame* frame_ptr = NULL;

	INIT_LIST_HEAD(&tx_ptr->frame_list_head);
	mutex_init(&tx_ptr->list_mutex);
	tx_ptr->counter = 0;

	memset(tx_transfer_frame, 0x0, sizeof(tx_transfer_frame));
	for(t = 0; t < MAX_MIPC_TX_FRAME_NUM;  t++) {
		frame_ptr = &tx_transfer_frame[t];
		frame_ptr->buf_size =  MAX_MIPC_TX_FRAME_SIZE;
		frame_ptr->buf_ptr = (u8*)kmalloc(frame_ptr->buf_size , GFP_KERNEL);
		if(!frame_ptr->buf_ptr)
			return -ENOMEM;
		frame_ptr->pos = 0;
		frame_ptr->flag = 0;
		list_add_tail(&frame_ptr->link, &tx_ptr->frame_list_head);
		tx_ptr->counter++;
	}

	INIT_LIST_HEAD(&rx_ptr->frame_list_head);
	mutex_init(&rx_ptr->list_mutex);
	rx_ptr->counter = 0;

	memset(rx_transfer_frame, 0x0, sizeof(rx_transfer_frame));
	for(t = 0; t < MAX_MIPC_RX_FRAME_NUM;  t++) {
		frame_ptr = &rx_transfer_frame[t];
		frame_ptr->buf_size =  MAX_MIPC_TX_FRAME_SIZE;
		frame_ptr->buf_ptr = (u8*)kmalloc(frame_ptr->buf_size , GFP_KERNEL);
		if(!frame_ptr->buf_ptr)
			return -ENOMEM;
		frame_ptr->pos = 0;
		frame_ptr->flag = 0;
		list_add_tail(&frame_ptr->link, &rx_ptr->frame_list_head);
		rx_ptr->counter++;
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

static void ipc_rvclist_init(struct frame_list* rx_ptr)
{
	INIT_LIST_HEAD(&rx_ptr->frame_list_head);
	mutex_init(&rx_ptr->list_mutex);
	rx_ptr->counter = 0;
}

static int ipc_frame_init(struct ipc_spi_dev *dev)
{
	int ret;
	ret = ipc_freelist_Init(&dev->tx_free_lst, &dev->rx_free_lst);
	if(ret) {
		printk(KERN_ERR "ipc free list init failed \n");
		return ret;
	}
	ipc_rvclist_init(&dev->rx_recv_lst);
	ipc_transferinit(&dev->tx_transfer);
	dev->rx_curframe = NULL;
	dev->tx_curframe = NULL;
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
			printk("ipc_FlushTxTransfer %d, %p \n", transfer_ptr->counter,
				transfer_ptr->cur_frame_ptr);
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

static void ipc_FreeFrame(struct ipc_transfer_frame *frame_ptr, struct frame_list*  frame_list_ptr)
{
	if(!frame_list_ptr || !frame_ptr) {
		panic("%s[%d] frame_list_ptr = NULL!\r\n", __FILE__, __LINE__);
	}
	pr_debug("_FreeFrame:0x%x\r\n", (u32)frame_ptr);
	mutex_lock(&frame_list_ptr->list_mutex);/*get	lock */
	frame_ptr->pos = 0;
	frame_ptr->flag = 0;
	frame_ptr->seq = 0;
	frame_ptr->status = IDLE_STATUS;
	list_add_tail(&frame_ptr->link, &frame_list_ptr->frame_list_head);
	frame_list_ptr->counter++;
	mutex_unlock(&frame_list_ptr->list_mutex);/*set  lock */
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
	if(length > MAX_RECEIVER_SIZE)
		BUG_ON(1);
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
	rlen = ipc_calculate_len(rlen);
	wlen = ipc_calculate_len(wlen);
	ap2cp_wakeup();
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
	ap2cp_sleep();
	dev->tx_irqdisnum++;
	dev->task_status = 7;
	wait_event(dev->wait, dev->rx_ctl || !dev->ipc_enable);
	if(!dev->ipc_enable)
		return -1;
	dev->task_status = 8;
	dev->rwnum++;
	return ret;

}

static void ipc_freerxrcvframe(struct ipc_spi_dev* dev, struct ipc_transfer_frame *p_frame)
{
	struct frame_list *pfreelist = &dev->rx_free_lst;
	list_del(&p_frame->link);
	ipc_FreeFrame(p_frame, pfreelist);
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


static void ipc_ack(struct ipc_spi_dev* dev, u8* buf, u16* len)
{
	struct ack_packet *ack = (struct ack_packet*)buf;
	bool bfound = false;
	struct frame_list *plist = &dev->rx_recv_lst;
	struct ipc_transfer_frame *p_frame;
	mutex_lock(&plist->list_mutex);
	list_for_each_entry(p_frame, &plist->frame_list_head, link) {
		if(bfound) {
			if(p_frame->status == ack->type) {
				if(p_frame->seq == ack->seq_end + 1) {
					ack->seq_end = p_frame->seq;
					if(ack->type == TYPE_ACK) {
						//dev->rx_seqnum = p_frame->seq;
					}
					ipc_freerxrcvframe(dev, p_frame);
					p_frame = list_entry(&plist->frame_list_head, struct ipc_transfer_frame, link);
					continue;
				}
			}
			break;
		}
		if(p_frame->status == ACK_STATUS || p_frame->status == NAK_STATUS) {
			bfound = true;
			ack->seq_begin = p_frame->seq;
			ack->seq_end = p_frame->seq;
			ack->magic = ACK_MAGIC;
			if(p_frame->status == ACK_STATUS) {
				ack->type = TYPE_ACK;
				//dev->rx_seqnum = p_frame->seq;
			}
			else
				ack->type = TYPE_NAK;
			*len = sizeof(struct ack_packet);
			ipc_freerxrcvframe(dev, p_frame);
			p_frame = list_entry(&plist->frame_list_head, struct ipc_transfer_frame, link);
		}
	}
	ack->checksum = ipc_checksum((const unsigned char*)&ack->type, sizeof(struct ack_packet) - 4);
	mutex_unlock(&plist->list_mutex);
}

/*static*/ void ipc_setup(struct ipc_spi_dev* dev, u8* buf, u16* len)
{
	bool bfound = false;
	u32 offset = sizeof(struct data_packet_header);
	struct data_packet_header *header;
	struct ipc_transfer *ptransfer = &dev->tx_transfer;
	struct ipc_transfer_frame*  frame_ptr = NULL;
	struct ack_packet *ack = (struct ack_packet*)buf;

	if(dev->rx_status == NAKALL_STATUS) {
		ack->seq_begin = 0;
		ack->seq_end = 0;
		ack->magic = ACK_MAGIC;
		ack->type = TYPE_NAKALL;
		*len = sizeof(struct ack_packet);
		ack->checksum = ipc_checksum((const unsigned char*)&ack->type, sizeof(struct ack_packet) - 4);
		//dev->rx_status = IDLE_STATUS;
		return;
	}

	if(ptransfer->counter) {
		/*if(ipc_IsTransferFifoEmpty(ptransfer)) {
			if(ipc_FlushTxTransfer(ptransfer)) {
				BUG_ON(1);
			}
		}*/
		mutex_lock(&ptransfer->transfer_mutex);
		/*
		if(dev->remote_status == MODEM_STATUS_ASSERT) {
			if(ptransfer->cur_frame_ptr && (ptransfer->cur_frame_ptr->pos > sizeof( struct data_packet_header))) {
				ipc_AddFrameToTxTransferFifo(ptransfer->cur_frame_ptr, ptransfer);
				ptransfer->cur_frame_ptr = NULL;
			}
		} */
		list_for_each_entry(frame_ptr, &ptransfer->frame_fifo, link) {
			if(frame_ptr->status == IDLE_STATUS) {
				bfound = true;
				break;
			}/* else {
				if(dev->remote_status == MODEM_STATUS_ASSERT)
					break;
			}*/
		}
		if(!bfound/* && (dev->remote_status != MODEM_STATUS_ASSERT)*/) {
			if(ptransfer->cur_frame_ptr && (ptransfer->cur_frame_ptr->pos > sizeof( struct data_packet_header))) {
				ipc_AddFrameToTxTransferFifo(ptransfer->cur_frame_ptr, ptransfer);
				frame_ptr = ptransfer->cur_frame_ptr;
				ptransfer->cur_frame_ptr = NULL;
				bfound = true;
			}
		}
		mutex_unlock(&ptransfer->transfer_mutex);
		if(bfound) {
			dev->tx_curframe = frame_ptr;
			header = (struct data_packet_header*)frame_ptr->buf_ptr;
			ipc_ack(dev, (u8*)(&header->ack), len);
			if(*len > 0)
				header->magic = DATA_ACK_MAGIC;
			else
				header->magic = DATA_MAGIC;
			header->len = frame_ptr->pos - offset;
			header->seqnum = frame_ptr->seq;
			header->checksum = ipc_checksum(&frame_ptr->buf_ptr[4], header->len + offset - 4);
			*len = MAX_MIPC_TX_FRAME_SIZE;
			frame_ptr->status = ACK_STATUS;
			IPC_DBG("--seq %d \n", header->seqnum);
			return;
		}

	}
	ipc_ack(dev, buf, len);
}

/*static*/ void ipc_prepare_send(struct ipc_spi_dev* dev, u8* buf,
		u16* len)
{

	ipc_setup(dev, buf, len);
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


static void ipc_prepare_read(struct ipc_spi_dev* dev, u16* len)
{
	if(dev->rx_curframe == NULL) {
		dev->rx_curframe = ipc_AllocFrame(&dev->rx_free_lst);
		if(!dev->rx_curframe)
			BUG_ON(1);
	}
	dev->rx_curframe->status = IDLE_STATUS;
	*len = MAX_MIPC_RX_FRAME_SIZE;
}

static inline int ipc_txfreelist_empty(struct ipc_spi_dev *dev)
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
			frame_ptr->seq = dev->tx_seqnum++;
		}
		remain_size = frame_ptr->buf_size - frame_ptr->pos;
		if(len > remain_size) {
			//memcpy(&frame_ptr->buf_ptr[frame_ptr->pos], data_ptr + index, remain_size);
			//frame_ptr->pos  = frame_ptr->buf_size;
			//transfer_ptr->counter += remain_size;
			transfer_ptr->cur_frame_ptr = NULL;
			ipc_AddFrameToTxTransferFifo(frame_ptr, transfer_ptr);
			//ret += remain_size;
			//index += remain_size;
			//len -= remain_size;
			new_frame_ptr =  ipc_AllocFrame(&dev->tx_free_lst);
			if(!new_frame_ptr) {
				break;
			}
			new_frame_ptr->seq = dev->tx_seqnum++;
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
	if(ret > 0)
		dev->bneedsend = true;
	mutex_unlock(&transfer_ptr->transfer_mutex);/*set	lock */
	IPC_DBG("Enter ipc_AddDataToTxTransfer %d - \n", ret);
	return ret;
}

static u32 ipc_DelDataFromTxTransfer(struct ipc_spi_dev *dev,
		struct ipc_transfer_frame *frame_ptr)
{
	struct ipc_transfer* transfer_ptr  = &dev->tx_transfer;
	mutex_lock(&transfer_ptr->transfer_mutex);
	list_del(&frame_ptr->link);
	transfer_ptr->counter -=  frame_ptr->pos - sizeof(struct data_packet_header);
	mutex_unlock(&transfer_ptr->transfer_mutex);
	ipc_FreeFrame(frame_ptr, &dev->tx_free_lst);
	wake_up(&dev->tx_frame_wait);
	return 0;
}

/*static */void ipc_FreeAllTxTransferFrame(struct ipc_spi_dev *dev)
{
	struct ipc_transfer_frame *frame_ptr = NULL;
	struct ipc_transfer *p_transfer = &dev->tx_transfer;
	mutex_lock(&p_transfer->transfer_mutex);
	list_for_each_entry(frame_ptr, &p_transfer->frame_fifo, link) {
		printk("+-+- %d, %d \n", frame_ptr->pos, frame_ptr->seq);
		mutex_unlock(&p_transfer->transfer_mutex);
		ipc_DelDataFromTxTransfer(dev, frame_ptr);
		mutex_lock(&p_transfer->transfer_mutex);
		frame_ptr = list_entry(&p_transfer->frame_fifo, struct ipc_transfer_frame, link);
	}
	mutex_unlock(&p_transfer->transfer_mutex);
}

static void ipc_freeallrxframe(struct ipc_spi_dev *dev)
{
	struct ipc_transfer_frame *frame_ptr;
	struct frame_list *plist = &dev->rx_recv_lst;
	mutex_lock(&plist->list_mutex);
	list_for_each_entry(frame_ptr, &plist->frame_list_head, link) {
		list_del(&frame_ptr->link);
		ipc_FreeFrame(frame_ptr, &dev->rx_free_lst);
		frame_ptr = list_entry(&plist->frame_list_head, struct ipc_transfer_frame, link);
	}
	mutex_unlock(&plist->list_mutex);
}

static void ipc_print_errdata(u8 *buf, u16 len)
{
	int i;
	printk("++++++++++++++++++++++++\n");
	for(i = 0; i < 128;) {

		printk("data: 0x%x, 0x%x, 0x%x, 0x%x \n", buf[i], buf[i+1], buf[i+2], buf[i+3]);
		i+=4;
	}
	printk("error data \n");
	printk("++++++++++++++++++++++++\n");
}

static void ipc_AddFrameToRxrcvlist(struct ipc_spi_dev* dev, struct ipc_transfer_frame *frame, 
									struct frame_list *plist)
{
	struct ipc_transfer_frame* tframe;
	bool binsert = false;
	mutex_lock(&plist->list_mutex);
	list_for_each_entry(tframe, &plist->frame_list_head, link) {
		if(tframe->seq > frame->seq) {
			list_add_tail(&frame->link, &tframe->link);
			binsert = true;
			break;
		} else if (tframe->seq == frame->seq) {
			if(frame->status == ACK_STATUS) {
				tframe->status = ACK_STATUS;
				mutex_unlock(&plist->list_mutex);
				return;
			}
			if(tframe->status == NAK_STATUS && frame->status == IDLE_STATUS) {
				list_add_tail(&frame->link, &tframe->link);
				ipc_freerxrcvframe(dev, tframe);
				binsert = true;
			} else if(tframe->status == NAKALL_STATUS) {
				tframe->status = IDLE_STATUS;
				dev->bneedread = true;
				mutex_unlock(&plist->list_mutex);
				return;
			} else {
				mutex_unlock(&plist->list_mutex);
				return;
			}
			break;
		}
	}
	if(!binsert) {
		list_add_tail(&frame->link, &plist->frame_list_head);
	}
	dev->rx_curframe = NULL;
	dev->bneedread = true;
	mutex_unlock(&plist->list_mutex);
}

static void ipc_rcv_data(struct ipc_spi_dev* dev, u8 *buf, u16 len)
{
	u32 offset = sizeof(struct data_packet_header);
	struct data_packet_header *header = (struct data_packet_header*)buf;
	//u16 cs = ipc_checksum(buf + 4, header->len);
	if((header->len <= MAX_MIPC_RX_FRAME_SIZE - offset) && (header->checksum == ipc_checksum(buf + 4, header->len + offset - 4))) {
		if(dev->rx_seqnum >= header->seqnum) {
				dev->rx_curframe->status = ACK_STATUS;
		}
	} else {
		printk("ipc_spi error data data checksum error \n");
		ipc_print_errdata(buf, len);
		if(dev->rx_seqnum >= header->seqnum)
			dev->rx_curframe->status = ACK_STATUS;
		else
			dev->rx_curframe->status = NAK_STATUS;
	}
	IPC_DBG("++seq %d \n", header->seqnum);
	dev->rx_curframe->seq = header->seqnum;
	dev->rx_curframe->pos = offset;
	dev->rx_curframe->buf_size = header->len + offset;
	ipc_AddFrameToRxrcvlist(dev,dev->rx_curframe, &dev->rx_recv_lst);
	//dev->rx_curframe = NULL;
	wake_up(&dev->rx_read_wait);
}

static void ipc_process_errdata(struct ipc_spi_dev* dev)
{
	struct ipc_transfer_frame *frame;
	struct frame_list *plist = &dev->rx_recv_lst;
	struct ipc_transfer *p_transfer = &dev->tx_transfer;
	mutex_lock(&p_transfer->transfer_mutex);
	list_for_each_entry(frame, &p_transfer->frame_fifo, link) {
		frame->status = IDLE_STATUS;
	}
	mutex_unlock(&p_transfer->transfer_mutex);

	mutex_lock(&plist->list_mutex);
	list_for_each_entry(frame, &plist->frame_list_head, link) {
		if(frame->status == IDLE_STATUS || frame->status == ACK_STATUS) {
			frame->status = NAKALL_STATUS;
		} else if(frame->status == NAK_STATUS) {
			ipc_freerxrcvframe(dev, frame);
			frame = list_entry(&plist->frame_list_head, struct ipc_transfer_frame, link);
		}
	}
	mutex_unlock(&plist->list_mutex);
	dev->rx_status = NAKALL_STATUS;
}

static void ipc_rcv_ack(struct ipc_spi_dev* dev, u8 *buf, bool bcheck)
{
	u32 seqnum;
	struct ipc_transfer_frame *frame;
	struct ipc_transfer *p_transfer = &dev->tx_transfer;
	struct ack_packet *header = (struct ack_packet*)buf;
	IPC_DBG("ipc_rcv_ack %d, %d, %d \n", header->type, header->seq_begin, header->seq_end);
	if(bcheck
		&& (header->checksum != ipc_checksum((const unsigned char*)&header->type, sizeof(struct ack_packet) - 4))) {
		printk(KERN_ERR "ipc_spi wrong ack packet1 \n");
		ipc_print_errdata(buf, sizeof(struct ack_packet));
		ipc_process_errdata(dev);
		return;
	}
	if(header->type == TYPE_ACK) {
		mutex_lock(&p_transfer->transfer_mutex);
		list_for_each_entry(frame, &p_transfer->frame_fifo, link) {
			for(seqnum = header->seq_begin; seqnum < header->seq_end + 1; seqnum++) {
				if(frame->seq == seqnum) {
					mutex_unlock(&p_transfer->transfer_mutex);
					ipc_DelDataFromTxTransfer(dev, frame);
					mutex_lock(&p_transfer->transfer_mutex);
					frame = list_entry(&p_transfer->frame_fifo, struct ipc_transfer_frame, link);
				}
			}
		}
		mutex_unlock(&p_transfer->transfer_mutex);
	} else if(header->type == TYPE_NAK) {
		printk(KERN_WARNING "ipc receive nak status %d, %d \n", header->seq_begin, header->seq_end);
		mutex_lock(&p_transfer->transfer_mutex);
		list_for_each_entry(frame, &p_transfer->frame_fifo, link) {
			for(seqnum = header->seq_begin; seqnum < header->seq_end + 1; seqnum++) {
				if(frame->seq == seqnum)
					frame->status = IDLE_STATUS;
			}
		}
		mutex_unlock(&p_transfer->transfer_mutex);
	} else if(header->type == TYPE_NAKALL) {
		struct frame_list *plist = &dev->rx_recv_lst;
		printk(KERN_WARNING "ipc receive nakall status \n");
		mutex_lock(&p_transfer->transfer_mutex);
		list_for_each_entry(frame, &p_transfer->frame_fifo, link) {
			frame->status = IDLE_STATUS;
		}
		mutex_unlock(&p_transfer->transfer_mutex);

		mutex_lock(&plist->list_mutex);
		list_for_each_entry(frame, &plist->frame_list_head, link) {
			if(frame->status == IDLE_STATUS || frame->status == ACK_STATUS) {
				frame->status = NAKALL_STATUS;
			} else if(frame->status == NAK_STATUS) {
				ipc_freerxrcvframe(dev, frame);
				frame = list_entry(&plist->frame_list_head, struct ipc_transfer_frame, link);
			}
		}
		mutex_unlock(&plist->list_mutex);
	} else {
		ipc_print_errdata(buf, sizeof(struct ack_packet));
		ipc_process_errdata(dev);
		printk(KERN_ERR "ipc_spi wrong ack packet \n");
	}
}

static void ipc_rcv_dataack(struct ipc_spi_dev* dev, u8 *buf, u16 len)
{
	u32 offset = sizeof(struct data_packet_header);
	struct data_packet_header *header = (struct data_packet_header*)buf;
	if((header->len <= MAX_MIPC_RX_FRAME_SIZE - offset) && (header->checksum == ipc_checksum(buf + 4, header->len + offset - 4))) {
		struct data_packet_header *data_header;
		data_header = (struct data_packet_header*)buf;
		ipc_rcv_ack(dev, (u8*)&(data_header->ack), false);
		ipc_rcv_data(dev, buf, len);
	} else {
		printk(KERN_ERR "ipc_spi dataack checksum error \n");
		ipc_print_errdata(buf, len);
		ipc_process_errdata(dev);
		return;
	}
}

static void ipc_process_rcvdata(struct ipc_spi_dev* dev, u8 *buf, u16 len)
{
	u16 *magic = (u16*)buf;
	#if 0
	static int i = 0;
	i++;
	if((i % 100) == 0) {
		struct data_packet_header *data_header;
		data_header = (struct data_packet_header*)buf;
		printk("** %d, %d \n", data_header->magic, data_header->seqnum);
		ipc_process_errdata(dev);
		return;
	}
	#endif
	IPC_DBG("ipc_process_rcvdata 0x%x \n", *magic);
	if(dev->rx_status == NAKALL_STATUS) {
		dev->rx_status = IDLE_STATUS;
		return;
	}
	if(DATA_MAGIC == *magic)
		ipc_rcv_data(dev, buf, len);
	else if(ACK_MAGIC == *magic)
		ipc_rcv_ack(dev, buf, true);
	else if (DATA_ACK_MAGIC == *magic) {
		ipc_rcv_dataack(dev, buf, len);
	} else if(DUMMY_MAGIC == *magic) {
		IPC_DBG("ipc dump data just discard \n");
	} else {
		printk(KERN_ERR "ipc wrong data need to tell remote peer \n");
		ipc_print_errdata(buf, len);
		ipc_process_errdata(dev);
	}
}

static void ipc_process_send(struct ipc_spi_dev* dev)
{
}

static inline void ipc_hasdata(struct ipc_spi_dev* dev)
{
	bool bread = false;
	struct ipc_transfer_frame *frame;
	struct frame_list *plist = &dev->rx_recv_lst;
	mutex_lock(&plist->list_mutex);
	list_for_each_entry(frame, &plist->frame_list_head, link) {
		if((frame->status == IDLE_STATUS)
			&& (frame->seq == dev->rx_seqnum + 1)
			&& (frame->buf_size - frame->pos > 0)) {
			bread= true;
			break;
		}
	}
	dev->bneedread = bread;
	mutex_unlock(&plist->list_mutex);
}

static int ipc_readdatafromrvclist(struct ipc_spi_dev* dev,
				char *buf, size_t  count)
{
	u32 index = 0;
	struct ipc_transfer_frame *frame;
	struct frame_list *plist = &dev->rx_recv_lst;
	mutex_lock(&plist->list_mutex);
	list_for_each_entry(frame, &plist->frame_list_head, link) {
		if((frame->status == IDLE_STATUS)
			&& (frame->seq == dev->rx_seqnum + 1)
			&& (frame->buf_size - frame->pos > 0)) {
			if(frame->buf_size - frame->pos > count) {
				memcpy(&buf[index], &frame->buf_ptr[frame->pos], count);
				frame->pos += count;
				index += count;
				break;
			} else if(frame->buf_size - frame->pos == count) {
				memcpy(&buf[index], &frame->buf_ptr[frame->pos], count);
				frame->pos += count;
				frame->status = ACK_STATUS;
				index += count;
				dev->rx_seqnum++;
				dev->bneedsend = true;
				break;
			} else {
				memcpy(&buf[index], &frame->buf_ptr[frame->pos], frame->buf_size - frame->pos);
				index += frame->buf_size - frame->pos;
				count -= frame->buf_size - frame->pos;
				frame->pos = frame->buf_size;
				frame->status = ACK_STATUS;
				dev->rx_seqnum++;
				dev->bneedsend = true;
			}
		}
	}
	mutex_unlock(&plist->list_mutex);
	return index;
}

static int mux_ipc_spi_read(char *buf, size_t  count)
{
	int ret = 0;
	struct ipc_spi_dev *dev = ipc_dev;

	ipc_hasdata(dev);
    wait_event(dev->rx_read_wait, dev->bneedread
		|| (dev->rwctrl & IPC_READ_DISABLE));

	if(dev->rwctrl & IPC_READ_DISABLE) {
		printk(KERN_ERR "read was disabled \n");
		return -1;
	}
    IPC_DBG("[mipc]mux_ipc_spi_read read len:%d\r\n", count);
	ret = ipc_readdatafromrvclist(dev, buf, count);
	if(ret)
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
	if(count == 0) {
		printk("send count equal zero ? \n");
		return ret;
	}
	IPC_DBG("Enter mux_ipc_spi_write %d \n", count);
	do {
			ret = ipc_AddDataToTxTransfer(dev, (u8*)buf, count);
			if(!ret) {
				wait_event(dev->tx_frame_wait, !ipc_txfreelist_empty(dev));
			}
	} while(!ret);

	/* if tx transfer is empty so need to wake up thread */
	if(ret) {
		IPC_DBG("ipc write wake up \n");
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

    wake_up(&dev->rx_read_wait);

    return 0;
}


static struct sprdmux sprd_iomux = {
	.id		= SPRDMUX_ID_SPI,
	.io_read	= mux_ipc_spi_read,
	.io_write	= mux_ipc_spi_write,
	.io_stop  =  mux_ipc_spi_stop,
};

static void  spi_ipc_enable(struct modemsts_chg *s, unsigned int is_enable)
{
	struct ipc_spi_dev *dev = ipc_dev;
	printk("spi_ipc_enable %d, %d \n", is_enable, dev->task_status);
	if(is_enable == MODEM_STATUS_POWERON) {
		spi_hal_gpio_irq_init(dev);
		return ;
	}
	if(is_enable == MODEM_STATUS_ALVIE) {
		dev->rwctrl = 0;
		//dev->ipc_enable = true;
		//dev->remote_status = REMOTE_ALIVE_STATUS;
	} else if(is_enable == MODEM_STATUS_REBOOT){
		ap2cp_disable();
		dev->ipc_enable = false;
	}
	dev->remote_status = is_enable;
	wake_up(&dev->wait);
	IPC_DBG("[mipc]mux ipc enable:0x%x, status:0x%x\r\n", is_enable, dev->ipc_enable);
}

static void ipc_reset(struct ipc_spi_dev *dev)
{
	dev->rx_ctl = false;
	dev->bneedrcv = false;
	dev->bneedread = false;
	dev->bneedsend = false;
	dev->rx_status = IDLE_STATUS;
	dev->tx_seqnum = 1;
	dev->rx_seqnum = 0;
	dev->irq_num = 0;
	dev->rwnum = 0;
	dev->tx_irqennum = 0;
	dev->tx_irqdisnum = 0;
	dev->ap_infocnt = 0;
}

static inline bool ipc_cansend(struct ipc_spi_dev *dev)
{
	bool bsend = false;
	struct ipc_transfer_frame*  frame_ptr = NULL;
	struct ipc_transfer *ptransfer = &dev->tx_transfer;
	struct frame_list *plist = &dev->rx_recv_lst;
	if(dev->rx_status == NAKALL_STATUS)
		return true;
	mutex_lock(&ptransfer->transfer_mutex);
	if(ptransfer->cur_frame_ptr) {
		if(ptransfer->cur_frame_ptr->pos > sizeof(struct data_packet_header))
			bsend = true;
	}
	if(!bsend) {
		list_for_each_entry(frame_ptr, &ptransfer->frame_fifo, link) {
			if(frame_ptr->status == IDLE_STATUS) {
				bsend = true;
				break;
			}
		}
	}
	dev->bneedsend = bsend;
	mutex_unlock(&ptransfer->transfer_mutex);
	if(bsend)
		return bsend;

	mutex_lock(&plist->list_mutex);
	list_for_each_entry(frame_ptr, &plist->frame_list_head, link) {
		if(frame_ptr->status == ACK_STATUS || frame_ptr->status == NAK_STATUS) {
			bsend = true;
			break;
		}
	}
	dev->bneedsend = bsend;
	mutex_unlock(&plist->list_mutex);
	return bsend;
}

static int mux_ipc_thread(void *data)
{
	int rval = 0;
	struct ipc_spi_dev *dev = (struct ipc_spi_dev*)data;
	struct sched_param	 param = {.sched_priority = 10};

	IPC_DBG("mux_ipc_tx_thread");
	sched_setscheduler(current, SCHED_FIFO, &param);

	spi_hal_gpio_init();

	IPC_DBG("%s() spi channel opened %d\n", __func__, rval);
	//spi_hal_gpio_irq_init(dev);
	dev->task_count = 0;
	dev->task_status = 0;

	while (!kthread_should_stop()) {
		dev->task_count++;
		dev->task_status = 1;
		wait_event(dev->wait, dev->remote_status || !dev->ipc_enable);
		dev->task_status = 2;
		wait_event(dev->wait,  dev->bneedsend || dev->bneedrcv || !dev->ipc_enable);
		dev->task_status = 3;
		ipc_recordaptime(dev);

		IPC_DBG("Enter mux_ipc_thread %d, %d \n",
				dev->tx_transfer.counter, dev->bneedrcv);
		if(!dev->ipc_enable) {
			printk(KERN_ERR "ipc was disabled , there must something wrong! \n");
			ipc_reset(dev);
			ipc_FlushTxTransfer(&dev->tx_transfer);
			ipc_FreeAllTxTransferFrame(dev);
			ipc_freeallrxframe(dev);
			ap2cp_disable();
			//dev->rwctrl = 0;
			dev->ipc_enable = true;
			continue;
		}
#ifdef CONFIG_PM
		wait_event(dev->wait, !dev->bsuspend);
#endif
		wake_lock(&dev->wake_lock);
		while(ipc_cansend(dev) || dev->bneedrcv) {
			u16 tx_len = 0;
			u16 rx_len = 0;
			ipc_prepare_send(dev, send_buf, &tx_len);
			ipc_prepare_read(dev, &rx_len);
			IPC_DBG("ipc done prepare %d, %d \n", tx_len, rx_len);
			dev->task_status = 4;
			if(tx_len == MAX_MIPC_TX_FRAME_SIZE) {
				rval = ipc_read_write_data(dev, dev->rx_curframe->buf_ptr, rx_len, dev->tx_curframe->buf_ptr, tx_len);
			} else
				rval = ipc_read_write_data(dev, dev->rx_curframe->buf_ptr, rx_len, send_buf, tx_len);
			IPC_DBG("ipc_read_write_data rval %d \n", rval);
			if(rval < 0)
				break;
			dev->task_status = 9;
			ipc_process_rcvdata(dev, dev->rx_curframe->buf_ptr, rx_len);
			dev->task_status = 10;
			ipc_process_send(dev);
			dev->task_status = 11;
		}
		wake_unlock(&dev->wake_lock);
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
	struct ipc_transfer *p_transfer = &dev->tx_transfer;
	struct ipc_transfer_frame *frame_ptr = NULL;
	struct frame_list *frame_lst = NULL;
	seq_printf(m, "======================================================================\n");
	seq_printf(m, "ipc_enable: %d: rwctrl: %d\n", dev->ipc_enable, dev->rwctrl);
	seq_printf(m, "bneedrcv: %d\n", dev->bneedrcv);
	seq_printf(m, "rx_ctl: %d\n", dev->rx_ctl);
	seq_printf(m, "rx status: %d  rx_seqnum: %d\n",
			dev->rx_status, dev->rx_seqnum);
	seq_printf(m, "tx_seqnum: %d, remote_status: %d \n", dev->tx_seqnum, dev->remote_status);
	seq_printf(m, "tx_transfer.counter: %d: \n", dev->tx_transfer.counter);
	seq_printf(m, "tx_free_lst.counter: %d: \n", dev->tx_free_lst.counter);
	seq_printf(m, "ap2cp_sts: %d: cp2ap_sts: %d\n", ap2cp_sts(), cp2ap_sts());
	seq_printf(m, "task_count: %d: task_status: %d\n", dev->task_count, dev->task_status);
	seq_printf(m, "irq_num: %d rwnum: %d\n", dev->irq_num, dev->rwnum);
	seq_printf(m, "tx_irqennum: %d tx_irqdisnum: %d\n", dev->tx_irqennum, dev->tx_irqdisnum);

	seq_printf(m, "======================================================================\n");


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

	seq_printf(m, "======================================================================\n");
	seq_printf(m, "show tx transfer list\n");
	mutex_lock(&p_transfer->transfer_mutex);
	list_for_each_entry(frame_ptr, &p_transfer->frame_fifo, link) {
		seq_printf(m, "frameptr: %p, pos: %d, seq: %d status: %d \n", frame_ptr, frame_ptr->pos, frame_ptr->seq, frame_ptr->status);
	}
	mutex_unlock(&p_transfer->transfer_mutex);
	seq_printf(m, "======================================================================\n");
	seq_printf(m, "show tx free list\n");
	frame_lst = &dev->tx_free_lst;
	mutex_lock(&frame_lst->list_mutex);
	list_for_each_entry(frame_ptr, &frame_lst->frame_list_head, link) {
		seq_printf(m, "frameptr: %p, pos: %d, seq: %d status: %d \n", frame_ptr, frame_ptr->pos, frame_ptr->seq, frame_ptr->status);
	}
	mutex_unlock(&frame_lst->list_mutex);
	seq_printf(m, "======================================================================\n");
	seq_printf(m, "show rx receive list\n");
	frame_lst = &dev->rx_recv_lst;
	mutex_lock(&frame_lst->list_mutex);
	list_for_each_entry(frame_ptr, &frame_lst->frame_list_head, link) {
		seq_printf(m, "frameptr: %p, pos: %d, seq: %d status: %d \n", frame_ptr, frame_ptr->pos, frame_ptr->seq, frame_ptr->status);
	}
	mutex_unlock(&frame_lst->list_mutex);
	seq_printf(m, "======================================================================\n");
	seq_printf(m, "show rx free list\n");
	frame_lst = &dev->rx_free_lst;
	mutex_lock(&frame_lst->list_mutex);
	list_for_each_entry(frame_ptr, &frame_lst->frame_list_head, link) {
		seq_printf(m, "frameptr: %p, pos: %d, seq: %d status: %d \n", frame_ptr, frame_ptr->pos, frame_ptr->seq, frame_ptr->status);
	}
	mutex_unlock(&frame_lst->list_mutex);
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

static int ipc_debug_show1(struct seq_file *m, void *private)
{
	u32 max ;
	u32 i;
	struct ipc_spi_dev *dev = ipc_dev;

	dev->rx_status = NAKALL_STATUS;
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
	ipc_reset(dev);

	dev->ipc_enable = true;
	dev->remote_status = 0;
#ifdef CONFIG_PM
	dev->bsuspend = false;
#endif
	wake_lock_init(&dev->wake_lock, WAKE_LOCK_SUSPEND, "ipc_spi");
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

	spi->max_speed_hz = 19200000;
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
	dev->modemsts.level = MODEMSTS_LEVEL_SPI;
	dev->modemsts.data = &dev->modemsts;
	dev->modemsts.modemsts_notifier = spi_ipc_enable;
	modemsts_notifier_register(&dev->modemsts);

	return 0;
error_unregister_dev:
	misc_deregister(&dev->miscdev);

error_free_dev:
	for(i = 0; i < MAX_MIPC_TX_FRAME_NUM; i++) {
		if(tx_transfer_frame[i].buf_ptr)
			kfree(tx_transfer_frame[i].buf_ptr);
	}
	for(i = 0; i < MAX_MIPC_RX_FRAME_NUM; i++) {
		if(rx_transfer_frame[i].buf_ptr)
			kfree(rx_transfer_frame[i].buf_ptr);
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
	for(i = 0; i < MAX_MIPC_TX_FRAME_NUM; i++) {
		if(tx_transfer_frame[i].buf_ptr)
			kfree(tx_transfer_frame[i].buf_ptr);
	}
	for(i = 0; i < MAX_MIPC_RX_FRAME_NUM; i++) {
		if(rx_transfer_frame[i].buf_ptr)
			kfree(rx_transfer_frame[i].buf_ptr);
	}
	kfree(dev);
	return 0;
}

#ifdef CONFIG_PM
static int ipc_spi_suspend(struct device *dev)
{
	struct ipc_spi_dev *ipc_dev = dev_get_drvdata(dev);
	IPC_DBG("ipc_spi_suspend\n");
	ipc_dev->bsuspend = true;
	return 0;
}

static int ipc_spi_resume(struct device *dev)
{
	struct ipc_spi_dev *ipc_dev = dev_get_drvdata(dev);
	IPC_DBG("ipc_spi_resume\n");
	ipc_dev->bsuspend = false;
	wake_up(&ipc_dev->wait);
	return 0;
}

static const struct dev_pm_ops ipc_spi_pm_ops = {
	.suspend = ipc_spi_suspend,
	.resume  = ipc_spi_resume,
};
#define IPC_SPI_PM_OPS (&ipc_spi_pm_ops)

#else
#define IPC_SPI_PM_OPS NULL
#endif


static struct spi_driver ipc_spi_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = IPC_SPI_PM_OPS,
	},
	.probe = ipc_spi_probe,
	.remove = __exit_p(ipc_spi_remove),
};
module_spi_driver(ipc_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhongping.tan<zhongping.tan@spreadtrum.com>");
MODULE_DESCRIPTION("MUX spi Driver");
