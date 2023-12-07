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
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sec_sysfs.h>

#include "wacom.h"
#include "wacom_i2c_func.h"
#include "wacom_i2c_firm.h"

#define WACOM_FW_PATH_SDCARD "/sdcard/firmware/wacom_firm.bin"

#define GPIO_NAME_LEN 20

static struct wacom_features wacom_feature_EMR = {
	.comstat = COM_QUERY,
	.fw_version = 0x0,
	.update_status = FW_UPDATE_PASS,
};

unsigned char screen_rotate;
unsigned char user_hand = 1;
extern unsigned int system_rev;

extern int wacom_i2c_flash(struct wacom_i2c *wac_i2c);
extern int wacom_i2c_usermode(struct wacom_i2c *wac_i2c);

#ifdef CONFIG_OF
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
#define WACOM_GET_PDATA(drv_data) (drv_data ? drv_data->pdata : NULL)

struct wacom_i2c *wacom_get_drv_data(void * data)
{
	static void *drv_data = NULL;
	if (unlikely(data))
		drv_data = data;
	return (struct wacom_i2c *)drv_data;
}

int wacom_power(bool on)
{
	struct wacom_i2c *wac_i2c = wacom_get_drv_data(NULL);
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

static int wacom_suspend_hw(void)
{
	struct wacom_g5_platform_data *pdata = WACOM_GET_PDATA(wacom_get_drv_data(NULL));

	if (pdata->gpios[I_RESET])
		gpio_direction_output(pdata->gpios[I_RESET], 0);

	wacom_power(0);
	return 0;
}

static int wacom_resume_hw(void)
{
	struct wacom_g5_platform_data *pdata = WACOM_GET_PDATA(wacom_get_drv_data(NULL));

	if (pdata->gpios[I_RESET])
		gpio_direction_output(pdata->gpios[I_RESET], 1);

	/*gpio_direction_output(pdata->gpios[I_PDCT], 1);*/

	wacom_power(1);

	/*msleep(100);*/
	/*gpio_direction_input(pdata->gpios[I_PDCT]);*/
	return 0;
}

static int wacom_reset_hw(void)
{
	wacom_suspend_hw();
	msleep(20);
	wacom_resume_hw();

	return 0;
}

static void wacom_compulsory_flash_mode(bool en)
{
	struct wacom_i2c *wac_i2c = wacom_get_drv_data(NULL);
	struct i2c_client *client = wac_i2c->client;
	struct wacom_g5_platform_data *pdata = wac_i2c->pdata;

	if (likely(pdata->boot_on_ldo)) {
		static bool is_enabled = false;
		struct regulator *reg_fwe = NULL;
		int ret = 0;

		if (is_enabled == en) {
			input_info(true, &client->dev, "fwe already %s\n", en ? "enabled" : "disabled");
			return ;
		}

		reg_fwe = regulator_get(NULL, "wacom_fwe_1.8v");
		if (IS_ERR_OR_NULL(reg_fwe)) {
			input_err(true, &client->dev, "%s reg get err\n", __func__);
			regulator_put(reg_fwe);
			return ;
		}

		if (en) {
			ret = regulator_enable(reg_fwe);
			if (ret)
				input_err(true, &client->dev, "failed to enable fwe reg(%d)\n", ret);

			input_info(true, &client->dev, "fwe1.8v enabled\n");
		} else {
			if (regulator_is_enabled(reg_fwe))
				regulator_disable(reg_fwe);
				input_info(true, &client->dev, "fwe1.8v disabled\n");
		}
		regulator_put(reg_fwe);

		is_enabled = en;
	} else
		gpio_direction_output(pdata->gpios[I_FWE], en);
}

static int wacom_get_irq_state(void)
{
	struct wacom_g5_platform_data *pdata =
		WACOM_GET_PDATA(wacom_get_drv_data(NULL));

	int level = gpio_get_value(pdata->gpios[I_IRQ]);

	if (pdata->irq_type & (IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW))
		return !level;

	return level;
}
#endif

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

static void wacom_i2c_reset_hw(struct wacom_i2c *wac_i2c)
{
	/* Reset IC */
	wac_i2c->pdata->suspend_platform_hw();
	msleep(50);
	wac_i2c->pdata->resume_platform_hw();
	msleep(200);
}

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
	wac_i2c->pdata->resume_platform_hw();
	wac_i2c->power_state = POWER_STATE_ON;

	wac_i2c->pdata->compulsory_flash_mode(false); /* compensation to protect from flash mode  */

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

	if (wac_i2c->power_state != POWER_STATE_ON) {
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

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "wake_lock active. pass pwr off\n");
		goto out_power_off;
	}

	/* power off */
	wac_i2c->power_state = POWER_STATE_OFF;
	wac_i2c->pdata->suspend_platform_hw();

	wac_i2c->pdata->compulsory_flash_mode(false); /* compensation to protect from flash mode  */

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
			input_err(true, &wac_i2c->client->dev,
				"%s: LPM: -ERESTARTSYS if interrupted, %d\n",
				__func__, ret);
				return IRQ_HANDLED;
		}
	}

	if (wac_i2c->survey_state) {
		input_info(true, &wac_i2c->client->dev,
				"%s: run AOP interrupt handler\n", __func__);

		wac_i2c->survey_state = false;

		ret = wacom_get_aop_data(wac_i2c);

		input_report_key(wac_i2c->input_dev, KEY_WAKEUP_UNLOCK, 1);
		input_sync(wac_i2c->input_dev);

		input_report_key(wac_i2c->input_dev, KEY_WAKEUP_UNLOCK, 0);
		input_sync(wac_i2c->input_dev);

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

	if (wac_i2c->query_status == false)
		return IRQ_HANDLED;

	wac_i2c->pen_pdct = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &client->dev, "pdct %d(%d)\n",
		wac_i2c->pen_pdct, wac_i2c->pen_prox);
