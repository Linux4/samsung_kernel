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

#include "sprdwl.h"
#include "sdio_ba.h"
#include "msg.h"
#include "txrx.h"

#if defined(CONFIG_ARCH_SCX35LT8) || defined(CONFIG_ARCH_WHALE)
#include <linux/gpio.h>
#else
#include <mach/gpio.h>
#endif
#include <linux/of.h>
#include <linux/of_gpio.h>

#define SPRDWL_TX_MSG_NUM 256
#define SPRDWL_TX_DATA_STOPE_QUEU_NUM (SPRDWL_TX_MSG_NUM - 4)
#define SPRDWL_TX_DATA_START_QUEU_NUM (SPRDWL_TX_DATA_STOPE_QUEU_NUM - 4)
#define SPRDWL_RX_MSG_NUM 10
#define HW_TX_SIZE              (13 * 1024)
#define HW_RX_SIZE              (12288)
#define PKT_AGGR_NUM            (14)
#define ALIGN_4BYTE(a)		(((a) + 3) & ~3)

static struct sprdwl_sdio *sprdwl_sdev;

/* after MCC flowctl fixme, maybe move it to txrx.c*/
static void sprdwl_sdio_wake_net_ifneed(struct sprdwl_sdio *sdev)
{
	struct sprdwl_vif *vif, *tmp_vif;
	struct sprdwl_priv *priv;

	if (unlikely(sdev->exit))
		return;
	priv = sdev->priv;
	if (test_bit(SPRDWL_AP_FLOW_CTR, &priv->flags) &&
	    sprdwl_msg_ref(&sdev->tx_list) < SPRDWL_TX_DATA_START_QUEU_NUM) {
		spin_lock_bh(&priv->list_lock);
		list_for_each_entry_safe(vif, tmp_vif, &priv->vif_list,
					 vif_node)
			if (vif->ndev)
				netif_wake_queue(vif->ndev);
		clear_bit(SPRDWL_AP_FLOW_CTR, &priv->flags);
		spin_unlock_bh(&priv->list_lock);
	}
}

static int sprdwl_tx_buf_init(struct sprdwl_tx_buf *txbuf, unsigned int size)
{
	int *num;

	txbuf->base = kmalloc(size, GFP_ATOMIC);
	if (txbuf->base) {
		txbuf->buf_len = size;
		txbuf->curpos = 4;
		num = (int *)txbuf->base;
		*num = 0;
		return 0;
	}
	pr_err("%s alloc tx buf err:size:%u\n", __func__, size);
	return -ENOMEM;
}

static void sprdwl_tx_buf_deinit(struct sprdwl_tx_buf *txbuf)
{
	kfree(txbuf->base);
}

static int sprdwl_rx_buf_init(struct sprdwl_rx_buf *rxbuf, unsigned int size)
{
	rxbuf->base = kmalloc(size, GFP_ATOMIC);
	if (rxbuf->base) {
		rxbuf->buf_len = size;
		rxbuf->data_len = 0;
		return 0;
	}
	pr_err("%s alloc rx buf err:size:%u\n", __func__, size);
	return -ENOMEM;
}

static void sprdwl_rx_buf_deinit(struct sprdwl_rx_buf *rxbuf)
{
	kfree(rxbuf->base);
}

static void sprdwl_clean_txinfo(struct sprdwl_tx_buf *txbuf)
{
	unsigned int *num;

	txbuf->curpos = 4;
	num = (int *)txbuf->base;
	*num = 0;
}

static int sprdwl_packet_txinfo(struct sprdwl_msg_buf *msg_buf,
				struct sprdwl_tx_buf *txbuf)
{
	unsigned short msg_len;
	unsigned int *num;

	msg_len = ALIGN_4BYTE(msg_buf->len);
	num = (int *)txbuf->base;
	if (msg_len > txbuf->buf_len - txbuf->curpos || *num >= PKT_AGGR_NUM)
		return -ENOMEM;
	memcpy(txbuf->base + txbuf->curpos, msg_buf->skb->data, msg_buf->len);
	txbuf->curpos += msg_len;
	*num = *num + 1;

	return 0;
}

