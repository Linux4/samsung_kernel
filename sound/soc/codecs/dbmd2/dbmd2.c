/*
 * DSPG DBMD2 codec driver
 *
 * Copyright (C) 2014 DSP Group
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_i2c.h>
#include <linux/of_gpio.h>
#endif
#include <linux/kfifo.h>
#include <linux/vmalloc.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>

#include "dbmd2-interface.h"
#include "dbmd2-customer.h"
#include "dbmd2-va-regmap.h"
#include "dbmd2-vqe-regmap.h"
#include "dbmd2-i2s.h"
#include <sound/dbmd2-export.h>

/* Size must be power of 2 */
#define MAX_KFIFO_BUFFER_SIZE			(131072 * 2) /* >4 seconds */
#define MAX_RETRIES_TO_WRITE_TOBUF		3
#define MAX_AMODEL_SIZE				(128 * 1024)

#define DRIVER_VERSION				"3.016"

#define DBD2_AUDIO_MODE_PCM			0
#define DBD2_AUDIO_MODE_MU_LAW			1

#define DETECTION_MODE_PHRASE			1
#define DETECTION_MODE_VOICE_ENERGY		2


#define VA_MIXER_REG(cmd)			\
	(((cmd) >> 16) & 0x7fff)
#define VQE_MIXER_REG(cmd)			\
	(((cmd) >> 16) & 0xffff)

static const char *dbmd2_power_mode_names[DBMD2_PM_STATES] = {
	"BOOTING",
	"ACTIVE",
	"FALLING_ASLEEP",
	"SLEEPING",
};

enum dbmd2_states {
	DBMD2_IDLE = 0,
	DBMD2_DETECTION = 1,
	DBMD2_RESERVED = 2,
	DBMD2_BUFFERING = 3,
	DBMD2_SLEEP_PLL_ON = 4,
	DBMD2_SLEEP_PLL_OFF = 5,
	DBMD2_HIBERNATE = 6,
	DBMD2_NR_OF_STATES,
};

static const char *dbmd2_state_names[DBMD2_NR_OF_STATES] = {
	"IDLE",
	"DETECTION",
	"RESERVED_2",
	"BUFFERING",
	"SLEEP_PLL_ON",
	"SLEEP_PLL_OFF",
	"HIBERNATE",
};

/* Global Variables */
struct dbmd2_private *dbd2_data;
void (*g_event_callback)(int) = NULL;
void (*g_set_i2c_freq_callback)(struct i2c_adapter*, enum i2c_freq_t) = NULL;

/* Forward declarations */
static int dbmd2_perform_recovery(struct dbmd2_private *p);

static const char *dbmd2_fw_type_to_str(int fw_type)
{
	switch (fw_type) {
	case DBMD2_FW_VA: return "VA";
	case DBMD2_FW_VQE: return "VQE";
	case DBMD2_FW_PRE_BOOT: return "PRE_BOOT";
	default: return "ERROR";
	}
}

static void dbmd2_set_va_active(struct dbmd2_private *p)
{
	/* set VA as active firmware */
	p->active_fw = DBMD2_FW_VA;
	/* reset all flags */
	memset(&p->va_flags, 0, sizeof(p->va_flags));
	memset(&p->vqe_flags, 0, sizeof(p->vqe_flags));
}

static void dbmd2_set_vqe_active(struct dbmd2_private *p)
{
	/* set VQE as active firmware */
	p->active_fw = DBMD2_FW_VQE;
	/* reset all flags */
	memset(&p->va_flags, 0, sizeof(p->va_flags));
	memset(&p->vqe_flags, 0, sizeof(p->vqe_flags));
}

static void dbmd2_set_boot_active(struct dbmd2_private *p)
{
	/* set nothing as active firmware */
	p->active_fw = DBMD2_FW_PRE_BOOT;
	p->device_ready = false;
	p->asleep = false;
}

static void dbmd2_reset_set(struct dbmd2_private *p)
{
	if (p->pdata->gpio_d2strap1)
		gpio_direction_output(p->pdata->gpio_d2strap1,
				p->pdata->gpio_d2strap1_rst_val);

	gpio_set_value(p->pdata->gpio_reset, 0);
}

static void dbmd2_reset_release(struct dbmd2_private *p)
{
	gpio_set_value(p->pdata->gpio_reset, 1);

	if (p->pdata->gpio_d2strap1)
		gpio_direction_input(p->pdata->gpio_d2strap1);
}

static void dbmd2_reset_sequence(struct dbmd2_private *p)
{
	dbmd2_reset_set(p);
	usleep_range(300, 400);
	dbmd2_reset_release(p);
}

static int dbmd2_can_wakeup(struct dbmd2_private *p)
{
	return (p->pdata->gpio_wakeup < 0 ? 0 : 1);
}

static void dbmd2_wakeup_set(struct dbmd2_private *p)
{
	if (p->pdata->gpio_wakeup < 0)
		return;
	dev_dbg(p->dev, "%s\n", __func__);
	gpio_set_value(p->pdata->gpio_wakeup, 1);
}

static void dbmd2_wakeup_release(struct dbmd2_private *p)
{
	if (p->pdata->gpio_wakeup < 0)
		return;
	dev_dbg(p->dev, "%s\n", __func__);
	gpio_set_value(p->pdata->gpio_wakeup, 0);
}

static unsigned long dbmd2_clk_get_rate(
		struct dbmd2_private *p, enum dbmd2_clocks clk)
{
	switch (clk) {
	case DBMD2_CLK_CONSTANT:
		if (p->pdata->constant_clk_rate != -1)
			return p->pdata->constant_clk_rate;
		else
			return customer_dbmd2_clk_get_rate(p, clk);
		break;
	case DBMD2_CLK_MASTER:
		if (p->pdata->master_clk_rate != -1)
			return p->pdata->master_clk_rate;
		else
			return customer_dbmd2_clk_get_rate(p, clk);
		break;
	}
	return 0;
}

static int dbmd2_clk_enable(struct dbmd2_private *p, enum dbmd2_clocks clk)
{
	int ret = 0;

	switch (clk) {
	case DBMD2_CLK_CONSTANT:
		if (p->constant_clk)
			ret = clk_prepare_enable(p->constant_clk);
		else
			ret = customer_dbmd2_clk_enable(p, clk);
		break;
	case DBMD2_CLK_MASTER:
		if (p->master_clk)
			ret = clk_prepare_enable(p->master_clk);
		else
			ret = customer_dbmd2_clk_enable(p, clk);
		break;
	}
	if (ret < 0)
		dev_err(p->dev, "%s: %s clock enable failed\n",
			__func__,
			clk == DBMD2_CLK_CONSTANT ? "constant" : "master");
	else
		ret = 0;

	return ret;
}

static int dbmd2_clk_disable(struct dbmd2_private *p, enum dbmd2_clocks clk)
{
	switch (clk) {
	case DBMD2_CLK_CONSTANT:
		if (p->constant_clk)
			clk_disable_unprepare(p->constant_clk);
		else
			customer_dbmd2_clk_disable(p, clk);
		break;
	case DBMD2_CLK_MASTER:
		if (p->master_clk)
			clk_disable_unprepare(p->master_clk);
		else
			customer_dbmd2_clk_disable(p, clk);
		break;
	}
	return 0;
}

static void dbmd2_lock(struct dbmd2_private *p)
{
	mutex_lock(&p->p_lock);
}

static void dbmd2_unlock(struct dbmd2_private *p)
{
	mutex_unlock(&p->p_lock);
}

static int dbmd2_verify_checksum(struct dbmd2_private *p,
	const u8 *expect, const u8 *got, size_t size)
{
	int ret;

	ret = memcmp(expect, got, size);
	if (ret) {
		switch (size) {
		case 4:
			dev_info(p->dev,
				"%s: Got:      0x%02x 0x%02x 0x%02x 0x%02x\n",
				__func__,
				got[0], got[1], got[2], got[3]);
			dev_info(p->dev,
				"%s: Expected: 0x%02x 0x%02x 0x%02x 0x%02x\n",
				__func__,
				expect[0], expect[1], expect[2], expect[3]);
			break;
		default:
			break;
		}
	}
	return ret;
}

/* FIXME: likely to be removed when re-adding support for overlay FW */
#if 0
static ssize_t dbmd2_send_data(struct dbmd2_private *p, const void *buf,
			       size_t len)
{
	return p->chip->write(p, buf, len);
}
#endif

static int dbmd2_send_cmd(struct dbmd2_private *p, u32 command, u16 *response)
{
	int ret;

	switch (p->active_fw) {
	case DBMD2_FW_VA:
		ret = p->chip->send_cmd_va(p, command, response);
		break;
	case DBMD2_FW_VQE:
		ret = p->chip->send_cmd_vqe(p, command, response);
		break;
	default:
		dev_err(p->dev, "%s: Don't know how to handle fw type %d\n",
			__func__, p->active_fw);
		ret = -EIO;
		break;
	}
	return ret;
}

static int dbmd2_va_alive(struct dbmd2_private *p)
{
	u16 result = 0;
	int ret = 0;
	unsigned long stimeout = jiffies + msecs_to_jiffies(1000);

	do {

		/* check if VA firmware is still alive */
		ret = dbmd2_send_cmd(p, DBMD2_VA_FW_ID, &result);
		if (ret < 0)
			continue;
		if (result == 0xdbd2)
			ret = 0;
		else
			ret = -1;
	} while (time_before(jiffies, stimeout) && ret != 0);

	if (ret != 0)
		dev_err(p->dev, "%s: VA firmware dead\n", __func__);

	return ret;
}

static int dbmd2_vqe_alive(struct dbmd2_private *p)
{
	unsigned long timeout;
	int ret = -EIO;
	u16 resp;

	usleep_range(21000, 22000);

	timeout = jiffies + msecs_to_jiffies(1000);
	while (time_before(jiffies, timeout)) {
		ret = dbmd2_send_cmd(p,
				     DBMD2_VQE_SET_PING_CMD | 0xaffe,
				     &resp);
		if (ret == 0 && resp == 0xaffe)
			break;
		usleep_range(10000, 11000);
	}
	if (ret != 0)
		dev_dbg(p->dev, "%s: VQE firmware dead\n", __func__);

	if (resp != 0xaffe)
		ret = -EIO;

	return ret;
}

static int dbmd2_vqe_mode_valid(struct dbmd2_private *p, unsigned int mode)
{
	unsigned int i;

	if (p->pdata->vqe_modes_values == 0)
		return 1;

	for (i = 0; i < p->pdata->vqe_modes_values; i++) {
		if (mode == p->pdata->vqe_modes_value[i])
			return 1;
	}

	dev_dbg(p->dev, "%s: Invalid VQE mode: 0x%x\n", __func__, mode);

	return 0;
}


static int dbmd2_va_set_speed(struct dbmd2_private *p,
			      enum dbmd2_va_speeds speed)
{
	int ret;

	dev_info(p->dev, "%s: set speed to %u\n",
		__func__, speed);

	ret = dbmd2_send_cmd(
		p,
		DBMD2_VA_CLK_CFG | p->pdata->va_speed_cfg[speed].cfg,
		NULL);
	if (ret != 0)
		ret = -EIO;
	return ret;
}

/* FIXME: likely to be removed when re-adding support for overlay FW */
#if 0
static int dbmd2_va_set_high_speed(struct dbmd2_private *p)
{
	int ret;

	ret = dbmd2_va_set_speed(p, DBMD2_VA_SPEED_MAX);
	if (ret != 0) {
		dev_err(p->dev,
			"%s: could not change to high speed\n",
			__func__);
	}
	return ret;
}
#endif

static int dbmd2_buf_to_int(const char *buf)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	return (int)val;
}

static int dbmd2_set_bytes_per_sample(struct dbmd2_private *p,
				      unsigned int mode)
{
	int ret;
	u16 resp = 0xffff;

	/* Changing the buffer conversion causes trouble: adapting the
	 * UART baudrate doesn't work anymore (firmware bug?) */
	if (((mode == DBD2_AUDIO_MODE_PCM) && (p->bytes_per_sample == 2)) ||
	    ((mode == DBD2_AUDIO_MODE_MU_LAW) &&
	     (p->bytes_per_sample == 1)))
		return 0;

	/* read configuration */
	ret = dbmd2_send_cmd(p,
			     DBMD2_VA_AUDIO_BUFFER_CONVERSION,
			     &resp);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to read DBMD2_VA_AUDIO_BUFFER_CONVERSION\n",
			__func__);
		return ret;
	}

	resp &= ~(1 << 12);
	resp |= (mode << 12);
	ret = dbmd2_send_cmd(p,
			     DBMD2_VA_AUDIO_BUFFER_CONVERSION | resp,
			     NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set DBMD2_VA_AUDIO_BUFFER_CONVERSION\n",
			__func__);
		return ret;
	}

	switch (mode) {
	case DBD2_AUDIO_MODE_PCM:
		p->bytes_per_sample = 2;
		p->audio_mode = mode;
		break;
	case DBD2_AUDIO_MODE_MU_LAW:
		p->bytes_per_sample = 1;
		p->audio_mode = mode;
		break;
	default:
		break;
	}

	return 0;
}

static int dbd2_set_backlog_len(struct dbmd2_private *p, u16 history)
{
	int ret;
	u16 mode;

	history &= ~(1 << 12);

	mode = ((u16)(p->audio_mode << 12) | history);

	dev_info(p->dev, "%s: history 0x%x, mode 0x%x\n",
		__func__, (u32)history, (u32)mode);

	ret = dbmd2_send_cmd(p,
			  DBMD2_VA_AUDIO_BUFFER_CONVERSION | mode ,
			  NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set backlog size\n", __func__);
		return ret;
	}

	p->va_backlog_length = history;

	return 0;
}


static int dbmd2_sleeping(struct dbmd2_private *p)
{
	return p->asleep;
}

static int dbmd2_vqe_set_tdm_bypass(struct dbmd2_private *p, int onoff)
{
	int ret;

	ret = dbmd2_send_cmd(p,
			     DBMD2_VQE_SET_HW_TDM_BYPASS_CMD |
			       p->pdata->vqe_tdm_bypass_config,
			     NULL);
	if (ret != 0)
		dev_err(p->dev,
			"%s: failed to %s TDM bypass (%x)\n",
			__func__,
			(onoff ? "enable" : "disable"),
			p->pdata->vqe_tdm_bypass_config);
	return 0;
}

static int dbmd2_force_wake(struct dbmd2_private *p)
{
	int ret = 0;
	u16 resp = 0xffff;

	/* assert wake pin */
	p->wakeup_set(p);

	if (p->active_fw == DBMD2_FW_VQE) {
		p->clk_enable(p, DBMD2_CLK_CONSTANT);
		usleep_range(1000, 2000);
	}

	p->chip->transport_enable(p, true);

	/* give some time to wakeup */
	msleep(35); /* FIXME: why so long? check with FW guys */

	if (p->active_fw == DBMD2_FW_VA) {
		/* test if VA firmware is up */
		ret = dbmd2_va_alive(p);
		if (ret < 0) {
			dev_err(p->dev,	"%s: VA fw did not wakeup\n",
				__func__);
			ret = -EIO;
			goto out;
		}

		/* get operation mode register */
		ret = dbmd2_send_cmd(p, DBMD2_VA_OPR_MODE, &resp);
		if (ret < 0) {
			dev_err(p->dev,	"%s: failed to get operation mode\n",
				__func__);
			goto out;
		}
		p->va_flags.mode = resp;
	} else {
		/* test if VQE firmware is up */
		ret = dbmd2_vqe_alive(p);
		if (ret != 0) {
			dev_err(p->dev, "%s: VQE fw did not wakeup\n",
				__func__);
			ret = -EIO;
			goto out;
		}
		/* default mode is idle mode */
	}

	p->power_mode = DBMD2_PM_ACTIVE;

	/* make it not sleeping */
	p->asleep = false;

	dev_dbg(p->dev, "%s: woke up\n", __func__);
out:
	return ret;
}

static int dbmd2_wake(struct dbmd2_private *p)
{

	/* if chip not sleeping there is nothing to do */
	if (!dbmd2_sleeping(p) && p->va_flags.mode != DBMD2_DETECTION)
		return 0;

	return dbmd2_force_wake(p);
}

static int dbmd2_set_power_mode(
		struct dbmd2_private *p, enum dbmd2_power_modes mode)
{
	int ret = 0;
	enum dbmd2_power_modes new_mode = p->power_mode;

	dev_dbg(p->dev, "%s: would move %s -> %s (%2.2d -> %2.2d)\n",
			__func__,
			dbmd2_power_mode_names[p->power_mode],
			dbmd2_power_mode_names[mode],
			p->power_mode,
			mode);

	switch (p->power_mode) {
	case DBMD2_PM_BOOTING:
		switch (mode) {
		case DBMD2_PM_FALLING_ASLEEP:
			/* queue delayed work to set the chip to sleep*/
			queue_delayed_work(p->dbmd2_workq,
					&p->delayed_pm_work,
					msecs_to_jiffies(100));

		case DBMD2_PM_BOOTING:
		case DBMD2_PM_ACTIVE:
			new_mode = mode;
			break;

		default:
			goto illegal_transition;
		}
		break;

	case DBMD2_PM_ACTIVE:
		switch (mode) {
		case DBMD2_PM_ACTIVE:
			if (p->va_flags.mode == DBMD2_BUFFERING ||
				p->va_flags.mode == DBMD2_DETECTION)
				ret = dbmd2_wake(p);
			break;

		case DBMD2_PM_FALLING_ASLEEP:
			if (p->va_flags.mode == DBMD2_BUFFERING ||
				p->va_flags.mode == DBMD2_DETECTION ||
				p->vqe_flags.in_call) {
				dev_dbg(p->dev,
					"%s: no sleep during detection/buff/in call\n",
					__func__);
			} else {
			    /* queue delayed work to set the chip to sleep */
			    queue_delayed_work(p->dbmd2_workq,
				    &p->delayed_pm_work,
				    msecs_to_jiffies(100));
			    new_mode = mode;
			}
			break;

		case DBMD2_PM_BOOTING:
			new_mode = mode;
			break;

		default:
			goto illegal_transition;
		}
		break;

	case DBMD2_PM_FALLING_ASLEEP:
		switch (mode) {
		case DBMD2_PM_BOOTING:
		case DBMD2_PM_ACTIVE:
			/*
			 * flush queue if going to active
			 */
			p->unlock(p);
			cancel_delayed_work_sync(&p->delayed_pm_work);
			p->lock(p);
			new_mode = mode;
			/* wakeup chip */
			ret = dbmd2_wake(p);
			break;

		case DBMD2_PM_FALLING_ASLEEP:
			break;

		default:
			goto illegal_transition;
		}
		break;

	case DBMD2_PM_SLEEPING:
		/*
		 * wakeup the chip if going to active/booting
		 */
		switch (mode) {
		case DBMD2_PM_FALLING_ASLEEP:
			dev_dbg(p->dev,
				"%s: already sleeping; leave it this way...",
				__func__);
			new_mode = DBMD2_PM_SLEEPING;
			break;
		case DBMD2_PM_ACTIVE:
		case DBMD2_PM_BOOTING:
			ret = dbmd2_wake(p);
			if (ret) {
				dev_err(p->dev,
					"%s: failed to wake the chip up!\n",
					__func__);
				return ret;
			}
		case DBMD2_PM_SLEEPING:
			new_mode = mode;
			break;
		default:
			goto illegal_transition;
		}
		break;

	default:
		dev_err(p->dev, "%s: unknown power mode: %d",
				__func__, p->power_mode);
		return -EINVAL;
	}

