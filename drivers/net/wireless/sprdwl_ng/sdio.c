/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
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

#include <linux/platform_device.h>
#include <linux/marlin_platform.h>

#include <linux/sdiom_rx_api.h>
#include <linux/sdiom_tx_api.h>
#include <linux/debugfs.h>

#include "sprdwl.h"
#include "sdio.h"
#include "msg.h"
#include "txrx.h"

#define SPRDWL_MSG_TIMEOUT	1500

#define SPRDWL_SDIOM_WIFI 2
enum sprdwl_sdiom_port {
	/* cp port 8 */
	SPRDWL_SDIOM_CMD_TX,
	/* cp port 8 */
	SPRDWL_SDIOM_CMD_RX = 0,
	/* cp port 9 */
	SPRDWL_SDIOM_DATA_TX = 1,
	/* cp port 9 */
	SPRDWL_SDIOM_DATA_RX = 1,
};

#define SPRDWL_TX_MSG_CMD_NUM 2
#define SPRDWL_TX_MSG_DATA_NUM 128
#define SPRDWL_TX_DATA_START_NUM (SPRDWL_TX_MSG_DATA_NUM - 4)
#define SPRDWL_RX_MSG_NUM 128

/* tx len less than cp len 4 byte as sdiom 4 bytes align */
#define SPRDWL_MAX_CMD_TXLEN	1396
#define SPRDWL_MAX_CMD_RXLEN	1088
#define SPRDWL_MAX_DATA_TXLEN	1596
#define SPRDWL_MAX_DATA_RXLEN	1592

#define SPRDWL_SDIO_MASK_LIST0	0x1
#define SPRDWL_SDIO_MASK_LIST1	0x2
#define SPRDWL_SDIO_MASK_LIST2	0x4

static struct sprdwl_sdio *sprdwl_sdev;

static void sprdwl_sdio_wakeup_tx(void);

static void sprdwl_sdio_wake_net_ifneed(struct sprdwl_sdio *sdev,
					struct sprdwl_msg_list *list,
					enum sprdwl_mode mode)
{
	if (atomic_read(&list->flow)) {
		if (atomic_read(&list->ref) <= SPRDWL_TX_DATA_START_NUM) {
			sdev->net_start_cnt++;
			sprdwl_net_flowcontrl(sdev->priv, mode, true);
			atomic_set(&list->flow, 0);
		}
	}
}

static int sprdwl_sdio_txmsg(void *spdev, struct sprdwl_msg_buf *msg)
{
	u16 len;
	int needwake = 1;
	unsigned char *info;
	struct sprdwl_sdio *sdev = (struct sprdwl_sdio *)spdev;

	if (msg->msglist == &sprdwl_sdev->tx_list0) {
		len = SPRDWL_MAX_CMD_TXLEN;
		info = "cmd";
	} else if (msg->msglist == &sprdwl_sdev->tx_list1) {
		len = SPRDWL_MAX_DATA_TXLEN;
		info = "data";
		if (sprdwl_sdev->tx_hang & SPRDWL_SDIO_MASK_LIST1)
			needwake = 0;
	} else {
		len = SPRDWL_MAX_DATA_TXLEN;
		info = "data";
		if (sprdwl_sdev->tx_hang & SPRDWL_SDIO_MASK_LIST2)
			needwake = 0;
	}
	if (msg->len > len) {
		dev_err(&sdev->pdev->dev,
			"%s err:%s too long:%d > %d,drop it\n",
			__func__, info, msg->len, len);
		sprdwl_free_msg_buf(msg, msg->msglist);
		return 0;
	}

	msg->timeout = jiffies + sdev->msg_timeout;
	sprdwl_queue_msg_buf(msg, msg->msglist);
	if (needwake)
		sprdwl_sdio_wakeup_tx();
	if (!work_pending(&sdev->tx_work))
		queue_work(sdev->tx_queue, &sdev->tx_work);

	return 0;
}

static struct
sprdwl_msg_buf *sprdwl_sdio_get_msg_buf(void *spdev,
					enum sprdwl_head_type type,
					enum sprdwl_mode mode)
{
	u8 sdiom_type;
	struct sprdwl_sdio *sdev;
	struct sprdwl_msg_buf *msg;
	struct sprdwl_msg_list *list;

