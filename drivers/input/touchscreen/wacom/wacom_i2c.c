/*
 *  wacom_i2c.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "wacom_i2c.h"
#include "wacom_i2c_flash.h"

int wacom_i2c_send(struct wacom_i2c *wac_i2c,
			  const char *buf, int count, bool mode)
{
	struct i2c_client *client;

	if (!wac_i2c->enabled) {
		dev_info(&wac_i2c->client->dev, "%s: is not enabled\n", __func__);
		return 0;
	}

	client = mode ? wac_i2c->client_boot : wac_i2c->client;
	if (wac_i2c->boot_mode && !mode) {
		dev_info(&client->dev,
			 "%s: failed to send\n",
			 __func__);
		return 0;
	}

	return i2c_master_send(client, buf, count);
}

int wacom_i2c_recv(struct wacom_i2c *wac_i2c,
			char *buf, int count, bool mode)
{
	struct i2c_client *client;

	if (!wac_i2c->enabled) {
		dev_info(&wac_i2c->client->dev, "%s: is not enabled\n", __func__);
		return 0;
	}

	client = mode ? wac_i2c->client_boot : wac_i2c->client;
	if (wac_i2c->boot_mode && !mode) {
		dev_info(&client->dev,
			 "%s: failed to received\n",
			 __func__);
		return 0;
	}

	return i2c_master_recv(client, buf, count);
}

static int wacom_i2c_set_query_data(struct wacom_i2c *wac_i2c, unsigned char *query_data, unsigned char length)
{
	unsigned char *data;

	data = kzalloc(length, GFP_KERNEL);
	if (!data) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed to allocate query data memory\n",
				__func__);
		return -ENOMEM;
	}

	memcpy(data, query_data, length);

	wac_i2c->wac_query_data->x_max = (u16)(data[2] + (data[1] << 8));
	wac_i2c->wac_query_data->y_max = (u16)(data[4] + (data[3] << 8));

	wac_i2c->wac_query_data->pressure_max = (u16)(data[6] + (data[5] << 8));
	wac_i2c->wac_query_data->mpu_type = data[9];
	wac_i2c->wac_query_data->bootloader_ver = data[10];
	wac_i2c->wac_query_data->tiltx_max = data[11];
	wac_i2c->wac_query_data->tilty_max = data[12];
	wac_i2c->wac_query_data->height_max = data[13];

	dev_info(&wac_i2c->client->dev,
			"%s: maxX:%d, maxY:%d, maxP:%d, fw_ver: 0x%X, mpu:0x%X, bootloader:0x%x, tiltX:%d, tiltY:%d, maxH:%d\n",
			__func__,wac_i2c->wac_query_data->x_max, wac_i2c->wac_query_data->y_max,
			wac_i2c->wac_query_data->pressure_max, wac_i2c->wac_query_data->fw_version_ic,
			wac_i2c->wac_query_data->mpu_type, wac_i2c->wac_query_data->bootloader_ver,
			wac_i2c->wac_query_data->tiltx_max,	wac_i2c->wac_query_data->tilty_max,
			wac_i2c->wac_query_data->height_max);
	wac_i2c->query_status = true;

/*
 * This code will be removed!!!!!
 * Over Wacom Firmware version 0025, Y axis is not inverted
 */
#ifdef CONFIG_SEC_TRLTE_PROJECT
	if (wac_i2c->wac_query_data->fw_version_ic >= 0x25)
		wac_i2c->wac_dt_data->y_invert = 0;
	else
		wac_i2c->wac_dt_data->y_invert = 1;
#endif

	kfree(data);

	return 0;

}

int wacom_i2c_query_no_wait(struct wacom_i2c *wac_i2c)
{
	int ret;
	u8 data[COM_READ_ALL_DATA_LENGTH] = {0, };
	int i = 0, j = 0;
	const int query_limit = 3;

	dev_info(&wac_i2c->client->dev,
			"%s: start\n", __func__);

	for (i = 0; i < query_limit; i++) {
		ret = wac_i2c->wacom_i2c_recv(wac_i2c, data, COM_READ_ALL_DATA_LENGTH, false);
		if ((ret < 0) || (COM_READ_ALL_DATA_LENGTH != ret)) {
			dev_err(&wac_i2c->client->dev,
				"%s: I2C recv failed(%d, %d)\n",
				__func__, i, ret);
			msleep(100);
			continue;
		}

		if (0x0f == data[NUM_OF_UNNECESSARY_DATA]) {
			wac_i2c->wac_query_data->fw_version_ic =
				((u16) data[NUM_OF_UNNECESSARY_DATA + 7] << 8) + (u16) data[NUM_OF_UNNECESSARY_DATA + 8];

			for (j = 0; j < 4; j++)
				dev_info(&wac_i2c->client->dev,
				       "%s:[%d] %X, %X, %X, %X, %X, %X, %X\n",
				       __func__, j,
				       data[(j * 7)], data[(j * 7) + 1], data[(j * 7) + 2], data[(j * 7) + 3],
				       data[(j * 7) + 4], data[(j * 7) + 5], data[(j * 7) + 6]);

			break;
		} else {
			for (j = 0; j < 4; j++)
				dev_info(&wac_i2c->client->dev,
				       "%s:[%d] %X, %X, %X, %X, %X, %X, %X\n",
				       __func__, j,
				       data[(j * 7)], data[(j * 7) + 1], data[(j * 7) + 2], data[(j * 7) + 3],
				       data[(j * 7) + 4], data[(j * 7) + 5], data[(j * 7) + 6]);

		}
	}

	if ((i == query_limit)) {
		dev_info(&wac_i2c->client->dev, "%s: failed\n", __func__);
		wac_i2c->query_status = false;

		wac_i2c->wac_query_data->x_max = (u16) wac_i2c->wac_dt_data->max_x;
		wac_i2c->wac_query_data->y_max = (u16) wac_i2c->wac_dt_data->max_y;
		wac_i2c->wac_query_data->pressure_max = (u16) wac_i2c->wac_dt_data->max_pressure;
		wac_i2c->wac_query_data->fw_version_ic = 0;

		return ret;
	}

	wacom_i2c_set_query_data(wac_i2c, &data[NUM_OF_UNNECESSARY_DATA], COM_QUERY_NUM);

	return wac_i2c->wac_query_data->fw_version_ic;
}

int wacom_i2c_query(struct wacom_i2c *wac_i2c)
{
	int ret;
	u8 buf;
	u8 data[COM_QUERY_NUM] = {0, };
	int i = 0, j = 0;
	const int query_limit = 3;

	buf = COM_QUERY;

	dev_info(&wac_i2c->client->dev,
			"%s: start\n", __func__);

	ret = wacom_i2c_query_no_wait(wac_i2c);
	if (ret > 0) {
		dev_info(&wac_i2c->client->dev, "%s: read query(no_wait):%X\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < query_limit; i++) {
		ret = wac_i2c->wacom_i2c_send(wac_i2c, &buf, 1, false);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
				 "%s: I2C send failed(%d)\n",
				 __func__, ret);
			continue;
		}
		msleep(100);
		ret = wac_i2c->wacom_i2c_recv(wac_i2c, data, COM_QUERY_NUM, false);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
				"%s: I2C recv failed(%d)\n",
				__func__, ret);
			continue;
		}
		dev_info(&wac_i2c->client->dev,
				"%s: %dth ret of wacom query=%d\n",
				__func__, i, ret);
		if (COM_QUERY_NUM != ret) {
			dev_info(&wac_i2c->client->dev,
			"%s: epen:failed to read i2c(%d)\n",
			__func__, ret);
			continue;
		}
			if (0x0f == data[0]) {
				wac_i2c->wac_query_data->fw_version_ic =
					((u16) data[7] << 8) + (u16) data[8];
				break;
			}
		}

	for (j = 0; j < 2; j++)
		dev_info(&wac_i2c->client->dev,
			   "%s:[%d] %X, %X, %X, %X, %X, %X, %X\n",
			   __func__, j,
			   data[(j * 7)], data[(j * 7) + 1], data[(j * 7) + 2], data[(j * 7) + 3],
			   data[(j * 7) + 4], data[(j * 7) + 5], data[(j * 7) + 6]);

	if (i == query_limit) {
		dev_info(&wac_i2c->client->dev, "%s: failed\n", __func__);
		wac_i2c->query_status = false;

		wac_i2c->wac_query_data->x_max = (u16) wac_i2c->wac_dt_data->max_x;
		wac_i2c->wac_query_data->y_max = (u16) wac_i2c->wac_dt_data->max_y;
		wac_i2c->wac_query_data->pressure_max = (u16) wac_i2c->wac_dt_data->max_pressure;
		wac_i2c->wac_query_data->fw_version_ic = 0;

		return ret;
	}

	wacom_i2c_set_query_data(wac_i2c, data, COM_QUERY_NUM);

	return wac_i2c->wac_query_data->fw_version_ic;
}

