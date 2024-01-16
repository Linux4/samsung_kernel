/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_dev.c
 *
 * LXS touch core layer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "lxs_ts_dev.h"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
static irqreturn_t lxs_ts_irq_thread(int irq, void *data);
static irqreturn_t secure_filter_interrupt(struct lxs_ts_data *ts)
{
	mutex_lock(&ts->secure_lock);
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->secure_pending_irqs));
		}

		mutex_unlock(&ts->secure_lock);
		return IRQ_HANDLED;
	}

	mutex_unlock(&ts->secure_lock);
	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lxs_ts_data *ts = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct lxs_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);
		
		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		lxs_ts_irq_enable(ts, false);

		/* Release All Finger */

		if (pm_runtime_get_sync(ts->client->controller->dev.parent) < 0) {
			enable_irq(ts->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);

		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		lxs_ts_irq_enable(ts, true);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);

	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(ts->client->controller->dev.parent);
		atomic_set(&ts->secure_enabled, 0);

		sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		complete(&ts->secure_interrupt);
		complete_all(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

		lxs_ts_pinctrl_configure(ts, true);

	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lxs_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	mutex_lock(&ts->secure_lock);
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		mutex_unlock(&ts->secure_lock);
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		mutex_unlock(&ts->secure_lock);
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));
	}

	mutex_unlock(&ts->secure_lock);
	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

static int secure_touch_init(struct lxs_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	return 0;
}

static void secure_touch_stop(struct lxs_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);

		sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);
static DEVICE_ATTR(secure_ownership, S_IRUGO, secure_ownership_show, NULL);
static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};
#endif

u32 t_dev_dbg_mask;
module_param_named(dev_dbg_mask, t_dev_dbg_mask, uint, S_IRUGO|S_IWUSR|S_IWGRP);

#if IS_ENABLED(CONFIG_MTK_SPI)
const struct mt_chip_conf __spi_conf = {
	.setuptime = 25,
	.holdtime = 25,
	.high_time = 5,	/* 10MHz (SPI_SPEED=100M / (high_time+low_time(10ns)))*/
	.low_time = 5,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,
	.cpol = 0,
	.cpha = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = DMA_TRANSFER,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#elif IS_ENABLED(CONFIG_SPI_MT65XX)
/*
const struct mtk_chip_config __spi_config = {
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .cs_pol = 0,
};
*/
#endif

#if defined(__SUPPORT_SAMSUNG_TUI)
static struct lxs_ts_data *tsp_info;

extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

int stui_tsp_enter(void)
{
	struct lxs_ts_data *ts = tsp_info;
	int ret = 0;

	if (!ts)
		return -EINVAL;

	lxs_ts_irq_enable(ts, false);

	ret = stui_spi_lock(ts->client->master);
	if (ret) {
		pr_err("[STUI] stui_spi_lock failed, %d\n", ret);
		lxs_ts_irq_enable(ts, true);
		return -1;
	}

	return 0;
}

int stui_tsp_exit(void)
{
	struct lxs_ts_data *ts = tsp_info;
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_spi_unlock(ts->client->master);
	if (ret) {
		pr_err("[STUI] stui_spi_unlock failed, %d\n", ret);
	}

	lxs_ts_irq_enable(ts, true);

	return ret;
}
#endif	/* __SUPPORT_SAMSUNG_TUI */

int lxs_ts_gpio_reset(struct lxs_ts_data *ts, bool on)
{
	int ret = 0;

#if defined(USE_HW_RST)
	if (!ts->regulator_tsp_reset) {
		gpio_set_value(ts->plat_data->reset_gpio, !!on);
		if (!on) {
			atomic_set(&ts->init, TC_IC_INIT_NEED);
		//	atomic_set(&ts->fwsts, 0);
		}

		input_info(true, &ts->client->dev, "%s: gpio_set_value(%d, %d)\n",
			__func__, ts->plat_data->reset_gpio, !!on);
	} else {
		lxs_ts_tsp_reset_ctrl(ts, !!on);
	}
#else
	if (on)
		lxs_ts_sw_reset(ts);
	else
		atomic_set(&ts->init, TC_IC_INIT_NEED);
#endif

	return ret;
}

int lxs_ts_pinctrl_configure(struct lxs_ts_data *ts, bool on)
{
	struct pinctrl_state *state;

	if (IS_ERR_OR_NULL(ts->pinctrl))
		return 0;

	if (on) {
		state = pinctrl_lookup_state(ts->pinctrl, "on_state");
	} else {
		state = pinctrl_lookup_state(ts->pinctrl, "off_state");
	}

	if (IS_ERR_OR_NULL(state))
		return 0;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, on ? "ACTIVE" : "SUSPEND");

	return pinctrl_select_state(ts->pinctrl, state);
}

static int lxs_ts_bus_init(struct lxs_ts_data *ts)
{
	char *tx_buf;
	int size = BUS_BUF_MAX_SZ + BUS_BUF_MARGIN;

	ts->buf_size = size;

	tx_buf = kzalloc(size<<1, GFP_KERNEL | GFP_DMA);
	if (tx_buf == NULL) {
		input_err(true, &ts->client->dev, "%s\n", "failed to allocate bus buffer");
		return -ENOMEM;
	}

	ts->tx_buf = tx_buf;
	ts->rx_buf = tx_buf + size;

	input_info(true, &ts->client->dev, "bus buffer allocated, 0x%X\n", size);

	return 0;
}

static void lxs_ts_bus_free(struct lxs_ts_data *ts)
{
	if (ts->tx_buf) {
		kfree(ts->tx_buf);
		ts->tx_buf = NULL;
		ts->rx_buf = NULL;
	}
}

static int __lxs_ts_spi_reg_read(struct lxs_ts_data *ts, u32 addr, void *data, int size)
{
	int max_size = ts->buf_size;
	int tx_hdr_size = SPI_TX_HDR_SZ;
	int rx_hdr_size = SPI_RX_HDR_SZ;
	int rx_dummy_size = SPI_RX_HDR_DUMMY;
	int tx_size = tx_hdr_size;
	u8 *tx_buf = ts->tx_buf;
	u8 *rx_buf = ts->rx_buf;
	struct spi_message *m;
	struct spi_transfer *msg;
	char result[32];
	int retry = 0;
	int ret = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: secure touch is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
	if (!ts->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if defined(__SUPPORT_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (!tx_buf || !rx_buf) {
		input_err(true, &ts->client->dev, "%s: spi rd: buffer NULL\n", __func__);
		return -ENOMEM;
	}

	if (size > max_size) {
		input_err(true, &ts->client->dev, "%s: spi rd: buffer overflow - rx %Xh\n", __func__, size);
		return -EOVERFLOW;
	}

	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	msg = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!msg) {
		kfree(m);
		return -ENOMEM;
	}

	tx_buf[0] = (size > 4) ? 0x20 : 0x00;
	tx_buf[0] |= ((addr >> 8) & 0x0f);
	tx_buf[1] = (addr & 0xff);

	while (rx_dummy_size--)
		tx_buf[tx_size++] = 0;

	spi_message_init(m);

	msg->tx_buf = tx_buf,
	msg->rx_buf = rx_buf,
	msg->len = rx_hdr_size + size,
	msg->bits_per_word = SPI_BPW,
	msg->speed_hz = SPI_SPEED_HZ,

	spi_message_add_tail(msg, m);

	for (retry = 0; retry < SPI_RETRY_CNT; retry++) {
		if (ts->plat_data->cs_gpio > 0) {
			gpio_direction_output(ts->plat_data->cs_gpio, 0);
			ret = spi_sync(ts->client, m);
			gpio_direction_output(ts->plat_data->cs_gpio, 1);
		} else {
			ret = spi_sync(ts->client, m);
		}
		if (!ret)
			break;

		input_err(true, &ts->client->dev, "read reg error(0x%04X, 0x%04X), %d, retry %d\n",
			(u32)addr, (u32)size, ret, retry + 1);

		if (ts->sec.fac_dev) {
			snprintf(result, sizeof(result), "RESULT=SPI");
			sec_cmd_send_event_to_user(&ts->sec, NULL, result);
		}

		usleep_range(1 * 1000, 1 * 1000);
	}

	if (retry == SPI_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: SPI read over retry limit\n", __func__);
		ret = -EIO;
		/*
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
		*/
	}

	memcpy(data, &rx_buf[rx_hdr_size], size);
	kfree(msg);
	kfree(m);

	return ret;
}

static int __lxs_ts_spi_reg_write(struct lxs_ts_data *ts, u32 addr, void *data, int size)
{
	int max_size = ts->buf_size;
	int tx_hdr_size = SPI_TX_HDR_SZ;
	int tx_size = tx_hdr_size + size;
	u8 *tx_buf = ts->tx_buf;
	struct spi_message *m;
	struct spi_transfer *msg;
	char result[32];
	int retry = 0;
	int ret = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: secure touch is enabled!\n", __func__);
		return -EBUSY;
	}
#endif

	if (!ts->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if defined(__SUPPORT_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (!tx_buf) {
		input_err(true, &ts->client->dev, "%s: spi wr: buffer NULL\n", __func__);
		return -ENOMEM;
	}

	if (tx_size > max_size) {
		input_err(true, &ts->client->dev, "%s: spi wr: buffer overflow - tx %Xh\n", __func__, tx_size);
		return -EOVERFLOW;
	}

	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	msg = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!msg) {
		kfree(m);
		return -ENOMEM;
	}

	tx_buf = ts->tx_buf;

	tx_buf[0] = 0x60;
	tx_buf[0] |= ((addr >> 8) & 0x0f);
	tx_buf[1] = (addr & 0xff);

	memcpy(&tx_buf[tx_hdr_size], data, size);

	spi_message_init(m);

	msg->tx_buf = tx_buf,
	msg->len = tx_size,
	msg->bits_per_word = SPI_BPW,
	msg->speed_hz = SPI_SPEED_HZ,

	spi_message_add_tail(msg, m);

	for (retry = 0; retry < SPI_RETRY_CNT; retry++) {
		if (ts->plat_data->cs_gpio > 0) {
			gpio_direction_output(ts->plat_data->cs_gpio, 0);
			ret = spi_sync(ts->client, m);
			gpio_direction_output(ts->plat_data->cs_gpio, 1);
		} else {
			ret = spi_sync(ts->client, m);
		}
		if (!ret)
			break;

		input_err(true, &ts->client->dev, "write reg error(0x%04X, 0x%04X), %d, retry %d\n",
			(u32)addr, (u32)size, ret, retry + 1);

		if (ts->sec.fac_dev) {
			snprintf(result, sizeof(result), "RESULT=SPI");
			sec_cmd_send_event_to_user(&ts->sec, NULL, result);
		}

		usleep_range(1 * 1000, 1 * 1000);
	}

	if (retry == SPI_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: SPI write over retry limit\n", __func__);
		ret = -EIO;
		/*
		if (ts->probe_done && !ts->reset_is_on_going && !ts->plat_data->shutdown_called)
			schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
		*/
	}

	kfree(msg);
	kfree(m);

	return ret;
}

int lxs_ts_reg_read(struct lxs_ts_data *ts, u32 addr, void *data, int size)
{
	int ret = 0;

	if (!size)
		return 0;

	if (lxs_addr_is_skip(addr))
		return 0;

	if (!data) {
		input_err(true, &ts->client->dev, "rd: NULL data(0x%04X, 0x%04X)\n", addr, size);
		return -EFAULT;
	}

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "rd: POWER_STATUS : OFF, data(0x%04X, 0x%04X)\n", addr, size);
		return -EIO;
	}

	mutex_lock(&ts->bus_mutex);
	ret = __lxs_ts_spi_reg_read(ts, addr, data, size);
	mutex_unlock(&ts->bus_mutex);

	return ret;
}

int lxs_ts_reg_write(struct lxs_ts_data *ts, u32 addr, void *data, int size)
{
	int ret = 0;

	if (!size)
		return 0;

	if (lxs_addr_is_skip(addr))
		return 0;

	if (!data) {
		input_err(true, &ts->client->dev, "rd: NULL data(0x%04X, 0x%04X)\n", addr, size);
		return -EFAULT;
	}

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "rd: POWER_STATUS : OFF, data(0x%04X, 0x%04X)\n", addr, size);
		return -EIO;
	}

	mutex_lock(&ts->bus_mutex);
	ret = __lxs_ts_spi_reg_write(ts, addr, data, size);
	mutex_unlock(&ts->bus_mutex);

	return ret;
}

