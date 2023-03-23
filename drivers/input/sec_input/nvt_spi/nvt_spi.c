/* drivers/input/sec_input/nvt/nvt_spi.c
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "nvt_dev.h"
#include "nvt_reg.h"

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
int nvt_pm_runtime_get_sync(struct nvt_ts_data *ts)
{
	return pm_runtime_get_sync(ts->client->controller->dev.parent);
}

void nvt_pm_runtime_put_sync(struct nvt_ts_data *ts)
{
	pm_runtime_put_sync(ts->client->controller->dev.parent);
}
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

int nvt_stui_tsp_enter(void)
{
	struct nvt_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif

	disable_irq(ts->irq);
	nvt_ts_release_all_finger(ts);

	ret = stui_spi_lock(ts->client->controller);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
		enable_irq(ts->irq);
		return -1;
	}

	return 0;
}

int nvt_stui_tsp_exit(void)
{
	struct nvt_ts_data *ts = dev_get_drvdata(ptsp);
	int ret = 0;

	if (!ts)
		return -EINVAL;

	ret = stui_spi_unlock(ts->client->controller);
	if (ret < 0)
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);

	enable_irq(ts->irq);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_notify(&ts->nvt_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif

	return ret;
}

int nvt_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_STM;
}
#endif

#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

void nvt_ts_set_spi_mode(struct nvt_ts_data *ts)
{
	if (ts->plat_data->gpio_spi_cs > 0) {
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);
		input_err(true, &ts->client->dev, "%s: cs gpio: %d\n",
			__func__, gpio_get_value(ts->plat_data->gpio_spi_cs));
	}

	ts->client->bits_per_word = 8;
	ts->client->mode = SPI_MODE_0;
	input_err(true, &ts->client->dev, "%s: mode: %d, max_speed_hz: %d: cs: %d\n", __func__, ts->client->mode, ts->client->max_speed_hz, ts->plat_data->gpio_spi_cs);
	input_err(true, &ts->client->dev, "%s: chip_select: %d, bits_per_word: %d\n", __func__, ts->client->chip_select, ts->client->bits_per_word);
}

int nvt_ts_spi_write(struct nvt_ts_data *ts, u8 *reg, int tlen, u8 *data, int len)
{
	struct spi_message *m;
	struct spi_transfer *t;
	int ret = -ENOMEM;
	int wlen;
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
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
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: TUI is enabled\n", __func__);
		return -EBUSY;
	}
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled) != 0) {
		input_err(true, &ts->client->dev, "%s: TVM is enabled\n", __func__);
		return -EBUSY;
	}
#endif
#endif
	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t) {
		kfree(m);
		return -ENOMEM;
	}

	mutex_lock(&ts->read_write_lock);
	reg[0] = SPI_WRITE_MASK(reg[0]);

	memset(ts->tbuf, 0, tlen + DUMMY_BYTES);
	memcpy(&ts->tbuf[0], reg, tlen);
	memcpy(&ts->tbuf[tlen], data, len);
	wlen = tlen + len;

	t->len = wlen;
	t->tx_buf = ts->tbuf;

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

		pr_info("sec_input: spi: W: ");
		for (i = 0; i < wlen; i++)
			pr_cont("%02X ", ts->tbuf[i]);
		pr_cont("\n");

	}

	kfree(m);
	kfree(t);
	mutex_unlock(&ts->read_write_lock);

	return ret;
}

int nvt_ts_spi_read(struct nvt_ts_data *ts, u8 *reg, int tlen, u8 *buf, int rlen)
{
	struct spi_message *m;
	struct spi_transfer *t;
	int ret = -ENOMEM;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
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
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		input_err(true, &ts->client->dev, "%s: TUI is enabled\n", __func__);
		return -EBUSY;
	}
#if IS_ENABLED(CONFIG_GH_RM_DRV)
	if (atomic_read(&ts->trusted_touch_enabled) != 0) {
		input_err(true, &ts->client->dev, "%s: TVM is enabled\n", __func__);
		return -EBUSY;
	}
#endif
#endif
	m = kzalloc(sizeof(struct spi_message), GFP_KERNEL);
	if (!m)
		return -ENOMEM;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);
	if (!t) {
		kfree(m);
		return -ENOMEM;
	}

	mutex_lock(&ts->read_write_lock);
	
	reg[0] = SPI_READ_MASK(reg[0]);
	memset(ts->tbuf, 0, tlen + DUMMY_BYTES);
	memset(ts->rbuf, 0, rlen + DUMMY_BYTES);
	memcpy(ts->tbuf, reg, tlen);
	memcpy(&ts->tbuf[1], buf, rlen);

	spi_message_init(m);

	t->len = rlen + 1 + tlen;
	t->tx_buf = ts->tbuf;
	t->rx_buf = ts->rbuf;
	t->delay_usecs = 10;

	spi_message_add_tail(t, m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(ts->client, m);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: %d\n", __func__, ret);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	mutex_unlock(&ts->read_write_lock);

	if (ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int i;

		pr_info("sec_input: spi: R: ");
		for (i = 0; i < tlen; i++)
			pr_cont("%02X ", ts->tbuf[i]);
		pr_cont("| ");
		for (i = 0; i < rlen + 1 + tlen; i++) {
			if (i == tlen + 1)
				pr_cont("|| ");
			pr_cont("%02X ", ts->rbuf[i]);
		}
		pr_cont("\n");

	}
	memcpy(buf, ts->rbuf + 2, rlen);

	kfree(m);
	kfree(t);

	return ret;
}

int nvt_ts_write_addr(struct nvt_ts_data *ts, int addr, char data)
{
	int ret = 0;
	char buf[4] = {0};

	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = ts->nvt_ts_write(ts, buf, 3, NULL, 0);
	if (ret) {
		input_err(true, &ts->client->dev,"set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = data;
	ret = ts->nvt_ts_write(ts, &buf[0], 1, &buf[1], 1);
	if (ret) {
		input_err(true, &ts->client->dev,"write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

int nvt_set_page(struct nvt_ts_data *ts, int addr)
{
	char buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return ts->nvt_ts_write(ts, buf, 3, NULL, 0);
}

void nvt_eng_reset(struct nvt_ts_data *ts)
{
	nvt_ts_write_addr(ts, 0x7FFF80, 0x5A);
	sec_delay(1);
}

void nvt_bootloader_reset(struct nvt_ts_data *ts)
{
	//---reset cmds to SWRST_N8_ADDR---
	nvt_ts_write_addr(ts, ts->nplat_data->swrst_n8_addr, 0x69);

	sec_delay(50);	//wait tBRST2FR after Bootload RST

	if (ts->nplat_data->spi_rd_fast_addr) {
		/* disable SPI_RD_FAST */
		nvt_ts_write_addr(ts, ts->nplat_data->spi_rd_fast_addr, 0x00);
	}
}

