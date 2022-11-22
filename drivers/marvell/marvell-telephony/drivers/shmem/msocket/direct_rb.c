/*
    Marvell PXA9XX ACIPC-MSOCKET driver for Linux
    Copyright (C) 2012 Marvell International Ltd.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
#include <linux/export.h>
#endif
#include <linux/module.h>
#include <linux/miscdevice.h>

#include "msocket.h"
#include "shm.h"
#include "diag.h"
#include "shm_share.h"
#include "direct_rb.h"

extern struct wake_lock acipc_wakeup; //used to ensure Workqueue scheduled.

static struct direct_rbctl direct_rbctl[direct_rb_type_total_cnt];
static enum shm_rb_type direct_shm_rb_type[direct_rb_type_total_cnt] = {
       shm_rb_diag
};
static int direct_dump_flag = 0;

static int msocketDirectDump_open(struct inode *inode, struct file *filp)
{
	filp->private_data = (void *)direct_dump_flag;
	return 0;
}
static int msocketDirectDump_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t msocketDirectDump_read(struct file *filp, char __user * buf, size_t len, loff_t * f_pos)
{
	char temp[256];
	int flag = direct_dump_flag;

	sprintf(temp,"0x%08x", flag);
	if(copy_to_user(buf, (void *)&temp, strlen(temp) + 1))
	{
		printk(KERN_ERR "%s: copy_to_user failed.\n", __func__);
		return -EFAULT;
	}
	printk("%s: get flag :%s\n", __func__, temp);
	return 0;
}

static ssize_t msocketDirectDump_write(struct file *filp, const char __user * buf, size_t len, loff_t * f_pos)
{
	char temp[256];
	int flag = 0;
	int flagSave;
	int i;
	unsigned long flags;
	if(copy_from_user((void *)&temp, buf, len))
	{
		printk(KERN_ERR "%s: copy_from_user failed.\n", __func__);
		return -EFAULT;
	}
	flag = simple_strtol(temp, NULL, 0);
	flagSave = flag;
	printk("%s: set flag :0x%08x\n",  __func__, flag);
	for(i = 0; i < sizeof(flag) * 8; i++)
	{
		if(flag & 0x1)
		{
			if (i >= direct_rb_type_total_cnt)
			{
				break;
			}
			spin_lock_irqsave(&direct_rbctl[i].rb_rx_lock, flags);
			if(direct_rbctl[i].refcount == 0)
			{
				spin_unlock_irqrestore(&direct_rbctl[i].rb_rx_lock, flags);
				flag >>= 1;
				continue;
			}
			direct_rbctl[i].is_ap_recv_empty = false;
			spin_unlock_irqrestore(&direct_rbctl[i].rb_rx_lock, flags);
			printk("%s: i = %d, cp_wptr=%d, ap_rptr=%d\n",  __func__, i,
				direct_rbctl[i].rbctl->skctl_va->cp_wptr, direct_rbctl[i].rbctl->skctl_va->ap_rptr);
			wake_up_interruptible(&(direct_rbctl[i].rb_rx_wq));
		}
		flag >>= 1;
	}
	direct_dump_flag = flagSave;
	return len;
}

static struct file_operations msocketDirectDump_fops = {
	.owner = THIS_MODULE,
	.open = msocketDirectDump_open,
	.release = msocketDirectDump_close,
	.read = msocketDirectDump_read,
	.write = msocketDirectDump_write
};
static struct miscdevice msocketDirectDump_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "diag_dump",
	.fops = &msocketDirectDump_fops
};

void direct_rb_packet_send_cb(struct shm_rbctl *rbctl)
{
	struct direct_rbctl *dir_ctl = rbctl->priv;
	spin_lock(&dir_ctl->rb_rx_lock);
	if(dir_ctl->refcount == 0)
	{
		spin_unlock(&dir_ctl->rb_rx_lock);
		return;
	}
	dir_ctl->is_ap_recv_empty = false;
	spin_unlock(&dir_ctl->rb_rx_lock);
	wake_lock_timeout(&acipc_wakeup, HZ * 2);
	wake_up_interruptible(&(dir_ctl->rb_rx_wq));
}

struct shm_callback direct_path_shm_cb = {
        .peer_sync_cb    = NULL,
        .packet_send_cb  = direct_rb_packet_send_cb,
        .port_fc_cb      = NULL,
        .rb_stop_cb      = NULL,
        .rb_resume_cb    = NULL,
};

struct direct_rbctl* direct_rb_open(enum direct_rb_type direct_type, int svc_id)
{
	unsigned long flags;

	if (direct_type >= direct_rb_type_total_cnt || direct_type < 0) {
		printk(KERN_ERR "%s: incorrect type %d\n", __func__, direct_type);
		return NULL;
	}

	spin_lock_irqsave(&direct_rbctl[direct_type].rb_rx_lock, flags);
	if(direct_rbctl[direct_type].refcount > 0)
	{
		direct_rbctl[direct_type].refcount++;
		goto exit;
	}
	direct_rbctl[direct_type].refcount++;
	direct_rbctl[direct_type].svc_id = svc_id;
	direct_rbctl[direct_type].is_linkdown_msg_received = false;
	direct_rbctl[direct_type].is_linkup_msg_received = false;
exit:
	spin_unlock_irqrestore(&direct_rbctl[direct_type].rb_rx_lock, flags);
	return &direct_rbctl[direct_type];
}
EXPORT_SYMBOL(direct_rb_open);

void direct_rb_close(struct direct_rbctl *rbctl)
{
	unsigned long flags;
	if (!rbctl) {
		printk(KERN_ERR "%s: empty data channel\n", __func__);
		return;
	}
	spin_lock_irqsave(&rbctl->rb_rx_lock, flags);
	if(rbctl->refcount > 0)
		rbctl->refcount--;
	spin_unlock_irqrestore(&rbctl->rb_rx_lock, flags);
}
EXPORT_SYMBOL(direct_rb_close);

/* direct_rb_init */
int direct_rb_init(void)
{
	int rc = -1;
	struct direct_rbctl *dir_ctl;
	struct direct_rbctl *dir_ctl2;
	const struct direct_rbctl *dir_ctl_end = direct_rbctl + direct_rb_type_total_cnt;

	for (dir_ctl = direct_rbctl; dir_ctl != dir_ctl_end; ++dir_ctl) {
		dir_ctl->direct_type = dir_ctl - direct_rbctl;
		dir_ctl->rbctl = shm_open(direct_shm_rb_type[dir_ctl - direct_rbctl], &direct_path_shm_cb, dir_ctl);
		if (!dir_ctl->rbctl) {
			printk(KERN_ERR "%s: cannot open shm\n", __func__);
			goto exit;
		}
		init_waitqueue_head(&(dir_ctl->rb_rx_wq));
		spin_lock_init(&dir_ctl->rb_rx_lock);
		dir_ctl->refcount = 0;
		dir_ctl->is_ap_recv_empty = true;
	}
	if ((rc = misc_register(&msocketDirectDump_dev)) < 0) {
		dir_ctl = direct_rbctl;
		goto exit;
	}
	return 0;

exit:
	for (dir_ctl2 = direct_rbctl; dir_ctl2 != dir_ctl; ++dir_ctl2)
		shm_close(dir_ctl2->rbctl);

	return rc;
}
EXPORT_SYMBOL(direct_rb_init);

