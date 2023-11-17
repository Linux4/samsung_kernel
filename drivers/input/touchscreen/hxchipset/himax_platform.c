/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for QCT platform
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_platform.h"
#include "himax_ic_core.h"
#include "himax_common.h"


int i2c_error_count;
struct spi_device *spi;

static uint8_t *gBuffer;

int himax_dev_set(struct himax_ts_data *ts)
{
	int ret = 0;

	ts->input_dev = input_allocate_device();

	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}

	ts->input_dev->name = "himax-touchscreen";

	if (!ic_data->HX_PEN_FUNC)
		goto skip_pen_operation;

	ts->hx_pen_dev = input_allocate_device();

	if (ts->hx_pen_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device-hx_pen_dev\n", __func__);
		return ret;
	}

	ts->hx_pen_dev->name = "himax-pen";
skip_pen_operation:

	return ret;
}
int himax_input_register_device(struct input_dev *input_dev)
{
	return input_register_device(input_dev);
}

void himax_vk_parser(struct device_node *dt,
		struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;

	node = of_parse_phandle(dt, "virtualkey", 0);

	if (node == NULL) {
		I(" DT-No vk info in DT\n");
		return;
	}
	while ((pp = of_get_next_child(node, pp)))
		cnt++;

	if (!cnt)
		return;

	vk = kcalloc(cnt, sizeof(struct himax_virtual_key), GFP_KERNEL);
	if (vk == NULL) {
		E("%s, vk init fail!\n", __func__);
		return;
	}
	pp = NULL;

	while ((pp = of_get_next_child(node, pp))) {
		if (of_property_read_u32(pp, "idx", &data) == 0)
			vk[i].index = data;

		if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
			vk[i].x_range_min = coords[0];
			vk[i].x_range_max = coords[1];
			vk[i].y_range_min = coords[2];
			vk[i].y_range_max = coords[3];
		} else {
			I(" range faile\n");
		}

		i++;
	}

	pdata->virtual_key = vk;

	for (i = 0; i < cnt; i++)
		I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i,
				pdata->virtual_key[i].index,
				pdata->virtual_key[i].x_range_min,
				pdata->virtual_key[i].y_range_max);

}

#if defined(HX_CONFIG_DRM)
static int check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			himax_panel = panel;
			return 0;
		}
	}

	return -ENODEV;
}

static int check_default_tp(struct device_node *dt, const char *prop)
{
	const char *active_tp;
	const char *compatible;
	char *start;
	int ret;

	ret = of_property_read_string(dt->parent, prop, &active_tp);
	if (ret) {
		pr_err(" %s:fail to read %s %d\n", __func__, prop, ret);
		return -ENODEV;
	}

	ret = of_property_read_string(dt, "compatible", &compatible);
	if (ret < 0) {
		pr_err(" %s:fail to read %s %d\n", __func__, "compatible", ret);
		return -ENODEV;
	}

	start = strnstr(active_tp, compatible, strlen(active_tp));
	if (start == NULL) {
		pr_err(" %s:no match compatible, %s, %s\n",
			__func__, compatible, active_tp);
		ret = -ENODEV;
	}

	return ret;
}
#endif

int himax_parse_dt(struct himax_ts_data *ts,
		struct himax_i2c_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = {0};
	struct property *prop;
	struct device_node *dt = ts->dev->of_node;
	u32 data = 0;
	int ret = 0;

	prop = of_find_property(dt, "himax,panel-coords", NULL);

	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			D(" %s:Invalid panel coords size %d\n",
				__func__, coords_size);
	}

	ret = of_property_read_u32_array(dt, "himax,panel-coords",
			coords, coords_size);
	if (ret == 0) {
		pdata->abs_x_min = coords[0];
		pdata->abs_x_max = (coords[1] - 1);
		pdata->abs_y_min = coords[2];
		pdata->abs_y_max = (coords[3] - 1);
		I(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__,
				pdata->abs_x_min,
				pdata->abs_x_max,
				pdata->abs_y_min,
				pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);

	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			D(" %s:Invalid display coords size %d\n",
				__func__, coords_size);
	}

	rc = of_property_read_u32_array(dt, "himax,display-coords",
			coords, coords_size);

	if (rc && (rc != -EINVAL)) {
		D(" %s:Fail to read display-coords %d\n", __func__, rc);
		return rc;
	}

	pdata->screenWidth  = coords[1];
	pdata->screenHeight = coords[3];
	I(" DT-%s:display-coords = (%d, %d)\n", __func__,
			pdata->screenWidth,
			pdata->screenHeight);
	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_irq))
		I(" DT:gpio_irq value is not valid\n");


	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_reset))
		I(" DT:gpio_rst value is not valid\n");

	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_3v3_en))
		I(" DT:gpio_3v3_en value is not valid\n");

	I(" DT:gpio_irq=%d, gpio_rst=%d, gpio_3v3_en=%d\n",
			pdata->gpio_irq,
			pdata->gpio_reset,
			pdata->gpio_3v3_en);

	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d\n", pdata->protocol_type);
	}

	himax_vk_parser(dt, pdata);
	return 0;
}
EXPORT_SYMBOL(himax_parse_dt);