int wacom_i2c_modecheck(struct wacom_i2c *wac_i2c)
{
	u8 buf = COM_QUERY;
	int ret;
	int mode = WACOM_I2C_MODE_NORMAL;

	ret = wacom_i2c_send(wac_i2c, &buf, 1, false);
	if (ret < 0) {
		mode = WACOM_I2C_MODE_BOOT;
	}
	else{
		mode = WACOM_I2C_MODE_NORMAL;
	}
	dev_info(&wac_i2c->client->dev,
		"%s :I2C send at usermode(%d)\n", __func__, ret);
	return mode;
}

static void wacom_enable_irq(struct wacom_i2c *wac_i2c, bool enable)
{
	static int depth;
	mutex_lock(&wac_i2c->irq_lock);

	if (enable) {
		if (depth) {
			--depth;
			enable_irq(wac_i2c->irq);
#ifdef WACOM_PDCT_WORK_AROUND
			enable_irq(wac_i2c->irq_pdct);
#endif
		}
	} else {
		if (!depth) {
			++depth;
			disable_irq(wac_i2c->irq);
#ifdef WACOM_PDCT_WORK_AROUND
			disable_irq(wac_i2c->irq_pdct);
#endif
		}
	}
	mutex_unlock(&wac_i2c->irq_lock);
}

#define WACOM_USE_PMLDO
#ifdef WACOM_USE_PMLDO
static struct regulator *reg_l18;
#endif
static int wacom_start(struct wacom_i2c *wac_i2c)
{
	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	if (wac_i2c->wac_dt_data->gpio_pen_reset_n > 0)
		gpio_direction_output(wac_i2c->wac_dt_data->gpio_pen_reset_n, 1);

	if(wac_i2c->wac_dt_data->vdd_en > 0)
	gpio_direction_output(wac_i2c->wac_dt_data->vdd_en, 1);

#ifdef WACOM_USE_PMLDO
	{
		int ret;
		if (!regulator_is_enabled(reg_l18)) {
			ret = regulator_enable(reg_l18);
			if (ret) {
				dev_err(&wac_i2c->client->dev, "enable L18 failed, rc=%d\n", ret);
				return ret;
			}
			dev_info(&wac_i2c->client->dev, "wacom 3.3V on is finished.\n");
		} else{
			dev_err(&wac_i2c->client->dev, "wacom 3.3V is already on.\n");
		}
	}
#endif

	
	wac_i2c->power_enable = true;
	return 0;
}

static int wacom_stop(struct wacom_i2c *wac_i2c)
{

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	if (wac_i2c->wac_dt_data->gpio_pen_reset_n > 0)
		gpio_direction_output(wac_i2c->wac_dt_data->gpio_pen_reset_n, 0);

	if(wac_i2c->wac_dt_data->vdd_en > 0)
	gpio_direction_output(wac_i2c->wac_dt_data->vdd_en, 0);

#ifdef WACOM_USE_PMLDO
	{
		int ret;
		if (regulator_is_enabled(reg_l18)) {
			ret = regulator_disable(reg_l18);
			if (ret) {
				dev_err(&wac_i2c->client->dev, "disable L18 failed, rc=%d\n", ret);
				return ret;
			}
			dev_info(&wac_i2c->client->dev, "wacom 3.3V off is finished.\n");
		} else {
			dev_err(&wac_i2c->client->dev, "wacom 3.3V is already off.\n");
		}
	}
#endif

#ifdef WACOM_BOOSTER
	if(wac_i2c->wacom_booster->dvfs_set)
		wac_i2c->wacom_booster->dvfs_set(wac_i2c->wacom_booster, -1);

#endif

#ifdef USE_WACOM_BLOCK_KEYEVENT
	wac_i2c->touch_pressed = false;
	wac_i2c->touchkey_skipped = false;
#endif

	wac_i2c->power_enable = false;
	return 0;
}

static int wacom_reset_hw(struct wacom_i2c *wac_i2c)
{
	wac_i2c->wacom_stop(wac_i2c);
	msleep(30);
	wac_i2c->wacom_start(wac_i2c);
	msleep(200);

	return 0;
}

void forced_release(struct wacom_i2c *wac_i2c)
{
	dev_dbg(&wac_i2c->client->dev,"%s\n", __func__);
	input_report_abs(wac_i2c->input_dev, ABS_X, wac_i2c->last_x);
	input_report_abs(wac_i2c->input_dev, ABS_Y, wac_i2c->last_y);
	input_report_abs(wac_i2c->input_dev, ABS_PRESSURE, 0);
#ifdef USE_WACOM_TILT_HEIGH
	input_report_abs(wac_i2c->input_dev, ABS_DISTANCE, 0);
	input_report_abs(wac_i2c->input_dev, ABS_TILT_X, 0);
	input_report_abs(wac_i2c->input_dev, ABS_TILT_Y, 0);
#endif
	input_report_key(wac_i2c->input_dev, BTN_STYLUS, 0);
	input_report_key(wac_i2c->input_dev, BTN_TOUCH, 0);
	input_report_key(wac_i2c->input_dev, wac_i2c->tool, 0);

	input_sync(wac_i2c->input_dev);

	wac_i2c->last_x = 0;
	wac_i2c->last_y = 0;
	wac_i2c->pen_prox = 0;
	wac_i2c->pen_pressed = 0;
	wac_i2c->side_pressed = 0;
	wac_i2c->pen_pdct = PDCT_NOSIGNAL;

}

static void wacom_i2c_enable(struct wacom_i2c *wac_i2c)
{
	bool en = true;

	dev_info(&wac_i2c->client->dev,
			"%s\n", __func__);

#ifdef BATTERY_SAVING_MODE
	if (wac_i2c->battery_saving_mode
		&& wac_i2c->pen_insert)
		en = false;
#endif

	if (en) {
		if (!wac_i2c->power_enable)
			wac_i2c->wacom_start(wac_i2c);

		wac_i2c->compulsory_flash_mode(wac_i2c, false); /* compensation to protect from flash mode  */

		cancel_delayed_work_sync(&wac_i2c->resume_work);
		schedule_delayed_work(&wac_i2c->resume_work, HZ / 5);
	}
}

static void wacom_i2c_disable(struct wacom_i2c *wac_i2c)
{

	cancel_delayed_work_sync(&wac_i2c->resume_work);

	if (wac_i2c->power_enable) {
		wac_i2c->wacom_enable_irq(wac_i2c, false);

		/* release pen, if it is pressed */
		if (wac_i2c->pen_pressed || wac_i2c->side_pressed
			|| wac_i2c->pen_prox)
			forced_release(wac_i2c);

		wac_i2c->wacom_stop(wac_i2c);
		wac_i2c->compulsory_flash_mode(wac_i2c, false); /* compensation to protect from flash mode  */
	}
}

