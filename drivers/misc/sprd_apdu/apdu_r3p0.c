// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Unisoc, Inc.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include <net/sock.h>
#include "apdu_r3p0.h"
#include "qogirn6pro_ise_pd.h"

static struct sock *sprd_apdu_nlsk;

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-apdu: " fmt

static void sprd_apdu_dump_data(u32 *buf, u32 len)
{
	int i;

	for (i = 0; i < len; i++)
		pr_info("0x%8x ", buf[i]);
	pr_info("\n");
}

static void sprd_apdu_dump_dbg_data(u32 *buf, u32 len)
{
	int i;

	for (i = 0; i < len; i++)
		pr_debug("0x%8x ", buf[i]);
	pr_debug("\n");
}

static void sprd_apdu_int_en(void __iomem *base, u32 int_en)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INT_EN);
	all_int |= (int_en & APDU_INT_BITS);
	writel_relaxed(all_int, base + APDU_INT_EN);
}

static void sprd_apdu_int_dis(void __iomem *base, u32 int_dis)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INT_EN);
	all_int &= ~(int_dis & APDU_INT_BITS);
	writel_relaxed(all_int, base + APDU_INT_EN);
}

static u32 sprd_apdu_int_en_status(void __iomem *base)
{
	return readl_relaxed(base + APDU_INT_EN);
}

static u32 sprd_apdu_int_raw_status(void __iomem *base)
{
	return readl_relaxed(base + APDU_INT_RAW);
}

static u32 sprd_apdu_int_mask_status(void __iomem *base)
{
	return readl_relaxed(base + APDU_INT_MASK);
}

static void sprd_apdu_clear_int(void __iomem *base, u32 clear_int)
{
	writel_relaxed(clear_int & APDU_INT_BITS, base + APDU_INT_CLR);
}

static void sprd_apdu_inf_int_en(void __iomem *base, u32 int_en)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INF_INT_EN);
	all_int |= int_en;
	writel_relaxed(all_int, base + APDU_INF_INT_EN);
}

static void sprd_apdu_inf_int_dis(void __iomem *base, u32 int_dis)
{
	u32 all_int;

	all_int = readl_relaxed(base + APDU_INF_INT_EN);
	all_int &= ~int_dis;
	writel_relaxed(all_int, base + APDU_INF_INT_EN);
}

static u32 sprd_apdu_inf_int_en_status(void __iomem *base)
{
	return readl_relaxed(base + APDU_INF_INT_EN);
}

static u32 sprd_apdu_inf_int_raw_status(void __iomem *base)
{
	return readl_relaxed(base + APDU_INF_INT_RAW);
}

static u32 sprd_apdu_inf_int_mask_status(void __iomem *base)
{
	return readl_relaxed(base + APDU_INF_INT_MASK);
}

static void sprd_apdu_clear_inf_int(void __iomem *base, u32 clear_int)
{
	writel_relaxed(clear_int, base + APDU_INF_INT_CLR);
}

static void sprd_apdu_clear_cnt(void __iomem *base)
{
	writel_relaxed((BIT(0) | BIT(1)), base + APDU_CNT_CLR);
}

static void sprd_apdu_rst(void __iomem *base)
{
	/* missing reset apdu */
	sprd_apdu_clear_int(base, APDU_INT_BITS);
	sprd_apdu_clear_cnt(base);
	sprd_apdu_clear_inf_int(base, APDU_INF_INT_BITS);
}

static void sprd_apdu_clear_fifo(void __iomem *base)
{
	/* send clear fifo req to ISE, and then ISE will clear fifo */
	writel_relaxed(BIT(4), base + APDU_CNT_CLR);
}

static void sprd_apdu_read_rx_fifo(void __iomem *base,
				   u32 *data_ptr, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++) {
		data_ptr[i] = readl_relaxed(base + APDU_RX_FIFO);
		pr_debug("r_data[%d]:0x%08x ", i, data_ptr[i]);
	}
}

static void sprd_apdu_write_tx_fifo(void __iomem *base,
				    u32 *data_ptr, u32 len)
{
	u32 i;

	for (i = 0; i < len; i++) {
		writel_relaxed(data_ptr[i], base + APDU_TX_FIFO);
		pr_debug("w_data[%d]:0x%08x ", i, data_ptr[i]);
	}
}

static u32 sprd_apdu_get_rx_fifo_len(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status =
		(fifo_status >> APDU_FIFO_RX_OFFSET) & APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_rx_fifo_point(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status =
		(fifo_status >> APDU_FIFO_RX_POINT_OFFSET) & APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_rx_fifo_cnt(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS1);
	fifo_status =
		(fifo_status >> APDU_FIFO_RX_OFFSET) & APDU_CNT_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_tx_fifo_len(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status &= APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_tx_fifo_point(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS0);
	fifo_status =
		(fifo_status >> APDU_FIFO_TX_POINT_OFFSET) & APDU_FIFO_LEN_MASK;

	return fifo_status;
}

static u32 sprd_apdu_get_tx_fifo_cnt(void __iomem *base)
{
	u32 fifo_status;

	fifo_status = readl_relaxed(base + APDU_STATUS1);
	fifo_status = fifo_status & APDU_CNT_LEN_MASK;

	return fifo_status;
}

static void sprd_apdu_set_watermark(void __iomem *base, u8 watermark)
{
	writel_relaxed(watermark & GENMASK(7, 0), base + APDU_WATER_MARK);
}

static u8 sprd_apdu_get_watermark(void __iomem *base)
{
	return (readl_relaxed(base + APDU_WATER_MARK) & GENMASK(7, 0));
}

static long sprd_apdu_check_clr_fifo_done(struct sprd_apdu_device *apdu)
{
	u32 ret;

	ret = sprd_apdu_get_tx_fifo_len(apdu->base);
	ret |= sprd_apdu_get_tx_fifo_point(apdu->base);
	ret |= sprd_apdu_get_tx_fifo_cnt(apdu->base);
	ret |= sprd_apdu_get_rx_fifo_len(apdu->base);
	ret |= sprd_apdu_get_rx_fifo_point(apdu->base);
	ret |= sprd_apdu_get_rx_fifo_cnt(apdu->base);
	if (ret != 0)
		/* fifo not cleard */
		return 1;

	return 0;
}

static int sprd_apdu_enable_check(struct sprd_apdu_device *apdu)
{
	u32 int_en, inf_int_en;

	int_en = sprd_apdu_int_en_status(apdu->base);
	inf_int_en = sprd_apdu_inf_int_en_status(apdu->base);
	if ((int_en == APDU_INT_DEFAULT_EN) &&
	    (inf_int_en == APDU_INF_INT_DEFAULT_EN))
		return 1;

	return 0;
}

static void sprd_apdu_enable(struct sprd_apdu_device *apdu)
{
	/*
	 * pmu reg:REG_PMU_APB_PD_ISE_CFG_0
	 * should be pre-configured to power up ISE.
	 */

	sprd_apdu_clear_int(apdu->base, APDU_INT_BITS);
	sprd_apdu_int_dis(apdu->base, APDU_INT_BITS);
	sprd_apdu_inf_int_dis(apdu->base, APDU_INF_INT_BITS);
	sprd_apdu_int_en(apdu->base, APDU_INT_RX_EMPTY_TO_NOEMPTY);
	sprd_apdu_int_en(apdu->base, APDU_INT_MED_WR_DONE);
	sprd_apdu_int_en(apdu->base, APDU_INT_MED_ERR);

	sprd_apdu_inf_int_en(apdu->base, APDU_INF_INT_GET_ATR);
	sprd_apdu_inf_int_en(apdu->base, APDU_INF_INT_FAULT);
}

long sprd_apdu_power_on_check(struct sprd_apdu_device *apdu, u32 times)
{
	u8 status;
	long ret;

	/* It takes about 500us to power on ISE and apdu module */
	do {
		/* check ISE pd status--apdu access rely on ISE power on */
		ret = apdu->pd_ise->ise_pd_status_check(apdu);
		if (ret == ISE_PD_PWR_DOWN) {
			usleep_range(10, 20);
			continue;
		} else if (ret < 0) {
			goto end;
		}

		sprd_apdu_set_watermark(apdu->base, APDU_TX_THRESHOLD);
		status = sprd_apdu_get_watermark(apdu->base);
		/* write reg succeed, apdu module release complete */
		if (status == APDU_TX_THRESHOLD)
			return 0;
		/* AP has up to 30ms window to shake hands with ISE */
		usleep_range(10, 20);
	} while (--times);

end:
	dev_err(apdu->dev, "apdu module enable failure!\n");
	return -ENXIO;
}

static int sprd_apdu_read_data(struct sprd_apdu_device *apdu,
			       u32 *buf, u32 count)
{
	u32 rx_fifo_status;
	u32 once_read_len, left;
	u32 index = 0, loop = 0;
	u32 need_read = count;

	/* read need_read words */
	while (need_read > 0) {
		do {
			rx_fifo_status = sprd_apdu_get_rx_fifo_len(apdu->base);
			dev_dbg(apdu->dev,
				"rx_fifo_status:0x%x\n", rx_fifo_status);
			if (rx_fifo_status)
				break;

			if (++loop >= APDU_WR_RD_TIMEOUT) {
				dev_err(apdu->dev, "apdu read data timeout!\n");
				return -EBUSY;
			}
			/* read timeout is less than 2~4s */
			usleep_range(10000, 20000);
		} while (1);

		once_read_len = (need_read < rx_fifo_status)
					? need_read : rx_fifo_status;
		sprd_apdu_read_rx_fifo(apdu->base, &buf[index], once_read_len);

		index += once_read_len;
		need_read -= once_read_len;
		if (need_read == 0) {
			left = sprd_apdu_get_rx_fifo_len(apdu->base);
			/* may another remaining packet in RX FIFO */
			if (left)
				dev_err(apdu->dev, "read left len:%u\n", left);
			break;
		}
	}
	return 0;
}

static int sprd_apdu_write_data(struct sprd_apdu_device *apdu,
				void *buf, u32 count)
{
	u32 *data_buffer = (u32 *)buf;
	u32 len, pad_len;
	u32 tx_fifo_free_space;
	u32 data_to_write;
	u32 index = 0, loop = 0, timeout = 0;
	char header[4] = {0x00, 0x00, 0xaa, 0x55};

	if (count > APDU_TX_MAX_SIZE) {
		dev_err(apdu->dev, "write len:%u exceed max:%u!\n",
			count, APDU_TX_MAX_SIZE);
		return -EFBIG;
	}

	pad_len = count % 4;
	if (pad_len) {
		memset(buf + count, 0, 4 - pad_len);
		len = count / 4 + 1;
	} else {
		len = count / 4;
	}

	/* wait for interrupt receive ISE critical data */
	while (apdu->ise_atr.atr_rcv_status ||
	       apdu->med_rewr.med_wr_done) {
		usleep_range(50, 100);
		timeout++;
		if (timeout > APDU_WR_PRE_TIMEOUT_3) {
			dev_err(apdu->dev, "APDU receiving critical data\n");
			return -EBUSY;
		}
	}
	timeout = 0;

	/* clear fifo for further read operation */
	while (sprd_apdu_get_rx_fifo_len(apdu->base) != 0) {
		usleep_range(100, 200);
		timeout++;
		if (timeout == APDU_WR_PRE_TIMEOUT) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev, "such rx fifo data discard\n");
		} else if (timeout > APDU_WR_PRE_TIMEOUT_2) {
			dev_err(apdu->dev, "clear fifo timeout/fail\n");
			break;
		}
	}
	/* apdu workflow is send request first and rcv answer later */
	apdu->rx_done = 0;

	header[0] = count & 0xff;
	header[1] = (count >> 8) & 0xff;
	/* send protocol header */
	sprd_apdu_write_tx_fifo(apdu->base, (u32 *)header, 1);

	/* write len words */
	while (len > 0) {
		do {
			tx_fifo_free_space = APDU_FIFO_LENGTH -
				sprd_apdu_get_tx_fifo_len(apdu->base);
			if (tx_fifo_free_space)
				break;

			if (++loop >= APDU_WR_RD_TIMEOUT) {
				sprd_apdu_clear_fifo(apdu->base);
				dev_err(apdu->dev,
					"wr data timeout!please resend\n");
				return -EBUSY;
			}
			/* write timeout is less than 2~4s */
			usleep_range(10000, 20000);
		} while (1);

		data_to_write = (len < tx_fifo_free_space)
					? len : tx_fifo_free_space;
		sprd_apdu_write_tx_fifo(apdu->base,
					&data_buffer[index], data_to_write);
		index += data_to_write;
		len -= data_to_write;
	}

	return 0;
}