static int sprdwl_rx_process(struct sprdwl_priv *priv,
			     unsigned char *buf, unsigned int max_len)
{
	unsigned short len;
	unsigned int p = 0;
	unsigned char *pdata = NULL;

	if (!buf || (max_len < 8)) {
		dev_err(&sprdwl_sdev->pdev->dev, "[%s][ERROR]\n", __func__);
		return 0;
	}

	buf += 8;
	max_len -= 8;
	while (p < max_len) {
		pdata = buf + p;
		if (pdata[0] == 0xff)
			return 0;
		switch (SPRDWL_HEAD_GET_TYPE(pdata)) {
		case SPRDWL_TYPE_DATA:
			len = sprdwl_rx_data_process(priv, pdata);
			break;
		case SPRDWL_TYPE_CMD:
			len = sprdwl_rx_rsp_process(priv, pdata);
			break;
		case SPRDWL_TYPE_EVENT:
			len = sprdwl_rx_event_process(priv, pdata);
			break;
		default:
			dev_dbg(&sprdwl_sdev->pdev->dev, "rx unkonow type:%d\n",
				SPRDWL_HEAD_GET_TYPE(pdata));
			len = 0;
			break;
		}
		if (len) {
			p += ALIGN_4BYTE(len);
		} else {
			dev_err(&sprdwl_sdev->pdev->dev, "erro %s\n", __func__);
			return 0;
		}
	}

	return 0;
}

static int sprdwl_hw_init(struct hw_info *hw)
{
	struct device_node *np;

	memset(hw, 0, sizeof(struct hw_info));

	hw->sdio_tx_chn.num          = 3;
	hw->sdio_tx_chn.chn[0]       = 0;
	hw->sdio_tx_chn.chn[1]       = 1;
	hw->sdio_tx_chn.chn[2]       = 2;
	hw->sdio_tx_chn.bit_map      = 0x0007;
	hw->sdio_tx_chn.timeout_time = 600;
	hw->sdio_tx_chn.timeout_flag = false;

	hw->sdio_rx_chn.num = 6;
	hw->sdio_rx_chn.chn[0] = 8;
	hw->sdio_rx_chn.chn[1] = 9;
	hw->sdio_rx_chn.chn[2] = 14;
	hw->sdio_rx_chn.chn[3] = 11;
	hw->sdio_rx_chn.chn[4] = 15;
	hw->sdio_rx_chn.chn[5] = 13;
	hw->sdio_rx_chn.bit_map = 0xeb00;
	hw->sdio_rx_chn.gpio_high = false;
	hw->sdio_rx_chn.timeout_time = 3000;
	hw->sdio_rx_chn.timeout_flag = false;

	np = of_find_node_by_name(NULL, "sprd-marlin");
	if (!np) {
		dev_err(&sprdwl_sdev->pdev->dev, "sprd-marlin not found");
		return -1;
	}

	hw->rx_gpio = of_get_gpio(np, 1);

	dev_dbg(&sprdwl_sdev->pdev->dev, "[SDIO_TX_CHN][0x%x][0x%x]\n",
		hw->sdio_tx_chn.bit_map, HW_TX_SIZE);
	dev_dbg(&sprdwl_sdev->pdev->dev, "[SDIO_RX_CHN][0x%x][0x%x]\n",
		hw->sdio_rx_chn.bit_map, HW_RX_SIZE);

	spin_lock_init(&hw->sdio_rx_chn.lock);

	hw->wakeup = 0;
	return 0;
}

void sprdwl_rx_chn_isr(int chn)
{
	if (!sprdwl_sdev->exit) {
		if (!work_pending(&sprdwl_sdev->rx_work))
			queue_work(sprdwl_sdev->rx_queue,
				   &sprdwl_sdev->rx_work);
	}
}

static int check_valid_chn(int flag, unsigned short status,
			   struct sdio_chn *chn_info)
{
	int i, index = -1;

	if (flag == 1)
		status = (status & chn_info->bit_map);
	else
		status = ((status & chn_info->bit_map) ^ (chn_info->bit_map));
	if (!status)
		return -1;
	for (i = 0; i < chn_info->num; i++) {
		if (status & (0x1 << chn_info->chn[i])) {
			index = chn_info->chn[i];
			break;
		}
	}
	return index;
}

static int sprdwl_send_packet(struct sprdwl_sdio *sdev,
			      struct sprdwl_tx_buf *txbuf, int retry)
{
	unsigned short status;
	int ret, index;
	struct sdio_chn *tx_chn;