#ifdef WACOM_USE_SOFTKEY
static int keycode[] = {
	KEY_RECENT, KEY_BACK,
};

void wacom_i2c_softkey(struct wacom_i2c *wac_i2c, s16 key, s16 pressed)
{
	if (wac_i2c->pen_prox) {
		dev_info(&wac_i2c->client->dev,
				"%s: prox:%d, run release_hover\n",
				__func__, wac_i2c->pen_prox);

		input_report_abs(wac_i2c->input_dev, ABS_PRESSURE, 0);
#ifdef USE_WACOM_TILT_HEIGH
		input_report_abs(wac_i2c->input_dev, ABS_DISTANCE, 0);
		input_report_abs(wac_i2c->input_dev, ABS_TILT_X, 0);
		input_report_abs(wac_i2c->input_dev, ABS_TILT_Y, 0);
#endif
		input_report_key(wac_i2c->input_dev, BTN_STYLUS, 0);
		input_report_key(wac_i2c->input_dev, BTN_TOUCH, 0);
		input_report_key(wac_i2c->input_dev, wac_i2c->tool, 0);
		input_sync(wac_i2c->input_dev);

		wac_i2c->pen_prox = 0;
	}

#ifdef USE_WACOM_BLOCK_KEYEVENT
	wac_i2c->touchkey_skipped = false;
#endif
	input_report_key(wac_i2c->input_dev,
			keycode[key], pressed);
	input_sync(wac_i2c->input_dev);

#ifdef WACOM_BOOSTER
	if(wac_i2c->wacom_booster->dvfs_set)
		wac_i2c->wacom_booster->dvfs_set(wac_i2c->wacom_booster, pressed);
#endif

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	dev_info(&wac_i2c->client->dev,
			"%s: keycode:%d pressed:%d. pen_prox=%d\n",
			__func__, keycode[key], pressed, wac_i2c->pen_prox);
#else
	dev_info(&wac_i2c->client->dev,
			"%s: pressed:%d\n",
			__func__, pressed);
#endif
}
#endif

static bool wacom_i2c_coord_range(struct wacom_i2c *wac_i2c, s16 *x, s16 *y)
{
	if (wac_i2c->wac_dt_data->xy_switch) {
		if ((*x <= wac_i2c->wac_query_data->y_max) && (*y <= wac_i2c->wac_query_data->x_max))
			return true;
	} else {
		if ((*x <= wac_i2c->wac_query_data->x_max) && (*y <= wac_i2c->wac_query_data->y_max))
			return true;
	}
	return false;
}

#ifdef USE_WACOM_LCD_WORKAROUND
void wacom_i2c_write_vsync(struct wacom_i2c *wac_i2c)
{
	int retval = 1;

	if (wac_i2c->wait_done) {
		dev_info(&wac_i2c->client->dev, "%s write %d\n", __func__, wac_i2c->vsync);
		retval = ldi_fps(wac_i2c->vsync);
		if (!retval)
			dev_info(&wac_i2c->client->dev, "%s failed\n", __func__);
		wac_i2c->wait_done = false;
		schedule_delayed_work(&wac_i2c->read_vsync_work,
					msecs_to_jiffies(wac_i2c->delay_time * 1000));

	}
}
#endif

int wacom_i2c_coord(struct wacom_i2c *wac_i2c)
{
	bool prox = false;
	int ret = 0;
	u8 *data;
	int rubber, stylus;
	static s16 x, y, pressure;
	static s16 tmp;
	int rdy = 0;
	u8 gain = 0;
	s8 tilt_x = 0, tilt_y = 0;

#ifdef WACOM_USE_SOFTKEY
	static s16 softkey, pressed, keycode;
#endif

	data = wac_i2c->wac_query_data->data;

	ret = wac_i2c->wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM, false);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed to read i2c.L%d\n",
				__func__, __LINE__);
		return ret;
	}

#ifdef USE_WACOM_LCD_WORKAROUND
	if (!data[1] && !data[2] && !data[3] && !data[4]) {
		if ((data[0] >> 7 == 0) && wac_i2c->boot_done && (data[10] != 0) && (data[11] != 0)) {
			wac_i2c->vsync = 2000000 * 1000 / (((data[10] << 8) | data[11]) + 1);
			wacom_i2c_write_vsync(wac_i2c);
		}
	}