#else
	input_info(true, &client->dev, "pdct %d(%d) %d\n",
		wac_i2c->pen_pdct, wac_i2c->pen_prox, wac_i2c->pdata->get_irq_state());
#endif
#if 0
	if (wac_i2c->pen_pdct == PDCT_NOSIGNAL) {
		/* If rdy is 1, pen is still working*/
		if (wac_i2c->pen_prox == 0)
			forced_release(wac_i2c);
	} else if (wac_i2c->pen_prox == 0)
		forced_hover(wac_i2c);
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
		request_threaded_irq(
			irq, NULL,
			wacom_pen_detect,
			IRQF_DISABLED | IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"pen_insert", wac_i2c);
	if (ret < 0) {
		input_err(true, &client->dev,
			"failed to request pen insert irq\n");
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
	if (wac_i2c->aop_mode) {
		if (wac_i2c->survey_state)
			wacom_i2c_exit_survey_mode(wac_i2c);
	} else
#endif
		wacom_power_on(wac_i2c);

	return ret;
}

static void wacom_i2c_input_close(struct input_dev *dev)
{
	struct wacom_i2c *wac_i2c = input_get_drvdata(dev);

#ifdef WACOM_USE_SURVEY_MODE
	if (wac_i2c->aop_mode)
		wacom_i2c_enter_survey_mode(wac_i2c);
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

static int wacom_check_emr_prox(struct wacom_g5_callbacks *cb)
{
	struct wacom_i2c *wac = container_of(cb, struct wacom_i2c, callbacks);
	input_info(true, &wac->client->dev, "%s:\n", __func__);

	return wac->pen_prox;
}

static void wacom_i2c_resume_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
			container_of(work, struct wacom_i2c, resume_work.work);
	struct i2c_client *client = wac_i2c->client;
	u8 irq_state = 0;
	int ret = 0;

	if (wac_i2c->wcharging_mode)
		wacom_i2c_set_sense_mode(wac_i2c);

	irq_state = wac_i2c->pdata->get_irq_state();
	wacom_enable_irq(wac_i2c, true);

	if (unlikely(irq_state)) {
		u8 data[COM_COORD_NUM] = {0,};
		input_info(true, &client->dev, "irq was enabled\n");
		ret = wacom_i2c_recv(wac_i2c, data, COM_COORD_NUM, false);
		if (ret < 0) {
			input_err(true, &client->dev, "%s failed to read i2c.L%d\n", __func__,
				__LINE__);
		}
	}

	ret = wacom_i2c_modecheck(wac_i2c);
	if(ret) wacom_i2c_usermode(wac_i2c);

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &client->dev, "%s\n", __func__);
#else
	ret = gpio_get_value(wac_i2c->pdata->gpios[I_PDCT]);
	input_info(true, &client->dev, "%s %d%d\n", __func__, irq_state, ret);