	if (txbuf->curpos == 4)
		return 0;
	tx_chn = &sdev->hw.sdio_tx_chn;
get_status:
	ret = set_marlin_wakeup(0, 1);
	if (ret) {
		if (ret == -3) {
			dev_err(&sdev->pdev->dev,
				"tx call set_marlin_wakeup return:%d\n", ret);
			usleep_range(2000, 3000);
			return -1;
		}
		if (ret != -2) {
			dev_err(&sdev->pdev->dev,
				"tx call set_marlin_wakeup return:%d\n", ret);
			msleep(50);
			return -1;
		}
	}
	ret = sdio_chn_status(tx_chn->bit_map, &status);
	if (ret) {
		if (retry) {
			retry--;
			dev_err(&sdev->pdev->dev, "ch status %d\n", retry);
			usleep_range(200, 400);
			goto get_status;
		}
		return -1;
	}
	index = check_valid_chn(0, status, tx_chn);
	if (index < 0) {
		if (retry) {
			retry--;
			dev_err(&sdev->pdev->dev, "ch valid %d\n", retry);
			usleep_range(40, 50);
			goto get_status;
		}
		return -1;
	}
resend:
	ret = sdio_dev_write(index, txbuf->base,
			     (txbuf->curpos + 1023) & (~0x3FF));
	if (ret) {
		if (retry) {
			retry--;
			dev_err(&sdev->pdev->dev, "dev write %d\n", retry);
			goto resend;
		}
		dev_err(&sdev->pdev->dev, "%s write err retry:%d\n",
			__func__, retry);
		return -1;
	}
	sprdwl_clean_txinfo(txbuf);

	return 0;
}

#define SPRDWL_SDIO_TX_TIMEOUT 1500
static void sprdwl_tx_work_queue(struct work_struct *work)
{
	int ret;
	unsigned long timeout;
	struct sprdwl_sdio *sdev;
	struct sprdwl_msg_buf *msgbuf;
	struct sprdwl_priv *priv;

	sdev = container_of(work, struct sprdwl_sdio, tx_work);
	priv = sdev->priv;

	wake_lock(&sdev->tx_wakelock);
	timeout = jiffies + msecs_to_jiffies(SPRDWL_SDIO_TX_TIMEOUT);
TRYSEND:
	while ((msgbuf = sprdwl_peek_msg_buf(&sdev->tx_list))) {
		if (unlikely(sdev->exit)) {
			dev_kfree_skb(msgbuf->skb);
			sprdwl_dequeue_msg_buf(msgbuf, &sdev->tx_list);
			continue;
		}
		ret = sprdwl_packet_txinfo(msgbuf, &sdev->txbuf);
		if (!ret) {
			dev_kfree_skb(msgbuf->skb);
			sprdwl_dequeue_msg_buf(msgbuf, &sdev->tx_list);
			sprdwl_sdio_wake_net_ifneed(sdev);
		} else {
			break;
		}
	}

#define SPRDWL_TX_RETRY_COUNT 0
	if (!(sprdwl_send_packet(sdev, &sdev->txbuf, SPRDWL_TX_RETRY_COUNT))) {
		timeout = jiffies + msecs_to_jiffies(SPRDWL_SDIO_TX_TIMEOUT);
	} else {
		if (time_after(jiffies, timeout)) {
			dev_err(&sdev->pdev->dev,
				"%s send packet erro drop msg\n", __func__);
			sprdwl_clean_txinfo(&sdev->txbuf);
			msleep(100);
		} else {
			usleep_range(80, 100);
			goto TRYSEND;
		}
	}
	/* maybe high throuput need it, or not */
	if (sprdwl_msg_tx_pended(&sdev->tx_list))
		goto TRYSEND;
	wake_unlock(&sdev->tx_wakelock);
	/*fixme if sdio err, data can't send to cp.\n*/
}