int lxs_ts_read_value(struct lxs_ts_data *ts, u32 addr, u32 *value)
{
	return lxs_ts_reg_read(ts, addr, value, sizeof(u32));
}

int lxs_ts_write_value(struct lxs_ts_data *ts, u32 addr, u32 value)
{
	return lxs_ts_reg_write(ts, addr, &value, sizeof(u32));
}

static int __lxs_ts_reg_bit_mask(struct lxs_ts_data *ts, u32 addr, u32 *value, u32 mask, int set)
{
	const char *str = (set) ? "set" : "clr";
	u32 rdata = 0;
	u32 data = 0;
	int ret = 0;

	ret = lxs_ts_read_value(ts, addr, &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: bit %s failed, addr 0x%04X, mask 0x%08X\n",
			__func__, str, addr, mask);
		return ret;
	}

	rdata = data;

	if (set) {
		data |= mask;
	} else {
		data &= ~mask;
	}

	ret = lxs_ts_write_value(ts, addr, data);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: bit %s failed, addr 0x%04X, mask 0x%08X (0x%08X <- 0x%08X)\n",
			__func__, str, addr, mask, data, rdata);
		return ret;
	}

	if (value)
		*value = data;

	return 0;
}

int lxs_ts_reg_bit_set(struct lxs_ts_data *ts, u32 addr, u32 *value, u32 mask)
{
	return __lxs_ts_reg_bit_mask(ts, addr, value, mask, 1);
}

int lxs_ts_reg_bit_clr(struct lxs_ts_data *ts, u32 addr, u32 *value, u32 mask)
{
	return __lxs_ts_reg_bit_mask(ts, addr, value, mask, 0);
}

static int lxs_ts_chk_lp_state(struct lxs_ts_data *ts)
{
	int lowpower_mode = ts->lowpower_mode;

	if (ts->aot_enable || ts->spay_enable)
		if (ts->prox_power_off || ts->ear_detect_mode)
			lowpower_mode = 0;	// for ed

	if (lowpower_mode || ts->lcdoff_test) {
		input_info(true, &ts->client->dev, "%s: LP(AOT %d, SPAY %d)\n",
			__func__, ts->aot_enable, ts->spay_enable);
		return OP_STATE_LP;
	}

	if (ts->ear_detect_mode == 1) {
		if (ts->aot_enable || ts->spay_enable) {
			input_info(true, &ts->client->dev, "%s: LP(AOT %d, SPAY %d) & Prox 1\n",
				__func__, ts->aot_enable, ts->spay_enable);
			return OP_STATE_LP;
		}
	}

	if (ts->ear_detect_mode) {
		input_info(true, &ts->client->dev, "%s: Prox %d\n",
			__func__, ts->ear_detect_mode);
		return OP_STATE_PROX;
	}

	input_info(true, &ts->client->dev, "%s: Stop\n", __func__);
	return OP_STATE_STOP;
}

static void lxs_ts_trigger_reset(struct lxs_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	lxs_ts_gpio_reset(ts, false);

	lxs_ts_pinctrl_configure(ts, false);

	lxs_ts_delay(5);

	lxs_ts_pinctrl_configure(ts, true);

	lxs_ts_gpio_reset(ts, true);

	lxs_ts_delay(TOUCH_POWER_ON_DWORK_TIME);
}

static int lxs_ts_reinit(struct lxs_ts_data *ts)
{
	int i;
	int ret = 0;

	input_info(true, &ts->client->dev,
		"%s: charger=0x%x, Power mode=0x%x\n",
		__func__, ts->charger_mode, ts_get_power_state(ts));

	ts->touch_noise_status = 0;
	ts->touch_pre_noise_status = 0;
	ts->wet_mode = 0;

	atomic_set(&ts->init, TC_IC_INIT_NEED);

	for (i = 0; i < 2; i++) {
		ret = lxs_ts_ic_init(ts);
		if (ret >= 0)
			break;

		input_err(true, &ts->client->dev, "retry getting ic info (%d)\n", i);

		lxs_ts_trigger_reset(ts);
	}

//	lxs_ts_sync_set(ts, 1);

	return ret;
}

static void lxs_ts_read_info_work(struct work_struct *work)
{
	struct lxs_ts_data *ts = container_of(work, struct lxs_ts_data,
			work_read_info.work);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s done, do not run work\n", __func__);
		return;
	}

	if (work_busy(&ts->reset_work.work)) {
               input_info(true, &ts->client->dev, "%s: reset busy, delayed\n", __func__);
               schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(150));
               return;
	}

	lxs_ts_run_rawdata_all(ts);

	schedule_work(&ts->work_print_info.work);
}

static void lxs_ts_print_info(struct lxs_ts_data *ts)
{
	if (!ts)
		return;

	ts->print_info_cnt_open++;

	if (ts->print_info_cnt_open > 0xfff0)
		ts->print_info_cnt_open = 0;

	if (ts->touch_count == 0)
		ts->print_info_cnt_release++;

	input_info(true, &ts->client->dev,
		"tc:%d noise:%d Sensitivity:%d sip:%d game:%d // v:LX%08X, LX%04X  // irq:%d //#%d %d\n",
		ts->touch_count, ts->touch_noise_status, ts->sensitivity_mode, ts->sip_mode,
		ts->game_mode, ts->fw.ver_code_dev, ts->fw.ver_code_dev >> 16,		
		gpio_get_value(ts->plat_data->irq_gpio), ts->print_info_cnt_open, ts->print_info_cnt_release);
}


static void lxs_ts_print_info_work(struct work_struct *work)
{
	struct lxs_ts_data *ts = container_of(work, struct lxs_ts_data,
			work_print_info.work);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s done, do not run work\n", __func__);
		return;
	}
	lxs_ts_print_info(ts);

	schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

static int lxs_ts_start_device(struct lxs_ts_data *ts)
{
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	if (ts_get_power_state(ts) != LP_MODE_EXIT)
		lxs_ts_pinctrl_configure(ts, true);

	mutex_lock(&ts->device_mutex);

	if (is_ts_power_state_on(ts)) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	lxs_ts_locked_release_finger(ts);

	ts_set_power_state_on(ts);
	lxs_ts_gpio_reset(ts, true);

	lxs_ts_delay(TOUCH_POWER_ON_DWORK_TIME);

	ts->op_state = OP_STATE_NP;
	ret = lxs_ts_reinit(ts);
	if (ret < 0)
		goto out;

	if (atomic_read(&ts->init) == TC_IC_INIT_DONE)
		lxs_ts_irq_enable(ts, true);

out:
	mutex_unlock(&ts->device_mutex);

	return 0;
}

static int lxs_ts_stop_device(struct lxs_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	lxs_ts_irq_enable(ts, false);

	ts->op_state = OP_STATE_STOP;
	lxs_ts_touch_off(ts);

	lxs_ts_locked_release_finger(ts);

	lxs_ts_gpio_reset(ts, false);
	ts_set_power_state_off(ts);

	lxs_ts_pinctrl_configure(ts, false);

out:
	mutex_unlock(&ts->device_mutex);

	return 0;
}

#if defined(USE_LOWPOWER_MODE)
static int lxs_ts_lpm_device(struct lxs_ts_data *ts)
{
	int ret = 0;

	input_info(true, &ts->client->dev,
		"%s: ED %d, Prox_power_off %d, LPM 0x%X\n",
		__func__, ts->ear_detect_mode,
		ts->prox_power_off, ts->lowpower_mode);

	ts_set_power_state_lpm(ts);

	ret = lxs_ts_ic_lpm(ts);

	return ret;
}
#endif

static void lxs_ts_reset_retry(struct lxs_ts_data *ts)
{
#if defined(USE_RESET_RETRY_MAX)
	ts->reset_retry++;
	if (ts->reset_retry < TC_RESET_RETRY)
		lxs_ts_ic_reset(ts);
	else
		input_err(true, &ts->client->dev, "%s: reset retry halted, %d\n",
			__func__, ts->reset_retry);
#else
	lxs_ts_ic_reset(ts);
#endif
}

static void lxs_ts_reset_work(struct work_struct *work)
{
	struct lxs_ts_data *ts = container_of(work, struct lxs_ts_data,
			reset_work.work);
	char result[32];
	char test[32];
#if defined(USE_TOUCH_AUTO_START)
	static bool boot_on = true;
#else
	static bool boot_on;
#endif
	int ret = 0;

	if (ts->shutdown_called)
		return;

	if (ts->reset_is_on_going) {
		input_info(true, &ts->client->dev, "%s: reset is ongoing\n", __func__);
		return;
	}

	if (is_ts_power_state_off(ts)) {
		input_info(true, &ts->client->dev, "%s: skipped, stop state\n", __func__);
		return;
	}

	if (!boot_on) {
		cancel_delayed_work(&ts->reset_work);

		if (is_ts_power_state_lpm(ts))
			lxs_ts_irq_wake(ts, false);

		lxs_ts_irq_enable(ts, false);

		lxs_ts_locked_release_finger(ts);

		lxs_ts_trigger_reset(ts);
	}
	boot_on = false;

	mutex_lock(&ts->modechange);

	pm_stay_awake(&ts->client->dev);
	ts->reset_is_on_going = true;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	switch (ts->op_state) {
	case OP_STATE_PROX:
		/* TBD */
		break;
	default:
		lxs_ts_init_recon(ts);
		ret = lxs_ts_reinit(ts);
		break;
	}
	if (ret < 0) {
		if (ts->sec.fac_dev) {
			snprintf(test, sizeof(test), "TEST=RECOVERY");
			snprintf(result, sizeof(result), "RESULT=FAIL");
			sec_cmd_send_event_to_user(&ts->sec, test, result);
		}

		input_err(true, &ts->client->dev, "%s: failed to reset, %d\n", __func__, ret);
		ts->reset_is_on_going = false;
		cancel_delayed_work(&ts->reset_work);
		mutex_unlock(&ts->modechange);

		if (ts->sec.fac_dev) {
			snprintf(result, sizeof(result), "RESULT=RESET");
			sec_cmd_send_event_to_user(&ts->sec, NULL, result);
		}

		pm_relax(&ts->client->dev);

		lxs_ts_reset_retry(ts);
		return;
	}

	ts->reset_is_on_going = false;

	if (atomic_read(&ts->init) == TC_IC_INIT_DONE) {
		ts->reset_retry = 0;

		if (is_ts_power_state_lpm(ts))
			lxs_ts_irq_wake(ts, true);

		if (!is_ts_power_state_off(ts))
			lxs_ts_irq_enable(ts, true);
	}

	mutex_unlock(&ts->modechange);

	if (ts->sec.fac_dev) {
		snprintf(result, sizeof(result), "RESULT=RESET");
		sec_cmd_send_event_to_user(&ts->sec, NULL, result);
	}

	pm_relax(&ts->client->dev);

	lxs_ts_prd_init(ts);

#if 0
	if (!ts->shutdown_called)
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(50));
#endif
}

void lxs_ts_ic_reset(struct lxs_ts_data *ts)
{
	if (!ts->reset_is_on_going)
		schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
}

void lxs_ts_ic_reset_sync(struct lxs_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	if (!ts->reset_is_on_going)
		lxs_ts_reset_work(&ts->reset_work.work);
}