static ssize_t himax_spi_sync(struct himax_ts_data *ts,
		struct spi_message *message)
{
	int status;

	status = spi_sync(ts->spi, message);

	if (status == 0) {
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static int himax_spi_read(uint8_t *command, uint8_t command_len,
		uint8_t *data, uint32_t length, uint8_t toRetry)
{
	struct spi_message message;
	struct spi_transfer xfer[2];
	int retry;
	int error;

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	xfer[0].tx_buf = command;
	xfer[0].len = command_len;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].rx_buf = data;
	xfer[1].len = length;
	spi_message_add_tail(&xfer[1], &message);

	for (retry = 0; retry < toRetry; retry++) {
		error = spi_sync(private_ts->spi, &message);
		if (unlikely(error))
			E("SPI read error: %d\n", error);
		else
			break;
	}

	if (retry == toRetry) {
		E("%s: SPI read error retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}

	return 0;
}

static int himax_spi_write(uint8_t *buf, uint32_t length)
{

	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= length,
	};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return himax_spi_sync(private_ts, &m);

}

int himax_bus_read(uint8_t command, uint8_t *data,
		uint32_t length, uint8_t toRetry)
{
	int result = 0;
	uint8_t spi_format_buf[3];

	mutex_lock(&(private_ts->rw_lock));
	spi_format_buf[0] = 0xF3;
	spi_format_buf[1] = command;
	spi_format_buf[2] = 0x00;

	result = himax_spi_read(&spi_format_buf[0], 3, data, length, toRetry);
	mutex_unlock(&(private_ts->rw_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_read);

int himax_bus_write(uint8_t command, uint8_t *data,
		uint32_t length, uint8_t toRetry)
{
	uint8_t *spi_format_buf = gBuffer;
	int i = 0;
	int result = 0;

	mutex_lock(&(private_ts->rw_lock));
	spi_format_buf[0] = 0xF2;
	spi_format_buf[1] = command;

	for (i = 0; i < length; i++)
		spi_format_buf[i + 2] = data[i];

	result = himax_spi_write(spi_format_buf, length + 2);
	mutex_unlock(&(private_ts->rw_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_write);

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

void himax_int_enable(int enable)
{
	struct himax_ts_data *ts = private_ts;
	unsigned long irqflags = 0;
	int irqnum = ts->hx_irq;

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	I("%s: Entering! irqnum = %d\n", __func__, irqnum);
	if (enable == 1 && atomic_read(&ts->irq_state) == 0) {
		atomic_set(&ts->irq_state, 1);
		enable_irq(irqnum);
		private_ts->irq_enabled = 1;
	} else if (enable == 0 && atomic_read(&ts->irq_state) == 1) {
		atomic_set(&ts->irq_state, 0);
		disable_irq_nosync(irqnum);
		private_ts->irq_enabled = 0;
	}

	I("enable = %d\n", enable);
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}
EXPORT_SYMBOL(himax_int_enable);

#if defined(HX_RST_PIN_FUNC)
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}
EXPORT_SYMBOL(himax_rst_gpio_set);
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int himax_regulator_configure(struct himax_i2c_platform_data *pdata)
{
	int retval;
	/* struct i2c_client *client = private_ts->client; */

	pdata->vcc_dig = regulator_get(private_ts->dev, "vdd");

	if (IS_ERR(pdata->vcc_dig)) {
		E("%s: Failed to get regulator vdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_dig);
		return retval;
	}

	pdata->vcc_ana = regulator_get(private_ts->dev, "avdd");

	if (IS_ERR(pdata->vcc_ana)) {
		E("%s: Failed to get regulator avdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_ana);
		regulator_put(pdata->vcc_dig);
		return retval;
	}

	return 0;
};

static void himax_regulator_deinit(struct himax_i2c_platform_data *pdata)
{
	I("%s: entered.\n", __func__);

	if (!IS_ERR(pdata->vcc_ana))
		regulator_put(pdata->vcc_ana);

	if (!IS_ERR(pdata->vcc_dig))
		regulator_put(pdata->vcc_dig);

	I("%s: regulator put, completed.\n", __func__);
};

static int himax_power_on(struct himax_i2c_platform_data *pdata, bool on)
{
	int retval;

	if (on) {
		retval = regulator_enable(pdata->vcc_dig);

		if (retval) {
			E("%s: Failed to enable regulator vdd\n",
					__func__);
			return retval;
		}

		/*msleep(100);*/
		usleep_range(1000, 1001);
		retval = regulator_enable(pdata->vcc_ana);

		if (retval) {
			E("%s: Failed to enable regulator avdd\n",
					__func__);
			regulator_disable(pdata->vcc_dig);
			return retval;
		}
	} else {
		regulator_disable(pdata->vcc_dig);
		regulator_disable(pdata->vcc_ana);
	}

	return 0;
}

int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error;
	/* struct i2c_client *client = private_ts->client; */

	error = himax_regulator_configure(pdata);

	if (error) {
		E("Failed to intialize hardware\n");
		goto err_regulator_not_on;
	}

#if defined(HX_RST_PIN_FUNC)

	if (gpio_is_valid(pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_reset);
			goto err_regulator_on;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_req;
		}
	}

#endif
	error = himax_power_on(pdata, true);

	if (error) {
		E("Failed to power on hardware\n");
		goto err_power_on;
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "hmx_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_req_irq_gpio;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_set_gpio_irq;
		}

		private_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_req_irq_gpio;
	}

	/*msleep(20);*/
	usleep_range(2000, 2001);
#if defined(HX_RST_PIN_FUNC)

	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
					pdata->gpio_reset);
			goto err_set_gpio_irq;
		}
	}

#endif
	return 0;
err_set_gpio_irq:

	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

err_req_irq_gpio:
	himax_power_on(pdata, false);
err_power_on:
#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_req:
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

err_regulator_on:
#endif
	himax_regulator_deinit(pdata);
err_regulator_not_on:
	return error;
}

#else
int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error = 0;
	/* struct i2c_client *client = private_ts->client; */
#if defined(HX_RST_PIN_FUNC)