static ssize_t sprd_apdu_read(struct file *fp, char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	ssize_t r = 0;
	u32 xfer, data_len, word_len, wait_time, header = 0;
	unsigned long wait_event_time;
	long ret;
	int ret2;

	mutex_lock(&apdu->mutex);
	ret = sprd_apdu_power_on_check(apdu, 1);
	if (ret < 0) {
		r = -ENXIO;
		dev_err(apdu->dev,
			"ISE has not power on or apdu has not release\n");
		goto end;
	}
	wait_event_time = msecs_to_jiffies(MAX_WAIT_TIME);
	wait_time = AP_WAIT_TIMES;
	do {
		if (!apdu->rx_done) {
			ret = wait_event_interruptible_timeout(apdu->read_wq, apdu->rx_done,
							       wait_event_time);
			if (ret < 0) {
				r = -EIO;
				dev_err(apdu->dev, "ap read error\n");
				goto end;
			} else if (ret == 0 && apdu->rx_done == 0) {
				dev_err(apdu->dev,
					"ap read timeout, busy(%d)\n",
					(AP_WAIT_TIMES - wait_time + 1));
				r = -ETIMEDOUT;
				goto end;
			}
		}

		if (sprd_apdu_get_rx_fifo_len(apdu->base) == 0) {
			r = -ENODATA;
			dev_err(apdu->dev, "%s():rx fifo empty for header.\n",
				__func__);
			goto end;
		}

		/* check apdu packet valid */
		sprd_apdu_read_rx_fifo(apdu->base, &header, 1);
		if (APDU_MAGIC_MASK(header) != APDU_MAGIC_NUM) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev,
				"read data magic error! clr fifo.\n");
			r = -ENODATA;
			goto end;
		}

		data_len = APDU_DATA_LEN_MASK(header);
		if (data_len > APDU_RX_MAX_SIZE) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev, "rd len:%u, max:%u! clr fifo.\n",
				data_len, APDU_RX_MAX_SIZE);
			r = -EFBIG;
			goto end;
		}

		word_len = DIV_CEILING(data_len, 4);
		ret2 = sprd_apdu_read_data(apdu, (u32 *)apdu->rx_buf, word_len);
		if (ret2 < 0) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev,
				"apdu read fifo fail(0x%x)\n", ret2);
			r = -EBUSY;
			goto end;
		}

		if (((*(char *)apdu->rx_buf) == ISE_BUSY_STATUS) &&
		    (data_len == 0x1)) {
			apdu->rx_done = 0;
			dev_dbg(apdu->dev, "ise read busy(%d)\n",
				(AP_WAIT_TIMES - wait_time + 1));
		} else {
			break;
		}
	} while (--wait_time);

	if (((*(char *)apdu->rx_buf) == ISE_BUSY_STATUS) && (data_len == 0x1))
		dev_err(apdu->dev, "ISE busy, exceed AP max wait times\n");

	xfer = (data_len < count) ? data_len : count;
	r = xfer;
	if (copy_to_user(buf, apdu->rx_buf, xfer))
		r = -EFAULT;

end:
	mutex_unlock(&apdu->mutex);
	return r;
}

static ssize_t sprd_apdu_write(struct file *fp, const char __user *buf,
			       size_t count, loff_t *f_pos)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	ssize_t r = 0;
	u32 xfer;
	long ret;
	int ret2;

	mutex_lock(&apdu->mutex);
	ret = sprd_apdu_power_on_check(apdu, 1);
	if (ret < 0) {
		r = -ENXIO;
		dev_err(apdu->dev,
			"ISE has not power on or apdu has not release\n");
		goto end;
	}
	r = count;

	while (count > 0) {
		if (count > APDU_TX_MAX_SIZE) {
			dev_err(apdu->dev, "write len:%zu exceed max:%d!\n",
				count, APDU_TX_MAX_SIZE);
			r = -EFBIG;
			goto end;
		} else {
			xfer = count;
		}
		if (xfer && copy_from_user(apdu->tx_buf, buf, xfer)) {
			dev_err(apdu->dev, "get data fail!\n");
			r = -EFAULT;
			goto end;
		}

		ret2 = sprd_apdu_write_data(apdu, apdu->tx_buf, xfer);
		if (ret2) {
			dev_err(apdu->dev, "ap write fail(0x%x)\n", ret2);
			r = -EBUSY;
			goto end;
		}
		buf += xfer;
		count -= xfer;
	}

end:
	mutex_unlock(&apdu->mutex);
	return r;
}

static void sprd_apdu_get_atr(struct sprd_apdu_device *apdu)
{
	u32 word_len, atr_len, i;
	int ret;

	atr_len = APDU_ATR_DATA_MAX_SIZE + 4;
	memset((void *)apdu->ise_atr.atr, 0x0, atr_len);
	if (sprd_apdu_get_rx_fifo_len(apdu->base) == 0x0) {
		dev_err(apdu->dev, "get atr fail: rx fifo not data.\n");
		return;
	}

	sprd_apdu_read_rx_fifo(apdu->base, apdu->ise_atr.atr, 1);
	atr_len = APDU_DATA_LEN_MASK(*apdu->ise_atr.atr);
	word_len = DIV_CEILING(atr_len, 4);

	if ((APDU_MAGIC_MASK(*apdu->ise_atr.atr) == APDU_MAGIC_NUM) &&
	    (word_len <= (APDU_ATR_DATA_MAX_SIZE / 4))) {
		ret = sprd_apdu_read_data(apdu, &(apdu->ise_atr.atr[1]),
					  word_len);
		if (ret < 0) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev, "get atr error(0x%x)\n", ret);
			return;
		}
		dev_dbg(apdu->dev, "get atr done, len:%u word\n", word_len);
		for (i = 0; i < (word_len + 1); i++)
			dev_dbg(apdu->dev, "0x%8x ", apdu->ise_atr.atr[i]);
	} else {
		sprd_apdu_clear_fifo(apdu->base);
		if (APDU_MAGIC_MASK(*apdu->ise_atr.atr) != APDU_MAGIC_NUM)
			/* may data left in rx fifo before receive atr */
			dev_err(apdu->dev, "not a effective header\n");
		else if (word_len > (APDU_ATR_DATA_MAX_SIZE / 4))
			dev_err(apdu->dev, "atr over size\n");
		else
			dev_err(apdu->dev, "get atr fail!\n");
	}
}

static long sprd_apdu_send_enter_apdu_loop_req(struct sprd_apdu_device *apdu)
{
	int ret;
	char cmd_enter_apdu_loop[4] = {0x00, 0xf5, 0x5a, 0xa5};

	if (!apdu)
		return -EINVAL;

	ret = sprd_apdu_write_data(apdu, (void *)cmd_enter_apdu_loop, 4);
	if (ret < 0)
		dev_err(apdu->dev,
			"enter apdu loop ins:write error(%d)\n", ret);

	dev_dbg(apdu->dev, "apdu send enter apdu loop return %d!\n", ret);
	return ret;
}

