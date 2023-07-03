/*
 * driver/irled/watchonir.c
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/sec_sysfs.h>

#include "irled-common.h"

#ifdef CONFIG_OF
static struct of_device_id watchonir_match_table[] = {
	{ .compatible = "nochip,irled", },
	{ },
};
#else
#define watchonir_match_table NULL
#endif

#define BUFFER_SIZE (64 * 1024)
#define WORD_SIZE   2

struct watchonir {
	struct spi_device *spi;
	struct device *sec_dev;
	struct spi_message spi_msg;
	struct spi_transfer spi_xfer;
	u8 *tx_buf;
	u32 hz;
	int duty;
	struct irled_power irled_power;
};

static u16 g_duty[] = {
	(0x0000) << 15,
	(0x0001) << 14,
	(0x0003) << 13,
	(0x0007) << 12,
	(0x000f) << 11,
	(0x001f) << 10,
	(0x003f) << 9,
	(0x007f) << 8,
	(0x00ff) << 7,
	(0x01ff) << 6,
	(0x03ff) << 5,
	(0x07ff) << 4,
	(0x0fff) << 3,
	(0x1fff) << 2,
	(0x3fff) << 1,
	(0x7fff) << 0,
};

static int prev_tx_status;

static int watchonir_write(struct watchonir *watchonir, u32 hz, char *buf, size_t count)
{
	struct spi_message *spi_msg = &watchonir->spi_msg;
	struct spi_device *spi = watchonir->spi;
	struct spi_transfer *spi_xfer = kcalloc(1, sizeof(struct spi_transfer), GFP_KERNEL);
	u8 *tx_buf = watchonir->tx_buf;
	u32 size = (count < BUFFER_SIZE * WORD_SIZE) ? count : BUFFER_SIZE * WORD_SIZE;
	int ret;

	spi_message_init(spi_msg);
	spi_xfer->tx_buf = tx_buf;
	spi_xfer->rx_buf = NULL;
	spi_xfer->len = size;
	spi_xfer->bits_per_word = WORD_SIZE * 8;
	spi_xfer->speed_hz = hz;
	spi_message_add_tail(spi_xfer, spi_msg);

	ret = spi_sync(spi, spi_msg);

	kfree(spi_xfer);

	return ret;
}

static unsigned int g_support_hz[] = {
	12570,
	30300,
	32750,
	36000,
	38000,
	40000,
	56000
};

static ssize_t
watchonir_bin_write(struct file *fp, struct kobject *kobj, struct bin_attribute *bin_attr,
		    char *buf, loff_t off, size_t count)
{
	struct device *dev;
	struct watchonir *watchonir;

	dev = container_of(kobj, struct device, kobj);
	watchonir = dev_get_drvdata(dev);

	pr_info("%s: off(0x%d) count(0x%d)\n", __func__, (int)off, (int)count);

	if (off == 0 && count == sizeof(int)) {
		int lop;
		unsigned int minhz = 0xffffffff;
		unsigned int minval = 0xffffffff;
		unsigned int hz = *(u32 *)(buf + off);
		unsigned int deltahz;

		for (lop = 0; lop < ARRAY_SIZE(g_support_hz); lop++) {
			unsigned int curhz = g_support_hz[lop];

			if (hz < curhz)
				deltahz = curhz - hz;
			else
				deltahz = hz - curhz;

			if (minval > deltahz) {
				minval = deltahz;
				minhz = curhz;
			}
		}

		prev_tx_status = 0;
		watchonir->hz = minhz * 16;
		pr_info("%s: set hz(%d)\n", __func__, watchonir->hz);
	} else if (count > 0) {
		if (off + count > BUFFER_SIZE * WORD_SIZE) {
			pr_info("%s: buffer overflow!\n", __func__);
			return 0;
		}
		memcpy(watchonir->tx_buf + off, buf, count);

		if (count != PAGE_SIZE) {
			int ret;

			ret = irled_power_on(&watchonir->irled_power);
			if (ret)
				pr_info("%s: power on error\n", __func__);

			ret = watchonir_write(watchonir, watchonir->hz, (char *)watchonir->tx_buf, off + count);

			if (ret) {
				prev_tx_status = 0;
				pr_info("%s: irsend FAIL\n", __func__);
			} else {
				prev_tx_status = 1;
				pr_info("%s: irsend SUCCESS\n", __func__);
			}

			ret = irled_power_off(&watchonir->irled_power);
			if (ret)
				pr_info("%s: power off error\n", __func__);
		}
	} else {
		pr_info("%s: invalid count\n", __func__);
	}

	return count;
}

static ssize_t ir_send_result_show(struct device *dev, struct device_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%d\n", prev_tx_status);
}

static ssize_t ir_send_result_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static struct bin_attribute bin_attr_watchonir;
static DEVICE_ATTR(ir_send_result, S_IRUGO | S_IWUSR | S_IWGRP, ir_send_result_show, ir_send_result_store);

static struct attribute *sec_ir_attributes[] = {
	&dev_attr_ir_send_result.attr,
	NULL,
};

static struct attribute_group sec_ir_attr_group = {
	.attrs = sec_ir_attributes,
};

#if defined(CONFIG_OF)
static int of_watchonir_parse_dt(struct device *dev, struct watchonir *watchonir)
{
	struct device_node *np_irled = dev->of_node;
	const __be32 *prop;
	int len;
	int ret;

	int default_hz = 38000;
	int duty = 6;

	if (!np_irled)
		return -EINVAL;

	pr_info("%s\n", __func__);

	prop = of_get_property(np_irled, "irled,default_hz", &len);
	if (!prop || len < sizeof(*prop)) {
		pr_info("%s: default HZ error\n", __func__);
		return -EINVAL;
	} else {
		default_hz = be32_to_cpup(prop);
		pr_info("%s: default HZ : %d\n", __func__, default_hz);
	}

	prop = of_get_property(np_irled, "irled,high_bit_size", &len);
	if (!prop || len < sizeof(*prop)) {
		pr_info("%s: high_bit_size error\n", __func__);
		return -EINVAL;
	} else {
		duty = be32_to_cpup(prop);

		if (duty >= ARRAY_SIZE(g_duty) || duty < 0) {
			pr_info("%s: duty error\n", __func__);
			return -EINVAL;
		}
	}

	ret = of_irled_power_parse_dt(dev, &watchonir->irled_power);

	watchonir->hz = default_hz;
	watchonir->duty = duty;

	return ret;
}
#else
static int of_watchonir_parse_dt(struct device *dev, struct watchonir *watchonir)
{
	return -EINVAL;
}
#endif /* CONFIG_OF */

