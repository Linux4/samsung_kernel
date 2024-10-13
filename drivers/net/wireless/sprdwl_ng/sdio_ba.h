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

#ifndef __SPRDWL_SDIO_BA_H__
#define __SPRDWL_SDIO_BA_H__

#include <linux/types.h>
#include <linux/wakelock.h>

struct sdio_chn {
	unsigned char chn[16];
	unsigned char num;
	unsigned short bit_map;
	/* sdio_chn lock */
	spinlock_t lock;
	int gpio_high;
	int chn_timeout_cnt;
	unsigned long timeout;
	unsigned long timeout_time;
	bool timeout_flag;
};

struct hw_info {
	int wakeup;
	int can_sleep;
	unsigned int rx_record;
	struct sdio_chn sdio_tx_chn;
	struct sdio_chn sdio_rx_chn;
	u32 rx_gpio;
};

struct sprdwl_tx_buf {
	unsigned char *base;
	unsigned short buf_len;
	unsigned short curpos;
};

struct sprdwl_rx_buf {
	unsigned char *base;
	unsigned short buf_len;
	unsigned short data_len;
};

struct sprdwl_priv;
struct sprdwl_sdio {
	struct platform_device *pdev;
	/* priv use void *, after MCC adn priv->flags,
	 * and change txrx intf pass priv to void later
	 */
	struct sprdwl_priv *priv;
	/* if nedd more flags which not only exit, fix it*/
	/* unsigned int exit:1; */
	int exit;

	struct wake_lock rx_wakelock;
	struct wake_lock tx_wakelock;

	/* tx work */
	struct sprdwl_msg_list tx_list;
	struct sprdwl_tx_buf txbuf;
	struct work_struct tx_work;
	struct workqueue_struct *tx_queue;

	struct sprdwl_msg_list rx_list;
	struct sprdwl_rx_buf rxbuf;
	struct work_struct rx_work;
	struct workqueue_struct *rx_queue;
	struct sdio_chn chn;
	struct hw_info hw;
};

bool get_sdiohal_status(void);
int sdio_chn_status(unsigned short chn, unsigned short *status);
int sdio_dev_read(unsigned int chn, void *read_buf, unsigned int *count);
int sdio_dev_write(unsigned int chn, void *data_buf, unsigned int count);
void invalid_recv_flush(unsigned int chn);
int sdiodev_readchn_init(int chn, void *callback, bool with_para);
int set_marlin_wakeup(unsigned int chn, unsigned int user_id);
int sdiodev_readchn_uninit(unsigned int chn);
void mdbg_at_cmd_read(void);
void mdbg_loopcheck_read(void);
void mdbg_sdio_read(void);
#endif
