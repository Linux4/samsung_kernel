/*
    Marvell SSIPC misc driver for Linux
    Copyright (C) 2010 Marvell International Ltd.

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
#ifdef CONFIG_SSIPC_SUPPORT
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "shm.h"
#include "shm_share.h"
#include "msocket.h"
#include "seh_linux.h"

#define DPRINT(fmt, args ...)   pr_info("SSIPC MISC: " fmt, ## args)
#define DBGMSG(fmt, args ...)   pr_debug("SSIPC MISC: " fmt, ## args)
#define ERRMSG(fmt, args ...)   pr_err("SSIPC MISC: " fmt, ## args)

#define SHMEM_RECV_LIMIT 2048
#define SHMEM_PAYLOAD_LIMIT (SHMEM_RECV_LIMIT - sizeof(ShmApiMsg) \
		- sizeof(struct shm_skhdr))

#define SSIPC_START_PROC_ID	0x06
#define SSIPC_DATA_PROC_ID	0x04

#define MAX_IOD_RXQ_LEN 2000
#define MAX_IOD_RXQ_HI_WM 1000
#define MAX_IOD_RXQ_LOW_WM 500

static struct class *misc_class;
static int misc_major;
#define SSIPC_MISC_NAME "ssipc_misc"
struct _long_packet_header {
	u16 total_len;
	u16 seq;
};
struct _long_packet {
	int total_len;
	int cur_len;
	int cur_seq;
	struct sk_buff *long_skb;
};

struct io_device {
	char *name;
	char *app;
	int   port;
	struct device *dev;
	int misc_minor;
	wait_queue_head_t wq;
	struct sk_buff_head sk_rx_q;
	int long_packet_enable;
	struct _long_packet *long_packet;
	int sock_fd;
	struct task_struct *misc_init_task_ref;
	int channel_inited;

	/* Reference count */
	atomic_t opened;

	/*channel status*/
	int channel_status;
	struct work_struct init_work;
	struct work_struct deinit_work;
	struct workqueue_struct *workq;
};

