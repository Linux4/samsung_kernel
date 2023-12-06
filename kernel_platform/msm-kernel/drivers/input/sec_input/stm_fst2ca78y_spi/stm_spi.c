// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_dev.h"
#include "stm_reg.h"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
int stm_pm_runtime_get_sync(struct stm_ts_data *ts)
{
	return pm_runtime_get_sync(ts->client->controller->dev.parent);
}

void stm_pm_runtime_put_sync(struct stm_ts_data *ts)
{
	pm_runtime_put_sync(ts->client->controller->dev.parent);
}
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
int stm_stui_tsp_enter(void)
{
	struct stm_ts_data *ts = g_ts;
	int ret = 0;

	if (!ts)
		return -EINVAL;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	disable_irq(ts->irq);
	stm_ts_release_all_finger(ts);

	ret = stui_spi_lock(ts->client->controller);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		enable_irq(ts->irq);
		return -1;
	}

	return 0;
}

int stm_stui_tsp_exit(void)
{
	struct stm_ts_data *ts = g_ts;
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_spi_unlock(ts->client->controller);
	if (ret < 0)
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);

	enable_irq(ts->irq);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->stm_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return ret;
}

int stm_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_STM;
}
#endif

int stm_ts_wire_mode_change(struct stm_ts_data *ts)
{
	u8 reg[STM_TS_EVENT_BUFF_SIZE] = {0};
	int rc = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x34;
	reg[5] = 0x02;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x3E;//STM_REG_MISO_PORT_SETTING;
	reg[5] = 0x07;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(2);

	reg[0] = STM_TS_CMD_REG_W;
	reg[1] = 0x20;
	reg[2] = 0x00;
	reg[3] = 0x00;
	reg[4] = 0x2D;
	reg[5] = 0x02;
	rc = ts->stm_ts_write(ts, &reg[0], 6, NULL, 0);
	if (rc < 0)
		return rc;
	sec_delay(30);

	return 0;
}

void stm_ts_set_spi_mode(struct stm_ts_data *ts)
{
	if (ts->plat_data->gpio_spi_cs > 0) {
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);
		input_err(true, &ts->client->dev, "%s: cs gpio: %d\n",
			__func__, gpio_get_value(ts->plat_data->gpio_spi_cs));
	}

	ts->client->mode = SPI_MODE_0;
	input_err(true, &ts->client->dev, "%s: mode: %d, max_speed_hz: %d: cs: %d\n", __func__, ts->client->mode, ts->client->max_speed_hz, ts->plat_data->gpio_spi_cs);
	input_err(true, &ts->client->dev, "%s: chip_select: %d, bits_per_word: %d\n", __func__, ts->client->chip_select, ts->client->bits_per_word);
}

int stm_ts_read_from_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	char *tbuf;
	char *rbuf;
	int ret = -ENOMEM;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	tbuf = kzalloc(3, GFP_KERNEL);
	if (!tbuf)
		return -ENOMEM;

	rbuf = kzalloc(length, GFP_KERNEL);
	if (!rbuf) {
		kfree(tbuf);
		return -ENOMEM;
	}

	tbuf[0] = STM_TS_CMD_SPONGE_READ_CMD;
	tbuf[1] = data[1];
	tbuf[2] = data[0];
	ret = ts->stm_ts_read(ts, &tbuf[0], 3, &rbuf[0], length);

	memcpy(&data[0], &rbuf[0], length);

	kfree(tbuf);
	kfree(rbuf);

	return ret;
}

int stm_ts_write_to_sponge(struct stm_ts_data *ts, u8 *data, int length)
{
	char *tbuf;
	int ret = -ENOMEM;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	tbuf = kzalloc(length + 1, GFP_KERNEL);	// command(1) + data(length)
	if (!tbuf)
		return -ENOMEM;

	tbuf[0] = STM_TS_CMD_SPONGE_WRITE_CMD;
	tbuf[1] = data[1];
	tbuf[2] = data[0];
	memcpy(&tbuf[3], &data[2], length - 2);

	ret = ts->stm_ts_write(ts, &tbuf[0], length + 1, NULL, 0);

	tbuf[0] = STM_TS_CMD_SPONGE_NOTIFY_CMD;
	ret = ts->stm_ts_write(ts, &tbuf[0], 1, NULL, 0);

	kfree(tbuf);

	return ret;
}

