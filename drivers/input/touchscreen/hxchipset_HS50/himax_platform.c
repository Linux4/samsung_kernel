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
#include "himax_common.h"
#include "himax_ic_core.h"
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 start*/
#include <linux/power_supply.h>
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 end*/
#include <linux/touchscreen_info.h>

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

#if defined(HX_PON_PIN_SUPPORT)
	pdata->gpio_pon = of_get_named_gpio(dt, "himax,pon-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_pon))
		I(" DT:gpio_pon value is not valid\n");

	pdata->lcm_rst = of_get_named_gpio(dt, "himax,lcm-rst", 0);

	if (!gpio_is_valid(pdata->lcm_rst))
		I(" DT:tp-rst value is not valid\n");

	I(" DT:pdata->gpio_pon=%d, pdata->lcm_rst=%d\n",
		pdata->gpio_pon, pdata->lcm_rst);
#endif

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
	if (pdata->lcm_rst >= 0) {
		error = gpio_request(pdata->lcm_rst, "lcm-reset");

		if (error < 0) {
			E("%s: request lcm-reset pin failed\n", __func__);
			goto err_lcm_rst_req;
		}

		error = gpio_direction_output(pdata->lcm_rst, 0);
		if (error) {
			E("unable to set direction for lcm_rst [%d]\n",
					pdata->lcm_rst);
			goto err_lcm_rst_dir;
		}
	}

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

	usleep_range(2000, 2001);

#if defined(HX_PON_PIN_SUPPORT)
	msleep(20);

	if (pdata->lcm_rst >= 0) {
		error = gpio_direction_output(pdata->lcm_rst, 1);

		if (error) {
			E("lcm_rst unable to set direction for gpio [%d]\n",
			  pdata->lcm_rst);
			goto err_lcm_reset_set_high;
		}
	}
	msleep(20);
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
#if defined(HX_PON_PIN_SUPPORT)
err_lcm_reset_set_high:
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
err_lcm_rst_dir:
	if (gpio_is_valid(pdata->lcm_rst))
		gpio_free(pdata->lcm_rst);
err_lcm_rst_req:
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
    int ret = 0;
    if (!ts) {
        E("[INTR]: Invalid himax_ts_data");
        return IRQ_HANDLED;
    }

    if (ts->dev_pm_suspend) {
        ret = wait_for_completion_timeout(&ts->dev_pm_suspend_completion, msecs_to_jiffies(700));
        if (!ret) {
            E("system(spi) can't finished resuming procedure, skip it");
            return IRQ_HANDLED;
        }
    }

	himax_ts_isr_func((struct himax_ts_data *)ptr);

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
#if !defined(HX_CONTAINER_SPEED_UP) && !defined(HX_RESUME_BY_THREAD)
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
#if defined(HX_CONTAINER_SPEED_UP) || defined(HX_RESUME_BY_THREAD)
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
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
		container_of(self, struct himax_ts_data, fb_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	D("DRM  %s\n", __func__);

	if (evdata->data
	&& event == MSM_DRM_EARLY_EVENT_BLANK
	&& ts != NULL
	&& ts->dev != NULL) {
		blank = evdata->data;
		switch (*blank) {
		case MSM_DRM_BLANK_POWERDOWN:
			if (!ts->initialized)
				return -ECANCELED;
			himax_common_suspend(ts->dev);
			break;
		}
	}

	if (evdata->data
	&& event == MSM_DRM_EVENT_BLANK
	&& ts != NULL
	&& ts->dev != NULL) {
		blank = evdata->data;
		switch (*blank) {
		case MSM_DRM_BLANK_UNBLANK:
			himax_common_resume(ts->dev);
			break;
		}
	}

	return 0;
}
#endif
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 start*/
/**himax add gesture function start***/
int hx_lcm_bias_power_init(struct himax_ts_data *data)
{
	int ret;

	data->lcm_lab = regulator_get(&data->spi->dev, "lcm_lab");
	if (IS_ERR(data->lcm_lab)){
		ret = PTR_ERR(data->lcm_lab);
		E("Regulator get failed lcm_lab ret=%d", ret);
		goto _end;
	}
	if (regulator_count_voltages(data->lcm_lab)>0){
		ret = regulator_set_voltage(data->lcm_lab, LCM_LAB_MIN_UV, LCM_LAB_MAX_UV);
		if (ret){
			E("Regulator set_vtg failed lcm_lab ret=%d", ret);
			goto reg_lcm_lab_put;
		}
	}
	data->lcm_ibb = regulator_get(&data->spi->dev, "lcm_ibb");
	if (IS_ERR(data->lcm_ibb)){
		ret = PTR_ERR(data->lcm_ibb);
		E("Regulator get failed lcm_ibb ret=%d", ret);
		goto reg_set_lcm_lab_vtg;
	}
	if (regulator_count_voltages(data->lcm_ibb)>0){
		ret = regulator_set_voltage(data->lcm_ibb, LCM_IBB_MIN_UV, LCM_IBB_MAX_UV);
		if (ret){
			E("Regulator set_vtg failed lcm_lab ret=%d", ret);
			goto reg_lcm_ibb_put;
		}
	}
	return 0;
reg_lcm_ibb_put:
	regulator_put(data->lcm_ibb);
	data->lcm_ibb = NULL;
reg_set_lcm_lab_vtg:
	if (regulator_count_voltages(data->lcm_lab) > 0){
		regulator_set_voltage(data->lcm_lab, 0, LCM_LAB_MAX_UV);
	}
reg_lcm_lab_put:
	regulator_put(data->lcm_lab);
	data->lcm_lab = NULL;
_end:
	return ret;
}

int hx_lcm_bias_power_deinit(struct himax_ts_data *data)
{
	if (data-> lcm_ibb != NULL){
		if (regulator_count_voltages(data->lcm_ibb) > 0){
			regulator_set_voltage(data->lcm_ibb, 0, LCM_LAB_MAX_UV);
		}
		regulator_put(data->lcm_ibb);
	}
	if (data-> lcm_lab != NULL){
		if (regulator_count_voltages(data->lcm_lab) > 0){
			regulator_set_voltage(data->lcm_lab, 0, LCM_LAB_MAX_UV);
		}
		regulator_put(data->lcm_lab);
	}
	return 0;

}

int hx_lcm_power_source_ctrl(struct himax_ts_data *data, int enable)
{
	int rc;

	if (data->lcm_lab!= NULL && data->lcm_ibb!= NULL){
		if (enable){
			if (atomic_inc_return(&(data->lcm_lab_power)) == 1) {
				rc = regulator_enable(data->lcm_lab);
				if (rc) {
					atomic_dec(&(data->lcm_lab_power));
					E("Regulator lcm_lab enable failed rc=%d", rc);
				}
			}
			else {
				atomic_dec(&(data->lcm_lab_power));
			}
			if (atomic_inc_return(&(data->lcm_ibb_power)) == 1) {
				rc = regulator_enable(data->lcm_ibb);
				if (rc) {
					atomic_dec(&(data->lcm_ibb_power));
					E("Regulator lcm_ibb enable failed rc=%d", rc);
				}
			}
			else {
				atomic_dec(&(data->lcm_ibb_power));
			}
		}
		else {
			if (atomic_dec_return(&(data->lcm_lab_power)) == 0) {
				rc = regulator_disable(data->lcm_lab);
				if (rc)
				{
					atomic_inc(&(data->lcm_lab_power));
					E("Regulator lcm_lab disable failed rc=%d", rc);
				}
			}
			else{
				atomic_inc(&(data->lcm_lab_power));
			}
			if (atomic_dec_return(&(data->lcm_ibb_power)) == 0) {
				rc = regulator_disable(data->lcm_ibb);
				if (rc)	{
					atomic_inc(&(data->lcm_ibb_power));
					E("Regulator lcm_ibb disable failed rc=%d", rc);
				}
			}
			else{
				atomic_inc(&(data->lcm_ibb_power));
			}
		}
	}
	else
		E("Regulator lcm_ibb or lcm_lab is invalid");
	return 0;
}

/*HS50 code for HS50-3915 by gaozhengwei at 2020/11/03 start*/
void hx_lcm_power_source_ctrl_disable(void)
{
	hx_lcm_power_source_ctrl(private_ts, 0);
}
/*HS50 code for HS50-3915 by gaozhengwei at 2020/11/03 end*/
/**himax add gesture function end***/
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 end*/
#include <linux/proc_fs.h>
#define HWINFO_NAME		"tp_wake_switch"
static struct platform_device hwinfo_device= {
	.name = HWINFO_NAME,
	.id = -1,
};
#define HX_INFO_PROC_FILE "tp_info"
static struct proc_dir_entry *hx_info_proc_entry;

static ssize_t hx_proc_getinfo_read(struct file *filp, char __user *buff, size_t size, loff_t *pPos)
{
	struct himax_ts_data *ts = private_ts;
	char buf[150] = {0};
	int rc = 0;
	snprintf(buf, 150, "IC=%s module=%s TOUCH_VER : %X\n",ts->chip_name,ts->himax_name,ic_data->vendor_touch_cfg_ver);
	rc = simple_read_from_buffer(buff, size, pPos, buf, strlen(buf));
	return rc;
}

static const struct file_operations hx_info_proc_fops = {
	.owner = THIS_MODULE,
	.read = hx_proc_getinfo_read,
};

/*******************************************************
Description:
	himax touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
static int32_t hx_extra_proc_init(void)
{
	hx_info_proc_entry = proc_create(HX_INFO_PROC_FILE, 0777, NULL, &hx_info_proc_fops);
	if (NULL == hx_info_proc_entry)
	{
		E( "Couldn't create proc entry!");
		return -ENOMEM;
	}
	else
	{
		I( "Create proc entry success!");
	}
	return 0;
}

static ssize_t hx_ito_test_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	int ret = 0;
	ssize_t count = 0;

	I("%s: enter, %d\n", __func__, __LINE__);

	if (private_ts->suspended == 1) {
		E("%s: please do self test in normal active mode\n", __func__);
		count = sprintf(buf, "%d\n", ret);//return test  result
		return count;
	}

	himax_int_enable(0);/* disable irq */

	private_ts->in_self_test = 1;

	ret = g_core_fp.fp_chip_self_test_for_hq();

	private_ts->in_self_test = 0;

#if defined(HX_EXCP_RECOVERY)
	HX_EXCP_RESET_ACTIVATE = 1;
#endif
	himax_int_enable(1);
	count = sprintf(buf, "%d\n", ret);//return test  result
	return count;
}