	sdev = (struct sprdwl_sdio *)spdev;
	if (unlikely(sdev->exit))
		return NULL;
	if (type == SPRDWL_TYPE_DATA) {
		sdiom_type = SPRDWL_SDIOM_DATA_TX;
		if (mode <= SPRDWL_MODE_AP)
			list = &sdev->tx_list1;
		else
			list = &sdev->tx_list2;
	} else {
		sdiom_type = SPRDWL_SDIOM_CMD_TX;
		list = &sdev->tx_list0;
	}
	msg = sprdwl_alloc_msg_buf(list);
	if (msg) {
		msg->type = sdiom_type;
		msg->msglist = list;
		msg->mode = mode;
		return msg;
	}

	if (type == SPRDWL_TYPE_DATA) {
		sdev->net_stop_cnt++;
		sprdwl_net_flowcontrl(sdev->priv, mode, false);
	}
	atomic_set(&list->flow, 1);
	dev_err(&sdev->pdev->dev, "%s no more msgbuf for %s\n",
		__func__, type == SPRDWL_TYPE_DATA ? "data" : "cmd");

	return NULL;
}

static void sprdwl_sdio_free_msg_buf(void *spdev, struct sprdwl_msg_buf *msg)
{
	sprdwl_free_msg_buf(msg, msg->msglist);
}

static void sprdwl_sdio_force_exit(void)
{
	sprdwl_sdev->exit = 1;
	wake_up_all(&sprdwl_sdev->waitq);
}

static int sprdwl_sdio_is_exit(void)
{
	return sprdwl_sdev->exit;
}