static int ssipc_dsds;
#define PORTS_GOURP_ONE_NUM 12
static struct io_device umts_io_devices[] = {
	[0] = {
	       .name = "umts_boot0",
	       .app = "s-ril",
	       .port = 0},
	[1] = {
	       .name = "umts_ipc0",
	       .app = "s-ril",
	       .port = CISTUB_PORT,
	       .long_packet_enable = 1},
	[2] = {
	       .name = "umts_attest0",
	       .app = "serial_client",
	       .port = RAW_AT_PORT},
	[3] = {
	       .name = "umts_atdun0",
	       .app = "pc modem",
	       .port = RAW_AT_DUN_PORT},
	[4] = {
	       .name = "umts_atprod0",
	       .app = "atd",
	       .port = RAW_AT_PROD_PORT},
	[5] = {
	       .name = "umts_atsimal0",
	       .app = "simal",
	       .port = RAW_AT_SIMAL_PORT},
	[6] = {
	       .name = "umts_atsol0",
	       .app = "at_router",
	       .port = RAW_AT_CLIENT_SOL_PORT},
	[7] = {
	       .name = "umts_atunsol0",
	       .app = "at_router",
	       .port = RAW_AT_CLIENT_UNSOL_PORT},
	[8] = {
	       .name = "umts_at0",
	       .app = "reserved0_1",
	       .port = RAW_AT_RESERVERED_PORT},
	[9] = {
	       .name = "umts_atgps0",
	       .app = "gpsagent",
	       .port = RAW_AT_GPS_PORT},
	[10] = {
		.name = "umts_rfs0",
		.app = "s-ril",
		.port = NVMSRV_PORT},
	/*here may add rfs node
	 * need increase PORTS_GOURP_ONE_NUM after add rfs
	 * */
	[PORTS_GOURP_ONE_NUM - 1] = {
	       .name = "umts_at0_1",
	       .app = "reserved0_2",
	       .port = RAW_AT_RESERVERED2_PORT},
	[PORTS_GOURP_ONE_NUM] = {
	       .name = "umts_ipc1",
	       .app = "s-ril 2",
	       .port = CIDATASTUB_PORT,
	       .long_packet_enable = 1},
	[PORTS_GOURP_ONE_NUM + 1] = {
	       .name = "umts_attest1",
	       .app = "serial_client 2",
	       .port = RAW_AT_PORT2},
	[PORTS_GOURP_ONE_NUM + 2] = {
	       .name = "umts_atdun1",
	       .app = "ppp modem 2",
	       .port = RAW_AT_DUN_PORT2},
	[PORTS_GOURP_ONE_NUM + 3] = {
	       .name = "umts_atprod1",
	       .app = "atd 2",
	       .port = RAW_AT_PROD_PORT2},
	[PORTS_GOURP_ONE_NUM + 4] = {
	       .name = "umts_atsimal1",
	       .app = "simal 2",
	       .port = RAW_AT_SIMAL_PORT2},
	[PORTS_GOURP_ONE_NUM + 5] = {
	       .name = "umts_atsol1",
	       .app = "at_router 2",
	       .port = RAW_AT_CLIENT_SOL_PORT2},
	[PORTS_GOURP_ONE_NUM + 6] = {
	       .name = "umts_atunsol1",
	       .app = "at_router 2",
	       .port = RAW_AT_CLIENT_UNSOL_PORT2},
	[PORTS_GOURP_ONE_NUM + 7] = {
	       .name = "umts_atgps1",
	       .app = "gpsagent 2",
	       .port = RAW_AT_GPS_PORT2},
	[PORTS_GOURP_ONE_NUM + 8] = {
	       .name = "umts_at1",
	       .app = "reserved1_0",
	       .port = RAW_AT_RESERVERED_PORT2},
	[PORTS_GOURP_ONE_NUM + 9] = {
	       .name = "umts_at1_1",
	       .app = "reserved1_1",
	       .port = RAW_AT_RESERVERED2_PORT2},
};

/* polling modem status */
#define IOCTL_MODEM_STATUS		_IO('o', 0x27)
/* trigger modem force reset */
#define IOCTL_MODEM_RESET		_IO('o', 0x21)
/* trigger modem crash, final action rely on EE_CFG in NVM */
#define IOCTL_MODEM_FORCE_CRASH_EXIT		_IO('o', 0x34)

/* trigger ssipc channel start work */
#define IOCTL_MODEM_CHANNEL_START		_IO('o', 0x35)
/* trigger ssipc channel stop work */
#define IOCTL_MODEM_CHANNEL_STOP		_IO('o', 0x36)
/* enable ssipc dsds channels */
#define IOCTL_MODEM_CHANNEL_DSDS		_IO('o', 0x37)

/* modem state report for SSIPC use
 * currently only report OFFLINE/ONLINE/CRASH_RESET
 */

enum modem_state {
	STATE_OFFLINE,
	STATE_CRASH_RESET,
	STATE_CRASH_EXIT,
	STATE_BOOTING,
	STATE_ONLINE,
	STATE_NV_REBUILDING,
	STATE_LOADER_DONE,
	STATE_SIM_ATTACH,
	STATE_SIM_DETACH,
};

static const char const *modem_state_str[] = {
	[STATE_OFFLINE]		= "OFFLINE",
	[STATE_CRASH_RESET]	= "CRASH_RESET",
	[STATE_CRASH_EXIT]	= "CRASH_EXIT",
	[STATE_BOOTING]		= "BOOTING",
	[STATE_ONLINE]		= "ONLINE",
	[STATE_NV_REBUILDING]	= "NV_REBUILDING",
	[STATE_LOADER_DONE]	= "LOADER_DONE",
	[STATE_SIM_ATTACH]	= "SIM_ATTACH",
	[STATE_SIM_DETACH]	= "SIM_DETACH",
};