static ssize_t hx_ito_test_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(factory_check, 0644, hx_ito_test_show, hx_ito_test_store);

static struct attribute *hx_ito_test_attributes[] ={
	&dev_attr_factory_check.attr,
	NULL
};

static struct attribute_group hx_ito_test_attribute_group = {
	.attrs = hx_ito_test_attributes
};

static int hx_test_node_init(struct platform_device *tpinfo_device)
{
    int err=0;
    err = sysfs_create_group(&tpinfo_device->dev.kobj, &hx_ito_test_attribute_group);
    if (0 != err)
    {
        E( "ERROR: ITO node create failed.");
        sysfs_remove_group(&tpinfo_device->dev.kobj, &hx_ito_test_attribute_group);
        return -EIO;
    }
    else
    {
        I("ITO node create_succeeded.");
    }
    return err;
}
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 start*/
#ifdef HX_USB_DETECT_GLOBAL
extern bool USB_detect_flag;
static int himax_charger_notifier_callback(struct notifier_block *nb,
								unsigned long val, void *v) {
	int ret = 0;
	struct power_supply *psy = NULL;
	union power_supply_propval prop;

	psy= power_supply_get_by_name("usb");
	if (!psy) {
		E("Couldn't get usbpsy\n");
		return -EINVAL;
	}
	if (!strcmp(psy->desc->name, "usb")) {
		if (psy && val == POWER_SUPPLY_PROP_STATUS) {
			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,&prop);
			if (ret < 0) {
				E("Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", ret);
				return ret;
			} else {
				USB_detect_flag = prop.intval;
			}
		}
	}
	return 0;
}
#endif
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 end*/
/*HS50 code for SR-QL3095-01-735 by fengzhigang at 2020/10/10 start*/
static int himax_get_tp_module(void)
{
    struct device_node *chosen = NULL;
    const char *panel_name = NULL;
    int fw_num = 0;

    chosen = of_find_node_by_name(NULL, "chosen");
    if (NULL == chosen)
        E("DT: chosen node is not found\n");
    else {
        of_property_read_string(chosen, "bootargs", &panel_name);
        I("cmdline:%s\n", panel_name);
    }

    if (NULL != strstr(panel_name, "qcom,mdss_dsi_gx_boe_hx83102d_7mask_720p_video")){
        fw_num = MODEL_HX_BOE_7MASK;
	} else if(NULL != strstr(panel_name, "qcom,mdss_dsi_gx_boe_6mask_hx83102d_720p_video")){
		fw_num = MODEL_HX_BOE_6MASK;
	} else if(NULL != strstr(panel_name, "qcom,mdss_dsi_txd_inx_hx83102d_swid51_720p_video")){
		fw_num = MODEL_TXD_INX;
	} else if(NULL != strstr(panel_name, "qcom,mdss_dsi_liansi_boe_9mask_hx83112a_swid24_720p_video")){
		fw_num = MODEL_LS_BOE;
	} else if(NULL != strstr(panel_name, "qcom,mdss_dsi_jz_inx_hx83102d_swid90_720p_video")){
		fw_num = MODEL_JZ_INX;
	}else
		fw_num = MODEL_DEFAULT;
       /*
        * TODO: users should implement this function
        * if there are various tp modules been used in projects.
        */

    return fw_num;
}

