/*
 *  wacom_i2c.c - Wacom Digitizer Controller (I2C bus)
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

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sec_sysfs.h>

#include "wacom.h"
#include "wacom_i2c_func.h"

#define WACOM_FW_PATH_SDCARD "/sdcard/Firmware/WACOM/Wacom_firm.bin"

#define GPIO_NAME_LEN 20

static struct wacom_features wacom_feature_EMR = {
	.comstat = COM_QUERY,
	.fw_version = 0x0,
	.update_status = FW_UPDATE_PASS,
};

extern int wacom_i2c_flash(struct wacom_i2c *wac_i2c);

char *gpios_name[] = {
	"irq",
	"pdct",
	"fwe",
	"sense",
	"reset",
};

enum {
	I_IRQ = 0,
	I_PDCT,
	I_FWE,
	I_SENSE,
	I_RESET,
};

static int wacom_power(struct wacom_i2c *wac_i2c, bool on)
{
	struct i2c_client *client = wac_i2c->client;
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;
	static bool wacom_power_enabled = false;
	int ret = 0;
	static struct regulator *avdd = NULL;
	static struct timeval off_time = {0, 0};
	struct timeval cur_time = {0, 0};

	if (!pdata) {
		input_err(true, &client->dev, "%s, pdata is null\n", __func__);
		return -1;
	}

	if (wacom_power_enabled == on) {
		input_info(true, &client->dev, "pwr already %s\n", on ? "enabled" : "disabled");
		return 0;
	}

	if (on) {
		long sec, usec;

		do_gettimeofday(&cur_time);
		sec = cur_time.tv_sec - off_time.tv_sec;
		usec = cur_time.tv_usec - off_time.tv_usec;
		if (!sec) {
			usec = EPEN_OFF_TIME_LIMIT - usec;
			if (usec > 500) {
				usleep_range(usec, usec);
				input_info(true, &client->dev, "%s, pwr on usleep %d\n", __func__, (int)usec);
			}
		}
	}

	if (!avdd) {
		avdd = devm_regulator_get(&client->dev, "avdd");
		if (IS_ERR_OR_NULL(avdd)) {
			input_err(true, &client->dev, "%s reg get err\n", __func__);
			avdd = NULL;
			return -ENODEV;
		}
	}

	if (on) {
		ret = regulator_enable(avdd);
	} else {
		if (regulator_is_enabled(avdd)) {
			regulator_disable(avdd);
			do_gettimeofday(&off_time);
		}
	}

	wacom_power_enabled = on;

	return 0;
}

static int wacom_suspend_hw(struct wacom_i2c *wac_i2c)
{
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;

	if (gpio_is_valid(pdata->gpios[I_RESET]))
		gpio_direction_output(pdata->gpios[I_RESET], 0);

	wacom_power(wac_i2c, 0);

	return 0;
}

static int wacom_resume_hw(struct wacom_i2c *wac_i2c)
{
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;

	if (gpio_is_valid(pdata->gpios[I_RESET]))
		gpio_direction_output(pdata->gpios[I_RESET], 1);

	wacom_power(wac_i2c, 1);

	return 0;
}

static int wacom_reset_hw(struct wacom_i2c *wac_i2c)
{
	wacom_suspend_hw(wac_i2c);
	/* recommended delay in spec */
	msleep(100);
	wacom_resume_hw(wac_i2c);

	return 0;
}

static void wacom_compulsory_flash_mode(struct wacom_i2c *wac_i2c, bool en)
{
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;

	gpio_direction_output(pdata->gpios[I_FWE], en);
}

static int wacom_get_irq_state(struct wacom_i2c *wac_i2c)
{
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;

	int level = gpio_get_value(pdata->gpios[I_IRQ]);

	if (pdata->irq_type & (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW))
		return !level;

	return level;
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

#ifdef WACOM_IRQ_DEBUG
	input_info(true, &wac_i2c->client->dev, "Enable %d, depth %d\n", (int)enable, depth);
#endif
}

static void wacom_irq_state_recovery(struct wacom_i2c *wac_i2c, u8 irq_state)
{
	struct i2c_client *client = wac_i2c->client;
	u8 data[COM_COORD_NUM] = {0,};
	int ret;

	input_info(true, &client->dev, "irq was enabled\n");

	ret = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM, WACOM_I2C_MODE_NORMAL);
	if (ret < 0) {
		input_err(true, &client->dev,
			"%s: failed to receive\n", __func__);
	}
}

#ifdef WACOM_USE_SURVEY_MODE
static void wacom_survey_off(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;

	mutex_lock(&wac_i2c->lock);

	if (wac_i2c->survey_state)
		wacom_i2c_exit_survey_mode(wac_i2c);

	if (device_may_wakeup(&client->dev)) {
		disable_irq_wake(wac_i2c->irq);
		disable_irq_wake(wac_i2c->irq_pdct);
	}

	mutex_unlock(&wac_i2c->lock);

	input_info(true, &client->dev, "%s\n", __func__);
}

static void wacom_survey_on(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	u8 irq_state;

	mutex_lock(&wac_i2c->lock);

	if (device_may_wakeup(&client->dev)) {
		enable_irq_wake(wac_i2c->irq);
		enable_irq_wake(wac_i2c->irq_pdct);
	}

	irq_state = wac_i2c->get_irq_state(wac_i2c);
	if (unlikely(irq_state))
		wacom_irq_state_recovery(wac_i2c, irq_state);

	wacom_i2c_enter_survey_mode(wac_i2c);

	/* release pen, if it is pressed */
	if (wac_i2c->pen_pressed || wac_i2c->side_pressed || wac_i2c->pen_prox)
		forced_release(wac_i2c);

	mutex_unlock(&wac_i2c->lock);

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &client->dev, "%s\n", __func__);
#else
	input_info(true, &client->dev, "%s %d%d\n", __func__, irq_state,
				gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]));
#endif
}
#endif

static void wacom_power_on(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;

	mutex_lock(&wac_i2c->lock);

	if (wac_i2c->power_state == POWER_STATE_ON) {
		input_info(true, &client->dev, "pass pwr on\n");
		goto out_power_on;
	}

	if (wac_i2c->battery_saving_mode && wac_i2c->pen_insert)
		goto out_power_on;

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "wake_lock active. pass pwr on\n");
		goto out_power_on;
	}

	/* power on */
	wacom_resume_hw(wac_i2c);
	wac_i2c->power_state = POWER_STATE_ON;

	/* compensation to protect from flash mode  */
	wac_i2c->compulsory_flash_mode(wac_i2c, false);

	cancel_delayed_work_sync(&wac_i2c->resume_work);
	schedule_delayed_work(&wac_i2c->resume_work, msecs_to_jiffies(EPEN_RESUME_DELAY));

	input_info(true, &client->dev, "%s\n", __func__);
 out_power_on:
	mutex_unlock(&wac_i2c->lock);
}