static void ssipc_recv_callback(struct sk_buff *skb, void *arg);
static void io_channel_init(struct io_device *iod);
static void io_channel_deinit(struct io_device *iod);

static const inline char *get_modem_state_str(int state)
{
	return modem_state_str[state];
}

static int if_msocket_connect(void)
{
	return cp_is_synced;
}

static int get_modem_state(struct io_device *iod)
{
	if (!iod->port)
		return if_msocket_connect() ? STATE_ONLINE : STATE_OFFLINE;

	else
		return iod->channel_status;
}

/* check if rx queue is below low water-mark */
static inline int rxlist_below_lo_wm(struct io_device *iod)
{
	return skb_queue_len(&iod->sk_rx_q) < MAX_IOD_RXQ_LOW_WM;
}

/* check if rx queue is above high water-mark */
static inline int rxlist_above_hi_wm(struct io_device *iod)
{
	return skb_queue_len(&iod->sk_rx_q) >= MAX_IOD_RXQ_HI_WM;
}

static inline int queue_skb_to_iod(struct sk_buff *skb, struct io_device *iod)
{
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	struct sk_buff *victim;

	skb_queue_tail(rxq, skb);

	if (rxlist_above_hi_wm(iod))
		msocket_recv_throttled(iod->sock_fd);
	if (rxq->qlen > MAX_IOD_RXQ_LEN) {
		ERRMSG("%s: %s application may be dead (rxq->qlen %d > %d)\n",
			iod->name, iod->app ? iod->app : "corresponding",
			rxq->qlen, MAX_IOD_RXQ_LEN);
		victim = skb_dequeue(rxq);
		if (victim)
			dev_kfree_skb_any(victim);
		return -ENOSPC;
	} else {
		DBGMSG("%s: rxq->qlen = %d\n", iod->name, rxq->qlen);
		return 0;
	}
}

int packet_complete(struct sk_buff **skb_out, struct io_device *iod)
{
	int ret = 0;
	if (iod->long_packet_enable) {
		struct sk_buff *skb = *skb_out;
		struct _long_packet_header *header;
		u8 header_len = sizeof(struct _long_packet_header);
		u16 msg_len;
		u16 cur_seq;
		u16 total_len;
		pr_debug("%s: long packet!!!(skb_len, %d)\n",
				__func__, skb->len);
		if (skb->len <= header_len) {
			pr_err("%s: error long packet detected, too small.\n",
					__func__);
			goto error_out;
		}
		header = (struct _long_packet_header *)skb->data;
		msg_len = skb->len - header_len;
		total_len = header->total_len;
		cur_seq = header->seq;
		pr_debug("%s: msg_len: %d, total_len:%d, seq:%d\n", __func__,
					msg_len, total_len, cur_seq);
		if (!iod->long_packet) {
			if (msg_len == total_len) {
				ret = 1;
				skb_pull(skb, header_len);
				goto out;
			}
			iod->long_packet = kzalloc(sizeof(struct _long_packet),
						GFP_KERNEL);
			if (!iod->long_packet) {
				pr_err("%s: alloc long_packet error\n",
						__func__);
				goto error_out;
			}
			iod->long_packet->long_skb =
					alloc_skb(total_len, GFP_KERNEL);
			if (!iod->long_packet->long_skb) {
				pr_err("%s: alloc long_packet skb error.\n",
						__func__);
				goto error_out;
			}
			iod->long_packet->total_len = total_len;
		} else {
			pr_debug("%s: lp->cur: %d, lp->total:%d, lp->seq:%d\n",
					__func__,
					iod->long_packet->cur_len,
					iod->long_packet->total_len,
					iod->long_packet->cur_seq);
			if (cur_seq != iod->long_packet->cur_seq + 1) {
				pr_err("%s: error long packet detected, seq error.\n",
					__func__);
				goto error_out;
			} else if (iod->long_packet->cur_len + msg_len >
					iod->long_packet->total_len) {
				pr_err("%s: error long packet detected, lens error.\n",
					__func__);
				goto error_out;
			}
		}
		skb_pull(skb, header_len);
		memcpy(skb_put(iod->long_packet->long_skb, msg_len),
				skb->data, skb->len);
		kfree_skb(skb);
		iod->long_packet->cur_len += msg_len;
		iod->long_packet->cur_seq = cur_seq;
		if (iod->long_packet->cur_len ==
				iod->long_packet->total_len) {
			*skb_out = iod->long_packet->long_skb;
			iod->long_packet->long_skb = NULL;
			kfree(iod->long_packet);
			iod->long_packet = NULL;
			ret = 1;
		}
		goto out;
error_out:
		kfree_skb(skb);
		skb = NULL;
		if (iod->long_packet) {
			if (iod->long_packet->long_skb) {
				kfree_skb(iod->long_packet->long_skb);
				iod->long_packet->long_skb = NULL;
			}
			kfree(iod->long_packet);
			iod->long_packet = NULL;
		}
	} else
		ret = 1;
out:
	return ret;
}
static int rx_raw_misc(struct sk_buff *skb, struct io_device *iod)
{
	/* Remove the msocket header */
	skb_pull(skb, SHM_HEADER_SIZE);

	if (packet_complete(&skb, iod)) {
	queue_skb_to_iod(skb, iod);

	wake_up_interruptible(&iod->wq);
	}
	return 0;
}