	if (pdata->gpio_reset >= 0) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");

		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			goto err_gpio_reset_req;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
					pdata->gpio_reset);
			goto err_gpio_reset_dir;
		}
	}

#endif

#if defined(HX_PON_PIN_SUPPORT)
	if (gpio_is_valid(pdata->gpio_pon)) {
		error = gpio_request(pdata->gpio_pon, "hmx_pon_gpio");

		if (error) {
			E("unable to request scl gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_req;
		}

		error = gpio_direction_output(pdata->gpio_pon, 0);

		I("gpio_pon LOW [%d]\n", pdata->gpio_pon);

		if (error) {
			E("unable to set direction for pon gpio [%d]\n",
				pdata->gpio_pon);
			goto err_gpio_pon_dir;
		}
	}
#endif


	if (pdata->gpio_3v3_en >= 0) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");

		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			goto err_gpio_3v3_req;
		}

		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en set 1 get pin = %d\n",
			gpio_get_value(pdata->gpio_3v3_en));
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			goto err_gpio_irq_req;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
				pdata->gpio_irq);
			goto err_gpio_irq_set_input;
		}

		private_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_gpio_irq_req;
	}
#if defined(HX_PON_PIN_SUPPORT)
	msleep(20);
#else
	usleep_range(2000, 2001);
#endif

#if defined(HX_RST_PIN_FUNC)

	if (pdata->gpio_reset >= 0) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_set_high;
		}
	}