#if defined(USE_VBUS_NOTIFIER)
static int lxs_ts_vbus_chk_otg(struct lxs_ts_data *ts)
{
	union power_supply_propval val = {0, };
	/* Ref. : {kernel top}/drivers/battery/common/sec_battery.c */
	char *sec_bat_status_str[] = {
		"Unknown",
		"Charging",
		"Discharging",
		"Not-charging",
		"Full"
	};
	char *sec_cable_type[] = {
		"UNKNOWN",		/* 0 */
		"NONE",			/* 1 */
		"PREPARE_TA",		/* 2 */
		"TA",			/* 3 */
		"USB",			/* 4 */
		"USB_CDP",		/* 5 */
		"9V_TA",		/* 6 */
		"9V_ERR",		/* 7 */
		"9V_UNKNOWN",		/* 8 */
		"12V_TA",		/* 9 */
		"WC",			/* 10 */
		"HV_WC",		/* 11 */
		"PMA_WC",		/* 12 */
		"WC_PACK",		/* 13 */
		"WC_HV_PACK",		/* 14 */
		"WC_STAND",		/* 15 */
		"WC_HV_STAND",		/* 16 */
		"OC20",			/* 17 */
		"QC30",			/* 18 */
		"PDIC",			/* 19 */
		"UARTOFF",		/* 20 */
		"OTG",			/* 21 */
		"LAN_HUB",		/* 22 */
		"POWER_SHARGING",	/* 23 */
		"HMT_CONNECTED",	/* 24 */
		"HMT_CHARGE",		/* 25 */
		"HV_TA_CHG_LIMIT",	/* 26 */
		"WC_VEHICLE",		/* 27 */
		"WC_HV_VEHICLE",	/* 28 */
		"WC_HV_PREPARE",	/* 29 */
		"TIMEOUT",		/* 30 */
		"SMART_OTG",		/* 31 */
		"SMART_NOTG",		/* 32 */
		"WC_TX",		/* 33 */
		"HV_WC_20",		/* 34 */
		"HV_WC_20_LIMIT",	/* 35 */
		"WC_FAKE",		/* 36 */
		"HV_WC_20_PREPARE",	/* 37 */
		"PDIC_APDO",		/* 38 */
		"POGO",			/* 39 */
	};
	int status_batt, capa_batt;
	int online_batt, online_usb, online_otg;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, val);
	status_batt = val.intval;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_CAPACITY, val);
	capa_batt = val.intval;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, val);
	online_batt = val.intval;

	psy_do_property("usb", get, POWER_SUPPLY_PROP_ONLINE, val);
	online_usb = val.intval;

	psy_do_property("otg", get, POWER_SUPPLY_PROP_ONLINE, val);
	online_otg = val.intval;

	input_info(true, &ts->client->dev,
		"%s: battery %s(%d) %s(%d) %d%%, usb %sconnected, otg %sconnected\n",
		__func__,
		sec_bat_status_str[status_batt], status_batt,
		sec_cable_type[online_batt], online_batt,
		capa_batt,
		online_usb ? "" : "dis",
		online_otg ? "" : "dis");

	return online_otg;
}

static void lxs_ts_vbus_work(struct work_struct *work)
{
	struct lxs_ts_data *ts = container_of(work, struct lxs_ts_data,
				vbus_work.work);

	if (ts->shutdown_called)
		return;

	mutex_lock(&ts->device_mutex);

	switch (ts->vbus_type) {
	case STATUS_VBUS_HIGH:
		if (lxs_ts_vbus_chk_otg(ts))
			goto out;

		ts->charger_mode |= LXS_TS_BIT_CHARGER_MODE_WIRE_CHARGER;
		break;
	case STATUS_VBUS_LOW:
		if (!(ts->charger_mode & LXS_TS_BIT_CHARGER_MODE_WIRE_CHARGER))
			goto out;

		ts->charger_mode &= ~LXS_TS_BIT_CHARGER_MODE_WIRE_CHARGER;
		break;
	default:
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: %sconnected\n", __func__,
		(ts->charger_mode & LXS_TS_BIT_CHARGER_MODE_WIRE_CHARGER) ? "" : "dis");

	lxs_ts_set_charger(ts);

out:
	mutex_unlock(&ts->device_mutex);
}

static int lxs_ts_vbus_notification(struct notifier_block *nb, unsigned long cmd, void *data)
{
	struct lxs_ts_data *ts = container_of(nb, struct lxs_ts_data, vbus_nb);
	int vbus_type = (data) ? *(int *)data : 0;
	u32 delay;

	if (ts->shutdown_called)
		return 0;

	delay = (vbus_type == STATUS_VBUS_HIGH) ? 300 : 10;

	ts->vbus_type = vbus_type;

	schedule_delayed_work(&ts->vbus_work, msecs_to_jiffies(delay));

	return 0;
}
#endif

static void lxs_ts_external_func(struct lxs_ts_data *ts)
{
	/* TBD */
}

static const struct lxs_ts_status_filter __status_filter[] = {
	_STS_FILTER(STS_ID_VALID_DEV_CTL, 1, STS_POS_VALID_DEV_CTL,
		0, "device ctl not set"),
	_STS_FILTER(STS_ID_VALID_CODE_CRC, 1, STS_POS_VALID_CODE_CRC,
		0, "code crc invalid"),
	_STS_FILTER(STS_ID_ERROR_ABNORMAL, 1, STS_POS_ERROR_ABNORMAL,
		STS_FILTER_FLAG_TYPE_ERROR,
		"abnormal status detected"),
	_STS_FILTER(STS_ID_ERROR_SYSTEM, 1, STS_POS_ERROR_SYSTEM,
		STS_FILTER_FLAG_TYPE_ERROR,
		"system error detected"),
	_STS_FILTER(STS_ID_VALID_IRQ_PIN, 1, STS_POS_VALID_IRQ_PIN,
		0, "irq pin invalid"),
	_STS_FILTER(STS_ID_VALID_IRQ_EN, 1, STS_POS_VALID_IRQ_EN,
		0, "irq status invalid"),
	_STS_FILTER(STS_ID_VALID_TC_DRV, 1, STS_POS_VALID_TC_DRV,
		0, "driving invalid"),
	/* end mask */
	_STS_FILTER(STS_ID_NONE, 0, 0, 0, NULL),
};

static int lxs_ts_read_report(struct lxs_ts_data *ts)
{
	char *buf = (char *)&ts->report;
	u32 addr = IC_STATUS;
	int size = 0;
	int ret = 0;

	size = (TC_REPORT_BASE_HDR<<2);
	size += (sizeof(struct lxs_ts_touch_data) * MAX_FINGER);

	ret = lxs_ts_reg_read(ts, addr, buf, size);
	if (ret < 0)
		return ret;

	return 0;
}

static void lxs_ts_check_dbg_report(struct lxs_ts_data *ts)
{
	u32 tc_debug[4] = {0, };
	int ret = 0;

	ret = lxs_ts_reg_read(ts, TC_DBG_REPORT, tc_debug, sizeof(tc_debug));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get TC_DBG_REPORT, %d\n", __func__, ret);
		return;
	}

	input_info(true, &ts->client->dev, "%s: 0x%08X 0x%08X 0x%08X 0x%08X\n",
		__func__, tc_debug[0], tc_debug[1], tc_debug[2], tc_debug[3]);
}

static int lxs_ts_check_status_sub(struct lxs_ts_data *ts, u32 status, u32 ic_status)
{
	const struct lxs_ts_status_filter *filter = __status_filter;
	u32 check_mask, detected;
	int type_error;
	u32 irq_type = tc_sts_irq_type(status);
	u32 log_flag = 0;
	u32 ic_abnormal, ic_error;
	int log_max = IC_CHK_LOG_MAX;
	char log[IC_CHK_LOG_MAX] = {0, };
	int abnormal_type = 0;
	int esd_irq = 0;
	int len = 0;
	int ret = 0;

	if (ic_status & ~IC_STATUS_MASK_VALID) {
		input_err(true, &ts->client->dev, "%s: status %08Xh, ic_status %08Xh, ic_status invalid\n",
			__func__, status, ic_status);
		return -ERESTART;
	}

	if (ts->dbg_mask & DBG_LOG_IRQ) {
		input_info(true, &ts->client->dev, "%s: status %08Xh, ic_status %08Xh\n",
			__func__, status, ic_status);
	}

	while (1) {
		if (!filter->id || !filter->width)
			break;

		type_error = !!(filter->flag & STS_FILTER_FLAG_TYPE_ERROR);

		check_mask = ((1<<filter->width)-1)<<filter->pos;

		detected = (type_error) ? (status & check_mask) : !(status & check_mask);

		if (check_mask && detected) {
			log_flag |= check_mask;

			len += sts_snprintf(log, log_max, len,
						"[b%d] %s ", filter->pos, filter->str);

			if (filter->id == STS_ID_ERROR_ABNORMAL) {
				abnormal_type = tc_sts_abnormal_type(status);

				switch (abnormal_type) {
				case STS_AB_ESD_DIC:
					esd_irq = 1;
					break;
				case STS_AB_ESD_DSI:
					esd_irq = 2;
					break;
				}
			}
		}

		filter++;
	}

	if (log_flag)
		input_err(true, &ts->client->dev, "status %08Xh, ic_status %08Xh, (%08Xh) %s\n",
			status, ic_status, log_flag, log);

	ic_abnormal = ic_status & ts->ic_status_mask_abnormal;
	ic_error = ic_status & ts->ic_status_mask_error;

	if (ic_abnormal || ic_error) {
		u32 err_val[3] = { ic_abnormal, ic_error };
		char *err_str_oled[3] = {
			"spck",
			"watchdog",
			"dic"
		};
		char **err_str = err_str_oled;
		int log_add = !log_flag;
		int err_pre, i;

		err_pre = 0;
		for (i = 0; i < ARRAY_SIZE(err_val) ; i++) {
			if (!err_val[i])
				continue;

			if (err_pre)
				len += sts_snprintf(log, log_max, len, " & ");

			len += sts_snprintf(log, log_max, len, "%s", err_str[i]);
			err_pre |= err_val[i];
		}

		if (log_add) {
			len += sts_snprintf(log, log_max, len,
						" - status %08Xh, ic_status %08Xh",
						status, ic_status);
		}

		input_err(true, &ts->client->dev, "%s\n", log);

		ret = -ERESTART;
	}

	if (abnormal_type) {
		input_err(true, &ts->client->dev,
			"abnormal detected, type %d\n", abnormal_type);
		if (esd_irq)
			ret = -ETESDIRQ;
	}

	switch (irq_type) {
	case TC_STS_IRQ_TYPE_ABNORMAL:
	case TC_STS_IRQ_TYPE_DEBUG:
		lxs_ts_check_dbg_report(ts);
		if (!ret)
			ret = -ERANGE;
		return ret;
	}

	if (ret == -ERESTART)
		return ret;

	/*
	 * Check interrupt_type[19:16] in TC_STATUS
	 */
	switch (irq_type) {
	case TC_STS_IRQ_TYPE_INIT_DONE:
		input_info(true, &ts->client->dev, "%s\n", "TC Driving OK");
		ret = -ERANGE;
		break;
	case TC_STS_IRQ_TYPE_REPORT:	/* Touch report */
		break;
	default:
		input_dbg(true, &ts->client->dev, "irq_type %Xh\n", irq_type);
		ret = -ERANGE;
		break;
	}

	return ret;
}

static int lxs_ts_check_status(struct lxs_ts_data *ts)
{
	u32 ic_status = ts->report.ic_status;
	u32 status = ts->report.device_status;
	u32 reset_clr_bit = 0;
	u32 logging_clr_bit = 0;
	u32 int_norm_mask = 0;
	u32 status_mask = 0;
	int ret_pre = 0;
	int ret = 0;

	if (!status && !ic_status) {
		input_err(true, &ts->client->dev, "%s\n", "all low detected");
		return -STS_RET_ERR;
	}

	if ((status == ~0) && (ic_status == ~0)) {
		input_err(true, &ts->client->dev, "%s\n", "all high detected");
		return -STS_RET_ERR;
	}

	reset_clr_bit = STATUS_MASK_RESET;
	logging_clr_bit = STATUS_MASK_LOGGING;
	int_norm_mask = STATUS_MASK_NORMAL;

	status_mask = status ^ int_norm_mask;

	if (status_mask & reset_clr_bit) {
		input_err(true, &ts->client->dev,
			"need reset : status %08Xh, ic_status %08Xh, chk %08Xh (%08Xh)\n",
			status, ic_status, status_mask & reset_clr_bit, reset_clr_bit);
		ret_pre = -ERESTART;
	} else if (status_mask & logging_clr_bit) {
		input_err(true, &ts->client->dev,
			"need logging : status %08Xh, ic_status %08Xh, chk %08Xh (%08Xh)\n",
			status, ic_status, status_mask & logging_clr_bit, logging_clr_bit);
		ret_pre = -ERANGE;
	}

	ret = lxs_ts_check_status_sub(ts, status, ic_status);

	if (ret == -ETESDIRQ)
		return ret;

	if (ret_pre) {
		if (ret != -ERESTART)
			ret = ret_pre;
	}

	return ret;
}

static int lxs_ts_parse_abs_palm(struct lxs_ts_data *ts, int track_id, int event)
{
	if (ts->report.palm_bit) {
		input_info(true, &ts->client->dev, "%s: Palm(bit) Detected\n", __func__);
		goto out;
	}

	if (track_id != PALM_ID)
		return 0;

	if (event == TOUCHSTS_DOWN)
		input_info(true, &ts->client->dev, "%s\n", "Palm Detected");
	else if (event == TOUCHSTS_UP)
		input_info(true, &ts->client->dev, "%s\n", "Palm Released");

out:
	ts->tcount = 0;
	ts->intr_status |= TS_TOUCH_IRQ_FINGER;

	return PALM_DETECTED;
}