static int misc_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(inode);
	struct io_device *iod;
	int ref_cnt;
	if (minor >= ARRAY_SIZE(umts_io_devices)) {
		ERRMSG("%s invlid minor detected", __func__);
		return -EINVAL;
	}

	iod = &umts_io_devices[minor];
	filp->private_data = (void *)iod;

	ref_cnt = atomic_inc_return(&iod->opened);

	DPRINT("%s (opened %d)\n", iod->name, ref_cnt);

	return 0;
}

static int misc_release(struct inode *inode, struct file *filp)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	int ref_cnt;

	skb_queue_purge(&iod->sk_rx_q);
	ref_cnt = atomic_dec_return(&iod->opened);

	DPRINT("%s (opened %d)\n", iod->name, ref_cnt);

	return 0;
}

static unsigned int misc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	int p_state = get_modem_state(iod);

	poll_wait(filp, &iod->wq, wait);

	if (!skb_queue_empty(&iod->sk_rx_q) && p_state != STATE_OFFLINE)
		return POLLIN | POLLRDNORM;

	if (p_state == STATE_CRASH_RESET
	    || p_state == STATE_CRASH_EXIT
	    || p_state == STATE_NV_REBUILDING) {
		return POLLHUP;
	} else {
		return 0;
	}
}

static void set_dsds_feature(int enable)
{
	ssipc_dsds = enable;
}

static int get_dsds_feature(void)
{
	return ssipc_dsds;
}

static void start_ssipc_channel(void)
{
	int i;
	int num_iodevs = ssipc_dsds ?
			ARRAY_SIZE(umts_io_devices) : PORTS_GOURP_ONE_NUM;
		for (i = 0; i < num_iodevs; i++)
			io_channel_init(&umts_io_devices[i]);
}

static void stop_ssipc_channel(void)
{
	int i;
	int num_iodevs = ssipc_dsds ?
			ARRAY_SIZE(umts_io_devices) : PORTS_GOURP_ONE_NUM;
		for (i = 0; i < num_iodevs; i++)
			io_channel_deinit(&umts_io_devices[i]);
}