#endif

#if defined(HX_PON_PIN_SUPPORT)
	msleep(800);

	if (gpio_is_valid(pdata->gpio_pon)) {

		error = gpio_direction_output(pdata->gpio_pon, 1);

		I("gpio_pon HIGH [%d]\n", pdata->gpio_pon);

		if (error) {
			E("gpio_pon unable to set direction for gpio [%d]\n",
					pdata->gpio_pon);
			goto err_gpio_pon_set_high;
		}
	}
#endif
	return error;

#if defined(HX_PON_PIN_SUPPORT)
err_gpio_pon_set_high:
#endif
#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_set_high:
#endif
err_gpio_irq_set_input:
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);
err_gpio_irq_req:
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);
err_gpio_3v3_req:
#if defined(HX_PON_PIN_SUPPORT)
err_gpio_pon_dir:
	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
err_gpio_pon_req:
#endif
#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_dir:
	if (pdata->gpio_reset >= 0)
		gpio_free(pdata->gpio_reset);
err_gpio_reset_req:
#endif
	return error;
}

#endif

void himax_gpio_power_deconfig(struct himax_i2c_platform_data *pdata)
{
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

#if defined(HX_RST_PIN_FUNC)
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
#endif

#if defined(CONFIG_HMX_DB)
	himax_power_on(pdata, false);
	himax_regulator_deinit(pdata);
#else
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);

#if defined(HX_PON_PIN_SUPPORT)
	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
#endif

#endif
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	struct himax_ts_data *ts = (struct himax_ts_data *)ptr;
	int32_t ret = -1;
//+bug614711, guoyan1.wt, add, 2021/05/12, TP HIMAX gesture panic
#ifdef CONFIG_PM
		//E("this is gesture test_gy\n");
		if (ts->dev_pm_suspend) {
			ret = wait_for_completion_timeout(&ts->dev_pm_resume_completion, msecs_to_jiffies(700));
			if (!ret) {
				E("system(bus) can't finished resuming procedure, skip it\n");
				return IRQ_HANDLED;
			}
		}
#endif /* #ifdef CONFIG_PM */
//-bug614711, guoyan1.wt, add, 2021/05/12, TP HIMAX gesture panic
	//himax_ts_isr_func((struct himax_ts_data *)ptr);
	himax_ts_isr_func(ts);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work,
		struct himax_ts_data, work);

	himax_ts_work(ts);
}

int himax_int_register_trigger(void)
{
	int ret = 0;
	struct himax_ts_data *ts = private_ts;

	if (ic_data->HX_INT_IS_EDGE) {
		I("%s edge triiger falling\n", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			HIMAX_common_NAME, ts);
	} else {
		I("%s level trigger low\n", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	}

	return ret;
}

int himax_int_en_set(void)
{
	int ret = NO_ERR;

	ret = himax_int_register_trigger();
	return ret;
}

int himax_ts_register_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	/* struct i2c_client *client = private_ts->client; */
	int ret = 0;

	ts->irq_enabled = 0;

	/* Work functon */
	if (private_ts->hx_irq) {/*INT mode*/
		ts->use_irq = 1;
		ret = himax_int_register_trigger();

		if (ret == 0) {
			ts->irq_enabled = 1;
			atomic_set(&ts->irq_state, 1);
			I("%s: irq enabled at gpio: %d\n", __func__,
				private_ts->hx_irq);
#if defined(HX_SMART_WAKEUP)
			irq_set_irq_wake(private_ts->hx_irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: private_ts->hx_irq is empty, use polling mode.\n",
			__func__);
	}

	/*if use polling mode need to disable HX_ESD_RECOVERY function*/
	if (!ts->use_irq) {
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}

	return ret;
}

int himax_ts_unregister_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0;

	I("%s: entered.\n", __func__);

	/* Work functon */
	if (private_ts->hx_irq && ts->use_irq) {/*INT mode*/
#if defined(HX_SMART_WAKEUP)
		irq_set_irq_wake(ts->hx_irq, 0);
#endif
		free_irq(ts->hx_irq, ts);
		I("%s: irq disabled at qpio: %d\n", __func__,
			private_ts->hx_irq);
	}

	/*if use polling mode need to disable HX_ESD_RECOVERY function*/
	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
		cancel_work_sync(&ts->work);
		if (ts->himax_wq != NULL)
			destroy_workqueue(ts->himax_wq);
		I("%s: polling mode destroyed", __func__);
	}

	return ret;
}

