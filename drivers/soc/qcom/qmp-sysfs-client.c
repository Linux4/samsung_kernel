// SPDX-License-Identifier: GPL-2.0-only

/* Copyright (c) 2017-2019, 2021 The Linux Foundation. All rights reserved. */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mailbox_client.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/mailbox/qmp.h>
#include <linux/uaccess.h>
#include <linux/mailbox_controller.h>

#define MAX_MSG_SIZE 96 /* Imposed by the remote*/

struct qmp_sysfs_data {
	struct qmp_pkt pkt;
	char buf[MAX_MSG_SIZE + 1];
};

static struct qmp_sysfs_data data_pkt[MBOX_TX_QUEUE_LEN];
static struct mbox_chan *chan;
static struct mbox_client *cl;

static DEFINE_MUTEX(qmp_sysfs_mutex);
bool ddr_fmax_limit = false;

static ssize_t aop_msg_write(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t len)
{
	static int count;
	int target, ret;
	int cmd_len;
	
	if (!len || (len > MAX_MSG_SIZE))
		return len;

	ret = kstrtoint(buf, 10, &target);
	if (ret)
		return -EINVAL;

	if ((target != 2736) && (target != 3196)) {
		pr_err("[%s] %d is not a valid input\n", __func__, target);
		return -EINVAL;
	}
	
	mutex_lock(&qmp_sysfs_mutex);

	if (count >= MBOX_TX_QUEUE_LEN)
		count = 0;
	
	memset(&(data_pkt[count]), 0, sizeof(data_pkt[count]));

	cmd_len = snprintf(data_pkt[count].buf, MAX_MSG_SIZE, "{class: ddr, res: capped, val: %d}", target);

	/*
	 * Trim the leading and trailing white spaces
	 */
	strim(data_pkt[count].buf);

	pr_info("[%s] cmd : %s (%d)\n", __func__, data_pkt[count].buf, cmd_len);
	/*
	 * Controller expects a 4 byte aligned buffer
	 */
	
	data_pkt[count].pkt.size = (cmd_len + 0x3) & ~0x3;
	data_pkt[count].pkt.data = data_pkt[count].buf;

	if (mbox_send_message(chan, &(data_pkt[count].pkt)) < 0)
		pr_err("Failed to send qmp request\n");
	else
	{
		ddr_fmax_limit = (target == 3196) ? false : true;
		count++;
	}

	mutex_unlock(&qmp_sysfs_mutex);
	return len;
}

static DEVICE_ATTR(aop_send_message, 0200, NULL, aop_msg_write);

static int qmp_msg_probe(struct platform_device *pdev)
{
	int ret;
	
	pr_err("[%s] +++ %s +++\n", __func__, dev_name(&pdev->dev));

	cl = devm_kzalloc(&pdev->dev, sizeof(*cl), GFP_KERNEL);
	if (!cl)
		return -ENOMEM;

	cl->dev = &pdev->dev;
	cl->tx_block = true;
	cl->tx_tout = 1000;
	cl->knows_txdone = false;

	chan = mbox_request_channel(cl, 0);
	if (IS_ERR(chan)) {
		dev_err(&pdev->dev, "Failed to mbox channel\n");
		return PTR_ERR(chan);
	}

	ret = device_create_file(&pdev->dev, &dev_attr_aop_send_message);

	if (ret)
		goto err;

	mutex_init(&qmp_sysfs_mutex);
	
	pr_err("[%s] successfully probed\n", __func__);
	return 0;
err:
	mbox_free_channel(chan);
	chan = NULL;
	return -ENOMEM;
}

static const struct of_device_id aop_qmp_match_tbl[] = {
	{.compatible = "qcom,sysfs-qmp-client"},
	{},
};

static struct platform_driver aop_qmp_msg_driver = {
	.probe = qmp_msg_probe,
	.driver = {
		.name = "sysfs-qmp-client",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
		.of_match_table = aop_qmp_match_tbl,
	},
};
builtin_platform_driver(aop_qmp_msg_driver);