static long misc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	int p_state;

	switch (cmd) {
	case IOCTL_MODEM_STATUS:
		p_state = get_modem_state(iod);
		DBGMSG("%s: IOCTL_MODEM_STATUS (state %s)\n",
				iod->name, get_modem_state_str(p_state));
		return p_state;
	case IOCTL_MODEM_FORCE_CRASH_EXIT:
		DPRINT("%s: IOCTL_MODEM_FORCE_CRASH_EXIT triggered\n",
				iod->name);
		trigger_modem_crash(0, "ssipc force crash");
		break;
	case IOCTL_MODEM_RESET:
		DPRINT("%s: IOCTL_MODEM_RESET triggered\n",
				iod->name);
		trigger_modem_crash(1, "ssipc force reset");
		break;
	case IOCTL_MODEM_CHANNEL_START:
		DPRINT("%s: IOCTL_MODEM_START triggered\n",
				iod->name);
		start_ssipc_channel();
		break;
	case IOCTL_MODEM_CHANNEL_STOP:
		DPRINT("%s: IOCTL_MODEM_STOP triggered\n",
				iod->name);
		stop_ssipc_channel();
		break;
	case IOCTL_MODEM_CHANNEL_DSDS:
		DPRINT("%s: IOCTL_MODEM_DSDS triggered\n",
				iod->name);
		set_dsds_feature(1);
		break;

	default:
		ERRMSG("%s: ERR! undefined cmd 0x%X\n", iod->name, cmd);
		return -EINVAL;
	}

	return 0;
}

static int ssipc_init_task(void *data)
{
	struct io_device *iod = (struct io_device *)data;
	ShmApiMsg shm_msg_hdr;

	while (!kthread_should_stop()) {
		if (!if_msocket_connect()) {
			msleep_interruptible(1000);
			continue;
		}
		break;
	}

	if (!if_msocket_connect()) {
		ERRMSG("%s: channel of %s fail & close\n",
				__func__, iod->name);
		return -1;
	}

	DPRINT("%s:io->port:%d opened\n", __func__, iod->port);
	if (iod->sock_fd == -1) {
		iod->sock_fd = msocket_with_cb(iod->port,
				(void *)ssipc_recv_callback, (void *)iod);
		if (iod->sock_fd < 0) {
			ERRMSG("%s: sock fd of %s opened fail\n",
					__func__, iod->name);
			return -1;
		}
	}

	shm_msg_hdr.svcId = iod->port;
	shm_msg_hdr.msglen = 0;
	shm_msg_hdr.procId = SSIPC_START_PROC_ID;

	while (!kthread_should_stop()) {
		msend(iod->sock_fd, (u8 *)&shm_msg_hdr,
				SHM_HEADER_SIZE, MSOCKET_ATOMIC);
		DBGMSG("%s: port:%d, send_handshake: %d!\n",
				iod->name,
				iod->port,
				iod->channel_inited);
		msleep_interruptible(1000);
	}

	DPRINT("%s: port:%d, init: %d!\n",
			iod->name, iod->port, iod->channel_inited);
	return 0;
}

/*note: the recv callback function is under atomic context which
 * need an work start the init thread*/
static void init_work(struct work_struct *work)
{
	struct io_device *iod = container_of(work, struct io_device, init_work);
	char init_task_name[50] = {0};
	sprintf(init_task_name, "init_t(%s)", iod->name);
	if (iod) {
		if (iod->misc_init_task_ref == NULL)
			iod->misc_init_task_ref =
			    kthread_run(ssipc_init_task, iod, init_task_name);
		else
			ERRMSG("%s, iod->misc_init_task_ref is not NULL",
				 __func__);
	}
}

static void deinit_work(struct work_struct *work)
{
	struct io_device *iod = container_of(work,
				struct io_device, deinit_work);
	if (iod) {
		if (iod->misc_init_task_ref)
			kthread_stop(iod->misc_init_task_ref);
		iod->misc_init_task_ref = NULL;
	}
}