	dev_dbg(p->dev, "%s: has moved  %s -> %s (%2.2d -> %2.2d)\n",
			__func__,
			dbmd2_power_mode_names[p->power_mode],
			dbmd2_power_mode_names[new_mode],
			p->power_mode,
			new_mode);

	p->power_mode = new_mode;

	return 0;

illegal_transition:
	dev_err(p->dev, "%s: can't move %s -> %s\n", __func__,
		dbmd2_power_mode_names[p->power_mode],
		dbmd2_power_mode_names[mode]);
	return -EINVAL;
}


static int dbmd2_set_mode(struct dbmd2_private *p, int mode)
{
	int ret = 0;
	unsigned int cur_opmode = p->va_flags.mode;
	enum dbmd2_power_modes new_power_mode = p->power_mode;

	if (mode >= 0 && mode < DBMD2_NR_OF_STATES) {
		dev_dbg(p->dev, "%s: mode: %d (%s)\n",
			__func__, mode, dbmd2_state_names[mode]);
	} else {
		dev_dbg(p->dev, "%s: mode: %d (invalid)\n", __func__, mode);
		return -EINVAL;
	}

	mode &= 0xffff;

	/*
	 * transform HIBERNATE to SLEEP in case no wakeup pin
	 * is available
	 */
	if (!dbmd2_can_wakeup(p) && mode == DBMD2_HIBERNATE)
		mode = DBMD2_SLEEP_PLL_ON;

	p->va_flags.buffering = 0;

	p->va_flags.irq_inuse = 0;

	/* wake up if asleep */
	dbmd2_wake(p);

	if (cur_opmode == DBMD2_DETECTION)
		/* explicit transp. enable if in detection */
		p->chip->transport_enable(p, true);

	switch (mode) {
	case DBMD2_IDLE:
		/* set power state to FALLING ASLEEP */
		new_power_mode = DBMD2_PM_FALLING_ASLEEP;
		break;

	case DBMD2_DETECTION:
		p->va_flags.irq_inuse = 1;
	case DBMD2_BUFFERING:
		/* switch to ACTIVE */
		new_power_mode = DBMD2_PM_ACTIVE;
		break;

	case DBMD2_SLEEP_PLL_OFF:
	case DBMD2_SLEEP_PLL_ON:
	case DBMD2_HIBERNATE:
		p->asleep = true;
		break;
	}

	if (mode == DBMD2_DETECTION) {
		if (!p->va_flags.a_model_loaded &&
			(p->va_detection_mode == DETECTION_MODE_PHRASE)) {
			dev_err(p->dev,
				"%s: Cannot set detection mode, AModel was not loaded\n",
				__func__);
			p->va_flags.irq_inuse = 0;
			ret = -1;
			goto out;
		}
		ret = dbmd2_send_cmd(p,
				     DBMD2_VA_RECOGNITION_MODE |
				     p->va_detection_mode,
				     NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to set recognition mode 0x%x\n",
				__func__, mode);
			p->va_flags.irq_inuse = 0;
			goto out;
		}

		dev_info(p->dev, "%s: Recognition mode was set to  0x%x\n",
				__func__, p->va_detection_mode);
	}

	/* set operation mode register */
	ret = dbmd2_send_cmd(p,
			     DBMD2_VA_OPR_MODE | mode,
			     NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set mode 0x%x\n",
			__func__, mode);
		p->va_flags.irq_inuse = 0;
		goto out;
	}
	p->va_flags.mode = mode;

	/* Verify that mode was set */
	if (!p->asleep) {
		unsigned short new_mode;
		ret = dbmd2_send_cmd(p, DBMD2_VA_OPR_MODE, &new_mode);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to read DBMD2_VA_OPR_MODE\n",
				__func__);
			goto out;
		}

		if (mode != new_mode)
			dev_err(p->dev, "%s: Mode verification failed: 0x%x\n",
			__func__, mode);

	}

	if (new_power_mode != p->power_mode) {
		ret = dbmd2_set_power_mode(p, new_power_mode);
		if (ret) {
			dev_err(p->dev, "%s: Failed to set power mode\n",
			__func__);
			goto out;
		}
	}

	if (mode == DBMD2_BUFFERING) {
		p->va_flags.buffering = 1;
		schedule_work(&p->sv_work);
	}

	ret = 0;
	goto out;

out:
	dev_dbg(p->dev, "%s: new mode is %x\n", __func__, p->va_flags.mode);
	return ret;
}

static void dbmd2_delayed_pm_work_hibernate(struct work_struct *work)
{
	int ret;
	struct dbmd2_private *p =
		container_of(work, struct dbmd2_private,
			     delayed_pm_work.work);

	dev_dbg(p->dev, "%s\n", __func__);

	p->lock(p);

	if (p->active_fw == DBMD2_FW_VA) {
		p->wakeup_release(p);
		ret = dbmd2_set_mode(p, DBMD2_HIBERNATE);
	} else {
		/* VQE */

		/* Activate HW TDM bypass
		 * FIXME: make it conditional
		 */
		ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HW_TDM_BYPASS_CMD |
				DBMD2_VQE_SET_HW_TDM_BYPASS_MODE_1 |
				DBMD2_VQE_SET_HW_TDM_BYPASS_FIRST_PAIR_EN,
				NULL);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to activate HW TDM bypass\n",
				__func__);
		}

		p->wakeup_release(p);

		ret = dbmd2_send_cmd(
			p, DBMD2_VQE_SET_POWER_STATE_CMD |
			DBMD2_VQE_SET_POWER_STATE_HIBERNATE, NULL);
	}

	if (ret) {
		p->wakeup_set(p);
		p->power_mode = DBMD2_PM_ACTIVE;
		dev_err(p->dev, "%s: fail to set to HIBERNATE - %d\n",
			__func__, ret);
		goto out;
	}

	msleep(20);

	p->asleep = true;
	p->power_mode = DBMD2_PM_SLEEPING;

	if (p->active_fw == DBMD2_FW_VQE)
		p->clk_disable(p, DBMD2_CLK_CONSTANT);

	p->chip->transport_enable(p, false);

out:
	dev_dbg(p->dev, "%s: current power mode: %s\n",
		__func__, dbmd2_power_mode_names[p->power_mode]);
	p->unlock(p);
}

static int dbmd2_vqe_set_use_case(struct dbmd2_private *p, unsigned int uc)
{
	int ret = 0;

	uc &= 0xffff;

	if (uc == 0) {
		/* if already sleeping we are already idle */
		if (dbmd2_sleeping(p))
			goto out;
		/* enable TDM bypass */
		dbmd2_vqe_set_tdm_bypass(p, 1);
	} else {
		if (dbmd2_sleeping(p)) {
			ret = dbmd2_wake(p);
			if (ret)
				goto out;
		}
		/* stop TDM bypass */
		dbmd2_vqe_set_tdm_bypass(p, 0);
	}

	ret = dbmd2_send_cmd(p,
			     DBMD2_VQE_SET_USE_CASE_CMD | uc,
			     NULL);
	if (ret < 0)
		dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
			__func__, uc, DBMD2_VQE_SET_USE_CASE_CMD);

out:
	return ret;
}

/* Microphone modes */
enum dbmd2_microphone_mode {
	DBMD2_MIC_MODE_DIGITAL_LEFT = 0,
	DBMD2_MIC_MODE_DIGITAL_RIGHT,
	DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT,
	DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT,
	DBMD2_MIC_MODE_ANALOG,
	DBMD2_MIC_MODE_DISABLE,
};

enum dbmd2_microphone_type {
	DBMD2_MIC_TYPE_DIGITAL_LEFT = 0,
	DBMD2_MIC_TYPE_DIGITAL_RIGHT,
	DBMD2_MIC_TYPE_ANALOG,
};

static int dbmd2_set_microphone_mode(struct dbmd2_private *p,
				     enum dbmd2_microphone_mode mode)
{
	int ret1 = 0;
	int ret2 = 0;

	dev_dbg(p->dev, "%s: mode: %d\n", __func__, mode);

	/* first disable both mics */
	ret1 = dbmd2_send_cmd(p, DBMD2_VA_MICROPHONE2_CONFIGURATION,
				      NULL);

	if (ret1 < 0) {
		dev_err(p->dev, "%s: failed to set microphone mode 0x%x\n",
			__func__, mode);
		return -1;
	}

	switch (mode) {
	case DBMD2_MIC_MODE_DISABLE:
		ret1 = dbmd2_send_cmd(p, DBMD2_VA_MICROPHONE1_CONFIGURATION,
				      NULL);
		break;
	case DBMD2_MIC_MODE_DIGITAL_LEFT:
		ret1 = dbmd2_send_cmd(p,
			DBMD2_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMD2_MIC_TYPE_DIGITAL_LEFT],
			NULL);
		break;
	case DBMD2_MIC_MODE_DIGITAL_RIGHT:
		ret1 = dbmd2_send_cmd(p,
			DBMD2_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMD2_MIC_TYPE_DIGITAL_RIGHT],
			NULL);
		break;
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
		ret1 = dbmd2_send_cmd(p,
			DBMD2_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMD2_MIC_TYPE_DIGITAL_LEFT],
			NULL);
		ret2 = dbmd2_send_cmd(p,
			DBMD2_VA_MICROPHONE2_CONFIGURATION |
			p->pdata->va_mic_config[DBMD2_MIC_TYPE_DIGITAL_RIGHT],
			NULL);
		break;
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
		ret1 = dbmd2_send_cmd(p,
			DBMD2_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMD2_MIC_TYPE_DIGITAL_RIGHT],
			NULL);
		ret2 = dbmd2_send_cmd(p,
			DBMD2_VA_MICROPHONE2_CONFIGURATION |
			p->pdata->va_mic_config[DBMD2_MIC_TYPE_DIGITAL_LEFT],
			NULL);
		break;
	case DBMD2_MIC_MODE_ANALOG:
		ret1 = dbmd2_send_cmd(p,
			DBMD2_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMD2_MIC_TYPE_ANALOG], NULL);
		break;
	default:
		dev_err(p->dev, "%s: Unsupported microphone mode 0x%x\n",
			__func__, mode);
		return -EINVAL;
	}

	if (ret1 < 0 || ret2 < 0) {
		dev_err(p->dev, "%s: failed to set microphone mode 0x%x\n",
			__func__, mode);
		return -1;
	}

	p->va_current_mic_config = mode;

	switch (mode) {
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
		p->pdata->va_audio_channels = 2;
		break;
	case DBMD2_MIC_MODE_DISABLE:
	case DBMD2_MIC_MODE_DIGITAL_LEFT:
	case DBMD2_MIC_MODE_DIGITAL_RIGHT:
	case DBMD2_MIC_MODE_ANALOG:
		p->pdata->va_audio_channels = 1;
		break;
	default:
		break;
	}

	return 0;
}

static int dbmd2_calc_amodel_checksum(struct dbmd2_private *p,
				      const char *amodel,
				      unsigned long len, unsigned long *chksum)
{
	unsigned long sum = 0;
	u16 val;
	unsigned long i;
	u32 pos = 0, chunk_len;

	*chksum = (unsigned long)0 - 1;

	while (pos < len) {
		val = *(u16 *)(&amodel[pos]);
		pos += 2;
		if (pos >= len) {
			dev_dbg(p->dev, "%s:%d %u", __func__,
				__LINE__, pos);
			return -1;
		}

		if (val == 0x025a) {
			sum += 0x5a + 0x02;

			chunk_len = *(u32 *)(&amodel[pos]);
			pos += 4;
			if (pos >= len) {
				dev_dbg(p->dev, "%s:%d %u", __func__,
					__LINE__, pos);
				return -1;
			}

			sum += chunk_len;

			sum += *(u32 *)(&amodel[pos]);
			pos += 4;

			if ((pos + (chunk_len * 2)) > len) {
				dev_dbg(p->dev, "%s:%d %u, %u",
					__func__, __LINE__, pos, chunk_len);
				return -1;
			}

			for (i = 0; i < chunk_len; i++) {
				sum += *(u16 *)(&amodel[pos]);
				pos += 2;
			}
		} else
			return -1;
	}

	sum += 0x5A + 0x0e;
	*chksum = sum;

	return 0;
}

static int dbmd2_acoustic_model_load(struct dbmd2_private *p,
				      const u8 *data, size_t size)
{
	int ret;
	int ret2;

	dev_dbg(p->dev, "%s: size:%zd\n", __func__, size);

	p->device_ready = false;

	/* flush pending sv work if any */
	p->va_flags.buffering = 0;
	flush_work(&p->sv_work);

	p->lock(p);

	/* set chip to idle mode */
	ret = dbmd2_set_mode(p, DBMD2_IDLE);
	if (ret) {
		dev_err(p->dev, "%s: failed to set device to idle mode\n",
			__func__);
		goto out_unlock;
	}

	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	p->va_flags.a_model_loaded = 0;

	/* prepare the chip interface for A-Model loading */
	ret = p->chip->prepare_amodel_loading(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: failed to prepare A-Model loading\n",
			__func__);
		goto out_unlock;
	}

	/* load A-Model and verify checksum */
	ret = p->chip->load_amodel(p,
				   data,
				   size - 4,
				   &data[size - 4],
				   4,
				   1);
	if (ret) {
		dev_err(p->dev, "%s: sending amodel failed\n", __func__);
		ret = -1;
		goto out_finish_loading;
	}

	ret = dbmd2_set_bytes_per_sample(p, p->audio_mode);
	if (ret) {
		dev_err(p->dev,
			"%s: failed to set bytes per sample\n", __func__);
		ret = -1;
		goto out_finish_loading;
	}

	ret = dbmd2_va_alive(p);
	if (ret) {
		dev_err(p->dev, "%s: fw is dead\n", __func__);
		ret = -1;
		goto out_finish_loading;
	}

	dev_info(p->dev, "%s: acoustic model sent successfully\n", __func__);
	p->va_flags.a_model_loaded = 1;

out_finish_loading:
	/* finish A-Model loading */
	ret2 = p->chip->finish_amodel_loading(p);
	if (ret2 != 0)
		dev_err(p->dev, "%s: failed to finish A-Model loading\n",
			__func__);

	if (p->pdata->auto_detection && p->va_flags.a_model_loaded) {
		dev_info(p->dev, "%s: enforcing DETECTION opmode\n",
			 __func__);
		ret = dbmd2_set_mode(p, DBMD2_DETECTION);
		if (ret) {
			dev_err(p->dev,
				"%s: failed to set device to detection mode\n",
				__func__);
			goto out_unlock;
		}
		p->chip->transport_enable(p, false);
	}

out_unlock:
	p->device_ready = true;

	p->unlock(p);

	return ret;
}


static ssize_t dbmd2_acoustic_model_build(struct dbmd2_private *p,
					const u8 *gram_data,
					size_t gram_size,
					const u8 *net_data,
					size_t net_size)
{
	unsigned char head[10] = { 0 };
	size_t pos;
	unsigned long checksum;
	int ret;

	pos = 0;

	head[0] = 0x5A;
	head[1] = 0x02;
	head[2] =  (gram_size/2)        & 0xff;
	head[3] = ((gram_size/2) >>  8) & 0xff;
	head[4] = ((gram_size/2) >> 16) & 0xff;
	head[5] = ((gram_size/2) >> 24) & 0xff;
	head[6] = 0x01;
	head[7] = 0x00;
	memcpy(p->amodel_buf, head, 10);

	pos += 10;

	if (pos + gram_size > MAX_AMODEL_SIZE) {
		dev_err(p->dev,
			"%s: adding gram exceeds max size %zd>%d\n",
			__func__, pos + gram_size + 6, MAX_AMODEL_SIZE);
		ret = -1;
		goto out;
	}

	memcpy(p->amodel_buf + pos, gram_data, gram_size);

	pos += gram_size;

	head[0] = 0x5A;
	head[1] = 0x02;
	head[2] =  (net_size/2)        & 0xff;
	head[3] = ((net_size/2) >>  8) & 0xff;
	head[4] = ((net_size/2) >> 16) & 0xff;
	head[5] = ((net_size/2) >> 24) & 0xff;
	head[6] = 0x02;
	head[7] = 0x00;
	memcpy(p->amodel_buf + pos, head, 10);

	pos += 10;

	if (pos + net_size + 6 > MAX_AMODEL_SIZE) {
		dev_err(p->dev,
			"%s: adding net exceeds max size %zd>%d\n",
			__func__, pos + net_size + 6, MAX_AMODEL_SIZE);
		ret = -1;
		goto out;
	}

	memcpy(p->amodel_buf + pos, net_data, net_size);

	ret = dbmd2_calc_amodel_checksum(p,
					(char *)p->amodel_buf,
					pos + net_size,
					&checksum);
	if (ret) {
		dev_err(p->dev, "%s: failed to calculate Amodel checksum\n",
			__func__);
		ret = -1;
		goto out;
	}

	*(unsigned long *)(p->amodel_buf + pos + net_size) = checksum;

	ret = p->va_flags.amodel_len = pos + net_size + 4;

out:
	return ret;
}

static void dbmd2_get_firmware_version(const char *data, size_t size,
				       char *buf, size_t buf_size)
{
	int i, j;

	buf[0] = 0;
	i = size - 58;
	if ((data[i]   == 0x10) && (data[i+1]  == 0x32) &&
			(data[i+2] == 0x1a) && (data[i+3]  == 0xd2)) {
		/* VQE FW */
		buf += sprintf(buf,
				"Product %X%X%X%X Ver V%X.%X.%X%X%X%X.%X%X",
				/* PRODUCT */
				(int)(data[i+1]), (int)(data[i]),
				(int)(data[i+3]), (int)(data[i+2]),
				/* VERSION */
				(int)(data[i+5]), (int)(data[i+4]),
				(int)(data[i+7]), (int)(data[i+6]),
				(int)(data[i+9]), (int)(data[i+8]),
				(int)(data[i+11]), (int)(data[i+10]));

		sprintf(buf,
				"Compiled at %c%c%c%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c",
				/* DATE */
				(int)(data[i+12]), (int)(data[i+14]),
				(int)(data[i+16]), (int)(data[i+18]),
				(int)(data[i+20]), (int)(data[i+22]),
				(int)(data[i+24]), (int)(data[i+26]),
				(int)(data[i+28]), (int)(data[i+30]),
				(int)(data[i+32]),
				/* TIME */
				(int)(data[i+36]), (int)(data[i+38]),
				(int)(data[i+40]), (int) (data[i+42]),
				(int)(data[i+44]), (int)(data[i+46]),
				(int)(data[i+48]), (int)(data[i+50]));

		return;
	} else {
		/* VA FW */
		for (i = size - 13; i > 0; i--) {
			if ((data[i]   == 'v') && (data[i+2]  == 'e') &&
			    (data[i+4] == 'r') && (data[i+6]  == 's') &&
			    (data[i+8] == 'i') && (data[i+10] == 'o')) {
				for (j = 0; i + j < size; j++) {
					if (j == buf_size - 1)
						break;
					buf[j] = data[i];
					i += 2;
					if (((buf[j] > 0) && (buf[j] < 32))
					    || (buf[j] > 126))
						return;
					if (buf[j] == 0)
						buf[j] = ' ';
				}
				buf[j] = 0;
				return;
			}
		}
	}
}

