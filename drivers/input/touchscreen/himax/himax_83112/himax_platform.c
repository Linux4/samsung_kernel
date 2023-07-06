/*  Himax Android Driver Sample Code for QCT platform

    Copyright (C) 2018 Himax Corporation.

    This software is licensed under the terms of the GNU General Public
    License version 2, as published by the Free Software Foundation, and
    may be copied, distributed, and modified under those terms.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/

#include "himax_platform.h"
#include "himax_common.h"

int i2c_error_count = 0;
int irq_enable_count = 0;
struct spi_device *spi;

extern struct himax_ic_data *ic_data;
extern struct himax_ts_data *g_ts;
extern void himax_ts_work(struct himax_ts_data *ts);
extern enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer);

extern int himax_chip_common_init(void);
extern void himax_chip_common_deinit(void);

int himax_dev_set(struct himax_ts_data *ts)
{
	int ret = 0;
	ts->input_dev = input_allocate_device();

	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}

//	ts->input_dev->name = "himax-touchscreen";
	ts->input_dev->name = "sec_touchscreen";
	return ret;
}
int himax_input_register_device(struct input_dev *input_dev)
{
	return input_register_device(input_dev);
}

#if defined(HX_PLATFOME_DEFINE_KEY)
void himax_platform_key(void)
{
	I("Nothing to be done! Plz cancel it!\n");
}
#endif

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
		input_info(true, g_ts->dev, "DT-No vk info in DT\n");
		return;
	} else {
		while ((pp = of_get_next_child(node, pp)))
			cnt++;

		if (!cnt)
			return;

		vk = kzalloc(cnt * sizeof(struct himax_virtual_key), GFP_KERNEL);
		pp = NULL;

		while ((pp = of_get_next_child(node, pp))) {
			if (of_property_read_u32(pp, "idx", &data) == 0)
				vk[i].index = data;

			if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
				vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
				vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
			} else {
				input_info(true, g_ts->dev, "range faile\n");
			}

			i++;
		}

		pdata->virtual_key = vk;

		for (i = 0; i < cnt; i++)
			input_info(true, g_ts->dev, "vk[%d] idx:%d x_min:%d, y_max:%d\n",
					i, pdata->virtual_key[i].index,
					pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
	}
}

int himax_parse_dt(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = {0};
	struct property *prop;
	struct device_node *dt = ts->dev->of_node;
	u32 data = 0;
	int count = 0;
	u32 px_zone[3] = { 0 };
#if defined(CONFIG_EXYNOS_DPU30)
	int lcdtype = 0;
	int connected = 0;
	u32 paneltype = 0;
#endif

	/* only for a20s */
	rc = of_property_read_u32(dt, "himax,panel_type", &paneltype);
	if (rc < 0) {
		input_err(true, ts->dev, "%s: fail to get himax,panel_type value(0x0X)\n", __func__, paneltype);
	}