static int sprd_apdu_send_usrmsg(char *pbuf, uint16_t len)
{
	struct sk_buff *nl_skb;
	struct nlmsghdr *nlh;
	int ret;

	if (!sprd_apdu_nlsk) {
		pr_err("netlink uninitialized\n");
		return -EFAULT;
	}

	nl_skb = nlmsg_new(len, GFP_ATOMIC);
	if (!nl_skb) {
		pr_err("netlink alloc fail\n");
		return -EFAULT;
	}

	nlh = nlmsg_put(nl_skb, 0, 0, APDU_NETLINK, len, 0);
	if (!nlh) {
		pr_err("nlmsg_put fail\n");
		nlmsg_free(nl_skb);
		return -EFAULT;
	}

	memcpy(nlmsg_data(nlh), pbuf, len);
	ret = netlink_unicast(sprd_apdu_nlsk, nl_skb,
			      APDU_USER_PORT, MSG_DONTWAIT);

	return ret;
}

static void sprd_apdu_netlink_rcv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	char *umsg = NULL;
	char *kmsg;
	char netlink_kmsg[30] = {0};

	if (skb->len >= nlmsg_total_size(0)) {
		kmsg = netlink_kmsg;
		nlh = nlmsg_hdr(skb);
		umsg = NLMSG_DATA(nlh);
		if (umsg)
			sprd_apdu_send_usrmsg(kmsg, strlen(kmsg));
	}
}

static struct netlink_kernel_cfg sprd_apdu_netlink_cfg = {
	.input = sprd_apdu_netlink_rcv_msg,
};

static long med_set_high_addr(struct sprd_apdu_device *apdu,
					u64 med_base_addr)
{
	u64 high_addr;
	u32 base_addr;
	u32 bit_offset = apdu->pub_ise_cfg.pub_ise_bit_offset;
	long ret;

	/* med base addr need aligned to 64MB */
	if (((med_base_addr & MED_LOW_ADDRESSING) != 0) ||
	    (med_base_addr <= DDR_BASE)) {
		dev_err(apdu->dev, "error med base addr\n");
		return -EFAULT;
	}

	high_addr = (med_base_addr - DDR_BASE) & MED_HIGH_ADDRESSING;
	base_addr = (u32)((high_addr >> (26 - bit_offset)) &
			  GENMASK_ULL(bit_offset + 8, bit_offset));

	ret = regmap_update_bits(apdu->pub_ise_cfg.pub_reg_base,
				 apdu->pub_ise_cfg.pub_ise_reg_offset,
				 GENMASK(bit_offset + 8, bit_offset),
				 base_addr);
	if (ret) {
		dev_err(apdu->dev, "regmap write med base addr fail\n");
		return ret;
	}

	return 0;
}

static int med_ise_size_parse(struct sprd_apdu_device *apdu)
{
	if (apdu->pub_ise_cfg.med_size <= MED_DDR_MIN_SIZE) {
		apdu->pub_ise_cfg.med_ise_size = 0;
		dev_err(apdu->dev, "AP DDR reserved too small for ISE\n");
		return -EFAULT;
	} else if (apdu->pub_ise_cfg.med_size >= MED_DDR_MAX_SIZE) {
		apdu->pub_ise_cfg.med_ise_size = MED_ISE_MAX_SIZE;
	} else {
		apdu->pub_ise_cfg.med_ise_size =
			(apdu->pub_ise_cfg.med_size - MED_DDR_MIN_SIZE) / 2;
	}

	dev_dbg(apdu->dev, "ise med size:0x%llx\n",
		apdu->pub_ise_cfg.med_ise_size);
	return 0;
}

static long med_rewrite_info_parse(struct sprd_apdu_device *apdu,
				   struct med_parse_info_t *med_info,
				   u32 ise_side_data_offset,
				   u32 ise_side_data_len)
{
	u32 temp, temp2;
	u64 med_ise_size = apdu->pub_ise_cfg.med_ise_size;

	dev_dbg(apdu->dev, "sprd-apdu:input ise side offset:0x%x, len:0x%x\n",
		 ise_side_data_offset, ise_side_data_len);
	if ((ise_side_data_offset >= (med_ise_size + ISE_MED_BASE_ADDR)) ||
	    (ise_side_data_offset < ISE_MED_BASE_ADDR)) {
		dev_err(apdu->dev, "error input offset:0x%x, limit: 0x%x to 0x%llx\n",
		       ise_side_data_offset, ISE_MED_BASE_ADDR,
		       (ISE_MED_BASE_ADDR + med_ise_size - 1));
		return -EFAULT;
	}
	if ((ise_side_data_offset + ise_side_data_len) >
	    (med_ise_size + ISE_MED_BASE_ADDR) || (ise_side_data_len == 0)) {
		dev_err(apdu->dev, "error input range, offset:0x%x, len: 0x%x\n",
		       ise_side_data_offset, ise_side_data_len);
		return -EFAULT;
	}

	ise_side_data_offset -= ISE_MED_BASE_ADDR;

	/* all 512Byte level1 rng need rewrite to flash*/
	med_info->level1_rng_offset = 0x400;
	med_info->level1_rng_length = 0x200;

	/*
	 * one block data (512KB) range change, rewrite 1024Byte lv2 rng/hash
	 * ap level2 offset default add 0x800
	 */
	temp = ise_side_data_offset / 0x80000;
	med_info->level2_rng_offset = temp * 0x400 + 0x800;
	temp2 = DIV_CEILING(ise_side_data_offset + ise_side_data_len, 0x80000);
	med_info->level2_rng_length = (temp2 - temp) * 0x400;

	/*
	 * one block data (4KB) range change, rewrite 1024Byte lv3 rng/hash
	 * ap level3 offset default add 0xE800
	 */
	temp = ise_side_data_offset / 0x1000;
	med_info->level3_rng_offset = temp * 0x400 + 0xe800;
	temp2 = DIV_CEILING(ise_side_data_offset + ise_side_data_len, 0x1000);
	med_info->level3_rng_length = (temp2 - temp) * 0x400;

	/*
	 * 32B ise data range change, ap rewrite 32B ciphertext  and 32B hash
	 * ap data offset default add 0x800000
	 */
	temp = (ise_side_data_offset & 0xffffffe0);
	med_info->ap_side_data_offset = temp * 2 + 0x800000;
	temp2 = DIV_CEILING(ise_side_data_offset + ise_side_data_len, 0x20);
	temp2 = (temp2 * 0x20) - temp;
	med_info->ap_side_data_length = temp2 * 2;

	return 0;
}

static void med_rewrite_post_process(struct sprd_apdu_device *apdu)
{
	u32 msg_buf[APDU_MED_INFO_PARSE_SZ + 1] = {0};
	struct med_origin_info_t *med_origin;
	struct med_parse_info_t *med_post;
	u32 i, len;
	long ret;

	msg_buf[0] = MESSAGE_HEADER_MED_INFO;
	med_origin = (struct med_origin_info_t *)apdu->med_rewr.med_rewrite;
	med_post = (struct med_parse_info_t *)(&msg_buf[1]);

	for (i = 0; i < MED_INFO_MAX_BLOCK; i++) {
		/* ISE side rewrite offset and len array */
		if (med_origin[i].med_data_offset == 0 &&
		    med_origin[i].med_data_len == 0)
			break;
		ret = med_rewrite_info_parse(apdu, &med_post[i],
					     med_origin[i].med_data_offset,
					     med_origin[i].med_data_len);
		if (ret < 0) {
			dev_err(apdu->dev,
				"%s() med rewrite info prase fail\n", __func__);
			break;
		}
	}
	if (i > 0) {
		len = i * sizeof(struct med_parse_info_t) + 4;
		dev_dbg(apdu->dev, "parsed info processing\n");
		sprd_apdu_dump_dbg_data(msg_buf, len / 4);

		if (sprd_apdu_send_usrmsg((char *)msg_buf, len) < 0)
			dev_err(apdu->dev, "med rewrite info send to user space fail\n");
	} else {
		dev_err(apdu->dev, "med rewrite info unable to parse\n");
	}
}