#define SPRDWL_SDIO_DEBUG_BUFLEN 128
static ssize_t sprdwl_sdio_read_info(struct file *file,
				     char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	unsigned int buflen, len;
	size_t ret;
	unsigned char *buf;
	struct sprdwl_sdio *sdev;

	sdev = (struct sprdwl_sdio *)file->private_data;
	buflen = SPRDWL_SDIO_DEBUG_BUFLEN;
	buf = kzalloc(buflen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len = 0;
	len += scnprintf(buf, buflen, "net: stop %u, start %u\n"
			 "drop cnt: cmd %u, sta %u, p2p %u\n"
			 "ring_ap:%u ring_cp:%u common:%u sta:%u p2p:%u\n",
			 sdev->net_stop_cnt, sdev->net_start_cnt,
			 sdev->drop_cmd_cnt, sdev->drop_data1_cnt,
			 sdev->drop_data2_cnt,
			 sdev->ring_ap, sdev->ring_cp,
			 atomic_read(&sdev->flow0),
			 atomic_read(&sdev->flow1),
			 atomic_read(&sdev->flow2));
	if (len > buflen)
		len = buflen;

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static ssize_t sprdwl_sdio_write(struct file *file,
				 const char __user *__user_buf,
				 size_t count, loff_t *ppos)
{
	char buf[20];

	if (!count || count >= sizeof(buf)) {
		dev_err(&sprdwl_sdev->pdev->dev,
			"write len too long:%d >= %d\n", count, sizeof(buf));
		return -EINVAL;
	}
	if (copy_from_user(buf, __user_buf, count))
		return -EFAULT;
	buf[count] = '\0';
	dev_err(&sprdwl_sdev->pdev->dev, "write info:%s\n", buf);

	return count;
}

static const struct file_operations sprdwl_sdio_debug_fops = {
	.read = sprdwl_sdio_read_info,
	.write = sprdwl_sdio_write,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static void sprdwl_sdio_debugfs(struct dentry *dir)
{
	debugfs_create_file("sdioinfo", S_IRUSR,
			    dir, sprdwl_sdev, &sprdwl_sdio_debug_fops);
}

static struct sprdwl_if_ops sprdwl_sdio_ops = {
	.get_msg_buf = sprdwl_sdio_get_msg_buf,
	.free_msg_buf = sprdwl_sdio_free_msg_buf,
	.tx = sprdwl_sdio_txmsg,
	.force_exit = sprdwl_sdio_force_exit,
	.is_exit = sprdwl_sdio_is_exit,
	.debugfs = sprdwl_sdio_debugfs,
};

static void sprdwl_rx_work_queue(struct work_struct *work)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_priv *priv;
	struct sprdwl_sdio *sdev;

	sdev = container_of(work, struct sprdwl_sdio, rx_work);
	priv = sdev->priv;

	while ((msg = sprdwl_peek_msg_buf(&sdev->rx_list))) {
		if (sdev->exit)
			goto next;
		switch (SPRDWL_HEAD_GET_TYPE(msg->tran_data)) {
		case SPRDWL_TYPE_DATA:
			if (msg->len > SPRDWL_MAX_DATA_RXLEN)
				dev_err(&sdev->pdev->dev,
					"err rx data too long:%d > %d\n",
					msg->len, SPRDWL_MAX_DATA_RXLEN);
			sprdwl_rx_data_process(priv, msg->tran_data);
			break;
		case SPRDWL_TYPE_CMD:
			if (msg->len > SPRDWL_MAX_CMD_RXLEN)
				dev_err(&sdev->pdev->dev,
					"err rx cmd too long:%d > %d\n",
					msg->len, SPRDWL_MAX_CMD_RXLEN);
			sprdwl_rx_rsp_process(priv, msg->tran_data);
			break;
		case SPRDWL_TYPE_EVENT:
			if (msg->len > SPRDWL_MAX_CMD_RXLEN)
				dev_err(&sdev->pdev->dev,
					"err rx event too long:%d > %d\n",
					msg->len, SPRDWL_MAX_CMD_RXLEN);
			sprdwl_rx_event_process(priv, msg->tran_data);
			break;
		default:
			dev_err(&sdev->pdev->dev, "rx unkonow type:%d\n",
				SPRDWL_HEAD_GET_TYPE(msg->tran_data));
			break;
		}
next:
		sdiom_pt_read_release(msg->fifo_id);
		sprdwl_dequeue_msg_buf(msg, &sdev->rx_list);
	};
}

static int sprdwl_tx_cmd(struct sprdwl_sdio *sdev, struct sprdwl_msg_list *list)
{
	int ret;
	struct sprdwl_msg_buf *msgbuf;
	int cnt = 0;

	while ((msgbuf = sprdwl_peek_msg_buf(list))) {
		if (unlikely(sdev->exit)) {
			dev_kfree_skb(msgbuf->skb);
			sprdwl_dequeue_msg_buf(msgbuf, list);
			continue;
		}
		if (time_after(jiffies, msgbuf->timeout)) {
			sdev->drop_cmd_cnt++;
			dev_err(&sdev->pdev->dev,
				"tx drop cmd msg,dropcnt:%u\n",
				sdev->drop_cmd_cnt);
			sprdwl_dequeue_msg_buf(msgbuf, list);
			continue;
		}
		ret = sdiom_pt_write_skb(msgbuf->tran_data, msgbuf->skb,
					 msgbuf->len, SPRDWL_SDIOM_WIFI,
					 msgbuf->type);
		if (!ret) {
			cnt++;
			sprdwl_dequeue_msg_buf(msgbuf, list);
		} else {
			dev_err(&sdev->pdev->dev, "%s err:%d\n", __func__, ret);
			/* fixme if need retry */
			dev_kfree_skb(msgbuf->skb);
			sprdwl_dequeue_msg_buf(msgbuf, list);
		}
	}

	return 0;
}

static int sprdwl_tx_data(struct sprdwl_sdio *sdev,
			  struct sprdwl_msg_list *list, atomic_t * flow)
{
	int ret;
	/* sendnum0 shared */
	int sendnum0, sendnum1;
	int sendcnt0 = 0;
	int sendcnt1 = 0;
	int *psend;
	unsigned int cnt;
	struct sprdwl_msg_buf *msgbuf;
	struct sprdwl_data_hdr *hdr;

	/* cp: self free link */
	sendnum0 = atomic_read(flow);
	/* ap: shared free link */
	sendnum1 = atomic_read(&sdev->flow0);
	if (!sendnum0 && !sendnum1)
		return 0;
	while ((msgbuf = sprdwl_peek_msg_buf(list))) {
		if (unlikely(sdev->exit)) {
			dev_kfree_skb(msgbuf->skb);
			sprdwl_dequeue_msg_buf(msgbuf, list);
			continue;
		}
		if (time_after(jiffies, msgbuf->timeout)) {
			if (list == &sdev->tx_list1)
				cnt = sdev->drop_data1_cnt++;
			else
				cnt = sdev->drop_data2_cnt++;
			dev_err(&sdev->pdev->dev,
				"tx drop data msg,dropcnt:%u\n", cnt);
			sprdwl_dequeue_msg_buf(msgbuf, list);
			sprdwl_sdio_wake_net_ifneed(sdev, list, msgbuf->mode);
			continue;
		}
		if (sendnum0) {
			sendnum0--;
			psend = &sendcnt0;
			hdr = (struct sprdwl_data_hdr *)msgbuf->tran_data;
			hdr->flow3 = 1;
		} else if (sendnum1) {
			sendnum1--;
			psend = &sendcnt1;
			hdr = (struct sprdwl_data_hdr *)msgbuf->tran_data;
			hdr->flow3 = 0;
		} else {
			break;
		}

		ret = sdiom_pt_write_skb(msgbuf->tran_data, msgbuf->skb,
					 msgbuf->len, SPRDWL_SDIOM_WIFI,
					 msgbuf->type);
		if (!ret) {
			*psend += 1;
			sprdwl_dequeue_msg_buf(msgbuf, list);
			sprdwl_sdio_wake_net_ifneed(sdev, list, msgbuf->mode);
		} else {
			printk_ratelimited("%s pt_write_skb err:%d\n",
					   __func__, ret);
			usleep_range(800, 1000);
			break;
		}
	}

	sdev->ring_ap += sendcnt0 + sendcnt1;
	if (sendcnt1)
		atomic_sub(sendcnt1, &sdev->flow0);
	if (sendcnt0)
		atomic_sub(sendcnt0, flow);

	return sendcnt0 + sendcnt1;
}

static void sprdwl_sdio_flush_txlist(struct sprdwl_msg_list *list)
{
	struct sprdwl_msg_buf *msgbuf;

	while ((msgbuf = sprdwl_peek_msg_buf(list))) {
		dev_kfree_skb(msgbuf->skb);
		sprdwl_dequeue_msg_buf(msgbuf, list);
		continue;
	}
}

static void sprdwl_sdio_flush_all_txlist(struct sprdwl_sdio *sdev)
{
	sprdwl_sdio_flush_txlist(&sdev->tx_list0);
	sprdwl_sdio_flush_txlist(&sdev->tx_list1);
	sprdwl_sdio_flush_txlist(&sdev->tx_list2);
}

static void sprdwl_tx_work_queue(struct work_struct *work)
{
	unsigned int send_list, needsleep;
	struct sprdwl_sdio *sdev;
	int send_num0, send_num1, send_num2;

	send_num0 = 0;
	send_num1 = 0;
	send_num2 = 0;
	sdev = container_of(work, struct sprdwl_sdio, tx_work);
RETRY:
	if (unlikely(sdev->exit)) {
		sprdwl_sdio_flush_all_txlist(sdev);
		return;
	}
	send_list = 0;
	needsleep = 0;
	sdev->do_tx = 0;
	if (sprdwl_msg_tx_pended(&sdev->tx_list0))
		send_list |= SPRDWL_SDIO_MASK_LIST0;

	if (sprdwl_msg_tx_pended(&sdev->tx_list1)) {
		send_num0 = atomic_read(&sdev->flow0);
		send_num1 = atomic_read(&sdev->flow1);
		if (send_num1 || send_num0)
			send_list |= SPRDWL_SDIO_MASK_LIST1;
		else
			needsleep |= SPRDWL_SDIO_MASK_LIST1;
	}
	if (sprdwl_msg_tx_pended(&sdev->tx_list2)) {
		if (!send_num0)
			send_num0 = atomic_read(&sdev->flow0);
		send_num2 = atomic_read(&sdev->flow2);
		if (send_num2 || send_num0)
			send_list |= SPRDWL_SDIO_MASK_LIST2;
		else
			needsleep |= SPRDWL_SDIO_MASK_LIST2;
	}

	if (!send_list) {
		if (!needsleep)
			return;
		printk_ratelimited("%s need sleep  -- 0x%x %d %d %d\n",
				   __func__, needsleep, send_num0,
				   send_num1, send_num2);
		spin_lock_bh(&sdev->lock);
		if (!sdev->do_tx) {
			sdev->tx_hang = needsleep;
			spin_unlock_bh(&sdev->lock);
		} else {
			spin_unlock_bh(&sdev->lock);
			goto RETRY;
		}
		/* fixme */
		wait_event(sdev->waitq,
			   ((!sdev->tx_hang || sdev->do_tx || sdev->exit)));
		goto RETRY;
	}

	if (send_list & SPRDWL_SDIO_MASK_LIST0)
		sprdwl_tx_cmd(sdev, &sdev->tx_list0);
	if (send_list & SPRDWL_SDIO_MASK_LIST2)
		sprdwl_tx_data(sdev, &sdev->tx_list2, &sdev->flow2);
	if (send_list & SPRDWL_SDIO_MASK_LIST1)
		sprdwl_tx_data(sdev, &sdev->tx_list1, &sdev->flow1);

	goto RETRY;
}

static void sprdwl_sdio_wakeup_tx(void)
{
	spin_lock_bh(&sprdwl_sdev->lock);
	if (sprdwl_sdev->tx_hang) {
		sprdwl_sdev->tx_hang = 0;
		wake_up(&sprdwl_sdev->waitq);
	}
	sprdwl_sdev->do_tx = 1;
	spin_unlock_bh(&sprdwl_sdev->lock);
}

static int sprdwl_sdio_process_credit(void *data)
{
	int ret = 0;
	unsigned char *flow;
	struct sprdwl_common_hdr *common;

	common = (struct sprdwl_common_hdr *)data;
	if (common->rsp && common->type == SPRDWL_TYPE_DATA) {
		flow = &((struct sprdwl_data_hdr *)data)->flow0;
		goto out;
	} else if (common->type == SPRDWL_TYPE_EVENT) {
		struct sprdwl_cmd_hdr *cmd;

		cmd = (struct sprdwl_cmd_hdr *)data;
		if (cmd->cmd_id == WIFI_EVENT_SDIO_FLOWCON) {
			flow = cmd->paydata;
			ret = -1;
			goto out;
		}
	}
	return 0;

out:
	if (flow[0])
		atomic_add(flow[0], &sprdwl_sdev->flow0);
	if (flow[1])
		atomic_add(flow[1], &sprdwl_sdev->flow1);
	if (flow[2])
		atomic_add(flow[2], &sprdwl_sdev->flow2);
	if (flow[0] || flow[1] || flow[2]) {
		sprdwl_sdev->ring_cp += flow[0] + flow[1] + flow[2];
		sprdwl_sdio_wakeup_tx();
	}

	return ret;
}

unsigned int sprdwl_sdio_rx_handle(void *data, unsigned int len,
				   unsigned int fifo_id)
{
	struct sprdwl_msg_buf *msg;

	if (!data || !len) {
		dev_err(&sprdwl_sdev->pdev->dev,
			"%s param erro:%p %d\n", __func__, data, len);
		goto out;
	}

	if (unlikely(sprdwl_sdev->exit))
		goto out;

	if (sprdwl_sdio_process_credit(data))
		goto out;

	msg = sprdwl_alloc_msg_buf(&sprdwl_sdev->rx_list);
	if (!msg) {
		pr_err("%s no msgbuf\n", __func__);
		goto out;
	}
	sprdwl_fill_msg(msg, NULL, data, len);
	msg->fifo_id = fifo_id;
	sprdwl_queue_msg_buf(msg, &sprdwl_sdev->rx_list);
	if (!work_pending(&sprdwl_sdev->rx_work))
		queue_work(sprdwl_sdev->rx_queue, &sprdwl_sdev->rx_work);

	return 0;
out:
	sdiom_pt_read_release(fifo_id);
	return 0;
}

static int sprdwl_sdio_init(struct sprdwl_sdio *sdev)
{
	int ret;
	/*fixme sdio status*/

	spin_lock_init(&sdev->lock);
	init_waitqueue_head(&sdev->waitq);
	sdev->msg_timeout = msecs_to_jiffies(SPRDWL_MSG_TIMEOUT);
	atomic_set(&sdev->flow0, 0);
	atomic_set(&sdev->flow1, 0);
	atomic_set(&sdev->flow2, 0);
	ret = sprdwl_msg_init(SPRDWL_RX_MSG_NUM, &sdev->tx_list0);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s no tx_list0\n", __func__);
		return ret;
	}
	/* fixme return */
	ret = sprdwl_msg_init(SPRDWL_TX_MSG_DATA_NUM, &sdev->tx_list1);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s no tx_list1\n", __func__);
		goto err_tx_list1;
	}

	ret = sprdwl_msg_init(SPRDWL_TX_MSG_DATA_NUM, &sdev->tx_list2);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s no tx_list2\n", __func__);
		goto err_tx_list2;
	}

	ret = sprdwl_msg_init(SPRDWL_RX_MSG_NUM, &sdev->rx_list);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s no rx_list:%d\n", __func__, ret);
		goto err_rx_list;
	}

	sdev->tx_queue = create_singlethread_workqueue("SPRDWL_TX_QUEUE");
	if (!sdev->tx_queue) {
		dev_err(&sdev->pdev->dev,
			"%s SPRDWL_TX_QUEUE create failed", __func__);
		ret = -ENOMEM;
		/* temp debug */
		goto err_tx_work;
	}
	INIT_WORK(&sdev->tx_work, sprdwl_tx_work_queue);

	/* rx init */
	sdev->rx_queue = create_singlethread_workqueue("SPRDWL_RX_QUEUE");
	if (!sdev->rx_queue) {
		dev_err(&sdev->pdev->dev, "%s SPRDWL_RX_QUEUE create failed",
			__func__);
		ret = -ENOMEM;
		goto err_rx_work;
	}
	INIT_WORK(&sdev->rx_work, sprdwl_rx_work_queue);

	sdiom_register_pt_tx_release(SPRDWL_SDIOM_WIFI,
				     SPRDWL_SDIOM_CMD_TX,
				     consume_skb);
	sdiom_register_pt_tx_release(SPRDWL_SDIOM_WIFI,
				     SPRDWL_SDIOM_DATA_TX,
				     consume_skb);

	sdiom_register_pt_rx_process(SPRDWL_SDIOM_WIFI,
				     SPRDWL_SDIOM_CMD_RX,
				     sprdwl_sdio_rx_handle);
	sdiom_register_pt_rx_process(SPRDWL_SDIOM_WIFI,
				     SPRDWL_SDIOM_DATA_RX,
				     sprdwl_sdio_rx_handle);
	return 0;