/* direct_rb_exit */
void direct_rb_exit(void)
{
	unsigned long flags;
	struct direct_rbctl *dir_ctl;
	const struct direct_rbctl *dir_ctl_end = direct_rbctl + direct_rb_type_total_cnt;

	for (dir_ctl = direct_rbctl; dir_ctl != dir_ctl_end; ++dir_ctl) {
		spin_lock_irqsave(&dir_ctl->rb_rx_lock, flags);
		dir_ctl->refcount = 0;
		shm_close(dir_ctl->rbctl);
		spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
	}
}
EXPORT_SYMBOL(direct_rb_exit);


#define DIAG_TX_RETRY_DELAY_MS 10
int direct_rb_xmit(enum direct_rb_type direct_type, const char __user *buf, int len)
{
	struct shm_rbctl *rbctl;
	struct direct_rb_skhdr *hdr;
	struct shm_skctl *skctl;
	int slot;

	if (direct_type >= direct_rb_type_total_cnt || direct_type < 0) {
		printk(KERN_ERR "%s: incorrect type %d\n", __func__, direct_type);
		return -1;
	}

	if (!msocket_is_synced)
		return -1;

	rbctl = direct_rbctl[direct_type].rbctl;
	skctl = rbctl->skctl_va;

	if (len > rbctl->tx_skbuf_size - sizeof(*hdr)) {
		printk(KERN_ERR
		       "DIAG_RB: %s: len is %d larger than tx_skbuf_size\n",
		       __func__, len);
		return -1;
	}

	/* as no flow control for the diag ring-buffer, just delay
	 * the tx in case the ring buffer is full and then check again
         */
	while (shm_is_xmit_full(rbctl) && msocket_is_synced)    /* if ring buffer is full */
		msleep (DIAG_TX_RETRY_DELAY_MS);

	slot = shm_get_next_tx_slot(rbctl, skctl->ap_wptr);

	hdr = (struct direct_rb_skhdr *)SHM_PACKET_PTR(rbctl->tx_va, slot, rbctl->tx_skbuf_size);
	hdr->length = len;
	if (copy_from_user((char *)hdr + sizeof(hdr), buf, len)) {
		printk(KERN_ERR "MSOCK: %s: copy_from_user failed.\n",
			__func__);
		return -EFAULT;
	}

	skctl->ap_wptr = slot;  /* advance pointer index */

	shm_notify_packet_sent(rbctl);
	return 0;
}
EXPORT_SYMBOL(direct_rb_xmit);

