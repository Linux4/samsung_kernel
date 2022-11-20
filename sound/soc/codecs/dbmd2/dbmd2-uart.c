/*
 * DSPG DBMD2 UART interface driver
 *
 * Copyright (C) 2014 DSP Group
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
/* #define DEBUG */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/tty.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>

#include "dbmd2-interface.h"
#include "dbmd2-va-regmap.h"
#include "dbmd2-uart-sbl.h"

#define DRIVER_VERSION				"1.0"

#define RETRY_COUNT				5

#define MAX_MSECS_IN_FW_BUFFER			3000

/* baud rate used during fw upload */
#define UART_TTY_MAX_BAUD_RATE			3000000
/* baud rate used during boot-up */
#define UART_TTY_BOOT_BAUD_RATE			115200
/* baud rate used during fast boot-up */
#define UART_TTY_FAST_BOOT_BAUD_RATE		230400
/* baud rate used when in normal command mode */
#define UART_TTY_NORMAL_BAUD_RATE		57600
/* number of stop bits during boot-up */
#define UART_TTY_BOOT_STOP_BITS			2
/* number of stop bits during normal operation */
#define UART_TTY_NORMAL_STOP_BITS		1
#define UART_SYNC_LENGTH			300 /* in msec */

static const u8 clr_crc[] = {0x5A, 0x03, 0x52, 0x0a, 0x00,
			     0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 boot_cmd[2] = {0x5a, 0x0b};
static const u8 read_checksum[2] = {0x5a, 0x0e};

static int uart_set_speed_host_only(struct dbmd2_private *p, int index);
static int uart_set_speed(struct dbmd2_private *p, int index);
static int uart_wait_till_alive(struct dbmd2_private *p);
static int uart_boot_rate_by_clk(struct dbmd2_private *p, enum xtal_id clk_id);

struct dbmd2_uart_private {
	struct platform_device		*pdev;
	struct dbd2_uart_data		*pdata;
	struct device			*dev;
	struct chip_interface		chip;
	struct tty_struct		*tty;
	struct file			*fp;
	struct tty_ldisc		*ldisc;
	unsigned int			boot_baud_rate;
	unsigned int			boot_lock_buffer_size;
	int				uart_open;
	atomic_t			stop_uart_probing;
	struct task_struct		*uart_probe_thread;
	struct completion		uart_done;
	u8				tbuf[MAX_READ_SZ];
};

static int uart_configure_tty(struct dbmd2_uart_private *p, u32 bps, int stop,
			      int parity, int flow);

static int uart_open_file(struct dbmd2_uart_private *p)
{
	long err = 0;
	struct file *fp;
	int attempt = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(60000);

	if (p->uart_open)
		goto out_ok;

	/*
	 * Wait for the device node to appear in the filesystem. This can take
	 * some time if the kernel is still booting up and filesystems are
	 * being mounted.
	 */
	do {
		msleep(50);
		dev_dbg(p->dev,
			"%s(): probing for tty on %s (attempt %d)\n",
			 __func__, p->pdata->uart_dev, ++attempt);

		fp = filp_open(p->pdata->uart_dev,
				O_RDWR | O_NONBLOCK | O_NOCTTY, 0);

		err = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && (err == -ENOENT) &&
		 (atomic_read(&p->stop_uart_probing) == 0));

	if (atomic_read(&p->stop_uart_probing)) {
		dev_dbg(p->dev, "%s: UART probe thread stopped\n", __func__);
		atomic_set(&p->stop_uart_probing, 0);
		err = -EIO;
		goto out;
	}

	if (IS_ERR_OR_NULL(fp)) {
		dev_err(p->dev, "%s: UART device node open failed\n", __func__);
		err = -ENODEV;
		goto out;
	}

	/* set uart_dev members */
	p->fp = fp;
	p->tty = ((struct tty_file_private *)fp->private_data)->tty;
	p->ldisc = tty_ldisc_ref(p->tty);
	p->uart_open = 1;
	err = 0;

	dev_dbg(p->dev, "%s: UART successfully opened\n", __func__);
out_ok:
	/* finish probe */
	complete(&p->uart_done);
out:
	return err;
}

static int uart_open_file_noprobe(struct dbmd2_uart_private *p)
{
	long err = 0;
	struct file *fp;
	int attempt = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	if (p->uart_open)
		goto out;

	/*
	 * Wait for the device node to appear in the filesystem. This can take
	 * some time if the kernel is still booting up and filesystems are
	 * being mounted.
	 */
	do {
		if (attempt > 0)
			msleep(50);
		dev_dbg(p->dev,
			"%s(): probing for tty on %s (attempt %d)\n",
			 __func__, p->pdata->uart_dev, ++attempt);

		fp = filp_open(p->pdata->uart_dev,
				O_RDWR | O_NONBLOCK | O_NOCTTY, 0);

		err = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && IS_ERR_OR_NULL(fp));


	if (IS_ERR_OR_NULL(fp)) {
		dev_err(p->dev, "%s: UART device node open failed, err=%d\n",
			__func__,
			(int)err);
		err = -ENODEV;
		goto out;
	}

	/* set uart_dev members */
	p->fp = fp;
	p->tty = ((struct tty_file_private *)fp->private_data)->tty;
	p->ldisc = tty_ldisc_ref(p->tty);
	p->uart_open = 1;

	err = 0;
	dev_dbg(p->dev, "%s: UART successfully opened\n", __func__);

out:
	return err;
}



static void uart_close_file(struct dbmd2_uart_private *p)
{
	if (p->uart_probe_thread) {
		atomic_inc(&p->stop_uart_probing);
		kthread_stop(p->uart_probe_thread);
		p->uart_probe_thread = NULL;
	}
	if (p->uart_open) {
		tty_ldisc_deref(p->ldisc);
		filp_close(p->fp, 0);
		p->uart_open = 0;
	}
	atomic_set(&p->stop_uart_probing, 0);
}

static void uart_flush_rx_fifo(struct dbmd2_uart_private *p)
{
	dev_dbg(p->dev, "%s\n", __func__);

	if (!p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return;
	}

	tty_buffer_flush(p->tty);
	tty_ldisc_flush(p->tty);
}

static int uart_configure_tty(struct dbmd2_uart_private *p, u32 bps, int stop,
			      int parity, int flow)
{
	int rc = 0;
	struct ktermios termios;

	if (!p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	memcpy(&termios, &(p->tty->termios), sizeof(termios));

	tty_wait_until_sent(p->tty, 0);
	usleep_range(50, 60);

	/* clear csize, baud */
	termios.c_cflag &= ~(CBAUD | CSIZE | PARENB | CSTOPB);
	termios.c_cflag |= BOTHER; /* allow arbitrary baud */
	termios.c_cflag |= CS8;
	if (parity)
		termios.c_cflag |= PARENB;

	if (stop == 2)
		termios.c_cflag |= CSTOPB;

	/* set uart port to raw mode (see termios man page for flags) */
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		| INLCR | IGNCR | ICRNL | IXON | IXOFF);

	if (flow && p->pdata->software_flow_control)
		termios.c_iflag |= IXOFF; /* enable XON/OFF for input */

	termios.c_oflag &= ~(OPOST);
	termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* set baud rate */
	termios.c_ospeed = bps;
	termios.c_ispeed = bps;

	rc = tty_set_termios(p->tty, &termios);
	return rc;
}

static ssize_t uart_read_data(struct dbmd2_private *p, void *buf, size_t len)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	mm_segment_t oldfs;
	int rc;
	int i = 0;
	size_t bytes_to_read = len;
	unsigned long timeout;

	dev_dbg(uart_p->dev, "%s\n", __func__);
	/* stuck for more than 1s means something went wrong */
	/*+ usecs_to_jiffies(max((size_t)200000, len * 20));*/
	timeout = jiffies + msecs_to_jiffies(1000);

	if (!uart_p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	do {
		rc = uart_p->ldisc->ops->read(uart_p->tty,
					      uart_p->fp,
					      (char __user *)buf + i,
					      bytes_to_read);
		if (rc > 0) {
			bytes_to_read -= rc;
			i += rc;
		} else if (rc == 0 || rc == -EAGAIN) {
			usleep_range(2000, 2100);
		} else
			dev_err(p->dev,
				"%s: Failed to read err= %d bytes to read=%zu\n",
				__func__,
				rc, bytes_to_read);
	} while (time_before(jiffies, timeout) && bytes_to_read);

	/* restore old fs context */
	set_fs(oldfs);

	if (bytes_to_read) {
		dev_err(uart_p->dev,
			"%s: timeout: unread %zu bytes ,requested %zu\n",
			__func__, bytes_to_read, len);
		return -EIO;
	}

	return len;
}

static ssize_t uart_write_data_no_sync(struct dbmd2_private *p, const void *buf,
				       size_t len)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret = 0;
	size_t count_remain = len;
	ssize_t bytes_wr = 0;
	unsigned int count;
	mm_segment_t oldfs;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (!uart_p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (count_remain > 0) {
		if (count_remain > p->wsize)
			count = p->wsize;
		else
			count = count_remain;
		/* block until tx buffer space is available */
		do {
			ret = tty_write_room(uart_p->tty);
			usleep_range(100, 110);
		} while (ret <= 0);

		if (ret < count)
			count = ret;

		ret = uart_p->ldisc->ops->write(uart_p->tty,
						uart_p->fp,
						buf + bytes_wr,
						min_t(size_t, p->wsize, count));

		if (ret < 0) {
			bytes_wr = ret;
			goto err_out;
		}

		bytes_wr += ret;
		count_remain -= ret;
	}

err_out:
	/* restore old fs context */
	set_fs(oldfs);

	return bytes_wr;
}

static ssize_t uart_write_data(struct dbmd2_private *p, const void *buf,
			       size_t len)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	ssize_t bytes_wr;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (!uart_p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	bytes_wr = uart_write_data_no_sync(p, buf, len);

	tty_wait_until_sent(uart_p->tty, 0);
	usleep_range(50, 60);

	return bytes_wr;
}

static ssize_t send_uart_cmd_vqe(struct dbmd2_private *p, u32 command,
			     u16 *response)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	char tmp[3];
	u8 send[7];
	u8 recv[6] = {0, 0, 0, 0, 0, 0};
	int ret;

	dev_dbg(uart_p->dev, "%s: Send 0x%04x\n", __func__, command);

	if (response)
		uart_flush_rx_fifo(uart_p);

	ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
	if (ret < 0)
		goto out;
	send[0] = tmp[0];
	send[1] = tmp[1];
	send[2] = 'w';

	ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
	if (ret < 0)
		goto out;
	send[3] = tmp[0];
	send[4] = tmp[1];

	ret = snprintf(tmp, 3, "%02x", command & 0xff);
	if (ret < 0)
		goto out;
	send[5] = tmp[0];
	send[6] = tmp[1];

	ret = uart_write_data(p, send, 7);
	if (ret != 7)
		goto out;

	ret = 0;

	/* the sleep command cannot be acked before the device goes to sleep */
	if (command == DBMD2_VA_SET_POWER_STATE_SLEEP)
		goto out;

	if (!response)
		goto out;

	ret = uart_read_data(p, recv, 5);
	if (ret < 0)
		goto out;
	ret = kstrtou16(recv, 16, response);
	if (ret < 0) {
		dev_err(uart_p->dev, "%s: %2.2x:%2.2x:%2.2x:%2.2x\n",
			__func__, recv[0], recv[1], recv[2], recv[3]);
		goto out;
	}
	ret = 0;
out:
	return ret;
}