static int lxs_ts_parse_abs_finger(struct lxs_ts_data *ts)
{
	struct lxs_ts_input_touch_data *tfinger = NULL;
	struct lxs_ts_touch_data *data = ts->report.data;
	u32 touch_count = ts->report.touch_cnt;
	int finger_index = 0;
	int i = 0;
	int ret = 0;

	ts->new_mask = 0;

	ts->noise_mode = ts->report.noise_mode;
	ts->noise_freq = ts->report.noise_freq;
	ts->noise_meas = ts->report.noise_meas;

	ret = lxs_ts_parse_abs_palm(ts, data->track_id, data->event);
	if (ret == PALM_DETECTED)
		goto out;

	for (i = 0; i < touch_count; i++, data++) {
		if (data->track_id >= MAX_FINGER)
			continue;

		if ((data->event == TOUCHSTS_DOWN) ||
			(data->event == TOUCHSTS_MOVE)) {
			ts->new_mask |= BIT(data->track_id);
			tfinger = &ts->tfinger[data->track_id];

			tfinger->id = data->track_id;
			tfinger->type = data->tool_type;
			tfinger->event = data->event;
			tfinger->x = data->x;
			tfinger->y = data->y;
			tfinger->pressure = data->pressure;
			tfinger->width_major = data->width_major;
			tfinger->width_minor = data->width_minor;
			tfinger->orientation = (s8)data->angle;

			finger_index++;
		}
	}

	ts->tcount = finger_index;
	ts->intr_status |= TS_TOUCH_IRQ_FINGER;

out:
	return ret;
}

static int lxs_ts_parse_abs(struct lxs_ts_data *ts)
{
	u32 touch_count = ts->report.touch_cnt;
	int ret = 0;

	if (touch_count > MAX_FINGER)
		return -ERANGE;

	if (!touch_count)
		return -ERANGE;

	ret = lxs_ts_parse_abs_finger(ts);
	if (ret < 0)
		return ret;

	return 0;
}

static int lxs_ts_parse_gesture(struct lxs_ts_data *ts)
{
	u32 type = ts->report.wakeup_type;
	u32 rdata[8] = { 0, };
	int i;

	switch (type) {
	case KNOCK:
		memcpy(&rdata, ts->report.data, sizeof(u32) * TS_MAX_LPWG_CODE);

		for (i = 0; i < TS_MAX_LPWG_CODE; i++) {
			ts->knock[i].x = rdata[i] & 0xFFFF;
			ts->knock[i].y = (rdata[i] >> 16) & 0xFFFF;
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev,
				"%s: KNOCK data %d, %d\n", __func__,
				ts->knock[i].x, ts->knock[i].y);
#else
			input_info(true, &ts->client->dev,
				"%s: KNOCK data \n", __func__);
#endif
		}

		ts->intr_status = TS_TOUCH_IRQ_KNOCK;
		break;
	case SWIPE:
		memcpy(&rdata, ts->report.data, sizeof(u32) * (TS_MAX_SWIPE_CODE + 1));

		for (i = 0; i < TS_MAX_SWIPE_CODE; i++) {
			ts->gesture_id = SPONGE_EVENT_TYPE_SPAY;
			ts->swipe[i].x = rdata[i] & 0xFFFF;
			ts->swipe[i].y = (rdata[i] >> 16) & 0xFFFF;
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev,
				"%s: SWIPE data %d, %d\n", __func__,
				ts->swipe[i].x, ts->swipe[i].y);
#else
			input_info(true, &ts->client->dev,
				"%s: SWIPE data\n", __func__);
#endif
		}
		ts->swipe_time = rdata[i] & 0xFFFF;
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &ts->client->dev,
			"%s: SWIPE time %d\n", __func__,
			ts->swipe_time);
#endif

		ts->intr_status = TS_TOUCH_IRQ_GESTURE;
		break;
	}

	return 0;
}

static int lxs_ts_parse_prox(struct lxs_ts_data *ts)
{
	u32 type = ts->report.wakeup_type;

	ts->prox_state = ts->report.prox_sts;
	input_info(true, &ts->client->dev,
		"%s: [%d] PROX state %d\n", __func__, type, ts->prox_state);
	ts->intr_status |= TS_TOUCH_IRQ_PROX;

	return 0;
}

static int lxs_ts_read_event(struct lxs_ts_data *ts)
{
	int wakeup_type = 0;
	int touch_cnt = 0;
	int is_abs = 0;
	int is_prox = 0;
	int ret = 0;

	ts->intr_status = 0;

	if (atomic_read(&ts->init) == TC_IC_INIT_NEED) {
		input_err(true, &ts->client->dev, "%s\n", "Not Ready, Need IC init (irq)");
		return 0;
	}

	ret = lxs_ts_read_report(ts);
	if (ret < 0)
		return ret;

	ret = lxs_ts_check_status(ts);
	if (ret < 0)
		return ret;

	wakeup_type = ts->report.wakeup_type;
	touch_cnt = ts->report.touch_cnt;

	if (ts->dbg_mask & DBG_LOG_IRQ) {
		input_info(true, &ts->client->dev, "%s: wakeup_type %d, touch_cnt %d, prox_sts %d\n",
			__func__, wakeup_type, touch_cnt, ts->report.prox_sts);
	}

	switch (wakeup_type) {
	case PROX_ABS:
		is_abs = 1;
		is_prox = 1;
		break;
	case PROX:
		is_prox = 1;
		break;
	case ABS_MODE:
		is_abs = 1;
		break;
	}

	if (is_abs) {
		ret = lxs_ts_parse_abs(ts);
		if (ret) {
			input_err(true, &ts->client->dev,
				"[%d] lxs_ts_parse_abs failed(%d), %d", wakeup_type, touch_cnt, ret);
			return ret;;
		}
	}

	if (is_prox)
		lxs_ts_parse_prox(ts);

	if (!is_abs && !is_prox)
		lxs_ts_parse_gesture(ts);

	return 0;
}

#define LXS_TS_LOCATION_DETECT_SIZE	6
/************************************************************
*  720  * 1480 : <48 96 60> indicator: 24dp navigator:48dp edge:60px dpi=320
* 1080  * 2220 :  4096 * 4096 : <133 266 341>  (approximately value)
************************************************************/

static void location_detect(struct lxs_ts_data *ts, char *loc, int x, int y)
{
	int i;

	for (i = 0; i < LXS_TS_LOCATION_DETECT_SIZE; ++i)
		loc[i] = 0;

	if (x < ts->plat_data->area_edge)
		strcat(loc, "E.");
	else if (x < (ts->plat_data->max_x - ts->plat_data->area_edge))
		strcat(loc, "C.");
	else
		strcat(loc, "e.");

	if (y < ts->plat_data->area_indicator)
		strcat(loc, "S");
	else if (y < (ts->plat_data->max_y - ts->plat_data->area_navigation))
		strcat(loc, "C");
	else
		strcat(loc, "N");
}

static void lxs_ts_event_ctrl_finger(struct lxs_ts_data *ts, int release_all)
{
	struct lxs_ts_input_touch_data *tfinger = ts->tfinger;
	struct input_dev *input = ts->input_dev;
	struct device *idev = &input->dev;
	u32 old_mask = ts->old_mask;
	u32 new_mask = ts->new_mask;
	u32 press_mask = 0;
	u32 release_mask = 0;
	u32 change_mask = 0;
	int is_finger = !!(ts->intr_status & TS_TOUCH_IRQ_FINGER);
	int tcount = ts->tcount;
	int i;
	char location[LXS_TS_LOCATION_DETECT_SIZE] = { 0, };
	static int mc[MAX_FINGER] = {0, };
	static int pre_x[MAX_FINGER] = {0, };
	static int pre_y[MAX_FINGER] = {0, };

	if (!input)
		return;

	if (!is_finger)
		return;

	change_mask = old_mask ^ new_mask;
	press_mask = new_mask & change_mask;
	release_mask = old_mask & change_mask;

	for (i = 0; i < MAX_FINGER; i++, tfinger++) {
		location_detect(ts, location, tfinger->x, tfinger->y);
		if (new_mask & (1 << i)) {
			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, 1);
			input_report_key(input, BTN_TOUCH, 1);
			input_report_key(input, BTN_TOOL_FINGER, 1);
			input_report_abs(input, ABS_MT_POSITION_X, tfinger->x);
			input_report_abs(input, ABS_MT_POSITION_Y, tfinger->y);
			input_report_abs(input, ABS_MT_PRESSURE, tfinger->pressure);
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, tfinger->width_major);
			input_report_abs(input, ABS_MT_TOUCH_MINOR, tfinger->width_minor);
			input_report_abs(input, ABS_MT_ORIENTATION, tfinger->orientation);

			if (press_mask & (1 << i)) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &ts->client->dev, "[P] tId:%d,%d x:%d y:%d major:%d minor:%d loc:%s tc:%d type:%d ns:%d,%d,%d %s\n",
					i, (ts->input_dev->mt->trkid - 1) &TRKID_MAX,
					tfinger->x, tfinger->y, tfinger->width_major, tfinger->width_minor,
					location, ts->tcount, tfinger->type, ts->noise_mode, ts->noise_freq, ts->noise_meas, dev_name(idev));
#else
				input_info(true, &ts->client->dev, "[P] tId:%d,%d major:%d minor:%d loc:%s tc:%d type:%d %s\n",
					i, (ts->input_dev->mt->trkid - 1) &TRKID_MAX,
					tfinger->width_major, tfinger->width_minor,
					location, ts->tcount, tfinger->type, dev_name(idev));
#endif
				pre_x[i] = tfinger->x;
				pre_y[i] = tfinger->y;
			} else {
				mc[i]++;
				if (ts->dbg_mask & DBG_LOG_MOVE_FINGER) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &ts->client->dev, "[C] tId:%d,%d x:%d y:%d major:%d minor:%d loc:%s tc:%d type:%d ns:%d,%d,%d %s\n",
					i, (ts->input_dev->mt->trkid - 1) &TRKID_MAX,
					tfinger->x, tfinger->y, tfinger->width_major, tfinger->width_minor,
					location, ts->tcount, tfinger->type, ts->noise_mode, ts->noise_freq, ts->noise_meas, dev_name(idev));
#else
				input_info(true, &ts->client->dev, "[C] tId:%d,%d major:%d minor:%d loc:%s tc:%d type:%d %s\n",
					i, (ts->input_dev->mt->trkid - 1) &TRKID_MAX,
					tfinger->width_major, tfinger->width_minor,
					location, ts->tcount, tfinger->type, dev_name(idev));
#endif
				}
			}

			continue;
		}

		if (release_mask & (1 << i)) {
			char *rstr = "[R]";

			if (release_all)
				rstr = "[RA]";
			else if (ts->report.palm_bit)
				rstr = "[RP]";

			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
		#if 0
			input_report_key(input, BTN_TOUCH, 0);
			input_report_key(input, BTN_TOOL_FINGER, 0);
			input_report_abs(input, ABS_MT_PRESSURE, 0);
		#endif
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev, "%s tId:%d dd:%d,%d loc:%s tc:%d mc:%d lx:%d, ly:%d ns:%d,%d,%d\n",
				rstr, i, pre_x[i] - tfinger->x, pre_y[i] - tfinger->y,
				location, ts->tcount, mc[i], tfinger->x, tfinger->y, ts->noise_mode, ts->noise_freq, ts->noise_meas);
#else
			input_info(true, &ts->client->dev, "%s tId:%d dd:%d,%d loc:%s tc:%d mc:%d\n",
				rstr, i, pre_x[i] - tfinger->x, pre_y[i] - tfinger->y,
				location, ts->tcount, mc[i]);
#endif
			mc[i]= 0;
			pre_x[i] = 0;
			pre_y[i] = 0;
		}
	}

	if (!tcount) {
		input_mt_slot(input, 0);

		input_report_key(input, BTN_TOUCH, false);
		input_report_key(input, BTN_TOOL_FINGER, false);

		ts->palm_flag = 0;
		ts->touch_count = 0;
	}

	ts->old_mask = new_mask;

	input_sync(input);
}

static void lxs_ts_event_ctrl_knock(struct lxs_ts_data *ts)
{
#if defined(USE_LOWPOWER_MODE)
	struct input_dev *input = ts->input_dev;

	if (!input)
		return;

	input_info(true, &ts->client->dev, "%s: Gesture : Double Click\n", __func__);

	input_report_key(input, KEY_WAKEUP, 1);
	input_sync(input);

	input_report_key(input, KEY_WAKEUP, 0);
	input_sync(input);
#endif
}