#endif

	if (data[0] & 0x80) {
		/* enable emr device */
#ifdef WACOM_USE_SOFTKEY
		softkey = !!(data[5] & 0x80);
		if (softkey) {
			pressed = !!(data[5] & 0x40);
			keycode = (data[5] & 0x30) >> 4;
#ifdef USE_WACOM_BLOCK_KEYEVENT
			if (wac_i2c->touch_pressed) {
				if (pressed) {
					wac_i2c->touchkey_skipped = true;
					dev_info(&wac_i2c->client->dev,
							"%s : skip key press\n", __func__);
				} else {
					wac_i2c->touchkey_skipped = false;
					dev_info(&wac_i2c->client->dev,
							"%s : skip key release\n", __func__);
				}
			} else {
				if (wac_i2c->touchkey_skipped) {
					dev_info(&wac_i2c->client->dev,
							"%s: skipped touchkey event[%d]\n",
							__func__, pressed);
					if (!pressed)
						wac_i2c->touchkey_skipped = false;
				} else {
					wacom_i2c_softkey(wac_i2c, keycode, pressed);
				}
			}
#else
			wacom_i2c_softkey(wac_i2c, keycode, pressed);
#endif
			return 0;
		}
#endif

		if (!wac_i2c->pen_prox) {

#ifdef WACOM_PDCT_WORK_AROUND
			if (gpio_get_value(wac_i2c->wac_dt_data->gpio_pen_pdct)) {
				dev_info(&wac_i2c->client->dev,
						"%s: IC interrupt ocurrs, but PDCT HIGH, return.\n",
						__func__);
				return 0;
			}
#endif

#ifdef WACOM_BOOSTER
			if(wac_i2c->wacom_booster->dvfs_set)
				wac_i2c->wacom_booster->dvfs_set(wac_i2c->wacom_booster, 1);
#endif
			wac_i2c->pen_prox = 1;

			if (data[0] & 0x40)
				wac_i2c->tool = BTN_TOOL_RUBBER;
			else
				wac_i2c->tool = BTN_TOOL_PEN;
		}
		prox = !!(data[0] & 0x10);
		stylus = !!(data[0] & 0x20);
		rubber = !!(data[0] & 0x40);
		rdy = !!(data[0] & 0x80);

		x = ((u16) data[1] << 8) + (u16) data[2];
		y = ((u16) data[3] << 8) + (u16) data[4];
		pressure = ((u16) data[5] << 8) + (u16) data[6];

		gain = data[7];

		if (wac_i2c->wac_dt_data->x_invert)
			x = wac_i2c->wac_query_data->x_max - x;
		if (wac_i2c->wac_dt_data->y_invert)
			y = wac_i2c->wac_query_data->y_max - y;

		if (wac_i2c->wac_dt_data->xy_switch) {
			tmp = x;
			x = y;
			y = tmp;
		}

		if (wacom_i2c_coord_range(wac_i2c, &x, &y)) {
			input_report_abs(wac_i2c->input_dev, ABS_X, x);
			input_report_abs(wac_i2c->input_dev, ABS_Y, y);
			input_report_abs(wac_i2c->input_dev,
					 ABS_PRESSURE, pressure);

#ifdef USE_WACOM_TILT_HEIGH
			input_report_abs(wac_i2c->input_dev,
				ABS_DISTANCE, gain);

			tilt_x = (s8)data[9];
			tilt_y = -(s8)data[8];
			input_report_abs(wac_i2c->input_dev,
				ABS_TILT_X, tilt_x);
			input_report_abs(wac_i2c->input_dev,
				ABS_TILT_Y, tilt_y);
#endif

			input_report_key(wac_i2c->input_dev,
					 BTN_STYLUS, stylus);
			input_report_key(wac_i2c->input_dev, BTN_TOUCH, prox);
			input_report_key(wac_i2c->input_dev, wac_i2c->tool, 1);
			input_sync(wac_i2c->input_dev);
			wac_i2c->last_x = x;
			wac_i2c->last_y = y;

			if (prox && !wac_i2c->pen_pressed) {
#ifdef USE_WACOM_BLOCK_KEYEVENT
				wac_i2c->touch_pressed = true;
#endif

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_info(&wac_i2c->client->dev,
						"%s: pressed. x:%d, y:%d, tx:%d, ty:%d, h:%d, p:%d, data[0]:%X\n",
						__func__, x, y, tilt_x, tilt_y, gain, pressure, data[0]);
#else
				dev_info(&wac_i2c->client->dev,
						"%s: pressed\n",
						__func__);
#endif
			} else if (!prox && wac_i2c->pen_pressed) {
#ifdef USE_WACOM_BLOCK_KEYEVENT
				schedule_delayed_work(&wac_i2c->touch_pressed_work,
					msecs_to_jiffies(wac_i2c->key_delay_time));
#endif
				dev_info(&wac_i2c->client->dev,
						"%s: released\n",
						__func__);
			}

			wac_i2c->pen_pressed = prox;

			if (stylus && !wac_i2c->side_pressed)
				dev_info(&wac_i2c->client->dev,
						"%s: side on\n",
						__func__);
			else if (!stylus && wac_i2c->side_pressed)
				dev_info(&wac_i2c->client->dev,
						"%s: side off\n",
						__func__);

			wac_i2c->side_pressed = stylus;
		}
	} else {

		if (wac_i2c->pen_prox) {
			/* input_report_abs(wac->input_dev,
			   ABS_X, x); */
			/* input_report_abs(wac->input_dev,
			   ABS_Y, y); */

			input_report_abs(wac_i2c->input_dev, ABS_PRESSURE, 0);
#ifdef USE_WACOM_TILT_HEIGH
			input_report_abs(wac_i2c->input_dev, ABS_DISTANCE, 0);
#endif
			input_report_key(wac_i2c->input_dev, BTN_STYLUS, 0);
			input_report_key(wac_i2c->input_dev, BTN_TOUCH, 0);
			input_report_key(wac_i2c->input_dev, wac_i2c->tool, 0);

			input_sync(wac_i2c->input_dev);

#ifdef USE_WACOM_BLOCK_KEYEVENT
			schedule_delayed_work(&wac_i2c->touch_pressed_work,
					msecs_to_jiffies(wac_i2c->key_delay_time));
#endif
			dev_info(&wac_i2c->client->dev,
					"%s: is out\n",
					__func__);
		}
		wac_i2c->pen_prox = 0;
		wac_i2c->pen_pressed = 0;
		wac_i2c->side_pressed = 0;
		wac_i2c->last_x = 0;
		wac_i2c->last_y = 0;

#ifdef WACOM_BOOSTER
		if(wac_i2c->wacom_booster->dvfs_set)
			wac_i2c->wacom_booster->dvfs_set(wac_i2c->wacom_booster, 0);

#endif
	}

	return 0;
}

#ifdef WACOM_HAVE_FWE_PIN
/* bool en is TRUE ? boot flash mode : boot normal mode  */
static void wacom_compulsory_flash_mode(struct wacom_i2c *wac_i2c, bool en)
{
	int retry = 100;
	int status = 0;
	while (retry--) {
		gpio_direction_output(wac_i2c->wac_dt_data->gpio_pen_fwe1, en ? 1 : 0);

		status = gpio_get_value(wac_i2c->wac_dt_data->gpio_pen_fwe1);
		if (status == en) {
			dev_info(&wac_i2c->client->dev, "%s: FWE1 is %s, status:%d\n",
					__func__, en ? "HIGH" : "LOW", status);
			break;
		} else {
			dev_err(&wac_i2c->client->dev, "%s: FWE1 is not set [%s]/[%d]\n",
					__func__, en ? "HIGH" : "LOW", status);
			usleep_range(100, 110);
		}
	}
}
#endif

static irqreturn_t wacom_interrupt(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;

	wacom_i2c_coord(wac_i2c);

	return IRQ_HANDLED;
}

#if defined(WACOM_PDCT_WORK_AROUND)
static irqreturn_t wacom_interrupt_pdct(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;

	if (wac_i2c->query_status == false) {
		dev_info(&wac_i2c->client->dev, "%s: query_read_failed\n", __func__);
		return IRQ_HANDLED;
	}
	wac_i2c->pen_pdct = gpio_get_value(wac_i2c->wac_dt_data->gpio_pen_pdct);

	dev_info(&wac_i2c->client->dev, "%s: pdct %d(%d) [%s]\n",
			__func__, wac_i2c->pen_pdct, wac_i2c->pen_prox,
			wac_i2c->pen_pdct ? "Released" : "Pressed");

	return IRQ_HANDLED;
}
#endif

#ifdef WACOM_PEN_DETECT
static void pen_insert_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, pen_insert_dwork.work);

	dev_info(&wac_i2c->client->dev, "%s: %d\n", __func__, __LINE__);

	if (wac_i2c->init_fail)
		return;
	wac_i2c->pen_insert = !gpio_get_value(wac_i2c->gpio_pen_insert);

	dev_info(&wac_i2c->client->dev, "%s: pen %s\n",
		__func__, wac_i2c->pen_insert ? "instert" : "remove");

	input_report_switch(wac_i2c->input_dev,
		SW_PEN_INSERT, !wac_i2c->pen_insert);
	input_sync(wac_i2c->input_dev);

#ifdef BATTERY_SAVING_MODE
	if (wac_i2c->pen_insert) {
		if (wac_i2c->battery_saving_mode)
			wac_i2c->wacom_i2c_disable(wac_i2c);
	} else {
		wac_i2c->wacom_i2c_enable(wac_i2c);
	}
#endif
}

static irqreturn_t wacom_pen_detect(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	bool temp_gpio1,temp_gpio2; 
	
	dev_info(&wac_i2c->client->dev, "%s: %d\n", __func__, __LINE__);
	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);

	temp_gpio1 = !gpio_get_value(wac_i2c->gpio_pen_insert);
	msleep(10); // call noise is 6~7ms
	temp_gpio2 = !gpio_get_value(wac_i2c->gpio_pen_insert);

	if(temp_gpio1 != temp_gpio2){
		dev_info(&wac_i2c->client->dev, "%s: happened noise. old:%d t1:%d, t2:%d\n", __func__, wac_i2c->pen_insert, temp_gpio1, temp_gpio2);
	}
	
	if(wac_i2c->pen_insert != temp_gpio2){
		schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 20);

		wake_lock_timeout(&wac_i2c->wakelock_pen_insert, 3 * HZ);
	}else{
		dev_info(&wac_i2c->client->dev, "%s: ignored. state same.\n", __func__);
	}

	return IRQ_HANDLED;
}
#endif

static int wacom_i2c_input_open(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);

	dev_info(&wac_i2c->client->dev,
			"%s\n", __func__);

	wac_i2c->wacom_i2c_enable(wac_i2c);
	wac_i2c->enabled = true;
	return 0;
}

static void wacom_i2c_input_close(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);

	if (wake_lock_active(&wac_i2c->wakelock)) {
		dev_err(&wac_i2c->client->dev, "%s: wakelock active\n",
					__func__);
		return;
	}

	dev_info(&wac_i2c->client->dev,
			"%s\n", __func__);

	wac_i2c->wacom_i2c_disable(wac_i2c);
	wac_i2c->enabled = false;
}