static ssize_t direct_rb_recv_broadcast_msg(enum direct_rb_type direct_type,
	char __user *buf, int len)
{
	struct direct_rbctl *dir_ctl = direct_rbctl + direct_type;
	struct shm_rbctl *rbctl = dir_ctl->rbctl;
	void *msg;
	int msg_size;
	int rc = -EFAULT;
	int proc;
	unsigned long flags;

	spin_lock_irqsave(&dir_ctl->rb_rx_lock, flags);
	if(dir_ctl->is_linkdown_msg_received)
	{
		proc = MsocketLinkdownProcId;
		dir_ctl->is_linkdown_msg_received = false;
	}
	else if(dir_ctl->is_linkup_msg_received)
	{
		proc = MsocketLinkupProcId;
		dir_ctl->is_linkup_msg_received = false;
		/*Now both AP and CP will not send packet to ring buffer or receive packet from ring buffer,
		so cleanup any packet in ring buffer and initialize some key data structure to the beginning state
		otherwise user space process may occur error.*/
		shm_rb_data_init(rbctl);
	}
	else
	{
		rc = 0;
		spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
		goto exit;
	}

	if(direct_type == direct_rb_type_diag)
		msg_size = sizeof(DiagMsgHeader);
	else
		msg_size = sizeof(ShmApiMsg);

	if (msg_size > len) {
		printk (KERN_ERR "direct_rb: %s error,: no enough space, msg len = 0x%x, recv buf len = 0x%x\n",
			__func__, msg_size, len);
		rc = -EINVAL;
		spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
		goto exit;
	}
	if((msg = kmalloc(msg_size, GFP_ATOMIC)) == NULL)
	{
		printk (KERN_ERR "direct_rb: %s: malloc error\n", __func__);
		rc = -ENOMEM;
		spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
		goto exit;
	}

	if(direct_type == direct_rb_type_diag)
	{
		DiagMsgHeader *pDiagMsgHeader = (DiagMsgHeader *)msg;
		pDiagMsgHeader->diagHeader.packetlen = sizeof(pDiagMsgHeader->procId);
		pDiagMsgHeader->diagHeader.seqNo = 0;
		pDiagMsgHeader->diagHeader.msgType = proc;
		pDiagMsgHeader->procId = proc;
	}
	else
	{
		ShmApiMsg *pShmApimsg = (ShmApiMsg *)msg;
		pShmApimsg->svcId = dir_ctl->svc_id;
		pShmApimsg->procId = proc;
		pShmApimsg->msglen = 0;
	}

	spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
	/*since copy_to_user may sleep, so make sure release spin lock when call it*/
	if (copy_to_user(buf, (char *)msg, msg_size)) {
		printk(KERN_ERR "direct_rb: %s: copy_to_user failed.\n", __func__);
		kfree(msg);
		goto exit;
	}

	kfree(msg);
	rc = msg_size;
exit:
	if(rc < 0)
	{
		/*restore broadcast message state after failing to push these message to user space*/
		spin_lock_irqsave(&dir_ctl->rb_rx_lock, flags);
		if(proc == MsocketLinkdownProcId)
		{
			dir_ctl->is_linkdown_msg_received = true;
		}
		else if(proc == MsocketLinkupProcId && dir_ctl->is_linkdown_msg_received == false)
		{
			dir_ctl->is_linkup_msg_received = true;
		}
		spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
	}
	return rc;
}