err_rx_work:
	destroy_workqueue(sdev->tx_queue);
err_tx_work:
	sprdwl_msg_deinit(&sdev->rx_list);
err_rx_list:
	sprdwl_msg_deinit(&sdev->tx_list2);
err_tx_list2:
	sprdwl_msg_deinit(&sdev->tx_list1);
err_tx_list1:
	sprdwl_msg_deinit(&sdev->tx_list0);
	return ret;
}

static void sprdwl_sdio_deinit(struct sprdwl_sdio *sdev)
{
	sdev->exit = 1;

	sdiom_register_pt_rx_process(SPRDWL_SDIOM_WIFI,
				     SPRDWL_SDIOM_CMD_RX, NULL);
	sdiom_register_pt_rx_process(SPRDWL_SDIOM_WIFI,
				     SPRDWL_SDIOM_DATA_RX, NULL);

	wake_up_all(&sdev->waitq);
	flush_workqueue(sdev->tx_queue);
	destroy_workqueue(sdev->tx_queue);
	flush_workqueue(sdev->rx_queue);
	destroy_workqueue(sdev->rx_queue);

	sprdwl_msg_deinit(&sdev->tx_list0);
	sprdwl_msg_deinit(&sdev->tx_list1);
	sprdwl_msg_deinit(&sdev->tx_list2);
	sprdwl_msg_deinit(&sdev->rx_list);

	pr_info("%s\t"
		"net: stop %u, start %u\t"
		"drop cnt: cmd %u, sta %u, p2p %u\t"
		"ring_ap:%u ring_cp:%u common:%u sta:%u p2p:%u\n",
		__func__,
		sdev->net_stop_cnt, sdev->net_start_cnt,
		sdev->drop_cmd_cnt, sdev->drop_data1_cnt,
		sdev->drop_data2_cnt,
		sdev->ring_ap, sdev->ring_cp,
		atomic_read(&sdev->flow0),
		atomic_read(&sdev->flow1),
		atomic_read(&sdev->flow2));
}