#endif
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

int load_fw_built_in(struct wacom_i2c *wac_i2c)
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

int load_fw_sdcard(struct wacom_i2c *wac_i2c)
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

int wacom_i2c_load_fw(struct wacom_i2c *wac_i2c, u8 fw_path)
{
	int ret = 0;
	struct fw_image *fw_img;
	struct i2c_client *client = wac_i2c->client;

	switch (fw_path) {
	case FW_BUILT_IN:
		ret = load_fw_built_in(wac_i2c);
		break;
	case FW_IN_SDCARD:
		ret = load_fw_sdcard(wac_i2c);
		break;
	default:
		input_err(true, &client->dev, "unknown path(%d)\n", fw_path);
		goto err_load_fw;
	}

	if (ret < 0)
		goto err_load_fw;

	fw_img = wac_i2c->fw_img;

	/* header check */
	if (fw_img->hdr_ver == 1 && fw_img->hdr_len == sizeof(struct fw_image)) {
		fw_data = (u8 *)fw_img->data;
		if (fw_path == FW_BUILT_IN) {
			fw_ver_file = fw_img->fw_ver1;
			memcpy(fw_chksum, fw_img->checksum, 5);
		}
	} else {
		input_err(true, &client->dev, "no hdr\n");
		fw_data = (u8 *)fw_img;
	}

	return ret;

err_load_fw:
	fw_data = NULL;
	return ret;
}

void wacom_i2c_unload_fw(struct wacom_i2c *wac_i2c)
{
	switch (wac_i2c->fw_path) {
	case FW_BUILT_IN:
		release_firmware(wac_i2c->firm_data);
		break;
	case FW_IN_SDCARD:
		kfree(wac_i2c->fw_img);
		break;
	default:
		break;
	}

	wac_i2c->fw_img = NULL;
	wac_i2c->fw_path = FW_NONE;
	wac_i2c->firm_data = NULL;
	fw_data = NULL;
}