/* check if broadcast message received */
static inline bool direct_rb_broadcast_msg_recv_empty(struct direct_rbctl *dir_ctl)
{
       return dir_ctl->is_linkup_msg_received == false &&
           dir_ctl->is_linkdown_msg_received == false;
}

ssize_t direct_rb_recv(enum direct_rb_type direct_type,
	char __user *buf, int len)
{
	struct direct_rbctl *dir_ctl = direct_rbctl + direct_type;
	struct shm_rbctl *rbctl = dir_ctl->rbctl;
	struct shm_skctl *skctl = rbctl->skctl_va;
	struct direct_rb_skhdr *hdr;
	int rc = -EFAULT;
	unsigned long flags;
	ssize_t packet_len;

	int slot;

	while (1)
	{
		if (wait_event_interruptible(dir_ctl->rb_rx_wq, !(dir_ctl->is_ap_recv_empty
			&& direct_rb_broadcast_msg_recv_empty(dir_ctl))))
		{
			/* signal: tell the fs layer to handle it */
			return -EINTR;
		}

		if(!direct_rb_broadcast_msg_recv_empty(dir_ctl))
			break;

		spin_lock_irqsave(&dir_ctl->rb_rx_lock, flags);
		if(shm_is_recv_empty(rbctl))
		{
			dir_ctl->is_ap_recv_empty = true;
			spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
		}
		else
		{
			spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
			break;
		}
	}

	/*rc =0 means not have received a broadcast message so go to receive a message from ring buffer.
	For other case means have received a broadcast, no matter push to user space successfully or not,
	return directly here.*/
	if((rc = direct_rb_recv_broadcast_msg(direct_type, buf, len)) != 0)
	{
		return rc;
	}

	slot = shm_get_next_rx_slot(rbctl, skctl->ap_rptr);

	hdr = (struct direct_rb_skhdr *)SHM_PACKET_PTR(rbctl->rx_va, slot,
				rbctl->rx_skbuf_size);

	if (hdr->length > len) {
		printk (KERN_ERR "direct_rb: %s error,: no enough space, slot = %d, cp_wptr = %d, pkt len = 0x%x, hdr = 0x%x \n",
			__func__, slot, skctl->cp_wptr, (unsigned int)hdr->length, (unsigned int)hdr);
		/*
		 * just skip it as AP side regard it as an invalid.
		 */
		printk ("discard this frame and jump ahead...  \n");
		skctl->ap_rptr = slot;
		return rc;
	}

	if (copy_to_user(buf, (char *)hdr + sizeof(hdr), hdr->length)) {
		printk(KERN_ERR "direct_rb: %s: copy_to_user failed.\n", __func__);
		return rc;
	}
	/* save packet length before advancing reader pointer */
	packet_len = hdr->length;

	/* advance reader pointer */
	skctl->ap_rptr = slot;

	return packet_len;

}
EXPORT_SYMBOL(direct_rb_recv);

void direct_rb_broadcast_msg(int proc)
{
	struct direct_rbctl *dir_ctl;
	unsigned long flags;

	const struct direct_rbctl *dir_ctl_end = direct_rbctl + direct_rb_type_total_cnt;
	for (dir_ctl = direct_rbctl; dir_ctl != dir_ctl_end; ++dir_ctl) {
		spin_lock_irqsave(&dir_ctl->rb_rx_lock, flags);
		if(dir_ctl->refcount == 0)
		{
			spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
			continue;
		}
		if(proc == MsocketLinkdownProcId)
		{
			/*If receive linkdown message, linkup message have received is out
			of date, so clear it.*/
			dir_ctl->is_linkup_msg_received = false;
			dir_ctl->is_linkdown_msg_received = true;
		}
		else if(proc == MsocketLinkupProcId)
		{
			dir_ctl->is_linkup_msg_received = true;
		}
		spin_unlock_irqrestore(&dir_ctl->rb_rx_lock, flags);
		wake_up_interruptible(&(dir_ctl->rb_rx_wq));
	}
}