static int sprdwl_probe(struct platform_device *pdev)
{
	struct sprdwl_sdio *sdev;
	struct sprdwl_priv *priv;
	int ret;

	sdev = kzalloc(sizeof(*sdev), GFP_ATOMIC);
	if (!sdev)
		return -ENOMEM;
	platform_set_drvdata(pdev, sdev);
	sdev->pdev = pdev;
	sprdwl_sdev = sdev;

	ret = sprdwl_sdio_init(sdev);
	if (ret)
		goto err_sdio_init;

	priv = sprdwl_core_create(SPRDWL_HW_SDIO, &sprdwl_sdio_ops);
	if (!priv) {
		ret = -ENXIO;
		goto err_core_create;
	}
	priv->hw_priv = sdev;
	sdev->priv = priv;

	ret = sprdwl_core_init(&pdev->dev, priv);
	if (ret)
		goto err_core_init;

	return 0;

err_core_init:
	sprdwl_core_free((struct sprdwl_priv *)sdev->priv);
err_core_create:
	sprdwl_sdio_deinit(sdev);
err_sdio_init:
	kfree(sdev);

	return ret;
}

static int sprdwl_remove(struct platform_device *pdev)
{
	struct sprdwl_sdio *sdev;
	struct sprdwl_priv *priv;

	sdev = platform_get_drvdata(pdev);
	priv = sdev->priv;

	sprdwl_core_deinit(priv);
	sprdwl_sdio_deinit(sdev);
	sprdwl_core_free(priv);
	kfree(sdev);

	return 0;
}