int wacom_fw_update(struct wacom_i2c *wac_i2c, u8 fw_path, bool bforced)
{
	struct i2c_client *client = wac_i2c->client;
	u32 fw_ver_ic = wac_i2c->wac_feature->fw_version;
	int ret;

	if (wake_lock_active(&wac_i2c->fw_wakelock)) {
		input_info(true, &client->dev, "update is already running. pass\n");
		return 0;
	}

	wacom_enable_irq(wac_i2c, false);

	input_info(true, &client->dev, "%s\n", __func__);

	ret = wacom_i2c_load_fw(wac_i2c, fw_path);
	if (ret < 0) {
		input_info(true, &client->dev, "failed to load fw data\n");
		wac_i2c->wac_feature->update_status = FW_UPDATE_FAIL;
		goto err_update_load_fw;
	}
	wac_i2c->fw_path = fw_path;

	/* firmware info */
	input_info(true, &client->dev, "wacom fw ver : 0x%x, new fw ver : 0x%x\n",
		wac_i2c->wac_feature->fw_version, fw_ver_file);

	if (!bforced) {
		if (fw_ver_ic == fw_ver_file) {
			input_info(true, &client->dev, "pass fw update\n");
			wac_i2c->do_crc_check = true;
			/* need to check crc */
		} else if (fw_ver_ic > fw_ver_file){
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

	if (wac_i2c->fw_path == FW_NONE)
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
		wac_i2c->wac_feature->fw_version, fw_ver_file);

	return sprintf(buf, "%04X\t%04X\n",
		wac_i2c->wac_feature->fw_version,
		fw_ver_file);
}

static ssize_t epen_firmware_update_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	u8 fw_path = FW_NONE;

	input_info(true, &wac_i2c->client->dev, "%s\n", __func__);

	switch (*buf) {
	case 'i':
	case 'I':
		fw_path = FW_IN_SDCARD;
		break;
	case 'k':
	case 'K':
		fw_path = FW_BUILT_IN;
		break;
	default:
		input_err(true, &wac_i2c->client->dev, "wrong parameter\n");
		return count;
	}

	wacom_fw_update(wac_i2c, fw_path, true);

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
		/*wacom_i2c_reset_hw(wac_i2c->pdata);*/
		wacom_i2c_reset_hw(wac_i2c);
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

	cmd = WACOM_I2C_STOP;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		input_err(true, &client->dev, "failed to send stop command\n");
		return -1;
	}

	usleep_range(500, 500);

	cmd = WACOM_I2C_GRID_CHECK;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		input_err(true, &client->dev, "failed to send stop command\n");
		return -1;
	}

	msleep(150);

	cmd = WACOM_STATUS;
	do {
		input_info(true, &client->dev, "read status, retry %d\n", retry);
		ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
		if (ret != 1) {
			input_err(true, &client->dev, "failed to send cmd(ret:%d)\n", ret);
			continue;
		}
		usleep_range(500, 500);
		ret = wacom_i2c_recv(wac_i2c, buf, 4, false);
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
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		input_err(true, &client->dev, "failed to send stop cmd for end\n");
		return -2;
	}

	cmd = WACOM_I2C_START;
	ret = wacom_i2c_send(wac_i2c, &cmd, 1, false);
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
		wac_i2c->pdata->resume_platform_hw();
		input_info(true, &client->dev, "pwr on\n");
		msleep(200);
	}

	while (retry--) {
		ret = wacom_open_test(wac_i2c);
		if (!ret)
			break;

		input_err(true, &client->dev, "failed. retry %d\n", retry);
		wac_i2c->pdata->reset_platform_hw();
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
#endif
	&dev_attr_epen_wcharging_mode.attr,
	NULL,
};

static struct attribute_group epen_attr_group = {
	.attrs = epen_attributes,
};