static void himax_update_module_info(void)
{
    int module = 0;
    struct himax_ts_data *ts = private_ts;

    module = himax_get_tp_module();
    switch (module)
    {
    case MODEL_HX_BOE_7MASK:
            strcpy(ts->himax_name,"GX_BOE_7MASK");
            strcpy(ts->himax_nomalfw_rq_name, "Himax_7mask_firmware.bin");
            strcpy(ts->himax_mpfw_rq_name, "Himax_7mask_mpfw.bin");
            strcpy(ts->himax_csv_name, "hx_7mask_criteria.csv");
            break;
	case MODEL_HX_BOE_6MASK:
            strcpy(ts->himax_name,"GX_BOE_6MASK");
            strcpy(ts->himax_nomalfw_rq_name, "Himax_6mask_firmware.bin");
            strcpy(ts->himax_mpfw_rq_name, "Himax_6mask_mpfw.bin");
            strcpy(ts->himax_csv_name, "hx_6mask_criteria.csv");
            break;
	/*HS50 code for SR-QL3095-01-831 by fengzhigang at 2020/10/26 start*/
	case MODEL_TXD_INX:
            strcpy(ts->himax_name,"TXD_INX");
            strcpy(ts->himax_nomalfw_rq_name, "Txd_inx_hx83102d_firmware.bin");
            strcpy(ts->himax_mpfw_rq_name, "Txd_inx_hx83102d_mpfw.bin");
            strcpy(ts->himax_csv_name, "Txd_inx_hx83102d_criteria.csv");
            break;
	case MODEL_LS_BOE:
            strcpy(ts->himax_name,"LS_BOE");
            strcpy(ts->himax_nomalfw_rq_name, "ls_boe_hx83112a_firmware.bin");
            strcpy(ts->himax_mpfw_rq_name, "ls_boe_hx83112a_mpfw.bin");
            strcpy(ts->himax_csv_name, "ls_boe_hx83112a_criteria.csv");
            break;
	/*HS50 code for SR-QL3095-01-831 by fengzhigang at 2020/10/26 end*/
	case MODEL_JZ_INX:
            strcpy(ts->himax_name,"JZ_INX");
            strcpy(ts->himax_nomalfw_rq_name, "Txd_inx_hx83102d_firmware.bin");
            strcpy(ts->himax_mpfw_rq_name, "Txd_inx_hx83102d_mpfw.bin");
            strcpy(ts->himax_csv_name, "Txd_inx_hx83102d_criteria.csv");
            break;
    default:
            strcpy(ts->himax_name,"UNKNOWN");
            break;
    }
}
/*HS50 code for SR-QL3095-01-735 by fengzhigang at 2020/10/10 end*/
extern enum tp_module_used tp_is_used;
extern struct sec_cmd himax_commands[];
int himax_chip_common_probe(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	int ret = 0;

	I("Enter %s\n", __func__);
	if(tp_is_used != UNKNOWN_TP) {
        I("it is not himax tp\n");
        return -ENOMEM;
    }

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		dev_err(&spi->dev,
			"%s: Full duplex not supported by host\n",
			__func__);
		return -EIO;
	}

	gBuffer = kzalloc(sizeof(uint8_t) * ( HX_MAX_WRITE_SZ + 2), GFP_KERNEL);
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
	/*HS50 code for SR-QL3095-01-735 by fengzhigang at 2020/10/10 start*/
    himax_update_module_info();