static void wacom_power_off(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;

	mutex_lock(&wac_i2c->lock);

	if (wac_i2c->power_state == POWER_STATE_OFF) {
		input_info(true, &client->dev, "pass pwr off\n");
		goto out_power_off;
	}

	wacom_enable_irq(wac_i2c, false);

	/* release pen, if it is pressed */
	if (wac_i2c->pen_pressed || wac_i2c->side_pressed
		|| wac_i2c->pen_prox)
		forced_release(wac_i2c);

	cancel_delayed_work_sync(&wac_i2c->resume_work);

#ifdef LCD_FREQ_SYNC
	cancel_work_sync(&wac_i2c->lcd_freq_work);
	cancel_delayed_work_sync(&wac_i2c->lcd_freq_done_work);
	wac_i2c->lcd_freq_wait = false;
#endif
#ifdef WACOM_USE_SOFTKEY_BLOCK
	cancel_delayed_work_sync(&wac_i2c->softkey_block_work);
	wac_i2c->block_softkey = false;
#endif
	cancel_work_sync(&wac_i2c->reset_work);
	wac_i2c->reset_on_going = false;

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "wake_lock active. pass pwr off\n");
		goto out_power_off;
	}

	/* power off */
	wac_i2c->power_state = POWER_STATE_OFF;
	wacom_suspend_hw(wac_i2c);

	/* compensation to protect from flash mode  */
	wac_i2c->compulsory_flash_mode(wac_i2c, false);

	input_info(true, &client->dev, "%s\n", __func__);
 out_power_off:
	mutex_unlock(&wac_i2c->lock);
}

static irqreturn_t wacom_interrupt(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
#ifdef WACOM_USE_SURVEY_MODE
	int ret;

	/* in LPM, waiting blsp block resume */
	if (wac_i2c->power_state == POWER_STATE_LPM_SUSPEND) {
		wake_lock_timeout(&wac_i2c->wakelock,
					msecs_to_jiffies(3 * MSEC_PER_SEC));
		ret = wait_for_completion_interruptible_timeout(
					&wac_i2c->resume_done,
					msecs_to_jiffies(3 * MSEC_PER_SEC));
		if (ret < 0) {
			input_err(true, &wac_i2c->client->dev,
				"%s: LPM: pm resume is not handled\n",
				__func__);
				return IRQ_HANDLED;
		} else if (!ret) {
			wac_i2c->timeout = true;
			input_err(true, &wac_i2c->client->dev,
				"%s: LPM: -ERESTARTSYS if interrupted, %d\n",
				__func__, ret);
				return IRQ_HANDLED;
		}
	}

	/* wacom is occurred interrupt by specific action in survey mode. this is dependent on F/W */
	if (wac_i2c->survey_state) {
		ret = wacom_get_aop_data(wac_i2c);
		if (ret < 0)
			return IRQ_HANDLED;

		input_info(true, &wac_i2c->client->dev,
				"%s: run AOP interrupt handler\n", __func__);

		input_report_key(wac_i2c->input_dev, KEY_WAKEUP_UNLOCK, 1);
		input_sync(wac_i2c->input_dev);

		input_report_key(wac_i2c->input_dev, KEY_WAKEUP_UNLOCK, 0);
		input_sync(wac_i2c->input_dev);

		wac_i2c->send_done = true;
		wac_i2c->survey_pos.id = 1;

		return IRQ_HANDLED;
	}
#endif

	wacom_i2c_coord(wac_i2c);

	return IRQ_HANDLED;
}

#if defined(WACOM_PDCT_WORK_AROUND)
static irqreturn_t wacom_interrupt_pdct(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	struct i2c_client *client = wac_i2c->client;

	wac_i2c->pen_pdct = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);

	if (!wac_i2c->query_status) {
		input_err(true, &client->dev,
					"failed to query to IC. pdct(%d)\n",
					wac_i2c->pen_pdct);

			return IRQ_HANDLED;
	}

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &client->dev, "pdct %d(%d)\n",
		wac_i2c->pen_pdct, wac_i2c->pen_prox);
#else
	input_info(true, &client->dev, "pdct %d(%d) %d\n",
		wac_i2c->pen_pdct, wac_i2c->pen_prox, wac_i2c->get_irq_state(wac_i2c));
#endif
	return IRQ_HANDLED;
}
#endif

static void pen_insert_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, pen_insert_dwork.work);

	wac_i2c->pen_insert = !gpio_get_value(wac_i2c->pdata->gpios[I_SENSE]);

	input_info(true, &wac_i2c->client->dev, "%s : %d\n",
		__func__, wac_i2c->pen_insert);

	input_report_switch(wac_i2c->input_dev,
		SW_PEN_INSERT, !wac_i2c->pen_insert);
	input_sync(wac_i2c->input_dev);

	/* batter saving mode */
	if (wac_i2c->pen_insert) {
		if (wac_i2c->battery_saving_mode)
			wacom_power_off(wac_i2c);
	} else
		wacom_power_on(wac_i2c);
}
static irqreturn_t wacom_pen_detect(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;

	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);
	wake_lock_timeout(&wac_i2c->det_wakelock, HZ / 10);
	schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 20);
	return IRQ_HANDLED;
}

static int init_pen_insert(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	int irq = gpio_to_irq(wac_i2c->pdata->gpios[I_SENSE]);
	struct i2c_client *client = wac_i2c->client;

	INIT_DELAYED_WORK(&wac_i2c->pen_insert_dwork, pen_insert_work);

	ret =
		devm_request_threaded_irq(
			&client->dev, irq, NULL,
			wacom_pen_detect,
			IRQF_DISABLED | IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"sec_epen_insert", wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev,
				"failed to request irq(%d) - %d\n", irq, ret);
		return -1;
	}
	input_info(true, &client->dev, "init sense irq %d\n", irq);

	enable_irq_wake(irq);

	/* update the current status */
	schedule_delayed_work(&wac_i2c->pen_insert_dwork, HZ / 2);

	return 0;
}

static int wacom_i2c_input_open(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);
	int ret = 0;

#ifdef WACOM_USE_SURVEY_MODE
	if (wac_i2c->aop_mode)
		wacom_survey_off(wac_i2c);
	else
#endif
		wacom_power_on(wac_i2c);

	return ret;
}

static void wacom_i2c_input_close(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);

#ifdef WACOM_USE_SURVEY_MODE
	if (wac_i2c->aop_mode)
		wacom_survey_on(wac_i2c);
	else
#endif
		wacom_power_off(wac_i2c);
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

	input_dev->open = wacom_i2c_input_open;
	input_dev->close = wacom_i2c_input_close;

	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_PEN, input_dev->keybit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
	__set_bit(BTN_STYLUS, input_dev->keybit);
	__set_bit(KEY_UNKNOWN, input_dev->keybit);
	/* __set_bit(KEY_PEN_PDCT, input_dev->keybit); */
	__set_bit(ABS_DISTANCE, input_dev->absbit);
	__set_bit(ABS_TILT_X, input_dev->absbit);
	__set_bit(ABS_TILT_Y, input_dev->absbit);

	/* __set_bit(BTN_STYLUS2, input_dev->keybit); */
	/* __set_bit(ABS_MISC, input_dev->absbit); */

	/* softkey */
	__set_bit(KEY_RECENT, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);