static ssize_t send_uart_cmd_va(struct dbmd2_private *p, u32 command,
				   u16 *response)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	char tmp[3];
	u8 send[7];
	u8 recv[6] = {0, 0, 0, 0, 0, 0};
	int ret;

	dev_dbg(uart_p->dev, "%s: Send 0x%02x\n", __func__, command);
	if (response) {
		uart_flush_rx_fifo(uart_p);

		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		send[0] = tmp[0];
		send[1] = tmp[1];
		send[2] = 'r';

		ret = uart_write_data(p, send, 3);
		if (ret != 3)
			goto out;

		ret = 0;

		/* the sleep command cannot be ack'ed before the device goes
		 * to sleep */
		if (command == DBMD2_VA_SET_POWER_STATE_SLEEP)
			goto out;

		if (!response)
			goto out;

		ret = uart_read_data(p, recv, 5);
		if (ret < 0)
			goto out;
		ret = kstrtou16(recv, 16, response);
		if (ret < 0) {
			dev_err(uart_p->dev, "%s: %2.2x:%2.2x:%2.2x:%2.2x\n",
				__func__, recv[0], recv[1], recv[2], recv[3]);
			goto out;
		}

		dev_dbg(uart_p->dev,
				"%s: Received 0x%02x\n", __func__, *response);

		ret = 0;
	} else {
		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		if (ret < 0)
			goto out;
		send[0] = tmp[0];
		send[1] = tmp[1];
		send[2] = 'w';

		ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
		if (ret < 0)
			goto out;
		send[3] = tmp[0];
		send[4] = tmp[1];

		ret = snprintf(tmp, 3, "%02x", command & 0xff);
		if (ret < 0)
			goto out;
		send[5] = tmp[0];
		send[6] = tmp[1];

		ret = uart_write_data(p, send, 7);
		if (ret != 7)
			goto out;
		ret = 0;

	}