#if defined(CONFIG_EXYNOS_DPU30)

	connected = get_lcd_info("connected");
	if (connected < 0) {
		input_err(true, ts->dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}

	if (!connected) {
		input_err(true, ts->dev, "%s: lcd is disconnected\n", __func__);
		return -ENODEV;
	}

	input_info(true, ts->dev, "%s: lcd is connected\n", __func__);

	lcdtype = get_lcd_info("id");
	if (lcdtype < 0) {
		input_err(true, ts->dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}
	input_info(true, ts->dev, "%s: lcdtype : 0x%08X, panel type : 0x%08X\n", __func__, lcdtype, paneltype);

	if (paneltype != 0x00 && paneltype != lcdtype) {
		input_err(true, ts->dev, "%s: panel mismatched, unload driver\n", __func__);
		return -EINVAL;
	}
#endif

	prop = of_find_property(dt, "himax,panel-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			input_info(true, ts->dev, "%s:Invalid panel coords size %d\n", __func__, coords_size);
	}

	if (of_property_read_u32_array(dt, "himax,panel-coords", coords, coords_size) == 0) {
		pdata->abs_x_min = coords[0], pdata->abs_x_max = (coords[1] - 1);
		pdata->abs_y_min = coords[2], pdata->abs_y_max = (coords[3] - 1);
		input_info(true, ts->dev, "DT-%s:panel-coords = %d, %d, %d, %d\n",
					__func__, pdata->abs_x_min, pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);

	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			input_info(true, ts->dev, "%s:Invalid display coords size %d\n", __func__, coords_size);
	}

	rc = of_property_read_u32_array(dt, "himax,display-coords", coords, coords_size);

	if (rc && (rc != -EINVAL)) {
		input_err(true, ts->dev, "%s:Fail to read display-coords %d\n", __func__, rc);
		return rc;
	}

	pdata->screenWidth  = coords[1];
	pdata->screenHeight = coords[3];
	input_info(true, ts->dev, "DT-%s:display-coords = (%d, %d)\n", __func__, pdata->screenWidth, pdata->screenHeight);
	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);

	if (of_property_read_u32_array(dt, "himax,area-size", px_zone, 3)) {
		input_info(true, ts->dev, "Failed to get zone's size\n");
		pdata->area_indicator = 48;
		pdata->area_navigation = 96;
		pdata->area_edge = 60;
	} else {
		pdata->area_indicator = px_zone[0];
		pdata->area_navigation = px_zone[1];
		pdata->area_edge = px_zone[2];
	}
	input_info(true, ts->dev, "%s : zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, pdata->area_indicator, pdata->area_navigation ,pdata->area_edge);

	if (!gpio_is_valid(pdata->gpio_irq)) {
		input_info(true, ts->dev, "DT:gpio_irq value is not valid\n");
	}

	pdata->lcd_gpio_reset = of_get_named_gpio(dt, "himax,lcd-rst-gpio", 0);

	if (!gpio_is_valid(pdata->lcd_gpio_reset))
		input_info(true, ts->dev, "DT:gpio_rst value is not valid\n");
	else
		input_info(true, ts->dev, "lcd_gpio_reset=%d\n", pdata->lcd_gpio_reset);

	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_reset)) {
		input_info(true, ts->dev, "DT:gpio_rst value is not valid\n");
	}

	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_3v3_en)) {
		input_info(true, ts->dev, "DT:gpio_3v3_en value is not valid\n");
	}

	input_info(true, ts->dev, "DT:gpio_irq=%d, gpio_rst=%d, gpio_3v3_en=%d\n", pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_3v3_en);

	pdata->gpio_cs = of_get_named_gpio(dt, "himax,cs-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_cs)) {
		input_info(true, ts->dev, "DT:gpio_cs value is not valid\n");

	} else {
		int error;
		input_info(true, ts->dev, "DT:gpio_cs=%d\n", pdata->gpio_cs);
		error = gpio_request(pdata->gpio_cs, "himax-gpio_cs");

		if (error < 0) {
			input_err(true, ts->dev, "%s: request gpio_cs pin failed\n", __func__);
		} else {
			gpio_direction_output(pdata->gpio_cs, 1);
		}
	}

	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		input_info(true, ts->dev, "DT:protocol_type=%d\n", pdata->protocol_type);
	}

	count = of_property_count_strings(dt, "himax,firmware_name");
	if (count <= 0) {
		pdata->i_CTPM_firmware_name = "Himax_firmware.bin";
	} else {
		of_property_read_string(dt, "himax,firmware_name", &pdata->i_CTPM_firmware_name);
	}
	input_info(true, ts->dev, "DT:firmware_name=%s\n", pdata->i_CTPM_firmware_name);

	pdata->pinctrl = devm_pinctrl_get(ts->dev);
	if (IS_ERR(pdata->pinctrl))
		input_err(true, ts->dev, "%s: could not get pinctrl\n", __func__);

	himax_vk_parser(dt, pdata);
	ts->pdata = pdata;
	return 0;
}