static void wacom_i2c_set_input_values(struct i2c_client *client,
				       struct wacom_i2c *wac_i2c,
				       struct input_dev *input_dev)
{
	/*Set input values before registering input device */

	input_dev->name = "sec_e-pen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	input_dev->evbit[0] |= BIT_MASK(EV_SW);
	input_set_capability(input_dev, EV_SW, SW_PEN_INSERT);
#ifdef WACOM_PEN_DETECT
	input_dev->open = wacom_i2c_input_open;
	input_dev->close = wacom_i2c_input_close;
#endif

	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_PEN, input_dev->keybit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
	__set_bit(BTN_STYLUS, input_dev->keybit);
	__set_bit(KEY_UNKNOWN, input_dev->keybit);
	__set_bit(KEY_PEN_PDCT, input_dev->keybit);
#ifdef USE_WACOM_TILT_HEIGH
	__set_bit(ABS_DISTANCE, input_dev->absbit);
	__set_bit(ABS_TILT_X, input_dev->absbit);
	__set_bit(ABS_TILT_Y, input_dev->absbit);
#endif
	/*  __set_bit(BTN_STYLUS2, input_dev->keybit); */
	/*  __set_bit(ABS_MISC, input_dev->absbit); */

	/*softkey*/
#ifdef WACOM_USE_SOFTKEY
	__set_bit(KEY_RECENT, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
#endif
}

static void wacom_i2c_resume_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
	    container_of(work, struct wacom_i2c, resume_work.work);

	if (wac_i2c->init_fail) {
		dev_info(&wac_i2c->client->dev, "%s: is init failed\n", __func__);
		return;
	}

	if (!wac_i2c->enabled) {
		dev_info(&wac_i2c->client->dev, "%s: is not enabled, turn off IC\n", __func__);

		return;
	}

	wac_i2c->wacom_enable_irq(wac_i2c, true);

	if (wacom_i2c_modecheck(wac_i2c))
		wacom_i2c_usermode(wac_i2c);

	dev_info(&wac_i2c->client->dev,
			"%s\n", __func__);
}

#ifdef USE_WACOM_BLOCK_KEYEVENT
static void wacom_i2c_touch_pressed_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
	    container_of(work, struct wacom_i2c, touch_pressed_work.work);

	cancel_delayed_work(&wac_i2c->touch_pressed_work);
	wac_i2c->touch_pressed = false;
}
#endif

#ifdef USE_WACOM_LCD_WORKAROUND
static void wacom_i2c_read_vsync_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
	    container_of(work, struct wacom_i2c, read_vsync_work.work);

	wac_i2c->wait_done = true;
}

static void wacom_i2c_boot_done_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
	    container_of(work, struct wacom_i2c, boot_done_work.work);

	wac_i2c->boot_done = true;
}
#endif

static int wacom_firmware_update(struct wacom_i2c *wac_i2c)
{
	int ret = 0;

	ret = wacom_load_fw_from_req_fw(wac_i2c);
	if (ret)
		goto failure;

	if(wac_i2c->wac_dt_data->wacom_firmup_flag != 1){
		dev_info(&wac_i2c->client->dev,
				"%s: firmup pass (~hw rev03), firmup_flag=%d\n",
				__func__, wac_i2c->wac_dt_data->wacom_firmup_flag);

		return 0;
	}
	
	if (wac_i2c->wac_query_data->fw_version_ic < wac_i2c->wac_query_data->fw_version_bin) {
		/*start firm update*/
		dev_info(&wac_i2c->client->dev,
				"%s: Start firmware flashing (kernel image).\n",
				__func__);
		wake_lock(&wac_i2c->wakelock);
		mutex_lock(&wac_i2c->lock);
		wac_i2c->wacom_enable_irq(wac_i2c, false);
		wac_i2c->wac_query_data->firm_update_status = 1;
		ret = wacom_i2c_firm_update(wac_i2c);
		if (ret)
			goto update_err;
		wac_i2c->fw_data= NULL;
		wac_i2c->wacom_i2c_query(wac_i2c);
		wac_i2c->wac_query_data->firm_update_status = 2;
		wac_i2c->wacom_enable_irq(wac_i2c, true);
		mutex_unlock(&wac_i2c->lock);
 		wake_unlock(&wac_i2c->wakelock);
	} else {
		dev_info(&wac_i2c->client->dev,
			"%s: firmware update does not need.\n",
			__func__);
	}
	return ret;

update_err:
	wac_i2c->fw_data= NULL;
	wac_i2c->wac_query_data->firm_update_status = -1;
	wac_i2c->wacom_enable_irq(wac_i2c, true);
	mutex_unlock(&wac_i2c->lock);
 	wake_unlock(&wac_i2c->wakelock);
failure:
	return ret;
}
static void wacom_init_abs_params(struct wacom_i2c *wac_i2c)
{
#ifdef USE_WACOM_TILT_HEIGH
	int temp, temp1;
#endif
	if (wac_i2c->wac_dt_data->xy_switch) {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, 0,
			wac_i2c->wac_query_data->y_max, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, 0,
			wac_i2c->wac_query_data->x_max, 4, 0);
	} else {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, 0,
			wac_i2c->wac_query_data->x_max, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, 0,
			wac_i2c->wac_query_data->y_max, 4, 0);
	}

	input_set_abs_params(wac_i2c->input_dev, ABS_PRESSURE, 0,
		wac_i2c->wac_query_data->pressure_max, 0, 0);

#ifdef USE_WACOM_TILT_HEIGH
	temp = wac_i2c->wac_query_data->tiltx_max;
	temp1 = temp * -1;

	input_set_abs_params(wac_i2c->input_dev, ABS_TILT_X, temp1,
		temp, 0, 0);

	temp = wac_i2c->wac_query_data->tilty_max;
	temp1 = temp * -1;

	input_set_abs_params(wac_i2c->input_dev, ABS_TILT_Y, temp1,
		temp, 0, 0);
 
	input_set_abs_params(wac_i2c->input_dev, ABS_DISTANCE, 0,
		wac_i2c->wac_query_data->height_max, 0, 0);
#endif
}