static int dbmd2_firmware_ready(const struct firmware *fw,
				struct dbmd2_private *p)
{
	const u8 *fw_file_checksum;
	char fw_version[200];
	int ret;

	if (!fw) {
		dev_err(p->dev, "%s: firmware request failed\n", __func__);
		return -EIO;
	}

	if (fw->size <= 4) {
		dev_err(p->dev, "%s: firmware size (%zu) invalid\n",
			__func__, fw->size);
		goto out_err;
	}

	fw_file_checksum = &fw->data[fw->size - 4];

	/*
	 *  read firmware version from file, not sure if this is the same
	 *  for VA and VQE firmware
	 */
	memset(fw_version, 0, 200);
	dbmd2_get_firmware_version(fw->data, fw->size, fw_version, 200);
	if (strlen(fw_version) > 15)
		dev_info(p->dev, "%s: firmware: %s\n", __func__, fw_version);

	/* check if the chip interface is ready to boot */
	ret = p->chip->can_boot(p);
	if (ret)
		goto out_err;

	/* prepare boot if required */
	ret = p->chip->prepare_boot(p);
	if (ret)
		goto out_err;

	/* enable high speed clock for boot */
	p->clk_enable(p, DBMD2_CLK_MASTER);

	/* boot */
	ret = p->chip->boot(fw, p, fw_file_checksum, 4, 1);
	if (ret)
		goto out_disable_hs_clk;

	/* disable high speed clock after boot */
	dbmd2_clk_disable(p, DBMD2_CLK_MASTER);

	/* finish boot if required */
	ret = p->chip->finish_boot(p);
	if (ret)
		goto out_err;

	ret = 0;
	goto out;

out_disable_hs_clk:
	dbmd2_clk_disable(p, DBMD2_CLK_MASTER);
out_err:
	dev_err(p->dev, "%s: firmware request failed\n", __func__);
	ret = -EIO;
out:
	return ret;
}

static int dbmd2_config_va_mode(struct dbmd2_private *p)
{
	unsigned int i;
	int ret;
	u16 fwver = 0xffff;

	dev_dbg(p->dev, "%s\n", __func__);

	for (i = 0; i < p->pdata->va_cfg_values; i++)
		dbmd2_send_cmd(p, p->pdata->va_cfg_value[i], NULL);

	/* set sample size */
	ret = dbmd2_set_bytes_per_sample(p, p->audio_mode);
	if (ret)
		goto out;

	/* Set Backlog */
	ret = dbd2_set_backlog_len(p, p->va_backlog_length);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: could not set backlog history configuration\n",
			__func__);
		goto out;
	}

	ret = dbmd2_set_microphone_mode(p, p->va_current_mic_config);
	if (ret < 0) {
		dev_err(p->dev, "%s: couldn't set mic configuration\n",
			__func__);
		goto out;
	}

	/* read firmware version */
	ret = dbmd2_send_cmd(p, DBMD2_VA_GET_FW_VER, &fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}

	dev_info(p->dev, "%s: VA firmware 0x%x ready\n", __func__, fwver);

	ret = dbmd2_set_mode(dbd2_data, DBMD2_IDLE);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not set to idle\n", __func__);
		goto out;
	}

	if (p->va_detection_mode != DETECTION_MODE_VOICE_ENERGY)
		p->va_detection_mode = DETECTION_MODE_PHRASE;

	ret = 0;
out:
	return ret;
}

static int dbmd2_va_firmware_ready(struct dbmd2_private *p)
{
	int ret;

	dev_dbg(p->dev, "%s\n", __func__);

	/* common boot */
	ret = dbmd2_firmware_ready(p->va_fw, p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not load VA firmware\n", __func__);
		return -EIO;
	}

	dbmd2_set_va_active(p);

	ret = p->chip->set_va_firmware_ready(p);
	if (ret) {
		dev_err(p->dev, "%s: could not set to ready VA firmware\n",
			__func__);
		return -EIO;
	}

	ret = dbmd2_config_va_mode(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not configure VA firmware\n",
			__func__);
		return -EIO;
	}

	return 0;
}

static int dbmd2_vqe_get_ver(struct dbmd2_private *p)
{
	int ret;
	u16 fwver = 0xffff;

	/* read firmware version */
	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HOST_STATUS_CMD |
				DBMD2_VQE_HOST_STATUS_CMD_PRODUCT_MAJOR_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware product major: 0x%x\n",
		__func__, fwver);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HOST_STATUS_CMD |
				DBMD2_VQE_HOST_STATUS_CMD_PRODUCT_MINOR_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware product minor: 0x%x\n",
		__func__, fwver);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HOST_STATUS_CMD |
				DBMD2_VQE_HOST_STATUS_CMD_FW_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware version: 0x%x\n", __func__, fwver);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HOST_STATUS_CMD |
				DBMD2_VQE_HOST_STATUS_CMD_PATCH_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware patch version: 0x%x\n",
		__func__, fwver);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HOST_STATUS_CMD |
				DBMD2_VQE_HOST_STATUS_CMD_DEBUG_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware debug version: 0x%x\n",
		__func__, fwver);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HOST_STATUS_CMD |
				DBMD2_VQE_HOST_STATUS_CMD_TUNING_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware tunning version: 0x%x\n",
		__func__, fwver);
	dev_info(p->dev, "%s: VQE firmware ready\n", __func__);

out:
	return ret;
}


static int dbmd2_config_vqe_mode(struct dbmd2_private *p)
{
	unsigned int i;
	int ret;

	dev_dbg(p->dev, "%s\n", __func__);

	ret = dbmd2_vqe_alive(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: VQE firmware not ready\n", __func__);
		goto out;
	}

	for (i = 0; i < p->pdata->vqe_cfg_values; i++)
		(void)dbmd2_send_cmd(p, p->pdata->vqe_cfg_value[i], NULL);


	ret = 0;

out:
	return ret;
}

static int dbmd2_vqe_firmware_ready(struct dbmd2_private *p,
				    int vqe, int load_non_overlay)
{
	int ret;

	dev_dbg(p->dev, "%s: non-overlay: %d\n", __func__, load_non_overlay);

#if 0
	/* check if non-overlay firmware is available */
	if (p->vqe_non_overlay_fw && load_non_overlay) {
		ssize_t send;

		/* VA firmware must be active for this */
		if (p->active_fw != DBMD2_FW_VA) {
			dev_err(p->dev,
				"%s: VA firmware must be active for non-overlay loading\n",
				__func__);
			return -EIO;
		}
		/* change to high speed */
		ret = dbmd2_va_set_high_speed(p);
		if (ret != 0) {
			dev_err(p->dev,
				"%s: could not change to high speed\n",
				__func__);
			return -EIO;
		}

		msleep(300);

		/* restore AHB memory */
		ret = dbmd2_send_cmd(p,
				     DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL | 2,
				     NULL);
		if (ret != 0) {
			dev_err(p->dev,
				"%s: could not prepare non-overlay loading\n",
				__func__);
			return -EIO;
		}
		usleep_range(10000, 11000);

		/* check if firmware is still alive */
		ret = dbmd2_va_alive(p);
		if (ret)
			return ret;

		/* reset the chip */
		p->reset_sequence(p);

		msleep(300);

		/* send non-overlay part */
		send = dbmd2_send_data(p,
				      p->vqe_non_overlay_fw->data,
				      p->vqe_non_overlay_fw->size);
		if (send != p->vqe_non_overlay_fw->size) {
			dev_err(p->dev,
				"%s: failed to send non-overlay VQE firmware: %zu\n",
				__func__,
				send);
			return -EIO;
		}
		usleep_range(10000, 11000);
	}
#endif
	if (!vqe)
		return 0;

	/*
	 * common boot
	 */
	ret = dbmd2_firmware_ready(p->vqe_fw, p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not load VQE firmware\n",
			__func__);
		return -EIO;
	}
	dbmd2_set_vqe_active(p);

	ret = p->chip->set_vqe_firmware_ready(p);
	if (ret) {
		dev_err(p->dev, "%s: could not set to ready VQE firmware\n",
			__func__);
		return -EIO;
	}

	/* special setups for the VQE firmware */
	ret = dbmd2_config_vqe_mode(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not configure VQE firmware\n",
			__func__);
		return -EIO;
	}

	return 0;
}

static int dbmd2_switch_to_va_firmware(struct dbmd2_private *p, bool do_reset)
{
	int ret;

	if (p->active_fw == DBMD2_FW_VA)
		return 0;

	if (!p->pdata->feature_va) {
		dev_err(p->dev, "%s: VA feature not enabled\n", __func__);
		return -EINVAL;
	}

	if (do_reset)
		dbmd2_set_boot_active(p);

	dbmd2_set_power_mode(p, DBMD2_PM_BOOTING);

	dev_dbg(p->dev, "%s: switching to VA firmware\n", __func__);

	p->device_ready = false;

	ret = dbmd2_va_firmware_ready(p);
	if (ret)
		return ret;

	p->device_ready = true;
	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	return 0;
}

static int dbmd2_switch_to_vqe_firmware(struct dbmd2_private *p, bool do_reset)
{
	int ret;

	if (p->active_fw == DBMD2_FW_VQE)
		return 0;

	if (!p->pdata->feature_vqe) {
		dev_err(p->dev, "%s: VQE feature not enabled\n", __func__);
		return -EINVAL;
	}
	if (do_reset)
		dbmd2_set_boot_active(p);

	dbmd2_set_power_mode(p, DBMD2_PM_BOOTING);

	dev_dbg(p->dev, "%s: switching to VQE firmware\n", __func__);

	p->device_ready = false;

	ret = dbmd2_vqe_firmware_ready(p, 1, 0);
	if (ret)
		return ret;

	p->device_ready = true;
	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	return 0;
}

static int dbmd2_request_and_load_fw(struct dbmd2_private *p,
				     int va, int vqe, int vqe_non_overlay)
{
	int ret;
	int retries = 5;

	dev_dbg(p->dev, "%s %s/%s\n",
		__func__, va ? "VA" : "-", vqe ? "VQE" : "-");

	p->lock(p);

	dbmd2_set_boot_active(p);

	if (va && p->va_fw) {
		release_firmware(p->va_fw);
		p->va_fw = NULL;
	}

	if (vqe && p->vqe_fw) {
		release_firmware(p->vqe_fw);
		p->vqe_fw = NULL;
	}

	if (p->vqe_non_overlay_fw && vqe_non_overlay) {
		release_firmware(p->vqe_non_overlay_fw);
		p->vqe_non_overlay_fw = NULL;
	}

	ret = dbmd2_set_power_mode(p, DBMD2_PM_BOOTING);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not change to DBMD2_PM_BOOTING\n",
			__func__);
		goto out;
	}

	if (p->pdata->feature_va && va) {
		/* request VA firmware */
		do {
			dev_info(p->dev, "%s: request VA firmware - %s\n",
				 __func__, p->pdata->va_firmware_name);
			ret =
			request_firmware((const struct firmware **)&p->va_fw,
					       p->pdata->va_firmware_name,
					       p->dev);
			if (ret != 0) {
				dev_err(p->dev,
					"%s: failed to request VA firmware\n",
					__func__);
				msleep(200);
				continue;
			}
			break;
		} while (--retries);

		if (retries == 0) {
			dev_err(p->dev, "%s: failed to request VA firmware\n",
				__func__);
			ret = -EIO;
			goto out;
		}

	}

	if (p->pdata->feature_vqe && vqe) {
		/* request VQE firmware */
		do {

			dev_info(p->dev, "%s: request VQE firmware - %s\n",
				 __func__, p->pdata->vqe_firmware_name);
			ret =
			request_firmware((const struct firmware **)&p->vqe_fw,
					       p->pdata->vqe_firmware_name,
					       p->dev);
			if (ret != 0) {
				dev_err(p->dev,
					"%s: failed to request VQE firmware\n",
					__func__);
				msleep(200);
				continue;
			}
			break;
		} while (--retries);

		if (retries == 0) {
			dev_err(p->dev, "%s: failed to request VQE firmware\n",
				__func__);
			ret = -EIO;
			goto out;
		}

		if (p->pdata->feature_fw_overlay) {
			dev_info(p->dev,
				"%s: request VQE non-overlay firmware - %s\n",
				__func__,
				p->pdata->vqe_non_overlay_firmware_name);
			ret = request_firmware(
			       (const struct firmware **)&p->vqe_non_overlay_fw,
			       p->pdata->vqe_non_overlay_firmware_name,
			       p->dev);
			if (ret != 0) {
				dev_err(p->dev,
					"%s: failed to request VQE non-overlay firmware\n",
					__func__);
				ret = -EIO;
				goto out_err;
			}
		}
	}

	if (p->pdata->feature_vqe && (vqe || vqe_non_overlay)) {
		ret = dbmd2_switch_to_vqe_firmware(p, 1);
		if (ret != 0) {
			dev_err(p->dev, "%s: failed to boot VQE firmware\n",
				__func__);
			ret = -EIO;
			goto out_err;
		}
		dbmd2_vqe_get_ver(p);
	} else if (p->pdata->feature_va && va) {
		ret = dbmd2_switch_to_va_firmware(p, 1);
		if (ret != 0) {
			dev_err(p->dev, "%s: failed to boot VA firmware\n",
				__func__);
			ret = -EIO;
			goto out_err;
		}
	}

	/* fall asleep by default after boot */
	ret = dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);
	if (ret != 0) {
		dev_err(p->dev,
			"%s: could not change to DBMD2_PM_FALLING_ASLEEP\n",
			__func__);
		goto out_err;
	}

	p->device_ready = true;
	ret = 0;
	goto out;

out_err:
	if (p->vqe_fw) {
		release_firmware(p->vqe_fw);
		p->vqe_fw = NULL;
	}
	if (p->vqe_non_overlay_fw) {
		release_firmware(p->vqe_non_overlay_fw);
		p->vqe_non_overlay_fw = NULL;
	}
	if (p->va_fw) {
		release_firmware(p->va_fw);
		p->va_fw = NULL;
	}
out:
	p->unlock(p);
	return ret;
}

/* ------------------------------------------------------------------------
 * sysfs attributes
 * ------------------------------------------------------------------------ */
static int dbd2_d2param_get(struct dbmd2_private *p, u32 addr)
{
	int ret = 0;
	u16 val = 0xffff;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);
	ret = dbmd2_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		goto out;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VA_SET_D2PARAM_ADDR | addr, NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set d2paramaddr: %d\n",
				__func__, ret);
		goto out;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VA_GET_D2PARAM, &val);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to read d2param: %d\n",
				__func__, ret);
		goto out;
	}

	ret = (int)val;

out:
	p->unlock(p);
	return ret;
}

static ssize_t dbmd2_reg_show(struct device *dev, u32 command,
			      struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;
	u16 val = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s: get reg %x\n", __func__, command);

	if ((p->active_fw == DBMD2_FW_VQE) && (command & DBMD2_VA_CMD_MASK)) {
		dev_err(p->dev, "%s: VA mode is not active\n", __func__);
		return -ENODEV;
	}

	if ((p->active_fw == DBMD2_FW_VA) && !(command & DBMD2_VA_CMD_MASK)) {
		dev_err(p->dev, "%s: VQE mode is not active\n", __func__);
		return -ENODEV;
	}

	p->lock(p);

	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	ret = dbmd2_send_cmd(p, command, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %x error\n",
				__func__, command);
		goto out_unlock;
	}

	if (command == DBMD2_VA_AUDIO_HISTORY)
		val = val & 0x0fff;

	ret = sprintf(buf, "0x%x\n", val);

out_unlock:
	dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);

	p->unlock(p);
	return ret;
}

static ssize_t dbmd2_reg_show_long(struct device *dev,
				   u32 command, u32 command1,
				   struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;
	u16 val = 0;
	u32 result;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	ret = dbmd2_send_cmd(p, command1, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %u error\n",
				__func__, command);
		goto out_unlock;
	}

	result = (u32)(val & 0xffff);
	dev_dbg(p->dev, "%s: val = 0x%08x\n", __func__, result);

	val = 0;
	ret = dbmd2_send_cmd(p, command, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %u error\n",
				__func__, command1);
		goto out_unlock;
	}

	result += ((u32)val << 16);
	dev_dbg(p->dev, "%s: val = 0x%08x\n", __func__, result);

	ret = sprintf(buf, "0x%x\n", result);

out_unlock:

	dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);

	p->unlock(p);
	return ret;
}

static ssize_t dbmd2_reg_store(struct device *dev, u32 command,
			       struct device_attribute *attr,
			       const char *buf, size_t size, int fw)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	p->lock(p);

	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	if (fw != p->active_fw) {
		if (fw == DBMD2_FW_VA)
			ret = dbmd2_switch_to_va_firmware(p, 0);
		if (fw == DBMD2_FW_VQE)
			ret = dbmd2_switch_to_vqe_firmware(p, 0);
		if (ret)
			goto out_unlock;
	}

	if (p->active_fw == DBMD2_FW_VA) {
		if (command == DBMD2_VA_OPR_MODE) {
			ret = dbmd2_set_mode(p, val);
			if (ret)
				size = ret;
			goto out_unlock;
		}

		if (command == DBMD2_VA_AUDIO_BUFFER_CONVERSION) {
			u16 mode = val & 0x1000;
			ret = dbmd2_set_bytes_per_sample(p, mode);
			if (ret) {
				size = ret;
				goto out_pm_mode;
			}
			command = DBMD2_VA_AUDIO_HISTORY;
		}

		if (command == DBMD2_VA_AUDIO_HISTORY) {
			ret = dbd2_set_backlog_len(p, val);
			if (ret < 0) {
				dev_err(p->dev, "%s: set history error\n",
					__func__);
				size = ret;
				goto out_pm_mode;
			}
		}

		ret = dbmd2_send_cmd(p, command | (u32)val, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: set VA reg error\n", __func__);
			size = ret;
			goto out_pm_mode;
		}
	}

	if (p->active_fw == DBMD2_FW_VQE) {
		ret = dbmd2_send_cmd(p, command | (u32)val, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: set VQE reg error\n", __func__);
			size = ret;
			goto out_pm_mode;
		}
	}

out_pm_mode:
	dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);

out_unlock:
	p->unlock(p);
	return size;
}

static ssize_t dbmd2_reg_store_long(struct device *dev, u32 command,
				    u32 command1,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;
	dev_err(p->dev, "%s: val = %u\n", __func__, (int)val);

	p->lock(p);

	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);
	ret = dbmd2_send_cmd(p, command1 | (val & 0xffff), NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: set reg error\n", __func__);
		size = ret;
		goto out_unlock;
	}

	ret = dbmd2_send_cmd(p, command | (val >> 16), NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: set reg error\n", __func__);
		size = ret;
		goto out_unlock;
	}

out_unlock:

	dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);

	p->unlock(p);
	return size;
}

static ssize_t dbmd2_fw_ver_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	/* FIXME: add support for VQE */
	return dbmd2_reg_show(dev, DBMD2_VA_GET_FW_VER, attr, buf);
}

static ssize_t dbmd2_va_opmode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_OPR_MODE, attr, buf);
}

