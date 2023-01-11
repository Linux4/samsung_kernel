/*
    Marvell Audio Stub driver for Linux
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/semaphore.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include "msocket.h"
#include "shm_share.h"
#include "portqueue.h"
#include "shm.h"
#include "audio_stub.h"

static void start_handshake(void);
static void stop_handshake(void);

static int audiostub_major;
static int audiostub_minor;
static struct cdev *audiodev_devices;
static struct class *audiodev_class;

static u32 pending_msgid;
static u32 pcm_pktsize;
static int audiostubfd = -1;
static bool pcm_wb = true;
static bool pcm_master = true;
static bool audiostub_inited;
static bool pcm_rx_opened, pcm_tx_opened;
static u16 mute_input_cnt, mute_output_cnt;
static struct task_struct *recv_taskref;
static struct task_struct *handshake_taskref;
static struct sk_buff_head ctl_rxq, pcm_rxq, pcm_txq;
static u32 pcm_rxcnt, pcm_txcnt, pcm_underrun_cnt;

#define ATC_VER 0x12
#define MAKE_REQ_HANDLE() (++pending_msgid)
static DEFINE_MUTEX(ioctl_lock);
static DEFINE_SEMAPHORE(pcm_rxsema);
static DEFINE_SEMAPHORE(pcm_txsema);
static DECLARE_COMPLETION(ioctl_completion);
static DECLARE_WAIT_QUEUE_HEAD(pcm_rxwq);
static DECLARE_WAIT_QUEUE_HEAD(pcm_txwq);
static DECLARE_WAIT_QUEUE_HEAD(pcm_drainwq);

#define AUDIO_CTL_OFFSET 0
#define AUDIO_PCM_OFFSET 1

#define AUDIO_DEVICE_CNT 2
#define MAX_AUDIO_RXMSLEN 1024
#define MAX_AUDIO_PACKET_NUM 50
#define PCM_WRITE_TIMEOUT 5	/* sec */
#define IOCTL_TIMEOUT	1	/* sec */

static struct sk_buff *malloc_audio_skb(const char *buf, int len, int flags)
{
	struct sk_buff *skb;

	skb = alloc_skb(len + sizeof(struct shm_skhdr)
			+ sizeof(struct atc_header), flags);
	if (!skb) {
		pr_err("%s: out of memory.\n", __func__);
		return NULL;
	}
	skb_reserve(skb, sizeof(struct shm_skhdr) + sizeof(struct atc_header));
	memcpy(skb_put(skb, len), buf, len);
	skb_push(skb, sizeof(struct atc_header));
	return skb;
}

static inline int audio_tx_rawskb(struct sk_buff *skb)
{
	return msendskb(audiostubfd, skb, skb->len, MSOCKET_KERNEL);
}

static int audio_send(u8 *msg, int msglen, int sub_cmd, int cmd_type)
{
	struct atc_header *header;
	struct sk_buff *skb;
	int ret;

	skb = malloc_audio_skb(msg, msglen, GFP_KERNEL);
	if (!skb) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}
	header = (struct atc_header *)skb->data;
	header->data_len = msglen;
	header->cmd_code = AUDIO_CMD_CODE;
	header->sub_cmd = (u8) sub_cmd;
	header->cmd_type = (u8) cmd_type;
	ret = audio_tx_rawskb(skb);
	if (ret < 0)
		return -EFAULT;
	return 0;
}

static inline int audio_tx_rawdata(u8 *msg, int msglen)
{
	return msend(audiostubfd, msg, msglen, MSOCKET_KERNEL);
}

/*
 * Check if mute operation is indeed needed to avoid
 * possible race condition with multiple mute commands.
 * Returns 1 if needed, 0 otherwise.
 */
static int audio_mute_check(struct mute_ctlmsg *mute)
{
	if (mute->direction == 0) {
		if (mute->mute) {
			mute_input_cnt++;
			if (mute_input_cnt == 1)
				return 1;
		} else if (mute_input_cnt != 0) {
			mute_input_cnt--;
			if (mute_input_cnt == 0)
				return 1;
		}
	} else if (mute->direction == 1) {
		if (mute->mute) {
			mute_output_cnt++;
			if (mute_output_cnt == 1)
				return 1;
		} else if (mute_output_cnt != 0) {
			mute_output_cnt--;
			if (mute_output_cnt == 0)
				return 1;
		}
	}

	return 0;
}

