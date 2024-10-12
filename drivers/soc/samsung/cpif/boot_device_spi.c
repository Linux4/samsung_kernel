// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014-2019, Samsung Electronics.
 *
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

#define MAX_IMAGE_SIZE	SZ_128K
#define MAX_SPI_DEVICE	2

struct cpboot_spi {
	struct spi_device *spi;
};

static struct cpboot_spi _cpboot[MAX_SPI_DEVICE];

static struct cpboot_spi *cpboot_spi_get_device(int bus_num)
{
	int i;

	for (i = 0; i < MAX_SPI_DEVICE; i++) {
		if (_cpboot[i].spi && bus_num == _cpboot[i].spi->controller->bus_num) {
			mif_info("Get bus_num:%d\n", bus_num);
			return &_cpboot[i];
		}
	}

	mif_err("Can not get bus_num:%d\n", bus_num);

	return NULL;
}

/*
 * Export functions
 */
int cpboot_spi_load_cp_image(struct link_device *ld, struct io_device *iod, unsigned long arg)
{
	int ret = 0;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	struct cpboot_spi *cpboot = cpboot_spi_get_device(mld->spi_bus_num);
	struct cp_image img;
	char *buff = NULL;
	struct spi_message msg;
	struct spi_transfer xfer;

	if (!cpboot || !cpboot->spi) {
		mif_err("spi is null\n");
		return -EPERM;
	}

	ret = copy_from_user(&img, (const void __user *)arg, sizeof(struct cp_image));
	if (ret) {
		mif_err("copy_from_user() arg error:%d\n", ret);
		goto exit;
	}

	mif_info("size:%d bus_num:%d\n", img.size, cpboot->spi->controller->bus_num);
	if ((img.size == 0) || (img.size > MAX_IMAGE_SIZE))  {
		mif_err("size error:%d\n", img.size);
		ret = -EINVAL;
		goto exit;
	}

	/* MUST not enable dma-mode on SPI
	 * dma-mode does not support non-contiguous buffer
	 */
	buff = kvzalloc(img.size, GFP_KERNEL);
	if (!buff) {
		mif_err("vzalloc(%u) error\n", img.size);
		ret = -ENOMEM;
		goto exit;
	}

	ret = copy_from_user(buff, (const void __user *)img.binary, img.size);
	if (ret) {
		mif_err("copy_from_user() buff error:%d\n", ret);
		goto exit;
	}

	memset(&xfer, 0, sizeof(struct spi_transfer));
	xfer.len = img.size;
	xfer.tx_buf = buff;
	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(cpboot->spi, &msg);
	if (ret < 0) {
		mif_err("spi_sync() error:%d\n", ret);
		goto exit;
	}

exit:
	if (buff)
		kvfree(buff);

	return ret;
}
EXPORT_SYMBOL(cpboot_spi_load_cp_image);

/*
 * Probe
 */
static int cpboot_spi_probe(struct spi_device *spi)
{
	static int _count;
	int ret = 0;

	mif_info("bus_num:%d count:%d\n", spi->controller->bus_num, _count);

	if (_count >= MAX_SPI_DEVICE) {
		mif_err("_count is over %d\n", MAX_SPI_DEVICE);
		ret = -EINVAL;
		goto err;
	}

	spi->bits_per_word = 8;
	if (spi_setup(spi)) {
		mif_err("ERR! spi_setup fail\n");
		ret = -EINVAL;
		goto err;
	}
	spi_set_drvdata(spi, &_cpboot[_count]);
	_cpboot[_count].spi = spi;

	_count++;
	return 0;

err:
	panic("CP SPI driver probe failed\n");
	return ret;
}

static int cpboot_spi_remove(struct spi_device *spi)
{
	return 0;
}

static const struct of_device_id cpboot_spi_dt_match[] = {
	{ .compatible = "samsung,exynos-cp-spi" },
	{},
};
MODULE_DEVICE_TABLE(of, cpboot_spi_dt_match);

static struct spi_driver cpboot_spi_driver = {
	.probe = cpboot_spi_probe,
	.remove = cpboot_spi_remove,
	.driver = {
		.name = "cpboot_spi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cpboot_spi_dt_match),
		.suppress_bind_attrs = true,
	},
};
module_spi_driver(cpboot_spi_driver);

MODULE_DESCRIPTION("Exynos SPI driver to load CP bootloader");
MODULE_LICENSE("GPL");