static ssize_t dbmd2_opr_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_OPR_MODE, attr,
			       buf, size, DBMD2_FW_VA);
}

static ssize_t dbmd2_va_uart_delay_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_UART_DELAY, attr, buf);
}

static ssize_t dbmd2_va_uart_delay_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_UART_DELAY, attr,
			       buf, size, DBMD2_FW_VA);
}

static ssize_t dbmd2_va_uart_fifo_size_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_UART_FIFO_SIZE, attr, buf);
}

static ssize_t dbmd2_va_uart_fifo_size_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_UART_FIFO_SIZE, attr,
			       buf, size, DBMD2_FW_VA);
}

static ssize_t dbmd2_reboot_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int va = 0;
	int vqe = 0;
	int non_overlay = 0;
	int ret = 0;

	if (size == 3)
		va = strncmp("va", buf, 2) == 0;
	if (size == 4)
		vqe = strncmp("vqe", buf, 3) == 0;

	if (!va && !vqe) {
		dev_warn(p->dev, "%s: not valid mode requested: %s\n",
			__func__, buf);
		return size;
	}

	if (va && !p->pdata->feature_va) {
		dev_dbg(p->dev, "%s: VA feature not enabled\n", __func__);
		va = 0;
	}

	if (vqe && !p->pdata->feature_vqe) {
		dev_dbg(p->dev, "%s: VQE feature not enabled\n", __func__);
		vqe = 0;
	}

	/*
	 * if VQE needs to be loaded and not VA but both features are enabled
	 * the VA firmware needs to be loaded first in order to load the non
	 * overlay part
	 */
	if (!va && vqe &&
	    (p->pdata->feature_va && p->pdata->feature_vqe &&
	    p->pdata->feature_fw_overlay)) {
		va = 1;
		non_overlay = 1;
	}

	if (va && !vqe &&
	    (p->pdata->feature_va && p->pdata->feature_vqe &&
	    p->pdata->feature_fw_overlay))
		non_overlay = 1;

	ret = dbmd2_request_and_load_fw(p, va, vqe, non_overlay);
	if (ret != 0)
		return -EIO;

	return size;
}

static ssize_t dbmd2_va_trigger_level_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_TG_THERSHOLD, attr, buf);
}

static ssize_t dbmd2_va_trigger_level_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_TG_THERSHOLD, attr,
			       buf, size, DBMD2_FW_VA);
}

static ssize_t dbmd2_va_verification_level_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_VERIFICATION_THRESHOLD, attr, buf);
}

static ssize_t dbmd2_va_verification_level_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_VERIFICATION_THRESHOLD, attr, buf,
			      size, DBMD2_FW_VA);
}

static ssize_t dbmd2_va_gain_shift_factor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_GAIN_SHIFT_VALUE, attr, buf);
}

static ssize_t dbmd2_va_gain_shift_factor_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_GAIN_SHIFT_VALUE, attr,
			       buf, size, DBMD2_FW_VA);
}

static ssize_t dbmd2_io_addr_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show_long(dev, DBMD2_VA_IO_PORT_ADDR_HI,
				  DBMD2_VA_IO_PORT_ADDR_LO, attr, buf);
}

static ssize_t dbmd2_io_addr_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	return dbmd2_reg_store_long(dev, DBMD2_VA_IO_PORT_ADDR_HI,
				   DBMD2_VA_IO_PORT_ADDR_LO, attr, buf, size);
}

static int dbmd2_load_new_acoustic_model_sync(struct dbmd2_private *p)
{
	int ret;
	struct firmware	*va_gram_fw = NULL;
	struct firmware	*va_net_fw = NULL;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (p->active_fw == DBMD2_FW_VQE) {

		if (p->vqe_flags.in_call) {
			dev_err(p->dev,
				"%s: Switching to VA is not allowed when in CALL\n",
				__func__);
			return -EIO;
		}

		dev_info(p->dev, "%s: VA firmware not active, switching\n",
			__func__);

		p->lock(p);
		ret = dbmd2_switch_to_va_firmware(p, 0);
		p->unlock(p);
		if (ret != 0) {
			dev_err(p->dev, "%s: Error switching to VA firmware\n",
				__func__);
			return -EIO;
		}
	}

	ret = request_firmware((const struct firmware **)&va_gram_fw,
				DBD2_GRAM_NAME,
				p->gram_dev);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to request VA gram firmware(%d)\n",
			__func__, ret);
		return ret;
	}

	dev_info(p->dev, "%s: gram firmware requested\n", __func__);

	dev_dbg(p->dev, "%s gram=%zu bytes\n", __func__, va_gram_fw->size);

	if (!va_gram_fw->size) {
		dev_warn(p->dev, "%s gram size is 0. Ignore...\n", __func__);
		ret = -EIO;
		goto release;
	}

	ret = request_firmware((const struct firmware **)&va_net_fw,
				DBD2_NET_NAME,
				p->gram_dev);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to request VA net firmware(%d)\n",
			__func__, ret);
		ret = -EIO;
		goto release;
	}

	dev_info(p->dev, "%s: net firmware requested\n", __func__);

	dev_dbg(p->dev, "%s net=%zu bytes\n", __func__, va_net_fw->size);

	if (!va_net_fw->size) {
		dev_warn(p->dev, "%s net size is 0. Ignore...\n", __func__);
		ret = -EIO;
		goto release;
	}

	p->lock(p);

	p->va_flags.amodel_len = 0;

	ret = dbmd2_acoustic_model_build(p,
					va_gram_fw->data,
					va_gram_fw->size,
					va_net_fw->data,
					va_net_fw->size);

	p->unlock(p);

	if (ret <= 0) {
		dev_err(p->dev,	"%s: amodel build failed: %d\n", __func__, ret);
		ret = -EIO;
		goto release;
	}

	if (p->va_flags.amodel_len > 0) {
		ret = dbmd2_acoustic_model_load(
				p, p->amodel_buf, p->va_flags.amodel_len);
		if (ret < 0) {
			dev_err(p->dev,	"%s: Failed to load amodel: %d\n",
				__func__, ret);
			ret = -EIO;
			goto release;
		}

	}

release:
	if (va_gram_fw)
		release_firmware(va_gram_fw);
	if (va_net_fw)
		release_firmware(va_net_fw);

	return ret;
}

static ssize_t dbmd2_va_acoustic_model_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;
	int val = dbmd2_buf_to_int(buf);

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (val == 0) {
		/* 0 means load model */
		ret = dbmd2_load_new_acoustic_model_sync(p);
	} else if (val == 1) {
		/* 1 means send 1 to register 0xF */
		p->lock(p);
		ret = dbmd2_send_cmd(p, DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL |
				    (u32)val, NULL);
		p->unlock(p);
		if (ret < 0)
			dev_err(p->dev,
				"%s: failed to set DBMD2_VA_LOAD_NEW_ACUSTIC_MODEL to %d\n",
				__func__, val);
		else
			ret = 0;
	} else {
		/* don't know what to do */
		ret = 0;
	}
	return (ret < 0 ? ret : size);
}

static ssize_t dbmd2_va_max_sample_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_LAST_MAX_SMP_VALUE, attr, buf);
}

static ssize_t dbmd2_io_value_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show_long(dev, DBMD2_VA_IO_PORT_VALUE_HI,
				  DBMD2_VA_IO_PORT_VALUE_LO, attr, buf);
}

static ssize_t dbmd2_io_value_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return dbmd2_reg_store_long(dev, DBMD2_VA_IO_PORT_VALUE_HI,
				   DBMD2_VA_IO_PORT_VALUE_LO, attr, buf, size);
}

static ssize_t dbmd2_va_buffer_size_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_AUDIO_BUFFER_SIZE, attr, buf);
}

static ssize_t dbmd2_va_buffer_size_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	return dbmd2_reg_store(dev,
			       DBMD2_VA_AUDIO_BUFFER_SIZE,
			       attr,
			       buf,
			       size,
			       DBMD2_FW_VA);
}

static ssize_t dbmd2_va_buffsmps_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_NUM_OF_SMP_IN_BUF, attr, buf);
}

static ssize_t dbmd2_va_capture_on_detect_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = sprintf(buf, "%d\n", p->va_capture_on_detect);

	return ret;
}

static ssize_t dbmd2_va_capture_on_detect_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (!p)
		return -EAGAIN;

	if (2 < val) {
		dev_err(p->dev, "%s: invalid captute on detection mode\n",
			__func__);
		return -EINVAL;
	}

	p->va_capture_on_detect = (bool)val;

	return size;
}

static ssize_t dbmd2_va_audio_conv_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_AUDIO_BUFFER_CONVERSION, attr, buf);
}

static ssize_t dbmd2_va_audio_conv_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_AUDIO_BUFFER_CONVERSION, attr, buf,
			      size, DBMD2_FW_VA);
}

static ssize_t dbmd2_va_micgain_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_MIC_GAIN, attr, buf);
}

static ssize_t dbmd2_va_micgain_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_MIC_GAIN, attr,
			       buf, size, DBMD2_FW_VA);
}

static ssize_t dbd2_va_backlog_size_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VA_AUDIO_HISTORY, attr, buf);
}

static ssize_t dbd2_va_backlog_size_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VA_AUDIO_HISTORY, attr, buf,
			      size, DBMD2_FW_VA);
}

static ssize_t dbmd2_reset_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	ret = dbmd2_perform_recovery(p);
	if (ret) {
		dev_err(p->dev, "%s: recovery failed\n", __func__);
		return -EIO;
	}

	return size;
}

static ssize_t dbmd2_d2param_addr_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	p->lock(p);
	ret = dbmd2_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		p->unlock(p);
		return ret;
	}

	if (p->active_fw == DBMD2_FW_VQE)
		ret = dbmd2_send_cmd(p,
			DBMD2_VQE_SET_INDIRECT_REG_ADDR_ACCESS_CMD | (u32)val,
			NULL);
	else
		ret = dbmd2_send_cmd(p,
				     DBMD2_VA_SET_D2PARAM_ADDR | (u32)val,
				     NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: set d2paramaddr error\n", __func__);
		p->unlock(p);
		return ret;
	}

	p->unlock(p);
	return size;
}

static ssize_t dbmd2_d2param_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;
	u16 val;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);
	ret = dbmd2_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		p->unlock(p);
		return ret;
	}

	if (p->active_fw == DBMD2_FW_VQE)
		ret = dbmd2_send_cmd(p,
			DBMD2_VQE_GET_INDIRECT_REG_DATA_ACCESS_CMD,
			&val);
	else
		ret = dbmd2_send_cmd(p, DBMD2_VA_GET_D2PARAM, &val);

	if (ret < 0) {
		dev_err(p->dev, "%s: get d2param error\n", __func__);
		p->unlock(p);
		return ret;
	}

	p->unlock(p);
	return sprintf(buf, "%u\n", val);
}

static ssize_t dbmd2_d2param_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	p->lock(p);
	ret = dbmd2_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		p->unlock(p);
		return ret;
	}

	if (p->active_fw == DBMD2_FW_VQE)
		ret = dbmd2_send_cmd(p,
			DBMD2_VQE_SET_INDIRECT_REG_DATA_ACCESS_CMD | (u32)val,
			NULL);
	else
		ret = dbmd2_send_cmd(p, DBMD2_VA_SET_D2PARAM | (u32)val, NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: set d2param error\n", __func__);
		p->unlock(p);
		return ret;
	}

	p->unlock(p);
	return size;
}

static ssize_t dbmd2_dump_state(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int off = 0;

	if (!p)
		return -EAGAIN;

	off += sprintf(buf + off, "\tDBMD2 Driver Ver:\t%s\n",
		DRIVER_VERSION);
	off += sprintf(buf + off, "\tActive firmware:\t%s\n",
		p->active_fw == DBMD2_FW_VQE ? "VQE" :
		p->active_fw == DBMD2_FW_VA ? "VA" : "PRE_BOOT");
	off += sprintf(buf + off, "\tPower mode:\t\t%s\n",
				dbmd2_power_mode_names[p->power_mode]);
	off += sprintf(buf + off, "\t=======VA Dump==========\n");
	off += sprintf(buf + off, "\t-------VA Settings------\n");
	off += sprintf(buf + off, "\tVA Backlog_length\t%d\n",
				p->va_backlog_length);
	off += sprintf(buf + off, "\tVA Microphone conf:\t%d\n",
				p->va_current_mic_config);
	off += sprintf(buf + off, "\tVA Detection mode:\t%s\n",
			p->va_detection_mode == DETECTION_MODE_PHRASE ?
				"PASSPHRASE" : "VOICE");
	off += sprintf(buf + off, "\tVA Capture On Detect:\t%s\n",
			p->va_capture_on_detect ? "ON" : "OFF");
	off += sprintf(buf + off, "\tSample size:\t\t%d\n",
				p->audio_mode);
	off += sprintf(buf + off, "\t-------VA Status------\n");
	off += sprintf(buf + off, "\tVA Device Ready:\t%s\n",
				p->device_ready ? "Yes" : "No");
	off += sprintf(buf + off, "\tVA Operational mode:\t%s\n",
				dbmd2_state_names[p->va_flags.mode]);
	off += sprintf(buf + off, "\tAcoustic model:\t\t%s\n",
				p->va_flags.a_model_loaded == 1 ?
					"Loaded" : "None");
	off += sprintf(buf + off, "\tAcoustic model size\t%d bytes\n",
				p->va_flags.amodel_len);
	off += sprintf(buf + off, "\tBuffering:\t\t%s\n",
			p->va_flags.buffering ? "ON" : "OFF");

	off += sprintf(buf + off, "\t=======VQE Dump==========\n");
	off += sprintf(buf + off, "\tIn Call:\t\t%s\n",
			p->vqe_flags.in_call ? "Yes" : "No");
	off += sprintf(buf + off, "\tVQE Use Case:\t\t%d\n",
		p->vqe_flags.use_case);
	off += sprintf(buf + off, "\tVQE Speaker Vol Lvl:\t%d\n",
		p->vqe_flags.speaker_volume_level);
	off += sprintf(buf + off, "\tVQE VC syscfg:\t\t%d\n",
		p->vqe_vc_syscfg);

	if (p->chip->dump)
		off += p->chip->dump(p, buf + off);

	return off;
}

static ssize_t dbmd2_va_rxsize_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", p->rxsize);
}

static ssize_t dbmd2_va_rxsize_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct dbmd2_private *p = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val % 16 != 0)
		return -EINVAL;

	p->rxsize = val;

	return size;
}

static ssize_t dbmd2_va_rsize_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", p->rsize);
}

static ssize_t dbmd2_va_rsize_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct dbmd2_private *p = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val == 0 || (val % 2 != 0) || val > MAX_READ_SZ)
		return -EINVAL;

	p->rsize = val;

	return size;
}

static ssize_t dbmd2_va_wsize_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", p->wsize);
}

static ssize_t dbmd2_va_wsize_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct dbmd2_private *p = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val == 0 || (val % 2 != 0) || val > MAX_WRITE_SZ)
		return -EINVAL;

	p->wsize = val;

	return size;
}

static ssize_t dbmd2_power_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);

	return sprintf(buf, "%s: %s (%d)\n",
		       p->active_fw == DBMD2_FW_VA ? "VA" : "VQE",
		       dbmd2_power_mode_names[p->power_mode],
		       p->power_mode);
}

static ssize_t dbmd2_vqe_ping_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s\n", __func__);

	if (p->active_fw != DBMD2_FW_VQE)
		return sprintf(buf, "VQE firmware not loaded\n");

	p->lock(p);
	ret = dbmd2_vqe_alive(p);
	p->unlock(p);
	if (ret != 0)
		ret = sprintf(buf, "VQE firmware dead\n");
	else
		ret = sprintf(buf, "VQE firmware alive\n");
	return ret;
}

static ssize_t dbmd2_vqe_use_case_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (p->active_fw == DBMD2_FW_VQE) {
		if (!p->vqe_flags.in_call)
			/* special value as used to terminate call */
			return sprintf(buf, "0x100");

		return dbmd2_reg_show(dev,
				      DBMD2_VQE_GET_USE_CASE_CMD,
				      attr,
				      buf);
	}

	dev_err(p->dev, "%s: VQE firmware not active\n", __func__);
	return -EIO;
}

static int dbmd2_vqe_activate_call(struct dbmd2_private *p, unsigned long val)
{
	int ret;

	dev_dbg(p->dev, "%s: val: 0x%04x\n", __func__, (u16)val);

	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_SYSTEM_CONFIG_CMD |
				p->vqe_vc_syscfg,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_USE_CASE_CMD | val, NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}

	p->vqe_flags.in_call = 1;
	p->vqe_flags.use_case = val;

out_fail:
	return ret;
}

static int dbmd2_vqe_change_call_use_case(
		struct dbmd2_private *p, unsigned long val)
{
	int ret;

	dev_dbg(p->dev, "%s: val: 0x%04x\n", __func__, (u16)val);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_FADE_IN_OUT_CMD |
				DBMD2_VQE_SET_FADE_IN_OUT_FADE_OUT_EN,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}
	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_USE_CASE_CMD | val, NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}

	p->vqe_flags.use_case = val;

out_fail:
	return ret;
}

static int dbmd2_vqe_terminate_call(struct dbmd2_private *p, unsigned long val)
{
	int ret;

	dev_dbg(p->dev, "%s: val: 0x%04x\n", __func__, (u16)val);

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_FADE_IN_OUT_CMD |
				DBMD2_VQE_SET_FADE_IN_OUT_FADE_OUT_EN,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error FADE_OUT_EN\n", __func__);
		goto out_fail;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_USE_CASE_CMD |
				DBMD2_VQE_SET_USE_CASE_CMD_IDLE,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error USE_CASE_CMD_IDLE\n", __func__);
		goto out_fail;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_SYSTEM_CONFIG_CMD |
				DBMD2_VQE_SET_SYSTEM_CONFIG_PRIMARY_CFG,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error _CONFIG_PRIMARY_CFG\n", __func__);
		goto out_fail;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VQE_SET_HW_TDM_BYPASS_CMD |
				DBMD2_VQE_SET_HW_TDM_BYPASS_MODE_1 |
				DBMD2_VQE_SET_HW_TDM_BYPASS_FIRST_PAIR_EN,
				NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: HW_TDM_BYPASS_MODE_1: sys is not ready\n",
				__func__);
		goto out_fail;
	}

	p->vqe_flags.in_call = 0;

	dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);

out_fail:
	return ret;
}

static ssize_t dbmd2_vqe_use_case_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret;
	struct dbmd2_private *p = dev_get_drvdata(dev);
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (!p)
		return -EAGAIN;

	ret = dbmd2_vqe_mode_valid(p, (u32)val);
	if (!ret) {
		dev_err(p->dev, "%s: Invalid VQE mode 0x%x\n",
			__func__, (u32)val);
		return -EINVAL;
	}

	if (p->active_fw != DBMD2_FW_VQE) {
		dev_info(p->dev, "%s: VQE firmware not active, switching\n",
			__func__);
		if (p->va_flags.mode != DBMD2_IDLE) {
			p->lock(p);
			ret = dbmd2_set_mode(p, DBMD2_IDLE);
			p->unlock(p);
			if (ret)
				dev_err(p->dev,
					"%s: failed to set device to idle mode\n",
					__func__);
		}
		p->lock(p);
		ret = dbmd2_switch_to_vqe_firmware(p, 0);
		p->unlock(p);
		if (ret != 0) {
			dev_err(p->dev, "%s: failed switching to VQE mode\n",
					__func__);
			return -EIO;
		}

	}

	dev_info(p->dev, "%s: VQE firmware use case: %lu\n", __func__, val);

	p->lock(p);

	/*Check required operation: Call Activation or Deactivation */
	if (val & DBMD2_VQE_SET_USE_CASE_DE_ACT_MASK) {
		if (p->vqe_flags.in_call)
			ret = dbmd2_vqe_terminate_call(p, val);
		else
			/* simply re-ensure the sleep mode */
			ret = dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);
	} else if (p->vqe_flags.in_call)
		/* already in call */
		ret = dbmd2_vqe_change_call_use_case(p, val);
	else {
		ret = dbmd2_vqe_activate_call(p, val);
	}

	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_unlock;
	}

	ret = size;