static int himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if defined(HX_CONFIG_DRM) && !defined(HX_CONFIG_FB)
	if (!ts->initialized)
		return -ECANCELED;
#endif
	himax_chip_common_suspend(ts);
	return 0;
}
#if !defined(HX_CONTAINER_SPEED_UP)
static int himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if defined(HX_CONFIG_DRM) && !defined(HX_CONFIG_FB)
	/*
	 *	wait until device resume for TDDI
	 *	TDDI: Touch and display Driver IC
	 */
	if (!ts->initialized) {
		if (himax_chip_common_init())
			return -ECANCELED;
	}
#endif
	himax_chip_common_resume(ts);
	return 0;
}
#endif

#if defined(HX_CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
	    container_of(self, struct himax_ts_data, fb_notif);

	I(" %s\n", __func__);
	if (evdata && evdata->data &&
#if defined(HX_CONTAINER_SPEED_UP)
		event == FB_EARLY_EVENT_BLANK
#else
		event == FB_EVENT_BLANK
#endif
		&& ts != NULL &&
		ts->dev != NULL) {
		blank = evdata->data;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
#if defined(HX_CONTAINER_SPEED_UP)
			queue_delayed_work(ts->ts_int_workqueue,
				&ts->ts_int_work,
				msecs_to_jiffies(DELAY_TIME));
#else
				himax_common_resume(ts->dev);
#endif
			break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			himax_common_suspend(ts->dev);
			break;
		}
	}

	return 0;
}
#elif defined(HX_CONFIG_DRM)
int drm_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct drm_panel_notifier *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
		container_of(self, struct himax_ts_data, fb_notif);

	if (!evdata || (evdata->id == 0) || !(evdata->data))
		return 0;

	I("DRM blank = %d, event = %ld %s\n", *(int*)(evdata->data), event, __func__);

	if (evdata->data
	&& event == DRM_PANEL_EARLY_EVENT_BLANK
	&& ts != NULL
	&& ts->dev != NULL) {
		blank = evdata->data;
		switch (*blank) {
		case DRM_PANEL_BLANK_POWERDOWN:
			if (!ts->initialized)
				return -ECANCELED;
			himax_common_suspend(ts->dev);
			break;
		}
	}

	if (evdata->data
	&& event == DRM_PANEL_EVENT_BLANK
	&& ts != NULL
	&& ts->dev != NULL) {
		blank = evdata->data;
		switch (*blank) {
		case DRM_PANEL_BLANK_UNBLANK:
#ifdef HX_CONTAINER_SPEED_UP
			queue_delayed_work(ts->ts_int_workqueue,
				&ts->ts_int_work,
				msecs_to_jiffies(0));
#else
			himax_common_resume(ts->dev);
#endif
			break;
		}
	}

	return 0;
}
#endif

/******************************
* modify this section,if needed
* panel_dsc will add to fw_name
******************************/
extern char *saved_command_line;

struct panel_info{
	uint8_t panel_id;
	char * panel_dsc;
} himax_panel_info[] = {{0,"NULL"}, {1,"inx_fhd"}, {2,"txd_inx"}, {3, "hlt_auo"},
				{4, "txd_auo_al"}, {5, "txd_auo"}, {6, "lide_hsd"}, {7, "tianma_tianma"}, {8, "djn_jdi"}};
//+bug616968, guoyan1.wt, add, 2021/03/04, TP hx83102e tianma_tianma bringup
//+bug616968, guoyan1.wt, add, 2021/03/04, TP hx83102e tianma_tianma bringup
#define NO_PANEL 0
uint8_t  himax_get_panel_info(void)
{
	bool panel_found = false;
	uint8_t loop_i = 0;
	uint8_t panel_num = 0;
	char * pos = NULL;
	char *buf = saved_command_line;

 	panel_num = sizeof(himax_panel_info) /sizeof(struct panel_info);

	for( loop_i = 0; loop_i < panel_num; loop_i++ ){
		pos = strstr(buf, himax_panel_info[loop_i].panel_dsc);
		if( NULL != pos ){
			panel_found = true;
			I("panel id : %d,%s panel exist.\n",
				himax_panel_info[loop_i].panel_id, himax_panel_info[loop_i].panel_dsc);
			break;
		}
	}

	if (panel_found)
		return himax_panel_info[loop_i].panel_id;
	else{
		E("Panel info can't match,pls check and retry.\n");
		return NO_PANEL;
	}

}