#ifdef WACOM_USE_SURVEY_MODE
	/* AOP */
	__set_bit(KEY_WAKEUP_UNLOCK, input_dev->keybit);
#endif
}

static void wacom_i2c_resume_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
			container_of(work, struct wacom_i2c, resume_work.work);
	struct i2c_client *client = wac_i2c->client;
	u8 irq_state = 0;

	if (wac_i2c->wcharging_mode)
		wacom_i2c_set_sense_mode(wac_i2c);

	irq_state = wac_i2c->get_irq_state(wac_i2c);
	if (unlikely(irq_state))
		wacom_irq_state_recovery(wac_i2c, irq_state);

	wacom_enable_irq(wac_i2c, true);

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &client->dev, "%s\n", __func__);
#else
	input_info(true, &client->dev, "%s %d%d\n", __func__, irq_state,
				gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]));
#endif
}

static void wacom_i2c_reset_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
			container_of(work, struct wacom_i2c, reset_work);
	struct i2c_client *client = wac_i2c->client;

	input_info(true, &client->dev, "%s\n", __func__);

	if (wac_i2c->power_state == POWER_STATE_OFF) {
		input_err(true, &client->dev,
			"%s: power is off state.\n", __func__);
		return;
	}

	wacom_enable_irq(wac_i2c, false);

	wacom_reset_hw(wac_i2c);
	msleep(EPEN_RESUME_DELAY);

	wacom_i2c_query(wac_i2c);

	wac_i2c->reset_on_going = false;

	wacom_enable_irq(wac_i2c, true);
}

#ifdef LCD_FREQ_SYNC
#define SYSFS_WRITE_LCD	"/sys/class/lcd/panel/ldi_fps"
static void wacom_i2c_sync_lcd_freq(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	mm_segment_t old_fs;
	struct file *write_node;
	char freq[12] = {0, };
	int lcd_freq = wac_i2c->lcd_freq;
	struct i2c_client *client = wac_i2c->client;

	mutex_lock(&wac_i2c->freq_write_lock);

	sprintf(freq, "%d", lcd_freq);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	write_node = filp_open(SYSFS_WRITE_LCD, /*O_RDONLY*/O_RDWR | O_SYNC, 0664);
	if (IS_ERR(write_node)) {
		ret = PTR_ERR(write_node);
		input_err(&client->dev,
			"%s: node file open fail, %d\n", __func__, ret);
		goto err_open_node;
	}

	ret = write_node->f_op->write(write_node, (char __user *)freq, strlen(freq), &write_node->f_pos);
	if (ret != strlen(freq)) {
		input_err(&client->dev,
			"%s: Can't write node data\n", __func__);
	}
	input_info(true, &client->dev, "%s write freq %s\n", __func__, freq);

	filp_close(write_node, current->files);

err_open_node:
	set_fs(old_fs);
//err_read_framerate:
	mutex_unlock(&wac_i2c->freq_write_lock);

	schedule_delayed_work(&wac_i2c->lcd_freq_done_work, HZ * 5);
}

static void wacom_i2c_finish_lcd_freq_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, lcd_freq_done_work.work);

	wac_i2c->lcd_freq_wait = false;

	input_info(true, &wac_i2c->client->dev, "%s\n", __func__);
}

static void wacom_i2c_lcd_freq_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, lcd_freq_work);

	wacom_i2c_sync_lcd_freq(wac_i2c);
}
#endif

#ifdef WACOM_USE_SOFTKEY_BLOCK
static void wacom_i2c_block_softkey_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, softkey_block_work.work);

	wac_i2c->block_softkey = false;
}
#endif

#ifdef WACOM_USE_SURVEY_MODE
static void wacom_i2c_ignore_garbage_irq_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, irq_ignore_work.work);

	wac_i2c->garbage_irq = false;

	input_info(true, &wac_i2c->client->dev, "%s\n", __func__);
}
#endif

static int load_fw_built_in(struct wacom_i2c *wac_i2c)
{
	int retry = 3;
	int ret;

	while (retry--) {
		ret =
			request_firmware(&wac_i2c->firm_data, wac_i2c->pdata->fw_path,
			&wac_i2c->client->dev);
		if (ret < 0) {
			input_err(true, &wac_i2c->client->dev,
				"Unable to open firmware. ret %d retry %d\n",
				ret, retry);
			continue;
		}
		break;
	}

	if (ret < 0)
		return ret;

	wac_i2c->fw_img = (struct fw_image *)wac_i2c->firm_data->data;

	return ret;
}

static int load_fw_sdcard(struct wacom_i2c *wac_i2c)
{
	struct file *fp;
	struct i2c_client *client = wac_i2c->client;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
	unsigned int nSize;
	unsigned long nSize2;
	u8 *ums_data;

	nSize = WACOM_FW_SIZE;
	nSize2 = nSize + sizeof(struct fw_image);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(WACOM_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		input_err(true, &client->dev, "failed to open %s.\n", WACOM_FW_PATH_SDCARD);
		ret = -ENOENT;
		set_fs(old_fs);
		return ret;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	input_info(true, &client->dev,
		"start, file path %s, size %ld Bytes\n",
		WACOM_FW_PATH_SDCARD, fsize);

	if ((fsize != nSize) && (fsize != nSize2)) {
		input_err(true, &client->dev, "UMS firmware size is different\n");
		ret = -EFBIG;
		goto size_error;
	}

	ums_data = kmalloc(fsize, GFP_KERNEL);
	if (IS_ERR(ums_data)) {
		input_err(true, &client->dev,
			"%s, kmalloc failed\n", __func__);
		ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)ums_data,
		fsize, &fp->f_pos);
	input_info(true, &client->dev, "nread %ld Bytes\n", nread);
	if (nread != fsize) {
		input_err(true, &client->dev,
			"failed to read firmware file, nread %ld Bytes\n",
			nread);
		ret = -EIO;
		kfree(ums_data);
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);

	wac_i2c->fw_img = (struct fw_image *)ums_data;

	return 0;

read_err:
malloc_error:
size_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	return ret;
}

static int wacom_i2c_load_fw(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	struct fw_image *fw_img;
	struct i2c_client *client = wac_i2c->client;
	u8 fw_update_way = wac_i2c->fw_update_way;

	switch (fw_update_way) {
	case FW_BUILT_IN:
		ret = load_fw_built_in(wac_i2c);
		break;
	case FW_IN_SDCARD:
		ret = load_fw_sdcard(wac_i2c);
		break;
	default:
		input_err(true, &client->dev, "unknown path(%d)\n", fw_update_way);
		goto err_load_fw;
	}

	if (ret < 0)
		goto err_load_fw;

	fw_img = wac_i2c->fw_img;

	/* header check */
	if (fw_img->hdr_ver == 1 && fw_img->hdr_len == sizeof(struct fw_image)) {
		wac_i2c->fw_data = (u8 *)fw_img->data;
		if (fw_update_way == FW_BUILT_IN) {
			wac_i2c->fw_ver_file = fw_img->fw_ver1;
			memcpy(wac_i2c->fw_chksum, fw_img->checksum, 5);
		}
	} else {
		input_err(true, &client->dev, "no hdr\n");
		wac_i2c->fw_data = (u8 *)fw_img;
	}

	return ret;

err_load_fw:
	wac_i2c->fw_data = NULL;
	return ret;
}