#ifdef CONFIG_HW_INFO
    set_tp_module_name(ts->himax_name);
#endif
	/*HS50 code for SR-QL3095-01-735 by fengzhigang at 2020/10/10 end*/
	ts->dev_pm_suspend = false;
	init_completion(&ts->dev_pm_suspend_completion);
	platform_device_register(&hwinfo_device);
	hx_test_node_init(&hwinfo_device);//creat /sys/devices/platform/tp_wake_switch/factory_check
	hx_extra_proc_init();//creat /proc/tp_info
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 start*/
	sec_cmd_init(&ts->sec, himax_commands, himax_get_array_size(), SEC_CLASS_DEVT_TSP);
	ret = sysfs_create_link(&ts->sec.fac_dev->kobj,&ts->input_dev->dev.kobj, "input");
	if (ret < 0)
	{
		E("create enable node fail\n");
	}
	#ifdef HX_USB_DETECT_GLOBAL
	ts->notifier_charger.notifier_call = himax_charger_notifier_callback;
	ret = power_supply_reg_notifier(&ts->notifier_charger);
	if (ret < 0) {
		E("power_supply_reg_notifier failed\n");
	}
	#endif
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 end*/
	tp_is_used = HIMAX;

	return ret;

err_common_init_failed:
	kfree(ts);