out_unlock:
	p->unlock(p);
	return ret;
}



static ssize_t dbmd2_vqe_d2syscfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VQE_GET_SYSTEM_CONFIG_CMD,
					 attr, buf);
}

static ssize_t dbmd2_vqe_d2syscfg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VQE_SET_SYSTEM_CONFIG_CMD, attr,
				 buf, size, DBMD2_FW_VQE);
}

static ssize_t dbmd2_vqe_vc_syscfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = sprintf(buf, "%d\n", p->vqe_vc_syscfg);

	return ret;
}


static ssize_t dbmd2_vqe_vc_syscfg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (!p)
		return -EAGAIN;

	if (2 < val || val < 0) {
		dev_err(p->dev, "%s: invalid vqe vc system config value [0,1,2]\n",
			__func__);
		return -EINVAL;
	}

	p->vqe_vc_syscfg = (u32)val;

	return size;
}


static ssize_t dbmd2_vqe_hwbypass_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VQE_SET_HW_TDM_BYPASS_CMD, attr,
				 buf, size, DBMD2_FW_VQE);
}

static ssize_t dbmd2_vqe_spkvollvl_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return dbmd2_reg_show(dev, DBMD2_VQE_GET_SPK_VOL_LVL_CMD,
					 attr, buf);
}

static ssize_t dbmd2_vqe_spkvollvl_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return dbmd2_reg_store(dev, DBMD2_VQE_SET_SPK_VOL_LVL_CMD, attr,
				 buf, size, DBMD2_FW_VQE);
}

static ssize_t dbmd2_wakeup_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);
	int ret;
	int gpio_val;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (!dbmd2_can_wakeup(p))
		ret = sprintf(buf, "No WakeUp GPIO\n");
	else {
		gpio_val = gpio_get_value(p->pdata->gpio_wakeup);
		ret = sprintf(buf, "WakeUp GPIO: %d\n", gpio_val);
	}

	return ret;
}

static ssize_t dbmd2_wakeup_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmd2_private *p = dev_get_drvdata(dev);

	dbmd2_force_wake(p);

	return size;
}

static DEVICE_ATTR(fwver, S_IRUGO,
		   dbmd2_fw_ver_show, NULL);
static DEVICE_ATTR(d2paramaddr, S_IWUSR,
		   NULL, dbmd2_d2param_addr_store);
static DEVICE_ATTR(d2param, S_IRUGO | S_IWUSR,
		   dbmd2_d2param_show, dbmd2_d2param_store);
static DEVICE_ATTR(dump,  S_IRUGO,
		   dbmd2_dump_state, NULL);
static DEVICE_ATTR(io_addr, S_IRUGO | S_IWUSR,
		   dbmd2_io_addr_show, dbmd2_io_addr_store);
static DEVICE_ATTR(io_value, S_IRUGO | S_IWUSR,
		   dbmd2_io_value_show, dbmd2_io_value_store);
static DEVICE_ATTR(power_mode,  S_IRUGO,
		   dbmd2_power_mode_show, NULL);
static DEVICE_ATTR(reboot, S_IWUSR,
		   NULL, dbmd2_reboot_store);
static DEVICE_ATTR(reset, S_IWUSR,
		   NULL, dbmd2_reset_store);
static DEVICE_ATTR(va_audioconv, S_IRUGO | S_IWUSR,
		   dbmd2_va_audio_conv_show, dbmd2_va_audio_conv_store);
static DEVICE_ATTR(va_backlog_size, S_IRUGO | S_IWUSR,
		   dbd2_va_backlog_size_show, dbd2_va_backlog_size_store);
static DEVICE_ATTR(va_buffsize, S_IRUGO | S_IWUSR,
		   dbmd2_va_buffer_size_show, dbmd2_va_buffer_size_store);
static DEVICE_ATTR(va_buffsmps, S_IRUGO,
		   dbmd2_va_buffsmps_show, NULL);
static DEVICE_ATTR(va_capture_on_detect, S_IRUGO | S_IWUSR,
		   dbmd2_va_capture_on_detect_show,
		   dbmd2_va_capture_on_detect_store);
static DEVICE_ATTR(va_gain_shift_f, S_IRUGO | S_IWUSR,
		   dbmd2_va_gain_shift_factor_show,
		   dbmd2_va_gain_shift_factor_store);
static DEVICE_ATTR(va_load_amodel, S_IRUGO | S_IWUSR ,
		   NULL, dbmd2_va_acoustic_model_store);
static DEVICE_ATTR(va_max_sample, S_IRUGO,
		   dbmd2_va_max_sample_show, NULL);
static DEVICE_ATTR(va_micgain, S_IRUGO | S_IWUSR,
		   dbmd2_va_micgain_show, dbmd2_va_micgain_store);
static DEVICE_ATTR(va_opmode,  S_IRUGO | S_IWUSR ,
		   dbmd2_va_opmode_show, dbmd2_opr_mode_store);
static DEVICE_ATTR(va_rsize, S_IRUGO | S_IWUSR,
		   dbmd2_va_rsize_show, dbmd2_va_rsize_store);
static DEVICE_ATTR(va_rxsize, S_IRUGO | S_IWUSR,
		   dbmd2_va_rxsize_show, dbmd2_va_rxsize_store);
static DEVICE_ATTR(va_trigger_level, S_IRUGO | S_IWUSR,
		   dbmd2_va_trigger_level_show, dbmd2_va_trigger_level_store);
static DEVICE_ATTR(va_uart_delay,  S_IRUGO | S_IWUSR ,
		   dbmd2_va_uart_delay_show, dbmd2_va_uart_delay_store);
static DEVICE_ATTR(va_uart_fifo_size,  S_IRUGO | S_IWUSR ,
		   dbmd2_va_uart_fifo_size_show, dbmd2_va_uart_fifo_size_store);
static DEVICE_ATTR(va_verif_level, S_IRUGO | S_IWUSR,
		   dbmd2_va_verification_level_show,
		   dbmd2_va_verification_level_store);
static DEVICE_ATTR(va_wsize, S_IRUGO | S_IWUSR,
		   dbmd2_va_wsize_show, dbmd2_va_wsize_store);
static DEVICE_ATTR(vqe_ping,  S_IRUGO,
		   dbmd2_vqe_ping_show, NULL);
static DEVICE_ATTR(vqe_use_case, S_IRUGO | S_IWUSR,
		   dbmd2_vqe_use_case_show, dbmd2_vqe_use_case_store);
static DEVICE_ATTR(vqe_d2syscfg,  S_IRUGO | S_IWUSR ,
		   dbmd2_vqe_d2syscfg_show, dbmd2_vqe_d2syscfg_store);
static DEVICE_ATTR(vqe_vc_syscfg,  S_IRUGO | S_IWUSR ,
		   dbmd2_vqe_vc_syscfg_show, dbmd2_vqe_vc_syscfg_store);
static DEVICE_ATTR(vqe_hwbypass, S_IWUSR,
		   NULL, dbmd2_vqe_hwbypass_store);
static DEVICE_ATTR(vqe_spkvollvl, S_IRUGO | S_IWUSR ,
		   dbmd2_vqe_spkvollvl_show, dbmd2_vqe_spkvollvl_store);
static DEVICE_ATTR(wakeup, S_IRUGO | S_IWUSR,
		   dbmd2_wakeup_show, dbmd2_wakeup_store);


static struct attribute *dbmd2_attributes[] = {
	&dev_attr_fwver.attr,
	&dev_attr_d2paramaddr.attr,
	&dev_attr_d2param.attr,
	&dev_attr_dump.attr,
	&dev_attr_io_addr.attr,
	&dev_attr_io_value.attr,
	&dev_attr_power_mode.attr,
	&dev_attr_reboot.attr,
	&dev_attr_reset.attr,
	&dev_attr_va_audioconv.attr,
	&dev_attr_va_backlog_size.attr,
	&dev_attr_va_buffsize.attr,
	&dev_attr_va_buffsmps.attr,
	&dev_attr_va_capture_on_detect.attr,
	&dev_attr_va_gain_shift_f.attr,
	&dev_attr_va_load_amodel.attr,
	&dev_attr_va_max_sample.attr,
	&dev_attr_va_micgain.attr,
	&dev_attr_va_opmode.attr,
	&dev_attr_va_rsize.attr,
	&dev_attr_va_rxsize.attr,
	&dev_attr_va_trigger_level.attr,
	&dev_attr_va_uart_delay.attr,
	&dev_attr_va_uart_fifo_size.attr,
	&dev_attr_va_verif_level.attr,
	&dev_attr_va_wsize.attr,
	&dev_attr_vqe_ping.attr,
	&dev_attr_vqe_use_case.attr,
	&dev_attr_vqe_vc_syscfg.attr,
	&dev_attr_vqe_d2syscfg.attr,
	&dev_attr_vqe_hwbypass.attr,
	&dev_attr_vqe_spkvollvl.attr,
	&dev_attr_wakeup.attr,
	NULL,
};

static const struct attribute_group dbmd2_attribute_group = {
	.attrs = dbmd2_attributes,
};

static int dbmd2_perform_recovery(struct dbmd2_private *p)
{
	int ret = 0;
	int active_fw = p->active_fw;

	dev_info(p->dev, "%s: active FW - %s\n", __func__,
			dbmd2_fw_type_to_str(active_fw));

	if (active_fw == DBMD2_FW_VA) {
		p->va_flags.buffering = 0;
		flush_work(&p->sv_work);
	}

	p->lock(p);

	ret = dbmd2_set_power_mode(p, DBMD2_PM_BOOTING);
	if (ret)
		goto out;

	dbmd2_set_boot_active(p);

	/* prepare boot if required */
	ret = p->chip->prepare_boot(p);
	if (ret)
		goto out;

	/* boot */
	ret = p->chip->boot(NULL, p, NULL, 0, 0);
	if (ret)
		goto out;

	/* finish boot if required */
	ret = p->chip->finish_boot(p);
	if (ret)
		goto out;

	if (active_fw == DBMD2_FW_VA) {
		struct va_flags flags;

		/* bkp flags */
		memcpy(&flags, &p->va_flags, sizeof(flags));

		dbmd2_set_va_active(p);

		/* restore flags */
		memcpy(&p->va_flags, &flags, sizeof(flags));

		ret = p->chip->set_va_firmware_ready(p);
		if (ret) {
			dev_err(p->dev,
				"%s: could not set to ready VA firmware\n",
				__func__);
			goto out;
		}

		ret = dbmd2_config_va_mode(p);
		if (ret) {
			dev_err(p->dev,
				"%s: could not config VA firmware\n",
				__func__);
			goto out;
		}

		if (p->va_flags.amodel_len > 0 && p->va_flags.a_model_loaded)
			p->unlock(p);
			dbmd2_acoustic_model_load(
				p, p->amodel_buf, p->va_flags.amodel_len);
			p->lock(p);

	} else if (active_fw == DBMD2_FW_VQE) {
		struct vqe_flags flags;

		/* bkp flags */
		memcpy(&flags, &p->vqe_flags, sizeof(flags));

		dbmd2_set_vqe_active(p);

		/* restore flags */
		memcpy(&p->vqe_flags, &flags, sizeof(flags));

		ret = p->chip->set_vqe_firmware_ready(p);
		if (ret) {
			dev_err(p->dev,
				"%s: could not set to ready VQE firmware\n",
				__func__);
			goto out;
		}

		/* special setups for the VQE firmware */
		ret = dbmd2_config_vqe_mode(p);
		if (ret) {
			dev_err(p->dev, "%s: could not config VQE firmware\n",
				__func__);
			goto out;
		}

		if (p->vqe_flags.in_call &&
			p->vqe_flags.use_case) {
			ret = dbmd2_vqe_activate_call(p, p->vqe_flags.use_case);
			if (ret) {
				dev_err(p->dev,
					"%s: failed to activate call\n",
					__func__);
				goto out;
			}
		}
	}

	p->device_ready = true;

	ret = dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);
	if (ret)
		goto out;

out:
	p->unlock(p);
	return ret;
}

/* ------------------------------------------------------------------------
 * interface functions for platform driver
 * ------------------------------------------------------------------------ */

int dbmd2_get_samples(char *buffer, unsigned int samples)
{
	struct dbmd2_private *p = dbd2_data;
	int avail = kfifo_len(&p->pcm_kfifo);
	int samples_avail = avail / p->bytes_per_sample;
	int ret;

	if (samples_avail < samples)
		return -1;

	ret = kfifo_out(&p->pcm_kfifo,
			buffer,
			samples * p->bytes_per_sample);

	return (ret == samples * p->bytes_per_sample ? 0 : -1);
}
EXPORT_SYMBOL(dbmd2_get_samples);

int dbmd2_codec_lock(void)
{
	if (!atomic_add_unless(&dbd2_data->audio_owner, 1, 1))
		return -EBUSY;

	return 0;
}
EXPORT_SYMBOL(dbmd2_codec_lock);

int dbmd2_codec_unlock(void)
{
	atomic_dec(&dbd2_data->audio_owner);
	return 0;
}
EXPORT_SYMBOL(dbmd2_codec_unlock);

void dbmd2_start_buffering(void)
{
	int ret;

	if (!dbd2_data->pdata->auto_buffering)
		return;

	dbd2_data->lock(dbd2_data);
	dbd2_data->va_flags.buffering = 1;
	ret = dbmd2_set_mode(dbd2_data, DBMD2_BUFFERING);
	dbd2_data->unlock(dbd2_data);
}
EXPORT_SYMBOL(dbmd2_start_buffering);

void dbmd2_stop_buffering(void)
{
	int ret;

	if (!dbd2_data->pdata->auto_buffering)
		return;

	dbd2_data->va_flags.buffering = 0;

	flush_work(&dbd2_data->sv_work);

	dbd2_data->lock(dbd2_data);
	ret = dbmd2_set_mode(dbd2_data, DBMD2_IDLE);
	dbd2_data->unlock(dbd2_data);
}
EXPORT_SYMBOL(dbmd2_stop_buffering);


/* ------------------------------------------------------------------------
 * codec driver section
 * ------------------------------------------------------------------------ */

#define DUMMY_REGISTER 0

static int dbmd2_dai_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0;
	struct dbmd2_private *p = snd_soc_codec_get_drvdata(rtd->codec);
	int channels;

	channels = params_channels(params);
	if (channels != p->pdata->va_audio_channels) {
		dev_warn(p->dev,
			 "%s: DAI channels %d not matching hw channels %d\n",
			 __func__, channels, p->pdata->va_audio_channels);
	}

	p->lock(p);
	if (dbmd2_sleeping(p)) {
		ret = -EIO;
		goto out_unlock;
	}
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		ret = dbmd2_set_bytes_per_sample(p, DBD2_AUDIO_MODE_PCM);
		dev_dbg(p->dev, "%s: set pcm format: SNDRV_PCM_FORMAT_S16_LE\n",
			__func__);
		break;
	case SNDRV_PCM_FORMAT_MU_LAW:
		ret = dbmd2_set_bytes_per_sample(p, DBD2_AUDIO_MODE_MU_LAW);
		dev_dbg(p->dev, "%s: set pcm format: SNDRV_PCM_FORMAT_MU_LAW\n",
			__func__);
		break;
	default:
		ret = -EINVAL;
	}

out_unlock:
	p->unlock(p);

	return ret;
}

static struct snd_soc_dai_ops dbmd2_dai_ops = {
	.hw_params = dbmd2_dai_hw_params,
};

/* DBMD2 codec DAI: */
static struct snd_soc_dai_driver dbmd2_dais[] = {
	{
		.name = "DBMD2_codec_dai",
		.capture = {
			.stream_name	= "vs_buffer",
			.channels_min	= 1,
			.channels_max	= 1,
			.rates		= SNDRV_PCM_RATE_8000 |
					  SNDRV_PCM_RATE_16000,
			.formats	= SNDRV_PCM_FMTBIT_MU_LAW |
					  SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &dbmd2_dai_ops,
	},
	{
		.name = "dbmd2_i2s0",
		.id = DBMD2_I2S0,
		.playback = {
			.stream_name	= "I2S0 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S0 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.ops = &dbmd2_i2s_dai_ops,
	},
	{
		.name = "dbmd2_i2s1",
		.id = DBMD2_I2S1,
		.playback = {
			.stream_name	= "I2S1 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S1 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.ops = &dbmd2_i2s_dai_ops,
	},
	{
		.name = "dbmd2_i2s2",
		.id = DBMD2_I2S2,
		.playback = {
			.stream_name	= "I2S2 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S2 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.ops = &dbmd2_i2s_dai_ops,
	},
	{
		.name = "dbmd2_i2s3",
		.id = DBMD2_I2S3,
		.playback = {
			.stream_name	= "I2S3 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S3 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMD2_I2S_RATES,
			.formats	= DBMD2_I2S_FORMATS,
		},
		.ops = &dbmd2_i2s_dai_ops,
	},
};

/* ASoC controls */
static unsigned int dbmd2_dev_read(struct snd_soc_codec *codec,
				   unsigned int reg)
{
	struct dbmd2_private *p = dbd2_data;
	int ret;
	u16 val = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	/* VA controls */
	if (p->active_fw != DBMD2_FW_VA)
		goto out_unlock;

	if (reg == DUMMY_REGISTER)
		goto out_unlock;

	/* just return 0 - the user needs to wakeup first */
	if (dbmd2_sleeping(p)) {
		dev_err(p->dev, "%s: device sleeping\n", __func__);
		goto out_unlock;
	}

	if (p->va_flags.mode == DBMD2_DETECTION) {
		dev_dbg(p->dev, "%s: device in detection state\n", __func__);
		goto out_unlock;
	}

	dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);

	ret = dbmd2_send_cmd(p, reg, &val);
	if (ret < 0)
		dev_err(p->dev, "%s: read 0x%x error\n", __func__, reg);

	dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);

out_unlock:
	p->unlock(p);
	return (unsigned int)val;
}

static int dbmd2_dev_write(struct snd_soc_codec *codec, unsigned int reg,
			   unsigned int val)
{
	struct dbmd2_private *p = dbd2_data;
	int ret = -EIO;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (!snd_soc_codec_writable_register(codec, reg)) {
		dev_err(p->dev, "%s: register not writable\n", __func__);
		return -EIO;
	}

	dev_dbg(p->dev, "%s: ------- VA control ------\n", __func__);
	if (p->active_fw != DBMD2_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	p->lock(p);

	ret = dbmd2_set_power_mode(p, DBMD2_PM_ACTIVE);
	if (reg == DUMMY_REGISTER)
		goto out_unlock;

	ret = dbmd2_send_cmd(p, (reg << 16) | (val & 0xffff), NULL);
	if (ret < 0)
		dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
			__func__, val, reg);

out_unlock:
	dbmd2_set_power_mode(p, DBMD2_PM_FALLING_ASLEEP);
	p->unlock(p);
out:
	return ret;
}

static int dbmd2_va_control_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned short val, reg = mc->reg;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	int ret;
	struct dbmd2_private *p = dbd2_data;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	/* VA controls */
	if (p->active_fw != DBMD2_FW_VA) {
		ucontrol->value.integer.value[0] = 0;
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out_unlock;
	}