long med_rewrite_post_process_check(struct sprd_apdu_device *apdu)
{
	u32 timeout = 0;

	/* wait med rewrite info(lv1/lv2/lv3/data) write to flash done */
	while (apdu->med_rewr.med_wr_done ||
	       apdu->med_rewr.med_data_processing) {
		usleep_range(800, 1000);
		timeout++;
		if (timeout > APDU_WR_PRE_TIMEOUT_2) {
			dev_err(apdu->dev, "wait med data proccess timeout\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static void sprd_apdu_get_med_rewrite_info(struct sprd_apdu_device *apdu)
{
	u32 word_len, med_rewrite_len, header = 0;
	int ret;

	memset((void *)apdu->med_rewr.med_rewrite, 0, APDU_MED_INFO_SIZE * 4);
	if (sprd_apdu_get_rx_fifo_len(apdu->base) == 0) {
		dev_err(apdu->dev, "get med info fail:rx fifo not data.\n");
		return;
	}

	sprd_apdu_read_rx_fifo(apdu->base, &header, 1);
	med_rewrite_len = APDU_DATA_LEN_MASK(header);
	word_len = DIV_CEILING(med_rewrite_len, 4);

	if ((APDU_MAGIC_MASK(header) == APDU_MAGIC_NUM) &&
	    (word_len <= APDU_MED_INFO_SIZE)) {
		ret = sprd_apdu_read_data(apdu, apdu->med_rewr.med_rewrite,
					  word_len);
		/* inform sprd_apdu_write_data get med rewrite info done */
		apdu->med_rewr.med_wr_done = 0;
		if (ret < 0) {
			sprd_apdu_clear_fifo(apdu->base);
			dev_err(apdu->dev,
				"get med rewrite info error(0x%x)\n", ret);
			return;
		}

		apdu->med_rewr.med_data_processing = 1;
		dev_dbg(apdu->dev, "get med info done len:%u word\n", word_len);
		sprd_apdu_dump_dbg_data(apdu->med_rewr.med_rewrite, word_len);
		med_rewrite_post_process(apdu);
		apdu->med_rewr.med_data_processing = 0;
	} else {
		sprd_apdu_clear_fifo(apdu->base);
		if (APDU_MAGIC_MASK(header) != APDU_MAGIC_NUM)
			/* may data left before receive med rewrite info */
			dev_err(apdu->dev, "not a effective header\n");
		else if (word_len > APDU_MED_INFO_SIZE)
			dev_err(apdu->dev, "excend med info max buf size!\n");
		else
			dev_err(apdu->dev, "get med info fail!\n");
	}
}

static void sprd_apdu_ise_fault_buffer_init(struct sprd_apdu_device *apdu)
{
	memset(apdu->ise_fault.ise_fault_buf, 0, ISE_ATTACK_BUFFER_SIZE * 4);
	apdu->ise_fault.ise_fault_buf[0] = MESSAGE_HEADER_FAULT;
	apdu->ise_fault.ise_fault_ptr = 1;

	memset(apdu->ise_atr.ise_gp_status, 0, ISE_ATTACK_BUFFER_SIZE * 4);
	apdu->ise_atr.gp_index = 0;
}

static void sprd_apdu_ise_fault_status_caching(struct sprd_apdu_device *apdu,
					       u32 ise_fault_status)
{
	u32 fault_ptr = apdu->ise_fault.ise_fault_ptr;
	u32 old_fault_ptr = apdu->ise_fault.ise_fault_ptr - 1;

	if (apdu->ise_fault.ise_fault_buf[old_fault_ptr] != ise_fault_status) {
		apdu->ise_fault.ise_fault_buf[fault_ptr] = ise_fault_status;
		apdu->ise_fault.ise_fault_ptr++;
	}

	if (apdu->ise_fault.ise_fault_ptr >= ISE_ATTACK_BUFFER_SIZE) {
		apdu->ise_fault.ise_fault_ptr = (ISE_ATTACK_BUFFER_SIZE - 1);
		dev_err(apdu->dev, "a risk of ise fault state loss\n");
	}
}

static void sprd_apdu_ise_gp_status_caching(struct sprd_apdu_device *apdu,
					    u32 status)
{
	if (apdu->ise_atr.gp_index >= ISE_ATTACK_BUFFER_SIZE)
		apdu->ise_atr.gp_index = 0;

	apdu->ise_atr.ise_gp_status[apdu->ise_atr.gp_index] = status;
	apdu->ise_atr.gp_index++;
}

static void sprd_apdu_send_ise_fault_status(struct sprd_apdu_device *apdu)
{
	/* send ramining fault status */
	if (apdu->ise_fault.ise_fault_ptr > 1) {
		apdu->ise_fault.ise_fault_allow_to_send_flag = 0;
		apdu->ise_fault.ise_fault_buf[0] = MESSAGE_HEADER_FAULT;
		sprd_apdu_send_usrmsg((char *)apdu->ise_fault.ise_fault_buf,
				      (4 * apdu->ise_fault.ise_fault_ptr));
		sprd_apdu_ise_fault_buffer_init(apdu);
	} else {
		apdu->ise_fault.ise_fault_allow_to_send_flag = 1;
	}

	apdu->ise_fault.ise_fault_status = 0;
}

static int sprd_apdu_send_rcv_cmd(struct sprd_apdu_device *apdu, char *info,
				  char *cmd, u32 cmd_len, u32 *rep_data,
				  u32 rep_word_len)
{
	unsigned long wait_event_time;
	int ret;
	long ret2;

	if (sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	apdu->rx_done = 0;
	ret = sprd_apdu_write_data(apdu, (void *)cmd, cmd_len);
	if (ret < 0) {
		dev_err(apdu->dev,
			"send cmd(%s):write error(%d)\n", info, ret);
		return ret;
	}

	wait_event_time = msecs_to_jiffies(MAX_WAIT_TIME);
	ret2 = wait_event_interruptible_timeout(apdu->read_wq, apdu->rx_done,
						wait_event_time);
	if (ret2 < 0) {
		dev_err(apdu->dev, "ap read error\n");
		return -EIO;
	} else if (ret == 0 && apdu->rx_done == 0) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev, "read cmd(%s) answer timeout\n", info);
		return -ETIMEDOUT;
	}

	/* answer include header --one word */
	ret = sprd_apdu_read_data(apdu, rep_data, rep_word_len);
	if (ret < 0)
		dev_err(apdu->dev, "rd cmd(%s) answer fail(%d)\n", info, ret);

	sprd_apdu_clear_fifo(apdu->base);
	return ret;
}

long sprd_apdu_normal_pd_ise_req(struct sprd_apdu_device *apdu)
{
	u32 rep_data[10] = {0};
	char cmd_normal_power_down[8] = {
		0x00, 0xf6, 0xff, 0xfa,
		0x00, 0x00, 0x00, 0x00
	};
	int ret;

	ret = sprd_apdu_send_rcv_cmd(apdu, "normal_pd_ise",
				     cmd_normal_power_down, 5, rep_data, 2);
	if (ret < 0) {
		dev_err(apdu->dev, "req nomal pd ise fail\n");
		return -ENXIO;
	}

	/* check ise ready, can be power down by setting pmu reg */
	if ((rep_data[1] & GENMASK(15, 0)) != ISE_APDU_ANSWER_SUCCESS) {
		dev_err(apdu->dev, "ise not ready for power down(0x%lx)\n",
			(rep_data[1] & GENMASK(15, 0)));
		return -EFAULT;
	}

	return 0;
}

static long sprd_apdu_set_current_slot(struct sprd_apdu_device *apdu,
				       u64 current_slot)
{
	if (current_slot == 0 || current_slot == 1) {
		apdu->slot = current_slot;
		return 0;
	}

	dev_err(apdu->dev, "%s() invalid slot value %lu\n",
		__func__, current_slot);
	return -1;
}

static int sprd_apdu_sync_counter(struct sprd_apdu_device *apdu)
{

	struct file *isedata_file = NULL;
	loff_t ise_counter_offset = MEDDDR_ISEDATA_OFFSET_BASE_ADDRESS +
				    (loff_t)(apdu->slot) * apdu->pub_ise_cfg.med_size;
	loff_t offset = 0;
	ssize_t ret;

	isedata_file = filp_open(ISEDATA_DEV_PATH, O_RDWR, 0644);
	if (IS_ERR(isedata_file)) {
		dev_err(apdu->dev,
			"%s() open isedata partition error\n", __func__);
		return -1;
	}

	offset = vfs_llseek(isedata_file, ise_counter_offset, SEEK_SET);

	if (offset != ise_counter_offset) {
		dev_err(apdu->dev,
			"%s() seek ise data partition error\n", __func__);
		filp_close(isedata_file, NULL);
		return -1;
	}
	dev_dbg(apdu->dev, "%s() ise_counter_offset = %ld, offset = %ld\n",
		__func__, ise_counter_offset, offset);

	ret = kernel_write(isedata_file, apdu->medddr_address,
			   MEDDDR_COUNTER_AREA_MAX_SIZE, &offset);
	if (ret != MEDDDR_COUNTER_AREA_MAX_SIZE) {
		dev_err(apdu->dev,
			"ise medddr dump file write failed: %zd\n", ret);
		filp_close(isedata_file, NULL);
		return -1;
	}

	filp_close(isedata_file, NULL);
	return 0;
}

static int sprd_apdu_save_medddr_area(struct sprd_apdu_device *apdu)
{

	struct file *isedata_file = NULL;
	loff_t ise_counter_offset = MEDDDR_ISEDATA_OFFSET_BASE_ADDRESS +
				    (loff_t)(apdu->slot) * apdu->pub_ise_cfg.med_size;
	loff_t offset = 0;
	ssize_t ret;

	isedata_file = filp_open(ISEDATA_DEV_PATH, O_RDWR, 0644);
	if (IS_ERR(isedata_file)) {
		dev_err(apdu->dev,
			"%s() open isedate partition error\n", __func__);
		return -1;
	}

	offset = vfs_llseek(isedata_file, ise_counter_offset, SEEK_SET);

	if (offset != ise_counter_offset) {
		dev_err(apdu->dev,
			"%s() seek ise data partition error\n", __func__);
		filp_close(isedata_file, NULL);
		return -1;
	}
	dev_dbg(apdu->dev, "%s() ise_counter_offset = %ld, offset = %ld\n",
		__func__, ise_counter_offset, offset);

	ret = kernel_write(isedata_file, apdu->medddr_address,
			   apdu->pub_ise_cfg.med_size, &offset);
	if (ret != apdu->pub_ise_cfg.med_size) {
		dev_err(apdu->dev, "save all medddr to flash write failed: %zd\n", ret);
		filp_close(isedata_file, NULL);
		return -1;
	}

	filp_close(isedata_file, NULL);
	return 0;
}

static int sprd_apdu_restore_medddr_area(struct sprd_apdu_device *apdu)
{

	struct file *isedata_file = NULL;
	loff_t ise_counter_offset = MEDDDR_ISEDATA_OFFSET_BASE_ADDRESS +
				    (loff_t)(apdu->slot) * apdu->pub_ise_cfg.med_size;
	loff_t offset = 0;
	ssize_t ret;

	isedata_file = filp_open(ISEDATA_DEV_PATH, O_RDWR, 0644);
	if (IS_ERR(isedata_file)) {
		dev_err(apdu->dev,
			"%s() open isedate partition error\n", __func__);
		return -1;
	}

	offset = vfs_llseek(isedata_file, ise_counter_offset, SEEK_SET);

	if (offset != ise_counter_offset) {
		dev_err(apdu->dev,
			"%s() seek ise data partition error\n", __func__);
		filp_close(isedata_file, NULL);
		return -1;
	}

	ret = kernel_read(isedata_file, apdu->medddr_address,
			  apdu->pub_ise_cfg.med_size, &offset);
	if (ret != apdu->pub_ise_cfg.med_size) {
		dev_err(apdu->dev,
			"restore medddr area from isedata read failed: %zd\n",
			ret);
		filp_close(isedata_file, NULL);
		return -1;
	}

	filp_close(isedata_file, NULL);
	return 0;
}

void sprd_apdu_normal_pd_sync_cnt_lv1(struct sprd_apdu_device *apdu)
{
	loff_t offset = 0;
	loff_t mem_pos = MEDDDR_ISEDATA_OFFSET_BASE_ADDRESS;
	struct file *isedata_file = NULL;
	ssize_t res;

	isedata_file = filp_open(ISEDATA_DEV_PATH, O_RDWR, 0644);
	if (IS_ERR(isedata_file)) {
		dev_err(apdu->dev,
			"%s open ise data partition error\n", __func__);
		return;
	}

	offset = vfs_llseek(isedata_file, mem_pos, SEEK_SET);

	if (offset != mem_pos) {
		dev_err(apdu->dev,
			"%s seek ise data partition error\n", __func__);
		filp_close(isedata_file, NULL);
		return;
	}

	/* may flush cnt and lv1 sometime when ise normal power down */
	res = kernel_write(isedata_file, apdu->medddr_address,
			   MEDDDR_COUNTER_LV1_AREA_MAX_SIZE, &mem_pos);
	if (res != MEDDDR_COUNTER_LV1_AREA_MAX_SIZE) {
		dev_err(apdu->dev,
			"ise normal power down write counter/lv1 failed: %zd\n",
			res);
		filp_close(isedata_file, NULL);
		return;
	}

	filp_close(isedata_file, NULL);
}

static long sprd_apdu_ioctl(struct file *fp, unsigned int code,
			    unsigned long value)
{
	struct sprd_apdu_device *apdu = fp->private_data;
	struct med_parse_info_t med_info = {0};
	long ret = 0;
	u64 rcv_data = 0;
	struct dma_buf *dmabuf = NULL;

	dev_info(apdu->dev, "apdu ioctl code = %u\n", code);

	mutex_lock(&apdu->mutex);
	switch (code) {
	case APDU_RESET:
		ret = sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES);
		if (ret < 0) {
			dev_err(apdu->dev, "power on check fail\n");
			break;
		}
		sprd_apdu_rst(apdu->base);
		break;

	case APDU_CLR_FIFO:
		ret = sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES);
		if (ret < 0) {
			dev_err(apdu->dev, "power on check fail\n");
			break;
		}
		sprd_apdu_clear_fifo(apdu->base);
		break;

	case APDU_CHECK_CLR_FIFO_DONE:
		ret = sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES);
		if (ret < 0) {
			dev_err(apdu->dev, "power on check fail\n");
			break;
		}

		/*
		 * check ISE answer the request of sprd_apdu_clear_fifo
		 * return value: 0--TRUE; 1--FALSE
		 */
		ret = sprd_apdu_check_clr_fifo_done(apdu);
		break;

	case APDU_CHECK_MED_ERROR_STATUS:
		ret = copy_to_user((void __user *)value,
				   &apdu->med_err.med_error_status,
				   sizeof(u32)) ? (-EFAULT) : 0;
		if (ret)
			break;
		apdu->med_err.med_error_status = 0;
		break;

	case APDU_CHECK_FAULT_STATUS:
		/* if not any ise fault, message just a header */
		apdu->ise_fault.ise_fault_buf[0] = MESSAGE_HEADER_FAULT;
		ret = copy_to_user((void __user *)value,
				   apdu->ise_fault.ise_fault_buf,
				   apdu->ise_fault.ise_fault_ptr * 4
				   ) ? (-EFAULT) : 0;
		if (ret)
			break;
		sprd_apdu_ise_fault_buffer_init(apdu);
		break;

	case APDU_GET_ATR_INF:
		ret = copy_to_user((void __user *)value,
				   &(apdu->ise_atr.atr[1]),
				   APDU_DATA_LEN_MASK(*apdu->ise_atr.atr)
				   ) ? (-EFAULT) : 0;
		break;

	case APDU_SET_MED_HIGH_ADDR:
		ret = copy_from_user(&rcv_data, (void __user *)value,
				     sizeof(u64)) ? (-EFAULT) : 0;
		if (ret)
			break;

		/* renew med high addr */
		apdu->pub_ise_cfg.med_hi_addr = rcv_data;
		ret = med_set_high_addr(apdu, rcv_data);
		break;

	case APDU_SEND_MED_HIGH_ADDR:
		/* send kernel apdu med addr info to user space */
		ret = copy_to_user((void __user *)value,
				   &(apdu->pub_ise_cfg.med_hi_addr),
				   sizeof(apdu->pub_ise_cfg.med_hi_addr)
				   ) ? (-EFAULT) : 0;
		break;

	case APDU_SEND_MED_RESERVED_SIZE:
		/* the size of reseaved med size of DDR */
		ret = copy_to_user((void __user *)value,
				   &(apdu->pub_ise_cfg.med_size),
				   sizeof(apdu->pub_ise_cfg.med_size)
				   ) ? (-EFAULT) : 0;
		break;

	case APDU_ION_MAP_MEDDDR_IN_KERNEL:
		ret = copy_from_user(&rcv_data, (void __user *)value,
				     sizeof(u64)) ? (-EFAULT) : 0;
		if (ret) {
			dev_err(apdu->dev, "copy meddddr address failed!\n");
			break;
		}

		dmabuf = dma_buf_get((int)rcv_data);
		apdu->medddr_address = sprd_ion_map_kernel(dmabuf, 0);
		dma_buf_put(dmabuf);
		break;

	case APDU_ION_UNMAP_MEDDDR_IN_KERNEL:
		ret = copy_from_user(&rcv_data, (void __user *)value,
				     sizeof(u64)) ? (-EFAULT) : 0;
		if (ret) {
			dev_err(apdu->dev, "copy medddr address failed!\n");
			break;
		}
		dmabuf = dma_buf_get((int)rcv_data);
		sprd_ion_unmap_kernel(dmabuf, 0);
		dma_buf_put(dmabuf);

		apdu->medddr_address = NULL;
		break;

	case APDU_MED_REWRITE_INFO_PARSE:
		ret = copy_from_user(&rcv_data, (void __user *)value,
				     sizeof(u64)) ? (-EFAULT) : 0;
		if (ret)
			break;
		/* first word--address, second word--length */
		ret = med_rewrite_info_parse(apdu, &med_info, (u32)rcv_data,
					     (u32)(rcv_data >> 32));
		if (ret < 0)
			break;
		ret = copy_to_user((void __user *)value, &med_info,
				   sizeof(struct med_parse_info_t)
				   ) ? (-EFAULT) : 0;
		break;

	case APDU_NORMAL_PWR_ON_CFG:
		ret = sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES);
		if (ret < 0)
			break;

		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);

		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);
		ret = med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		break;

	case APDU_ENTER_APDU_LOOP:
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret)
			break;

		ret = apdu->pd_ise->ise_soft_reset(apdu);
		if (ret)
			break;

		ret = sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES2);
		if (ret < 0)
			break;

		/*
		 * if enter ise romcode apdu loop,
		 * atr will match romcode atr format
		 */
		ret = sprd_apdu_send_enter_apdu_loop_req(apdu);
		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);
		break;

	case APDU_SOFT_RESET_ISE:
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret)
			break;

		ret = apdu->pd_ise->ise_soft_reset(apdu);
		if (ret)
			break;

		ret = sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES2);
		if (ret < 0)
			break;

		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);
		break;

	case APDU_FAULT_INT_RESOLVE_DONE:
		/*
		 * if last ise fault not decide to reset ise,
		 * user space inform apdu drvier allow to send next fault status
		 */
		sprd_apdu_send_ise_fault_status(apdu);
		break;

	case APDU_NORMAL_POWER_ON_ISE:
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret)
			break;

		/* just do one time when chip power on */
		ret = apdu->pd_ise->ise_cold_power_on(apdu);
		if (ret) {
			dev_err(apdu->dev, "ise cold boot fail\n");
			break;
		}

		/*
		 * if ISE (apdu module) power on after apdu driver probe,
		 * need enable apdu interrupt again.
		 */
		ret = sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES2);
		if (ret < 0) {
			dev_err(apdu->dev, "apdu power on check failed!\n");
			break;
		}

		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);
		dev_info(apdu->dev, "ise cold power on and reset success!\n");
		break;

	case APDU_NORMAL_POWER_DOWN_ISE:
		/* power down ise power_down  domain and ise AON domain */
		ret = apdu->pd_ise->ise_full_power_down(apdu);
		break;

	case APDU_POWEROFF_ISE_AON_DOMAIN:
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret)
			break;

		/* set hard reset signal */
		ret = apdu->pd_ise->ise_hard_reset_set(apdu);
		break;

	case APDU_SOFT_RESET_ISE_AON_DOMAIN:
		/* release hard reset signal */
		ret = apdu->pd_ise->ise_hard_reset_clr(apdu);
		break;

	case APDU_SAVE_MEDDDR_COUNTER_DATA:
		/* sync medddr counter area from DDR to flash */
		sprd_apdu_sync_counter(apdu);
		break;

	case APDU_LOAD_MEDDDR_DATA_IN_KERNEL:
		/* restore medddr all area data from flash to DDR */
		sprd_apdu_restore_medddr_area(apdu);
		break;

	case APDU_SAVE_MEDDDR_DATA_IN_KERNEL:
		/* save medddr all area data from DDR to flash */
		sprd_apdu_save_medddr_area(apdu);
		break;

	case APDU_SET_CURRENT_SLOT_IN_KERNEL:
		ret = copy_from_user(&rcv_data, (void __user *)value,
				     sizeof(u64)) ? (-EFAULT) : 0;
		if (ret)
			break;
		ret = sprd_apdu_set_current_slot(apdu, rcv_data);
		break;

	case APDU_ISE_STATUS_CHECK:
		ret = apdu->pd_ise->ise_pd_status_check(apdu);
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&apdu->mutex);
	return ret;
}