#define SPRDWL_RX_ERR_SLEEP(str, err_no)
static void sprdwl_rx_work_queue(struct work_struct *work)
{
	unsigned short status;
	unsigned int err_count;
	int ret;
	u32 read_len;
	u32 rx_gpio;
	int index = 0;
	struct sprdwl_rx_buf *rxbuf;
	struct sdio_chn *rx_chn;
	struct sprdwl_priv *priv;
	struct sprdwl_sdio *sdev;

	sdev = container_of(work, struct sprdwl_sdio, rx_work);
	priv = sdev->priv;
	rx_gpio = sdev->hw.rx_gpio;
	rx_chn = &sdev->hw.sdio_rx_chn;
	rxbuf = &sdev->rxbuf;

	err_count = 0;
	wake_lock(&sdev->rx_wakelock);
	while (gpio_get_value(rx_gpio)) {
		if (unlikely(sdev->exit)) {
			wake_unlock(&sdev->rx_wakelock);
			return;
		}
		ret = set_marlin_wakeup(0, 1);
		if (ret) {
			if (ret == -3) {
				dev_err(&sdev->pdev->dev,
					"rx call set_marlin_wakeup return:%d\n",
					ret);
				usleep_range(2000, 3000);
				continue;
			} else if (ret != -2) {
				dev_err(&sdev->pdev->dev,
					"rx call set_marlin_wakeup return:%d\n",
					ret);
				wake_unlock(&sdev->rx_wakelock);
				msleep(100);
				wake_lock(&sdev->rx_wakelock);
				continue;
			}
		}
		ret = sdio_chn_status(rx_chn->bit_map, &status);
		if (ret) {
			usleep_range(200, 300);
			continue;
		}

		index = check_valid_chn(1, status, rx_chn);
		if (index == 14) {
			mdbg_sdio_read();
			continue;
		} else if (index == 11) {
			mdbg_at_cmd_read();
			continue;
		} else if (index == 15) {
			mdbg_loopcheck_read();
			continue;
		} else if (index < 0) {
			SPRDWL_RX_ERR_SLEEP("rx chn status index error", index);
			continue;
		}

		ret = sdio_dev_read(index, rxbuf->base, &read_len);
		if (ret) {
			dev_err(&sdev->pdev->dev,
				"%s [chn %d][sdio_read] err:%d\n", __func__,
				index, ret);
			SPRDWL_RX_ERR_SLEEP("rx chn read error", ret);
			continue;
		} else if (read_len > rxbuf->buf_len) {
			dev_err(&sdev->pdev->dev,
				"%s rxlen:%d is longger than 12k, drop rxmsg here\n",
				__func__, read_len);
			BUG_ON(1);
			continue;
		}
		sprdwl_rx_process(priv, rxbuf->base, (unsigned short)read_len);
	}
	wake_unlock(&sdev->rx_wakelock);
}

static int sprdwl_sdio_txmsg(void *spdev, struct sprdwl_msg_buf *msg)
{
	struct sprdwl_sdio *sdev;
	/* here not jude sdev->exit */
	sdev = (struct sprdwl_sdio *)spdev;
	sprdwl_queue_msg_buf(msg, &sdev->tx_list);
	if (!work_pending(&sdev->tx_work))
		queue_work(sdev->tx_queue, &sdev->tx_work);
	return 0;
}

static struct
sprdwl_msg_buf *sprdwl_sdio_get_msg_buf(void *spdev,
					enum sprdwl_head_type type,
					enum sprdwl_mode mode)
{
	int len;
	struct sprdwl_sdio *sdev;
	struct sprdwl_msg_buf *msg;

	sdev = (struct sprdwl_sdio *)spdev;
	if (unlikely(sdev->exit))
		return NULL;
	if (type == SPRDWL_TYPE_DATA)
		len = SPRDWL_TX_DATA_STOPE_QUEU_NUM;
	else if (type == SPRDWL_TYPE_CMD)
		len = SPRDWL_TX_MSG_NUM;
	if (sprdwl_msg_ref(&sdev->tx_list) > len) {
		dev_err(&sdev->pdev->dev, "%s no more msgbuf for cmd tx 1\n",
			__func__);
		return NULL;
	}
	msg = sprdwl_alloc_msg_buf(&sdev->tx_list);
	if (msg) {
		msg->type = type;
		return msg;
	}

	dev_err(&sdev->pdev->dev, "%s no more msgbuf for cmd tx 2\n", __func__);
	return NULL;
}

static void sprdwl_sdio_free_msg_buf(void *spdev, struct sprdwl_msg_buf *msg)
{
	struct sprdwl_sdio *sdev = (struct sprdwl_sdio *)spdev;

	sprdwl_free_msg_buf(msg, &sdev->tx_list);
}

static void sprdwl_sdio_force_exit(void)
{
	sprdwl_sdev->exit = 1;
}

static int sprdwl_sdio_is_exit(void)
{
	return sprdwl_sdev->exit;
}

static struct sprdwl_if_ops sprdwl_sdio_ops = {
	.get_msg_buf = sprdwl_sdio_get_msg_buf,
	.free_msg_buf = sprdwl_sdio_free_msg_buf,
	.tx = sprdwl_sdio_txmsg,
	.force_exit = sprdwl_sdio_force_exit,
	.is_exit = sprdwl_sdio_is_exit,
};