	/* maybe just return and the user needs to wakeup */
	if (dbmd2_sleeping(p)) {
		dev_dbg(p->dev, "%s: device sleeping\n", __func__);
		goto out_unlock;
	}

	if (p->va_flags.mode == DBMD2_DETECTION) {
		dev_dbg(p->dev, "%s: device in detection state\n", __func__);
		goto out_unlock;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VA_CMD_MASK | ((reg & 0xff) << 16),
				  &val);
	if (ret < 0)
		dev_err(p->dev, "%s: read 0x%x error\n", __func__, reg);

	val &= mask;

	ucontrol->value.integer.value[0] = val;

out_unlock:
	p->unlock(p);

	return 0;
}

static int dbmd2_va_control_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned short val = ucontrol->value.integer.value[0];
	unsigned short reg = mc->reg;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	int ret;
	struct dbmd2_private *p = dbd2_data;
	unsigned int va_reg = DBMD2_VA_CMD_MASK | ((reg & 0xff) << 16);

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	if (!snd_soc_codec_writable_register(codec, reg)) {
		dev_err(p->dev, "%s: register not writable\n", __func__);
		return -EIO;
	}

	dev_dbg(p->dev, "%s: ------- VA control ------\n", __func__);
	if (p->active_fw != DBMD2_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	val &= mask;

	p->lock(p);

	/* maybe just return and the user needs to wakeup */
	if (dbmd2_sleeping(p)) {
		dev_err(p->dev, "%s: device sleeping\n", __func__);
		goto out_unlock;
	}

	if (va_reg == DBMD2_VA_AUDIO_HISTORY) {
		ret = dbd2_set_backlog_len(p, val);
		if (ret < 0)
			dev_err(p->dev, "%s: set history error\n", __func__);
	} else {
		ret = dbmd2_send_cmd(p, va_reg | (val & 0xffff), NULL);
		if (ret < 0)
			dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
				__func__, val, reg);
	}

out_unlock:
	p->unlock(p);

out:
	return 0;
}

static int dbmd2_vqe_use_case_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	unsigned short val;
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	if (p->active_fw != DBMD2_FW_VQE) {
		ucontrol->value.integer.value[0] = 5;
		dev_dbg(p->dev, "%s: VQE firmware not active\n", __func__);
		goto out_unlock;
	}

	if (dbmd2_sleeping(p)) {
		dev_dbg(p->dev, "%s: device sleeping\n", __func__);
		goto out_unlock;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VQE_GET_USE_CASE_CMD, &val);
	if (ret < 0)
		dev_err(p->dev, "%s: read 0x%x error\n",
			__func__, DBMD2_VQE_GET_USE_CASE_CMD);

	/* TODO: check this */
	ucontrol->value.integer.value[0] = (val == 0xffff ? 0 : val);

out_unlock:
	p->unlock(p);
	return 0;
}

static int dbmd2_vqe_use_case_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	unsigned short val = ucontrol->value.integer.value[0];
	int ret;
	int reg = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = dbmd2_vqe_mode_valid(p, (u32)val);
	if (!ret) {
		dev_err(p->dev, "%s: Invalid VQE mode 0x%x\n",
			__func__, (u32)val);
		return -EINVAL;
	}

	if (p->active_fw != DBMD2_FW_VQE) {
		dev_info(p->dev, "%s: VQE firmware not active, switching\n",
			__func__);
		p->lock(p);
		ret = dbmd2_switch_to_vqe_firmware(p, 0);
		p->unlock(p);
		if (ret != 0) {
			dev_info(p->dev,
				"%s: Error switching to VQE firmware\n",
				__func__);
			return -EIO;
		}

	}

	reg += (DBMD2_VQE_SET_CMD_OFFSET >> 16);

	p->lock(p);

	ret = dbmd2_vqe_set_use_case(p, val);
	if (ret == 0)
		ucontrol->value.integer.value[0] = val;

	p->unlock(p);

	return 0;
}

/* Operation modes */
static int dbmd2_operation_mode_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	unsigned short val;
	int ret = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	if (p->active_fw != DBMD2_FW_VA) {
		ucontrol->value.integer.value[0] = 6;
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out_unlock;
	}

	if (dbmd2_sleeping(p)) {
		val = p->va_flags.mode;
		goto out_report_mode;
	}

	if (p->va_flags.mode == DBMD2_DETECTION) {
		dev_dbg(p->dev, "%s: device in detection state\n", __func__);
		goto out_unlock;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VA_OPR_MODE, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to read DBMD2_VA_OPR_MODE\n",
			__func__);
		goto out_unlock;
	}


out_report_mode:
	if (val == DBMD2_SLEEP_PLL_ON)
		ucontrol->value.integer.value[0] = 1;
	else if (val == DBMD2_SLEEP_PLL_OFF)
		ucontrol->value.integer.value[0] = 2;
	else if (val == DBMD2_HIBERNATE)
		ucontrol->value.integer.value[0] = 3;
	else if (val == DBMD2_DETECTION)
		ucontrol->value.integer.value[0] = 4;
	else if (val == DBMD2_BUFFERING)
		ucontrol->value.integer.value[0] = 5;
	else if (val == DBMD2_IDLE)
		ucontrol->value.integer.value[0] = 0;
	else
		dev_err(p->dev, "%s: unknown operation mode: %u\n",
			__func__, val);

out_unlock:
	p->unlock(p);

	return ret;
}

static int dbmd2_operation_mode_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	int ret = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s: ------- VA control ------\n", __func__);
	if (p->active_fw != DBMD2_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	p->lock(p);

	if (ucontrol->value.integer.value[0] == 0)
		dbmd2_set_mode(p, DBMD2_IDLE);
	else if (ucontrol->value.integer.value[0] == 1)
		dbmd2_set_mode(p, DBMD2_SLEEP_PLL_ON);
	else if (ucontrol->value.integer.value[0] == 2)
		dbmd2_set_mode(p, DBMD2_SLEEP_PLL_OFF);
	else if (ucontrol->value.integer.value[0] == 3)
		dbmd2_set_mode(p, DBMD2_HIBERNATE);
	else if (ucontrol->value.integer.value[0] == 4)
		dbmd2_set_mode(p, DBMD2_DETECTION);
	else if (ucontrol->value.integer.value[0] == 5)
		dbmd2_set_mode(p, DBMD2_BUFFERING);
	else
		ret = -EINVAL;

	p->unlock(p);
out:
	return ret;
}

static const char *const dbmd2_vqe_use_case_texts[] = {
	"Idle",
	"HS_NB",
	"HS_WB",
	"HF_NB",
	"HF_WB",
	"Not_active",
};

static const struct soc_enum dbmd2_vqe_use_case_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmd2_vqe_use_case_texts),
			    dbmd2_vqe_use_case_texts);

static const char *const dbmd2_operation_mode_texts[] = {
	"Idle",
	"Sleep_pll_on",
	"Sleep_pll_off",
	"Hibernate",
	"Detection",
	"Buffering",
	"Not_active",
};

static const struct soc_enum dbmd2_operation_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmd2_operation_mode_texts),
			    dbmd2_operation_mode_texts);

static DECLARE_TLV_DB_SCALE(dbmd2_db_tlv, -3276700, 3276800, 0);

static const unsigned int dbmd2_mic_analog_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	0, 21, TLV_DB_SCALE_ITEM(-400, 200, 0),
};

static int dbmd2_amodel_load_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 3;
	return 0;
}

static int dbmd2_amodel_load_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	unsigned short value = ucontrol->value.integer.value[0];
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s: value - %d\n", __func__, value);


	if (p->active_fw == DBMD2_FW_VQE && p->vqe_flags.in_call) {
		dev_err(p->dev,
			"%s: Switching to VA is not allowed when in CALL\n",
			__func__);
		return -EAGAIN;
	}


	if (value == 0) {

		dev_info(p->dev, "%s: sleep mode: %d\n", __func__, value);

		if (p->active_fw != DBMD2_FW_VA) {
			dev_err(p->dev, "%s: VA firmware not active, error\n",
				__func__);
			return -EAGAIN;
		}
		p->lock(p);

		if (dbmd2_sleeping(p)) {
			p->unlock(p);
			return 0;
		}

		ret = dbmd2_set_mode(p, DBMD2_IDLE);
		if (ret) {
			dev_err(p->dev,
				"%s: failed to set device to idle mode\n",
				__func__);
			p->unlock(p);
			return -1;
		}
		p->unlock(p);
		return 0;
	} else if (value > 2) {
		dev_err(p->dev, "%s: Unsupported mode:%d\n", __func__, value);
		return -1;
	}

	p->lock(p);

	if (value == 2)
		p->va_detection_mode = DETECTION_MODE_VOICE_ENERGY;
	else
		p->va_detection_mode = DETECTION_MODE_PHRASE;

	p->unlock(p);

	/* trigger loading of new acoustic model */
	return dbmd2_load_new_acoustic_model_sync(p);
}

static int dbmd2_microphone_mode_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	unsigned short val, val2;
	int ret = 0;


	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	/* VA controls */
	if (p->active_fw != DBMD2_FW_VA) {
		ucontrol->value.integer.value[0] = DBMD2_MIC_MODE_DISABLE;
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out_unlock;
	}

	if (dbmd2_sleeping(p)) {
		ucontrol->value.integer.value[0] = DBMD2_MIC_MODE_DISABLE;
		goto out_unlock;
	}

	if (p->va_flags.mode == DBMD2_DETECTION) {
		dev_dbg(p->dev, "%s: device in detection state\n", __func__);
		goto out_unlock;
	}

	ret = dbmd2_send_cmd(p, DBMD2_VA_MICROPHONE1_CONFIGURATION, &val);
	if (ret < 0) {
		dev_err(p->dev,
				"%s: failed to read microphone configuration\n",
				__func__);
		goto out_unlock;
	}

	val &= 0x7;
	if (val == 0)
		ucontrol->value.integer.value[0] = DBMD2_MIC_MODE_DISABLE;
	else if (val == 5)
		ucontrol->value.integer.value[0] = DBMD2_MIC_MODE_ANALOG;
	else if (val > 0 && val < 5) { /* DIGITAL */
		ret = dbmd2_send_cmd(p,
				     DBMD2_VA_MICROPHONE2_CONFIGURATION,
				     &val2);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to read microphone2 configuration\n",
				__func__);
			goto out_unlock;
		}

		val2 &= 0x7;

		if (val2 == 0) {
			/* DIGITAL LEFT */
			if (val == 1 || val == 3)
				ucontrol->value.integer.value[0] =
					DBMD2_MIC_MODE_DIGITAL_LEFT;
			/* DIGITAL RIGHT */
			else if (val == 2 || val == 4)
				ucontrol->value.integer.value[0] =
					DBMD2_MIC_MODE_DIGITAL_RIGHT;
		} else {
			/* DIGITAL LEFT */
			if (val == 1 || val == 3)
				ucontrol->value.integer.value[0] =
				DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT;
			/* DIGITAL RIGHT */
			else if (val == 2 || val == 4)
				ucontrol->value.integer.value[0] =
				DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT;
		}
	} else
		dev_err(p->dev, "%s: unknown microphone mode: %u\n",
			__func__, val);

out_unlock:
	p->unlock(p);

	return ret;
}

static int dbmd2_microphone_mode_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	int ret = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s: ------- VA control ------\n", __func__);
	if (p->active_fw != DBMD2_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	p->lock(p);

	if (dbmd2_sleeping(p)) {
		dev_err(p->dev, "%s: device sleeping\n", __func__);
		goto out_unlock;
	}

	switch (ucontrol->value.integer.value[0]) {
	case DBMD2_MIC_MODE_DIGITAL_LEFT:
	case DBMD2_MIC_MODE_DIGITAL_RIGHT:
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
	case DBMD2_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
	case DBMD2_MIC_MODE_ANALOG:
	case DBMD2_MIC_MODE_DISABLE:
		ret = dbmd2_set_microphone_mode(
				p, ucontrol->value.integer.value[0]);
		break;
	default:
		dev_err(p->dev, "%s: unsupported microphone mode %d\n",
			__func__, (int)(ucontrol->value.integer.value[0]));
		ret = -EINVAL;
		break;
	}

out_unlock:
	p->unlock(p);
out:
	return ret;
}

static int dbmd2_va_capture_on_detect_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	int ret = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);
	ucontrol->value.integer.value[0] = p->va_capture_on_detect;
	p->unlock(p);

	return ret;
}

static int dbmd2_va_capture_on_detect_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmd2_private *p = dbd2_data;
	int ret = 0;

	p->lock(p);
	p->va_capture_on_detect = ucontrol->value.integer.value[0];
	p->unlock(p);

	return ret;
}

static const char * const dbmd2_microphone_mode_texts[] = {
	"DigitalLeft",
	"DigitalRight",
	"DigitalStereoTrigOnLeft",
	"DigitalStereoTrigOnRight",
	"Analog",
	"Disable",
};

static const struct soc_enum dbmd2_microphone_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmd2_microphone_mode_texts),
			    dbmd2_microphone_mode_texts);

static const char * const dbmd2_va_capture_on_detection_texts[] = {
	"OFF",
	"ON",
};

static const struct soc_enum dbmd2_va_capture_on_detect_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmd2_va_capture_on_detection_texts),
			    dbmd2_va_capture_on_detection_texts);

static const struct snd_kcontrol_new dbmd2_snd_controls[] = {
	/*
	 * VA mixer controls
	 */
	SOC_ENUM_EXT("Operation mode", dbmd2_operation_mode_enum,
		dbmd2_operation_mode_get, dbmd2_operation_mode_set),
	SOC_SINGLE_EXT_TLV("Trigger threshold", 0x02, 0, 0xffff, 0,
		dbmd2_va_control_get, dbmd2_va_control_put, dbmd2_db_tlv),
	SOC_SINGLE_EXT("Verification threshold", 0x03, 0, 8192, 0,
		dbmd2_va_control_get, dbmd2_va_control_put),
	SOC_SINGLE_EXT("Gain shift value", 0x04, 0, 15, 0,
		dbmd2_va_control_get, dbmd2_va_control_put),
	SOC_SINGLE_EXT_TLV("Microphone analog gain", 0x16, 0, 0xffff, 0,
		dbmd2_va_control_get, dbmd2_va_control_put,
		dbmd2_mic_analog_gain_tlv),
	SOC_SINGLE_EXT("Load acoustic model", 0, 0, 2, 0,
		dbmd2_amodel_load_get, dbmd2_amodel_load_set),
	SOC_SINGLE("Word ID",
		   VA_MIXER_REG(DBMD2_VA_LAST_DETECT_WORD_NUM),
		   0, 0xffff, 0),
	SOC_SINGLE("Trigger Level",
		   VA_MIXER_REG(DBMD2_VA_DETECT_TRIGER_LEVEL),
		   0, 0xffff, 0),
	SOC_SINGLE("Verification Level",
		   VA_MIXER_REG(DBMD2_VA_DETECT_VERIFICATION_LEVEL),
		   0, 0xffff, 0),
	SOC_SINGLE("Duration",
		   VA_MIXER_REG(DBMD2_VA_LAST_DURATION),
		   0, 0xffff, 0),
	SOC_SINGLE_EXT("Backlog size", 0x12, 0, 0x0fff, 0,
		dbmd2_va_control_get, dbmd2_va_control_put),
	SOC_SINGLE("Error",
		   VA_MIXER_REG(DBMD2_VA_LAST_ERROR),
		   0, 0xffff, 0),
	SOC_ENUM_EXT("Microphone mode", dbmd2_microphone_mode_enum,
		dbmd2_microphone_mode_get, dbmd2_microphone_mode_set),
	SOC_ENUM_EXT("Capture on detection", dbmd2_va_capture_on_detect_enum,
		dbmd2_va_capture_on_detect_get, dbmd2_va_capture_on_detect_set),

	/*
	 * VQE mixer controls
	 */
	SOC_SINGLE_EXT("Use case",
		       VQE_MIXER_REG(DBMD2_VQE_GET_USE_CASE_CMD),
		       0, 15, 0,
		       dbmd2_vqe_use_case_get,
		       dbmd2_vqe_use_case_put),
};

static int dbmd2_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		break;
	case SND_SOC_BIAS_OFF:
		break;
	default:
		return -EINVAL;
	}

	/* change to new state */
	codec->dapm.bias_level = level;

	return 0;
}

static int dbmd2_dev_probe(struct snd_soc_codec *codec)
{
	codec->control_data = NULL;

	return 0;
}

static int dbmd2_dev_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int dbmd2_dev_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int dbmd2_dev_resume(struct snd_soc_codec *codec)
{
	return 0;
}