static void lxs_ts_event_ctrl_gesture(struct lxs_ts_data *ts)
{
#if defined(USE_LOWPOWER_MODE)
	struct input_dev *input = ts->input_dev;
	u32 keycode = KEY_BLACK_UI_GESTURE;
	/* TBD */
	/*
	 * slide up : KEB_BLACK_UI_GESTURE
	 * else     : KEY_WAKEUP
	 */

	if (keycode > 0) {
		input_info(true, &ts->client->dev, "%s: Gesture : Swipe\n", __func__);

		input_report_key(input, keycode, 1);
		input_sync(input);
		input_report_key(input, keycode, 0);
		input_sync(input);
	}
#endif
}

static void lxs_ts_event_ctrl_prox(struct lxs_ts_data *ts)
{
#if defined(USE_PROXIMITY)
	struct input_dev *input = ts->input_dev_proximity;
	int state = ts->prox_state;

	if (!input)
		return;

	input_info(true, &ts->client->dev, "%s: prox state %d\n", __func__, state);

	input_report_abs(input, ABS_MT_CUSTOM, state);
	input_sync(input);
#endif
}

static void lxs_ts_event_ctrl(struct lxs_ts_data *ts)
{
	if (ts->intr_status & TS_TOUCH_IRQ_FINGER)
		lxs_ts_event_ctrl_finger(ts, 0);

	if (ts->intr_status & TS_TOUCH_IRQ_KNOCK)
		lxs_ts_event_ctrl_knock(ts);

	if (ts->intr_status & TS_TOUCH_IRQ_GESTURE)
		lxs_ts_event_ctrl_gesture(ts);

	if (ts->intr_status & TS_TOUCH_IRQ_PROX)
		lxs_ts_event_ctrl_prox(ts);
}

static void lxs_ts_esd_reset_work(struct work_struct *work)
{
	struct lxs_ts_data *ts = container_of(work, struct lxs_ts_data,
			esd_reset_work.work);

	input_info(true, &ts->client->dev, "%s reset_retry:%d\n",
		__func__, ts->reset_retry);

	lxs_ts_irq_enable(ts, false);
	sec_input_notify(&ts->lxs_input_nb, NOTIFIER_TSP_ESD_INTERRUPT, NULL);
	ts->esd_recovery = 1;
}

static int lxs_ts_irq_start(struct lxs_ts_data *ts)
{
	int ret = 0;

	if (gpio_get_value(ts->plat_data->irq_gpio) == 1)
		return -EINVAL;

	if (is_ts_power_state_lpm(ts)) {
		pm_wakeup_event(&ts->client->dev, 500);

		ret = wait_for_completion_interruptible_timeout(&ts->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return -EFAULT;
		}

		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled, %d\n", __func__, ret);
			return ret;
		}

		input_info(true, &ts->client->dev, "%s: run LPM interrupt handler, %d\n", __func__, ret);
	}

	return 0;
}

static irqreturn_t lxs_ts_irq_thread(int irq, void *ptr)
{
	struct lxs_ts_data *ts = (struct lxs_ts_data *)ptr;
	int ret = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif

#if 0	//defined(__SUPPORT_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return IRQ_HANDLED;
#endif
	ret = lxs_ts_irq_start(ts);
	if (ret < 0)
		return IRQ_HANDLED;

	mutex_lock(&ts->eventlock);

	ret = lxs_ts_read_event(ts);
	if (!ret)
		lxs_ts_event_ctrl(ts);

	lxs_ts_external_func(ts);

	mutex_unlock(&ts->eventlock);

	if (ret < 0) {
		if (ret == -ETESDIRQ) {
			schedule_delayed_work(&ts->esd_reset_work, 0);
			return IRQ_HANDLED;
		}

		if (ret == -ERESTART)
			lxs_ts_ic_reset(ts);
	}

	return IRQ_HANDLED;
}

int lxs_ts_irq_wake(struct lxs_ts_data *ts, bool enable)
{
	if (!ts->irq_activate)
		return 0;

	if (!device_may_wakeup(&ts->client->dev))
		return 0;

	if (!ts->irq) {
		input_err(true, &ts->client->dev, "%s: no irq\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (ts->irq_wake)
			return 0;

		enable_irq_wake(ts->irq);
		ts->irq_wake = true;
		input_info(true, &ts->client->dev, "%s: irq_wake(%d) enabled\n", __func__, ts->irq);
	} else {
		if (!ts->irq_wake)
			return 0;

		disable_irq_wake(ts->irq);
		ts->irq_wake = false;
		input_info(true, &ts->client->dev, "%s: irq_wake(%d) disabled\n", __func__, ts->irq);
	}

	return 0;
}

int lxs_ts_irq_enable(struct lxs_ts_data *ts, bool enable)
{
	if (!ts->irq_activate)
		return 0;

	if (!ts->irq) {
		input_err(true, &ts->client->dev, "%s: no irq\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (ts->irq_enabled)
			return 0;

		enable_irq(ts->irq);
		ts->irq_enabled = true;
		input_info(true, &ts->client->dev, "%s: irq(%d) enabled\n", __func__, ts->irq);
	} else {
		if (!ts->irq_enabled)
			return 0;

		disable_irq(ts->irq);
		ts->irq_enabled = false;
		input_info(true, &ts->client->dev, "%s: irq(%d) disabled\n", __func__, ts->irq);
	}

	return 0;
}

int lxs_ts_irq_activate(struct lxs_ts_data *ts, bool enable)
{
	int ret = 0;

	if (!ts->irq) {
		input_err(true, &ts->client->dev, "%s: no irq\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (ts->irq_activate)
			return 0;

		ret = request_threaded_irq(ts->irq, NULL, lxs_ts_irq_thread,
				ts->irq_type, LXS_TS_NAME, ts);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
				"%s: Failed to create irq thread, %d\n", __func__, ret);
			return ret;
		}

		ts->irq_activate = true;

		disable_irq(ts->irq);
	} else {
		if (!ts->irq_activate)
			return 0;

		lxs_ts_irq_enable(ts, false);

		free_irq(ts->irq, ts);
		input_info(true, &ts->client->dev,
			"%s: irq thread released\n", __func__);
		ts->irq_activate = false;
	}

	return 0;
}

void lxs_ts_release_finger(struct lxs_ts_data *ts)
{
	if (ts->prox_power_off) {
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, KEY_INT_CANCEL, 0);
		input_sync(ts->input_dev);
	}

	if (ts->input_dev_proximity) {
		input_report_abs(ts->input_dev_proximity, ABS_MT_CUSTOM, 0xff);
		input_sync(ts->input_dev_proximity);
	}

	if (ts->old_mask) {
		ts->new_mask = 0;
		ts->intr_status = TS_TOUCH_IRQ_FINGER;
		lxs_ts_event_ctrl_finger(ts, 1);
		ts->tcount = 0;
		memset(ts->tfinger, 0, sizeof(ts->tfinger));
	}

	ts->touch_count = 0;


}

void lxs_ts_locked_release_finger(struct lxs_ts_data *ts)
{
	mutex_lock(&ts->eventlock);

	lxs_ts_release_finger(ts);

	mutex_unlock(&ts->eventlock);
}

int lxs_ts_resume_early(struct lxs_ts_data *ts)
{
	u8 lpwg_dump[5] = {0x7, 0x0, 0x0, 0x0, 0x0};

	if (is_ts_power_state_on(ts)) {
		input_info(true, &ts->client->dev, "%s: already resume\n", __func__);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s\n", __func__);

	lxs_ts_irq_enable(ts, false);

	if (is_ts_power_state_lpm(ts)) {
		lxs_ts_get_lp_dump_data(ts);
		lxs_ts_lpwg_dump_buf_write(ts, lpwg_dump);
	}

	lxs_ts_gpio_reset(ts, false);

	if (is_ts_power_state_lpm(ts)) {
		if (device_may_wakeup(&ts->client->dev))
			lxs_ts_irq_wake(ts, false);

		mutex_lock(&ts->modechange);

		ts_set_power_state(ts, LP_MODE_EXIT);

		lxs_ts_lcd_reset_ctrl(ts, false);

		mutex_unlock(&ts->modechange);

		lxs_ts_delay(10);
	}

	return 0;
}

int lxs_ts_resume(struct lxs_ts_data *ts)
{
	int ret = 0;

	if (is_ts_power_state_on(ts)) {
		input_info(true, &ts->client->dev, "%s: already resume\n", __func__);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s\n", __func__);

	lxs_ts_irq_enable(ts, false);

	cancel_delayed_work_sync(&ts->work_read_info);

	mutex_lock(&ts->modechange);

	if (ts_get_power_state(ts) == LP_MODE_EXIT)
		lxs_ts_lcd_power_ctrl(ts, false);

	ts->enabled = true;
	ts->prox_power_off = 0;

	ts->lowpower_mode = 0;
	ts->lowpower_mode |= (ts->aot_enable) ? SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP : 0;
	ts->lowpower_mode |= (ts->spay_enable) ? SEC_TS_MODE_SPONGE_SWIPE : 0;

	ret = lxs_ts_start_device(ts);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);

	mutex_unlock(&ts->modechange);

#if 0
	cancel_delayed_work(&ts->work_print_info);
#endif

	ts->print_info_cnt_open = 0;
	ts->print_info_cnt_release = 0;

#if 0
	if (!ts->shutdown_called)
		schedule_work(&ts->work_print_info.work);
#endif

	return 0;
}

int lxs_ts_suspend_early(struct lxs_ts_data *ts)
{
	if (is_ts_power_state_off(ts) || is_ts_power_state_lpm(ts)) {
		input_info(true, &ts->client->dev, "%s: already suspend(%d)\n",
			__func__, ts_get_power_state(ts));
		return 0;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
		secure_touch_stop(ts, true);
#endif

	input_info(true, &ts->client->dev, "%s\n", __func__);

	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work(&ts->work_print_info);
	cancel_delayed_work(&ts->reset_work);

	if ((ts->dbg_mask & BIT(16)) || ts->shutdown_called)
		lxs_ts_irq_enable(ts, false);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	input_info(true, &ts->client->dev,
		"%s : start: AOT %d, SPAY %d, ED %d, Prox_power_off %d, LPM 0x%X\n",
		__func__, ts->aot_enable, ts->spay_enable, ts->ear_detect_mode,
		ts->prox_power_off, ts->lowpower_mode);

	mutex_lock(&ts->modechange);

//	sec_input_print_info(&ts->client->dev, ts->tdata);

	ts->op_state = lxs_ts_chk_lp_state(ts);

	/* power control */
#if defined(USE_LOWPOWER_MODE)
	switch (ts->op_state) {
	case OP_STATE_LP:
		lxs_ts_lcd_power_ctrl(ts, true);
		lxs_ts_lcd_reset_ctrl(ts, true);
		lxs_ts_knock(ts);
		lxs_ts_irq_enable(ts, true);
		lxs_ts_irq_wake(ts, true);
		break;
	case OP_STATE_PROX:
		lxs_ts_lcd_power_ctrl(ts, true);
		lxs_ts_lcd_reset_ctrl(ts, true);

		lxs_ts_irq_enable(ts, true);
		lxs_ts_irq_wake(ts, true);
		break;
	default:
		lxs_ts_stop_device(ts);
		input_info(true, &ts->client->dev, "%s: powr off mode\n", __func__);
	}
#else
	lxs_ts_stop_device(ts);
#endif

	mutex_unlock(&ts->modechange);

	return 0;
}

int lxs_ts_suspend(struct lxs_ts_data *ts)
{
	u8 lpwg_dump[5] = {0x3, 0x0, 0x0, 0x0, 0x0};
	if (is_ts_power_state_off(ts)) {
		input_info(true, &ts->client->dev, "%s: stop state(%d)\n",
			__func__, ts_get_power_state(ts));
		lxs_ts_locked_release_finger(ts);
		return 0;
	}

	if (is_ts_power_state_lpm(ts)) {
		input_info(true, &ts->client->dev, "%s: already suspend(%d)\n",
			__func__, ts_get_power_state(ts));
		lxs_ts_locked_release_finger(ts);
		return 0;
	}

	input_info(true, &ts->client->dev, "%s\n", __func__);

	if (ts->shutdown_called) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		lxs_ts_locked_release_finger(ts);
		return 0;
	}

	mutex_lock(&ts->modechange);

	ts->enabled = false;

	cancel_delayed_work(&ts->work_print_info);
	cancel_delayed_work(&ts->reset_work);

#if defined(USE_LOWPOWER_MODE)
	switch (ts->op_state) {
	case OP_STATE_LP:
		lxs_ts_lpm_device(ts);
		input_info(true, &ts->client->dev, "%s: lpwg mode(0x%02X)\n",
			__func__, ts->lowpower_mode);
		break;
	case OP_STATE_PROX:
		lxs_ts_lpm_device(ts);
		input_info(true, &ts->client->dev, "%s: ed mode\n", __func__);
		break;
	}
#endif
	if (is_ts_power_state_lpm(ts)) {
		lxs_ts_lpwg_dump_buf_write(ts, lpwg_dump);
	}

	lxs_ts_locked_release_finger(ts);

	mutex_unlock(&ts->modechange);

	return 0;
}

static void lxs_ts_input_set_prop(struct lxs_ts_data *ts, u8 propbit)
{
	struct input_dev *input_dev = ts->input_dev;
	static char sec_input_phys[64] = { 0, };

	snprintf(sec_input_phys, sizeof(sec_input_phys), "%s/input1", input_dev->name);
	input_dev->phys = sec_input_phys;
	input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = &ts->client->dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_SW, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);

	set_bit(propbit, input_dev->propbit);

	//USE_LOWPOWER_MODE
	set_bit(KEY_WAKEUP, input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, input_dev->keybit);

	//USE_PROXIMITY
	set_bit(KEY_INT_CANCEL, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ts->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ts->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
//	input_set_abs_params(input_dev, ABS_MT_ORIENTATION, -90, 90, 0, 0);

	if (propbit == INPUT_PROP_POINTER)
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_POINTER);
	else
		input_mt_init_slots(input_dev, SEC_TS_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);

	input_set_drvdata(input_dev, ts);
}