static int audiostub_open(struct inode *inode, struct file *filp)
{
	int minor = MINOR(inode->i_cdev->dev) - audiostub_minor;
	pr_debug("open minor=%d mode=%u\n", minor, filp->f_flags & O_ACCMODE);
	if (!audiostub_inited)
		return -EPIPE;

	if (minor == AUDIO_CTL_OFFSET) {
		filp->private_data = (void *)AUDIO_CTL_OFFSET;
	} else if (minor == AUDIO_PCM_OFFSET) {
		if ((filp->f_flags & O_ACCMODE) == O_RDONLY) {
			if (down_trylock(&pcm_rxsema)) {
				if (filp->f_flags & O_NONBLOCK)
					return -EAGAIN;
				pr_debug("open contention, minor=%d mode=%u\n",
				       minor, filp->f_flags & O_ACCMODE);
				if (down_interruptible(&pcm_rxsema))
					return -ERESTARTSYS;
			}
			skb_queue_head_init(&pcm_rxq);
			pcm_rxcnt = 0;
			pcm_rx_opened = true;
		} else if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
			if (down_trylock(&pcm_txsema)) {
				if (filp->f_flags & O_NONBLOCK)
					return -EAGAIN;
				pr_debug("open contention, minor=%d mode=%u\n",
				       minor, filp->f_flags & O_ACCMODE);
				if (down_interruptible(&pcm_txsema))
					return -ERESTARTSYS;
			}
			skb_queue_head_init(&pcm_txq);
			pcm_txcnt = 0;
			pcm_underrun_cnt = 0;
			pcm_tx_opened = true;
		} else
			return -EINVAL;

		filp->private_data = (void *)AUDIO_PCM_OFFSET;
	}

	return 0;
}

static int audiostub_close(struct inode *inode, struct file *filp)
{
	int minor = MINOR(inode->i_cdev->dev) - audiostub_minor;
	pr_debug("close minor=%d mode=%u\n", minor, filp->f_flags & O_ACCMODE);
	if (minor == AUDIO_PCM_OFFSET) {
		if ((filp->f_flags & O_ACCMODE) == O_RDONLY) {
			pr_debug("rx close len=%d\n", skb_queue_len(&pcm_rxq));
			pcm_rx_opened = false;
			skb_queue_purge(&pcm_rxq);
			up(&pcm_rxsema);
		} else if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
			pr_debug("tx close len=%d\n", skb_queue_len(&pcm_txq));
			pcm_tx_opened = false;
			skb_queue_purge(&pcm_txq);
			up(&pcm_txsema);
		}
	}
	return 0;
}

