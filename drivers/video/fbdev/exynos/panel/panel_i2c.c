// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include "panel_drv.h"
#include "panel_i2c.h"

#define PANEL_I2C_DRIVER_NAME "panel_i2c"

#define MAX_TX_SIZE 16
#define MAX_RX_SIZE 16

int panel_i2c_tx(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len)
{
	int ret = 0;
	int tx_len = i2c_dev->tx_len;
	int tx_cnt = 0;
	int i = 0;
	u8 tx_buf[MAX_TX_SIZE] = {0, };

	struct i2c_msg msg[] = {
		{
			.addr = i2c_dev->client->addr,
			.flags = 0,
			.len = tx_len,
			.buf = tx_buf,
		}
	};

	if (unlikely(!i2c_dev->client)) {
		pr_err("%s: failed, i2c Client is NULL.\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(&i2c_dev->lock);

	//pr_info("%s ++ [0]:0x%02x, len:%03d\n", __func__, arr[0], arr_len);

	tx_cnt = arr_len / i2c_dev->tx_len;

	if (arr_len % i2c_dev->tx_len) {
		pr_err("%s there is rest. arr_size: %d rest: %d.\n",
				__func__, arr_len, arr_len % i2c_dev->tx_len);
	}

	for (i = 0; i < tx_cnt; i++) {
		memcpy(tx_buf, arr + (i * tx_len), tx_len);

		ret = i2c_transfer(i2c_dev->client->adapter, msg, 1);
		if (unlikely(ret < 0)) {
			pr_err("%s: i2c tx failed. [0x%02x]... : [0x%02x]... error %d\n",
					__func__,  msg[0].buf[0], msg[0].buf[1], ret);
			goto tx_ret;
		}
	}

tx_ret:
	pr_info("%s --\n", __func__);
	mutex_unlock(&i2c_dev->lock);

	return 0;
}

int panel_i2c_rx(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len)
{
	int tx_len = i2c_dev->tx_len;
	int rx_cnt = 0;
	int i = 0;
	int j = 0;
	int printout_pos = 0;
	int ret = 0;
	char printout_buf[128] = {0, };
	u8 addr_buf[MAX_TX_SIZE] = {0, };
	u8 rx_buf[MAX_RX_SIZE] = {0, };

	struct i2c_msg msg[] = {
		{
			.addr = i2c_dev->client->addr,
			.flags = 0,
			.len = i2c_dev->addr_len,
			.buf = addr_buf,
		},
		{
			.addr = i2c_dev->client->addr,
			.flags = I2C_M_RD,
			.len = i2c_dev->data_len,
			.buf = rx_buf,
		},
	};

	if (unlikely(!i2c_dev->client)) {
		pr_err("%s: failed, i2c Client is NULL.\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(&i2c_dev->lock);

	rx_cnt = arr_len / i2c_dev->tx_len;

	pr_info("========= %s =========\n", __func__);
	for (i = 0; i < rx_cnt; i++) {
		memcpy(addr_buf, arr + (i * tx_len), i2c_dev->addr_len);
		memset(printout_buf, 0x00, ARRAY_SIZE(printout_buf));
		printout_pos = 0;

		ret = i2c_transfer(i2c_dev->client->adapter, msg, 2);
		if (unlikely(ret < 0)) {
			pr_err("%s: i2c rx failed at [%d][0x%02x] error %d\n",
					__func__,  i, msg[0].buf[0], ret);
			goto rx_ret;
		}

		/* addr * addr_len */
		for (j = 0; j < i2c_dev->addr_len; j++) {
			snprintf(printout_buf + printout_pos, 7, "[0x%02x]", addr_buf[j]);
			printout_pos = printout_pos + 6;
		}

		snprintf(printout_buf + printout_pos, 3, "=>");
		printout_pos = printout_pos + 2;

		/* rx_buf * data_len */
		for (j = 0; j < i2c_dev->data_len; j++) {
			snprintf(printout_buf + printout_pos, 7, "[0x%02x]", rx_buf[j]);
			printout_pos = printout_pos + 6;
		}

		pr_info("%s\n", printout_buf);
	}

rx_ret:
	mutex_unlock(&i2c_dev->lock);

	return 0;
}

static struct i2c_drv_ops panel_i2c_drv_ops = {
	.tx = panel_i2c_tx,
	.rx = panel_i2c_rx,
};

static int panel_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct panel_device *panel  = NULL;
	int ret = 0;

	if (unlikely(!client)) {
		pr_err("%s, invalid i2c\n", __func__);
		return -EINVAL;
	}

	if (id && id->driver_data)
		panel = (struct panel_device *)id->driver_data;

	panel->panel_i2c_dev.client = client;

	dev_info(&client->dev, "%s: (%s)(%s)\n", __func__, dev_name(&client->adapter->dev),
						of_node_full_name(client->dev.of_node));

	return ret;
}

static int panel_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_device_id panel_i2c_id[] = {
	{"panel_i2c", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, panel_i2c_id);

static const struct of_device_id panel_i2c_dt_ids[] = {
	{ .compatible = "panel_i2c" },
	{ }
};

MODULE_DEVICE_TABLE(of, panel_i2c_dt_ids);

static struct i2c_driver panel_i2c_driver = {
	.driver = {
		.name	= PANEL_I2C_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(panel_i2c_dt_ids),
	},
	.id_table = panel_i2c_id,
	.probe = panel_i2c_probe,
	.remove = panel_i2c_remove,
};

int panel_i2c_drv_probe(struct panel_device *panel, struct i2c_data *i2c_data)
{
	struct panel_i2c_dev *i2c_dev;
	int ret = 0;

	dev_info(panel->dev, "+ %s\n", __func__);

	panel_i2c_id->driver_data = (kernel_ulong_t)panel;
	ret = i2c_add_driver(&panel_i2c_driver);

	if (ret) {
		panel_err("%s failed to register for i2c device\n", __func__);
		goto rest_init;
	}

	i2c_dev = &panel->panel_i2c_dev;

	/* copy info */
	i2c_dev->addr_len = i2c_data->addr_len;
	i2c_dev->data_len = i2c_data->data_len;
	i2c_dev->tx_len = i2c_data->addr_len  + i2c_data->data_len;
	i2c_dev->ops = &panel_i2c_drv_ops;

	mutex_init(&i2c_dev->lock);

rest_init:
	dev_info(panel->dev, "- %s\n", __func__);
	return ret;
}

MODULE_DESCRIPTION("i2c driver for panel");
MODULE_LICENSE("GPL");