static int sprd_apdu_open(struct inode *inode, struct file *fp)
{
	struct sprd_apdu_device *apdu =
		container_of(fp->private_data, struct sprd_apdu_device, misc);

	fp->private_data = apdu;

	return 0;
}

static int sprd_apdu_release(struct inode *inode, struct file *fp)
{
	fp->private_data = NULL;

	return 0;
}

static const struct file_operations sprd_apdu_fops = {
	.owner = THIS_MODULE,
	.read = sprd_apdu_read,
	.write = sprd_apdu_write,
	.unlocked_ioctl = sprd_apdu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sprd_apdu_ioctl,
#endif
	.open = sprd_apdu_open,
	.release = sprd_apdu_release,
};

static irqreturn_t sprd_apdu_interrupt(int irq, void *data)
{
	u32 reg_int_status, reg_inf_int_status, mask;
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)data;

	/*
	 * may such interrupt not enable but triggered
	 * because of ISE or APDU module reset will clear APDU REE reg
	 * (but AP may unknown)
	 */
	if (sprd_apdu_enable_check(apdu)) {
		reg_int_status = sprd_apdu_int_raw_status(apdu->base);
		reg_inf_int_status = sprd_apdu_inf_int_raw_status(apdu->base);
	} else {
		reg_int_status = sprd_apdu_int_mask_status(apdu->base);
		reg_inf_int_status = sprd_apdu_inf_int_mask_status(apdu->base);
	}

	/*
	 * APDU_INT_MED_WR_DONE not clear here,
	 * clear after med_rewrite_post_process
	 */
	mask = (~APDU_INT_MED_WR_DONE) & APDU_INT_BITS;
	sprd_apdu_clear_int(apdu->base, reg_int_status & mask);
	sprd_apdu_clear_inf_int(apdu->base, reg_inf_int_status);

	/* all used inf interrupt */
	if (reg_inf_int_status & (APDU_INF_INT_FAULT | APDU_INF_INT_GET_ATR))
		sprd_apdu_ise_gp_status_caching(apdu, reg_inf_int_status);

	if (reg_inf_int_status & APDU_INF_INT_FAULT) {
		/* ise fault status, need to be saved as soon as possible */
		sprd_apdu_ise_fault_status_caching(apdu, (reg_inf_int_status &
							  APDU_INF_INT_FAULT));
		apdu->ise_fault.ise_fault_status = 1;
	}

	if (reg_inf_int_status & APDU_INF_INT_GET_ATR) {
		reg_int_status &= (~APDU_INT_RX_EMPTY_TO_NOEMPTY);
		/* inform sprd_apdu_write_data wait atr rx done */
		apdu->ise_atr.atr_rcv_status = 1;
	}
	if (reg_int_status & APDU_INT_MED_WR_DONE) {
		reg_int_status &= (~APDU_INT_RX_EMPTY_TO_NOEMPTY);
		/*
		 * disable interrupt for stop interrupt trigger,
		 * and clear interrupt after med rewrite info figure down.
		 * ISE will wait for AP by check interrupt sync status.
		 */
		sprd_apdu_int_dis(apdu->base, APDU_INT_MED_WR_DONE);
		apdu->med_rewr.med_wr_done = 1;
	}
	if (reg_int_status & APDU_INT_MED_ERR)
		apdu->med_err.med_error_status = 1;
	if (reg_int_status & APDU_INT_RX_EMPTY_TO_NOEMPTY) {
		apdu->rx_done = 1;
		wake_up(&apdu->read_wq);
	}

	/* using first interrupt to enable other interrupt when ISE reset */
	if (sprd_apdu_enable_check(apdu))
		sprd_apdu_enable(apdu);

	if ((apdu->ise_fault.ise_fault_status &&
	     apdu->ise_fault.ise_fault_allow_to_send_flag) ||
	    apdu->ise_atr.atr_rcv_status || apdu->med_rewr.med_wr_done ||
	    apdu->med_err.med_error_status)
		return IRQ_WAKE_THREAD;
	else
		return IRQ_HANDLED;
}