static ssize_t audiostub_read(struct file *file, char __user *data, size_t len,
			      loff_t *ppos)
{
	struct sk_buff *skb;
	size_t copied_bytes = 0;
	struct pcm_stream_data *pcm_data;
	int ret;

	if (file->private_data != (void *)AUDIO_PCM_OFFSET)
		return -ENODEV;

	while (len > 0) {
		skb = skb_dequeue(&pcm_rxq);
		if (skb == NULL) {
			if (copied_bytes == 0) {
				if (file->f_flags & O_NONBLOCK)
					return -EAGAIN;
				ret = wait_event_interruptible(pcm_rxwq,
					!skb_queue_empty(&pcm_rxq)
					|| !audiostub_inited);
				if (ret)
					return ret;

				if (unlikely(!audiostub_inited))
					return -EPIPE;

				skb = skb_dequeue(&pcm_rxq);
				if (unlikely(!skb))
					return -EFAULT;
			} else
				return copied_bytes;
		}

		pcm_data = (struct pcm_stream_data *)(skb->data +
					       sizeof(struct atc_header));
		pr_debug("audio read len=%d packet len=%u\n",
				(int)len, pcm_data->len);
		if (len < pcm_data->len) {
			skb_queue_head(&pcm_rxq, skb);
			return copied_bytes;
		}
		if (copy_to_user
		    (data + copied_bytes, pcm_data->data, pcm_data->len)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		copied_bytes += pcm_data->len;
		len -= pcm_data->len;
		kfree_skb(skb);
	}
	return copied_bytes;
}

static ssize_t audiostub_write(struct file *file, const char __user *data,
			       size_t len, loff_t *ppos)
{
	struct sk_buff *skb;
	struct atc_header *header;
	size_t copied_bytes = 0, pkt_size = pcm_pktsize;
	int ret;

	if (unlikely(file->private_data != (void *)AUDIO_PCM_OFFSET))
		return -ENODEV;

	if (unlikely(!audiostub_inited))
		return -EPIPE;

	if (unlikely(pkt_size == 0))
		return -EFAULT;

	while (len > 0) {
		if (len < pkt_size) {
			if (copied_bytes == 0)
				return -EINVAL;
			else
				return copied_bytes;
		}

		if (skb_queue_len(&pcm_txq) == MAX_AUDIO_PACKET_NUM) {
			if (copied_bytes == 0) {
				if (file->f_flags & O_NONBLOCK)
					return -EAGAIN;

				ret = wait_event_interruptible_timeout(
					pcm_txwq, (skb_queue_len(&pcm_txq)
						< MAX_AUDIO_PACKET_NUM)
						|| !audiostub_inited,
						PCM_WRITE_TIMEOUT * HZ);
				if (ret < 0)
					return ret;
				else if (ret == 0)
					return -ETIMEDOUT;

				if (unlikely(!audiostub_inited))
					return -EPIPE;
			} else
				return copied_bytes;
		}

		skb = alloc_skb(pkt_size + sizeof(struct shm_skhdr) +
				  sizeof(struct atc_header) +
				  sizeof(struct pcm_stream_data), GFP_KERNEL);
		if (!skb)
			return -ENOMEM;
		skb_reserve(skb, sizeof(struct shm_skhdr) +
				sizeof(struct atc_header) +
				sizeof(struct pcm_stream_data));
		if (copy_from_user(skb_put(skb, pkt_size),
				data + copied_bytes, pkt_size)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		header = (struct atc_header *)skb_push(skb,
				sizeof(struct atc_header) +
				sizeof(struct pcm_stream_data));
		header->cmd_code = AUDIO_CMD_CODE;
		header->sub_cmd = ATC_PCMPLAYSTREAM;
		header->cmd_type = CMD_TYPE_RESPONSE;
		header->data_len = sizeof(struct pcm_stream_data);
		skb_queue_tail(&pcm_txq, skb);
		copied_bytes += pkt_size;
		len -= pkt_size;
	}
	return copied_bytes;
}

static struct sk_buff *alloc_empty_data(void)
{
	struct sk_buff *skb;
	int len = pcm_pktsize;

	skb = alloc_skb(len + sizeof(struct shm_skhdr) +
		      sizeof(struct atc_header) +
		      sizeof(struct pcm_stream_data), GFP_ATOMIC | __GFP_ZERO);
	if (!skb)
		return NULL;

	skb_reserve(skb, sizeof(struct shm_skhdr));
	skb_put(skb, len + sizeof(struct atc_header) +
			sizeof(struct pcm_stream_data));
	return skb;
}

static unsigned int audiostub_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	if (filp->private_data != (void *)AUDIO_PCM_OFFSET)
		return 0;

	if ((filp->f_flags & O_ACCMODE) == O_RDONLY) {
		poll_wait(filp, &pcm_rxwq, wait);
		if (!skb_queue_empty(&pcm_rxq))
			mask |= POLLIN | POLLRDNORM;

	} else if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		poll_wait(filp, &pcm_txwq, wait);
		if (skb_queue_len(&pcm_txq) < MAX_AUDIO_PACKET_NUM)
			mask |= POLLOUT | POLLWRNORM;
	}

	return mask;
}