err_alloc_data_failed:
	kfree(gBuffer);
	gBuffer = NULL;
err_alloc_gbuffer_failed:

	return ret;
}

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

static void himax_chip_common_shutdown(struct spi_device *spi)
{
	/*HS50 code for SR-QL3095-01-715 by gaozhengwei at 2020/09/08 start*/
	int error = 0;
	/*HS50 code for SR-QL3095-01-715 by gaozhengwei at 2020/09/08 end*/
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 start*/
	struct himax_ts_data *ts = spi_get_drvdata(spi);

	hx_lcm_power_source_ctrl(ts, 0);//disable vsp/vsn
    I("enter shutdown fun disable vsp/vsn\n");
/*HS50 code for SR-QL3095-01-379 by fengzhigang at 2020/09/07 end*/
/*HS50 code for SR-QL3095-01-715 by gaozhengwei at 2020/09/08 start*/
#if defined(HX_RST_PIN_FUNC)
	if (ts->pdata->gpio_reset >= 0) {
		error = gpio_direction_output(ts->pdata->gpio_reset, 0);
		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  ts->pdata->gpio_reset);
		}
	}
#endif
/*HS50 code for SR-QL3095-01-715 by gaozhengwei at 2020/09/08 end*/
}

static int himax_pm_suspend(struct device *dev)
{
	struct himax_ts_data *ts_data = dev_get_drvdata(dev);
	ts_data->dev_pm_suspend = true;
	reinit_completion(&ts_data->dev_pm_suspend_completion);
	return 0;
}

static int himax_pm_resume(struct device *dev)
{
	struct himax_ts_data *ts_data = dev_get_drvdata(dev);
	ts_data->dev_pm_suspend = false;
	complete(&ts_data->dev_pm_suspend_completion);
	return 0;
}

static const struct dev_pm_ops himax_dev_pm_ops = {
	.suspend = himax_pm_suspend,
	.resume = himax_pm_resume,
};


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
	    .pm = &himax_dev_pm_ops,
		.of_match_table = himax_match_table,
	},
	.probe =	himax_chip_common_probe,
	.remove =	himax_chip_common_remove,
	.shutdown = himax_chip_common_shutdown,
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

module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");