out:
	return ret;
}

static int send_uart_cmd_boot(struct dbmd2_private *p, const void *buf,
			      size_t len)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);
	uart_flush_rx_fifo(uart_p);
	ret = uart_write_data(p, buf, len);

	return (ret != len ? -EIO : 0);
}

static int uart_can_boot(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	unsigned long remaining_time;
	int retries = RETRY_COUNT;
	int ret = -EBUSY;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	/*
	 * do additional waiting until UART device is really
	 * available
	 */
	do {
		remaining_time =
			wait_for_completion_timeout(&uart_p->uart_done, HZ);
	} while (!remaining_time && retries--);

	if (uart_p->uart_probe_thread) {
		atomic_inc(&uart_p->stop_uart_probing);
		kthread_stop(uart_p->uart_probe_thread);
		uart_p->uart_probe_thread = NULL;
	}

	INIT_COMPLETION(uart_p->uart_done);

	if (retries == 0) {
		dev_err(p->dev, "%s: UART not available\n", __func__);
		goto out;
	}
	ret = 0;

out:
	return ret;
}

static int uart_prepare_boot(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);
	uart_p->boot_baud_rate =  uart_boot_rate_by_clk(p, p->clk_type);
	dev_dbg(uart_p->dev, "%s: lookup TTY band rate  = %d\n",
		__func__, uart_p->boot_baud_rate);

	/* Send init sequence for up to 100ms at 115200baud.
	* 1 start bit, 8 data bits, 1 parity bit, 2 stop bits = 12 bits
	* FIXME: make sure it is multiple of 8 */
	uart_p->boot_lock_buffer_size = ((uart_p->boot_baud_rate / 12) *
			UART_SYNC_LENGTH) / 1000;

	return 0;
}