static int watchonir_probe(struct spi_device *spi)
{
	struct watchonir *watchonir;
	u8 *tx_buf;
	struct device *sec_dev;
	int ret;

	pr_info("%s\n", __func__);

	watchonir = kzalloc(sizeof(struct watchonir), GFP_KERNEL);
	if (watchonir == NULL) {
		pr_info("%s: watchonir alloc error\n", __func__);
		return -ENOMEM;
	}

	tx_buf = kmalloc(BUFFER_SIZE * WORD_SIZE + 128, GFP_ATOMIC);
	if (tx_buf == NULL) {
		kfree(watchonir);
		pr_info("%s: tx_buf alloc error\n", __func__);
		return -ENOMEM;
	}

	if (spi->dev.of_node) {
		ret = of_watchonir_parse_dt(&spi->dev, watchonir);
		if (ret) {
			pr_info("%s: parse dt error\n", __func__);
			goto err;
		}
	}

	sec_dev = sec_device_create(&watchonir, "sec_ir");
	if (IS_ERR(sec_dev)) {
		pr_info("%s: sec_ir create error\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	ret = sysfs_create_group(&sec_dev->kobj, &sec_ir_attr_group);
	if (ret) {
		pr_info("%s: sec_ir_attr_group create error\n", __func__);
		sec_device_destroy(sec_dev->devt);
		goto err;
	}

	bin_attr_watchonir.attr.name = "ir_data";
	bin_attr_watchonir.attr.mode = S_IWUGO;
	bin_attr_watchonir.read = NULL;
	bin_attr_watchonir.write = watchonir_bin_write;
	bin_attr_watchonir.size = BUFFER_SIZE * WORD_SIZE;

	ret = sysfs_create_bin_file(&sec_dev->kobj, &bin_attr_watchonir);
	if (ret) {
		pr_info("%s: sec watchonir create error\n", __func__);
		sysfs_remove_group(&sec_dev->kobj, &sec_ir_attr_group);
		sec_device_destroy(sec_dev->devt);
		goto err;
	}

	watchonir->spi = spi;
	watchonir->tx_buf = tx_buf;
	watchonir->sec_dev = sec_dev;

	dev_set_drvdata(sec_dev, watchonir);
	spi_set_drvdata(spi, watchonir);

	memset(tx_buf, 0, BUFFER_SIZE * WORD_SIZE);

	return ret;

err:
	kfree(tx_buf);
	kfree(watchonir);

	return ret;
}

int watchonir_remove(struct spi_device *spi)
{
	struct watchonir *watchonir;

	watchonir = spi_get_drvdata(spi);

	if (watchonir) {
		struct device *sec_dev;

		sec_dev = watchonir->sec_dev;
		if (sec_dev) {
			sysfs_remove_bin_file(&sec_dev->kobj, &bin_attr_watchonir);
			sysfs_remove_group(&sec_dev->kobj, &sec_ir_attr_group);
			sec_device_destroy(sec_dev->devt);
		}

		spi_set_drvdata(spi, NULL);

		kfree(watchonir->tx_buf);
		kfree(watchonir);
	}

	return 0;
}

static struct spi_driver watchonir_driver = {
	.driver = {
		.name = "watchonir",
		.owner = THIS_MODULE,
		.of_match_table = watchonir_match_table,
	},
	.probe = watchonir_probe,
};

static int __init watchonir_init(void)
{
	int ret;

	ret = spi_register_driver(&watchonir_driver);
	pr_info("%s: ret(%d)\n", __func__, ret);

	return ret;
}

static void __exit watchonir_exit(void)
{
	spi_unregister_driver(&watchonir_driver);
}

module_init(watchonir_init);
module_exit(watchonir_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WATCHON IR");