static void lxs_ts_input_set_prop_proximity(struct lxs_ts_data *ts)
{
	struct input_dev *input_dev = ts->input_dev_proximity;
	static char sec_input_phys[64] = { 0, };

	snprintf(sec_input_phys, sizeof(sec_input_phys), "%s/input1", input_dev->name);
	input_dev->phys = sec_input_phys;
	input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = &ts->client->dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_SW, input_dev->evbit);

	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);
	input_set_drvdata(input_dev, ts);
}

static int lxs_ts_input_init(struct lxs_ts_data *ts)
{
	struct device *dev = &ts->client->dev;
	int ret;

	/* register input_dev */
	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		input_err(true, dev, "%s: allocate input_dev err!\n", __func__);
		return -ENOMEM;
	}

	ts->input_dev->name = "sec_touchscreen";
	lxs_ts_input_set_prop(ts, INPUT_PROP_DIRECT);
	ret = input_register_device(ts->input_dev);
	if (ret) {
		input_err(true, dev, "%s: Unable to register %s input device\n",
				__func__, ts->input_dev->name);
		input_free_device(ts->input_dev);
		ts->input_dev = NULL;
		return ret;
	}
	input_info(true, dev, "%s: input device[%s, %s], x %d, y %d\n",
		__func__, dev_name(&ts->input_dev->dev), ts->input_dev->phys,
		ts->max_x, ts->max_y);

	if (ts->plat_data->support_ear_detect) {
		/* register input_dev_proximity */
		ts->input_dev_proximity = input_allocate_device();
		if (!ts->input_dev_proximity) {
			input_err(true, dev, "%s: allocate input_dev_proximity err!\n", __func__);

			input_unregister_device(ts->input_dev);
			ts->input_dev = NULL;
			return -ENOMEM;
		}

		ts->input_dev_proximity->name = "sec_touchproximity";
		lxs_ts_input_set_prop_proximity(ts);
		ret = input_register_device(ts->input_dev_proximity);
		if (ret) {
			input_err(true, dev, "%s: Unable to register %s input device\n",
					__func__, ts->input_dev_proximity->name);
			input_free_device(ts->input_dev_proximity);
			ts->input_dev_proximity = NULL;

			input_unregister_device(ts->input_dev);
			ts->input_dev = NULL;
			return ret;
		}
		input_info(true, dev, "%s: input device[%s, %s]\n",
			__func__, dev_name(&ts->input_dev_proximity->dev), ts->input_dev_proximity->phys);
	}

	return 0;
}

static void lxs_ts_input_free(struct lxs_ts_data *ts)
{
	if (ts->plat_data->support_ear_detect)
		if (ts->input_dev_proximity)
			input_unregister_device(ts->input_dev_proximity);

	if (ts->input_dev)
		input_unregister_device(ts->input_dev);
}