static int audio_ioctl_cmd(u8 *msg, unsigned long arg, int len, int sub_cmd)
{
	int ret, msgid;
	struct sk_buff *skb;
	struct atc_header *header;
	struct response_msg *reply;

	pr_debug("send ioctl cmd:%d\n", sub_cmd);
	if (copy_from_user(msg, (void __user *)arg, len))
		return -EFAULT;

	if (sub_cmd == ATC_MUTECTL) {
		ret = audio_mute_check((struct mute_ctlmsg *)msg);
		if (!ret)
			return 0;
	}

	mutex_lock(&ioctl_lock);
	msgid = MAKE_REQ_HANDLE();
	/* msg_id field is always at the end of cmd struct */
	*(u32 *) (msg + len - sizeof(reply->msg_id)) = msgid;
	ret = audio_send(msg, len, sub_cmd, CMD_TYPE_EXECUTE);
	if (ret < 0) {
		mutex_unlock(&ioctl_lock);
		return ret;
	}

	while (1) {
		ret = wait_for_completion_timeout(&ioctl_completion,
				IOCTL_TIMEOUT * HZ);
		if (!ret) {
			pr_err("<<<<ioctl cmd %d timeout>>>>\n", sub_cmd);
			mutex_unlock(&ioctl_lock);
			return -ETIMEDOUT;
		}
		/* go through all responses until desired one is found */
		skb = skb_dequeue(&ctl_rxq);
		if (unlikely(!skb)) {
			mutex_unlock(&ioctl_lock);
			return -EIO;
		}
		header = (struct atc_header *)skb->data;
		reply = (struct response_msg *)(skb->data +
					    sizeof(struct atc_header));
		if (header->sub_cmd == sub_cmd && reply->msg_id == msgid) {
			kfree_skb(skb);
			break;
		}
		kfree_skb(skb);
	}
	mutex_unlock(&ioctl_lock);
	return 0;
}