static int uart_wait_for_ok(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	u8 resp[5] = {0, 0, 0, 0, 0};
	const char match[] = "OK\n\r";
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	ret = uart_read_data(p, resp, 3);
	if (ret < 0) {
		dev_err(uart_p->dev, "%s: failed to read OK from uart: %d\n",
			__func__, ret);
		goto out;
	}
	ret = strncmp(match , resp, 2);
	if (ret)
		dev_err(uart_p->dev,
			"%s: result = %d : %2.2x:%2.2x:%2.2x\n",
			__func__, ret, resp[0], resp[1], resp[2]);
	if (ret)
		ret = strncmp(match + 1, resp, 2);
	if (ret)
		ret = strncmp(match, resp + 1, 2);
out:
	return ret;
}

static int uart_sync(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
			(struct dbmd2_uart_private *)p->chip->pdata;
	int rc;
	char *buf;
	int i;

	size_t size = uart_p->boot_lock_buffer_size;

	dev_info(p->dev, "%s: start boot sync\n", __func__);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(p->dev,
				"%s: failure: no memory for sync buffer\n",
				__func__);
		return -ENOMEM;
	}

	for (i = 0; i < size; i += 8) {
		buf[i]   = 0x00;
		buf[i+1] = 0x00;
		buf[i+2] = 0x00;
		buf[i+3] = 0x00;
		buf[i+4] = 0x00;
		buf[i+5] = 0x00;
		buf[i+6] = 0x41;
		buf[i+7] = 0x63;
	}

	rc = uart_write_data_no_sync(p, (void *)buf, size);
	if (rc != size)
		dev_err(uart_p->dev, "%s: sync buffer not sent correctly\n",
			__func__);

	/* release chip from reset */
	if (p->clk_get_rate(p, DBMD2_CLK_MASTER) > 32768)
		p->reset_release(p);

	kfree(buf);
	/* check if synchronization succeeded */
	usleep_range(300, 400);
	rc = uart_wait_for_ok(p);
	if (rc != 0) {
		dev_err(p->dev, "%s: boot fail: no sync found err = %d\n",
				__func__, rc);
		return  -EAGAIN;
	}

	uart_flush_rx_fifo(uart_p);

	dev_dbg(p->dev, "%s: boot sync successfully\n", __func__);

	return rc;
}



static int uart_reset(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);


	/* set baudrate to BOOT baud */
	ret = uart_configure_tty(uart_p,
				 uart_p->boot_baud_rate,
				 UART_TTY_BOOT_STOP_BITS,
				 1, 0);
	if (ret) {
		dev_err(p->dev, "%s: cannot configure tty to: %us%up%uf%u\n",
			__func__,
			uart_p->boot_baud_rate,
			UART_TTY_BOOT_STOP_BITS,
			1,
			0);
		return -1;
	}

	uart_flush_rx_fifo(uart_p);

	usleep_range(10000, 20000);

	dev_dbg(p->dev, "%s: start boot sync\n", __func__);

	/* put chip in reset */
	p->reset_set(p);

	usleep_range(1000, 1100);
	/* delay before sending commands */

	if (p->clk_get_rate(p, DBMD2_CLK_MASTER) <= 32768) {
		p->reset_release(p);
		msleep(275);
	} else
		usleep_range(15000, 20000);

	/* check if firmware sync is ok */
	ret = uart_sync(p);
	if (ret != 0) {
		dev_err(uart_p->dev, "%s: sync failed, no OK from firmware\n",
			__func__);
		return  -EAGAIN;
	}

	dev_dbg(p->dev, "%s: boot sync successful\n", __func__);

	uart_flush_rx_fifo(uart_p);

	return 0;
}