int stm_ts_no_lock_spi_write(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	struct spi_message *m;
	struct spi_transfer *t;
	char *tbuf;
	int ret = -ENOMEM;
	int wlen;

	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t) {
		kfree(m);
		return -ENOMEM;
	}

	tbuf = kzalloc(tlen + len + 1, GFP_KERNEL);
	if (!tbuf) {
		kfree(m);
		kfree(t);
		return -ENOMEM;
	}

	switch (reg[0]) {
	case STM_TS_CMD_REG_W:
	case STM_TS_CMD_REG_R:
	case STM_TS_CMD_FRM_BUFF_W:
	case STM_TS_CMD_FRM_BUFF_R:
	case STM_TS_CMD_REG_SPI_W:
	case STM_TS_CMD_REG_SPI_R:
	case STM_TS_CMD_SPONGE_W:
	case STM_TS_CMD_SPONGE_WRITE_CMD:
		memcpy(&tbuf[0], reg, tlen);
		memcpy(&tbuf[tlen], data, len);
		wlen = tlen + len;
		break;
	default:
		tbuf[0] = 0xC0;
		memcpy(&tbuf[1], reg, tlen);
		memcpy(&tbuf[1 + tlen], data, len);
		wlen = tlen + len + 1;
		break;
	}

	t->len = wlen;
	t->tx_buf = tbuf;

	spi_message_init(m);
	spi_message_add_tail(t, m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int i;
		char *buff;
		char *tbuff;
		int buff_len = (wlen * 3) + 3;
		int tbuff_len = 3 * 3 + 3;

		buff = kzalloc(buff_len, GFP_KERNEL);
		if (!buff)
			goto out_write;

		tbuff = kzalloc(tbuff_len, GFP_KERNEL);
		if (!tbuff) {
			kfree(buff);
			goto out_write;
		}

		for (i = 0; i < wlen; i++) {
			memset(tbuff, 0x00, tbuff_len);
			snprintf(tbuff, tbuff_len, "%02X ", tbuf[i]);
			strlcat(buff, tbuff, buff_len);
		}

		input_info(true, &ts->client->dev, "%s: debug: %s\n", __func__, buff);
		kfree(buff);
		kfree(tbuff);

	}

out_write:
	kfree(tbuf);
	kfree(m);
	kfree(t);

	return ret;
}

int stm_ts_spi_write(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	int ret = -ENOMEM;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	mutex_lock(&ts->read_write_mutex);
	ret = stm_ts_no_lock_spi_write(ts, reg, tlen, data, len);
	mutex_unlock(&ts->read_write_mutex);

	return ret;
}