static void wacom_i2c_unload_fw(struct wacom_i2c *wac_i2c)
{
	switch (wac_i2c->fw_update_way) {
	case FW_BUILT_IN:
		release_firmware(wac_i2c->firm_data);
		wac_i2c->firm_data = NULL;
		break;
	case FW_IN_SDCARD:
		kfree(wac_i2c->fw_img);
		wac_i2c->fw_img = NULL;
		break;
	default:
		break;
	}

	wac_i2c->fw_update_way = FW_NONE;
	wac_i2c->fw_data = NULL;
}

static int wacom_fw_update(struct wacom_i2c *wac_i2c, u8 fw_update_way, bool bforced)
{
	struct i2c_client *client = wac_i2c->client;
	u32 fw_ver_ic = wac_i2c->wac_feature->fw_version;
	int ret;

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "update is already running. pass\n");
		return 0;
	}

	wacom_enable_irq(wac_i2c, false);

	forced_release(wac_i2c);

	input_info(true, &client->dev, "%s\n", __func__);

	wac_i2c->fw_update_way = fw_update_way;

	ret = wacom_i2c_load_fw(wac_i2c);
	if (ret < 0) {
		input_info(true, &client->dev, "failed to load fw data\n");
		wac_i2c->wac_feature->update_status = FW_UPDATE_FAIL;
		goto err_update_load_fw;
	}

	/* firmware info */
	input_info(true, &client->dev, "wacom fw ver : 0x%x, new fw ver : 0x%x\n",
		wac_i2c->wac_feature->fw_version, wac_i2c->fw_ver_file);

	if (!bforced) {
		if (fw_ver_ic == wac_i2c->fw_ver_file) {
			input_info(true, &client->dev, "pass fw update\n");
			wac_i2c->do_crc_check = true;
			/* need to check crc */
		} else if (fw_ver_ic > wac_i2c->fw_ver_file){
			input_info(true, &client->dev, "dont need to update fw\n");
			goto out_update_fw;
		}

		/* ic < file then update */
	}

	queue_work(wac_i2c->fw_wq, &wac_i2c->update_work);

	return 0;

out_update_fw:
	wacom_i2c_unload_fw(wac_i2c);
err_update_load_fw:
	wacom_enable_irq(wac_i2c, true);

	return 0;
}

static void wacom_i2c_update_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
		container_of(work, struct wacom_i2c, update_work);
	struct i2c_client *client = wac_i2c->client;
	struct wacom_features *feature = wac_i2c->wac_feature;
	int ret = 0;
	int retry = 3;

	if (wac_i2c->fw_update_way == FW_NONE)
		goto end_fw_update;

	wake_lock(&wac_i2c->fw_wakelock);

	/* CRC Check */
	if (wac_i2c->do_crc_check) {
		wac_i2c->do_crc_check = false;
		ret = wacom_checksum(wac_i2c);
		if (ret)
			goto err_update_fw;
		input_info(true, &client->dev, "crc err, do update\n");
	}

	feature->update_status = FW_UPDATE_RUNNING;

	while (retry--) {
		ret = wacom_i2c_flash(wac_i2c);
		if (ret) {
			input_err(true, &client->dev, "failed to flash fw(%d)\n", ret);
			continue;
		}
		break;
	}
	if (ret) {
		feature->update_status = FW_UPDATE_FAIL;
		feature->fw_version = 0;
		goto err_update_fw;
	}

	ret = wacom_i2c_query(wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev, "failed to query to IC(%d)\n", ret);
		feature->update_status = FW_UPDATE_FAIL;
		goto err_update_fw;
	}

	feature->update_status = FW_UPDATE_PASS;

 err_update_fw:
	wake_unlock(&wac_i2c->fw_wakelock);
 end_fw_update:
	wacom_i2c_unload_fw(wac_i2c);
	wacom_enable_irq(wac_i2c, true);

	return ;
}

static ssize_t epen_firm_update_status_show(struct device *dev,
struct device_attribute *attr,
	char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int status = wac_i2c->wac_feature->update_status;

	input_info(true, &wac_i2c->client->dev, "%s:(%d)\n", __func__, status);

	if (status == FW_UPDATE_PASS)
		return sprintf(buf, "PASS\n");
	else if (status == FW_UPDATE_RUNNING)
		return sprintf(buf, "DOWNLOADING\n");
	else if (status == FW_UPDATE_FAIL)
		return sprintf(buf, "FAIL\n");
	else
		return 0;
}

static ssize_t epen_firm_version_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	input_info(true, &wac_i2c->client->dev, "%s: 0x%x|0x%X\n", __func__,
		wac_i2c->wac_feature->fw_version, wac_i2c->fw_ver_file);

	return sprintf(buf, "%04X\t%04X\n",
		wac_i2c->wac_feature->fw_version,
		wac_i2c->fw_ver_file);
}

static ssize_t epen_firmware_update_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	u8 fw_update_way = FW_NONE;

	input_info(true, &wac_i2c->client->dev, "%s\n", __func__);

	switch (*buf) {
	case 'i':
	case 'I':
		fw_update_way = FW_IN_SDCARD;
		break;
	case 'k':
	case 'K':
		fw_update_way = FW_BUILT_IN;
		break;
	default:
		input_err(true, &wac_i2c->client->dev, "wrong parameter\n");
		return count;
	}

	wacom_fw_update(wac_i2c, fw_update_way, true);

	return count;
}

static ssize_t epen_reset_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	sscanf(buf, "%d", &val);

	if (val == 1) {
		wacom_enable_irq(wac_i2c, false);

		/* Reset IC */
		wac_i2c->reset_platform_hw(wac_i2c);

		/* I2C Test */
		wacom_i2c_query(wac_i2c);

		wacom_enable_irq(wac_i2c, true);

		input_info(true, &wac_i2c->client->dev, "%s, result %d\n", __func__,
		       wac_i2c->query_status);
	}

	return count;
}

static ssize_t epen_reset_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->query_status) {
		input_info(true, &wac_i2c->client->dev, "%s, PASS\n", __func__);
		return sprintf(buf, "PASS\n");
	} else {
		input_err(true, &wac_i2c->client->dev, "%s, FAIL\n", __func__);
		return sprintf(buf, "FAIL\n");
	}
}

static ssize_t epen_checksum_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	sscanf(buf, "%d", &val);

	if (val != 1) {
		input_err(true, &wac_i2c->client->dev, "wrong cmd %d\n", val);
		return count;
	}

	wacom_enable_irq(wac_i2c, false);
	wacom_checksum(wac_i2c);
	wacom_enable_irq(wac_i2c, true);

	input_err(true, &wac_i2c->client->dev, "%s, result %d\n",
		__func__, wac_i2c->checksum_result);

	return count;
}