static int uart_boot_rate_by_clk(struct dbmd2_private *p, enum xtal_id clk_id)
{
	int ret = BOOT_TTY_BAUD_115200;
	int j;

	for (j = 0; j < ARRAY_SIZE(sbl_map); j++)	{
		if (sbl_map[j].id == clk_id)
			return sbl_map[j].boot_tty_rate;
	}

	dev_warn(p->dev,
			"%s: can't match rate for clk:%d. falling back to dflt\n",
			__func__, clk_id);

	return ret;
}

static int uart_sbl_search(struct dbmd2_private *p, enum xtal_id clk_id)
{
	int ret = -1;
	int j;

	for (j = 0; j < ARRAY_SIZE(sbl_map); j++) {
		if (sbl_map[j].id == clk_id) {
			dev_dbg(p->dev, "%s: found sbl type %d size %d",
				__func__,
				sbl_map[j].id, sbl_map[j].img_len);
			p->sbl_data = sbl_map[j].img_data;
			return  sbl_map[j].img_len;
		}
	}
	return ret;
}

static int uart_load_firmware(const struct firmware *fw,
	struct dbmd2_private *p, const void *checksum,
	size_t chksum_len)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret;
	int sbl_len = 0;
	u8 rx_checksum[6];

	/* serach proper sbl image */
	sbl_len = uart_sbl_search(p,  p->clk_type);
	if (0 > sbl_len) {
		dev_err(p->dev, "%s: ---------> can not find proper sbl img\n",
			__func__);
		return -1;
	}

	/* send SBL */
	ret = uart_write_data(p, (void *)p->sbl_data, sbl_len);
	if (ret != sbl_len) {
		dev_err(p->dev, "%s: ---------> load sbl error\n", __func__);
		return -1;
	}

	/* check if SBL is ok */
	ret = uart_wait_for_ok(p);
	if (ret != 0) {
		dev_err(p->dev,
			"%s: sbl does not respond with ok\n", __func__);
		return -1;
	}

	/* set baudrate to FW upload speed */
	ret = uart_set_speed_host_only(p, DBMD2_VA_SPEED_MAX);

	if (ret) {
		dev_err(p->dev, "%s: failed to send change speed command\n",
			__func__);
		return -1;
	}

	/* send CRC clear command */
	ret = send_uart_cmd_boot(p, clr_crc, sizeof(clr_crc));
	if (ret) {
		dev_err(p->dev, "%s: failed to clear CRC\n",
			 __func__);
		return -1;
	}

	/* send firmware */
	ret = uart_write_data(p, fw->data, fw->size - 4);
	if (ret != (fw->size - 4)) {
		dev_err(p->dev, "%s: -----------> load firmware error\n",
			__func__);
		return -1;
	}

	/* verify checksum */
	if (checksum) {
		msleep(50);
		uart_flush_rx_fifo(uart_p);
		ret = uart_write_data(p, read_checksum, 2);
		if (ret != 2) {
			dev_err(p->dev,
				"%s: failed to request checksum\n",
				__func__);
			return -1;
		}

		ret = uart_read_data(p, rx_checksum, 6);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to read checksum\n",
				__func__);
			return -1;
		}

		ret = p->verify_checksum(
				p, checksum, &rx_checksum[2], 4);
		if (ret) {
			dev_err(p->dev, "%s: checksum mismatch\n", __func__);
			return -1;
		}
	}

	dev_info(p->dev, "%s: ---------> firmware loaded\n", __func__);

	return 0;
}

static int uart_boot(const struct firmware *fw, struct dbmd2_private *p,
		     const void *checksum, size_t chksum_len, int load_fw)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int reset_retry = RETRY_COUNT;
	int fw_load_retry = RETRY_COUNT;
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	do {
		reset_retry = RETRY_COUNT;
		do {
			ret = uart_reset(p);
			if (ret == 0)
				break;
		} while (--reset_retry);

		/* Unable to reset device */
		if (reset_retry <= 0) {
			dev_err(p->dev,
				"%s, reset device err\n", __func__);
			return -ENODEV;
		}

		/* stop here if firmware does not need to be reloaded */
		if (load_fw) {

			ret = uart_load_firmware(fw, p, checksum, chksum_len);

			if (ret != 0) {
				dev_err(p->dev, "%s: failed to load firwmare\n",
					__func__);
				continue;
			}
		}

		ret = send_uart_cmd_boot(p, boot_cmd, sizeof(boot_cmd));
		if (ret) {
			dev_err(p->dev, "%s: booting the firmware failed\n",
				__func__);
			continue;
		}

		usleep_range(10000, 11000);

		ret = uart_set_speed_host_only(p, DBMD2_VA_SPEED_BUFFERING);

		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			continue;
		}

		ret = uart_wait_till_alive(p);

		if (!ret) {
			dev_err(p->dev,
				"%s: device not responding\n", __func__);
			continue;
		}

		/* everything went well */
		break;
	} while (--fw_load_retry);

	if (!fw_load_retry) {
		dev_err(p->dev, "%s: exceeded max attepmts to load fw\n",
				__func__);
		return -1;
	}

	return 0;
}