static irqreturn_t sprd_apdu_irq_thread_fn(int irq, void *data)
{
	struct sprd_apdu_device *apdu = (struct sprd_apdu_device *)data;
	u32 med_error_info = MESSAGE_HEADER_MED_ERR;

	if (apdu->ise_fault.ise_fault_status &&
	    apdu->ise_fault.ise_fault_allow_to_send_flag) {
		apdu->ise_fault.ise_fault_allow_to_send_flag = 0;
		apdu->ise_fault.ise_fault_status = 0;
		apdu->ise_fault.ise_fault_buf[0] = MESSAGE_HEADER_FAULT;
		if (apdu->ise_fault.ise_fault_ptr > 1)
			sprd_apdu_send_usrmsg((char *)apdu->ise_fault.ise_fault_buf,
					      (4 * apdu->ise_fault.ise_fault_ptr));
		sprd_apdu_ise_fault_buffer_init(apdu);
	}

	if (apdu->med_err.med_error_status) {
		sprd_apdu_send_usrmsg((char *)&med_error_info,
				      sizeof(med_error_info));
		apdu->med_err.med_error_status = 0;
	}

	if (apdu->med_rewr.med_wr_done) {
		sprd_apdu_get_med_rewrite_info(apdu);
		/*
		 * enable and clr last med wr done
		 * interrupt after figure done
		 */
		sprd_apdu_clear_int(apdu->base, APDU_INT_MED_WR_DONE);
		sprd_apdu_int_en(apdu->base, APDU_INT_MED_WR_DONE);
		apdu->med_rewr.med_wr_done = 0;
	}

	/* atr just received when ise power on or reset */
	if (apdu->ise_atr.atr_rcv_status) {
		sprd_apdu_get_atr(apdu);
		apdu->ise_atr.atr_rcv_status = 0;
	}

	return IRQ_HANDLED;
}

static ssize_t get_random_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 rep_data[0x10] = {0};
	char cmd_get_random[8] = {
		0x00, 0x84, 0x00, 0x00,
		0x04, 0x00, 0x00, 0x00
	};
	int ret;

	if (!apdu)
		return -EINVAL;

	ret = sprd_apdu_send_rcv_cmd(apdu, "get_random",
				     cmd_get_random, 8, rep_data, 3);
	if (ret < 0) {
		dev_err(apdu->dev, "get_random fail\n");
		return -ENXIO;
	}

	dev_info(apdu->dev, "get random:\n");
	/*
	 * first word -- header,
	 * second word -- random number,
	 * and third word -- status word
	 */
	sprd_apdu_dump_data(rep_data, 3);
	return 0;
}

static ssize_t get_atr_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 word_len;

	if (!apdu)
		return -EINVAL;

	word_len = DIV_CEILING(APDU_DATA_LEN_MASK(*apdu->ise_atr.atr), 4);
	dev_info(apdu->dev, "ise atr:\n");
	sprd_apdu_dump_data(&(apdu->ise_atr.atr[1]), word_len);
	if (apdu->ise_atr.atr[0] == 0x0)
		dev_err(apdu->dev,
			"did not get atr value, please check ISE status\n");
	return 0;
}

static ssize_t med_rewrite_info_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);

	if (!apdu)
		return -EINVAL;

	dev_info(apdu->dev, "ise rewrite info:\n");
	sprd_apdu_dump_data(apdu->med_rewr.med_rewrite, APDU_MED_INFO_SIZE);
	if (apdu->med_rewr.med_rewrite[0] == 0x0)
		dev_err(apdu->dev,
			"did not get med rewrite info, check ISE status\n");

	return 0;
}

static ssize_t get_fault_status_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);

	if (!apdu)
		return -EINVAL;

	dev_info(apdu->dev, "ise fault buf:\n");
	/* first word is message header */
	sprd_apdu_dump_data(apdu->ise_fault.ise_fault_buf,
			    ISE_ATTACK_BUFFER_SIZE);
	dev_info(apdu->dev, "ise fault ptr:\n");
	sprd_apdu_dump_data(&apdu->ise_fault.ise_fault_ptr, 1);
	dev_info(apdu->dev, "allow send flag:0x%x\n",
		 apdu->ise_fault.ise_fault_allow_to_send_flag);
	dev_info(apdu->dev, "ise gp index:0x%x, status:\n", apdu->ise_atr.gp_index);
	sprd_apdu_dump_data(apdu->ise_atr.ise_gp_status, ISE_ATTACK_BUFFER_SIZE);

	return 0;
}

static ssize_t med_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);

	if (!apdu)
		return -EINVAL;

	dev_info(apdu->dev, "med wr done:0x%x\n", apdu->med_rewr.med_wr_done);
	dev_info(apdu->dev, "med err status:0x%x\n", apdu->med_err.med_error_status);
	dev_info(apdu->dev, "med addr:0x%llx, ap size:0x%llx, ise size:0x%llx\n",
		 apdu->pub_ise_cfg.med_hi_addr, apdu->pub_ise_cfg.med_size,
		 apdu->pub_ise_cfg.med_ise_size);

	return 0;
}

static ssize_t packet_send_rcv_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	int ret, i;
	char cmd[2050] = {
		0x00, 0x84, 0x00, 0x00,
		0x04, 0x00, 0x00, 0x00,
	};

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	for (i = 8; i < sizeof(cmd); i++)
		cmd[i] = (char)i;
	ret = sprd_apdu_write_data(apdu, (void *)cmd, sizeof(cmd));
	if (ret < 0)
		dev_err(apdu->dev, "send error\n");

	return ((ret < 0) ? (-EFAULT) : count);
}