static ssize_t epen_checksum_result_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->checksum_result) {
		input_info(true, &wac_i2c->client->dev, "checksum, PASS\n");
		return sprintf(buf, "PASS\n");
	} else {
		input_err(true, &wac_i2c->client->dev, "checksum, FAIL\n");
		return sprintf(buf, "FAIL\n");
	}
}

static int wacom_open_test(struct wacom_i2c *wac_i2c)
{
	struct i2c_client *client = wac_i2c->client;
	u8 cmd = 0;
	u8 buf[4] = {0,};
	int ret = 0, retry = 10;

	wac_i2c->connection_check = false;
	wac_i2c->fail_channel = 0;
	wac_i2c->min_adc_val = 0;

	cmd = WACOM_I2C_STOP;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, WACOM_I2C_MODE_NORMAL);
	if (ret <= 0) {
		input_err(true, &client->dev, "failed to send stop command\n");
		return -1;
	}

	usleep_range(500, 500);

	cmd = WACOM_I2C_GRID_CHECK;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, WACOM_I2C_MODE_NORMAL);
	if (ret <= 0) {
		input_err(true, &client->dev, "failed to send stop command\n");
		return -1;
	}

	msleep(150);

	cmd = WACOM_STATUS;
	do {
		input_info(true, &client->dev, "read status, retry %d\n", retry);
		ret = wacom_i2c_send(wac_i2c, &cmd, 1, WACOM_I2C_MODE_NORMAL);
		if (ret != 1) {
			input_err(true, &client->dev, "failed to send cmd(ret:%d)\n", ret);
			continue;
		}
		usleep_range(500, 500);
		ret = wacom_i2c_recv(wac_i2c, buf, 4, WACOM_I2C_MODE_NORMAL);
		if (ret != 4) {
			input_err(true, &client->dev, "failed to recv data(ret:%d)\n", ret);
			continue;
		}

		/*
		*	status value
		*	0 : data is not ready
		*	1 : PASS
		*	2 : Fail (coil function error)
		*	3 : Fail (All coil function error)
		*/
		if (buf[0] == 1) {
			input_info(true, &wac_i2c->client->dev, "Pass\n");
			break;
		}

		msleep(50);
	} while (retry--);

	if (ret > 0)
		wac_i2c->connection_check = (1 == buf[0]);
	else {
		wac_i2c->connection_check = false;
		return -1;
	}

	wac_i2c->fail_channel = buf[1];
	wac_i2c->min_adc_val = buf[2]<<8 | buf[3];

	input_info(true, &client->dev,
	       "epen_connection : %s buf[0]:%d, buf[1]:%d, buf[2]:%d, buf[3]:%d\n",
	       wac_i2c->connection_check ? "Pass" : "Fail", buf[0], buf[1], buf[2], buf[3]);

	cmd = WACOM_I2C_STOP;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, WACOM_I2C_MODE_NORMAL);
	if (ret <= 0) {
		input_err(true, &client->dev, "failed to send stop cmd for end\n");
		return -2;
	}

	cmd = WACOM_I2C_START;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, WACOM_I2C_MODE_NORMAL);
	if (ret <= 0) {
		input_err(true, &client->dev, "failed to send start cmd for end\n");
		return -2;
	}

	return 0;
}

static ssize_t epen_connection_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;
	struct wacom_features *feature = wac_i2c->wac_feature;
	int retry = 3;
	int ret;

	if(feature->update_status == FW_UPDATE_RUNNING) {
		input_info(true, &client->dev, "Running fw update. Skip connection check\n");
		wac_i2c->connection_check = false;
		goto error;
	}

	mutex_lock(&wac_i2c->lock);

	input_info(true, &client->dev, "%s\n", __func__);
	wacom_enable_irq(wac_i2c, false);

	if (wac_i2c->power_state == POWER_STATE_OFF) {
		wacom_resume_hw(wac_i2c);
		input_info(true, &client->dev, "pwr on\n");
		msleep(200);
	}

	while (retry--) {
		ret = wacom_open_test(wac_i2c);
		if (!ret)
			break;

		input_err(true, &client->dev, "failed. retry %d\n", retry);
		wac_i2c->reset_platform_hw(wac_i2c);
		if (ret == -1) {
			msleep(200);
			continue;
		} else if (ret == -2) {
			break;
		}
	}
	wacom_enable_irq(wac_i2c, true);

	mutex_unlock(&wac_i2c->lock);

 error:

	input_info(true, &client->dev,
		"connection_check : %d\n",
		wac_i2c->connection_check);

	return sprintf(buf, "%s 1 %d %d\n",
		wac_i2c->connection_check ?
		"OK" : "NG", wac_i2c->fail_channel, wac_i2c->min_adc_val);
}

#ifdef WACOM_USE_SURVEY_MODE
static ssize_t epen_aop_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	if (wac_i2c->power_state == POWER_STATE_OFF) {
		input_err(true, &wac_i2c->client->dev,
				"%s, pwr is disabled\n", __func__);
		return count;
	}

	if (sscanf(buf, "%u", &val) == 1)
		wac_i2c->aop_mode = !!val;

	input_info(true, &wac_i2c->client->dev, "%s, val %d\n",
		__func__, wac_i2c->aop_mode);

	return count;
}

static ssize_t get_epen_pos_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;
	int id, x, y, max_x, max_y;

	id = wac_i2c->survey_pos.id;
	x = wac_i2c->survey_pos.x;
	y = wac_i2c->survey_pos.y;
	max_x = wac_i2c->pdata->max_x;
	max_y = wac_i2c->pdata->max_y;

	if (wac_i2c->pdata->xy_switch) {
		int tmp = max_x;
		max_x = max_y;
		max_y = tmp;
	}

	input_info(true, &client->dev,
			"%s: id(%d), x(%d), y(%d), max_x(%d), max_y(%d)\n",
			__func__, id, x, y, max_x, max_y);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d\n",
				id, x, y, max_x, max_y);
}
#endif

static ssize_t epen_saving_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	if (sscanf(buf, "%u", &val) == 1)
		wac_i2c->battery_saving_mode = !!val;

	input_info(true, &wac_i2c->client->dev, "%s, val %d\n",
		__func__, wac_i2c->battery_saving_mode);

	if (wac_i2c->power_state == POWER_STATE_OFF) {
		input_info(true, &wac_i2c->client->dev, "pass pwr control\n");
		return count;
	}

	if (wac_i2c->battery_saving_mode) {
		if (wac_i2c->pen_insert)
			wacom_power_off(wac_i2c);
	} else
		wacom_power_on(wac_i2c);
	return count;
}

static ssize_t epen_wcharging_mode_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	input_info(true, &wac_i2c->client->dev, "%s: %s\n",
			__func__, !wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");

	return sprintf(buf, "%s\n", !wac_i2c->wcharging_mode ? "NORMAL" : "LOWSENSE");
}