static int uart_finish_boot(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static int uart_dump_state(struct dbmd2_private *p, char *buf)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int off = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	off += sprintf(buf + off, "\t===UART Interface  Dump====\n");

	off += sprintf(buf + off, "\tUart Interface:\t%s\n",
			uart_p->uart_open ? "Open" : "Closed");
	return off;
}

static int uart_set_va_firmware_ready(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {
		/* set baudrate to NORMAL baud */
		ret = uart_set_speed(p, DBMD2_VA_SPEED_NORMAL);

		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			goto out;
		}
	}
out:
	return ret;
}

static int uart_set_vqe_firmware_ready(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static void uart_transport_enable(struct dbmd2_private *p, bool enable)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret;
	u32 uart_baud;

	dev_dbg(uart_p->dev, "%s (%s)\n", __func__, enable ? "ON" : "OFF");
	if (enable) {

		p->wakeup_set(p);

		if (uart_p->uart_open)
			return;
		ret = uart_open_file_noprobe(uart_p);
		if (ret < 0) {
			dev_err(uart_p->dev, "%s: failed to enable UART: %d\n",
			__func__, ret);
			return;
		}
		if (p->pdata->uart_low_speed_enabled)
			uart_baud = p->pdata->va_speed_cfg[0].uart_baud;
		else
			uart_baud = p->pdata->va_speed_cfg[1].uart_baud;

		ret = uart_configure_tty(uart_p,
					uart_baud,
					UART_TTY_NORMAL_STOP_BITS,
					0, 0);
		if (ret) {
			dev_err(uart_p->dev,
				"%s: cannot configure tty to: %us%up%uf%u\n",
				__func__,
				uart_baud,
				UART_TTY_NORMAL_STOP_BITS, 0, 0);
			return;
		}
	} else {

		p->wakeup_release(p);

		if (!uart_p->uart_open)
			return;
		uart_close_file(uart_p);
	}

	return;
}


static int uart_wait_till_alive(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;

	int ret = 0;
	u16 response;
	unsigned long stimeout = jiffies + msecs_to_jiffies(1000);

	msleep(100);

	uart_flush_rx_fifo(uart_p);

	/* Poll to wait for firmware completing its wakeup procedure:
	 * Read the firmware ID number (0xdbd2) */
	do {
		/* check if chip is alive */
		ret = send_uart_cmd_va(p, DBMD2_VA_FW_ID, &response);
		if (ret)
			continue;

		if (response == 0xdbd2)
			ret = 0;
		else
			ret = -1;
	} while (time_before(jiffies, stimeout) && ret != 0);

	if (ret != 0)
		dev_err(p->dev, "%s: failed to read firmware id\n", __func__);
	ret = (ret >= 0 ? 1 : 0);

	if (!ret)
		dev_err(p->dev, "%s(): failed = 0x%d\n", __func__,
			ret);

	return ret;
}

/* This function sets the uart speed and also can set the software flow
 * control according to the define */
static int uart_set_speed_host_only(struct dbmd2_private *p, int index)
{
	struct dbmd2_uart_private *uart_p =
			(struct dbmd2_uart_private *)p->chip->pdata;
	int ret;

	ret = uart_configure_tty(uart_p,
			p->pdata->va_speed_cfg[index].uart_baud,
			UART_TTY_NORMAL_STOP_BITS,
			0, 0);
	if (ret) {
		dev_err(p->dev, "%s: cannot configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			UART_TTY_NORMAL_STOP_BITS,
			0,
			0);
		goto out;
	}

	dev_info(p->dev, "%s: Configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			UART_TTY_NORMAL_STOP_BITS,
			0,
			0);

	uart_flush_rx_fifo(uart_p);
out:
	return ret;
}

/* this set the uart speed no flow control  */

static int uart_set_speed(struct dbmd2_private *p, int index)
{
	struct dbmd2_uart_private *uart_p =
			(struct dbmd2_uart_private *)p->chip->pdata;
	int ret;

	ret = p->va_set_speed(p, index);
	if (ret) {
		dev_err(p->dev, "%s: failed to send change speed command\n",
			__func__);
		goto out;
	}

	usleep_range(10000, 11000);

	/* set baudrate to FW baud (common case) */
	ret = uart_configure_tty(uart_p,
			p->pdata->va_speed_cfg[index].uart_baud,
			UART_TTY_NORMAL_STOP_BITS,
			0, 0);
	if (ret) {
		dev_err(p->dev, "%s: cannot configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			UART_TTY_NORMAL_STOP_BITS,
			0,
			0);
		goto out;
	}

	dev_info(p->dev, "%s: Configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			UART_TTY_NORMAL_STOP_BITS,
			0,
			0);

	uart_flush_rx_fifo(uart_p);

	ret = uart_wait_till_alive(p);

	if (!ret) {
		dev_err(p->dev, "%s: device not responding\n", __func__);
		goto out;
	}
	ret = 0;
	goto out;

out:
	return ret;
}