int stm_ts_spi_read(struct stm_ts_data *ts, u8 *reg, int tlen, u8 *buf, int rlen)
{
	struct spi_message *m;
	struct spi_transfer *t;
	char *tbuf, *rbuf;
	int ret = -ENOMEM;
	int wmsg = 0;
	int buf_len = tlen;
	int mem_len;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		return -EIO;
	}

	if (!ts->plat_data->resume_done.done) {
		ret = wait_for_completion_interruptible_timeout(&ts->plat_data->resume_done, msecs_to_jiffies(500));
		if (ret <= 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled:%d\n", __func__, ret);
			return -EIO;
		}
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t) {
		kfree(m);
		return -ENOMEM;
	}

	if (tlen > 3)
		mem_len = rlen + 1 + tlen;
	else
		mem_len = rlen + 1 + 3;

	tbuf = kzalloc(mem_len, GFP_KERNEL);
	if (!tbuf) {
		kfree(m);
		kfree(t);
		return -ENOMEM;
	}
	rbuf = kzalloc(mem_len, GFP_KERNEL);
	if (!rbuf) {
		kfree(m);
		kfree(t);
		kfree(tbuf);
		return -ENOMEM;
	}

	mutex_lock(&ts->read_write_mutex);

	switch (reg[0]) {
	case 0x87:
	case STM_TS_CMD_REG_W:
	case STM_TS_CMD_REG_R:
	case STM_TS_CMD_FRM_BUFF_W:
	case STM_TS_CMD_FRM_BUFF_R:
	case STM_TS_CMD_REG_SPI_W:
	case STM_TS_CMD_REG_SPI_R:
	case STM_TS_CMD_SPONGE_READ_CMD:
	case STM_TS_CMD_BURST_READ:
		memcpy(tbuf, reg, tlen);
		wmsg = 0;
		break;
	case 0x75:
		buf_len = 3;
		tbuf[0] = 0xD1;
		if (tlen == 1) {
			tbuf[1] = 0x00;
			tbuf[2] = 0x00;
		} else if (tlen == 2) {
			tbuf[1] = 0x00;
			tbuf[2] = reg[0];
		} else if (tlen == 3) {
			tbuf[1] = reg[1];
			tbuf[2] = reg[0];
		} else {
			input_err(true, &ts->client->dev, "%s: tlen is mismatched: %d\n", __func__, tlen);
			kfree(m);
			kfree(t);
			kfree(tbuf);
			kfree(rbuf);
			mutex_unlock(&ts->read_write_mutex);
			return -EINVAL;
		}

		wmsg = 0;
		break;
	default:
		buf_len = 3;
		tbuf[0] = 0xD1;

		if (tlen <= 3) {
			tbuf[1] = 0x00;
			tbuf[2] = 0x00;
		} else {
			input_err(true, &ts->client->dev, "%s: tlen is mismatched: %d\n", __func__, tlen);
			kfree(m);
			kfree(t);
			kfree(tbuf);
			kfree(rbuf);
			mutex_unlock(&ts->read_write_mutex);
			return -EINVAL;
		}

		wmsg = 1;
		break;
	}

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	if (wmsg)
		stm_ts_no_lock_spi_write(ts, reg, tlen, NULL, 0);

	spi_message_init(m);

	t->len = rlen + 1 + buf_len;
	t->tx_buf = tbuf;
	t->rx_buf = rbuf;

	spi_message_add_tail(t, m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	mutex_unlock(&ts->read_write_mutex);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;
		char *buff;
		char *tbuff;
		int buff_len = ((rlen + 1 + buf_len) * 3) + (3 * 3) + (1 * 3) + 3;
		int tbuff_len = buf_len * 3 + 3;

		buff = kzalloc(buff_len, GFP_KERNEL);
		if (!buff)
			goto out_read;

		tbuff = kzalloc(tbuff_len, GFP_KERNEL);
		if (!tbuff) {
			kfree(buff);
			goto out_read;
		}

		for (i = 0; i < buf_len; i++) {
			memset(tbuff, 0x00, tbuff_len);
			snprintf(tbuff, tbuff_len, "%02X ", tbuf[i]);
			strlcat(buff, tbuff, buff_len);
		}
		memset(tbuff, 0x00, tbuff_len);
		snprintf(tbuff, tbuff_len, "|| ");
		strlcat(buff, tbuff, buff_len);

		for (i = 0; i < rlen; i++) {
			memset(tbuff, 0x00, tbuff_len);
			snprintf(tbuff, tbuff_len, "%02X ", rbuf[i + 1 + buf_len]);
			strlcat(buff, tbuff, buff_len);
		}

		input_info(true, &ts->client->dev, "%s: debug: %s\n", __func__, buff);
		kfree(buff);
		kfree(tbuff);
	}

out_read:
	memcpy(buf, &rbuf[1 + buf_len], rlen);

	kfree(rbuf);
	kfree(tbuf);
	kfree(m);
	kfree(t);

	return ret;
}

int stm_ts_prefix_write_reg(struct stm_ts_data *ts, u8 *srcreg, int reglen, u8 *data, int dlen)
{
	u8 reg[8];
	int cmd_offset = 0;

	if (reglen > 8)
		return -EIO;

	if (srcreg[0] == 0x00) {
		reg[0] = STM_TS_CMD_REG_SPI_W; // 0xB2 command
		cmd_offset = 1;
	}
	memcpy(&reg[cmd_offset], srcreg, reglen);

	return ts->stm_ts_write(ts, &reg[0], reglen + cmd_offset, data, dlen);
}