static ssize_t epen_wcharging_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	struct i2c_client *client = wac_i2c->client;
	int retval = 0;

	if (sscanf(buf, "%u", &retval) == 1)
		wac_i2c->wcharging_mode = retval;

	input_info(true, &client->dev, "%s, val %d\n",
		__func__, wac_i2c->wcharging_mode);


	if (wac_i2c->power_state == POWER_STATE_OFF) {
		input_err(true, &client->dev, "%s: power off, save & return\n", __func__);
		return count;
	}

	retval = wacom_i2c_set_sense_mode(wac_i2c);
	if (retval < 0)
		input_err(true, &client->dev, "%s: do not set sensitivity mode, %d\n",
				__func__, retval);

	return count;

}

/* firmware update */
static DEVICE_ATTR(epen_firm_update,
			S_IWUSR | S_IWGRP, NULL, epen_firmware_update_store);
/* return firmware update status */
static DEVICE_ATTR(epen_firm_update_status,
			S_IRUGO, epen_firm_update_status_show, NULL);
/* return firmware version */
static DEVICE_ATTR(epen_firm_version, S_IRUGO, epen_firm_version_show, NULL);
/* For SMD Test */
static DEVICE_ATTR(epen_reset, S_IWUSR | S_IWGRP, NULL, epen_reset_store);
static DEVICE_ATTR(epen_reset_result,
			S_IRUSR | S_IRGRP, epen_reset_result_show, NULL);

/* For SMD Test. Check checksum */
static DEVICE_ATTR(epen_checksum, S_IWUSR | S_IWGRP, NULL, epen_checksum_store);
static DEVICE_ATTR(epen_checksum_result, S_IRUSR | S_IRGRP,
			epen_checksum_result_show, NULL);

static DEVICE_ATTR(epen_connection,
			S_IRUGO, epen_connection_show, NULL);

static DEVICE_ATTR(epen_saving_mode,
			S_IWUSR | S_IWGRP, NULL, epen_saving_mode_store);

#ifdef WACOM_USE_SURVEY_MODE
static DEVICE_ATTR(epen_aop_mode,
			S_IWUSR | S_IWGRP, NULL, epen_aop_mode_store);
static DEVICE_ATTR(screen_off_memo_enable,
			S_IWUSR | S_IWGRP, NULL, epen_aop_mode_store);

static DEVICE_ATTR(get_epen_pos,
			S_IRUGO, get_epen_pos_show, NULL);
#endif

static DEVICE_ATTR(epen_wcharging_mode,
			S_IRUGO | S_IWUSR | S_IWGRP, epen_wcharging_mode_show, epen_wcharging_mode_store);

static struct attribute *epen_attributes[] = {
	&dev_attr_epen_firm_update.attr,
	&dev_attr_epen_firm_update_status.attr,
	&dev_attr_epen_firm_version.attr,
	&dev_attr_epen_reset.attr,
	&dev_attr_epen_reset_result.attr,
	&dev_attr_epen_checksum.attr,
	&dev_attr_epen_checksum_result.attr,
	&dev_attr_epen_connection.attr,
	&dev_attr_epen_saving_mode.attr,
#ifdef WACOM_USE_SURVEY_MODE
	&dev_attr_epen_aop_mode.attr,
	&dev_attr_screen_off_memo_enable.attr,
	&dev_attr_get_epen_pos.attr,
#endif
	&dev_attr_epen_wcharging_mode.attr,
	NULL,
};

static struct attribute_group epen_attr_group = {
	.attrs = epen_attributes,
};

static void wacom_init_abs_params(struct wacom_i2c *wac_i2c)
{
	int max_x, max_y;
	int pressure;

	max_x = wac_i2c->pdata->max_x;
	max_y = wac_i2c->pdata->max_y;
	pressure = wac_i2c->pdata->max_pressure;

	if (wac_i2c->pdata->xy_switch) {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, 0,
			max_y, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, 0,
			max_x, 4, 0);
	} else {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, 0,
			max_x, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, 0,
			max_y, 4, 0);
	}
	input_set_abs_params(wac_i2c->input_dev, ABS_PRESSURE, 0,
		pressure, 0, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_DISTANCE, 0,
		1024, 0, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_TILT_X, -63,
		63, 0, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_TILT_Y, -63,
		63, 0, 0);
}

static int wacom_request_gpio(struct i2c_client *client,
				struct wacom_g5_platform_data *pdata)
{
	int ret;
	int i;
	char name[GPIO_NAME_LEN] = {0, };