static int uart_prepare_buffering(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = uart_set_speed(p, DBMD2_VA_SPEED_BUFFERING);

		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			goto out;
		}
	}
out:
	return ret;
}

static int uart_read_audio_data(struct dbmd2_private *p, size_t samples)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret;
	size_t bytes_to_read = samples * 8 * p->bytes_per_sample;
	unsigned long timeout = jiffies +
				msecs_to_jiffies(MAX_MSECS_IN_FW_BUFFER + 100);
	size_t count;
	ssize_t rc;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	ret = send_uart_cmd_va(p,
			    DBMD2_VA_READ_AUDIO_BUFFER | samples,
			    NULL);
	if (ret) {
		dev_err(p->dev, "%s: failed to request %zu audio samples\n",
			__func__, samples);
		ret = 0;
		goto out;
	}

	do {
		if (bytes_to_read > p->rsize)
			count = p->rsize;
		else
			count = bytes_to_read;

		rc = uart_read_data(p, uart_p->tbuf, count);
		if (rc > 0) {
			bytes_to_read -= rc;
			kfifo_in(&p->pcm_kfifo, uart_p->tbuf, rc);
		}
	} while (time_before(jiffies, timeout) && bytes_to_read);

	if (bytes_to_read) {
		dev_err(p->dev,
			"%s: read data timed out, still %zu bytes to read\n",
			__func__, bytes_to_read);
		ret = 0;
		goto out;
	}

	ret = samples;
out:
	/* restore old fs context */
	set_fs(oldfs);
	return ret;
}

static int uart_finish_buffering(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = uart_set_speed(p, DBMD2_VA_SPEED_NORMAL);
		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			goto out;
		}
	}
out:
	return ret;
}

static int uart_prepare_amodel_loading(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = uart_set_speed(p, DBMD2_VA_SPEED_BUFFERING);

		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			goto out;
		}
	}

out:
	return ret;
}

static int uart_load_amodel(struct dbmd2_private *p,  const void *data,
			    size_t size, const void *checksum,
			    size_t chksum_len, int load)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;
	int retry = RETRY_COUNT;
	int ret;
	ssize_t send_bytes;
	u8 rx_checksum[6];

	dev_dbg(uart_p->dev, "%s\n", __func__);

	while (retry--) {
		if (load)
			ret = send_uart_cmd_va(p,
				DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL,
				NULL);
		else
			ret = send_uart_cmd_va(p,
				DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL | 0x1,
				NULL);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set firmware to recieve new acoustic model\n",
				__func__);
			continue;
		}

		if (!load)
			return 0;

		dev_info(p->dev,
			"%s: ---------> acoustic model download start\n",
			__func__);

		send_bytes = uart_write_data(p, data, size);
		if (send_bytes != size) {
			dev_err(p->dev,
				"%s: sending of acoustic model data failed\n",
				__func__);
			continue;
		}

		/* verify checksum */
		if (checksum) {
			ret = send_uart_cmd_boot(p, read_checksum,
						 sizeof(read_checksum));
			if (ret < 0) {
				dev_err(uart_p->dev,
					"%s: could not read checksum\n",
					__func__);
				continue;
			}

			ret = uart_read_data(p, rx_checksum, 6);
			if (ret < 0) {
				dev_err(uart_p->dev,
					"%s: could not read checksum data\n",
					__func__);
				continue;
			}

			ret = p->verify_checksum(p, checksum, &rx_checksum[2],
						 4);
			if (ret) {
				dev_err(p->dev, "%s: checksum mismatch\n",
					__func__);
				continue;
			}
		}
		break;
	}

	/* no retries left, failed to load acoustic */
	if (retry < 0) {
		dev_err(p->dev, "%s: failed to load acoustic model\n",
			__func__);
		return -1;
	}

	/* send boot command */
	ret = send_uart_cmd_boot(p, boot_cmd, sizeof(boot_cmd));
	if (ret < 0) {
		dev_err(p->dev, "%s: booting the firmware failed\n",
			__func__);
		return -1;
	}

	usleep_range(10000, 11000);

	return 0;
}