#if IS_ENABLED(CONFIG_OF)
static int lxs_parse_dt(struct device *dev)
{
	struct lxs_ts_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 tmp[3] = { 0, };
	int lcd_id1_gpio = 0, lcd_id2_gpio = 0, lcd_id3_gpio = 0;
	int fw_name_cnt = 0;
	int lcdtype_cnt = 0;
	int fw_sel_idx = 0;
	int retval = 0;

#if IS_ENABLED(CONFIG_EXYNOS_DPU30)
	int lcdtype = 0;
	int connected = 0;
#endif

	input_info(true, dev, "%s: begins\n", __func__);

#if IS_ENABLED(CONFIG_EXYNOS_DPU30)
	connected = get_lcd_info("connected");
	if (connected < 0) {
		input_err(true, dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}

	if (!connected) {
		input_err(true, dev, "%s: lcd is disconnected\n", __func__);
		return -ENODEV;
	}

	input_info(true, dev, "%s: lcd is connected\n", __func__);

	lcdtype = get_lcd_info("id");
	if (lcdtype < 0) {
		input_err(true, dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}
	input_info(true, dev, "%s: lcdtype : 0x%08X\n", __func__, lcdtype);

#else
	input_info(true, dev, "%s: lcdtype : 0x%08X\n", __func__, lcdtype);
#endif

	fw_name_cnt = of_property_count_strings(np, "lxs,fw_name");
	if (fw_name_cnt == 0) {
		input_err(true, dev, "%s: abnormal fw count\n", __func__);
		return -EINVAL;
	} else if (fw_name_cnt == 1) {
		retval = of_property_read_u32(np, "lxs,lcdtype", &pdata->lcdtype);
		if (retval < 0) {
			input_err(true, dev, "%s Unable to read lxs,lcdid property\n", __func__);
			pdata->lcdtype = -1;
		} else {
			input_info(true, dev, "%s: DT lcd type(0x%06X), lcdtype(0x%06X)\n", __func__, pdata->lcdtype, lcdtype);

			if (pdata->lcdtype != 0x00 && pdata->lcdtype != (lcdtype & 0x00ffff)) {
				input_err(true, dev, "%s: panel mismatched, unload driver\n", __func__);
				return -EINVAL;
			}
		}
	} else {
		lcd_id1_gpio = of_get_named_gpio(np, "lxs,lcdid1-gpio", 0);
		if (gpio_is_valid(lcd_id1_gpio))
			input_info(true, dev, "%s: lcd id1_gpio %d(%d)\n",
				__func__, lcd_id1_gpio, gpio_get_value(lcd_id1_gpio));
		else {
			input_err(true, dev, "%s: Failed to get lxs,lcdid1-gpio\n", __func__);
			return -EINVAL;
		}
		lcd_id2_gpio = of_get_named_gpio(np, "lxs,lcdid2-gpio", 0);
		if (gpio_is_valid(lcd_id2_gpio)) {
			input_info(true, dev, "%s: lcd id2_gpio %d(%d)\n",
				__func__, lcd_id2_gpio, gpio_get_value(lcd_id2_gpio));
		} else {
			input_err(true, dev, "%s: Failed to get lxs,lcdid2-gpio\n", __func__);
			return -EINVAL;
		}
		/* support lcd id3 */
		lcd_id3_gpio = of_get_named_gpio(np, "lxs,lcdid3-gpio", 0);
		if (gpio_is_valid(lcd_id3_gpio)) {
			input_info(true, dev, "%s: lcd id3_gpio %d(%d)\n",
				__func__, lcd_id3_gpio, gpio_get_value(lcd_id3_gpio));
			fw_sel_idx =
				(gpio_get_value(lcd_id3_gpio) << 2) | (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);
		} else {
			input_err(true, dev, "%s: Failed to get lxs,lcdid3-gpio and use #1 &#2 id\n", __func__);
			fw_sel_idx = (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);
		}

		lcdtype_cnt = of_property_count_u32_elems(np, "lxs,lcdtype");
		input_info(true, dev, "%s: fw_name_cnt(%d) & lcdtype_cnt(%d) & fw_sel_idx(%d)\n",
					__func__, fw_name_cnt, lcdtype_cnt, fw_sel_idx);

		if (lcdtype_cnt <= 0 || fw_name_cnt <= 0 || lcdtype_cnt <= fw_sel_idx || fw_name_cnt <= fw_sel_idx) {
			input_err(true, dev, "%s: abnormal lcdtype & fw name count, fw_sel_idx(%d)\n",
					__func__, fw_sel_idx);
			return -EINVAL;
		}
		of_property_read_u32_index(np, "lxs,lcdtype", fw_sel_idx, &pdata->lcdtype);
		input_info(true, dev, "%s: lcd id(%d), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
						__func__, fw_sel_idx, lcdtype, pdata->lcdtype);
	}

	of_property_read_string_index(np, "lxs,fw_name", fw_sel_idx, &pdata->firmware_name_ts);
	if (pdata->firmware_name_ts == NULL || strlen(pdata->firmware_name_ts) == 0) {
		input_err(true, dev, "%s: Failed to get fw name\n", __func__);
		return -EINVAL;
	}
	input_info(true, dev, "%s: fw_name_ts \'%s\'\n", __func__, pdata->firmware_name_ts);

	if (of_property_read_string(np, "lxs,regulator_lcd_vdd", &pdata->regulator_lcd_vdd))
		input_err(true, dev, "%s: failed to get regulator_lcd_vdd\n", __func__);

	if (of_property_read_string(np, "lxs,regulator_lcd_reset", &pdata->regulator_lcd_reset))
		input_err(true, dev, "%s: failed to get regulator_lcd_reset\n", __func__);

	if (of_property_read_string(np, "lxs,regulator_lcd_bl", &pdata->regulator_lcd_bl_en))
		input_err(true, dev, "%s: failed to get regulator_lcd_bl\n", __func__);

	if (of_property_read_string(np, "lxs,regulator_lcd_vsp", &pdata->regulator_lcd_vsp))
		input_err(true, dev, "%s: failed to get regulator_lcd_vsp\n", __func__);

	if (of_property_read_string(np, "lxs,regulator_lcd_vsn", &pdata->regulator_lcd_vsn))
		input_err(true, dev, "%s: failed to get regulator_lcd_vsn\n", __func__);

	if (of_property_read_string(np, "lxs,regulator_tsp_reset", &pdata->regulator_tsp_reset))
		input_err(true, dev, "%s: Failed to get regulator_tsp_reset name property\n", __func__);

#if defined(USE_HW_RST)
	pdata->reset_gpio = of_get_named_gpio(np, "lxs,reset-gpio", 0);
	if (!gpio_is_valid(pdata->reset_gpio))
		input_err(true, dev, "%s: failed to get lxs,reset-gpio(%d)\n", __func__, pdata->reset_gpio);
	else
		input_info(true, dev, "%s: lxs,reset-gpio %d\n", __func__, pdata->reset_gpio);
#endif

	pdata->irq_gpio = of_get_named_gpio(np, "lxs,irq-gpio", 0);
	if (!gpio_is_valid(pdata->irq_gpio)) {
		input_err(true, dev, "%s: failed to get lxs,irq-gpio(%d)\n", __func__, pdata->irq_gpio);
		return -EINVAL;
	}
	input_info(true, dev, "%s: lxs,irq-gpio %d\n", __func__, pdata->irq_gpio);

	prop = of_find_property(np, "lxs,cs-gpio", NULL);
	if (prop && prop->length)
		pdata->cs_gpio = of_get_named_gpio_flags(np, "lxs,cs-gpio", 0, NULL);
	else
		pdata->cs_gpio = -1;
	input_info(true, dev, "%s: pdata->cs_gpio : %d\n", __func__, pdata->cs_gpio);

	/* 1080 x 2408 */
	if (of_property_read_u32_array(np, "lxs,resolution", tmp, 2)) {
		input_err(true, dev, "%s: failed to get lxs,resolution\n", __func__);
		return -EINVAL;
	}
	pdata->max_x = tmp[0];
	pdata->max_y = tmp[1];
	input_info(true, dev, "%s: max_x %d, max_y %d\n",
		__func__, pdata->max_x, pdata->max_y);

	if (of_property_read_u32_array(np, "lxs,area-size", tmp, 3)) {
		input_info(true, dev, "%s: zone's size not found\n", __func__);
		pdata->area_indicator = 48;
		pdata->area_navigation = 96;
		pdata->area_edge = 60;
	} else {
		pdata->area_indicator = tmp[0];
		pdata->area_navigation = tmp[1];
		pdata->area_edge = tmp[2];
	}
	input_info(true, dev, "%s: zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, pdata->area_indicator, pdata->area_navigation, pdata->area_edge);

	pdata->enable_sysinput_enabled = of_property_read_bool(np, "lxs,enable_sysinput_enabled");
	input_info(true, dev, "%s: Sysinput enabled %s\n",
		__func__, (pdata->enable_sysinput_enabled) ? "ON" : "OFF");

#if defined(USE_LOWPOWER_MODE)
	pdata->enable_settings_aot = of_property_read_bool(np, "lxs,enable_settings_aot");
	input_info(true, dev, "%s: AOT mode %s\n",
		__func__, (pdata->enable_settings_aot) ? "ON" : "OFF");

	pdata->support_spay = of_property_read_bool(np, "lxs,support_spay");
	input_info(true, dev, "%s: Spay mode %s\n",
		__func__, (pdata->support_spay) ? "ON" : "OFF");
#endif

#if defined(USE_PROXIMITY)
	pdata->support_ear_detect = of_property_read_bool(np, "lxs,support_ear_detect_mode");
	input_info(true, dev, "%s: ED mode %s\n",
		__func__, (pdata->support_ear_detect) ? "ON" : "OFF");

	pdata->prox_lp_scan_enabled = of_property_read_bool(np, "lxs,prox_lp_scan_enabled");
	input_info(true, dev, "%s: Prox LP Scan enabled %s\n",
		__func__, (pdata->prox_lp_scan_enabled) ? "ON" : "OFF");
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	of_property_read_u32(np, "stm,ss_touch_num", &pdata->ss_touch_num);
	input_info(true, dev, "%s: ss_touch_num:%d\n", __func__, pdata->ss_touch_num);
#endif

	input_info(true, dev, "%s: ends\n", __func__);

	return 0;
}
#else	/* !CONFIG_OF */
static int lxs_parse_dt(struct device *dev)
{
	struct lxs_ts_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;

	input_info(true, dev, "%s: begins(not CONFIG_OF)\n", __func__);

#if defined(USE_HW_RST)
	pdata->reset_gpio = LXS_RST_PIN;
#endif
	pdata->irq_gpio = LXS_INT_PIN;

	input_info(true, dev, "%s: ends(not CONFIG_OF)\n", __func__);
}
#endif	/* CONFIG_OF */

int lxs_ts_tsp_reset_ctrl(struct lxs_ts_data *ts, bool on)
{
	int ret;
	static bool enabled;

	if (ts->regulator_tsp_reset == NULL)
		return 0;

	if (enabled == on)
		return 0;

	if (on) {
		ret = regulator_enable(ts->regulator_tsp_reset);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: failed to enabled regulator_tsp_reset, %d\n",
				__func__, ret);
			return ret;
		}
	} else {
		regulator_disable(ts->regulator_tsp_reset);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s: %s done\n", __func__, (on) ? "on" : "off");

	return 0;
}

int lxs_ts_lcd_reset_ctrl(struct lxs_ts_data *ts, bool on)
{
	int ret;
	static bool enabled;

	if (ts->regulator_lcd_reset == NULL)
		return 0;

	if (enabled == on)
		return 0;

	if (on) {
		ret = regulator_enable(ts->regulator_lcd_reset);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: failed to enabled regulator_lcd_reset, %d\n",
				__func__, ret);
			return ret;
		}
	} else {
		regulator_disable(ts->regulator_lcd_reset);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s: %s done\n", __func__, (on) ? "on" : "off");

	return 0;
}

int lxs_ts_lcd_power_ctrl(struct lxs_ts_data *ts, bool on)
{
	int ret;
	static bool enabled;

	if (enabled == on)
		return 0;

	if (on) {
		if (ts->regulator_vdd) {
			ret = regulator_enable(ts->regulator_vdd);
			if (ret) {
				input_err(true, &ts->client->dev, "%s: failed to enabled regulator_vdd, %d\n",
					__func__, ret);
				return ret;
			}
		}

		if (ts->regulator_lcd_bl_en) {
			ret = regulator_enable(ts->regulator_lcd_bl_en);
			if (ret) {
				input_err(true, &ts->client->dev, "%s: failed to enabled regulator_lcd_bl_en, %d\n",
					__func__, ret);
				return ret;
			}
		}

		if (ts->regulator_lcd_vsp) {
			ret = regulator_enable(ts->regulator_lcd_vsp);
			if (ret) {
				input_err(true, &ts->client->dev, "%s: failed to enabled regulator_lcd_vsp, %d\n",
					__func__, ret);
				return ret;
			}
		}

		if (ts->regulator_lcd_vsn) {
			ret = regulator_enable(ts->regulator_lcd_vsn);
			if (ret) {
				input_err(true, &ts->client->dev, "%s: failed to enabled regulator_lcd_vsn, %d\n",
					__func__, ret);
				return ret;
			}
		}

	} else {
		if (ts->regulator_vdd) {
			ret = regulator_disable(ts->regulator_vdd);
			if (ret) {
				input_err(true, &ts->client->dev,
						"%s: Failed to disable regulator_vdd: %d\n", __func__, ret);
				return ret;
			}
		}

		if (ts->regulator_lcd_bl_en) {
			ret = regulator_disable(ts->regulator_lcd_bl_en);
			if (ret) {
				input_err(true, &ts->client->dev,
						"%s: Failed to disable regulator_lcd_bl_en: %d\n", __func__, ret);
				return ret;
			}
		}

		if (ts->regulator_lcd_vsp) {
			ret = regulator_disable(ts->regulator_lcd_vsp);
			if (ret) {
				input_err(true, &ts->client->dev,
						"%s: Failed to disable regulator_lcd_vsp: %d\n", __func__, ret);
				return ret;
			}
		}

		if (ts->regulator_lcd_vsn) {
			ret = regulator_disable(ts->regulator_lcd_vsn);
			if (ret) {
				input_err(true, &ts->client->dev,
						"%s: Failed to disable regulator_lcd_vsn: %d\n", __func__, ret);
				return ret;
			}
		}
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s: %s done\n", __func__, (on) ? "on" : "off");

	return 0;
}

static int lxs_ts_regulator_init(struct lxs_ts_data *ts)
{
	ts->regulator_vdd = regulator_get(NULL, ts->plat_data->regulator_lcd_vdd);
	if (IS_ERR_OR_NULL(ts->regulator_vdd)) {
		input_err(true, &ts->client->dev, "%s: failed to get regulator_lcd_vdd, dt:%s regulator\n",
			__func__, ts->plat_data->regulator_lcd_vdd);
		ts->regulator_vdd = NULL;
	}

	ts->regulator_lcd_reset = regulator_get(NULL, ts->plat_data->regulator_lcd_reset);
	if (IS_ERR_OR_NULL(ts->regulator_lcd_reset)) {
		input_err(true, &ts->client->dev, "%s: failed to get regulator_lcd_reset, dt:%s regulator\n",
			__func__, ts->plat_data->regulator_lcd_reset);
		ts->regulator_lcd_reset = NULL;
	}

	ts->regulator_lcd_bl_en = regulator_get(NULL, ts->plat_data->regulator_lcd_bl_en);
	if (IS_ERR_OR_NULL(ts->regulator_lcd_bl_en)) {
		input_err(true, &ts->client->dev, "%s: failed to get regulator_lcd_bl_en, dt:%s regulator\n",
			__func__, ts->plat_data->regulator_lcd_bl_en);
		ts->regulator_lcd_bl_en = NULL;
	}

	ts->regulator_lcd_vsp = regulator_get(NULL, ts->plat_data->regulator_lcd_vsp);
	if (IS_ERR_OR_NULL(ts->regulator_lcd_vsp)) {
		input_err(true, &ts->client->dev, "%s: failed to get regulator_lcd_vsp, dt:%s regulator\n",
			__func__, ts->plat_data->regulator_lcd_vsp);
		ts->regulator_lcd_vsp = NULL;
	}

	ts->regulator_lcd_vsn = regulator_get(NULL, ts->plat_data->regulator_lcd_vsn);
	if (IS_ERR_OR_NULL(ts->regulator_lcd_vsn)) {
		input_err(true, &ts->client->dev, "%s: failed to get regulator_lcd_vsn, dt:%s regulator\n",
			__func__, ts->plat_data->regulator_lcd_vsn);
		ts->regulator_lcd_vsn = NULL;
	}

	ts->regulator_tsp_reset = regulator_get(NULL, ts->plat_data->regulator_tsp_reset);
	if (IS_ERR_OR_NULL(ts->regulator_tsp_reset)) {
		input_err(true, &ts->client->dev, "%s: failed to get regulator_tsp_reset, dt:%s regulator\n",
			__func__, ts->plat_data->regulator_tsp_reset);
		ts->regulator_tsp_reset = NULL;
	}

	return 0;
}

static void lxs_ts_regulator_free(struct lxs_ts_data *ts)
{
	regulator_put(ts->regulator_vdd);
	regulator_put(ts->regulator_lcd_reset);
	regulator_put(ts->regulator_lcd_vsn);
	regulator_put(ts->regulator_lcd_vsp);
	regulator_put(ts->regulator_lcd_bl_en);
	regulator_put(ts->regulator_tsp_reset);
}

static int lxs_ts_gpio_init(struct lxs_ts_data *ts)
{
	const char *name;
	int ret = 0;

#if defined(USE_HW_RST)
	if (gpio_is_valid(ts->plat_data->reset_gpio) && !ts->regulator_tsp_reset) {
		name = "lxs-tp-rst";
		ret = gpio_request_one(ts->plat_data->reset_gpio, GPIOF_OUT_INIT_LOW, name);
		if (ret) {
			input_info(true, &ts->client->dev, "%s: failed to request reset_gpio\n", __func__);
			goto out;
		}
		input_info(true, &ts->client->dev, "%s: gpio_request_one(%d, %d, %s)\n",
			__func__, ts->plat_data->reset_gpio, GPIOF_OUT_INIT_LOW, name);
	}
#endif

	if (gpio_is_valid(ts->plat_data->irq_gpio)) {
		name = "lxs-tp-irq";
		ret = gpio_request_one(ts->plat_data->irq_gpio, GPIOF_IN, name);
		if (ret) {
			input_info(true, &ts->client->dev, "%s: failed to request irq_gpio\n", __func__);
			goto out_gpio_irq;
		}
		input_info(true, &ts->client->dev, "%s: gpio_request_one(%d, %d, %s)\n",
			__func__, ts->plat_data->irq_gpio, GPIOF_IN, name);
	}

	if (gpio_is_valid(ts->plat_data->cs_gpio)) {
		name = "lxs-tp-cs";
		ret = gpio_request_one(ts->plat_data->cs_gpio, GPIOF_OUT_INIT_HIGH, name);
		if (ret) {
			input_info(true, &ts->client->dev, "%s: failed to request cs_gpio\n", __func__);
			goto out_gpio_cs;
		}
		input_info(true, &ts->client->dev, "%s: gpio_request_one(%d, %d, %s)\n",
			__func__, ts->plat_data->cs_gpio, GPIOF_OUT_INIT_HIGH, name);
	}

	return 0;

out_gpio_cs:
	if (gpio_is_valid(ts->plat_data->irq_gpio))
		gpio_free(ts->plat_data->irq_gpio);

out_gpio_irq:
#if defined(USE_HW_RST)
	if (gpio_is_valid(ts->plat_data->reset_gpio) && !ts->regulator_tsp_reset)
		gpio_free(ts->plat_data->reset_gpio);
out:
#endif
	return ret;
}

static void lxs_tx_gpio_free(struct lxs_ts_data *ts)
{
#if defined(USE_HW_RST)
	if (gpio_is_valid(ts->plat_data->reset_gpio) && !ts->regulator_tsp_reset) {
		gpio_free(ts->plat_data->reset_gpio);
		input_info(true, &ts->client->dev, "%s: gpio_free(%d)\n",
			__func__, ts->plat_data->reset_gpio);
	}
#endif

	if (gpio_is_valid(ts->plat_data->irq_gpio)) {
		gpio_free(ts->plat_data->irq_gpio);
		input_info(true, &ts->client->dev, "%s: gpio_free(%d)\n",
			__func__, ts->plat_data->irq_gpio);
	}
}

static int lxs_ts_init(struct spi_device *client)
{
	struct lxs_ts_data *ts = NULL;
	struct lxs_ts_plat_data *pdata = NULL;
	struct pinctrl *pinctrl = NULL;
	int ret = 0;

	if (client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		input_err(true, &client->dev, "%s: Full duplex not supported by master\n", __func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		pdata = kzalloc(sizeof(struct lxs_ts_plat_data), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto out_allocate_pdata;
		}

		client->dev.platform_data = pdata;

		ret = lxs_parse_dt(&client->dev);
		if (ret) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			goto out_allocate_mem;
		}
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			input_err(true, &client->dev, "%s: No platform data found\n", __func__);
			ret = -ENOMEM;
			goto out_allocate_pdata;
		}
	}

	pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(pinctrl))
		input_info(true, &client->dev, "%s: pinctrl not found\n", __func__);

	client->bits_per_word = SPI_BPW;
	client->mode = SPI_MODE_0;
	client->max_speed_hz = SPI_SPEED_HZ;

	ret = spi_setup(client);
	if (ret < 0) {
		input_err(true, &client->dev, "failed to setup spi, %d\n", ret);
		goto out_spi_setup;
	}

#if IS_ENABLED(CONFIG_MTK_SPI)
	/* old usage of MTK spi API */
	memcpy(&ts->spi_conf, &__spi_conf, sizeof(struct mt_chip_conf));
	client->controller_data = (void *)&ts->spi_conf;
#elif IS_ENABLED(CONFIG_SPI_MT65XX)
	/* new usage of MTK spi API */
	/*
	memcpy(&ts->spi_config, &__spi_config, sizeof(struct mtk_chip_config));
	client->controller_data = (void *)&ts->spi_config;
	*/
#endif
	input_info(true, &client->dev, "%s: spi init: %d.%d Mhz, mode %d, bpw %d, cs %d (%s)\n",
		__func__,
		freq_to_mhz_unit(client->max_speed_hz),
		freq_to_khz_top(client->max_speed_hz),
		client->mode,
		client->bits_per_word,
		client->chip_select,
		dev_name(&client->master->dev));

	ts = devm_kzalloc(&client->dev, sizeof(struct lxs_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		goto out_allocate_mem;
	}

	spi_set_drvdata(client, ts);

	client->irq = gpio_to_irq(pdata->irq_gpio);

	ts->dbg_mask = t_dev_dbg_mask;

	ts->client = client;
	ts->plat_data = pdata;
	ts->irq = client->irq;
	ts->irq_type = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;

	ts->ic_status_mask_error = IC_STATUS_MASK_ERROR;
	ts->ic_status_mask_abnormal = IC_STATUS_MASK_ABNORMAL;

	ts->pinctrl = pinctrl;

	ts->max_x = pdata->max_x;
	ts->max_y = pdata->max_y;

	lxs_ts_pinctrl_configure(ts, true);

	ret = lxs_ts_regulator_init(ts);
	if (ret)
		goto out_reg_init;

	INIT_DELAYED_WORK(&ts->esd_reset_work, lxs_ts_esd_reset_work);
	INIT_DELAYED_WORK(&ts->reset_work, lxs_ts_reset_work);
	INIT_DELAYED_WORK(&ts->work_read_info, lxs_ts_read_info_work);
	INIT_DELAYED_WORK(&ts->work_print_info, lxs_ts_print_info_work);

	mutex_init(&ts->bus_mutex);
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->eventlock);
	mutex_init(&ts->modechange);
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	mutex_init(&ts->secure_lock);
#endif

	device_init_wakeup(&client->dev, true);

	init_completion(&ts->resume_done);
	complete_all(&ts->resume_done);

	ret = lxs_ts_gpio_init(ts);
	if (ret)
		goto out_gpio_init;

	ret = lxs_ts_bus_init(ts);
	if (ret < 0)
		goto out_bus_init;

	lxs_ts_lpwg_dump_buf_init(ts);

	return 0;

out_bus_init:
	lxs_tx_gpio_free(ts);

out_gpio_init:
	mutex_destroy(&ts->bus_mutex);
	mutex_destroy(&ts->device_mutex);
	mutex_destroy(&ts->eventlock);
	mutex_destroy(&ts->modechange);

	lxs_ts_regulator_free(ts);

out_reg_init:
	lxs_ts_pinctrl_configure(ts, false);

	spi_set_drvdata(client, NULL);

out_spi_setup:

out_allocate_mem:
	if (client->dev.of_node)
		kfree(pdata);

out_allocate_pdata:
	input_err(true, &client->dev, "%s: failed, %d\n", __func__, ret);
	input_log_fix();

	return ret;
}