static int dbmd2_is_writeable_register(struct snd_soc_codec *codec,
				       unsigned int reg)
{
	/* struct dbmd2_private *p = snd_soc_codec_get_drvdata(codec); */
	int ret = 1;

	/* VA control registers */
	switch (reg) {
	case VA_MIXER_REG(DBMD2_VA_LAST_DETECT_WORD_NUM):
	case VA_MIXER_REG(DBMD2_VA_DETECT_TRIGER_LEVEL):
	case VA_MIXER_REG(DBMD2_VA_DETECT_VERIFICATION_LEVEL):
	case VA_MIXER_REG(DBMD2_VA_LAST_DURATION):
	case VA_MIXER_REG(DBMD2_VA_LAST_ERROR):
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static struct snd_soc_codec_driver soc_codec_dev_dbmd2 = {
	.probe   = dbmd2_dev_probe,
	.remove  = dbmd2_dev_remove,
	.suspend = dbmd2_dev_suspend,
	.resume  = dbmd2_dev_resume,
	.set_bias_level = dbmd2_set_bias_level,
	.read = dbmd2_dev_read,
	.write = dbmd2_dev_write,
	.controls = dbmd2_snd_controls,
	.num_controls = ARRAY_SIZE(dbmd2_snd_controls),
	.writable_register = dbmd2_is_writeable_register,

	.reg_cache_size = 0,
	.reg_word_size = 0,
	.reg_cache_default = NULL,
	.ignore_pmdown_time = true,
};

int dbmd2_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

	dev_dbg(codec->dev, "%s start\n", __func__);

	rc = snd_soc_add_codec_controls(codec, dbmd2_snd_controls,
				ARRAY_SIZE(dbmd2_snd_controls));

	if (rc)
		dev_err(codec->dev,
			"%s(): dbmd2_remote_add_codec_controls failed\n",
			__func__);

	return rc;
}
EXPORT_SYMBOL(dbmd2_remote_add_codec_controls);

void dbmd2_remote_register_event_callback(event_cb func)
{
	if (dbd2_data)
		dbd2_data->event_callback = func;
	else
		g_event_callback = func;
}
EXPORT_SYMBOL(dbmd2_remote_register_event_callback);

void dbmd2_remote_register_set_i2c_freq_callback(set_i2c_freq_cb func)
{
	if (dbd2_data)
		dbd2_data->set_i2c_freq_callback = func;
	else
		g_set_i2c_freq_callback = func;
}
EXPORT_SYMBOL(dbmd2_remote_register_set_i2c_freq_callback);

static void dbmd2_uevent_work(struct work_struct *work)
{
	struct dbmd2_private *p = container_of(
			work, struct dbmd2_private,	uevent_work);
	int event_id;
	int ret;

	if (p->va_detection_mode == DETECTION_MODE_VOICE_ENERGY) {
		dev_info(p->dev, "%s: VOICE ENERGY\n", __func__);
		event_id = 0;
	} else {
		dev_info(p->dev, "%s: PASSPHRASE\n", __func__);

		p->lock(p);

		/* prepare anything if needed (e.g. increase speed) */
		ret = p->chip->prepare_buffering(p);
		if (ret)
			dev_warn(p->dev,
					"%s: failed switch to higher speed\n",
					__func__);

		p->unlock(p);


		event_id = dbd2_d2param_get(p, DBD2_D2PARAM_ALTWORDID);

		if (event_id < 0) {
			dev_err(p->dev,
					"%s: failed reading WordID\n",
					__func__);
			return;
		}

		dev_info(p->dev, "%s: last WordID:%d\n", __func__, event_id);

		if (event_id == 3) {
			/* as per SV Algo implementer's recommendation
			 * --> it 1 */
			dev_dbg(p->dev, "%s: fixing to 1\n", __func__);
			event_id = 1;
		}
	}

	if (p->event_callback)
		p->event_callback(event_id);
}

static void dbmd2_sv_work(struct work_struct *work)
{
	struct dbmd2_private *p = container_of(
			work, struct dbmd2_private, sv_work);
	int ret;
	int bytes_per_sample = p->bytes_per_sample;
	unsigned int bytes_to_read;
	u16 nr_samples;
	unsigned int total = 0;
	int kfifo_space = 0;
	int retries = 0;

	dev_dbg(p->dev, "%s\n", __func__);

	p->va_flags.irq_inuse = 0;

	if (!p->va_capture_on_detect) {
		dev_dbg(p->dev, "%s: no capture requested, exit..\n", __func__);
		goto out;
	}

	p->lock(p);

	/* flush fifo */
	kfifo_reset(&p->pcm_kfifo);

	/* prepare anything if needed (e.g. increase speed) */
	ret = p->chip->prepare_buffering(p);
	if (ret)
		goto out_fail_unlock;

	p->unlock(p);

	do {
		p->lock(p);
		bytes_to_read = 0;
		/* read number of samples available in audio buffer */
		if (dbmd2_send_cmd(p,
				   DBMD2_VA_NUM_OF_SMP_IN_BUF,
				   &nr_samples) < 0) {
			dev_err(p->dev,
				"%s: failed to read DBMD2_VA_NUM_OF_SMP_IN_BUF\n",
				__func__);
			p->unlock(p);
			goto out;
		}

		if (nr_samples == 0xffff) {
			dev_info(p->dev,
				"%s: buffering mode ended - fw in idle mode",
				__func__);
			p->unlock(p);
			break;
		}

		p->unlock(p);

		/* Now fill the kfifo. The user can access the data in
		 * parallel. The kfifo is safe for concurrent access of one
		 * reader (ALSA-capture/character device) and one writer (this
		 * work-queue). */
		if (nr_samples) {
			bytes_to_read = nr_samples * 8 * bytes_per_sample;

			/* limit transaction size (no software flow control ) */
			if (bytes_to_read > p->rxsize && p->rxsize)
				bytes_to_read = p->rxsize;

			kfifo_space = kfifo_avail(&p->pcm_kfifo);

			if (bytes_to_read > kfifo_space)
				bytes_to_read = kfifo_space;

			/* recalculate number of samples */
			nr_samples = bytes_to_read / (8 * bytes_per_sample);

			if (!nr_samples) {
				/* not enough samples, wait some time */
				usleep_range(5000, 6000);
				retries++;
				if (retries > MAX_RETRIES_TO_WRITE_TOBUF)
					break;
				continue;
			}
			/* get audio samples */
			p->lock(p);
			ret = p->chip->read_audio_data(p, nr_samples);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to read block of audio data: %d\n",
					__func__, ret);
				p->unlock(p);
				break;
			} else if (ret > 0)
				total += bytes_to_read;
			p->unlock(p);
		} else
			usleep_range(10000, 11000);

	} while (p->va_flags.buffering);

	dev_info(p->dev, "%s: audio buffer read, total of %u bytes\n",
		__func__, total);

out:
	p->lock(p);
	p->va_flags.buffering = 0;
	p->va_flags.mode = DBMD2_IDLE;
	ret = dbmd2_set_mode(p, DBMD2_IDLE);
	if (ret) {
		dev_err(p->dev, "%s: failed to set device to idle mode\n",
			__func__);
		goto out_fail_unlock;
	}

	/* finish audio buffering (e.g. reduce speed) */
	ret = p->chip->finish_buffering(p);
	if (ret) {
		dev_err(p->dev, "%s: failed to finish buffering\n", __func__);
		goto out_fail_unlock;
	}
out_fail_unlock:
	p->unlock(p);
	dev_dbg(p->dev, "%s done\n", __func__);
}

static irqreturn_t dbmd2_sv_interrupt_hard(int irq, void *dev)
{
	struct dbmd2_private *p = (struct dbmd2_private *)dev;

	if (p && (p->device_ready) && (p->va_flags.irq_inuse))

		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}


static irqreturn_t dbmd2_sv_interrupt_soft(int irq, void *dev)
{
	struct dbmd2_private *p = (struct dbmd2_private *)dev;

	dev_dbg(p->dev, "%s\n", __func__);

	if ((p->device_ready) && (p->va_flags.irq_inuse)) {
		p->chip->transport_enable(p, true);

		schedule_work(&p->uevent_work);

		p->va_flags.buffering = 1;
		schedule_work(&p->sv_work);

		dev_info(p->dev, "%s - SV EVENT\n", __func__);
	}

	return IRQ_HANDLED;
}

static int dbmd2_fw_load_thread(void *data)
{
	struct dbmd2_private *p = (struct dbmd2_private *)data;

	if (p->pdata->feature_va && p->pdata->feature_vqe &&
	    p->pdata->feature_fw_overlay)
		return dbmd2_request_and_load_fw(p, 1, 0, 1);
	else if (p->pdata->feature_va && p->pdata->feature_vqe)
		return dbmd2_request_and_load_fw(p, 1, 1, 0);
	else if (p->pdata->feature_vqe)
		return dbmd2_request_and_load_fw(p, 0, 1, 0);
	else if (p->pdata->feature_va)
		return dbmd2_request_and_load_fw(p, 1, 0, 0);
	return -EINVAL;
}


static int dbmd2_common_probe(struct dbmd2_private *p)
{
#ifdef CONFIG_OF
	struct device_node *np = p->dev->of_node;
#endif
	struct task_struct *boot_thread;
	int ret = 0;
	int fclk;

	dbd2_data = p;
	dev_set_drvdata(p->dev, p);

	/* enable constant clock */
	p->clk_enable(p, DBMD2_CLK_CONSTANT);

	p->amodel_buf = vmalloc(MAX_AMODEL_SIZE);
	if (!p->amodel_buf)
		goto err;

	atomic_set(&p->audio_owner, 0);

#ifdef CONFIG_OF
	if (!np) {
		dev_err(p->dev, "%s: error no devicetree entry\n", __func__);
		ret = -ENODEV;
		goto err_vfree;
	}
#endif

#ifdef CONFIG_OF
	/* reset */
	p->pdata->gpio_reset = of_get_named_gpio(np, "reset-gpio", 0);
#endif
	if (!gpio_is_valid(p->pdata->gpio_reset)) {
		dev_err(p->dev, "%s: reset gpio invalid\n", __func__);
		ret = -EINVAL;
		goto err_vfree;
	}

	ret = gpio_request(p->pdata->gpio_reset, "DBMD2 reset");
	if (ret < 0) {
		dev_err(p->dev, "%s: error requesting reset gpio\n", __func__);
		goto err_vfree;
	}
	gpio_direction_output(p->pdata->gpio_reset, 0);
	gpio_set_value(p->pdata->gpio_reset, 0);

	/* sv */
#ifdef CONFIG_OF
	p->pdata->gpio_sv = of_get_named_gpio(np, "sv-gpio", 0);
#endif
	if (!gpio_is_valid(p->pdata->gpio_sv)) {
		dev_err(p->dev, "%s: sv gpio invalid %d\n",
			__func__, p->pdata->gpio_sv);
		goto err_gpio_free;
	}

	ret = gpio_request(p->pdata->gpio_sv, "DBMD2 sv");
	if (ret < 0) {
		dev_err(p->dev, "%s: error requesting sv gpio\n", __func__);
		goto err_gpio_free;
	}
	gpio_direction_input(p->pdata->gpio_sv);

	/* interrupt gpio */
	p->sv_irq = ret = gpio_to_irq(p->pdata->gpio_sv);
	if (ret < 0) {
		dev_err(p->dev, "%s: cannot map gpio to irq\n", __func__);
		goto err_gpio_free2;
	}

	/* d2 strap 1 - only on some boards */
#ifdef CONFIG_OF
	p->pdata->gpio_d2strap1 = of_get_named_gpio(np, "d2-strap1", 0);
#endif
	if (gpio_is_valid(p->pdata->gpio_d2strap1)) {
		dev_info(p->dev,
			"%s: valid d2 strap1 gpio: %d\n",
			__func__,
			p->pdata->gpio_d2strap1);
		ret = gpio_request(p->pdata->gpio_d2strap1, "DBMD2 STRAP 1");
		if (ret < 0) {
			dev_err(p->dev, "%s: error requesting d2 strap1 gpio\n",
				__func__);
			goto err_gpio_free3;
		} else {
			/* keep it as input */
			if (gpio_direction_input(p->pdata->gpio_d2strap1)) {
				dev_err(p->dev,
					"%s: error setting d2 strap1 gpio to input\n",
					__func__);
				goto err_gpio_free3;
			}
#ifdef CONFIG_OF
			ret = of_property_read_u32(np, "d2-strap1-rst-val",
					&p->pdata->gpio_d2strap1_rst_val);
			if (ret) {
				dev_dbg(p->dev,
					"%s: no d2-strap1-rst-val in dts\n",
					__func__);
				p->pdata->gpio_d2strap1_rst_val = 0;
			}

			/* normalize */
			if (p->pdata->gpio_d2strap1_rst_val > 1)
				p->pdata->gpio_d2strap1_rst_val = 1;
#endif
		}
	} else {
		dev_info(p->dev,
			"%s: missing or invalid d2 strap1 gpio: %d\n",
			__func__,
			p->pdata->gpio_d2strap1);
		p->pdata->gpio_d2strap1 = 0;
	}

	/* wakeup */
#ifdef CONFIG_OF
	p->pdata->gpio_wakeup = of_get_named_gpio(np, "wakeup-gpio", 0);
#endif
	if (!gpio_is_valid(p->pdata->gpio_wakeup)) {
		dev_info(p->dev, "%s: wakeup gpio not specified\n", __func__);
	} else {
		ret = gpio_request(p->pdata->gpio_wakeup, "DBMD2 wakeup");
		if (ret < 0) {
			dev_err(p->dev, "%s: error requesting wakeup gpio\n",
				__func__);
			goto err_gpio_free3;
		}
		/* keep the wakeup pin high */
		gpio_direction_output(p->pdata->gpio_wakeup, 1);
	}

	INIT_WORK(&p->sv_work, dbmd2_sv_work);
	INIT_WORK(&p->uevent_work, dbmd2_uevent_work);
	mutex_init(&p->p_lock);
	ret = kfifo_alloc(&p->pcm_kfifo, MAX_KFIFO_BUFFER_SIZE, GFP_KERNEL);
	if (ret) {
		dev_err(p->dev, "%s: no kfifo memory\n", __func__);
		goto err_gpio_free4;
	}

	fclk = p->clk_get_rate(p, DBMD2_CLK_MASTER);
	switch (fclk) {
	case 32768:
		p->clk_type = DBMD2_XTAL_FREQ_32K_IMG7;
		break;
	case 9600000:
		p->clk_type = DBMD2_XTAL_FREQ_9M_IMG4;
		break;
	case 24000000:
	default:
		p->clk_type = DBMD2_XTAL_FREQ_24M_IMG1;
	}

	/* default mode is PCM mode */
	p->audio_mode = DBD2_AUDIO_MODE_PCM;

	/* Switch ON capture backlog by default */
	p->va_capture_on_detect = true;

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "auto_buffering",
		&p->pdata->auto_buffering);
	if ((ret && ret != -EINVAL) ||
	    (p->pdata->auto_buffering != 0 &&
	    p->pdata->auto_buffering != 1)) {
		dev_err(p->dev, "%s: invalid 'auto_buffering'\n", __func__);
		goto err_kfifo_free;
	}

	ret = of_property_read_u32(np, "auto_detection",
		&p->pdata->auto_detection);
	if ((ret && ret != -EINVAL) ||
			(p->pdata->auto_detection != 0 &&
				p->pdata->auto_detection != 1)) {
		dev_err(p->dev, "%s: invalid 'auto_detection'\n", __func__);
		goto err_kfifo_free;
	}

	ret = of_property_read_u32(np, "uart_low_speed_enabled",
		&p->pdata->uart_low_speed_enabled);
	if ((ret && ret != -EINVAL) ||
			(p->pdata->uart_low_speed_enabled != 0 &&
				p->pdata->uart_low_speed_enabled != 1)) {
		dev_err(p->dev, "%s: invalid 'uart_low_speed_enabled'\n",
			__func__);
		goto err_kfifo_free;
	}

#endif

	p->va_flags.irq_inuse = 0;
	ret = request_threaded_irq(p->sv_irq,
				  dbmd2_sv_interrupt_hard,
				  dbmd2_sv_interrupt_soft,
				  IRQF_TRIGGER_RISING,
				  "dbmd2_sv", p);


	if (ret < 0) {
		dev_err(p->dev, "%s: cannot get irq\n", __func__);
		goto err_kfifo_free;
	}

	ret = irq_set_irq_wake(p->sv_irq, 1);
	if (ret < 0) {
		dev_err(p->dev, "%s: cannot set irq_set_irq_wake\n", __func__);
		goto err_free_irq;
	}

	p->ns_class = class_create(THIS_MODULE, "voice_trigger");
	if (IS_ERR(p->ns_class)) {
		dev_err(p->dev, "%s: failed to create class\n", __func__);
		goto err_free_irq;
	}

	p->dbmd2_dev = device_create(p->ns_class, NULL, 0, p, "dbd2");
	if (IS_ERR(p->dbmd2_dev)) {
		dev_err(p->dev, "%s: could not create device\n", __func__);
		goto err_class_destroy;
	}

	p->gram_dev = device_create(p->ns_class, NULL, 0, p, "gram");
	if (IS_ERR(p->gram_dev)) {
		dev_err(p->dev, "%s: could not create device\n", __func__);
		goto err_class_destroy;
	}

	p->net_dev = device_create(p->ns_class, NULL, 0, p, "net");
	if (IS_ERR(p->net_dev)) {
		dev_err(p->dev, "%s: could not create device\n", __func__);
		goto err_class_destroy;
	}

	ret = sysfs_create_group(&p->dbmd2_dev->kobj, &dbmd2_attribute_group);
	if (ret) {
		dev_err(p->dbmd2_dev, "%s: failed to create sysfs group\n",
			__func__);
		goto err_device_unregister;
	}

	ret = dbmd2_register_cdev(p);
	if (ret)
		goto err_sysfs_remove_group;

	/* register the codec */
	ret = snd_soc_register_codec(p->dev,
				     &soc_codec_dev_dbmd2,
				     dbmd2_dais,
				     ARRAY_SIZE(dbmd2_dais));
	if (ret != 0) {
		dev_err(p->dev,
			"%s: Failed to register codec and its DAI: %d\n",
			__func__, ret);
		goto err_device_unregister2;
	}

	boot_thread = kthread_run(dbmd2_fw_load_thread,
				  (void *)p,
				  "dbmd2 probe thread");
	if (IS_ERR_OR_NULL(boot_thread)) {
		dev_err(p->dev,
			"%s: Cannot create DBMD2 boot thread\n", __func__);
		ret = -EIO;
		goto err_codec_unregister;
	}

	dev_info(p->dev, "%s: registered DBMD2 codec driver version = %s\n",
		__func__, DRIVER_VERSION);

	return 0;

err_codec_unregister:
	snd_soc_unregister_codec(p->dev);
err_device_unregister2:
	device_unregister(p->record_dev);
err_sysfs_remove_group:
	sysfs_remove_group(&p->dev->kobj, &dbmd2_attribute_group);
err_device_unregister:
	device_unregister(p->dbmd2_dev);
err_class_destroy:
	class_destroy(p->ns_class);
err_free_irq:
	p->va_flags.irq_inuse = 0;
	irq_set_irq_wake(p->sv_irq, 0);
	free_irq(p->sv_irq, p);
err_kfifo_free:
	kfifo_free(&p->pcm_kfifo);
err_gpio_free4:
	if (p->pdata->gpio_wakeup >= 0)
		gpio_free(p->pdata->gpio_wakeup);
err_gpio_free3:
	if (p->pdata->gpio_d2strap1)
		gpio_free(p->pdata->gpio_d2strap1);
err_gpio_free2:
	gpio_free(p->pdata->gpio_sv);
err_gpio_free:
	gpio_free(p->pdata->gpio_reset);
err_vfree:
	vfree(p->amodel_buf);
err:
	/* disable constant clock */
	p->clk_disable(p, DBMD2_CLK_CONSTANT);
	return ret;
}

static void dbmd2_common_remove(struct dbmd2_private *p)
{
	flush_workqueue(p->dbmd2_workq);
	destroy_workqueue(p->dbmd2_workq);
	snd_soc_unregister_codec(p->dev);
	dbmd2_deregister_cdev(p);
	sysfs_remove_group(&p->dev->kobj, &dbmd2_attribute_group);
	device_unregister(p->dbmd2_dev);
	class_destroy(p->ns_class);
	kfifo_free(&p->pcm_kfifo);
	p->va_flags.irq_inuse = 0;
	disable_irq(p->sv_irq);
	irq_set_irq_wake(p->sv_irq, 0);
	free_irq(p->sv_irq, p);
	if (p->pdata->gpio_wakeup >= 0)
		gpio_free(p->pdata->gpio_wakeup);
	gpio_free(p->pdata->gpio_reset);
	gpio_free(p->pdata->gpio_sv);
	kfifo_free(&p->pcm_kfifo);
	vfree(p->amodel_buf);
#ifdef CONFIG_OF
	if (p->pdata->vqe_cfg_values)
		kfree(p->pdata->vqe_cfg_value);
	if (p->pdata->vqe_modes_values)
		kfree(p->pdata->vqe_modes_value);
	if (p->pdata->va_cfg_values)
		kfree(p->pdata->va_cfg_value);
#endif
	/* disable constant clock */
	p->clk_disable(p, DBMD2_CLK_CONSTANT);
	if (p->constant_clk)
		clk_put(p->constant_clk);
	if (p->master_clk)
		clk_put(p->master_clk);
	kfree(p);
}