static int uart_finish_amodel_loading(struct dbmd2_private *p)
{
	struct dbmd2_uart_private *uart_p =
				(struct dbmd2_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	/* do the same as for finishing buffering */
	return uart_finish_buffering(p);
}

static int uart_open_thread(void *data)
{
	int ret;
	struct dbmd2_uart_private *p = (struct dbmd2_uart_private *)data;

	ret = uart_open_file(p);
	while (!kthread_should_stop())
		usleep_range(10000, 11000);
	return ret;
}

static int dbmd2_uart_probe(struct platform_device *pdev)
{
	int ret;
	struct dbmd2_uart_private *p;
	struct dbd2_uart_data *pdata;
#ifdef CONFIG_OF
	struct device_node *np;
#endif

	dev_dbg(&pdev->dev, "%s\n", __func__);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	p->pdev = pdev;
	p->dev = &pdev->dev;

	p->chip.pdata = p;

#ifdef CONFIG_OF
	np = p->dev->of_node;
	if (!np) {
		dev_err(p->dev, "%s: no devicetree entry\n", __func__);
		ret = -EINVAL;
		goto out_err_kfree;
	}

	pdata = kzalloc(sizeof(struct dbd2_uart_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: Failed to allocate memory\n",
			__func__);
		ret = -ENOMEM;
		goto out_err_kfree;
	}

	ret = of_property_read_string(np, "uart_device", &pdata->uart_dev);
	if (ret && ret != -EINVAL) {
		dev_err(p->dev, "%s: invalid 'uart_device'\n", __func__);
		ret = -EINVAL;
		goto out_err_kfree;
	}

	/* check for software flow control option */
	if (of_find_property(np, "software-flow-control", NULL)) {
		dev_info(p->dev, "%s: Software flow control enabled\n",
			__func__);
		pdata->software_flow_control = 1;
	} else
		dev_info(p->dev, "%s: Software flow control disabled\n",
			__func__);
#else
	pdata = dev_get_platdata(&pdev->dev);
#endif
	p->pdata = pdata;

	init_completion(&p->uart_done);
	atomic_set(&p->stop_uart_probing, 0);

	/* fill in chip interface functions */
	p->chip.can_boot = uart_can_boot;
	p->chip.prepare_boot = uart_prepare_boot;
	p->chip.boot = uart_boot;
	p->chip.finish_boot = uart_finish_boot;
	p->chip.dump = uart_dump_state;
	p->chip.set_va_firmware_ready = uart_set_va_firmware_ready;
	p->chip.set_vqe_firmware_ready = uart_set_vqe_firmware_ready;
	p->chip.transport_enable = uart_transport_enable;
	p->chip.read = uart_read_data;
	p->chip.write = uart_write_data;
	p->chip.send_cmd_vqe = send_uart_cmd_vqe;
	p->chip.send_cmd_va = send_uart_cmd_va;
	p->chip.prepare_buffering = uart_prepare_buffering;
	p->chip.read_audio_data = uart_read_audio_data;
	p->chip.finish_buffering = uart_finish_buffering;
	p->chip.prepare_amodel_loading = uart_prepare_amodel_loading;
	p->chip.load_amodel = uart_load_amodel;
	p->chip.finish_amodel_loading = uart_finish_amodel_loading;

	dev_set_drvdata(p->dev, &p->chip);

	p->uart_probe_thread = kthread_run(uart_open_thread,
					   (void *)p,
					   "dbmd2 probe thread");
	if (IS_ERR_OR_NULL(p->uart_probe_thread)) {
		dev_err(p->dev,
			"%s(): can't create dbmd2 uart probe thread = %p\n",
			__func__, p->uart_probe_thread);
		ret = -ENOMEM;
		goto out_err_kfree;
	}

	dev_info(p->dev, "%s: successfully probed\n", __func__);

	ret = 0;
	goto out;

out_err_kfree:
	kfree(p);
out:
	return ret;
}

static int dbmd2_uart_remove(struct platform_device *pdev)
{
	struct chip_interface *ci = dev_get_drvdata(&pdev->dev);
	struct dbmd2_uart_private *p = (struct dbmd2_uart_private *)ci->pdata;

	dev_set_drvdata(p->dev, NULL);

	uart_close_file(p);
	kfree(p);

	return 0;
}

static const struct of_device_id dbmd2_uart_of_match[] = {
	{ .compatible = "dspg,dbmd2-uart", },
	{},
};
MODULE_DEVICE_TABLE(of, dbmd2_uart_of_match);

static struct platform_driver dbmd2_uart_platform_driver = {
	.driver = {
		.name = "dbmd2-uart",
		.owner = THIS_MODULE,
		.of_match_table = dbmd2_uart_of_match,
	},
	.probe =  dbmd2_uart_probe,
	.remove = dbmd2_uart_remove,
};

static int __init dbmd2_modinit(void)
{
	return platform_driver_register(&dbmd2_uart_platform_driver);
}
module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	platform_driver_unregister(&dbmd2_uart_platform_driver);
}
module_exit(dbmd2_exit);

MODULE_DESCRIPTION("DSPG DBMD2 UART interface driver");
MODULE_LICENSE("GPL");