int stm_ts_prefix_read_reg(struct stm_ts_data *ts, u8 *srcreg, int reglen, u8 *data, int dlen, u8 type)
{
	u8 reg[8];
	int cmd_offset = 0;

	if (reglen > 8)
		return -EIO;

	switch (type) {
	case STM_TS_CMD_TYPE_REG:
		reg[0] = STM_TS_CMD_REG_SPI_R;
		cmd_offset = 1;
		break;
	case STM_TS_CMD_TYPE_HDM:
		reg[0] = STM_TS_CMD_FRM_BUFF_R;
		cmd_offset = 1;
		break;
	default:
		cmd_offset = 0;
		break;
	}
	memcpy(&reg[cmd_offset], srcreg, reglen);

	return ts->stm_ts_read(ts, &reg[0], reglen + cmd_offset, data, dlen);

}

int stm_ts_spi_probe(struct spi_device *client)
{
	struct stm_ts_data *ts;
	struct sec_ts_plat_data *pdata;
	struct sec_tclm_data *tdata;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	ts = devm_kzalloc(&client->dev, sizeof(struct stm_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		goto error_allocate_mem;
	}

	pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto error_allocate_pdata;
	}

	tdata = devm_kzalloc(&client->dev,
			sizeof(struct sec_tclm_data), GFP_KERNEL);
	if (!tdata) {
		ret = -ENOMEM;
		goto error_allocate_tdata;
	}
	client->dev.platform_data = pdata;

	ts->client = client;
	ts->dev = &client->dev;
	ts->plat_data = pdata;
	ts->stm_ts_read = stm_ts_spi_read;
	ts->stm_ts_write = stm_ts_spi_write;
	ts->stm_ts_read_sponge = stm_ts_read_from_sponge;
	ts->stm_ts_write_sponge = stm_ts_write_to_sponge;
	ts->stm_ts_prefix_write_reg = stm_ts_prefix_write_reg;
	ts->stm_ts_prefix_read_reg = stm_ts_prefix_read_reg;
	ts->tdata = tdata;
	spi_set_drvdata(client, ts);
	ret = spi_setup(client);
	input_info(true, &client->dev, "%s: spi setup: %d\n", __func__, ret);

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ts->plat_data->stui_tsp_enter = stm_stui_tsp_enter;
	ts->plat_data->stui_tsp_exit = stm_stui_tsp_exit;
	ts->plat_data->stui_tsp_type = stm_stui_tsp_type;
#endif
	ret = stm_ts_probe(ts);

	return ret;

error_allocate_tdata:
error_allocate_pdata:
error_allocate_mem:
	return ret;
}

int stm_ts_dev_remove(struct spi_device *client)
{
	struct stm_ts_data *ts = spi_get_drvdata(client);
	int ret = 0;

	ret = stm_ts_remove(ts);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
static void stm_ts_spi_remove(struct spi_device *client)
{
	stm_ts_dev_remove(client);
}
#else
static int stm_ts_spi_remove(struct spi_device *client)
{
	stm_ts_dev_remove(client);

	return 0;
}
#endif

void stm_ts_spi_shutdown(struct spi_device *client)
{
	struct stm_ts_data *ts = spi_get_drvdata(client);

	stm_ts_shutdown(ts);
}


#if IS_ENABLED(CONFIG_PM)
static int stm_ts_spi_pm_suspend(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	stm_ts_pm_suspend(ts);

	return 0;
}

static int stm_ts_spi_pm_resume(struct device *dev)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);

	stm_ts_pm_resume(ts);

	return 0;
}
#endif


static const struct spi_device_id stm_ts_spi_id[] = {
	{ STM_TS_SPI_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops stm_ts_dev_pm_ops = {
	.suspend = stm_ts_spi_pm_suspend,
	.resume = stm_ts_spi_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id stm_ts_match_table[] = {
	{ .compatible = "stm,stm_ts_spi", },
	{ },
};
#else
#define stm_ts_match_table NULL
#endif

static struct spi_driver stm_ts_driver = {
	.probe		= stm_ts_spi_probe,
	.remove		= stm_ts_spi_remove,
	.shutdown	= stm_ts_spi_shutdown,
	.id_table	= stm_ts_spi_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= STM_TS_SPI_NAME,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = stm_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &stm_ts_dev_pm_ops,
#endif
	},
};

module_spi_driver(stm_ts_driver);


MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_DESCRIPTION("stm TouchScreen driver");
MODULE_LICENSE("GPL");
