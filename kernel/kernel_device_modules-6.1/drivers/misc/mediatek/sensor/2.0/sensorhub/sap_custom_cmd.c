// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt) "sap_cust_cmd " fmt

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "sap_custom_cmd.h"
#include "tiny_crc8.h"
#include "ready.h"
#include "sap.h"

#define ipi_len(x) (((x) + MBOX_SLOT_SIZE - 1) / MBOX_SLOT_SIZE)

struct custom_cmd_notify {
	uint8_t sequence;
	uint8_t sensor_type;
	uint8_t length;
	uint8_t crc8;
	struct custom_cmd cmd;
};

static bool scp_status;
static DEFINE_MUTEX(bus_user_lock);
static atomic_t cust_cmd_sequence;
static DECLARE_COMPLETION(cust_cmd_done);
static DEFINE_SPINLOCK(rx_notify_lock);
static struct custom_cmd_notify rx_notify;
static uint8_t notify_payload[PIN_IN_SIZE_SENSOR_SAP_NOTIFY * MBOX_SLOT_SIZE];

static int sap_custom_cmd_notify_handler(unsigned int id, void *prdata,
		void *data, unsigned int len)
{
	uint8_t crc = 0;

	WARN_ON(id != IPI_IN_SENSOR_SAP_NOTIFY);
	memcpy(&rx_notify, data, sizeof(rx_notify));
	crc = tiny_crc8((uint8_t *)&rx_notify,
		offsetof(typeof(rx_notify), crc8));
	if (rx_notify.crc8 != crc) {
		pr_err("unrecognized packet %u %u %u %u %u\n",
			rx_notify.sequence, rx_notify.sensor_type,
			rx_notify.length, rx_notify.crc8, crc);
		return -EINVAL;
	}

	complete(&cust_cmd_done);
	return 0;
}

static int sap_custom_cmd_seq(int sensor_type, struct custom_cmd *cust_cmd)
{
	int ret = 0;
	uint8_t tx_len = 0;
	int timeout = 0;
	unsigned long flags = 0;
	struct custom_cmd_notify notify;
	struct custom_cmd *rx_cmd = NULL;

	/*
	 * NOTE: must reinit_completion before sensor_comm_notify
	 * wrong sequence:
	 * sensor_comm_notify ---> reinit_completion -> wait_for_completion
	 *		       |
	 *		    complete
	 * if complete before reinit_completion, will lose this complete
	 * right sequence:
	 * reinit_completion -> sensor_comm_notify -> wait_for_completion
	 */
	reinit_completion(&cust_cmd_done);

	/* safe sequence given by atomic, round from 0 to 255 */
	notify.sequence = atomic_inc_return(&cust_cmd_sequence);
	notify.sensor_type = sensor_type;
	tx_len = offsetof(typeof(*cust_cmd), data) + cust_cmd->tx_len;
	notify.length = offsetof(typeof(notify), cmd) + tx_len;
	notify.crc8 = tiny_crc8((uint8_t *)&notify,
		offsetof(typeof(notify), crc8));
	memcpy(&notify.cmd, cust_cmd, tx_len);
	ret = mtk_ipi_send(&sap_ipidev, IPI_OUT_SENSOR_SAP_NOTIFY,
		0, (void *)&notify, ipi_len(notify.length), 10);
	if (ret < 0 || !cust_cmd->rx_len)
		return ret;

	timeout = wait_for_completion_timeout(&cust_cmd_done,
		msecs_to_jiffies(100));
	if (!timeout)
		return -ETIMEDOUT;

	spin_lock_irqsave(&rx_notify_lock, flags);
	if (rx_notify.sequence != notify.sequence &&
	    rx_notify.sensor_type != notify.sensor_type) {
		spin_unlock_irqrestore(&rx_notify_lock, flags);
		return -EREMOTEIO;
	}

	rx_cmd = (struct custom_cmd *)&rx_notify.cmd;
	if (rx_cmd->command != cust_cmd->command ||
		rx_cmd->rx_len > sizeof(cust_cmd->data) ||
		rx_cmd->rx_len > cust_cmd->rx_len) {
		spin_unlock_irqrestore(&rx_notify_lock, flags);
		return -EIO;
	}

	cust_cmd->rx_len = rx_cmd->rx_len;
	memcpy(cust_cmd->data, rx_cmd->data, cust_cmd->rx_len);
	spin_unlock_irqrestore(&rx_notify_lock, flags);
	return 0;
}

int sap_custom_cmd_comm(int sensor_type, struct custom_cmd *cust_cmd)
{
	int retry = 0, ret = 0;
	const int max_retry = 3;

	if (!READ_ONCE(scp_status)) {
		pr_err_ratelimited("dropped comm %u %u\n",
			sensor_type, cust_cmd->command);
		return 0;
	}

	mutex_lock(&bus_user_lock);
	do {
		ret = sap_custom_cmd_seq(sensor_type, cust_cmd);
	} while (retry++ < max_retry && ret < 0);
	mutex_unlock(&bus_user_lock);

	if (ret < 0)
		pr_err("%u %u fast comm fail %d\n",
			sensor_type, cust_cmd->command, ret);

	return ret;
}

static int sap_custom_cmd_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	WRITE_ONCE(scp_status, !!event);
	return NOTIFY_DONE;
}

static struct notifier_block sap_custom_cmd_ready_notifier = {
	.notifier_call = sap_custom_cmd_ready_notifier_call,
	.priority = READY_HIGHESTPRI,
};

int sap_custom_cmd_init(void)
{
	unsigned long flags = 0;

	if (!sap_enabled())
		return 0;

	atomic_set(&cust_cmd_sequence, 0);
	spin_lock_irqsave(&rx_notify_lock, flags);
	memset(&rx_notify, 0, sizeof(rx_notify));
	spin_unlock_irqrestore(&rx_notify_lock, flags);

	sensor_ready_notifier_chain_register(&sap_custom_cmd_ready_notifier);
	return mtk_ipi_register(&sap_ipidev, IPI_IN_SENSOR_SAP_NOTIFY,
		sap_custom_cmd_notify_handler, NULL, notify_payload);
}

void sap_custom_cmd_exit(void)
{
	if (!sap_enabled())
		return;

	sensor_ready_notifier_chain_unregister(&sap_custom_cmd_ready_notifier);
	mtk_ipi_unregister(&sap_ipidev, IPI_IN_SENSOR_SAP_NOTIFY);
}