static ssize_t himax_spi_sync(struct himax_ts_data *ts, struct spi_message *message)
{
	int status;

	if (ts->pdata->gpio_cs > 0) {
		gpio_direction_output(ts->pdata->gpio_cs, 0);
//		gpio_set_value(ts->pdata->gpio_cs, 0);
		status = spi_sync(ts->spi, message);
		gpio_direction_output(ts->pdata->gpio_cs, 1);
//		gpio_set_value(ts->pdata->gpio_cs, 1);
	} else {
		status = spi_sync(ts->spi, message);
	}

	if (status == 0) {
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static int himax_spi_read(uint8_t *command, uint8_t command_len, uint8_t *data, uint32_t length, uint8_t toRetry)
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

		if (g_ts->pdata->gpio_cs > 0) {
//			I(" %s:gpio_cs=%d\n", __func__, ts->pdata->gpio_cs);
			gpio_direction_output(g_ts->pdata->gpio_cs, 0);
			error = spi_sync(g_ts->spi, &message);
			gpio_direction_output(g_ts->pdata->gpio_cs, 1);

		} else {
			error = spi_sync(g_ts->spi, &message);
		}

		if (unlikely(error)) {
			E("SPI read error: %d\n", error);
		} else{
			break;
		}
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

	return himax_spi_sync(g_ts, &m);

}

int himax_bus_read(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	int result = 0;
	uint8_t spi_format_buf[3];

	mutex_lock(&(g_ts->spi_lock));
	spi_format_buf[0] = 0xF3;
	spi_format_buf[1] = command;
	spi_format_buf[2] = 0x00;

	result = himax_spi_read(&spi_format_buf[0], 3, data, length, toRetry);
	mutex_unlock(&(g_ts->spi_lock));

	return result;
}

int himax_bus_write(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	uint8_t spi_format_buf[length + 2];
	int i = 0;
	int result = 0;

	mutex_lock(&(g_ts->spi_lock));
	spi_format_buf[0] = 0xF2;
	spi_format_buf[1] = command;

	for (i = 0; i < length; i++)
		spi_format_buf[i + 2] = data[i];

	result = himax_spi_write(spi_format_buf, length + 2);
	mutex_unlock(&(g_ts->spi_lock));

	return result;
}

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

int himax_bus_master_write(uint8_t *data, uint32_t length, uint8_t toRetry)
{
	uint8_t buf[length];

	struct spi_transfer	t = {
		.tx_buf	= buf,
		.len	= length,
	};
	struct spi_message	m;
	int result = 0;

	mutex_lock(&(g_ts->spi_lock));
	memcpy(buf, data, length);

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	result = himax_spi_sync(g_ts, &m);
	mutex_unlock(&(g_ts->spi_lock));

	return result;
}

void himax_int_enable(int enable)
{
	int irqnum = 0;
	irqnum = g_ts->hx_irq;

	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count = 1;
		g_ts->irq_enabled = 1;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count = 0;
		g_ts->irq_enabled = 0;
	}

	input_info(true, g_ts->dev, "irq_enable_count = %d\n", irq_enable_count);
}

#ifdef HX_RST_PIN_FUNC
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int himax_regulator_configure(struct himax_i2c_platform_data *pdata)
{
	int retval;
	/* struct i2c_client *client = g_ts->client; */
	pdata->vcc_dig = regulator_get(g_ts->dev, "vdd");

	if (IS_ERR(pdata->vcc_dig)) {
		E("%s: Failed to get regulator vdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_dig);
		return retval;
	}

	pdata->vcc_ana = regulator_get(g_ts->dev, "avdd");

	if (IS_ERR(pdata->vcc_ana)) {
		E("%s: Failed to get regulator avdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_ana);
		regulator_put(pdata->vcc_ana);
		return retval;
	}

	return 0;
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

		msleep(100);
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
	/* struct i2c_client *client = g_ts->client; */
	error = himax_regulator_configure(pdata);

	if (error) {
		E("Failed to intialize hardware\n");
		goto err_regulator_not_on;
	}

	if (pdata->lcd_gpio_reset >= 0) {
		error = gpio_request(pdata->lcd_gpio_reset, "himax-lcd-reset");

		if (error < 0) {
			E("%s: request lcd reset pin failed\n", __func__);
			//goto err_regulator_on;
		}

		error = gpio_direction_output(pdata->lcd_gpio_reset, 1);

		if (error) {
			E("unable to set lcd_rst direction for gpio [%d]\n",
					pdata->lcd_gpio_reset);
			//goto err_gpio_reset_req;
		}
		gpio_free(pdata->lcd_gpio_reset);
		I("%s: free lcd reset pin\n", __func__);
	}

#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");

		if (error) {
			E("unable to request gpio [%d]\n",
			  pdata->gpio_reset);
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
		goto err_gpio_reset_req;
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "hmx_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_power_on;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_gpio_irq_req;
		}

		g_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_power_on;
	}

	msleep(20);
#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_irq_req;
		}
	}

#endif
	return 0;
err_gpio_irq_req:

	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

err_power_on:
	himax_power_on(pdata, false);
err_gpio_reset_req:
#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

err_regulator_on:
#endif
err_regulator_not_on:
	return error;
}

#else
int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error = 0;
	/* struct i2c_client *client = g_ts->client; */

	if (pdata->lcd_gpio_reset >= 0) {
		error = gpio_request(pdata->lcd_gpio_reset, "himax-lcd-reset");

		if (error < 0) {
			E("%s: request lcd reset pin failed\n", __func__);
			//goto err_regulator_on;
		}

		error = gpio_direction_output(pdata->lcd_gpio_reset, 1);

		if (error) {
			E("unable to set lcd_rst direction for gpio [%d]\n",
					pdata->lcd_gpio_reset);
			//goto err_gpio_reset_req;
		}
		gpio_free(pdata->lcd_gpio_reset);
		I("%s: free lcd reset pin\n", __func__);
	}