static void lxs_ts_free(struct spi_device *client)
{
	struct lxs_ts_data *ts = spi_get_drvdata(client);

	cancel_delayed_work_sync(&ts->work_print_info);
	cancel_delayed_work_sync(&ts->work_read_info);
	cancel_delayed_work_sync(&ts->reset_work);
	cancel_delayed_work_sync(&ts->esd_reset_work);

	device_init_wakeup(&client->dev, false);

	ts->lowpower_mode = 0;

	ts->probe_done = false;

	if(ts->sec.fac_dev != NULL)
		lxs_ts_cmd_free(ts);

	lxs_ts_gpio_reset(ts, false);

	lxs_ts_bus_free(ts);

	lxs_tx_gpio_free(ts);

	mutex_destroy(&ts->bus_mutex);
	mutex_destroy(&ts->device_mutex);
	mutex_destroy(&ts->eventlock);
	mutex_destroy(&ts->modechange);

	lxs_ts_regulator_free(ts);

	lxs_ts_pinctrl_configure(ts, false);

	spi_set_drvdata(client, NULL);

	if (!IS_ERR_OR_NULL(ts->pinctrl)) {
		devm_pinctrl_put(ts->pinctrl);
		ts->pinctrl = NULL;
	}

	if (client->dev.of_node) {
		kfree(ts->plat_data);
		ts->plat_data = NULL;
		client->dev.platform_data = NULL;
	}

	devm_kfree(&client->dev, ts);

	input_info(true, &client->dev, "%s: done\n", __func__);
}

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
static int lxs_notifier_call(struct notifier_block *nb, unsigned long data, void *v)
{
	struct lxs_ts_data *ts = container_of(nb, struct lxs_ts_data, lcd_nb);
	struct panel_state_data *d = (struct panel_state_data *)v;

	if (data == PANEL_EVENT_STATE_CHANGED) {
		if (d->state == PANEL_ON) {
			if (ts->esd_recovery == 1) {
				input_info(true, &ts->client->dev, "%s: LCD ESD LCD ON POST, run fw update\n", __func__);
				lxs_ts_ic_reset_sync(ts);
				ts->esd_recovery = 0;
			}
		}
	}

	return 0;
}
#endif

static int lxs_ts_probe(struct spi_device *client)
{
	struct lxs_ts_data *ts;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	ret = lxs_ts_init(client);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: fail to init resource\n", __func__);
		return ret;
	}

	ts = spi_get_drvdata(client);

	ret = lxs_ts_hw_init(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to init hw\n", __func__);
		goto out_hw_init;
	}

	ret = lxs_ts_input_init(ts);
	if (ret) {
		input_err(true, &client->dev, "%s: failed to register input device, %d\n", __func__, ret);
		goto out_input_int;
	}

	ret = lxs_ts_cmd_init(ts);
	if (ret) {
		input_err(true, &client->dev, "%s: fail to init fn\n", __func__);
		goto out_cmd_init;
	}

	input_info(true, &ts->client->dev, "%s: request_irq = %d\n", __func__, client->irq);
	ret = lxs_ts_irq_activate(ts, true);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: Failed to enable attention interrupt\n", __func__);
		goto out_irq_activate;
	}

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, 1, &ts->input_dev->dev.kobj);
#endif

#if defined(USE_VBUS_NOTIFIER)
	INIT_DELAYED_WORK(&ts->vbus_work, lxs_ts_vbus_work);
	ret = vbus_notifier_register(&ts->vbus_nb,
					lxs_ts_vbus_notification, VBUS_NOTIFY_DEV_CHARGER);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: vbus notifier register failed %d\n",
				__func__, ret);
#endif

	ts->probe_done = true;
	ts->enabled = true;

	input_info(true, &client->dev, "%s: done\n", __func__);
	input_log_fix();

#if defined(__SUPPORT_SAMSUNG_TUI)
	tsp_info = ts;
#endif

#if defined(USE_TOUCH_AUTO_START)
	input_info(true, &client->dev, "%s: auto_start\n", __func__);
	ts->op_state = OP_STATE_NP;
	lxs_ts_ic_reset(ts);
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
	ts->lcd_nb.priority = 1;
	ts->lcd_nb.notifier_call = lxs_notifier_call;
	ss_panel_notifier_register(&ts->lcd_nb);
#endif

	input_info(true, &client->dev, "%s: done\n", __func__);
	if (!ts->shutdown_called)
		schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(150));

	return 0;

out_irq_activate:
	lxs_ts_cmd_free(ts);

out_cmd_init:
	lxs_ts_input_free(ts);

out_input_int:

out_hw_init:
	lxs_ts_free(client);

	return ret;
}

static void __lxs_ts_remove(struct spi_device *client)
{
	struct lxs_ts_data *ts = spi_get_drvdata(client);


	mutex_lock(&ts->modechange);
	ts->shutdown_called = true;
	mutex_unlock(&ts->modechange);

	lxs_ts_prd_remove(ts);

#if defined(USE_VBUS_NOTIFIER)
	cancel_delayed_work_sync(&ts->vbus_work);
	vbus_notifier_unregister(&ts->vbus_nb);
#endif

	lxs_ts_irq_activate(ts, false);

	lxs_ts_input_free(ts);

	lxs_ts_free(client);
}

static int lxs_ts_remove(struct spi_device *client)
{
	input_info(true, &client->dev, "%s\n", __func__);

	__lxs_ts_remove(client);

	return 0;
}

static void lxs_ts_shutdown(struct spi_device *client)
{
	input_info(true, &client->dev, "%s\n", __func__);

	__lxs_ts_remove(client);
}

#if IS_ENABLED(CONFIG_PM)
static int lxs_ts_pm_suspend(struct device *dev)
{
	struct lxs_ts_data *ts = dev_get_drvdata(dev);

	reinit_completion(&ts->resume_done);

	return 0;
}

static int lxs_ts_pm_resume(struct device *dev)
{
	struct lxs_ts_data *ts = dev_get_drvdata(dev);

	complete_all(&ts->resume_done);

	return 0;
}

static const struct dev_pm_ops lxs_ts_dev_pm_ops = {
	.suspend = lxs_ts_pm_suspend,
	.resume = lxs_ts_pm_resume,
};
#endif

static const struct spi_device_id lxs_ts_id[] = {
	{ LXS_TS_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id lxs_ts_match_table[] = {
	{ .compatible = "lxs,lxs_ts_spi",},
	{ },
};
#else
#define lxs_ts_match_table NULL
#endif

static struct spi_driver lxs_spi_driver = {
	.probe		= lxs_ts_probe,
	.remove		= lxs_ts_remove,
	.shutdown	= lxs_ts_shutdown,
	.id_table	= lxs_ts_id,
	.driver = {
		.name	= LXS_TS_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = lxs_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &lxs_ts_dev_pm_ops,
#endif
	},
};

/*******************************************************
Description:
	Driver Install function.

return:
	Executive Outcomes. 0---succeed. not 0---failed.
********************************************************/
static int32_t __init lxs_driver_init(void)
{
	int32_t ret = 0;

	pr_info("[sec_input] %s : start\n", __func__);

	//---add spi driver---
	ret = spi_register_driver(&lxs_spi_driver);
	if (ret) {
		pr_err("[sec_input] failed to add spi driver");
		goto err_driver;
	}

	pr_info("[sec_input] %s : finished\n", __func__);

err_driver:
	return ret;
}

/*******************************************************
Description:
	Driver uninstall function.

return:
	n.a.
********************************************************/
static void __exit lxs_driver_exit(void)
{
	spi_unregister_driver(&lxs_spi_driver);
}

module_init(lxs_driver_init);
module_exit(lxs_driver_exit);

MODULE_DESCRIPTION("LXS TouchScreen driver");
MODULE_LICENSE("GPL");