int nvt_ts_spi_probe(struct spi_device *client)
{
	struct nvt_ts_data *ts;
	struct sec_ts_plat_data *pdata;
	struct nvt_ts_platdata *npdata;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	ts = devm_kzalloc(&client->dev, sizeof(struct nvt_ts_data), GFP_KERNEL);
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

	
	npdata = devm_kzalloc(&client->dev,
					sizeof(struct nvt_ts_platdata), GFP_KERNEL);
	if (!npdata) {
		ret = -ENOMEM;
		goto error_allocate_npdata;
	}

	
	ts->tbuf = devm_kzalloc(&client->dev, NVT_TRANSFER_LEN + 1 + DUMMY_BYTES, GFP_KERNEL);
	if (!ts->tbuf) {
		ret = -ENOMEM;
		goto error_allocate_npdata;
	}

	ts->rbuf = devm_kzalloc(&client->dev, NVT_READ_LEN, GFP_KERNEL);
	if (!ts->rbuf) {
		ret = -ENOMEM;
		goto error_allocate_npdata;
	}

	client->dev.platform_data = pdata;

	ts->client = client;
	ts->plat_data = pdata;
	ts->nplat_data = npdata;
	ts->nvt_ts_read = nvt_ts_spi_read;
	ts->nvt_ts_write = nvt_ts_spi_write;

	spi_set_drvdata(client, ts);
	ret = spi_setup(client);
	input_info(true, &client->dev, "%s: spi setup: %d\n", __func__, ret);


#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	ts->plat_data->stui_tsp_enter = nvt_stui_tsp_enter;
	ts->plat_data->stui_tsp_exit = nvt_stui_tsp_exit;
	ts->plat_data->stui_tsp_type = nvt_stui_tsp_type;
#endif
	ret = nvt_ts_probe(ts);

	return ret;

error_allocate_npdata:
error_allocate_pdata:
error_allocate_mem:
	return ret;
}

int nvt_ts_spi_remove(struct spi_device *client)
{
	struct nvt_ts_data *ts = spi_get_drvdata(client);
	int ret = 0;

	ret = nvt_ts_remove(ts);

	return 0;
}

void nvt_ts_spi_shutdown(struct spi_device *client)
{
	struct nvt_ts_data *ts = spi_get_drvdata(client);

	nvt_ts_shutdown(ts);
}


#if IS_ENABLED(CONFIG_PM)
static int nvt_ts_spi_pm_suspend(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	nvt_ts_pm_suspend(ts);

	return 0;
}

static int nvt_ts_spi_pm_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	nvt_ts_pm_resume(ts);

	return 0;
}
#endif


static const struct spi_device_id nvt_ts_spi_id[] = {
	{ NVT_TS_SPI_NAME, 0 },
	{ },
};

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops nvt_ts_dev_pm_ops = {
	.suspend = nvt_ts_spi_pm_suspend,
	.resume = nvt_ts_spi_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id nvt_ts_match_table[] = {
	{ .compatible = "nvt,nvt_ts_spi",},
	{ },
};
#else
#define nvt_ts_match_table NULL
#endif

static struct spi_driver nvt_ts_driver = {
	.probe		= nvt_ts_spi_probe,
	.remove		= nvt_ts_spi_remove,
	.shutdown	= nvt_ts_spi_shutdown,
	.id_table	= nvt_ts_spi_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= NVT_TS_SPI_NAME,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = nvt_ts_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &nvt_ts_dev_pm_ops,
#endif
	},
};

module_spi_driver(nvt_ts_driver);


MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_DESCRIPTION("NVT TouchScreen driver");
MODULE_LICENSE("GPL");