static void wacom_request_gpio(struct wacom_devicetree_data *wac_dt_data)
{
	int ret;
	pr_info("%s: request gpio\n", __func__);

	ret = gpio_request(wac_dt_data->gpio_int, "wacom_irq");
	if (ret) {
		pr_err("%s: unable to request wacom_irq [%d]\n",
				__func__, wac_dt_data->gpio_int);
		return;
	}

	if(wac_dt_data->vdd_en > 0){
	ret = gpio_request(wac_dt_data->vdd_en, "wacom_vdd_en");
	if (ret) {
		pr_err("%s: unable to request wacom_vdd_en [%d]\n",
				__func__, wac_dt_data->vdd_en);
		return;
	}
	}

	if (wac_dt_data->gpio_pen_reset_n > 0) {
		ret = gpio_request(wac_dt_data->gpio_pen_reset_n, "wacom_pen_reset_n");
		if (ret) {
			pr_err("%s: unable to request wacom_pen_reset_n [%d]\n",
				__func__, wac_dt_data->gpio_pen_reset_n);
			return;
		}

		gpio_tlmm_config(GPIO_CFG(wac_dt_data->gpio_pen_reset_n, 0,
			GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	}

	ret = gpio_request(wac_dt_data->gpio_pen_pdct, "pen_pdct-gpio");
	if (ret) {
		pr_err("%s: unable to request pen_pdct-gpio [%d]\n",
				__func__, wac_dt_data->gpio_pen_pdct);
		return;
	}

	ret = gpio_request(wac_dt_data->gpio_pen_fwe1, "wacom_pen_fwe1");
	if (ret) {
		pr_err("%s: unable to request wacom_pen_fwe1 [%d]\n",
				__func__, wac_dt_data->gpio_pen_fwe1);
	}

	gpio_tlmm_config(GPIO_CFG(wac_dt_data->gpio_pen_fwe1, 0,
		GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_direction_output(wac_dt_data->gpio_pen_fwe1, 0);

	ret = gpio_request(wac_dt_data->gpio_pen_insert, "wacom_pen_insert");
	if (ret) {
		pr_err("[WACOM]%s: unable to request wacom_pen_insert [%d]\n",
				__func__, wac_dt_data->gpio_pen_insert);
		return;
	}

}

#ifdef CONFIG_OF
static int wacom_get_dt_coords(struct device *dev, char *name,
				struct wacom_devicetree_data *wac_dt_data)
{
	u32 coords[WACOM_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != WACOM_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "%s: Unable to read %s\n", __func__, name);
		return rc;
	}

	if (strncmp(name, "wacom,panel-coords",
			sizeof("wacom,panel-coords")) == 0) {
		wac_dt_data->x_invert = coords[0];
		wac_dt_data->y_invert = coords[1];
/*
 * below max_x, max_y, min_x, min_y, min_pressure, max_pressure values will be removed.
 * using received value from wacom query data.
 */
		wac_dt_data->min_x = coords[2];
		wac_dt_data->max_x = coords[3];
		wac_dt_data->min_y = coords[4];
		wac_dt_data->max_y = coords[5];
		wac_dt_data->xy_switch = coords[6];
		wac_dt_data->min_pressure = coords[7];
		wac_dt_data->max_pressure = coords[8];

		pr_err("%s: x_invert = %d, y_invert = %d, xy_switch = %d\n",
				__func__, wac_dt_data->x_invert,
				wac_dt_data->y_invert, wac_dt_data->xy_switch);
	} else {
		dev_err(dev, "%s: nsupported property %s\n", __func__, name);
		return -EINVAL;
	}

	return 0;
}

static int wacom_parse_dt(struct device *dev,
			struct wacom_devicetree_data *wac_dt_data)
{
	int rc;
	struct device_node *np = dev->of_node;

	rc = wacom_get_dt_coords(dev, "wacom,panel-coords", wac_dt_data);
	if (rc)
		return rc;

	/* regulator info */
	wac_dt_data->i2c_pull_up = of_property_read_bool(np, "wacom,i2c-pull-up");
	wac_dt_data->vdd_en = of_get_named_gpio(np, "vdd_en-gpio", 0);

	/* reset, irq gpio info */
	wac_dt_data->gpio_int = of_get_named_gpio_flags(np, "wacom,irq-gpio",
				0, &wac_dt_data->irq_gpio_flags);
	wac_dt_data->gpio_pen_fwe1 = of_get_named_gpio_flags(np,
		"wacom,pen_fwe1-gpio", 0, &wac_dt_data->pen_fwe1_gpio_flags);
	wac_dt_data->gpio_pen_reset_n = of_get_named_gpio_flags(np,
		"wacom,reset_n-gpio", 0, &wac_dt_data->pen_reset_n_gpio_flags);
	wac_dt_data->gpio_pen_pdct = of_get_named_gpio_flags(np,
		"wacom,pen_pdct-gpio", 0, &wac_dt_data->pen_pdct_gpio_flags);
	wac_dt_data->gpio_pen_insert = of_get_named_gpio(np, "wacom,sense-gpio", 0);

	rc = of_property_read_string(np, "wacom,basic_model", &wac_dt_data->basic_model);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read wacom,basic_model\n", __func__);
		wac_dt_data->basic_model = "NULL";
	}

	rc = of_property_read_u32(np, "wacom,ic_mpu_ver", &wac_dt_data->ic_mpu_ver);
	if (rc < 0)
		dev_info(dev, "%s: Unable to read wacom,ic_mpu_ver\n", __func__);

	/*Change below if irq is needed */
	rc = of_property_read_u32(np, "wacom,irq_flags", &wac_dt_data->irq_flags);
	if (rc < 0)
		dev_info(dev, "%s: Unable to read wacom,irq_flags\n", __func__);

	//wac_dt_data->wacom_firmup_flag = of_property_read_bool(np, "wacom,firmup_flag");
	rc = of_property_read_u32(np, "wacom,firmup_flag", &wac_dt_data->wacom_firmup_flag);
	if (rc < 0)
		dev_info(dev, "%s: Unable to read wacom,firmup_flag\n", __func__);


	pr_err("%s: en:%d, fwe1: %d, reset_n: %d, pdct: %d, insert: %d, model: %s, mpu: %x, irq_f=%x, fu_f=%d\n",
			__func__, wac_dt_data->vdd_en, wac_dt_data->gpio_pen_fwe1, wac_dt_data->gpio_pen_reset_n,
			wac_dt_data->gpio_pen_pdct, wac_dt_data->gpio_pen_insert, 
			wac_dt_data->basic_model, wac_dt_data->ic_mpu_ver, 
			wac_dt_data->irq_flags, wac_dt_data->wacom_firmup_flag);

	return 0;
}
#else
static int wacom_parse_dt(struct device *dev,
			struct wacom_devicetree_data *wac_dt_data)
{
	return -ENODEV;
}
#endif

static int wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	free_irq(client->irq, wac_i2c);
#ifdef WACOM_PDCT_WORK_AROUND
	free_irq(wac_i2c->irq_pdct, wac_i2c);
#endif
	free_irq(wac_i2c->irq_pen_insert, wac_i2c);

	cancel_delayed_work_sync(&wac_i2c->resume_work);
	cancel_delayed_work_sync(&wac_i2c->touch_pressed_work);
#ifdef USE_WACOM_LCD_WORKAROUND
	cancel_delayed_work_sync(&wac_i2c->read_vsync_work);
	cancel_delayed_work_sync(&wac_i2c->boot_done_work);
#endif
	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);
#ifdef WACOM_BOOSTER
	kfree(wac_i2c->wacom_booster);
#endif
	mutex_destroy(&wac_i2c->lock);
	mutex_destroy(&wac_i2c->irq_lock);

	wacom_factory_release(wac_i2c->dev);
	input_unregister_device(wac_i2c->input_dev);
	input_free_device(wac_i2c->input_dev);

	wake_lock_destroy(&wac_i2c->wakelock);
	wake_lock_destroy(&wac_i2c->wakelock_pen_insert);

	kfree(wac_i2c);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define wacom_i2c_suspend	NULL
#define wacom_i2c_resume	NULL

static void wacom_i2c_early_suspend(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c =
	    container_of(h, struct wacom_i2c, early_suspend);

	dev_info(&wac_i2c->client->dev,
			"%s\n", __func__);

	wac_i2c->wacom_i2c_disable(wac_i2c);
}

static void wacom_i2c_late_resume(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c =
	    container_of(h, struct wacom_i2c, early_suspend);

	dev_info(&wac_i2c->client->dev,
			"%s\n", __func__);

	wac_i2c->wacom_i2c_enable(wac_i2c);
}
#endif

static int wacom_i2c_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct wacom_devicetree_data *wac_dt_data;
	struct wacom_i2c *wac_i2c;
	struct input_dev *input;
	int ret = 0;
	int error;
	int fw_ver;
	unsigned char buffer_clear[COM_READ_ALL_DATA_LENGTH] = {0, };

	pr_err("%s\n", __func__);
/*
	if (boot_mode_recovery == 1) {
		dev_err(&client->dev, "%s: recovery mode\n", __func__);
		return -EIO;
	}

	if (!get_lcd_attached()) {
		dev_err(&client->dev, "%s: LCD is not attached\n",__func__);
		return -EIO;
	}
*/
	/*Check I2C functionality */
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!ret) {
		pr_err("%s: No I2C functionality found\n", __func__);
		ret = -ENODEV;
		goto err_i2c_fail;
	}

	/*Obtain kernel memory space for wacom i2c */
	if (client->dev.of_node) {
		wac_dt_data = devm_kzalloc(&client->dev,
			sizeof(struct wacom_devicetree_data), GFP_KERNEL);
		if (!wac_dt_data) {
				dev_err(&client->dev,
						"%s: Failed to allocate memory\n",
						__func__);
			return -ENOMEM;
		}
		error = wacom_parse_dt(&client->dev, wac_dt_data);
		if (error)
			return error;
	} else {
		wac_dt_data = client->dev.platform_data;
		if (!wac_dt_data) {
			dev_err(&client->dev, "%s: no wac_dt_data\n", __func__);
			ret = -ENODEV;
			goto err_i2c_fail;
		}
	}

	wacom_request_gpio(wac_dt_data);

	wac_i2c = kzalloc(sizeof(struct wacom_i2c), GFP_KERNEL);
	if (!wac_i2c) {
		dev_err(&client->dev,
				"%s: failed to allocate wac_i2c.\n",
				__func__);
		ret = -ENOMEM;
		goto err_i2c_fail;
	}

	wac_i2c->client_boot = i2c_new_dummy(client->adapter,
		WACOM_I2C_BOOT);
	if (!wac_i2c->client_boot) {
		dev_err(&client->dev, "Fail to register sub client[0x%x]\n",
			 WACOM_I2C_BOOT);
	}

	input = input_allocate_device();
	if (!input) {
		dev_err(&client->dev,
				"%s: failed to allocate input device.\n",
				__func__);
		ret = -ENOMEM;
		goto err_freemem;
	}

	wacom_i2c_set_input_values(client, wac_i2c, input);

	wac_i2c->wac_dt_data = wac_dt_data;
	wac_i2c->input_dev = input;
	wac_i2c->client = client;