#define SPRDWL_DEV_NAME "sprd_wlan"
static struct platform_device *sprdwl_device;

static struct platform_driver sprdwl_driver = {
	.probe = sprdwl_probe,
	.remove = sprdwl_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = SPRDWL_DEV_NAME,
		   },
};

static int __init sprdwl_init(void)
{
	int ret;

	pr_info("Spreadtrum WLAN Driver (Ver. %s, %s %s)\n",
		SPRDWL_DRIVER_VERSION, __DATE__, __TIME__);

	pr_info("%s power on the chip ...\n", __func__);
	if (start_marlin(MARLIN_WIFI)) {
		pr_err("%s failed to power on chip!\n", __func__);
		return -ENODEV;
	}

	sprdwl_device =
	    platform_device_register_simple(SPRDWL_DEV_NAME, 0, NULL, 0);
	if (IS_ERR(sprdwl_device)) {
		pr_err("%s register device erro\n", __func__);
		return PTR_ERR(sprdwl_device);
	}
	ret = platform_driver_register(&sprdwl_driver);
	if (ret) {
		pr_err("%s register driver erro\n", __func__);
		platform_device_unregister(sprdwl_device);
	}

	return ret;
}

static void __exit sprdwl_exit(void)
{
	platform_driver_unregister(&sprdwl_driver);
	platform_device_unregister(sprdwl_device);
	stop_marlin(MARLIN_WIFI);
	pr_info("%s\n", __func__);
}

module_init(sprdwl_init);
module_exit(sprdwl_exit);

MODULE_DESCRIPTION("Spreadtrum Wireless LAN Driver");
MODULE_AUTHOR("Spreadtrum WCN Division");
MODULE_LICENSE("GPL");