static int event_handler(struct sk_buff *skb, struct io_device *iod)
{
	ShmApiMsg *shm_msg_hdr;
	u8 *rxmsg = skb->data;
	int handled = 1;

	shm_msg_hdr = (ShmApiMsg *) rxmsg;
	if (shm_msg_hdr->svcId != iod->port) {
		ERRMSG("%s, svcId(%d) is incorrect, expect %d",
			__func__, shm_msg_hdr->svcId, iod->port);
		return handled;
	}

	DBGMSG("%s,srvId=%d, procId=%d, len=%d\n",
		__func__,
		shm_msg_hdr->svcId,
		shm_msg_hdr->procId,
		shm_msg_hdr->msglen);

	switch (shm_msg_hdr->procId) {
	case SSIPC_START_PROC_ID:
		iod->channel_inited = 1;
		iod->channel_status = STATE_ONLINE;
		queue_work(iod->workq, &iod->deinit_work);
		break;
	case SSIPC_DATA_PROC_ID:
		if (atomic_read(&iod->opened) <= 0)
			break;
		if (iod->channel_inited) {
			rx_raw_misc(skb, iod);
			handled = 0;
		}
		break;
	case MsocketLinkdownProcId:
		DPRINT("%s: %s: received  MsocketLinkdownProcId!\n",
				__func__, iod->name);
		iod->channel_inited = 0;
		if (read_ee_config_b_cp_reset() == 1)
			iod->channel_status = STATE_CRASH_RESET;
		else
			iod->channel_status = STATE_CRASH_EXIT;
		wake_up_interruptible(&iod->wq);
		queue_work(iod->workq, &iod->deinit_work);
		break;
	case MsocketLinkupProcId:
		DPRINT("%s: %s: received  MsocketLinkupProcId!\n",
				__func__, iod->name);
		iod->channel_status = STATE_OFFLINE;
		skb_queue_purge(&iod->sk_rx_q);
		queue_work(iod->workq, &iod->init_work);
		break;
	}
	return handled;
}