#ifdef WACOM_HAVE_FWE_PIN
	wac_i2c->compulsory_flash_mode = wacom_compulsory_flash_mode;
#endif
	wac_i2c->reset_platform_hw = wacom_reset_hw;
	wac_i2c->wacom_start = wacom_start;
	wac_i2c->wacom_stop = wacom_stop;
	wac_i2c->wacom_i2c_send = wacom_i2c_send;
	wac_i2c->wacom_i2c_recv = wacom_i2c_recv;
	wac_i2c->wacom_i2c_query = wacom_i2c_query;
	wac_i2c->wacom_i2c_enable = wacom_i2c_enable;
	wac_i2c->wacom_i2c_disable = wacom_i2c_disable;
	wac_i2c->wacom_enable_irq = wacom_enable_irq;

	wac_i2c->wac_query_data = kzalloc(sizeof(struct wacom_query_data), GFP_KERNEL);
	if (!wac_i2c->wac_query_data) {
		dev_err(&client->dev,
				"%s: failed to allocate wac_i2c.\n",
				__func__);
		ret = -ENOMEM;
		goto err_freemem;
	}
	/* Set default command state to QUERY */
	wac_i2c->wac_query_data->comstat = COM_QUERY;

	client->irq = gpio_to_irq(wac_i2c->wac_dt_data->gpio_int);
	dev_info(&wac_i2c->client->dev, "%s: gpio_to_irq : %d\n",
				__func__, client->irq);
	wac_i2c->irq = client->irq;

	/*Set client data */
	i2c_set_clientdata(client, wac_i2c);
	i2c_set_clientdata(wac_i2c->client_boot, wac_i2c);

	wake_lock_init(&wac_i2c->wakelock, WAKE_LOCK_SUSPEND, "wacom_wakelock");
	wake_lock_init(&wac_i2c->wakelock_pen_insert, WAKE_LOCK_SUSPEND, "wacom_pen_wakelock");

#ifdef WACOM_USE_PMLDO
	reg_l18 = regulator_get(&client->dev, "vcc_en");
	if (IS_ERR(reg_l18)) {
		dev_err(&client->dev, "%s, could not get 8084_l18, rc = %ld=n",__func__,PTR_ERR(reg_l18));
	}else{
		ret = regulator_set_voltage(reg_l18, 3300000, 3300000);
		if (ret) {
			dev_err(&client->dev, "%s: unable to set ldo18 voltage to 3.3V\n", __func__);
		}
	}
	dev_info(&wac_i2c->client->dev, "%s: vcc_en ldo18 is done %d\n", __func__, __LINE__);
#endif

#ifdef WACOM_BOOSTER
	wac_i2c->wacom_booster = kzalloc(sizeof(struct input_booster), GFP_KERNEL);
	if (!wac_i2c->wacom_booster) {
		dev_err(&wac_i2c->client->dev,
			"%s: Failed to alloc mem for wacom_booster\n", __func__);
		goto err_get_wacom_booster;
	} else {
		input_booster_init_dvfs(wac_i2c->wacom_booster, INPUT_BOOSTER_ID_WACOM);
	}

#endif

#ifdef WACOM_PDCT_WORK_AROUND
	wac_i2c->irq_pdct = gpio_to_irq(wac_dt_data->gpio_pen_pdct);
	wac_i2c->pen_pdct = PDCT_NOSIGNAL;
#endif
#ifdef WACOM_PEN_DETECT
	wac_i2c->gpio_pen_insert = wac_i2c->wac_dt_data->gpio_pen_insert;
	wac_i2c->irq_pen_insert = gpio_to_irq(wac_i2c->gpio_pen_insert);
#endif
	if (wac_i2c->wac_dt_data->ic_mpu_ver > 0) {
		wac_i2c->ic_mpu_ver = wac_i2c->wac_dt_data->ic_mpu_ver;

		wac_i2c->compulsory_flash_mode(wac_i2c, false);
		wac_i2c->wacom_start(wac_i2c);
		msleep(150);
		wac_i2c->enabled = true;
	} else {
		wac_i2c->compulsory_flash_mode(wac_i2c, true);
		/*Reset */
		wac_i2c->wacom_start(wac_i2c);
		msleep(200);
		wac_i2c->enabled = true;
		wac_i2c->ic_mpu_ver = wacom_check_flash_mode(wac_i2c, BOOT_MPU);
		dev_info(&wac_i2c->client->dev,
			"%s: mpu version: %x\n", __func__, ret);

		if (wac_i2c->ic_mpu_ver == MPU_W9001)
			wac_i2c->client_boot = i2c_new_dummy(client->adapter,
				WACOM_I2C_9001_BOOT);
		else if ((wac_i2c->ic_mpu_ver == MPU_W9007)||(wac_i2c->ic_mpu_ver == MPU_W9012)) {
				ret = wacom_enter_bootloader(wac_i2c);
				if (ret < 0) {
					dev_info(&wac_i2c->client->dev,
						"%s: failed to get BootLoader version, %d\n", __func__, ret);
					goto err_wacom_i2c_bootloader_ver;
			} else {
				dev_info(&wac_i2c->client->dev,
					"%s: BootLoader version: %x\n", __func__, ret);
			}
		}

		wac_i2c->compulsory_flash_mode(wac_i2c, false);
		wac_i2c->reset_platform_hw(wac_i2c);
		wac_i2c->power_enable = true;
	}

	/* Firmware Feature */
	fw_ver = wac_i2c->wacom_i2c_query(wac_i2c);
	pr_err("%s: wacom fw_ver = %d\n", __func__, fw_ver);

	wacom_init_abs_params(wac_i2c);
	input_set_drvdata(input, wac_i2c);

	/*Initializing for semaphor */
	mutex_init(&wac_i2c->lock);
	mutex_init(&wac_i2c->irq_lock);

	INIT_DELAYED_WORK(&wac_i2c->resume_work, wacom_i2c_resume_work);