	for (i = 0 ; i < WACOM_GPIO_CNT; ++i) {
		if (!gpio_is_valid(pdata->gpios[i]))
			continue;

		snprintf(name, GPIO_NAME_LEN, "wacom_%s", gpios_name[i]);

		ret = devm_gpio_request(&client->dev, pdata->gpios[i], name);
		if (ret) {
			input_err(true, &client->dev,
					"unable to request %s [%d]\n",
					name, pdata->gpios[i]);
			return ret;
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static struct wacom_g5_platform_data *wacom_parse_dt(struct i2c_client *client)
{
	struct wacom_g5_platform_data *pdata;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	struct property *prop;
	char name[GPIO_NAME_LEN] = {0, };
	u32 tmp[5] = {0, };
	int ret;
	int i;

	if (!np)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (ARRAY_SIZE(gpios_name) != WACOM_GPIO_CNT)
		input_err(true, &client->dev, "array size error, gpios_name\n");

	for (i = 0 ; i < WACOM_GPIO_CNT; ++i) {
		snprintf(name, GPIO_NAME_LEN, "wacom,%s-gpio", gpios_name[i]);

		if (of_find_property(np, name, NULL)) {
			pdata->gpios[i] = of_get_named_gpio_flags(np, name,
				0, &pdata->flag_gpio[i]);

			if (!gpio_is_valid(pdata->gpios[i]))
				input_err(true, &client->dev, "failed to get gpio %s, %d\n", name, pdata->gpios[i]);
		} else {
			input_info(true, &client->dev, "theres no prop %s\n", name);
			pdata->gpios[i] = -EINVAL;
		}
	}

	/* get features */
	ret = of_property_read_u32(np, "wacom,irq_type", tmp);
	if (ret) {
		input_err(true, &client->dev, "failed to read trigger type %d\n", ret);
		return ERR_PTR(-EINVAL);
	}
	pdata->irq_type = tmp[0];

	ret = of_property_read_u32(np, "wacom,boot_addr", tmp);
	if (ret) {
		input_err(true, &client->dev, "failed to read boot address %d\n", ret);
		return ERR_PTR(-EINVAL);
	}
	pdata->boot_addr = tmp[0];

	ret = of_property_read_u32_array(np, "wacom,origin", pdata->origin, 2);
	if (ret) {
		input_err(true, &client->dev, "failed to read origin %d\n", ret);
		return ERR_PTR(-EINVAL);
	}

	/* coords and pressure reinitialize in wacom_i2c_query */
	prop = of_find_property(np, "wacom,max_coords", NULL);
	if (prop && prop->length) {
		ret = of_property_read_u32_array(np,
					"wacom,max_coords", tmp, 2);
		if (ret)
			input_err(true, &client->dev,
					"failed to read coords %d\n", ret);
		else {
			pdata->max_x = tmp[0];
			pdata->max_y = tmp[1];
		}
	}

	prop = of_find_property(np, "wacom,max_pressure", NULL);
	if (prop && prop->length) {
		ret = of_property_read_u32_array(np,
					"wacom,max_pressure", tmp, 1);
		if (ret) {
			input_err(true, &client->dev,
					"failed to read pressure %d\n", ret);
		} else {
			pdata->max_pressure = tmp[0];
		}
	}

	ret = of_property_read_u32_array(np, "wacom,invert", tmp, 3);
	if (ret) {
		input_err(true, &client->dev, "failed to read inverts %d\n", ret);
		return ERR_PTR(-EINVAL);
	}
	pdata->x_invert = tmp[0];
	pdata->y_invert = tmp[1];
	pdata->xy_switch = tmp[2];

	ret = of_property_read_u32(np, "wacom,ic_type", &pdata->ic_type);
	if (ret) {
		input_err(true, &client->dev, "failed to read ic_type %d\n", ret);
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_string(np, "wacom,fw_path", (const char **)&pdata->fw_path);
	if (ret) {
		input_err(true, &client->dev, "failed to read fw_path %d\n", ret);
		pdata->fw_path = WACOM_FW_PATH;
	}

	ret = of_property_read_string_index(np, "wacom,project_name", 0, (const char **)&pdata->project_name);
	if (ret)
		input_err(true, &client->dev, "failed to read project name %d\n", ret);

	ret = of_property_read_string_index(np, "wacom,project_name", 1, (const char **)&pdata->model_name);
	if (ret)
		input_err(true, &client->dev, "failed to read model name %d\n", ret);

	pdata->boot_on_ldo = of_property_read_bool(np, "wacom,boot_on_ldo");

	input_info(true, &client->dev, "boot_addr: 0x%X, max_coords: (%d,%d), "\
		"origin: (%d,%d), max_pressure: %d, project_name: (%s,%s), "\
		"invert: (%d,%d,%d), fw_path: %s, ic_type: %d, boot_on_ldo: %d",
		pdata->boot_addr, pdata->max_x, pdata->max_y, pdata->origin[0],
		pdata->origin[1], pdata->max_pressure, pdata->project_name,
		pdata->model_name, pdata->x_invert, pdata->y_invert,
		pdata->xy_switch, pdata->fw_path, pdata->ic_type,
		pdata->boot_on_ldo);

	return pdata;
}
#else
static struct wacom_g5_platform_data *wacom_parse_dt(struct i2c_client *client)
{
	input_err(true, &client->dev, "no platform data specified\n");
	return ERR_PTR(-EINVAL);
}
#endif

static int wacom_i2c_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct wacom_g5_platform_data *pdata = dev_get_platdata(&client->dev);
	struct wacom_i2c *wac_i2c;
	struct input_dev *input;
	int ret = 0;

	/* Check I2C functionality */
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!ret) {
		input_err(true, &client->dev, "I2C functionality not supported\n");
		return -EIO;
	}

	/* init platform data */
	if (!pdata) {
		pdata = wacom_parse_dt(client);
		if (IS_ERR(pdata)) {
			input_err(true, &client->dev, "failed to parse dt\n");
			return PTR_ERR(pdata);
		}
	}

	ret = wacom_request_gpio(client, pdata);
	if (ret) {
		input_err(true, &client->dev, "failed to request gpio\n");
		return -EINVAL;
	}

	/* Obtain kernel memory space for wacom i2c */
	wac_i2c = devm_kzalloc(&client->dev, sizeof(*wac_i2c), GFP_KERNEL);
	if (!wac_i2c) {
		input_err(true, &client->dev, "failed to allocate wac_i2c\n");
		return -ENOMEM;
	}

	/* use managed input device */
	input = devm_input_allocate_device(&client->dev);
	if (!input) {
		input_err(true, &client->dev, "failed to allocate input device.\n");
		return -ENOMEM;
	}

	wac_i2c->client_boot = i2c_new_dummy(client->adapter, pdata->boot_addr);
	if (!wac_i2c->client_boot) {
		input_err(true, &client->dev, "failed to register sub client[0x%x]\n",
				pdata->boot_addr);
	}

	wac_i2c->client = client;
	wac_i2c->pdata = pdata;
	wac_i2c->input_dev = input;
	wac_i2c->irq = client->irq;
	wac_i2c->irq_pdct = gpio_to_irq(pdata->gpios[I_PDCT]);
	wac_i2c->wac_feature = &wacom_feature_EMR;

	/* set client data */
	i2c_set_clientdata(client, wac_i2c);
	i2c_set_clientdata(wac_i2c->client_boot, wac_i2c);

	/* Power on */
	wacom_resume_hw(wac_i2c);
	msleep(200);

	wac_i2c->power_state = POWER_STATE_ON;

	wacom_i2c_query(wac_i2c);

	wacom_i2c_set_input_values(client, wac_i2c, input);
	wacom_init_abs_params(wac_i2c);
	input_set_drvdata(input, wac_i2c);

	/* before registering input device, data in each input_dev must be set */
	ret = input_register_device(input);
	if (ret) {
		input_err(true, &client->dev,
				"failed to register input device.\n");
		/* managed input devices need not be explicitly unregistred or freed.*/
		return ret;
	}

	wac_i2c->pen_pdct = PDCT_NOSIGNAL;
	wac_i2c->fw_img = NULL;
	wac_i2c->fw_update_way = FW_NONE;

	wac_i2c->compulsory_flash_mode = wacom_compulsory_flash_mode;
	wac_i2c->reset_platform_hw = wacom_reset_hw;
	wac_i2c->get_irq_state = wacom_get_irq_state;

	/*Initializing for semaphor */
	mutex_init(&wac_i2c->lock);
	mutex_init(&wac_i2c->irq_lock);
	wake_lock_init(&wac_i2c->fw_wakelock, WAKE_LOCK_SUSPEND, "wacom fw");
	wake_lock_init(&wac_i2c->det_wakelock, WAKE_LOCK_SUSPEND, "wacom det");
	INIT_DELAYED_WORK(&wac_i2c->resume_work, wacom_i2c_resume_work);
	INIT_WORK(&wac_i2c->reset_work, wacom_i2c_reset_work);
#ifdef LCD_FREQ_SYNC
	mutex_init(&wac_i2c->freq_write_lock);
	INIT_WORK(&wac_i2c->lcd_freq_work, wacom_i2c_lcd_freq_work);
	INIT_DELAYED_WORK(&wac_i2c->lcd_freq_done_work, wacom_i2c_finish_lcd_freq_work);
	wac_i2c->use_lcd_freq_sync = true;
#endif
#ifdef WACOM_USE_SOFTKEY_BLOCK
	INIT_DELAYED_WORK(&wac_i2c->softkey_block_work, wacom_i2c_block_softkey_work);
	wac_i2c->block_softkey = false;
#endif
	INIT_WORK(&wac_i2c->update_work, wacom_i2c_update_work);
#ifdef WACOM_USE_SURVEY_MODE
	wake_lock_init(&wac_i2c->wakelock, WAKE_LOCK_SUSPEND, "wacom");
	init_completion(&wac_i2c->resume_done);
	INIT_DELAYED_WORK(&wac_i2c->irq_ignore_work, wacom_i2c_ignore_garbage_irq_work);
#endif

	/*Request IRQ */
	ret = devm_request_threaded_irq(&client->dev, wac_i2c->irq, NULL,
					wacom_interrupt,
					IRQF_DISABLED | IRQF_ONESHOT |
					wac_i2c->pdata->irq_type,
					"sec_epen_irq", wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev,
				"failed to request irq(%d) - %d\n",
				wac_i2c->irq, ret);
		goto err_request_irq;
	}

	ret = devm_request_threaded_irq(&client->dev, wac_i2c->irq_pdct, NULL,
					wacom_interrupt_pdct,
					IRQF_DISABLED | IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					"sec_epen_pdct", wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev,
				"failed to request irq(%d) - %d\n",
				wac_i2c->irq_pdct, ret);
		goto err_request_pdct_irq;
	}