#if (defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)) && defined(HX_VENDOR_UPGRADE)
#define FW_SUFFIX ".bin"
char g_fw_name[64] = "Himax_firmware_";
static bool himax_assigned_fw_name(void)
{
	uint8_t panel_id = 0;

	panel_id = himax_get_panel_info();

	if(panel_id != NO_PANEL) {
		strncat(g_fw_name, himax_panel_info[panel_id].panel_dsc,
				strlen(himax_panel_info[panel_id].panel_dsc));
		strncat(g_fw_name, FW_SUFFIX,strlen(FW_SUFFIX));
		I("fw name is %s.\n",g_fw_name);
		return 1;
	}else{
		E("fw name dose not assign, pls check panel info.\n");
	}
	return 0;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST
extern ssize_t himax_self_test(struct seq_file *s, void *v);
static int tid_open_short_test(struct device *dev, struct seq_file *seq,
                   const struct firmware *fw)
{
	return !himax_self_test(seq, NULL);
}
#endif

static int tid_get_version(struct device *dev, unsigned int *major,
               unsigned int *minor)
{
	*major = 0;
	*minor = (unsigned int)ic_data->vendor_touch_cfg_ver;
	return 0;
}

#ifdef SEC_TSP_FACTORY_TEST
static void get_fw_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s fw:%02X",
			private_ts->chip_name,
			ic_data->vendor_touch_cfg_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I("%s: %s\n", __func__, buff);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
	I("%s: %s\n", __func__, buff);
}
extern volatile bool tpgesture_to_lcd;
static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct himax_ts_data *ts = private_ts;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] == 1) {
		tpgesture_to_lcd = 1;
		ts->SMWP_enable = 1;
		ts->gesture_cust_en[0] = 1;
	} else {
		ts->SMWP_enable = 0;
		ts->gesture_cust_en[0] = 0;
		tpgesture_to_lcd = 0;
	}

	g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable, ts->suspended);
	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	I("%s: %s\n", __func__, buff);
}