static void wacom_init_abs_params(struct wacom_i2c *wac_i2c)
{
	int min_x, min_y;
	int max_x, max_y;
	int pressure;

	min_x = wac_i2c->pdata->min_x;
	max_x = wac_i2c->pdata->max_x;
	min_y = wac_i2c->pdata->min_y;
	max_y = wac_i2c->pdata->max_y;
	pressure = wac_i2c->pdata->max_pressure;

	if (wac_i2c->pdata->xy_switch) {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, min_y,
			max_y, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, min_x,
			max_x, 4, 0);
	} else {
		input_set_abs_params(wac_i2c->input_dev, ABS_X, min_x,
			max_x, 4, 0);
		input_set_abs_params(wac_i2c->input_dev, ABS_Y, min_y,
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
		if (!pdata->gpios[i])
			continue;

		if (pdata->boot_on_ldo) {
			if (i == I_FWE)
				continue;
		}

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
	int ret;
	int i;
	char name[GPIO_NAME_LEN] = {0, };
	u32 tmp[5] = {0, };

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

			if (pdata->gpios[i] < 0) {
				input_err(true, &client->dev, "failed to get gpio %s, %d\n", name, pdata->gpios[i]);
				pdata->gpios[i] = 0;
			}
		} else {
			input_info(true, &client->dev, "theres no prop %s\n", name);
			pdata->gpios[i] = 0;
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

	ret = of_property_read_u32_array(np, "wacom,max_coords", tmp, 2);
	if (ret) {
		input_err(true, &client->dev, "failed to read max coords %d\n", ret);
		return ERR_PTR(-EINVAL);
	}
	pdata->max_x = tmp[0];
	pdata->max_y = tmp[1];

	ret = of_property_read_u32(np, "wacom,max_pressure", tmp);
	if (ret) {
		input_err(true, &client->dev, "failed to read max pressure %d\n", ret);
		return ERR_PTR(-EINVAL);
	}
	pdata->max_pressure = tmp[0];

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
		pdata->ic_type = WACOM_IC_9012;
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

	if (of_find_property(np, "wacom,boot_on_ldo", NULL)) {
		ret = of_property_read_u32(np, "wacom,boot_on_ldo", &pdata->boot_on_ldo);
		if (ret) {
			input_err(true, &client->dev, "failed to read boot_on_ldo %d\n", ret);
			pdata->boot_on_ldo = 0;
		}
	}

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

	/*Check I2C functionality */
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

	/*Obtain kernel memory space for wacom i2c */
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

	/* set client data */
	i2c_set_clientdata(client, wac_i2c);
	i2c_set_clientdata(wac_i2c->client_boot, wac_i2c);

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

	wac_i2c->wac_feature = &wacom_feature_EMR;

	/* init_completion(&wac_i2c->init_done); */
	wac_i2c->pen_pdct = PDCT_NOSIGNAL;
	wac_i2c->fw_img = NULL;
	wac_i2c->fw_path = FW_NONE;

	wacom_get_drv_data(wac_i2c);

	wac_i2c->pdata->compulsory_flash_mode = wacom_compulsory_flash_mode;
	wac_i2c->pdata->suspend_platform_hw = wacom_suspend_hw;
	wac_i2c->pdata->resume_platform_hw = wacom_resume_hw;
	wac_i2c->pdata->reset_platform_hw = wacom_reset_hw;
	wac_i2c->pdata->get_irq_state = wacom_get_irq_state;

	/*Register callbacks */
	wac_i2c->callbacks.check_prox = wacom_check_emr_prox;
	if (wac_i2c->pdata->register_cb)
		wac_i2c->pdata->register_cb(&wac_i2c->callbacks);

	/* Power on */
	wac_i2c->pdata->resume_platform_hw();
	if (false == pdata->boot_on_ldo)
		msleep(200);

	wac_i2c->power_state = POWER_STATE_ON;

	wacom_i2c_query(wac_i2c);

	/*Initializing for semaphor */
	mutex_init(&wac_i2c->lock);
	mutex_init(&wac_i2c->irq_lock);
	wake_lock_init(&wac_i2c->fw_wakelock, WAKE_LOCK_SUSPEND, "wacom fw");
	wake_lock_init(&wac_i2c->det_wakelock, WAKE_LOCK_SUSPEND, "wacom det");
	INIT_DELAYED_WORK(&wac_i2c->resume_work, wacom_i2c_resume_work);
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

	wac_i2c->fw_wq = create_singlethread_workqueue(client->name);
	if (!wac_i2c->fw_wq) {
		input_err(ture, &client->dev, "fail to create workqueue for fw_wq\n");
		ret = -ENOMEM;
		goto err_create_fw_wq;
	}

	init_pen_insert(wac_i2c);

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

	/*complete_all(&wac_i2c->init_done);*/

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

	if (!wac_i2c)
		return;

	wac_i2c->pdata->suspend_platform_hw();

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

	if (wac_i2c->aop_mode)
		complete_all(&wac_i2c->resume_done);

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