static ssize_t packet_send_rcv_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u32 data_len, word_len, header = 0;
	int ret;

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	if (sprd_apdu_get_rx_fifo_len(apdu->base) == 0x0) {
		dev_err(apdu->dev, "rx fifo empty\n");
		return -EFAULT;
	}

	/* check apdu packet valid */
	sprd_apdu_read_rx_fifo(apdu->base, &header, 1);
	if (APDU_MAGIC_MASK(header) != APDU_MAGIC_NUM) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev,
			"read data magic error! req clr fifo.\n");
		return -EINVAL;
	}

	data_len = APDU_DATA_LEN_MASK(header);
	if (data_len > APDU_RX_MAX_SIZE) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev,
			"read len:%u exceed max:%u! req clr fifo.\n",
			data_len, APDU_RX_MAX_SIZE);
		return -EINVAL;
	}

	word_len = DIV_CEILING(data_len, 4);
	ret = sprd_apdu_read_data(apdu, (u32 *)apdu->rx_buf, word_len);
	if (ret < 0) {
		sprd_apdu_clear_fifo(apdu->base);
		dev_err(apdu->dev, "apdu read fifo fail(0x%x)\n", ret);
		return -EBUSY;
	}

	dev_info(apdu->dev, "ise packet rcv:\n");
	sprd_apdu_dump_data((u32 *)apdu->rx_buf, word_len);
	return ret;
}

static ssize_t set_med_high_addr_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	u64 med_ddr_base = 0;
	ssize_t ret;

	if (!apdu)
		return -EINVAL;
	ret = kstrtou64(buf, 16, &med_ddr_base);
	if (ret) {
		dev_err(apdu->dev, "invalid value.\n");
		return -EINVAL;
	}

	apdu->pub_ise_cfg.med_hi_addr = med_ddr_base;
	ret = (ssize_t)med_set_high_addr(apdu, med_ddr_base);

	return ((ret < 0) ? ret : count);
}

static ssize_t med_info_parse_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	struct med_parse_info_t med_info;
	u32 data_offset, data_len;
	u64 temp_value = 0;
	ssize_t ret;

	if (!apdu)
		return -EINVAL;
	ret = kstrtou64(buf, 16, &temp_value);
	if (ret) {
		dev_err(apdu->dev, "invalid value.\n");
		return -EINVAL;
	}

	/*
	 * Parse data as data_offset|data_len with each 4 byte
	 * --example:0x0000000000000020
	 * will be parse as offset:0x0; length:0x20
	 */
	data_offset = (u32)(temp_value >> 32);
	data_len = (u32)temp_value;
	ret = (ssize_t)med_rewrite_info_parse(apdu, &med_info, data_offset, data_len);
	dev_info(apdu->dev, "med parse info:\n");
	sprd_apdu_dump_data((u32 *)&med_info,
			    (sizeof(struct med_parse_info_t) / 4));

	return ((ret < 0) ? ret : count);
}

static ssize_t sprd_apdu_reset_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	long ret, timeout = 100;

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	sprd_apdu_clear_fifo(apdu->base);
	sprd_apdu_rst(apdu->base);

	while (timeout--) {
		usleep_range(100, 200);
		ret = sprd_apdu_check_clr_fifo_done(apdu);
		if (!ret) {
			dev_info(apdu->dev, "rst apdu ok\n");
			return 0;
		}
	}

	dev_err(apdu->dev, "wait for ISE reset apdu timeout!\n");
	return -ETIMEDOUT;
}

static ssize_t sprd_apdu_reenable_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);

	if (!apdu)
		return -EINVAL;
	if (sprd_apdu_power_on_check(apdu, APDU_POWER_ON_CHECK_TIMES) < 0) {
		dev_err(apdu->dev, "power on check fail\n");
		return -ENXIO;
	}

	memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
	sprd_apdu_ise_fault_buffer_init(apdu);
	sprd_apdu_enable(apdu);

	return 0;
}

static ssize_t ise_cold_power_on_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	ssize_t ret;

	if (!apdu)
		return -EINVAL;
	if (apdu->pd_ise->ise_pd_magic_num == ISE_PD_MAGIC_NUM) {
		apdu->pd_ise->ise_pd_magic_num = 0;
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = (ssize_t)med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret) {
			dev_err(apdu->dev, "set med high addr fail\n");
			return ret;
		}

		ret = (ssize_t)apdu->pd_ise->ise_cold_power_on(apdu);
		if (ret) {
			dev_err(apdu->dev, "ise cold power on fail\n");
			return ret;
		}

		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);
	} else {
		dev_err(apdu->dev, "ise pd magic num not correct\n");
		return -EFAULT;
	}

	return 0;
}

static ssize_t ise_full_power_down_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	ssize_t ret;

	if (!apdu)
		return -EINVAL;

	if (apdu->pd_ise->ise_pd_magic_num == ISE_PD_MAGIC_NUM) {
		apdu->pd_ise->ise_pd_magic_num = 0;
		ret = (ssize_t)apdu->pd_ise->ise_full_power_down(apdu);
	} else {
		dev_err(apdu->dev, "ise pd magic num not correct\n");
		return -EFAULT;
	}

	return ret;
}

static ssize_t ise_hard_reset_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	ssize_t ret;

	if (!apdu)
		return -EINVAL;
	if (apdu->pd_ise->ise_pd_magic_num == ISE_PD_MAGIC_NUM) {
		apdu->pd_ise->ise_pd_magic_num = 0;
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret) {
			dev_err(apdu->dev, "set med high addr fail\n");
			return ret;
		}

		ret = (ssize_t)apdu->pd_ise->ise_hard_reset(apdu);
		if (ret) {
			dev_err(apdu->dev, "ise hard reset fail\n");
			return ret;
		}

		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);
	} else {
		dev_err(apdu->dev, "ise pd magic num not correct\n");
		return -EFAULT;
	}

	return 0;
}

static ssize_t ise_soft_reset_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	ssize_t ret;

	if (!apdu)
		return -EINVAL;
	if (apdu->pd_ise->ise_pd_magic_num == ISE_PD_MAGIC_NUM) {
		apdu->pd_ise->ise_pd_magic_num = 0;
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret) {
			dev_err(apdu->dev, "set med high addr fail\n");
			return ret;
		}

		ret = (ssize_t)apdu->pd_ise->ise_soft_reset(apdu);
		if (ret) {
			dev_err(apdu->dev, "ise soft reset fail\n");
			return ret;
		}

		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);
	} else {
		dev_err(apdu->dev, "ise pd magic num not correct\n");
		return -EFAULT;
	}

	return 0;
}

static ssize_t ise_enter_apdu_loop_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	ssize_t ret;

	if (!apdu)
		return -EINVAL;
	if (apdu->pd_ise->ise_pd_magic_num == ISE_PD_MAGIC_NUM) {
		apdu->pd_ise->ise_pd_magic_num = 0;
		memset(apdu->ise_atr.atr, 0, (APDU_ATR_DATA_MAX_SIZE + 4));
		sprd_apdu_ise_fault_buffer_init(apdu);
		ret = (ssize_t)med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
		if (ret) {
			dev_err(apdu->dev, "set med high addr fail\n");
			return ret;
		}

		ret = (ssize_t)apdu->pd_ise->ise_soft_reset(apdu);
		if (ret) {
			dev_err(apdu->dev, "ise soft reset fail\n");
			return ret;
		}

		ret = sprd_apdu_send_enter_apdu_loop_req(apdu);
		if (ret) {
			dev_err(apdu->dev, "req enter apdu loop fail\n");
			return ret;
		}

		if (sprd_apdu_enable_check(apdu))
			sprd_apdu_enable(apdu);

		dev_info(apdu->dev, "please using 'get atr' to check match romcode atr\n");
	} else {
		dev_err(apdu->dev, "ise pd magic num not correct\n");
		return -EFAULT;
	}

	return 0;
}

static ssize_t ise_status_check_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	ssize_t ret;

	if (!apdu)
		return -EINVAL;

	ret = (ssize_t)apdu->pd_ise->ise_pd_status_check(apdu);
	if (ret == ISE_PD_PWR_DOWN)
		dev_info(apdu->dev, "ise pd status: power down\n");
	else if (ret == ISE_PD_PWR_ON)
		dev_info(apdu->dev, "ise pd status: power on\n");
	else
		dev_err(apdu->dev, "ise pd status: no current\n");
	return ret;
}