struct sec_cmd hxtp_commands[] = {
	/* the get_fw_ver_ic is same  whit the get_fw_ver_bin, because the TP's
	 * ic whitout flash in this project*/
	{SEC_CMD("get_fw_ver_bin", get_fw_ver),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};
#endif

int himax_chip_common_probe(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	int ret = 0;
	struct device_node *dp = spi->dev.of_node;
	struct touch_info_dev *tid;
	struct touch_info_dev_operations *tid_ops;

	I("Enter %s\n", __func__);
	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		dev_err(&spi->dev,
			"%s: Full duplex not supported by host\n",
			__func__);
		return -EIO;
	}

#if defined(HX_CONFIG_DRM)
	if (check_dt(dp)) {
		E("%s: check_dt \n", __func__);
		if (!check_default_tp(dp, "qcom,spi-touch-active"))
			ret = -EPROBE_DEFER;
		else
			ret = -ENODEV;

		return ret;
	}
#endif
	E("%s: 1 \n", __func__);
	gBuffer = kzalloc(sizeof(uint8_t) * HX_MAX_WRITE_SZ, GFP_KERNEL);
	if (gBuffer == NULL) {
		E("%s: allocate gBuffer failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_gbuffer_failed;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	tid = devm_tid_and_ops_allocate(&spi->dev);
	if (unlikely(!tid))
	    goto err_common_init_failed;
	E("%s: 2 \n", __func__);

#if (defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)) && defined(HX_VENDOR_UPGRADE)
	if (!himax_assigned_fw_name())
		memcpy(g_fw_name, "Himax_firmware.bin", sizeof("Himax_firmware.bin"));
#endif
	ts->tid = tid;
	tid->panel_maker = himax_panel_info[himax_get_panel_info()].panel_dsc;
	tid_ops = tid->tid_ops;
#ifdef CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST
	tid_ops->open_short_test = tid_open_short_test;
#endif
	tid_ops->get_version = tid_get_version;
//gest
#ifdef CONFIG_PM
	ts->dev_pm_suspend = false;
	init_completion(&ts->dev_pm_resume_completion);
#endif
//
	private_ts = ts;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	spi->chip_select = 0;

	ts->spi = spi;
	mutex_init(&ts->rw_lock);
	ts->dev = &spi->dev;
	dev_set_drvdata(&spi->dev, ts);
	spi_set_drvdata(spi, ts);

	ts->initialized = false;
	ret = himax_chip_common_init();
	if (ret < 0)
		goto err_common_init_failed;

	E("%s: 3 \n", __func__);

#ifdef SEC_TSP_FACTORY_TEST
	ret = sec_cmd_init(&ts->sec, hxtp_commands,
			ARRAY_SIZE(hxtp_commands), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		E("%s: Failed to sec_cmd_init\n", __func__);
		ret = -ENODEV;
		goto err_common_init_failed;
	}
#endif

	E("%s: 4 \n", __func__);

	ret = devm_tid_register(&spi->dev, tid);
	if (unlikely(ret))
		goto err_common_init_failed;

	E("%s: 5 \n", __func__);

	return ret;


err_common_init_failed:
	kfree(ts);
err_alloc_data_failed:
	kfree(gBuffer);
	gBuffer = NULL;
err_alloc_gbuffer_failed:

	return ret;
}

//gesture
#ifdef CONFIG_PM
static int himax_ts_pm_suspend(struct device *dev)
{
	struct himax_ts_data *ts = private_ts;
	printk("%s:++\n", __func__);

	ts->dev_pm_suspend = true;
	reinit_completion(&ts->dev_pm_resume_completion);

	printk("%s:--\n", __func__);
	return 0;
}

static int himax_ts_pm_resume(struct device *dev)
{
	struct himax_ts_data *ts = private_ts;
	printk("%s:++\n", __func__);

	ts->dev_pm_suspend = false;
	complete(&ts->dev_pm_resume_completion);

	printk("%s:--\n", __func__);
	return 0;
}

static const struct dev_pm_ops himax_ts_dev_pm_ops = {
	.suspend = himax_ts_pm_suspend,
	.resume = himax_ts_pm_resume,
};
#endif /* #ifdef CONFIG_PM */

//
int himax_chip_common_remove(struct spi_device *spi)
{
	struct himax_ts_data *ts = spi_get_drvdata(spi);

	if (g_hx_chip_inited)
		himax_chip_common_deinit();

	ts->spi = NULL;
	/* spin_unlock_irq(&ts->spi_lock); */
	spi_set_drvdata(spi, NULL);
	kfree(gBuffer);

	I("%s: completed.\n", __func__);

	return 0;
}

static const struct dev_pm_ops himax_common_pm_ops = {
#if (!defined(HX_CONFIG_FB)) && (!defined(HX_CONFIG_DRM))
	.suspend = himax_common_suspend,
	.resume  = himax_common_resume,
#endif
};

#if defined(CONFIG_OF)
static const struct of_device_id himax_match_table[] = {
	{.compatible = "himax,hxcommon" },
	{},
};
#else
#define himax_match_table NULL
#endif

static struct spi_driver himax_common_driver = {
	.driver = {
		.name =		HIMAX_common_NAME,
		.owner =	THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &himax_ts_dev_pm_ops,
#endif
		.of_match_table = himax_match_table,
	},
	.probe =	himax_chip_common_probe,
	.remove =	himax_chip_common_remove,
};

static int __init himax_common_init(void)
{
	I("Himax common touch panel driver init\n");
	D("Himax check double loading\n");
	if (g_mmi_refcnt++ > 0) {
		I("Himax driver has been loaded! ignoring....\n");
		return 0;
	}
	spi_register_driver(&himax_common_driver);

	return 0;
}

static void __exit himax_common_exit(void)
{
	if (spi) {
		spi_unregister_device(spi);
		spi = NULL;
	}
	spi_unregister_driver(&himax_common_driver);
}

device_initcall_sync(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");