static void ssipc_recv_callback(struct sk_buff *skb, void *arg)
{
	struct io_device *iod = (struct io_device *)arg;
	int handled = 0;

	if (!skb || !arg) {
		ERRMSG("%s: channel of %s got invalid parameters\n",
				__func__, iod->name);
		return;
	}

	handled = event_handler(skb, iod);
	if (handled) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void io_channel_init(struct io_device *iod)
{
	iod->channel_status = STATE_OFFLINE;
	iod->sock_fd = -1;

	if (!iod->port)
		return;

	queue_work(iod->workq, &iod->init_work);
}

static void io_channel_deinit(struct io_device *iod)
{
	if (iod->sock_fd != -1)
		mclose(iod->sock_fd);
	iod->sock_fd = -1;
	if (iod->misc_init_task_ref)
		kthread_stop(iod->misc_init_task_ref);
	iod->misc_init_task_ref = NULL;
}

static ssize_t misc_write(struct file *filp, const char __user *data,
			size_t count, loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;

	struct sk_buff *skb;
	struct shm_skhdr *hdr;
	ShmApiMsg *pshm;

	if (get_modem_state(iod) != STATE_ONLINE) {
		pr_debug_ratelimited("%s: channel(%s) is not ready\n",
				__func__, iod->name);
		return -EAGAIN;
	}

	if (count > SHMEM_PAYLOAD_LIMIT) {
		ERRMSG("%s: DATA size bigger than buffer size\n", __func__);
		return -ENOMEM;
	}

	skb = alloc_skb(count + sizeof(*hdr)+sizeof(*pshm), GFP_KERNEL);
	if (!skb) {
		ERRMSG("Data_channel: %s: out of memory.\n", __func__);
		return -ENOMEM;
	}

	skb_reserve(skb, sizeof(*hdr) + sizeof(*pshm));
	if (copy_from_user(skb_put(skb, count), data, count)) {
		kfree_skb(skb);
		ERRMSG("%s: %s: copy_from_user failed.\n",
		       __func__, iod->name);
		return -EFAULT;
	}

	pshm = (ShmApiMsg *)skb_push(skb, sizeof(*pshm));
	pshm->msglen = count;
	pshm->procId = SSIPC_DATA_PROC_ID;
	pshm->svcId = iod->port;
	msendskb(iod->port, skb, count + SHM_HEADER_SIZE, MSOCKET_KERNEL);

	return count;
}

static ssize_t misc_read(struct file *filp, char *buf, size_t count,
			loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	struct sk_buff *skb;
	int copied = 0;

	if (skb_queue_empty(rxq) && filp->f_flags & O_NONBLOCK) {
		DPRINT("%s: ERR! no data in rxq\n", iod->name);
		return -EAGAIN;
	}

	while (skb_queue_empty(rxq)) {
		if (wait_event_interruptible(iod->wq,
			(!skb_queue_empty(rxq) ||
			 (get_modem_state(iod) != STATE_ONLINE)))) {
			return -ERESTARTSYS;
		}
		if (get_modem_state(iod) != STATE_ONLINE) {
			pr_debug_ratelimited("%s: channel(%s) is not ready\n",
					__func__, iod->name);
			return 0;
		}
	}

	skb = skb_dequeue(rxq);
	if (rxlist_below_lo_wm(iod))
		msocket_recv_unthrottled(iod->sock_fd);

	copied = skb->len > count ? count : skb->len;

	if (copy_to_user(buf, skb->data, copied)) {
		ERRMSG("%s: ERR! copy_to_user fail\n", iod->name);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	DBGMSG("%s: data:%d copied:%d qlen:%d\n",
		iod->name, skb->len, copied, rxq->qlen);

	if (skb->len > count) {
		skb_pull(skb, count);
		skb_queue_head(rxq, skb);
	} else {
		dev_kfree_skb_any(skb);
	}

	return copied;
}

static const struct file_operations misc_io_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.poll = misc_poll,
	.unlocked_ioctl = misc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = misc_ioctl,
#endif
	.write = misc_write,
	.read = misc_read,
};

static ssize_t ssipc_dsds_store(struct device *sys_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int enable;

	if (kstrtoint(buf, 10, &enable) < 0)
		return 0;
	set_dsds_feature(enable);
	DPRINT("%s: buf={%s}, enable=%d\n", __func__, buf, enable);
	return len;
}

static ssize_t ssipc_dsds_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len;
	int enable;

	enable = get_dsds_feature();
	len = sprintf(buf, "%d\n", enable);
	return len;
}

static ssize_t ssipc_ch_enable_store(struct device *sys_dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int enable;

	if (kstrtoint(buf, 10, &enable) < 0)
		return 0;
	if (!enable)
		stop_ssipc_channel();
	else
		start_ssipc_channel();
	DPRINT("%s: buf={%s}, enable=%d\n", __func__, buf, enable);
	return len;
}

static ssize_t ssipc_ch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int len;
	int status;

	status = if_msocket_connect() ? STATE_ONLINE : STATE_OFFLINE;
	len = sprintf(buf, "%d\n", status);
	return len;
}

static DEVICE_ATTR(ssipc_dsds, 0644, ssipc_dsds_show, ssipc_dsds_store);
static DEVICE_ATTR(ssipc_ch_enable, 0644,
		ssipc_ch_enable_show, ssipc_ch_enable_store);
static struct device_attribute *ssipc_attr[] = {
	&dev_attr_ssipc_dsds,
	&dev_attr_ssipc_ch_enable,
};

static int ssipc_attr_add(struct device *dev)
{
	int i = 0, n;
	int ret;
	struct device_attribute *attr = NULL;

	n = ARRAY_SIZE(ssipc_attr);
	while ((attr = ssipc_attr[i++]) != NULL) {
		ret = device_create_file(dev, attr);
		if (ret)
			return -EIO;
	}
	return 0;
}

static int ssipc_attr_rm(struct device *dev)
{
	int i = 0, n;
	struct device_attribute *attr = NULL;

	n = ARRAY_SIZE(ssipc_attr);
	while ((attr = ssipc_attr[i++]) != NULL)
		device_remove_file(dev, attr);
	return 0;
}