#ifdef CONFIG_OF
static int of_dev_node_match(struct device *dev, void *data)
{
	return dev->of_node == data;
}

/* must call put_device() when done with returned i2c_client device */
struct platform_device *of_find_platform_device_by_node(
	struct device_node *node)
{
	struct device *dev;

	dev = bus_find_device(&platform_bus_type, NULL, node,
			      of_dev_node_match);
	if (!dev)
		return NULL;

	return container_of(dev, struct platform_device, dev);
}

struct spi_device *of_find_spi_device_by_node(struct device_node *node)
{
	struct device *dev;
	dev = bus_find_device(&spi_bus_type, NULL, node, of_dev_node_match);

	if (!dev)
		return NULL;

	return to_spi_device(dev);
}

static int dbmd2_get_devtree_pdata(struct device *dev,
		struct dbmd2_private *p)
{
	struct dbd2_platform_data *pdata = p->pdata;
	struct device_node *np;
	struct property *property = NULL;
	int ret = 0;
	int len;
	int i;

	np = dev->of_node;

	/* read file names for the various firmwares */
	/* read name of VA firmware */
	ret = of_property_read_string(np,
				      "va-firmware-name",
				      &pdata->va_firmware_name);
	if (ret != 0) {
		/* set default name */
		pdata->va_firmware_name = DBMD2_VA_FIRMWARE_NAME;
		dev_info(dev, "%s: using default VA firmware name: %s\n",
			 __func__, pdata->va_firmware_name);
	} else {
		dev_info(dev, "%s: using device-tree VA firmware name: %s\n",
			 __func__, pdata->va_firmware_name);
	}

	/* read name of VQE firmware, overlay */
	ret = of_property_read_string(np,
				      "vqe-firmware-name",
				      &pdata->vqe_firmware_name);
	if (ret != 0) {
		/* set default name */
		pdata->vqe_firmware_name = DBMD2_VQE_FIRMWARE_NAME;
		dev_info(dev, "%s: using default VQE firmware name: %s\n",
			__func__, pdata->vqe_firmware_name);
	} else {
		dev_info(dev, "%s: using device-tree VQE firmware name: %s\n",
			__func__, pdata->vqe_firmware_name);
	}

	/* read name of VQE firmware, non-overlay */
	ret = of_property_read_string(np,
				      "vqe-non-overlay-firmware-name",
				      &pdata->vqe_non_overlay_firmware_name);
	if (ret != 0) {
		/* set default name */
		pdata->vqe_non_overlay_firmware_name =
			DBMD2_VQE_FIRMWARE_NAME;
		dev_info(dev,
			"%s: using default VQE non-overlay firmware name: %s\n",
			 __func__, pdata->vqe_non_overlay_firmware_name);
	} else {
		dev_info(dev,
			"%s: using device-tree VQE non-overlay firmware name: %s\n",
			 __func__, pdata->vqe_non_overlay_firmware_name);
	}

	/* check for features */
	if (of_find_property(np, "feature-va", NULL)) {
		dev_info(dev, "%s: VA feature activated\n", __func__);
		pdata->feature_va = 1;
	}
	if (of_find_property(np, "feature-vqe", NULL)) {
		dev_info(dev, "%s: VQE feature activated\n", __func__);
		pdata->feature_vqe = 1;
	}
	if (of_find_property(np, "feature-firmware-overlay", NULL)) {
		dev_info(dev, "%s: Firmware overlay activated\n", __func__);
		pdata->feature_fw_overlay = 1;
	}

	/* check if enabled features make sense */
	if (!pdata->feature_va && !pdata->feature_vqe) {
		dev_err(dev, "%s: No feature activated\n", __func__);
		ret = -EINVAL;
		goto err_kfree;
	}

	property = of_find_property(np, "va-config", &pdata->va_cfg_values);
	if (property) {
		pdata->va_cfg_value = kzalloc(pdata->va_cfg_values, GFP_KERNEL);
		if (!pdata->va_cfg_value) {
			dev_err(dev, "%s: No memory for VA configuration\n",
				__func__);
			ret = -ENOMEM;
			goto err_kfree;
		}
		pdata->va_cfg_values /= sizeof(u32);
		ret = of_property_read_u32_array(np,
						 "va-config",
						 pdata->va_cfg_value,
						 pdata->va_cfg_values);
		if (ret) {
			dev_err(dev, "%s: Could not read VA configuration\n",
				__func__);
			ret = -EIO;
			goto err_kfree2;
		}
		dev_info(dev,
			"%s: using %u VA configuration values from device-tree\n",
			__func__,
			pdata->va_cfg_values);
		for (i = 0; i < pdata->va_cfg_values; i++)
			dev_dbg(dev, "%s: VA cfg %8.8x: 0x%8.8x\n",
				__func__, i, pdata->va_cfg_value[i]);
	}

	property = of_find_property(np, "va-speeds", &len);
	if (property) {
		if (len < DBMD2_VA_NR_OF_SPEEDS * 4) {
			dev_err(dev,
				"%s: VA speed configuration table too short\n",
				__func__);
			ret = -ENOMEM;
			goto err_kfree2;
		}
		ret = of_property_read_u32_array(np,
						 "va-speeds",
						 (u32 *)&pdata->va_speed_cfg,
						 DBMD2_VA_NR_OF_SPEEDS * 4);
		if (ret) {
			dev_err(dev,
				"%s: Could not read VA speed configuration\n",
				__func__);
			ret = -EIO;
			goto err_kfree2;
		}
		dev_info(dev,
			"%s: using %u VA speed configuration values from device-tree\n",
			__func__,
			DBMD2_VA_NR_OF_SPEEDS);
		for (i = 0; i < DBMD2_VA_NR_OF_SPEEDS; i++)
			dev_dbg(dev, "%s: VA speed cfg %8.8x: 0x%8.8x %u %u %u\n",
				__func__,
				i,
				pdata->va_speed_cfg[i].cfg,
				pdata->va_speed_cfg[i].uart_baud,
				pdata->va_speed_cfg[i].i2c_rate,
				pdata->va_speed_cfg[i].spi_rate);
	}

	property = of_find_property(np, "vqe-config", &pdata->vqe_cfg_values);
	if (property) {
		pdata->vqe_cfg_value = kzalloc(pdata->vqe_cfg_values,
					GFP_KERNEL);
		if (!pdata->vqe_cfg_value) {
			dev_err(dev, "%s: No memory for VQE configuration\n",
				__func__);
			ret = -ENOMEM;
			goto err_kfree2;
		}
		pdata->vqe_cfg_values /= sizeof(u32);
		ret = of_property_read_u32_array(np,
						 "vqe-config",
						 pdata->vqe_cfg_value,
						 pdata->vqe_cfg_values);
		if (ret) {
			dev_err(dev, "%s: Could not read VQE configuration\n",
				__func__);
			ret = -EIO;
			goto err_kfree3;
		}
		dev_info(dev,
			"%s: using %u VQE configuration values from device-tree\n",
			__func__,
			pdata->vqe_cfg_values);
		for (i = 0; i < pdata->vqe_cfg_values; i++)
			dev_dbg(dev, "%s: VQE cfg %8.8x: 0x%8.8x\n",
				__func__, i, pdata->vqe_cfg_value[i]);
	}

	property = of_find_property(np, "vqe-modes", &pdata->vqe_modes_values);
	if (property) {
		pdata->vqe_modes_value = kzalloc(pdata->vqe_modes_values,
					GFP_KERNEL);
		if (!pdata->vqe_modes_value) {
			dev_err(dev, "%s: No memory for VQE modes\n",
				__func__);
			ret = -ENOMEM;
			goto err_kfree3;
		}
		pdata->vqe_modes_values /= sizeof(u32);
		ret = of_property_read_u32_array(np,
						 "vqe-modes",
						 pdata->vqe_modes_value,
						 pdata->vqe_modes_values);
		if (ret) {
			dev_err(dev, "%s: Could not read VQE modes\n",
				__func__);
			ret = -EIO;
			goto err_kfree4;
		}
		dev_info(dev,
			"%s: using %u VQE modes values from device-tree\n",
			__func__,
			pdata->vqe_modes_values);
		for (i = 0; i < pdata->vqe_modes_values; i++)
			dev_dbg(dev, "%s: VQE mode %8.8x: 0x%8.8x\n",
				__func__, i, pdata->vqe_modes_value[i]);
	}

	of_property_read_u32(np,
			     "vqe-tdm-bypass-config",
			     &pdata->vqe_tdm_bypass_config);
	dev_info(dev, "%s: VQE TDM bypass config: 0x%8.8x\n",
		__func__,
		pdata->vqe_tdm_bypass_config);
	pdata->vqe_tdm_bypass_config &= 0x7;

	ret = of_property_read_u32_array(np,
					 "va-mic-config",
					 pdata->va_mic_config,
					 3);
	if (ret != 0) {
		dev_err(p->dev, "%s: invalid or missing 'va-mic-config'\n",
			__func__);
		goto err_kfree3;
	}
	dev_info(dev,
		"%s: using %u VA mic configuration values from device-tree\n",
		__func__, 3);
	for (i = 0; i < 3; i++)
		dev_dbg(dev, "%s: VA mic cfg %8.8x: 0x%8.8x\n",
			__func__, i, pdata->va_mic_config[i]);

	ret = of_property_read_u32(np,
				   "va-mic-mode",
				   &pdata->va_initial_mic_config);
	if (ret != 0) {
		dev_err(p->dev, "%s: invalid or missing 'va-mic-mode'\n",
			__func__);
		goto err_kfree3;
	}
	dev_info(dev, "%s: VA default mic config: 0x%8.8x\n",
		 __func__, pdata->va_initial_mic_config);

	ret = of_property_read_u32(np,
				   "va_backlog_length",
				   &p->va_backlog_length);
	if (ret != 0) {
		dev_info(dev,
			"%s: no va_backlog_length definition in device-tree\n",
			__func__);
		p->va_backlog_length = 1;
	} else {
		if (p->va_backlog_length > 0xfff)
			p->va_backlog_length = 0xfff;
		dev_info(dev,
			"%s: using backlog length of %d from device-tree\n",
			 __func__, p->va_backlog_length);
	}

	/*
	 * check for master-clk frequency in device-tree
	 */
	ret = of_property_read_u32(np,
				   "master-clk-rate",
				   &pdata->master_clk_rate);
	if (ret != 0) {
		dev_info(dev,
			"%s: L no master-clk-rate definition in device-tree\n",
			 __func__);
		pdata->master_clk_rate = -1;
	} else
		dev_info(dev,
			"%s: using master-clk-rate of %dHZ from device-tree\n",
			__func__,
			 pdata->master_clk_rate);

	/* gather clock information */
	p->master_clk = clk_get(p->dev, "dbmd2_master_clk");
	if (IS_ERR(p->master_clk)) {
		dev_info(dev,
			"%s: no master-clk definition in device-tree\n",
			__func__);
		/* nothing in the device tree */
		p->master_clk = NULL;
	} else {
		/* If clock rate not specified in dts, try to detect */
		if (pdata->master_clk_rate == -1) {
			pdata->master_clk_rate = clk_get_rate(p->master_clk);
			dev_info(dev,
				"%s: using master-clk %dHZ from real clock\n",
				__func__,
				pdata->master_clk_rate);
		}
	}

	/*
	 * check for constant-clk frequency in device-tree
	 */
	ret = of_property_read_u32(np,
				   "constant-clk-rate",
				   &pdata->constant_clk_rate);
	if (ret != 0) {
		dev_info(dev,
			"%s: no constant-clk-rate definition in device-tree\n",
			__func__);
		pdata->constant_clk_rate = -1;
	} else
		dev_info(dev,
			"%s: using constant-clk-rate %dHZ from device-tree\n",
			 __func__, pdata->constant_clk_rate);

	p->constant_clk = clk_get(p->dev, "dbmd2_constant_clk");
	if (IS_ERR(p->constant_clk)) {
		dev_info(dev,
			"%s: no constant-clk definition in device-tree\n",
			__func__);
		/* nothing in the device tree */
		p->constant_clk = NULL;
	} else {
		/* If clock rate not specified in dts, try to detect */
		if (pdata->constant_clk_rate == -1) {
			pdata->constant_clk_rate =
			    clk_get_rate(p->constant_clk);
			dev_info(dev,
				"%s: using const clk %dHZ from real clock\n",
				__func__,
				pdata->constant_clk_rate);
		}
	}

	return 0;
err_kfree4:
	if (pdata->vqe_modes_values)
		kfree(pdata->vqe_modes_value);
	pdata->vqe_modes_values = 0;
err_kfree3:
	if (pdata->vqe_cfg_values)
		kfree(pdata->vqe_cfg_value);
	pdata->vqe_cfg_values = 0;
err_kfree2:
	if (pdata->va_cfg_values)
		kfree(pdata->va_cfg_value);
	pdata->va_cfg_values = 0;
err_kfree:
	kfree(p);
	return ret;
}
#else
static int dbmd2_name_match(struct device *dev, void *dev_name)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (!pdev || !pdev->name)
		return 0;

	return !strcmp(pdev->name, dev_name);
}

struct i2c_client *dbmd2_find_i2c_device_by_name(char *dev_name)
{
	struct device *dev;

	dev = bus_find_device(&i2c_bus_type, NULL, dev_name,
					 dbmd2_name_match);
	if (!dev)
		return NULL;

	return to_i2c_client(dev);
}

struct platform_device *dbmd2_find_platform_device_by_name(char *dev_name)
{
	struct device *dev;

	dev = bus_find_device(&platform_bus_type, NULL,
				dev_name, dbmd2_name_match);
	return dev ? to_platform_device(dev) : NULL;
}
#endif

static int dbmd2_platform_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cmd_interface_np;
#endif
	struct dbd2_platform_data *pdata;
	struct dbmd2_private *p;
	int ret = 0;
	struct spi_device *spi_client;
	struct i2c_client *i2c_client;
	struct platform_device *uart_client;
	struct chip_interface *chip = NULL;

	dev_info(&pdev->dev, "%s: DBMD2 codec driver version = %s\n",
		__func__, DRIVER_VERSION);

#ifdef CONFIG_OF
	cmd_interface_np = of_parse_phandle(np, "cmd-interface", 0);
	if (!cmd_interface_np) {
		dev_err(&pdev->dev, "%s: invalid command interface node\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}
#endif

#ifdef CONFIG_OF
	i2c_client = of_find_i2c_device_by_node(cmd_interface_np);
#else
	i2c_client = dbmd2_find_i2c_device_by_name("dbmd2-i2c");
#endif
	if (i2c_client) {
		/* got I2C command interface */
		chip = i2c_get_clientdata(i2c_client);
		if (!chip)
			return -EPROBE_DEFER;
	}
#ifdef CONFIG_OF
	uart_client = of_find_platform_device_by_node(cmd_interface_np);
#else
	uart_client = dbmd2_find_platform_device_by_name("dbmd2-uart");
#endif
	if (uart_client) {
		/* got UART command interface */
		chip = dev_get_drvdata(&uart_client->dev);
		if (!chip)
			return -EPROBE_DEFER;
	}

#ifdef CONFIG_OF
	spi_client = of_find_spi_device_by_node(cmd_interface_np);
#else
	spi_client = 0; /* TODO: add support for no-dts */
#endif
	if (spi_client) {
		/* got spi command interface */
			dev_err(&pdev->dev,
				"%s: spi interface node %p\n",
				__func__, spi_client);

		chip = spi_get_drvdata(spi_client);
		if (!chip)
			return -EPROBE_DEFER;
	}

	if (!chip) {
		/* unknown command interface */
		dev_err(&pdev->dev, "%s: unknown command interface\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL) {
		ret = -ENOMEM;
		goto out;
	}

#ifdef CONFIG_OF
	pdata = kzalloc(sizeof(struct dbd2_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: Failed to allocate memory\n",
			__func__);
		goto out;
	}

	p->pdata = pdata;

	ret = dbmd2_get_devtree_pdata(&pdev->dev, p);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to find device tree\n",
			__func__);
		kfree(pdata);
		goto out;
	}
#else
	pdata = dev_get_platdata(&pdev->dev);
	p->pdata = pdata;
#endif

	p->dev = &pdev->dev;

	/* set initial mic as it appears in the platform data */
	p->va_current_mic_config = pdata->va_initial_mic_config;

	p->vqe_vc_syscfg = DBMD2_VQE_SET_SYSTEM_CONFIG_SECONDARY_CFG;

	/* initialize delayed pm work */
	INIT_DELAYED_WORK(&p->delayed_pm_work,
			  dbmd2_delayed_pm_work_hibernate);
	p->dbmd2_workq = create_workqueue("dbmd2-wq");
	if (!p->dbmd2_workq) {
		dev_err(&pdev->dev, "%s: Could not create workqueue\n",
			__func__);
		ret = -EIO;
		goto out;
	}

	/* set helper functions */
	p->reset_set = dbmd2_reset_set;
	p->reset_release = dbmd2_reset_release;
	p->reset_sequence = dbmd2_reset_sequence;
	p->wakeup_set = dbmd2_wakeup_set;
	p->wakeup_release = dbmd2_wakeup_release;
	p->lock = dbmd2_lock;
	p->unlock = dbmd2_unlock;
	p->verify_checksum = dbmd2_verify_checksum;
	p->va_set_speed = dbmd2_va_set_speed;
	p->clk_get_rate = dbmd2_clk_get_rate;
	p->clk_enable = dbmd2_clk_enable;
	p->clk_disable = dbmd2_clk_disable;

	/* set callbacks (if already set externally) */
	if (g_set_i2c_freq_callback)
		p->set_i2c_freq_callback = g_set_i2c_freq_callback;
	if (g_event_callback)
		p->event_callback = g_event_callback;

	/* set chip interface */
	p->chip = chip;

	p->rxsize = MAX_REQ_SIZE;
	p->rsize = MAX_READ_SZ;
	p->wsize = MAX_WRITE_SZ;

	ret = dbmd2_common_probe(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: probe failed\n", __func__);
		goto out;
	}

	dev_info(p->dev, "%s: successfully probed\n", __func__);
	return 0;
out:
	return ret;
}

static int dbmd2_platform_remove(struct platform_device *pdev)
{
	struct dbmd2_private *p = dev_get_drvdata(&pdev->dev);

	dbmd2_common_remove(p);

	return 0;
}

static struct of_device_id dbmd2_of_match[] = {
	{ .compatible = "dspg,dbmd2-codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(of, dbmd2_of_match);

static struct platform_driver dbmd2_platform_driver = {
	.driver = {
		.name = "dbmd2-codec",
		.owner = THIS_MODULE,
		.of_match_table = dbmd2_of_match,
	},
	.probe =    dbmd2_platform_probe,
	.remove =   dbmd2_platform_remove,
};

static int __init dbmd2_modinit(void)
{
	return platform_driver_register(&dbmd2_platform_driver);
}
module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	platform_driver_unregister(&dbmd2_platform_driver);
}
module_exit(dbmd2_exit);

MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION("DSPG DBMD2 codec driver");
MODULE_LICENSE("GPL");
