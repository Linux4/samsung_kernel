/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "el7xx.h"

int el7xx_spi_setup_conf(struct el7xx_data *etspi, u32 bits)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;

	if (bits < 1 || bits > 4) {
		retval = -EINVAL;
		pr_err("Invalid argument. bits(%d) > 4\n", bits);
		goto end;
	}

	etspi->spi->bits_per_word = 8 * bits;
	if (etspi->prev_bits_per_word != etspi->spi->bits_per_word) {
		retval = spi_setup(etspi->spi);
		if (retval != 0) {
			pr_err("spi_setup() is failed. status : %d\n", retval);
			goto end;
		}

		pr_info("prev-bpw:%d, bpw:%d\n",
				etspi->prev_bits_per_word, etspi->spi->bits_per_word);
		etspi->prev_bits_per_word = etspi->spi->bits_per_word;
	}
end:
	return retval;
#endif
}

int el7xx_io_read_register(struct el7xx_data *etspi, u8 *addr, u8 *buf, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	int read_len = 1;

	u8 val, addrval;

	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = ioc->len + 2,
	};

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R;

	if (copy_from_user(&addrval, (const u8 __user *) (uintptr_t) addr
		, read_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	*(etspi->buf + 1) = addrval;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	val = *(etspi->buf + 2);

	pr_debug("len = %d addr = %x val = %x\n", read_len, addrval, val);

	if (copy_to_user((u8 __user *) (uintptr_t) buf, &val, read_len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
#endif
}

int el7xx_io_burst_read_register(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = ioc->len + 2,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R_S;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
				ioc->len)) {
		retval = -EFAULT;
		pr_err("buffer copy_to_user fail retval\n");
		goto end;
	}
end:
	return retval;
#endif
}

int el7xx_io_burst_read_register_backward(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = ioc->len + 2,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R_S_BW;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t)ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
			ioc->len)) {
		retval = -EFAULT;
		pr_err("buffer copy_to_user fail retval\n");
		goto end;
	}
end:
	return retval;
#endif
}

int el7xx_io_write_register(struct el7xx_data *etspi, u8 *buf, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	int write_len = 2;
	struct spi_message m;

	u8 val[3];

	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = ioc->len + 1,
	};

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_W;

	if (copy_from_user(val, (const u8 __user *) (uintptr_t) buf,
			write_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	pr_debug("write_len = %d addr = %x data = %x\n",
			write_len, val[0], val[1]);

	*(etspi->buf + 1) = val[0];
	*(etspi->buf + 2) = val[1];
	*(etspi->buf + 3) = val[1];

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	return retval;
#endif
}

int el7xx_io_burst_write_register(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = ioc->len + 1,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	*etspi->buf = OP_REG_W_S;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		goto end;
	}
end:
	return retval;
#endif
}

int el7xx_io_burst_write_register_backward(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = ioc->len + 1,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	*etspi->buf = OP_REG_W_S_BW;
	if (copy_from_user(etspi->buf + 1,
		(const u8 __user *) (uintptr_t)ioc->tx_buf, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
		ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		goto end;
	}
end:
	return retval;
#endif
}

int el7xx_io_read_efuse(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = ioc->len + 1,
	};

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_EF_R;

	pr_debug("len = %d, xfer.len = %d, buf = %p, rx_buf = %p\n",
			ioc->len, xfer.len, buf, ioc->rx_buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf, buf + 1,
			ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}
end:
	kfree(buf);
	return retval;
#endif
}

int el7xx_io_write_efuse(struct el7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = ioc->len + 1,
	};

	buf = kzalloc(xfer.len, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user((u8 __user *) (uintptr_t) buf + 1, ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail retval\n");
		retval = -EFAULT;
		goto end;
	}

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_EF_W;

	pr_debug("len = %d, xfer.len = %d, buf = %p, tx_buf = %p\n",
			 ioc->len, xfer.len, buf, ioc->tx_buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0)
		pr_err("write data error retval = %d\n", retval);
end:
	kfree(buf);
	return retval;
#endif
}

int el7xx_io_get_frame(struct el7xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = size + 1,
	};

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_FB_R;

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, xfer.len, buf, fr);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) fr, buf + 1, size)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}

end:
	kfree(buf);
	return retval;
#endif
}

int el7xx_io_write_frame(struct el7xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
	    .tx_buf = NULL,
	    .rx_buf = NULL,
	    .len = size + 1,
	};

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user((u8 __user *)(uintptr_t)buf + 1, fr, size)) {
		pr_err("buffer copy_from_user fail retval\n");
		retval = -EFAULT;
		goto end;
	}

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_FB_W;

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, xfer.len, buf, fr);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0)
		pr_err("write data error retval = %d\n", retval);

end:
	kfree(buf);
	return retval;
#endif
}

int el7xx_io_transfer_command(struct el7xx_data *etspi, u8 *tx, u8 *rx, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	u8 *tr;

	struct spi_transfer xfer = {
	    .len = size,
	};

	pr_info("tx(%p), rx(%p), size(%d)\n", tx, rx, size);
	tr = kzalloc(size, GFP_KERNEL);
	if (tr == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = tr;

	if (copy_from_user(tr, (const u8 __user *)(uintptr_t)tx, size)) {
		pr_err("buffer copy_from_user fail. tr(%p), tx(%p)\n", tr,
		       tx);
		retval = -EFAULT;
		goto out;
	}

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("retval = %d\n", retval);
		goto out;
	}

	if (copy_to_user(rx, (const u8 __user *)(uintptr_t)tr, size)) {
		pr_err("buffer copy_to_user fail. tr(%p) rx(%p)\n", tr,
		       rx);
		retval = -EFAULT;
		goto out;
	}

out:
	kfree(tr);
	return retval;
#endif
}

int el7xx_write_register(struct el7xx_data *etspi, u8 addr, u8 buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;

	u8 tx[] = {OP_REG_W, addr, buf};

	struct spi_transfer xfer = {
		.tx_buf = tx,
		.rx_buf	= NULL,
		.len = 3,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval == 0) {
		pr_info("address = %x\n", addr);
	} else {
		pr_err("read data error retval = %d\n", retval);
	}

	return retval;
#endif
}

int el7xx_read_register(struct el7xx_data *etspi, u8 addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;

	u8 read_value[] = {OP_REG_R, addr, 0x00, 0x00};
	u8 result[] = {0xFF, 0xFF, 0xFF, 0xFF};

	struct spi_transfer xfer = {
		.tx_buf = read_value,
		.rx_buf	= result,
		.len = 4,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval == 0) {
		*buf = result[2];
		pr_info("address = %x result = %x %x\n", addr, result[1], result[2]);
	} else {
		pr_err("read data error retval = %d\n", retval);
	}

	return retval;
#endif
}