#ifdef USE_WACOM_BLOCK_KEYEVENT
	INIT_DELAYED_WORK(&wac_i2c->touch_pressed_work, wacom_i2c_touch_pressed_work);
	wac_i2c->key_delay_time = 100;
#endif

#ifdef USE_WACOM_LCD_WORKAROUND
	wac_i2c->wait_done = true;
	wac_i2c->delay_time = 5;
	INIT_DELAYED_WORK(&wac_i2c->read_vsync_work, wacom_i2c_read_vsync_work);

	wac_i2c->boot_done = false;

	INIT_DELAYED_WORK(&wac_i2c->boot_done_work, wacom_i2c_boot_done_work);
#endif
#ifdef WACOM_PEN_DETECT
	INIT_DELAYED_WORK(&wac_i2c->pen_insert_dwork, pen_insert_work);
#endif
	/*Before registering input device, data in each input_dev must be set */
	ret = input_register_device(input);
	if (ret) {
		pr_err("[E-PEN] failed to register input device.\n");
		goto err_input_allocate_device;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	wac_i2c->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	wac_i2c->early_suspend.suspend = wacom_i2c_early_suspend;
	wac_i2c->early_suspend.resume = wacom_i2c_late_resume;
	register_early_suspend(&wac_i2c->early_suspend);
#endif

	wac_i2c->dev = device_create(sec_class, NULL, 0, NULL, "sec_epen");
	if (IS_ERR(wac_i2c->dev)) {
		dev_err(&wac_i2c->client->dev,
				"%s: Failed to create device(wac_i2c->dev)!\n",
				__func__);
		goto err_sysfs_create_group;
	}

	dev_set_drvdata(wac_i2c->dev, wac_i2c);

	ret = wacom_factory_probe(wac_i2c->dev);
	if (ret) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed to create sysfs group\n",
				__func__);
		goto err_sysfs_create_group;
	}

	ret = sysfs_create_link(&wac_i2c->dev->kobj, &wac_i2c->input_dev->dev.kobj, "input");
	if (ret < 0)
		dev_err(&wac_i2c->client->dev, "%s: Failed to create input symbolic link[%d]\n",
				__func__, ret);
	ret = wacom_firmware_update(wac_i2c);
	if (ret) {
		dev_err(&wac_i2c->client->dev,
				"%s: firmware update failed.\n",
				__func__);
		if (fw_ver > 0 && wac_i2c->ic_mpu_ver < 0)
			dev_err(&wac_i2c->client->dev,
					"%s: read query but not enter boot mode[%x,%x]\n",
					__func__, fw_ver, wac_i2c->ic_mpu_ver);
		else
			goto err_fw_update;
	}
	/*Request IRQ */
	if (wac_dt_data->irq_flags) {
		ret =
		    request_threaded_irq(wac_i2c->irq, NULL, wacom_interrupt,
					IRQF_DISABLED | IRQF_TRIGGER_FALLING | IRQF_ONESHOT
					, WACOM_INTERRUPT_NAME, wac_i2c);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
					"%s: failed to request irq(%d) - %d\n",
					__func__, wac_i2c->irq, ret);
			goto err_fw_update;
		}

#if defined(WACOM_PDCT_WORK_AROUND)
		ret = request_threaded_irq(wac_i2c->irq_pdct, NULL,
					wacom_interrupt_pdct,
					IRQF_DISABLED | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					WACOM_PDCT_NAME, wac_i2c);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
					"%s: failed to request irq(%d) - %d\n",
					__func__, wac_i2c->irq_pdct, ret);
			goto err_request_irq_pdct;
		}
#endif

#ifdef WACOM_PEN_DETECT
		ret = request_threaded_irq(
					wac_i2c->irq_pen_insert, NULL,
					wacom_pen_detect,
					IRQF_DISABLED | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					WACOM_PEN_INSERT_NAME, wac_i2c);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
					"%s: failed to request irq(%d) - %d\n",
					__func__, wac_i2c->irq_pen_insert, ret);
			goto err_request_irq_pen_inster;
		}
		enable_irq_wake(wac_i2c->irq_pen_insert);

		/* update the current status */
		schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 2);
#endif

	}

#ifdef USE_WACOM_LCD_WORKAROUND
	schedule_delayed_work(&wac_i2c->boot_done_work,
					msecs_to_jiffies(20 * 1000));
#endif

	if (!gpio_get_value(wac_i2c->wac_dt_data->gpio_int)) {
		ret = wac_i2c->wacom_i2c_recv(wac_i2c, buffer_clear, COM_READ_ALL_DATA_LENGTH, false);
		if (ret < 0)
			dev_err(&wac_i2c->client->dev,
					"%s: failed to read register %d\n",
					__func__, ret);
		else
			dev_err(&wac_i2c->client->dev,
					"%s: clear ic i2c buffer\n", __func__);

	}

	return 0;

err_request_irq_pen_inster:
#ifdef WACOM_PDCT_WORK_AROUND
	free_irq(wac_i2c->irq_pdct, wac_i2c);
err_request_irq_pdct:
#endif
	free_irq(wac_i2c->irq, wac_i2c);
err_fw_update:
	wacom_factory_release(wac_i2c->dev);
err_sysfs_create_group:
	wac_i2c->init_fail = true;
	input_unregister_device(input);
err_input_allocate_device:
	cancel_delayed_work_sync(&wac_i2c->resume_work);
	cancel_delayed_work_sync(&wac_i2c->touch_pressed_work);
#ifdef USE_WACOM_LCD_WORKAROUND
	cancel_delayed_work_sync(&wac_i2c->read_vsync_work);
	cancel_delayed_work_sync(&wac_i2c->boot_done_work);
#endif
	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);

	wac_i2c->wacom_stop(wac_i2c);
	mutex_destroy(&wac_i2c->irq_lock);
	mutex_destroy(&wac_i2c->lock);
err_wacom_i2c_bootloader_ver:
#ifdef WACOM_BOOSTER
	kfree(wac_i2c->wacom_booster);
err_get_wacom_booster:
#endif
	kfree(wac_i2c->wac_query_data);
	wake_lock_destroy(&wac_i2c->wakelock);
	wake_lock_destroy(&wac_i2c->wakelock_pen_insert);
//	input_free_device(input);
err_freemem:
	kfree(wac_i2c);
err_i2c_fail:
	return ret;
}

static const struct i2c_device_id wacom_i2c_id[] = {
	{WACOM_DEVICE_NAME, 0},
	{},
};

#ifdef CONFIG_OF
static struct of_device_id wacom_match_table[] = {
	{ .compatible = "wacom,wacom_i2c-ts",},
	{ },
};
#else
#define wacom_match_table	NULL
#endif
/*Create handler for wacom_i2c_driver*/
static struct i2c_driver wacom_i2c_driver = {
	.driver = {
		   .name = WACOM_DEVICE_NAME,
#ifdef CONFIG_OF
		   .of_match_table = wacom_match_table,
#endif
		   },
	.probe = wacom_i2c_probe,
	.remove = wacom_i2c_remove,
	.id_table = wacom_i2c_id,
};

static int __init wacom_i2c_init(void)
{
	int ret = 0;
	pr_info("%s\n", __func__);

#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		pr_notice("%s : LPM Charging Mode!!\n", __func__);
		return 0;
	}
#endif

	ret = i2c_add_driver(&wacom_i2c_driver);
	if (ret)
		pr_err("%s: fail to i2c_add_driver\n", __func__);

	return ret;
}

static void __exit wacom_i2c_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}

#if defined(CONFIG_SEC_FACTORY)
module_init(wacom_i2c_init);
#else
deferred_initcall(wacom_i2c_init);
#endif
module_exit(wacom_i2c_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Driver for Wacom G5SP Digitizer Controller");

MODULE_LICENSE("GPL");
