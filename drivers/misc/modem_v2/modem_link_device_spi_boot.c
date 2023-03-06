/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/spi/spi.h>
#include <linux/if_arp.h>

#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_utils.h"

#include "modem_link_device_spi_boot.h"

#define to_spi_boot_link_dev(linkdev) \
		container_of(linkdev, struct spi_boot_link_device, ld)

struct spi_boot_link_device *spi_boot_ld;

static int spi_boot_init(struct link_device *ld, struct io_device *iod)
{
	return 0;
}

static void spi_boot_terminate(struct link_device *ld, struct io_device *iod)
{
}

static int spi_transmit_data(struct spi_device *spi,
		unsigned char *buf, size_t len)
{
	int ret;

	struct spi_message msg;
	struct spi_transfer t = {
		.len = len,
		.tx_buf = buf,
		.delay_usecs = 10,
	};

	spi_message_init(&msg);
	spi_message_add_tail(&t, &msg);

	ret = spi_sync(spi, &msg);
	if (ret < 0)
		mif_err("spi_sync() fail(%d)\n", ret);

	return ret;
}

static int spi_boot_tx_skb(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb)
{
	int ret = -EINVAL;

	struct spi_boot_link_device *sbld = to_spi_boot_link_dev(ld);

	unsigned char *buf = skb->data + SIPC5_HDR_LEN;
	int len = skb->len - SIPC5_HDR_LEN;

	skb_queue_tail(&sbld->tx_q, skb);

	ret = spi_transmit_data(sbld->spi, buf, len);
	if (ret < 0) {
		mif_err("spi_transmit_data() failed(%d)\n", ret);
		goto exit;
	}

exit:
	skb_unlink(skb, &sbld->tx_q);

	return ret;
}

static int spi_boot_send(struct link_device *ld, struct io_device *iod,
		struct sk_buff *skb)
{
	int ret = -EINVAL;

	if (iod->format != IPC_BOOT || ld->mode != LINK_MODE_BOOT) {
		mif_err("iod->format: %d, ld->mode: %d\n",
			iod->format, ld->mode);
		goto err;
	}

	ret = spi_boot_tx_skb(ld, iod, skb);
	if (ret < 0) {
		mif_err("spi_boot_tx_skb() failed(%d)\n", ret);
		goto err;
	}

	return skb->len;
err:
	mif_err("%s: drop packet, size=%d, com_state=%d,",
		iod->name, skb->len, ld->com_state);
	dev_kfree_skb_any(skb);

	return 0;
}

static int __devinit spi_boot_probe(struct spi_device *spi)
{
	int ret = -ENODEV;

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	ret = spi_setup(spi);
	if (ret < 0) {
		mif_err("spi_setup() fail(%d)\n", ret);
		return ret;
	}

	spi_boot_ld->spi = spi;

	mif_info("success\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spi_modem_boot_match[] = {
	{ .compatible = "spi_boot_link", },
	{}
};
MODULE_DEVICE_TABLE(of, spi_modem_boot_match);
#endif

static struct spi_driver spi_boot_driver = {
	.probe = spi_boot_probe,
	.driver = {
		.name = "spi_modem_boot",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(spi_modem_boot_match),
#endif
	},
};

struct link_device *spi_create_link_device(struct platform_device *pdev)
{
	int ret;

	struct modem_data *pdata = pdev->dev.platform_data;
	struct link_device *ld;

	if (!pdata) {
		mif_err("modem_data is null\n");
		return NULL;
	}

	spi_boot_ld = kzalloc(sizeof(struct spi_boot_link_device), GFP_KERNEL);
	if (!spi_boot_ld) {
		mif_err("allocation fail for spi_boot_ld\n");
		return NULL;
	}

	INIT_LIST_HEAD(&spi_boot_ld->ld.list);

	ld = &spi_boot_ld->ld;

	ld->name = "spi_boot";
	ld->init_comm = spi_boot_init;
	ld->terminate_comm = spi_boot_terminate;
	ld->send = spi_boot_send;
	ld->com_state = COM_NONE;

	ret = spi_register_driver(&spi_boot_driver);
	if (ret) {
		mif_err("spi_register_driver() fail(%d)\n", ret);
		goto err;
	}

	skb_queue_head_init(&spi_boot_ld->tx_q);

	mif_info("success\n");

	return ld;
err:
	kfree(spi_boot_ld);

	return NULL;
}