static long audiostub_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	long timeout;
	u8 msg[64];

	if (filp->private_data != (void *)AUDIO_CTL_OFFSET)
		return -ENOIOCTLCMD;

	if (unlikely(!audiostub_inited))
		return -EPIPE;

	switch (cmd) {
	case AUDIOSTUB_GET_STATUS:
		ret = put_user(pcm_wb | (pcm_master << 1), (int __user *)arg);
		break;
	case AUDIOSTUB_GET_WRITECNT:
		ret = put_user(pcm_txcnt, (u32 __user *) arg);
		break;
	case AUDIOSTUB_GET_READCNT:
		ret = put_user(pcm_rxcnt, (u32 __user *) arg);
		break;
	case AUDIOSTUB_SET_PKTSIZE:
		ret = get_user(pcm_pktsize, (u32 __user *) arg);
		break;
	case AUDIOSTUB_SET_CALLSTART:
		/* reset mute count on call start */
		mute_input_cnt = 0;
		mute_output_cnt = 0;
		break;
	case AUDIOSTUB_PCMPLAYBACK_DRAIN:
		ret = get_user(timeout, (u16 __user *) arg);
		if (!ret)
			break;
		if (timeout)
			timeout *= HZ;
		else
			timeout = MAX_SCHEDULE_TIMEOUT;
		ret = wait_event_interruptible_timeout(pcm_drainwq,
				skb_queue_empty(&pcm_txq) || !audiostub_inited,
				timeout);
		if (timeout != MAX_SCHEDULE_TIMEOUT && ret == 0)
			ret = -ETIMEDOUT;
		else if (ret > 0)
			ret = 0;
		break;
	case AUDIOSTUB_VOLUMECTL:
		ret = audio_ioctl_cmd(msg, arg, sizeof(struct volume_ctlmsg),
			ATC_VOLUMECTL);
		break;
	case AUDIOSTUB_MUTECTL:
		ret = audio_ioctl_cmd(msg, arg, sizeof(struct mute_ctlmsg),
			ATC_MUTECTL);
		break;
	case AUDIOSTUB_PATHCTL:
		ret = audio_ioctl_cmd(msg, arg, sizeof(struct path_ctlmsg),
			ATC_PATHCTL);
		break;
	case AUDIOSTUB_EQCTL:
		ret = audio_ioctl_cmd(msg, arg, sizeof(struct eq_ctlmsg),
			ATC_EQCTL);
		break;
	case AUDIOSTUB_LOOPBACKCTL:
		ret = audio_ioctl_cmd(msg, arg, sizeof(struct loop_ctlmsg),
			ATC_LOOPBACKCTL);
		break;
	case AUDIOSTUB_PCMRECCTL:
		ret = audio_ioctl_cmd(msg, arg,
			sizeof(struct pcm_record_ctlmsg), ATC_PCMRECCTL);
		break;
	case AUDIOSTUB_PCMPLAYBACKCTL:
		ret = audio_ioctl_cmd(msg, arg,
			sizeof(struct pcm_playback_ctlmsg),
			ATC_PCMPLAYBACKCTL);
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static const struct file_operations audiostub_fops = {
	.open = audiostub_open,
	.release = audiostub_close,
	.read = audiostub_read,
	.write = audiostub_write,
	.poll = audiostub_poll,
	.unlocked_ioctl = audiostub_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = audiostub_ioctl,
#endif
	.owner = THIS_MODULE
};

static void audio_data_handler(struct sk_buff *skb)
{
	void *audio_data;
	struct atc_header *header;

	header = (struct atc_header *)skb->data;
	audio_data = (u8 *) (skb->data + sizeof(struct atc_header));
	pr_debug_ratelimited("cmd_code:%u, sub_cmd:%u, cmd_type:%u, len:%u\n",
	       header->cmd_code, header->sub_cmd, header->cmd_type,
	       header->data_len);
	if (header->cmd_code != AUDIO_CMD_CODE) {
		pr_err("Invalid cmd code: %u\n", header->cmd_code);
		kfree_skb(skb);
		return;
	}

	if (header->cmd_type != CMD_TYPE_CONFIRM
	    && header->cmd_type != CMD_TYPE_INDICATION) {
		pr_err("Invalid cmd type: %u\n", header->cmd_type);
		kfree_skb(skb);
		return;
	}

	switch (header->sub_cmd) {
	case ATC_MSOCKET_LINKDOWN:
		pr_info("received MsocketLinkdown!\n");
		audiostub_inited = false;
		wake_up_interruptible(&pcm_rxwq);
		wake_up_interruptible(&pcm_txwq);
		wake_up_interruptible(&pcm_drainwq);
		stop_handshake();
		break;
	case ATC_MSOCKET_LINKUP:
		pr_info("received MsocketLinkup!\n");
		start_handshake();
		break;
	case ATC_HANDSHAKE:
		{
			struct handshake_msg *msg =
			    (struct handshake_msg *)audio_data;
			pr_info("audio_data_handler,pcm=%u, wb=%u, ver=%u\n",
				msg->pcm_master, msg->is_wb, msg->ver);
			pcm_master = msg->pcm_master > 0;
			pcm_wb = msg->is_wb > 0;
			audiostub_inited = true;
			stop_handshake();
			break;
		}
	case ATC_VOLUMECTL:
	case ATC_MUTECTL:
	case ATC_PATHCTL:
	case ATC_EQCTL:
	case ATC_LOOPBACKCTL:
	case ATC_PCMRECCTL:
	case ATC_PCMPLAYBACKCTL:
		skb_queue_tail(&ctl_rxq, skb);
		complete(&ioctl_completion);
		skb = NULL;
		break;
	case ATC_PCMRECSTREAM:
		if (pcm_rx_opened) {
			pcm_rxcnt++;
			if (skb_queue_len(&pcm_rxq) >= MAX_AUDIO_PACKET_NUM) {
				pr_err_ratelimited("PCM recv overflow\n");
				/* discard old packet */
				kfree_skb(skb_dequeue(&pcm_rxq));
			}
			skb_queue_tail(&pcm_rxq, skb);
			wake_up_interruptible(&pcm_rxwq);
			skb = NULL;
		} else
			pr_err("pcm rx device not opened yet\n");
		break;
	case ATC_PCMPLAYSTREAM:
		if (pcm_tx_opened) {
			struct pcm_stream_ind *pcm_indication =
			    (struct pcm_stream_ind *)audio_data;
			struct sk_buff *pcm_skb;
			int ret;
			struct pcm_stream_data *pcm_data;

			++pcm_txcnt;
			pcm_skb = skb_dequeue(&pcm_txq);
			if (!pcm_skb) {
				pr_err_ratelimited("PCM send underrun\n");
				++pcm_underrun_cnt;
				pcm_skb = alloc_empty_data();
				if (!pcm_skb)
					break;
				header = (struct atc_header *)pcm_skb->data;
				header->cmd_code = AUDIO_CMD_CODE;
				header->sub_cmd = ATC_PCMPLAYSTREAM;
				header->cmd_type = CMD_TYPE_RESPONSE;
				header->data_len = sizeof(*pcm_data);
				pcm_data = (struct pcm_stream_data *)
					(pcm_skb->data +
					sizeof(struct atc_header));
				pcm_data->msg_id = pcm_indication->msg_id;
				pcm_data->callback = pcm_indication->callback;
				pcm_data->len = pcm_pktsize;
				audio_tx_rawskb(pcm_skb);
			} else {
				pcm_data = (struct pcm_stream_data *)
					(pcm_skb->data +
					sizeof(struct atc_header));
				pcm_data->msg_id = pcm_indication->msg_id;
				pcm_data->callback = pcm_indication->callback;
				pcm_data->len = pcm_pktsize;
				ret = audio_tx_rawskb(pcm_skb);
				if (ret < 0)
					pr_err("PCM send error: %d\n", ret);
				wake_up_interruptible(&pcm_txwq);
				if (skb_queue_empty(&pcm_txq))
					wake_up_interruptible(&pcm_drainwq);
			}
		} else
			pr_err("pcm tx device not opened yet\n");
		break;
	default:
		pr_err("audio_data_handler,unknown:sub_cmd=%d, cmd_type=%d\n",
		       header->sub_cmd, header->cmd_type);
		break;
	}

	kfree_skb(skb);
}

static int audio_rcv_task(void *data)
{
	struct sk_buff *skb = NULL;

	pr_info("audio_rcv_task start...\n");
	allow_signal(SIGSTOP);

	/* make sure share memory is initialized before proceeding */
	while (!cp_is_synced)
		msleep_interruptible(3000);

	if (audiostubfd == -1) {
		audiostubfd = msocket(AUDIOSTUB_PORT);
		pr_debug("audiostubfd=%d\n", audiostubfd);
		if (audiostubfd < 0) {
			pr_err("failed to open audiostubfd\n");
			recv_taskref = NULL;
			return -1;
		}
	}

	if (!audiostub_inited)
		start_handshake();

	while (!kthread_should_stop()) {
		skb = mrecvskb(audiostubfd, MAX_AUDIO_RXMSLEN, MSOCKET_KERNEL);
		if (!skb)
			continue;
		audio_data_handler(skb);
	}
	pr_info("%s: quit thread\n", __func__);
	stop_handshake();
	return 0;
}

static int handshake_task(void *data)
{
	u8 startmsg[sizeof(struct atc_header) + sizeof(struct handshake_msg)];
	struct atc_header *header;
	struct handshake_msg *msg =
	    (struct handshake_msg *)(startmsg + sizeof(struct atc_header));

	header = (struct atc_header *)startmsg;
	header->cmd_code = AUDIO_CMD_CODE;
	header->cmd_type = CMD_TYPE_EXECUTE;
	header->sub_cmd = ATC_HANDSHAKE;
	header->data_len = sizeof(struct handshake_msg);

	msg->is_wb = 1;
	msg->msg_id = 0;
	msg->pcm_master = pcm_master;
	msg->reserved = 0;
	msg->ver = ATC_VER;

	pr_info("handshake_task start...\n");
	while (!kthread_should_stop()) {
		audio_tx_rawdata(startmsg, sizeof(startmsg));
		msleep_interruptible(1000);
	}

	pr_info("handshake_task quit\n");
	return 0;
}

static void start_handshake(void)
{
	if (handshake_taskref == NULL)
		handshake_taskref =
		    kthread_run(handshake_task, NULL, "audio_handshake");
}

static void stop_handshake(void)
{
	if (handshake_taskref) {
		kthread_stop(handshake_taskref);
		handshake_taskref = NULL;
	}
}

static void init_rcv_task(void)
{
	skb_queue_head_init(&ctl_rxq);
	if (recv_taskref == NULL)
		recv_taskref =
		    kthread_run(audio_rcv_task, NULL, "audio_rcv_task");

}

static void deinit_audioport(void)
{
	if (recv_taskref) {
		send_sig(SIGSTOP, recv_taskref, 1);
		kthread_stop(recv_taskref);
		recv_taskref = NULL;
	}

	if (audiostubfd > 0)
		mclose(audiostubfd);
	audiostubfd = -1;

	skb_queue_purge(&ctl_rxq);
}

module_param_named(pcm_master, pcm_master, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(pcm_master, "CP is PCM master");

module_param_named(handshake, audiostub_inited, invbool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(handshake, "Perform handshake on startup");

static int audiodev_setup_cdev(struct cdev *dev, int index)
{
	int err, devno = MKDEV(audiostub_major, audiostub_minor + index);

	cdev_init(dev, &audiostub_fops);
	dev->owner = THIS_MODULE;
	dev->ops = &audiostub_fops;
	err = cdev_add(dev, devno, 1);
	/* Fail gracefully if needed */
	if (err)
		pr_err("Error %d adding audiostub minor %d\n", err, index);

	return err;
}

static void audiodev_cleanup_module(int device_added)
{
	dev_t devno = MKDEV(audiostub_major, audiostub_minor);
	int i;

	deinit_audioport();

	/* Get rid of our char dev entries */
	if (audiodev_devices) {
		for (i = 0; i < device_added; i++) {
			cdev_del(&audiodev_devices[i]);
			device_destroy(audiodev_class,
				MKDEV(audiostub_major,
				audiostub_minor + i));
		}
		kfree(audiodev_devices);
	}

	class_destroy(audiodev_class);
	unregister_chrdev_region(devno, AUDIO_DEVICE_CNT);
}

static int __init audiostub_init(void)
{
	dev_t dev = 0;
	int result, device_added = 0;
	char name[32];

	pr_info("pcm master=%u, audiostub_inited=%u\n", pcm_master,
		audiostub_inited);
	if (audiostub_major) {
		dev = MKDEV(audiostub_major, audiostub_minor);
		result = register_chrdev_region(dev,
				AUDIO_DEVICE_CNT, "audiostub");
	} else {
		result = alloc_chrdev_region(&dev,
				audiostub_minor, AUDIO_DEVICE_CNT,
				"audiostub");
		audiostub_major = MAJOR(dev);
	}
	if (result < 0) {
		pr_err("audiostub: can't get major %d\n", audiostub_major);
		return result;
	}

	audiodev_devices =
	    kzalloc(sizeof(struct cdev) * AUDIO_DEVICE_CNT, GFP_KERNEL);
	if (!audiodev_devices) {
		result = -ENOMEM;
		goto fail;
	}

	/* Initialize each device. */
	audiodev_class = class_create(THIS_MODULE, "audiostub");
	strncpy(name, "audiostub_ctl", sizeof(name));
	device_create(audiodev_class, NULL,
		      MKDEV(audiostub_major,
			    audiostub_minor + AUDIO_CTL_OFFSET), NULL, name);

	result =
	    audiodev_setup_cdev(&audiodev_devices[AUDIO_CTL_OFFSET],
				AUDIO_CTL_OFFSET);

	if (result < 0)
		goto fail;

	device_added++;
	strncpy(name, "audiostub_pcm", sizeof(name));
	device_create(audiodev_class, NULL,
		      MKDEV(audiostub_major,
			    audiostub_minor + AUDIO_PCM_OFFSET), NULL, name);

	result = audiodev_setup_cdev(&audiodev_devices[AUDIO_PCM_OFFSET],
				AUDIO_PCM_OFFSET);
	if (result < 0)
		goto fail;

	init_rcv_task();
	return 0;

fail:
	audiodev_cleanup_module(device_added);
	return result;
}

static void __exit audiostub_exit(void)
{
	audiodev_cleanup_module(AUDIO_DEVICE_CNT);
}

module_init(audiostub_init);
module_exit(audiostub_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell audio stub.");