static int init_io_device(struct io_device *iod)
{
	int ret = 0;
	char workq_name[50] = {0};
	sprintf(workq_name, "init_wq(%s)", iod->name);

	/* init misc */
	atomic_set(&iod->opened, 0);

	init_waitqueue_head(&iod->wq);
	skb_queue_head_init(&iod->sk_rx_q);
	iod->misc_init_task_ref = NULL;
	iod->channel_status = STATE_OFFLINE;
	iod->sock_fd = -1;

	INIT_WORK(&iod->init_work, init_work);
	INIT_WORK(&iod->deinit_work, deinit_work);
	iod->workq = create_workqueue(workq_name);
	if (iod->workq == NULL) {
		ERRMSG("%s:Can't create work queue!\n", __func__);
		return -1;
	}

	/* register misc */
	iod->dev = device_create(misc_class, NULL,
				MKDEV(misc_major, iod->misc_minor),
				iod, "%s", iod->name);
	if (IS_ERR(iod->dev)) {
		ERRMSG("%s: ERR! device_create failed\n", iod->name);
		if (iod->workq) {
			destroy_workqueue(iod->workq);
			iod->workq = NULL;
		}
		return -1;
	}

	/* register sys node */
	if (!iod->port)
		ssipc_attr_add(iod->dev);

	DPRINT("%s is created\n", iod->name);
	return ret;
}

static void deinit_io_device(struct io_device *iod)
{

	/* unregister sys node */
	if (!iod->port)
		ssipc_attr_rm(iod->dev);

	/* release misc */
	device_destroy(misc_class, MKDEV(misc_major, iod->misc_minor));

	if (iod->workq) {
		destroy_workqueue(iod->workq);
		iod->workq = NULL;
	}

	io_channel_deinit(iod);

	DPRINT("%s is released\n", iod->name);
}

/* module initialization */
static int __init ssipc_misc_init(void)
{
	int i, ret = 0;
	int num_iodevs = ARRAY_SIZE(umts_io_devices);

	misc_class = class_create(THIS_MODULE, SSIPC_MISC_NAME);
	ret = PTR_ERR(misc_class);
	if (IS_ERR(misc_class)) {
		ERRMSG("%s: create %s class failed\n", __func__,
			SSIPC_MISC_NAME);
		goto misc_class_fail;
	}

	misc_major = register_chrdev(0, SSIPC_MISC_NAME, &misc_io_fops);
	if (misc_major < 0) {
		ERRMSG("%s: register chrdev failed\n", __func__);
		ret = misc_major;
		goto chrdev_fail;
	}

	/* init io deivces and connect to modemctl device */
	for (i = 0; i < num_iodevs; i++) {
		struct io_device *iod = &umts_io_devices[i];
		iod->misc_minor = i;
		ret = init_io_device(iod);
		if (ret) {
			ERRMSG("%s: init %s io device fail\n", __func__,
					iod->name);
			goto err_deinit;
		}
	}
	ssipc_dsds = 0;
	return ret;

err_deinit:
	for (--i; i >= 0; i--)
		deinit_io_device(&umts_io_devices[i]);
	unregister_chrdev(misc_major, SSIPC_MISC_NAME);
chrdev_fail:
	class_destroy(misc_class);
misc_class_fail:
	return ret;
}

/* module exit */
static void __exit ssipc_misc_exit(void)
{
	int i;
	int num_iodevs = ARRAY_SIZE(umts_io_devices);

	/* deinit io deivces and connect to modemctl device */
	for (i = 0; i < num_iodevs; i++)
		deinit_io_device(&umts_io_devices[i]);

	unregister_chrdev(misc_major, SSIPC_MISC_NAME);
	class_destroy(misc_class);

	ssipc_dsds = 0;
}

module_init(ssipc_misc_init);
module_exit(ssipc_misc_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell SSIPC misc Driver");
#endif