static int sprdwl_sdio_init(struct sprdwl_sdio *sdev)
{
	int ret;

	ret = sprdwl_tx_buf_init(&sdev->txbuf, HW_TX_SIZE);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s tx_buf create failed", __func__);
		goto err_tx_buf;
	}
	ret = sprdwl_msg_init(SPRDWL_TX_MSG_NUM, &sdev->tx_list);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s no tx_list\n", __func__);
		goto err_txmsg_init;
	}
	sdev->tx_queue = create_singlethread_workqueue("SPRDWL_TX_QUEUE");
	if (!sdev->tx_queue) {
		dev_err(&sdev->pdev->dev, "%s SPRDWL_TX_QUEUE create failed",
			__func__);
		ret = -ENOMEM;
		goto err_create_tx_queue;
	}
	INIT_WORK(&sdev->tx_work, sprdwl_tx_work_queue);

	ret = sprdwl_rx_buf_init(&sdev->rxbuf, HW_RX_SIZE);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s tx_buf create failed", __func__);
		goto err_rx_buf;
	}
	ret = sprdwl_msg_init(SPRDWL_RX_MSG_NUM, &sdev->rx_list);
	if (ret) {
		dev_err(&sdev->pdev->dev, "%s no tx_list:%d\n", __func__, ret);
		goto err_rxmsg_init;
	}
	sdev->rx_queue = create_singlethread_workqueue("SPRDWL_RX_QUEUE");
	if (!sdev->rx_queue) {
		dev_err(&sdev->pdev->dev, "%s SPRDWL_RX_QUEUE create failed",
			__func__);
		ret = -ENOMEM;
		goto err_create_rx_queue;
	}
	INIT_WORK(&sdev->rx_work, sprdwl_rx_work_queue);

	/*temp no sleep for BA, move it to tx adn rx*/
	wake_lock_init(&sdev->tx_wakelock, WAKE_LOCK_SUSPEND,
		       "sprdwl_tx_wakelock");
	wake_lock_init(&sdev->rx_wakelock, WAKE_LOCK_SUSPEND,
		       "sprdwl_rx_wakelock");

	sprdwl_hw_init(&sdev->hw);
	invalid_recv_flush(8);
	invalid_recv_flush(9);
	/* flush marlin log */
	invalid_recv_flush(14);
	sdiodev_readchn_init(8, (void *)sprdwl_rx_chn_isr, 1);
	sdiodev_readchn_init(9, (void *)sprdwl_rx_chn_isr, 1);

	return 0;

err_create_rx_queue:
	sprdwl_msg_deinit(&sdev->rx_list);
err_rxmsg_init:
	sprdwl_rx_buf_deinit(&sdev->rxbuf);
err_rx_buf:
	destroy_workqueue(sdev->tx_queue);
err_create_tx_queue:
	sprdwl_msg_deinit(&sdev->tx_list);
err_txmsg_init:
	sprdwl_tx_buf_deinit(&sdev->txbuf);
err_tx_buf:
	return ret;
}

static void sprdwl_sdio_deinit(struct sprdwl_sdio *sdev)
{
	sdev->exit = 1;
	sdiodev_readchn_uninit(8);
	sdiodev_readchn_uninit(9);

	flush_workqueue(sdev->rx_queue);
	destroy_workqueue(sdev->rx_queue);

	flush_workqueue(sdev->tx_queue);
	destroy_workqueue(sdev->tx_queue);

	wake_lock_destroy(&sdev->tx_wakelock);
	wake_lock_destroy(&sdev->rx_wakelock);

	sprdwl_msg_deinit(&sdev->rx_list);
	sprdwl_msg_deinit(&sdev->tx_list);

	sprdwl_tx_buf_deinit(&sdev->txbuf);
	sprdwl_rx_buf_deinit(&sdev->rxbuf);
}

static int sprdwl_probe(struct platform_device *pdev)
{
	struct sprdwl_sdio *sdev;
	struct sprdwl_priv *priv;
	int ret;

	ret = get_sdiohal_status();
	if (ret != 1) {
		dev_err(&pdev->dev, "%s SDIO not ready!\n", __func__);
		return -EPERM;
	}

	sdev = kzalloc(sizeof(*sdev), GFP_ATOMIC);
	if (!sdev)
		return -ENOMEM;
	platform_set_drvdata(pdev, sdev);
	sdev->pdev = pdev;
	sprdwl_sdev = sdev;

	ret = sprdwl_sdio_init(sdev);
	if (ret)
		goto err_sdio_init;

	priv = sprdwl_core_create(SPRDWL_HW_SDIO_BA, &sprdwl_sdio_ops);
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
	struct sprdwl_sdio *sdev = platform_get_drvdata(pdev);
	struct sprdwl_priv *priv = sdev->priv;

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
	pr_info("%s\n", __func__);
}

module_init(sprdwl_init);
module_exit(sprdwl_exit);

MODULE_DESCRIPTION("Spreadtrum Wireless LAN Driver");
MODULE_AUTHOR("Spreadtrum WCN Division");
MODULE_LICENSE("GPL");