#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");

		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			return error;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			return error;
		}
	}

#endif

	if (pdata->gpio_3v3_en >= 0) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");

		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			return error;
		}

		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en pin =%d\n", gpio_get_value(pdata->gpio_3v3_en));
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			return error;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_irq);
			return error;
		}

		g_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		return error;
	}

	// have to check delay?
	msleep(20);
#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			return error;
		}
	}

#endif
	return error;
}

#endif

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	himax_ts_isr_func((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

int himax_int_register_trigger(void)
{
	int ret = 0;
	struct himax_ts_data *ts = g_ts;

	if (ic_data->HX_INT_IS_EDGE) {
		input_info(true, g_ts->dev, "%s edge triiger falling\n ", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	} else {
		input_info(true, g_ts->dev, "%s level trigger low\n ", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT, HIMAX_common_NAME, ts);
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
	struct himax_ts_data *ts = g_ts;
	/* struct i2c_client *client = g_ts->client; */
	int ret = 0;
	ts->irq_enabled = 0;

	/* Work functon */
	if (g_ts->hx_irq) {/*INT mode*/
		ts->use_irq = 1;
		ret = himax_int_register_trigger();

		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			input_info(true, g_ts->dev, "%s: irq enabled at qpio: %d\n", __func__, g_ts->hx_irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(g_ts->hx_irq , 1);
#endif
		} else {
			ts->use_irq = 0;
			input_err(true, g_ts->dev, "%s: request_irq failed\n", __func__);
		}
	} else {
		input_info(true, g_ts->dev, "%s: g_ts->hx_irq is empty, use polling mode.\n", __func__);
	}

	if (!ts->use_irq) {/*if use polling mode need to disable HX_ESD_RECOVERY function*/
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		input_info(true, g_ts->dev, "%s: polling mode enabled\n", __func__);
	}

	return ret;
}

static int himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);
	I("%s: enter \n", __func__);
	himax_chip_common_suspend(ts);
	return 0;
}

static int himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);
	I("%s: enter \n", __func__);
	himax_chip_common_resume(ts);
	return 0;
}

#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self,
							unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
	    container_of(self, struct himax_ts_data, fb_notif);
	I(" %s\n", __func__);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts != NULL &&
	    ts->dev != NULL) {
		blank = evdata->data;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
			himax_common_resume(ts->dev);
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
#endif

int himax_chip_common_probe(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	int ret = 0;

	pr_info("[sec_input] %s Enter\n", __func__);

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		pr_err("[sec_input] %s: Full duplex not supported by host\n", __func__);
		return -EIO;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		pr_err("[sec_input] %s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	g_ts = ts;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;

	ts->spi = spi;
	mutex_init(&(ts->spi_lock));
	ts->dev = &spi->dev;

	dev_set_drvdata(&spi->dev, ts);
	spi_set_drvdata(spi, ts);

	ret = himax_chip_common_init();
	if (ret < 0) {
		input_err(true, g_ts->dev, "%s Done\n", __func__);
		goto err_init_failed;
	}
	input_info(true, g_ts->dev, "%s Done\n", __func__);

err_alloc_data_failed:
	return ret;

err_init_failed:
	g_ts = NULL;
	kfree(ts);
	return ret;

}

int himax_chip_common_remove(struct spi_device *spi)
{
	struct himax_ts_data *ts = spi_get_drvdata(spi);

	input_info(true, g_ts->dev, "%s Start\n", __func__);

	ts->spi = NULL;
	/* spin_unlock_irq(&ts->spi_lock); */
	spi_set_drvdata(spi, NULL);

	himax_chip_common_deinit();

	return 0;
}

void himax_chip_common_shutdown(struct spi_device *spi)
{
	struct himax_ts_data *ts = spi_get_drvdata(spi);

	input_info(true, g_ts->dev, "%s Start\n", __func__);

	himax_chip_common_suspend(ts);

	return;
}

static const struct dev_pm_ops himax_common_pm_ops = {
#if (!defined(CONFIG_FB))
	.suspend = himax_common_suspend,
	.resume  = himax_common_resume,
#endif
};

#ifdef CONFIG_OF
static struct of_device_id himax_match_table[] = {
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
		.of_match_table = himax_match_table,
	},
	.probe =	himax_chip_common_probe,
	.remove =	himax_chip_common_remove,
	.shutdown	= himax_chip_common_shutdown,
};

static int __init himax_common_init(void)
{
	pr_info("[sec_input] Himax common touch panel driver init\n");
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