static ssize_t ise_pd_set_magic_num_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct sprd_apdu_device *apdu = dev_get_drvdata(dev);
	ssize_t ret;

	if (!apdu)
		return -EINVAL;

	/* for ise pd secure, using a software magic num limit operation */
	ret = kstrtou32(buf, 16, &apdu->pd_ise->ise_pd_magic_num);
	if (ret) {
		dev_err(apdu->dev, "invalid value.\n");
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR_RO(get_random);
static DEVICE_ATTR_RO(get_atr);
static DEVICE_ATTR_RO(get_fault_status);
static DEVICE_ATTR_RO(med_status);
static DEVICE_ATTR_RW(packet_send_rcv);
static DEVICE_ATTR_RO(med_rewrite_info);
static DEVICE_ATTR_WO(med_info_parse);
static DEVICE_ATTR_WO(set_med_high_addr);
static DEVICE_ATTR_RO(sprd_apdu_reset);
static DEVICE_ATTR_RO(sprd_apdu_reenable);
static DEVICE_ATTR_RO(ise_cold_power_on);
static DEVICE_ATTR_RO(ise_full_power_down);
static DEVICE_ATTR_RO(ise_hard_reset);
static DEVICE_ATTR_RO(ise_soft_reset);
static DEVICE_ATTR_RO(ise_enter_apdu_loop);
static DEVICE_ATTR_RO(ise_status_check);
static DEVICE_ATTR_WO(ise_pd_set_magic_num);

static struct attribute *sprd_apdu_attrs[] = {
	&dev_attr_get_random.attr,
	&dev_attr_get_atr.attr,
	&dev_attr_get_fault_status.attr,
	&dev_attr_med_status.attr,
	&dev_attr_packet_send_rcv.attr,
	&dev_attr_med_rewrite_info.attr,
	&dev_attr_set_med_high_addr.attr,
	&dev_attr_med_info_parse.attr,
	&dev_attr_sprd_apdu_reset.attr,
	&dev_attr_sprd_apdu_reenable.attr,
	&dev_attr_ise_cold_power_on.attr,
	&dev_attr_ise_full_power_down.attr,
	&dev_attr_ise_hard_reset.attr,
	&dev_attr_ise_soft_reset.attr,
	&dev_attr_ise_enter_apdu_loop.attr,
	&dev_attr_ise_status_check.attr,
	&dev_attr_ise_pd_set_magic_num.attr,
	NULL
};
ATTRIBUTE_GROUPS(sprd_apdu);

static struct sprd_apdu_pd_ise_config_t qogirn6pro_ise_pd = {
	.ise_compatible = "sprd,qogirn6pro-ise-med",
	.ise_pd_status_check = qogirn6pro_ise_pd_status_check,
	.ise_cold_power_on = qogirn6pro_ise_cold_power_on,
	.ise_full_power_down = qogirn6pro_ise_full_power_down,
	.ise_hard_reset = qogirn6pro_ise_hard_reset,
	.ise_soft_reset = qogirn6pro_ise_soft_reset,
	.ise_hard_reset_set = qogirn6pro_ise_hard_reset_set,
	.ise_hard_reset_clr = qogirn6pro_ise_hard_reset_clr,
};

static const struct of_device_id sprd_apdu_match[] = {
	{.compatible = "sprd,qogirn6pro-apdu", .data = &qogirn6pro_ise_pd},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_apdu_match);

static int sprd_apdu_probe(struct platform_device *pdev)
{
	struct sprd_apdu_device *apdu;
	const struct of_device_id *of_id;
	struct device_node *ise_med_reserved;
	const __be32 *addr;
	struct resource *res;
	u32 buf_tx_sz = APDU_TX_MAX_SIZE;
	u32 buf_rx_sz = APDU_RX_MAX_SIZE;
	u32 atr_sz = APDU_ATR_DATA_MAX_SIZE + 4;
	u32 med_info_sz = APDU_MED_INFO_SIZE * 4;
	u32 ise_fault_buf_sz = ISE_ATTACK_BUFFER_SIZE * 4;
	int ret;

	apdu = devm_kzalloc(&pdev->dev, sizeof(*apdu), GFP_KERNEL);
	if (!apdu)
		return -ENOMEM;

	apdu->irq = platform_get_irq(pdev, 0);
	if (apdu->irq < 0) {
		dev_err(&pdev->dev, "failed to get apdu interrupt.\n");
		return apdu->irq;
	}

	of_id = of_match_node(sprd_apdu_match, pdev->dev.of_node);
	if (of_id) {
		apdu->pd_ise = (struct sprd_apdu_pd_ise_config_t *)of_id->data;
	} else {
		dev_err(&pdev->dev, "fail to fine matched id data\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	apdu->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(apdu->base)) {
		dev_err(&pdev->dev, "no apdu base specified\n");
		return PTR_ERR(apdu->base);
	}

	apdu->pub_ise_cfg.pub_reg_base =
		syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,sys-pub-reg");
	if (IS_ERR(apdu->pub_ise_cfg.pub_reg_base)) {
		dev_err(&pdev->dev, "no pub reg base specified\n");
		return PTR_ERR(apdu->pub_ise_cfg.pub_reg_base);
	}

	apdu->pd_ise->ise_pd_reg_base =
		syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,sys-ise-pd-reg");
	if (IS_ERR(apdu->pd_ise->ise_pd_reg_base)) {
		dev_err(&pdev->dev, "no pmu reg base specified\n");
		return PTR_ERR(apdu->pd_ise->ise_pd_reg_base);
	}

	apdu->pd_ise->ise_aon_reg_base =
		syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,sys-ise-aon-reg");
	if (IS_ERR(apdu->pd_ise->ise_aon_reg_base)) {
		dev_err(&pdev->dev, "no aon reg base specified\n");
		return PTR_ERR(apdu->pd_ise->ise_aon_reg_base);
	}

	apdu->pd_ise->ise_aon_clk_reg_base =
		syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,sys-ise-aon-clk-reg");
	if (IS_ERR(apdu->pd_ise->ise_aon_clk_reg_base)) {
		dev_err(&pdev->dev, "no aon clk reg base specified\n");
		return PTR_ERR(apdu->pd_ise->ise_aon_clk_reg_base);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,pub-ise-reg-offset",
				   &apdu->pub_ise_cfg.pub_ise_reg_offset);
	if (ret) {
		dev_err(&pdev->dev, "get pub-ise-reg-offset failed\n");
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,pub-ise-bit-offset",
				   &apdu->pub_ise_cfg.pub_ise_bit_offset);
	if (ret) {
		dev_err(&pdev->dev, "get pub-ise-bit-offset failed\n");
		return ret;
	}

	/* parse dts ise med reserved range in DDR and set med high addr */
	ise_med_reserved = of_find_node_by_name(NULL, "reserved-memory");
	if (!ise_med_reserved) {
		dev_err(&pdev->dev, "get reserved-memory fail\n");
		return -EFAULT;
	}
	ise_med_reserved =
		of_get_compatible_child(ise_med_reserved,
					apdu->pd_ise->ise_compatible);
	if (!ise_med_reserved) {
		dev_err(&pdev->dev, "get ise med reserverd info fail\n");
		return -EFAULT;
	}
	addr = of_get_address(ise_med_reserved, 0,
			      &apdu->pub_ise_cfg.med_size, NULL);
	if (!addr) {
		dev_err(&pdev->dev, "get ise med reserved addr/size fail\n");
		return -EFAULT;
	}
	apdu->pub_ise_cfg.med_hi_addr =
		of_translate_address(ise_med_reserved, addr);
	ret = (int)med_set_high_addr(apdu, apdu->pub_ise_cfg.med_hi_addr);
	if (ret) {
		dev_err(&pdev->dev, "set med_high_addr fail when probe\n");
		return -EFAULT;
	}
	ret = med_ise_size_parse(apdu);
	if (ret) {
		dev_err(&pdev->dev, "ise med size parse error\n");
		return -EFAULT;
	}

	apdu->tx_buf = devm_kzalloc(&pdev->dev, buf_tx_sz, GFP_KERNEL);
	if (!apdu->tx_buf)
		return -ENOMEM;
	apdu->rx_buf = devm_kzalloc(&pdev->dev, buf_rx_sz, GFP_KERNEL);
	if (!apdu->rx_buf)
		return -ENOMEM;
	apdu->ise_atr.atr = devm_kzalloc(&pdev->dev, atr_sz, GFP_KERNEL);
	if (!apdu->ise_atr.atr)
		return -ENOMEM;
	apdu->med_rewr.med_rewrite = devm_kzalloc(&pdev->dev, med_info_sz,
						  GFP_KERNEL);
	if (!apdu->med_rewr.med_rewrite)
		return -ENOMEM;
	apdu->ise_fault.ise_fault_buf = devm_kzalloc(&pdev->dev,
						     ise_fault_buf_sz,
						     GFP_KERNEL);
	if (!apdu->ise_fault.ise_fault_buf)
		return -ENOMEM;
	apdu->ise_fault.ise_fault_ptr = 1;
	apdu->ise_fault.ise_fault_allow_to_send_flag = 1;

	mutex_init(&apdu->mutex);
	init_waitqueue_head(&apdu->read_wq);

	apdu->misc.minor = MISC_DYNAMIC_MINOR;
	apdu->misc.name = APDU_DRIVER_NAME;
	apdu->misc.fops = &sprd_apdu_fops;
	ret = misc_register(&apdu->misc);
	if (ret) {
		dev_err(&pdev->dev, "misc_register FAILED\n");
		return ret;
	}

	apdu->dev = &pdev->dev;
	ret = devm_request_threaded_irq(apdu->dev, apdu->irq,
					sprd_apdu_interrupt,
					sprd_apdu_irq_thread_fn,
					0, APDU_DRIVER_NAME, apdu);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq %d\n", ret);
		goto err;
	}

	ret = sysfs_create_groups(&apdu->dev->kobj, sprd_apdu_groups);
	if (ret)
		dev_warn(apdu->dev, "failed to create apdu attributes\n");

	/* Create netlink socket */
	sprd_apdu_nlsk =
		(struct sock *)netlink_kernel_create(&init_net, APDU_NETLINK,
						     &sprd_apdu_netlink_cfg);
	if (!sprd_apdu_nlsk) {
		dev_err(apdu->dev, "netlink kernel create error!\n");
		goto err;
	}

	platform_set_drvdata(pdev, apdu);
	return 0;

err:
	misc_deregister(&apdu->misc);
	return ret;
}

static int sprd_apdu_remove(struct platform_device *pdev)
{
	struct sprd_apdu_device *apdu = platform_get_drvdata(pdev);

	misc_deregister(&apdu->misc);
	sysfs_remove_groups(&apdu->dev->kobj, sprd_apdu_groups);

	if (sprd_apdu_nlsk) {
		netlink_kernel_release(sprd_apdu_nlsk);
		sprd_apdu_nlsk = NULL;
	}

	return 0;
}

static struct platform_driver sprd_apdu_driver = {
	.probe = sprd_apdu_probe,
	.remove = sprd_apdu_remove,
	.driver = {
		.name = "sprd-apdu",
		.of_match_table = sprd_apdu_match,
	},
};

module_platform_driver(sprd_apdu_driver);
MODULE_DESCRIPTION("Spreadtrum APDU driver");
MODULE_LICENSE("GPL v2");