	if (gpio_is_valid(wac_i2c->pdata->gpios[I_SENSE]))
		init_pen_insert(wac_i2c);

	wac_i2c->fw_wq = create_singlethread_workqueue(client->name);
	if (!wac_i2c->fw_wq) {
		input_err(ture, &client->dev, "fail to create workqueue for fw_wq\n");
		ret = -ENOMEM;
		goto err_create_fw_wq;
	}

	wacom_fw_update(wac_i2c, FW_BUILT_IN, false);

	wac_i2c->dev = device_create(sec_class, NULL, 0, NULL, "sec_epen");
	if (IS_ERR(wac_i2c->dev)) {
		input_err(true, &client->dev, "failed to create device(wac_i2c->dev)!\n");
		ret = -ENODEV;
		goto err_create_device;
	}

	dev_set_drvdata(wac_i2c->dev, wac_i2c);

	ret = sysfs_create_group(&wac_i2c->dev->kobj, &epen_attr_group);
	if (ret) {
		input_err(true, &client->dev,
			    "failed to create sysfs group\n");
		goto err_sysfs_create_group;
	}

	ret = sysfs_create_link(&wac_i2c->dev->kobj, &input->dev.kobj, "input");
	if (ret) {
		input_err(true, &client->dev,
			"failed to create sysfs link\n");
		goto err_create_symlink;
	}

	device_init_wakeup(&client->dev, true);

	return 0;

err_create_symlink:
	sysfs_remove_group(&wac_i2c->dev->kobj, &epen_attr_group);
err_sysfs_create_group:
	sec_device_destroy(wac_i2c->dev->devt);
err_create_device:
err_create_fw_wq:
err_request_pdct_irq:
err_request_irq:
#ifdef WACOM_USE_SURVEY_MODE
	wake_lock_destroy(&wac_i2c->wakelock);
#endif
	wake_lock_destroy(&wac_i2c->det_wakelock);
	wake_lock_destroy(&wac_i2c->fw_wakelock);
#ifdef LCD_FREQ_SYNC
	mutex_destroy(&wac_i2c->freq_write_lock);
#endif
	mutex_destroy(&wac_i2c->irq_lock);
	mutex_destroy(&wac_i2c->lock);

	input_err(true, &client->dev, "failed to probe\n");

	return ret;
}

static int wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);
	cancel_delayed_work_sync(&wac_i2c->resume_work);
	cancel_work_sync(&wac_i2c->reset_work);
#ifdef WACOM_USE_SURVEY_MODE
	cancel_delayed_work_sync(&wac_i2c->irq_ignore_work);
#endif

	sysfs_remove_group(&wac_i2c->dev->kobj, &epen_attr_group);
	sec_device_destroy(wac_i2c->dev->devt);
	wake_lock_destroy(&wac_i2c->det_wakelock);
	wake_lock_destroy(&wac_i2c->fw_wakelock);
#ifdef LCD_FREQ_SYNC
	mutex_destroy(&wac_i2c->freq_write_lock);
#endif
	mutex_destroy(&wac_i2c->irq_lock);
	mutex_destroy(&wac_i2c->lock);

	return 0;
}

static void wacom_i2c_shutdown(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&wac_i2c->pen_insert_dwork);
	cancel_delayed_work_sync(&wac_i2c->resume_work);
	cancel_work_sync(&wac_i2c->reset_work);
#ifdef WACOM_USE_SURVEY_MODE
	cancel_delayed_work_sync(&wac_i2c->irq_ignore_work);
#endif

	wacom_suspend_hw(wac_i2c);

	input_info(true, &client->dev, "%s\n", __func__);
}

#if defined(CONFIG_PM) && defined(WACOM_USE_SURVEY_MODE)
static int wacom_i2c_suspend(struct device *dev)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->aop_mode)
		reinit_completion(&wac_i2c->resume_done);

	wac_i2c->power_state = POWER_STATE_LPM_SUSPEND;

	return 0;
}

static int wacom_i2c_resume(struct device *dev)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	u8 irq_state;

	if (wac_i2c->aop_mode) {
		irq_state = wac_i2c->get_irq_state(wac_i2c);
		if (unlikely(wac_i2c->timeout && irq_state)) {
			wac_i2c->timeout = false;
			wacom_irq_state_recovery(wac_i2c, irq_state);
		}

		complete_all(&wac_i2c->resume_done);
	}

	wac_i2c->power_state = POWER_STATE_LPM_RESUME;

	return 0;
}

static SIMPLE_DEV_PM_OPS(wacom_pm_ops, wacom_i2c_suspend, wacom_i2c_resume);
#endif

static const struct i2c_device_id wacom_i2c_id[] = {
	{"wacom_w90xx", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, wacom_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id wacom_dt_ids[] = {
	{ .compatible = "wacom,w90xx" },
	{ }
};
#endif

/*Create handler for wacom_i2c_driver*/
static struct i2c_driver wacom_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wacom_w90xx",
#if defined(CONFIG_PM) && defined(WACOM_USE_SURVEY_MODE)
		.pm = &wacom_pm_ops,
#endif
		.of_match_table = of_match_ptr(wacom_dt_ids),
	},
	.probe = wacom_i2c_probe,
	.remove = wacom_i2c_remove,
	.shutdown = &wacom_i2c_shutdown,
	.id_table = wacom_i2c_id,
};

#ifdef CONFIG_BATTERY_SAMSUNG
extern int poweroff_charging;
#endif

static int __init wacom_i2c_init(void)
{
	int ret = 0;

#ifdef CONFIG_BATTERY_SAMSUNG
	if (poweroff_charging == 1) {
			pr_notice("%s : Do not load driver due to : lpm %d\n",
			 SECLOG, poweroff_charging);
		return 0;
	}
#endif

	ret = i2c_add_driver(&wacom_i2c_driver);
	if (ret)
		pr_err("%s: fail to i2c_add_driver\n", SECLOG);

	return ret;
}

static void __exit wacom_i2c_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}

module_init(wacom_i2c_init);
module_exit(wacom_i2c_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Driver for Wacom Digitizer Controller");
MODULE_LICENSE("GPL");
