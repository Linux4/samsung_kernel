/*
 * DSPG DBMDX codec driver
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

/* #define DEBUG */

#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#if IS_ENABLED(CONFIG_OF_I2C)
#include <linux/of_i2c.h>
#endif /* CONFIG_OF_I2C */
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */
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
#include <linux/regulator/consumer.h>
#include <linux/kthread.h>
#include <linux/version.h>

#include "dbmdx-interface.h"
#include "dbmdx-customer.h"
#include "dbmdx-va-regmap.h"
#include "dbmdx-vqe-regmap.h"
#include "dbmdx-i2s.h"
#include <sound/dbmdx-export.h>

/* Size must be power of 2 */
#define MAX_KFIFO_BUFFER_SIZE_MONO		(32768 * 8) /* >8 seconds */
#define MAX_KFIFO_BUFFER_SIZE_STEREO		(MAX_KFIFO_BUFFER_SIZE_MONO * 2)
#define MAX_RETRIES_TO_WRITE_TOBUF		200
#define MAX_AMODEL_SIZE				(148 * 1024)

#define DRIVER_VERSION				"4.015"

#define DBMDX_AUDIO_MODE_PCM			0
#define DBMDX_AUDIO_MODE_MU_LAW			1

#define DBMDX_SND_PCM_RATE_16000		0xfcff
#define DBMDX_SND_PCM_RATE_32000		0xfdff
#define DBMDX_SND_PCM_RATE_48000		0xfeff
#define DBMDX_HW_VAD_MASK			0x00e0

#define DIGITAL_GAIN_TLV_MIN			0
#define DIGITAL_GAIN_TLV_MAX			240
#define DIGITAL_GAIN_TLV_SHIFT			120

#define MIN_EVENT_PROCESSING_TIME_MS		500

enum dbmdx_detection_mode {
	DETECTION_MODE_PHRASE = 1,
	DETECTION_MODE_VOICE_ENERGY,
	DETECTION_MODE_VOICE_COMMAND,
	DETECTION_MODE_DUAL,
	DETECTION_MODE_PHRASE_DONT_LOAD,
	DETECTION_MODE_MAX = DETECTION_MODE_PHRASE_DONT_LOAD
};

enum dbmdx_fw_debug_mode {
	FW_DEBUG_OUTPUT_UART = 0,
	FW_DEBUG_OUTPUT_CLK,
	FW_DEBUG_OUTPUT_CLK_UART,
	FW_DEBUG_OUTPUT_NONE
};

#define VA_MIXER_REG(cmd)			\
	(((cmd) >> 16) & 0x7fff)
#define VQE_MIXER_REG(cmd)			\
	(((cmd) >> 16) & 0xffff)


static const char *dbmdx_power_mode_names[DBMDX_PM_STATES] = {
	"BOOTING",
	"ACTIVE",
	"FALLING_ASLEEP",
	"SLEEPING",
};

static const char *dbmdx_state_names[DBMDX_NR_OF_STATES] = {
	"IDLE",
	"DETECTION",
	"RESERVED_2",
	"BUFFERING",
	"SLEEP_PLL_ON",
	"SLEEP_PLL_OFF",
	"HIBERNATE",
	"PCM_STREAMING",
	"DETECTION_AND_STREAMING",
};

/* Global Variables */
struct dbmdx_private *dbmdx_data;
void (*g_event_callback)(int) = NULL;
void (*g_set_i2c_freq_callback)(struct i2c_adapter*, enum i2c_freq_t) = NULL;

/* Forward declarations */
static int dbmdx_perform_recovery(struct dbmdx_private *p);
static int dbmdx_disable_microphones(struct dbmdx_private *p);
static int dbmdx_restore_microphones(struct dbmdx_private *p);
static int dbmdx_restore_fw_vad_settings(struct dbmdx_private *p);
static int dbmdx_disable_hw_vad(struct dbmdx_private *p);
static int dbmdx_read_fw_vad_settings(struct dbmdx_private *p);

static const char *dbmdx_fw_type_to_str(int fw_type)
{
	switch (fw_type) {
	case DBMDX_FW_VA: return "VA";
	case DBMDX_FW_VQE: return "VQE";
	case DBMDX_FW_PRE_BOOT: return "PRE_BOOT";
	default: return "ERROR";
	}
}

static void dbmdx_set_va_active(struct dbmdx_private *p)
{
	/* set VA as active firmware */
	p->active_fw = DBMDX_FW_VA;
	/* reset all flags */
	memset(&p->va_flags, 0, sizeof(p->va_flags));
	memset(&p->vqe_flags, 0, sizeof(p->vqe_flags));
}

static void dbmdx_set_vqe_active(struct dbmdx_private *p)
{
	/* set VQE as active firmware */
	p->active_fw = DBMDX_FW_VQE;
	/* reset all flags */
	memset(&p->va_flags, 0, sizeof(p->va_flags));
	memset(&p->vqe_flags, 0, sizeof(p->vqe_flags));
}

static void dbmdx_set_boot_active(struct dbmdx_private *p)
{
	/* set nothing as active firmware */
	p->active_fw = DBMDX_FW_PRE_BOOT;
	p->device_ready = false;
	p->asleep = false;
}

static void dbmdx_reset_set(struct dbmdx_private *p)
{
	if (p->pdata->gpio_d2strap1)
		gpio_direction_output(p->pdata->gpio_d2strap1,
				p->pdata->gpio_d2strap1_rst_val);

	gpio_set_value(p->pdata->gpio_reset, 0);
}

static void dbmdx_reset_release(struct dbmdx_private *p)
{
	gpio_set_value(p->pdata->gpio_reset, 1);

	if (p->pdata->gpio_d2strap1)
		gpio_direction_input(p->pdata->gpio_d2strap1);
}

static void dbmdx_reset_sequence(struct dbmdx_private *p)
{
	dbmdx_reset_set(p);
	usleep_range(300, 400);
	dbmdx_reset_release(p);
}

static int dbmdx_can_wakeup(struct dbmdx_private *p)
{
	/* If use_gpio_for_wakeup equals zero than transmit operation
	itself will wakeup the chip */
	if (!p->pdata->use_gpio_for_wakeup)
		return 1;

	return (p->pdata->gpio_wakeup < 0 ? 0 : 1);
}

static void dbmdx_wakeup_set(struct dbmdx_private *p)
{
	/* If use_gpio_for_wakeup equals zero than transmit operation
	itself will wakeup the chip */
	if (p->pdata->gpio_wakeup < 0 || !p->pdata->use_gpio_for_wakeup)
		return;
	dev_dbg(p->dev, "%s\n", __func__);
	gpio_set_value(p->pdata->gpio_wakeup, p->pdata->wakeup_set_value);
}

static void dbmdx_wakeup_release(struct dbmdx_private *p)
{
	/* If use_gpio_for_wakeup equals zero than transmit operation
	itself will wakeup the chip */
	if (p->pdata->gpio_wakeup < 0 || !p->pdata->use_gpio_for_wakeup)
		return;
	dev_dbg(p->dev, "%s\n", __func__);
	gpio_set_value(p->pdata->gpio_wakeup, !(p->pdata->wakeup_set_value));
}

static unsigned long dbmdx_clk_get_rate(
		struct dbmdx_private *p, enum dbmdx_clocks clk)
{
	switch (clk) {
	case DBMDX_CLK_CONSTANT:
		if (p->pdata->constant_clk_rate != -1)
			return p->pdata->constant_clk_rate;
		else
			return customer_dbmdx_clk_get_rate(p, clk);
		break;
	case DBMDX_CLK_MASTER:
		if (p->pdata->master_clk_rate != -1)
			return p->pdata->master_clk_rate;
		else
			return customer_dbmdx_clk_get_rate(p, clk);
		break;
	}
	return 0;
}

static int dbmdx_clk_enable(struct dbmdx_private *p, enum dbmdx_clocks clk)
{
	int ret = 0;

	switch (clk) {
	case DBMDX_CLK_CONSTANT:
		if (p->constant_clk)
			ret = clk_prepare_enable(p->constant_clk);
		else
			ret = customer_dbmdx_clk_enable(p, clk);
		break;
	case DBMDX_CLK_MASTER:
		if (p->master_clk)
			ret = clk_prepare_enable(p->master_clk);
		else
			ret = customer_dbmdx_clk_enable(p, clk);
		break;
	}
	if (ret < 0)
		dev_err(p->dev, "%s: %s clock enable failed\n",
			__func__,
			clk == DBMDX_CLK_CONSTANT ? "constant" : "master");
	else
		ret = 0;

	return ret;
}

static int dbmdx_clk_disable(struct dbmdx_private *p, enum dbmdx_clocks clk)
{
	switch (clk) {
	case DBMDX_CLK_CONSTANT:
		if (p->constant_clk)
			clk_disable_unprepare(p->constant_clk);
		else
			customer_dbmdx_clk_disable(p, clk);
		break;
	case DBMDX_CLK_MASTER:
		if (p->master_clk)
			clk_disable_unprepare(p->master_clk);
		else
			customer_dbmdx_clk_disable(p, clk);
		break;
	}
	return 0;
}

static void dbmdx_lock(struct dbmdx_private *p)
{
	mutex_lock(&p->p_lock);
}

static void dbmdx_unlock(struct dbmdx_private *p)
{
	mutex_unlock(&p->p_lock);
}

static int dbmdx_verify_checksum(struct dbmdx_private *p,
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
static ssize_t dbmdx_send_data(struct dbmdx_private *p, const void *buf,
			       size_t len)
{
	return p->chip->write(p, buf, len);
}
#endif

static int dbmdx_send_cmd(struct dbmdx_private *p, u32 command, u16 *response)
{
	int ret;

	switch (p->active_fw) {
	case DBMDX_FW_VA:
		ret = p->chip->send_cmd_va(p, command, response);
		break;
	case DBMDX_FW_VQE:
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

static int dbmdx_va_alive(struct dbmdx_private *p)
{
	u16 result = 0;
	int ret = 0;
	unsigned long stimeout = jiffies + msecs_to_jiffies(1000);

	do {

		/* check if VA firmware is still alive */
		ret = dbmdx_send_cmd(p, DBMDX_VA_FW_ID, &result);
		if (ret < 0)
			continue;
		if (result == p->pdata->firmware_id)
			ret = 0;
		else
			ret = -1;
	} while (time_before(jiffies, stimeout) && ret != 0);

	if (ret != 0)
		dev_err(p->dev, "%s: VA firmware dead\n", __func__);

	return ret;
}

static int dbmdx_va_alive_with_lock(struct dbmdx_private *p)
{
	int ret = 0;

	p->lock(p);

	ret = dbmdx_va_alive(p);

	p->unlock(p);

	return ret;
}

/* Place audio samples to kfifo according to operation flag
	AUDIO_CHANNEL_OP_COPY - copy samples directly to kfifo
	AUDIO_CHANNEL_OP_DUPLICATE_MONO - duplicate mono to channel to dual mono
	AUDIO_CHANNEL_OP_TRUNCATE_PRIMARY - take samples from primary channel
*/
static int dbmdx_add_audio_samples_to_kfifo(struct dbmdx_private *p,
	struct kfifo *fifo,
	const u8 *buf,
	unsigned int buf_length,
	enum dbmdx_audio_channel_operation audio_channel_op)
{

	if (audio_channel_op == AUDIO_CHANNEL_OP_COPY)
		kfifo_in(fifo, buf, buf_length);
	else if (audio_channel_op == AUDIO_CHANNEL_OP_DUPLICATE_MONO) {
		unsigned int i;
		u8 cur_sample_buf[4];
		for (i = 0; i < buf_length - 1; i += 2) {
			cur_sample_buf[0] = buf[i];
			cur_sample_buf[1] = buf[i+1];
			cur_sample_buf[2] = buf[i];
			cur_sample_buf[3] = buf[i+1];
			kfifo_in(fifo, cur_sample_buf, 4);
		}
	} else if (audio_channel_op == AUDIO_CHANNEL_OP_TRUNCATE_PRIMARY) {
		unsigned int i;
		u8 cur_sample_buf[2];
		for (i = 0; i < buf_length - 1; i += 4) {
			cur_sample_buf[0] = buf[i];
			cur_sample_buf[1] = buf[i+1];
			kfifo_in(fifo, cur_sample_buf, 2);
		}
	} else {
		dev_err(p->dev, "%s: Undefined audio channel operation\n",
			__func__);
		return -1;
	}

	return 0;
}
#if defined(CONFIG_SND_SOC_DBMDX_SND_CAPTURE)

static int dbmdx_suspend_pcm_streaming_work(struct dbmdx_private *p)
{
	int ret;
	p->va_flags.pcm_worker_active = 0;
	flush_work(&p->pcm_streaming_work);

	if (p->va_flags.pcm_streaming_active) {

		p->va_flags.pcm_streaming_pushing_zeroes = true;

		ret = dbmdx_set_pcm_timer_mode(p->active_substream, true);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: Error switching to pcm timer mode\n",
			__func__);
			return -1;
		}
		dev_dbg(p->dev,
			"%s: Switched to pcm timer mode (pushing zeroes)\n",
			__func__);
	}

	return 0;
}

#else
static int dbmdx_suspend_pcm_streaming_work(struct dbmdx_private *p)
{
	return 0;
}
#endif

static int dbmdx_vqe_alive(struct dbmdx_private *p)
{
	unsigned long timeout;
	int ret = -EIO;
	u16 resp;

	usleep_range(DBMDX_USLEEP_VQE_ALIVE,
		DBMDX_USLEEP_VQE_ALIVE + 1000);

	timeout = jiffies + msecs_to_jiffies(1000);
	while (time_before(jiffies, timeout)) {
		ret = dbmdx_send_cmd(p,
				     DBMDX_VQE_SET_PING_CMD | 0xaffe,
				     &resp);
		if (ret == 0 && resp == 0xaffe)
			break;
		usleep_range(DBMDX_USLEEP_VQE_ALIVE_ON_FAIL,
			DBMDX_USLEEP_VQE_ALIVE_ON_FAIL + 1000);
	}
	if (ret != 0)
		dev_dbg(p->dev, "%s: VQE firmware dead\n", __func__);

	if (resp != 0xaffe)
		ret = -EIO;

	return ret;
}

static int dbmdx_vqe_mode_valid(struct dbmdx_private *p, unsigned int mode)
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


static int dbmdx_va_set_speed(struct dbmdx_private *p,
			      enum dbmdx_va_speeds speed)
{
	int ret;

	dev_info(p->dev, "%s: set speed to %u\n",
		__func__, speed);

	ret = dbmdx_send_cmd(
		p,
		DBMDX_VA_CLK_CFG | p->pdata->va_speed_cfg[speed].cfg,
		NULL);
	if (ret != 0)
		ret = -EIO;
	return ret;
}

/* FIXME: likely to be removed when re-adding support for overlay FW */
#if 0
static int dbmdx_va_set_high_speed(struct dbmdx_private *p)
{
	int ret;

	ret = dbmdx_va_set_speed(p, DBMDX_VA_SPEED_MAX);
	if (ret != 0) {
		dev_err(p->dev,
			"%s: could not change to high speed\n",
			__func__);
	}
	return ret;
}
#endif

static int dbmdx_buf_to_int(const char *buf)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	return (int)val;
}
#if 0
static int dbmdx_set_bytes_per_sample(struct dbmdx_private *p,
				      unsigned int mode)
{
	int ret;
	u16 resp = 0xffff;

	/* Changing the buffer conversion causes trouble: adapting the
	 * UART baudrate doesn't work anymore (firmware bug?) */
	if (((mode == DBMDX_AUDIO_MODE_PCM) && (p->bytes_per_sample == 2)) ||
	    ((mode == DBMDX_AUDIO_MODE_MU_LAW) &&
	     (p->bytes_per_sample == 1)))
		return 0;

	/* read configuration */
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_AUDIO_BUFFER_CONVERSION,
			     &resp);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to read DBMDX_VA_AUDIO_BUFFER_CONVERSION\n",
			__func__);
		return ret;
	}

	resp &= ~(1 << 12);
	resp |= (mode << 12);
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_AUDIO_BUFFER_CONVERSION | resp,
			     NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set DBMDX_VA_AUDIO_BUFFER_CONVERSION\n",
			__func__);
		return ret;
	}

	switch (mode) {
	case DBMDX_AUDIO_MODE_PCM:
		p->bytes_per_sample = 2;
		p->audio_mode = mode;
		break;
	case DBMDX_AUDIO_MODE_MU_LAW:
		p->bytes_per_sample = 1;
		p->audio_mode = mode;
		break;
	default:
		break;
	}

	return 0;
}
#endif

static int dbmdx_set_backlog_len(struct dbmdx_private *p, u16 history)
{
	int ret;
	unsigned short val;

	history &= ~(1 << 12);

	dev_info(p->dev, "%s: history 0x%x\n", __func__, (u32)history);

	/* If history is specified in ms, we should verify that
	FW audio buffer size in large enough to contain the history
	*/
	if (history > 2) {

		u32 min_buffer_size_in_bytes;
		u32 min_buffer_size_in_chunks;
		u32 audio_buffer_size_in_bytes;

		ret = dbmdx_send_cmd(p, DBMDX_VA_AUDIO_BUFFER_SIZE, &val);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to read DBMDX_VA_AUDIO_BUFFER_SIZE\n",
				__func__);
			return ret;
		}

		min_buffer_size_in_bytes =
		(p->pdata->va_buffering_pcm_rate / 1000) *
		((u32)history + MIN_EVENT_PROCESSING_TIME_MS) *
		p->pdata->va_audio_channels *
		p->bytes_per_sample;

		min_buffer_size_in_chunks =
			min_buffer_size_in_bytes / (8 * p->bytes_per_sample);

		audio_buffer_size_in_bytes = (u32)val * 8 * p->bytes_per_sample;

		if (audio_buffer_size_in_bytes < min_buffer_size_in_bytes) {

			dev_err(p->dev,
				"%s: FW Audio buffer size is not enough"
				" for requested backlog size\t"
				"FW buffer size: %u bytes (%u smp. chunks)\t"
				"Min req. buffer size: %u bytes (%u smp. chunks)\n",
				__func__,
				audio_buffer_size_in_bytes,
				(u32)val,
				min_buffer_size_in_bytes,
				min_buffer_size_in_chunks);

			return -1;
		}

		dev_dbg(p->dev,
			"%s: FW Audio buffer size was verified\t"
			"FW buffer size: %u bytes (%u smp. chunks)\t"
			"Min req. buffer size: %u bytes (%u smp. chunks)\n",
			__func__,
			audio_buffer_size_in_bytes,
			(u32)val,
			min_buffer_size_in_bytes,
			min_buffer_size_in_chunks);
	}

	ret = dbmdx_send_cmd(p,
			  DBMDX_VA_AUDIO_HISTORY | history ,
			  NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set backlog size\n", __func__);
		return ret;
	}

	p->va_backlog_length = history;

	return 0;
}

static int dbmdx_sleeping(struct dbmdx_private *p)
{
	return p->asleep;
}

static int dbmdx_vqe_set_tdm_bypass(struct dbmdx_private *p, int onoff)
{
	int ret;

	ret = dbmdx_send_cmd(p,
			     DBMDX_VQE_SET_HW_TDM_BYPASS_CMD |
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

static int dbmdx_force_wake(struct dbmdx_private *p)
{
	int ret = 0;
	u16 resp = 0xffff;

	/* assert wake pin */
	p->wakeup_set(p);

	if (p->active_fw == DBMDX_FW_VQE) {
		p->clk_enable(p, DBMDX_CLK_CONSTANT);
		usleep_range(1000, 2000);
	}

	p->chip->transport_enable(p, true);

	/* give some time to wakeup */
#if 0
	msleep(DBMDX_MSLEEP_WAKEUP);
#endif
	if (p->active_fw == DBMDX_FW_VA) {
		/* test if VA firmware is up */
		ret = dbmdx_va_alive(p);
		if (ret < 0) {
			dev_err(p->dev,	"%s: VA fw did not wakeup\n",
				__func__);
			ret = -EIO;
			goto out;
		}

		/* get operation mode register */
		ret = dbmdx_send_cmd(p, DBMDX_VA_OPR_MODE, &resp);
		if (ret < 0) {
			dev_err(p->dev,	"%s: failed to get operation mode\n",
				__func__);
			goto out;
		}
		p->va_flags.mode = resp;

		if (p->pdata->firmware_id == DBMDX_FIRMWARE_ID_DBMD4) {
			ret = dbmdx_restore_microphones(p);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to restore microphones\n",
					__func__);
				goto out;
			}
		}

	} else {
		/* test if VQE firmware is up */
		ret = dbmdx_vqe_alive(p);
		if (ret != 0) {
			dev_err(p->dev, "%s: VQE fw did not wakeup\n",
				__func__);
			ret = -EIO;
			goto out;
		}
		/* default mode is idle mode */
	}

	p->power_mode = DBMDX_PM_ACTIVE;

	/* make it not sleeping */
	p->asleep = false;

	dev_dbg(p->dev, "%s: woke up\n", __func__);
out:
	return ret;
}

static int dbmdx_wake(struct dbmdx_private *p)
{

	/* if chip not sleeping there is nothing to do */
	if (!dbmdx_sleeping(p) && p->va_flags.mode != DBMDX_DETECTION)
		return 0;

	return dbmdx_force_wake(p);
}

static int dbmdx_set_power_mode(
		struct dbmdx_private *p, enum dbmdx_power_modes mode)
{
	int ret = 0;
	enum dbmdx_power_modes new_mode = p->power_mode;

	dev_dbg(p->dev, "%s: would move %s -> %s (%2.2d -> %2.2d)\n",
			__func__,
			dbmdx_power_mode_names[p->power_mode],
			dbmdx_power_mode_names[mode],
			p->power_mode,
			mode);

	switch (p->power_mode) {
	case DBMDX_PM_BOOTING:
		switch (mode) {
		case DBMDX_PM_FALLING_ASLEEP:
			/* queue delayed work to set the chip to sleep*/
			queue_delayed_work(p->dbmdx_workq,
					&p->delayed_pm_work,
					msecs_to_jiffies(100));

		case DBMDX_PM_BOOTING:
		case DBMDX_PM_ACTIVE:
			new_mode = mode;
			break;

		default:
			goto illegal_transition;
		}
		break;

	case DBMDX_PM_ACTIVE:
		switch (mode) {
		case DBMDX_PM_ACTIVE:
			if (p->va_flags.mode == DBMDX_BUFFERING ||
				p->va_flags.mode == DBMDX_DETECTION)
				ret = dbmdx_wake(p);
			break;

		case DBMDX_PM_FALLING_ASLEEP:
			if (p->va_flags.mode == DBMDX_DETECTION) {
				dev_dbg(p->dev,
					"%s: no sleep during detection\n",
					__func__);
				p->chip->transport_enable(p, false);
			} else if (p->va_flags.mode == DBMDX_BUFFERING ||
				p->va_flags.mode == DBMDX_STREAMING ||
				p->va_flags.mode ==
				DBMDX_DETECTION_AND_STREAMING ||
				p->vqe_flags.in_call) {
				dev_dbg(p->dev,
					"%s: no sleep during buff/in call\n",
					__func__);
			}  else if (p->va_flags.sleep_not_allowed) {
				dev_dbg(p->dev,
					"%s: Sleep mode is blocked\n",
					__func__);
			} else {
			    /* queue delayed work to set the chip to sleep */
			    queue_delayed_work(p->dbmdx_workq,
				    &p->delayed_pm_work,
				    msecs_to_jiffies(200));
			    new_mode = mode;
			}
			break;

		case DBMDX_PM_BOOTING:
			new_mode = mode;
			break;

		default:
			goto illegal_transition;
		}
		break;

	case DBMDX_PM_FALLING_ASLEEP:
		switch (mode) {
		case DBMDX_PM_BOOTING:
		case DBMDX_PM_ACTIVE:
			/*
			 * flush queue if going to active
			 */
			p->va_flags.cancel_pm_work = true;
			p->unlock(p);
			cancel_delayed_work_sync(&p->delayed_pm_work);
			p->va_flags.cancel_pm_work = false;
			p->lock(p);
			new_mode = mode;
			/* wakeup chip */
			ret = dbmdx_wake(p);
			break;

		case DBMDX_PM_FALLING_ASLEEP:
			break;

		default:
			goto illegal_transition;
		}
		break;

	case DBMDX_PM_SLEEPING:
		/*
		 * wakeup the chip if going to active/booting
		 */
		switch (mode) {
		case DBMDX_PM_FALLING_ASLEEP:
			dev_dbg(p->dev,
				"%s: already sleeping; leave it this way...",
				__func__);
			new_mode = DBMDX_PM_SLEEPING;
			break;
		case DBMDX_PM_ACTIVE:
		case DBMDX_PM_BOOTING:
			ret = dbmdx_wake(p);
			if (ret) {
				dev_err(p->dev,
					"%s: failed to wake the chip up!\n",
					__func__);
				return ret;
			}
		case DBMDX_PM_SLEEPING:
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
			dbmdx_power_mode_names[p->power_mode],
			dbmdx_power_mode_names[new_mode],
			p->power_mode,
			new_mode);

	p->power_mode = new_mode;

	return 0;

illegal_transition:
	dev_err(p->dev, "%s: can't move %s -> %s\n", __func__,
		dbmdx_power_mode_names[p->power_mode],
		dbmdx_power_mode_names[mode]);
	return -EINVAL;
}


static int dbmdx_set_mode(struct dbmdx_private *p, int mode)
{
	int ret = 0;
	unsigned int cur_opmode = p->va_flags.mode;
	int required_mode = mode;
	int new_effective_mode = mode;
	int send_set_mode_cmd = 1;
	enum dbmdx_power_modes new_power_mode = p->power_mode;

	if (mode >= 0 && mode < DBMDX_NR_OF_STATES) {
		dev_dbg(p->dev, "%s: new requested mode: %d (%s)\n",
			__func__, mode, dbmdx_state_names[mode]);
	} else {
		dev_dbg(p->dev, "%s: mode: %d (invalid)\n", __func__, mode);
		return -EINVAL;
	}

	mode &= 0xffff;

	/*
	 * transform HIBERNATE to SLEEP in case no wakeup pin
	 * is available
	 */
	 if (!dbmdx_can_wakeup(p) && mode == DBMDX_HIBERNATE)
		mode = DBMDX_SLEEP_PLL_ON;
	if ((!dbmdx_can_wakeup(p) || p->va_flags.sleep_not_allowed) &&
		(mode == DBMDX_SLEEP_PLL_ON || mode == DBMDX_HIBERNATE))
		mode = DBMDX_IDLE;

	p->va_flags.buffering = 0;

	p->va_flags.irq_inuse = 0;

	/* wake up if asleep */
	ret = dbmdx_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		goto out;
	}

	if (cur_opmode == DBMDX_DETECTION)
		/* explicit transp. enable if in detection */
		p->chip->transport_enable(p, true);

	/* Select new power mode */
	switch (mode) {
	case DBMDX_IDLE:
		/* set power state to FALLING ASLEEP */
		if (p->va_flags.pcm_streaming_active ||
			p->va_flags.sleep_not_allowed)
			new_power_mode = DBMDX_PM_ACTIVE;
		else
			new_power_mode = DBMDX_PM_FALLING_ASLEEP;
		break;

	case DBMDX_DETECTION:
		p->va_flags.irq_inuse = 1;
	case DBMDX_BUFFERING:
	case DBMDX_STREAMING:
	case DBMDX_DETECTION_AND_STREAMING:
		/* switch to ACTIVE */
		new_power_mode = DBMDX_PM_ACTIVE;
		break;

	case DBMDX_SLEEP_PLL_OFF:
	case DBMDX_SLEEP_PLL_ON:
	case DBMDX_HIBERNATE:
		p->asleep = true;
		break;
	}

	if (mode == DBMDX_IDLE || mode == DBMDX_BUFFERING)
		/* Stop PCM streaming work */
		p->va_flags.pcm_worker_active = 0;
	else if (mode == DBMDX_DETECTION) {
		unsigned short recog_mode =
			p->va_detection_mode == DETECTION_MODE_VOICE_ENERGY ?
			2 : 1;

		if (!p->va_flags.a_model_loaded && recog_mode == 1) {
			/* normal recognition mode but no a-model loaded? */
			dev_err(p->dev,
				"%s: can't set detection, a-model not loaded\n",
				__func__);
			p->va_flags.irq_inuse = 0;
			ret = -1;
			goto out;
		}
		ret = dbmdx_send_cmd(
			p, DBMDX_VA_SENS_RECOGNITION_MODE | recog_mode, NULL);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set recognition mode 0x%x\n",
				__func__, mode);
			p->va_flags.irq_inuse = 0;
			goto out;
		}

		if (p->va_flags.pcm_streaming_active) {
			new_effective_mode = DBMDX_DETECTION_AND_STREAMING;

			ret = dbmdx_disable_hw_vad(p);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to disable fw vad settings\n",
					__func__);
				p->va_flags.irq_inuse = 0;
				goto out;
			}
		} else {
			ret = dbmdx_restore_fw_vad_settings(p);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to restore fw vad settings\n",
					__func__);
				p->va_flags.irq_inuse = 0;
				goto out;
			}
		}

		dev_info(p->dev, "%s: Recognition mode was set to 0x%x\n",
				__func__, recog_mode);

	} else if (mode == DBMDX_STREAMING) {

		/* If DBMDX_STREAMING was requested, no passprase recog.
		is required. Thus set recognition mode to 0 */
		ret = dbmdx_send_cmd(
			p, DBMDX_VA_SENS_RECOGNITION_MODE, NULL);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set recognition mode 0\n",
				__func__);
			goto out;
		}

		ret = dbmdx_disable_hw_vad(p);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to disable fw vad settings\n",
				__func__);
			p->va_flags.irq_inuse = 0;
			goto out;
		}

		required_mode = DBMDX_DETECTION;
	} else if (mode == DBMDX_DETECTION_AND_STREAMING) {

		send_set_mode_cmd = 1;
		required_mode = DBMDX_DETECTION;

		/* We must go trough IDLE mode do disable HW VAD */
		ret = dbmdx_send_cmd(p,
				     DBMDX_VA_OPR_MODE | DBMDX_IDLE,
				     NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to set mode 0x%x\n",
				__func__, mode);
			p->va_flags.irq_inuse = 0;
			goto out;
		}

		ret = dbmdx_disable_hw_vad(p);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to disable fw vad settings\n",
				__func__);
			p->va_flags.irq_inuse = 0;
			goto out;
		}
		p->va_flags.irq_inuse = 1;

	}

	if (send_set_mode_cmd) {
		/* set operation mode register */
		ret = dbmdx_send_cmd(p,
				     DBMDX_VA_OPR_MODE | required_mode,
				     NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to set mode 0x%x\n",
				__func__, mode);
			p->va_flags.irq_inuse = 0;
			goto out;
		}
	}

	p->va_flags.mode = new_effective_mode;

	/* Verify that mode was set */
	if (!p->asleep) {
		unsigned short new_mode;

		usleep_range(DBMDX_USLEEP_SET_MODE,
			DBMDX_USLEEP_SET_MODE + 1000);
		ret = dbmdx_send_cmd(p, DBMDX_VA_OPR_MODE, &new_mode);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to read DBMDX_VA_OPR_MODE\n",
				__func__);
			goto out;
		}

		if (required_mode != new_mode)
			dev_err(p->dev, "%s: Mode verification failed: 0x%x\n",
			__func__, mode);

	}

	if (new_power_mode != p->power_mode) {
		ret = dbmdx_set_power_mode(p, new_power_mode);
		if (ret) {
			dev_err(p->dev, "%s: Failed to set power mode\n",
			__func__);
			goto out;
		}
	}

	if (required_mode == DBMDX_BUFFERING) {
		p->va_flags.buffering = 1;
		schedule_work(&p->sv_work);
#if defined(CONFIG_SND_SOC_DBMDX_SND_CAPTURE)
	} else if (new_effective_mode == DBMDX_DETECTION_AND_STREAMING ||
			new_effective_mode == DBMDX_STREAMING) {
		p->va_flags.pcm_worker_active = 1;
		schedule_work(&p->pcm_streaming_work);
#endif
	}

	ret = 0;
	goto out;

out:
	dev_dbg(p->dev,
		"%s: Successfull mode transition from %d to mode is %d\n",
		__func__, cur_opmode, p->va_flags.mode);
	return ret;
}

static int dbmdx_trigger_detection(struct dbmdx_private *p)
{
	int ret = 0;

	if (!p->va_flags.a_model_loaded &&
			p->va_detection_mode != DETECTION_MODE_VOICE_ENERGY) {
		dev_err(p->dev, "%s: a-model not loaded!\n", __func__);
		return -EINVAL;
	}

	/* set chip to idle mode before entering detection mode */
	ret = dbmdx_set_mode(p, DBMDX_IDLE);
	if (ret) {
		dev_err(p->dev, "%s: failed to set device to idle mode\n",
			__func__);
		return -EIO;
	}

	ret = dbmdx_set_mode(p, DBMDX_DETECTION);
	if (ret) {
		dev_err(p->dev,
			"%s: failed to set device to detection mode\n",
			__func__);
		return -EIO;
	}

	/* disable transport (if configured) so the FW goes into best power
	 * saving mode (only if no active pcm streaming in background) */
	if (p->va_flags.mode != DBMDX_STREAMING &&
		p->va_flags.mode != DBMDX_DETECTION_AND_STREAMING)
		p->chip->transport_enable(p, false);

	return 0;
}

static int dbmdx_set_fw_debug_mode(struct dbmdx_private *p,
	enum dbmdx_fw_debug_mode mode)
{
	int ret = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (p->active_fw != DBMDX_FW_VA) {
		dev_err(p->dev, "%s: VA FW is no active\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	switch (mode) {
	case FW_DEBUG_OUTPUT_UART:
		if (p->active_interface == DBMDX_INTERFACE_UART) {
			dev_err(p->dev, "%s: Not supported in UART mode\n",
				__func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		ret = dbmdx_send_cmd(p, DBMDX_VA_DEBUG_1 | 0x5, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to send cmd\n", __func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		break;
	case FW_DEBUG_OUTPUT_CLK:
		ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_ADDR_LO | 0x8, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to send cmd\n", __func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		ret = dbmdx_send_cmd(p, DBMDX_VA_DEBUG_1 | 0xB, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to send cmd\n", __func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		break;
	case FW_DEBUG_OUTPUT_CLK_UART:
		if (p->active_interface == DBMDX_INTERFACE_UART) {
			dev_err(p->dev, "%s: Not supported in UART mode\n",
				__func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_ADDR_LO | 0x8, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to send cmd\n", __func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		ret = dbmdx_send_cmd(p, DBMDX_VA_DEBUG_1 | 0xB, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to send cmd\n", __func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		ret = dbmdx_send_cmd(p, DBMDX_VA_DEBUG_1 | 0x5, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: failed to send cmd\n", __func__);
			ret = -EIO;
			goto out_pm_mode;
		}

		break;
	default:
		dev_err(p->dev, "%s: Unsupported FW Debug mode 0x%x\n",
			__func__, mode);
		ret = -EINVAL;
		goto out_pm_mode;
	}


out_pm_mode:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	p->unlock(p);
	return ret;
}


static void dbmdx_delayed_pm_work_hibernate(struct work_struct *work)
{
	int ret;
	struct dbmdx_private *p =
		container_of(work, struct dbmdx_private,
			     delayed_pm_work.work);

	dev_dbg(p->dev, "%s\n", __func__);

	p->lock(p);
	if (p->va_flags.cancel_pm_work) {
		dev_dbg(p->dev,
			"%s: the work has been just canceled\n",
			__func__);
		goto out;
	}

	if (p->active_fw == DBMDX_FW_VA) {
		if (p->pdata->firmware_id == DBMDX_FIRMWARE_ID_DBMD4) {
			ret = dbmdx_disable_microphones(p);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to disable microphones\n",
					__func__);
				goto out;
			}
		}
		p->wakeup_release(p);
		ret = dbmdx_set_mode(p, DBMDX_HIBERNATE);
	} else {
		/* VQE */

		/* Activate HW TDM bypass
		 * FIXME: make it conditional
		 */
		ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HW_TDM_BYPASS_CMD |
				DBMDX_VQE_SET_HW_TDM_BYPASS_MODE_1 |
				DBMDX_VQE_SET_HW_TDM_BYPASS_FIRST_PAIR_EN,
				NULL);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to activate HW TDM bypass\n",
				__func__);
		}

		p->wakeup_release(p);

		ret = dbmdx_send_cmd(
			p, DBMDX_VQE_SET_POWER_STATE_CMD |
			DBMDX_VQE_SET_POWER_STATE_HIBERNATE, NULL);
	}

	if (ret) {
		p->wakeup_set(p);
		p->power_mode = DBMDX_PM_ACTIVE;
		if (p->active_fw == DBMDX_FW_VA &&
			p->pdata->firmware_id == DBMDX_FIRMWARE_ID_DBMD4) {
			ret = dbmdx_restore_microphones(p);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to restore microphones\n",
					__func__);
			}
		}
		dev_err(p->dev, "%s: fail to set to HIBERNATE - %d\n",
			__func__, ret);
		goto out;
	}

	msleep(DBMDX_MSLEEP_HIBARNATE);

	p->asleep = true;
	p->power_mode = DBMDX_PM_SLEEPING;

	if (p->active_fw == DBMDX_FW_VQE)
		p->clk_disable(p, DBMDX_CLK_CONSTANT);

	p->chip->transport_enable(p, false);

out:
	dev_dbg(p->dev, "%s: current power mode: %s\n",
		__func__, dbmdx_power_mode_names[p->power_mode]);
	p->unlock(p);
}

static int dbmdx_vqe_set_use_case(struct dbmdx_private *p, unsigned int uc)
{
	int ret = 0;

	uc &= 0xffff;

	if (uc == 0) {
		/* if already sleeping we are already idle */
		if (dbmdx_sleeping(p))
			goto out;
		/* enable TDM bypass */
		dbmdx_vqe_set_tdm_bypass(p, 1);
	} else {
		if (dbmdx_sleeping(p)) {
			ret = dbmdx_wake(p);
			if (ret)
				goto out;
		}
		/* stop TDM bypass */
		dbmdx_vqe_set_tdm_bypass(p, 0);
	}

	ret = dbmdx_send_cmd(p,
			     DBMDX_VQE_SET_USE_CASE_CMD | uc,
			     NULL);
	if (ret < 0)
		dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
			__func__, uc, DBMDX_VQE_SET_USE_CASE_CMD);

out:
	return ret;
}

/* Microphone modes */
enum dbmdx_microphone_mode {
	DBMDX_MIC_MODE_DIGITAL_LEFT = 0,
	DBMDX_MIC_MODE_DIGITAL_RIGHT,
	DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT,
	DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT,
	DBMDX_MIC_MODE_ANALOG,
	DBMDX_MIC_MODE_DISABLE,
};

enum dbmdx_microphone_type {
	DBMDX_MIC_TYPE_DIGITAL_LEFT = 0,
	DBMDX_MIC_TYPE_DIGITAL_RIGHT,
	DBMDX_MIC_TYPE_ANALOG,
};

static int dbmdx_update_microphone_mode(struct dbmdx_private *p,
				     enum dbmdx_microphone_mode mode)
{
	int ret = 0;
	int ret1 = 0;
	unsigned int new_detection_kfifo_size;

	dev_dbg(p->dev, "%s: mode: %d\n", __func__, mode);

	/* first disable both mics */
	ret = dbmdx_send_cmd(p, DBMDX_VA_MICROPHONE2_CONFIGURATION,
				      NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set microphone mode 0x%x\n",
			__func__, mode);
		ret = -EINVAL;
		goto out;
	}

	switch (mode) {
	case DBMDX_MIC_MODE_DISABLE:
		ret = dbmdx_send_cmd(p, DBMDX_VA_MICROPHONE1_CONFIGURATION,
				      NULL);
		break;
	case DBMDX_MIC_MODE_DIGITAL_LEFT:
		ret = dbmdx_send_cmd(p,
			DBMDX_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMDX_MIC_TYPE_DIGITAL_LEFT],
			NULL);
		break;
	case DBMDX_MIC_MODE_DIGITAL_RIGHT:
		ret = dbmdx_send_cmd(p,
			DBMDX_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMDX_MIC_TYPE_DIGITAL_RIGHT],
			NULL);
		break;
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
		ret = dbmdx_send_cmd(p,
			DBMDX_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMDX_MIC_TYPE_DIGITAL_LEFT],
			NULL);
		ret1 = dbmdx_send_cmd(p,
			DBMDX_VA_MICROPHONE2_CONFIGURATION |
			p->pdata->va_mic_config[DBMDX_MIC_TYPE_DIGITAL_RIGHT],
			NULL);
		break;
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
		ret = dbmdx_send_cmd(p,
			DBMDX_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMDX_MIC_TYPE_DIGITAL_RIGHT],
			NULL);
		ret1 = dbmdx_send_cmd(p,
			DBMDX_VA_MICROPHONE2_CONFIGURATION |
			p->pdata->va_mic_config[DBMDX_MIC_TYPE_DIGITAL_LEFT],
			NULL);
		break;
	case DBMDX_MIC_MODE_ANALOG:
		ret = dbmdx_send_cmd(p,
			DBMDX_VA_MICROPHONE1_CONFIGURATION |
			p->pdata->va_mic_config[DBMDX_MIC_TYPE_ANALOG], NULL);
		break;
	default:
		dev_err(p->dev, "%s: Unsupported microphone mode 0x%x\n",
			__func__, mode);
		ret = -EINVAL;
		goto out;
	}

	if (ret < 0 || ret1 < 0) {
		dev_err(p->dev, "%s: failed to set microphone mode 0x%x\n",
			__func__, mode);
		ret = -EINVAL;
		goto out;
	}

	p->va_current_mic_config = mode;

	if (p->va_current_mic_config != DBMDX_MIC_MODE_DISABLE)
		p->va_active_mic_config = p->va_current_mic_config;

	new_detection_kfifo_size = p->detection_samples_kfifo_buf_size;

	switch (mode) {
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
		p->pdata->va_audio_channels = 2;
		if (p->pdata->detection_buffer_channels == 0 ||
			p->pdata->detection_buffer_channels == 2) {
			p->detection_achannel_op = AUDIO_CHANNEL_OP_COPY;
			new_detection_kfifo_size = MAX_KFIFO_BUFFER_SIZE_STEREO;
		} else {
			p->detection_achannel_op =
				AUDIO_CHANNEL_OP_TRUNCATE_PRIMARY;
			new_detection_kfifo_size = MAX_KFIFO_BUFFER_SIZE_MONO;
		}

		if (p->audio_pcm_channels == 2)
			p->pcm_achannel_op = AUDIO_CHANNEL_OP_COPY;
		else
			p->pcm_achannel_op =
				AUDIO_CHANNEL_OP_TRUNCATE_PRIMARY;

		break;
	case DBMDX_MIC_MODE_DIGITAL_LEFT:
	case DBMDX_MIC_MODE_DIGITAL_RIGHT:
	case DBMDX_MIC_MODE_ANALOG:
		p->pdata->va_audio_channels = 1;
		if (p->pdata->detection_buffer_channels == 0 ||
			p->pdata->detection_buffer_channels == 1) {
			p->detection_achannel_op = AUDIO_CHANNEL_OP_COPY;
			new_detection_kfifo_size = MAX_KFIFO_BUFFER_SIZE_MONO;
		} else {
			p->detection_achannel_op =
				AUDIO_CHANNEL_OP_DUPLICATE_MONO;
			new_detection_kfifo_size = MAX_KFIFO_BUFFER_SIZE_STEREO;
		}

		if (p->audio_pcm_channels == 1)
			p->pcm_achannel_op = AUDIO_CHANNEL_OP_COPY;
		else
			p->pcm_achannel_op =
				AUDIO_CHANNEL_OP_DUPLICATE_MONO;

		break;
	default:
		break;
	}

	if (new_detection_kfifo_size != p->detection_samples_kfifo_buf_size) {
		p->detection_samples_kfifo_buf_size = new_detection_kfifo_size;
		kfifo_init(&p->detection_samples_kfifo,
			p->detection_samples_kfifo_buf,
			new_detection_kfifo_size);
	}

out:
	return ret;
}

static int dbmdx_reconfigure_microphones(struct dbmdx_private *p,
				     enum dbmdx_microphone_mode mode)
{
	int ret;
	int current_mode;
	int current_audio_channels;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (p->active_fw != DBMDX_FW_VA) {
		dev_err(p->dev, "%s: VA firmware not active, error\n",
			__func__);
		return -EAGAIN;
	}


	dev_dbg(p->dev, "%s: val - %d\n", __func__, (int)mode);

	current_mode = p->va_flags.mode;
	current_audio_channels = p->pdata->va_audio_channels;

	/* flush pending buffering works if any */
	p->va_flags.buffering = 0;
	flush_work(&p->sv_work);

	p->va_flags.reconfigure_mic_on_vad_change = true;

	ret = dbmdx_suspend_pcm_streaming_work(p);
	if (ret < 0)
		dev_err(p->dev, "%s: Failed to suspend PCM Streaming Work\n",
			__func__);

	p->lock(p);

	ret = dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set PM_ACTIVE\n", __func__);
		ret = -EINVAL;
		goto out_unlock;
	}

	p->va_flags.sleep_not_allowed = true;

	/* set chip to idle mode */
	ret = dbmdx_set_mode(p, DBMDX_IDLE);
	if (ret) {
		dev_err(p->dev, "%s: failed to set device to idle mode\n",
			__func__);
		p->va_flags.sleep_not_allowed = false;
		goto out_unlock;
	}

	ret = dbmdx_update_microphone_mode(p, mode);

	p->va_flags.sleep_not_allowed = false;

	if (ret < 0) {
		dev_err(p->dev, "%s: set microphone mode error\n", __func__);
		goto out_pm_mode;
	}

	if (current_mode == DBMDX_DETECTION ||
		current_mode == DBMDX_DETECTION_AND_STREAMING) {

		ret = dbmdx_trigger_detection(p);
		if (ret) {
			dev_err(p->dev,
				"%s: failed to trigger detection\n",
				__func__);
			goto out_pm_mode;
		}

	} else if (current_mode == DBMDX_STREAMING) {
		ret = dbmdx_set_mode(p, DBMDX_STREAMING);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set DBMDX_STREAMING mode\n",
				__func__);
			goto out_pm_mode;
		}
	} else
		dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

	dev_dbg(p->dev, "%s: Microphone was set to mode:- %d\n",
		__func__, (int)mode);
out_pm_mode:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
out_unlock:
	p->unlock(p);
	return ret;
}

static int dbmdx_disable_microphones(struct dbmdx_private *p)
{
	if (p->va_current_mic_config == DBMDX_MIC_MODE_DISABLE)
		return 0;

	p->va_active_mic_config = p->va_current_mic_config;

	return dbmdx_update_microphone_mode(p, DBMDX_MIC_MODE_DISABLE);
}

static int dbmdx_restore_microphones(struct dbmdx_private *p)
{
	if (p->va_current_mic_config != DBMDX_MIC_MODE_DISABLE)
		return 0;

	return dbmdx_update_microphone_mode(p, p->va_active_mic_config);
}

static int dbmdx_set_pcm_rate(struct dbmdx_private *p,
				      unsigned int pcm_rate)
{
	u16 cur_config = 0xffff;
	int rate_mask;
	int ret = 0;

	if (p->current_pcm_rate == pcm_rate)
		return 0;

	switch (pcm_rate) {
	case 16000:
		rate_mask = DBMDX_SND_PCM_RATE_16000;
		break;
	case 32000:
		rate_mask = DBMDX_SND_PCM_RATE_32000;
		break;
	case 48000:
		rate_mask = DBMDX_SND_PCM_RATE_48000;
		break;
	default:
		dev_err(p->dev, "%s: Unsupported rate %u\n",
			__func__, pcm_rate);
		return -EINVAL;
	}

	dev_dbg(p->dev, "%s: set pcm rate: %u\n", __func__, pcm_rate);

	p->current_pcm_rate = pcm_rate;

	/* read configuration */
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2,
			     &cur_config);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to read DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	cur_config &= rate_mask;
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2 | cur_config,
			     NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	return dbmdx_update_microphone_mode(p, p->va_active_mic_config);

}

static int dbmdx_read_fw_vad_settings(struct dbmdx_private *p)
{
	u16 cur_config = 0xffff;
	int ret = 0;

	/* read configuration */
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2,
			     &cur_config);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to read DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	cur_config &= DBMDX_HW_VAD_MASK;
	p->fw_vad_type = ((cur_config >> 5) & 0x7);

	dev_dbg(p->dev, "%s: FW Vad is set to 0x%08x\n",
		__func__, p->fw_vad_type);

	return 0;
}

static int dbmdx_disable_hw_vad(struct dbmdx_private *p)
{
	u16 cur_config = 0xffff;
	int ret = 0;

	/* read configuration */
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2,
			     &cur_config);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to read DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	cur_config &= (~(DBMDX_HW_VAD_MASK) & 0xffff);

	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2 | cur_config,
			     NULL);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	if (p->va_flags.reconfigure_mic_on_vad_change) {

		ret = dbmdx_update_microphone_mode(p, p->va_active_mic_config);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to restore microphones\n",
				__func__);
		}
		p->va_flags.reconfigure_mic_on_vad_change = false;
	}

	dev_dbg(p->dev, "%s: HW Vad is disabled reg 0x23 is set to 0x%08x\n",
		__func__, cur_config);

	return 0;
}

static int dbmdx_restore_fw_vad_settings(struct dbmdx_private *p)
{
	u16 cur_config = 0xffff;
	int ret = 0;

	/* read configuration */
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2,
			     &cur_config);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to read DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	cur_config |= ((p->fw_vad_type << 5) & DBMDX_HW_VAD_MASK);

	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2 | cur_config,
			     NULL);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	if (p->va_flags.reconfigure_mic_on_vad_change) {

		ret = dbmdx_update_microphone_mode(p, p->va_active_mic_config);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to restore microphones\n",
				__func__);
		}
		p->va_flags.reconfigure_mic_on_vad_change = false;
	}

	dev_dbg(p->dev, "%s: HW Vad is restored reg 0x23 is set to 0x%08x\n",
		__func__, cur_config);

	return 0;
}

static int dbmdx_set_pcm_streaming_mode(struct dbmdx_private *p, u16 mode)
{
	u16 cur_config = 0xffff;
	int ret = 0;

	/* read configuration */
	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2,
			     &cur_config);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to read DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	if (mode > 1)
		mode = 1;

	cur_config &= ~(1 << 12);
	cur_config |= (mode << 12);

	ret = dbmdx_send_cmd(p,
			     DBMDX_VA_GENERAL_CONFIGURATION_2 | cur_config,
			     NULL);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set DBMDX_VA_GENERAL_CONFIGURATION_2\n",
			__func__);
		return ret;
	}

	dev_dbg(p->dev, "%s: PCM Streaming mode: %d, Reg 0x23: (0x%08x)\n",
		__func__, mode, cur_config);

	return 0;
}

static int dbmdx_calc_amodel_checksum(struct dbmdx_private *p,
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

static int dbmdx_acoustic_model_load(struct dbmdx_private *p,
				      const u8 *data,
				      size_t size,
				      size_t gram_size,
				      size_t net_size,
				      enum dbmdx_load_amodel_mode amode)
{
	int ret;
	int ret2;

	dev_dbg(p->dev, "%s: size:%zd\n", __func__, size);

	/* flush pending sv work if any */
	p->va_flags.buffering = 0;
	flush_work(&p->sv_work);

	p->lock(p);

	p->device_ready = false;

	/* set chip to idle mode */
	ret = dbmdx_set_mode(p, DBMDX_IDLE);
	if (ret) {
		dev_err(p->dev, "%s: failed to set device to idle mode\n",
			__func__);
		goto out_unlock;
	}

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	p->va_flags.a_model_loaded = 0;

	/* prepare the chip interface for A-Model loading */
	ret = p->chip->prepare_amodel_loading(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: failed to prepare A-Model loading\n",
			__func__);
		p->device_ready = true;
		goto out_unlock;
	}

	/* load A-Model and verify checksum */
	ret = p->chip->load_amodel(p,
			   data,
			   size - 4,
			   gram_size,
			   net_size,
			   &data[size - 4],
			   4,
			   amode);
	if (ret) {
		dev_err(p->dev, "%s: sending amodel failed\n", __func__);
		ret = -1;
		goto out_finish_loading;
	}


	if (amode == LOAD_AMODEL_2NDARY) {
		p->device_ready = true;
		p->unlock(p);
		dev_err(p->dev,
			"%s: upload done for secondary amodel\n", __func__);
		return ret;
	}
#if 0
	ret = dbmdx_set_bytes_per_sample(p, p->audio_mode);
	if (ret) {
		dev_err(p->dev,
			"%s: failed to set bytes per sample\n", __func__);
		ret = -1;
		goto out_finish_loading;
	}
#endif
	ret = dbmdx_va_alive(p);
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

	if (p->pdata->auto_detection && !p->va_flags.auto_detection_disabled) {
		dev_info(p->dev, "%s: enforcing DETECTION opmode\n",
			 __func__);
		ret = dbmdx_trigger_detection(p);
		if (ret) {
			dev_err(p->dev,
				"%s: failed to trigger detection\n",
				__func__);
			goto out_unlock;
		}
	}

	p->device_ready = true;

out_unlock:
	p->unlock(p);

	return ret;
}


static ssize_t dbmdx_acoustic_model_build(struct dbmdx_private *p,
					const u8 *gram_data,
					size_t gram_size,
					const u8 *net_data,
					size_t net_size)
{
	unsigned char head[DBMDX_AMODEL_HEADER_SIZE] = { 0 };
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
	memcpy(p->amodel_buf, head, DBMDX_AMODEL_HEADER_SIZE);

	pos += DBMDX_AMODEL_HEADER_SIZE;

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
	memcpy(p->amodel_buf + pos, head, DBMDX_AMODEL_HEADER_SIZE);

	pos += DBMDX_AMODEL_HEADER_SIZE;

	if (pos + net_size + 6 > MAX_AMODEL_SIZE) {
		dev_err(p->dev,
			"%s: adding net exceeds max size %zd>%d\n",
			__func__, pos + net_size + 6, MAX_AMODEL_SIZE);
		ret = -1;
		goto out;
	}

	memcpy(p->amodel_buf + pos, net_data, net_size);

	ret = dbmdx_calc_amodel_checksum(p,
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

static void dbmdx_get_firmware_version(const char *data, size_t size,
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

static int dbmdx_firmware_ready(const struct firmware *fw,
				struct dbmdx_private *p)
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
	dbmdx_get_firmware_version(fw->data, fw->size, fw_version, 200);
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
	p->clk_enable(p, DBMDX_CLK_MASTER);

	/* boot */
	ret = p->chip->boot(fw, p, fw_file_checksum, 4, 1);
	if (ret)
		goto out_disable_hs_clk;

	/* disable high speed clock after boot */
	dbmdx_clk_disable(p, DBMDX_CLK_MASTER);

	/* finish boot if required */
	ret = p->chip->finish_boot(p);
	if (ret)
		goto out_err;

	ret = 0;
	goto out;

out_disable_hs_clk:
	dbmdx_clk_disable(p, DBMDX_CLK_MASTER);
out_err:
	dev_err(p->dev, "%s: firmware request failed\n", __func__);
	ret = -EIO;
out:
	return ret;
}

int dbmdx_io_register_read(struct dbmdx_private *p,
				u32 addr, u32 *presult)
{
	u16 val;
	u32 result;
	int ret = 0;

	dev_dbg(p->dev, "%s\n", __func__);

	if (!p)
		return -EAGAIN;

	*presult = 0;

	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_ADDR_LO | (addr & 0xffff),
		NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: io_addr_read error(1)\n", __func__);
		ret = -1;
		goto out;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_ADDR_HI | (addr >> 16), NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: io_addr_read error(2)\n", __func__);
		ret = -1;
		goto out;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_VALUE_LO, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %u error\n",
				__func__, DBMDX_VA_IO_PORT_VALUE_LO);
		ret = -1;
		goto out;
	}

	result = (u32)(val & 0xffff);

	val = 0;
	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_VALUE_HI, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %u error\n",
				__func__, DBMDX_VA_IO_PORT_VALUE_HI);
		ret = -1;
		goto out;
	}

	result += ((u32)val << 16);

	*presult = result;

	dev_dbg(p->dev, "%s: addr=0x%08x, val = 0x%08x\n",
		__func__, addr, result);

out:
	return ret;
}

int dbmdx_io_register_write(struct dbmdx_private *p,
				u32 addr, u32 value)
{
	int ret = 0;

	dev_dbg(p->dev, "%s\n", __func__);

	if (!p)
		return -EAGAIN;

	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_ADDR_LO | (addr & 0xffff),
		NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: io_addr_write error(1)\n", __func__);
		ret = -1;
		goto out;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_ADDR_HI | (addr >> 16), NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: io_addr_write error(2)\n", __func__);
		ret = -1;
		goto out;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_VALUE_LO | (value & 0xffff),
		NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: io_addr_write error(3)\n", __func__);
		ret = -1;
		goto out;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VA_IO_PORT_VALUE_HI | (value >> 16),
		NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: io_addr_write error(4)\n", __func__);
		ret = -1;
		goto out;
	}

	dev_dbg(p->dev, "%s: addr=0x%08x was set to 0x%08x\n",
		__func__, addr, value);
out:
	return ret;
}

static int dbmdx_config_va_mode(struct dbmdx_private *p)
{
	unsigned int i;
	int ret;
	u16 fwver = 0xffff;

	dev_dbg(p->dev, "%s\n", __func__);

	for (i = 0; i < p->pdata->va_cfg_values; i++)
		dbmdx_send_cmd(p, p->pdata->va_cfg_value[i], NULL);

	/* Give to PLL enough time for stabilization */
	msleep(DBMDX_MSLEEP_CONFIG_VA_MODE_REG);

	p->chip->transport_enable(p, true);
#if 0
	/* set sample size */
	ret = dbmdx_set_bytes_per_sample(p, p->audio_mode);
	if (ret)
		goto out;
#endif
	/* Set Backlog */
	ret = dbmdx_set_backlog_len(p, p->va_backlog_length);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: could not set backlog history configuration\n",
			__func__);
		goto out;
	}

	/* read firmware version */
	ret = dbmdx_send_cmd(p, DBMDX_VA_GET_FW_VER, &fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}

	/* Enusre that PCM rate will be reconfigured */
	p->current_pcm_rate = 0;

	/* Set pcm rate and configure microphones*/
	ret = dbmdx_set_pcm_rate(p, p->pdata->va_buffering_pcm_rate);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set pcm rate\n", __func__);
		goto out;
	}

	if (p->va_cur_analog_gain != 0x1000) {

		ret = dbmdx_send_cmd(p,
			DBMDX_VA_ANALOG_MIC_GAIN |
			(p->va_cur_analog_gain & 0xffff),
			NULL);

		if (ret < 0) {
			dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
				__func__, p->va_cur_analog_gain,
				DBMDX_VA_ANALOG_MIC_GAIN);
			goto out;
		}
	}

	if (p->va_cur_digital_gain != 0x1000) {

		ret = dbmdx_send_cmd(p,
			DBMDX_VA_DIGITAL_GAIN |
			(p->va_cur_digital_gain & 0xffff),
			NULL);

		if (ret < 0) {
			dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
				__func__, p->va_cur_digital_gain,
				DBMDX_VA_DIGITAL_GAIN);
			goto out;
		}
	}

	ret = dbmdx_read_fw_vad_settings(p);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to read fw vad settings\n",
			__func__);
		goto out;
	}

	ret = dbmdx_set_pcm_streaming_mode(p,
		(u16)(p->pdata->pcm_streaming_mode));

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set pcm streaming mode\n",
			__func__);
		goto out;
	}

	dev_info(p->dev, "%s: VA firmware 0x%x ready\n", __func__, fwver);

	ret = dbmdx_set_mode(p, DBMDX_IDLE);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not set to idle\n", __func__);
		goto out;
	}

	ret = 0;
out:
	return ret;
}

static int dbmdx_va_firmware_ready(struct dbmdx_private *p)
{
	int ret;

	dev_dbg(p->dev, "%s\n", __func__);

	/* common boot */
	ret = dbmdx_firmware_ready(p->va_fw, p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not load VA firmware\n", __func__);
		return -EIO;
	}
	dbmdx_set_va_active(p);

	ret = p->chip->set_va_firmware_ready(p);
	if (ret) {
		dev_err(p->dev, "%s: could not set to ready VA firmware\n",
			__func__);
		return -EIO;
	}

	ret = dbmdx_config_va_mode(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not configure VA firmware\n",
			__func__);
		return -EIO;
	}

	return 0;
}

static int dbmdx_vqe_get_ver(struct dbmdx_private *p)
{
	int ret;
	u16 fwver = 0xffff;

	/* read firmware version */
	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HOST_STATUS_CMD |
				DBMDX_VQE_HOST_STATUS_CMD_PRODUCT_MAJOR_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware product major: 0x%x\n",
		__func__, fwver);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HOST_STATUS_CMD |
				DBMDX_VQE_HOST_STATUS_CMD_PRODUCT_MINOR_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware product minor: 0x%x\n",
		__func__, fwver);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HOST_STATUS_CMD |
				DBMDX_VQE_HOST_STATUS_CMD_FW_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware version: 0x%x\n", __func__, fwver);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HOST_STATUS_CMD |
				DBMDX_VQE_HOST_STATUS_CMD_PATCH_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware patch version: 0x%x\n",
		__func__, fwver);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HOST_STATUS_CMD |
				DBMDX_VQE_HOST_STATUS_CMD_DEBUG_VER,
				&fwver);
	if (ret < 0) {
		dev_err(p->dev, "%s: could not read firmware version\n",
			__func__);
		goto out;
	}
	dev_info(p->dev, "%s: VQE firmware debug version: 0x%x\n",
		__func__, fwver);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HOST_STATUS_CMD |
				DBMDX_VQE_HOST_STATUS_CMD_TUNING_VER,
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


static int dbmdx_config_vqe_mode(struct dbmdx_private *p)
{
	unsigned int i;
	int ret;

	dev_dbg(p->dev, "%s\n", __func__);

	ret = dbmdx_vqe_alive(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: VQE firmware not ready\n", __func__);
		goto out;
	}

	for (i = 0; i < p->pdata->vqe_cfg_values; i++)
		(void)dbmdx_send_cmd(p, p->pdata->vqe_cfg_value[i], NULL);


	ret = 0;

out:
	return ret;
}

static int dbmdx_vqe_firmware_ready(struct dbmdx_private *p,
				    int vqe, int load_non_overlay)
{
	int ret;

	dev_dbg(p->dev, "%s: non-overlay: %d\n", __func__, load_non_overlay);

#if 0
	/* check if non-overlay firmware is available */
	if (p->vqe_non_overlay_fw && load_non_overlay) {
		ssize_t send;

		/* VA firmware must be active for this */
		if (p->active_fw != DBMDX_FW_VA) {
			dev_err(p->dev,
				"%s: VA firmware must be active for non-overlay loading\n",
				__func__);
			return -EIO;
		}
		/* change to high speed */
		ret = dbmdx_va_set_high_speed(p);
		if (ret != 0) {
			dev_err(p->dev,
				"%s: could not change to high speed\n",
				__func__);
			return -EIO;
		}

		msleep(DBMDX_MSLEEP_NON_OVERLAY_BOOT);

		/* restore AHB memory */
		ret = dbmdx_send_cmd(p,
				     DBMDX_VA_LOAD_NEW_ACUSTIC_MODEL | 2,
				     NULL);
		if (ret != 0) {
			dev_err(p->dev,
				"%s: could not prepare non-overlay loading\n",
				__func__);
			return -EIO;
		}
		usleep_range(10000, 11000);

		/* check if firmware is still alive */
		ret = dbmdx_va_alive(p);
		if (ret)
			return ret;

		/* reset the chip */
		p->reset_sequence(p);

		msleep(DBMDX_MSLEEP_NON_OVERLAY_BOOT);

		/* send non-overlay part */
		send = dbmdx_send_data(p,
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
	ret = dbmdx_firmware_ready(p->vqe_fw, p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not load VQE firmware\n",
			__func__);
		return -EIO;
	}
	dbmdx_set_vqe_active(p);

	ret = p->chip->set_vqe_firmware_ready(p);
	if (ret) {
		dev_err(p->dev, "%s: could not set to ready VQE firmware\n",
			__func__);
		return -EIO;
	}

	/* special setups for the VQE firmware */
	ret = dbmdx_config_vqe_mode(p);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not configure VQE firmware\n",
			__func__);
		return -EIO;
	}

	return 0;
}

static int dbmdx_switch_to_va_firmware(struct dbmdx_private *p, bool do_reset)
{
	int ret;

	if (p->active_fw == DBMDX_FW_VA)
		return 0;

	if (!p->pdata->feature_va) {
		dev_err(p->dev, "%s: VA feature not enabled\n", __func__);
		return -EINVAL;
	}

	if (do_reset)
		dbmdx_set_boot_active(p);

	dbmdx_set_power_mode(p, DBMDX_PM_BOOTING);

	dev_dbg(p->dev, "%s: switching to VA firmware\n", __func__);

	p->device_ready = false;

	ret = dbmdx_va_firmware_ready(p);
	if (ret)
		return ret;

	p->device_ready = true;
	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	return 0;
}

static int dbmdx_switch_to_vqe_firmware(struct dbmdx_private *p, bool do_reset)
{
	int ret;

	if (p->active_fw == DBMDX_FW_VQE)
		return 0;

	if (!p->pdata->feature_vqe) {
		dev_err(p->dev, "%s: VQE feature not enabled\n", __func__);
		return -EINVAL;
	}
	if (do_reset)
		dbmdx_set_boot_active(p);

	dbmdx_set_power_mode(p, DBMDX_PM_BOOTING);

	dev_dbg(p->dev, "%s: switching to VQE firmware\n", __func__);

	p->device_ready = false;

	ret = dbmdx_vqe_firmware_ready(p, 1, 0);
	if (ret)
		return ret;

	p->device_ready = true;
	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	return 0;
}

static int dbmdx_request_and_load_fw(struct dbmdx_private *p,
				     int va, int vqe, int vqe_non_overlay)
{
	int ret;
	int retries = 5;

	dev_dbg(p->dev, "%s %s/%s\n",
		__func__, va ? "VA" : "-", vqe ? "VQE" : "-");

	p->lock(p);

	dbmdx_set_boot_active(p);

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

	ret = dbmdx_set_power_mode(p, DBMDX_PM_BOOTING);
	if (ret != 0) {
		dev_err(p->dev, "%s: could not change to DBMDX_PM_BOOTING\n",
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
				msleep(DBMDX_MSLEEP_REQUEST_FW_FAIL);
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
				msleep(DBMDX_MSLEEP_REQUEST_FW_FAIL);
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
		ret = dbmdx_switch_to_vqe_firmware(p, 1);
		if (ret != 0) {
			dev_err(p->dev, "%s: failed to boot VQE firmware\n",
				__func__);
			ret = -EIO;
			goto out_err;
		}
		dbmdx_vqe_get_ver(p);
	} else if (p->pdata->feature_va && va) {
		ret = dbmdx_switch_to_va_firmware(p, 1);
		if (ret != 0) {
			dev_err(p->dev, "%s: failed to boot VA firmware\n",
				__func__);
			ret = -EIO;
			goto out_err;
		}
	}
	/* fall asleep by default after boot */
	ret = dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	if (ret != 0) {
		dev_err(p->dev,
			"%s: could not change to DBMDX_PM_FALLING_ASLEEP\n",
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
static ssize_t dbmdx_reg_show(struct device *dev, u32 command,
			      struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;
	u16 val = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s: get reg %x\n", __func__, command);

	if ((p->active_fw == DBMDX_FW_VQE) && (command & DBMDX_VA_CMD_MASK)) {
		dev_err(p->dev, "%s: VA mode is not active\n", __func__);
		return -ENODEV;
	}

	if ((p->active_fw == DBMDX_FW_VA) && !(command & DBMDX_VA_CMD_MASK)) {
		dev_err(p->dev, "%s: VQE mode is not active\n", __func__);
		return -ENODEV;
	}

	p->lock(p);

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	ret = dbmdx_send_cmd(p, command, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %x error\n",
				__func__, command);
		goto out_unlock;
	}

	if (command == DBMDX_VA_AUDIO_HISTORY)
		val = val & 0x0fff;

	ret = sprintf(buf, "0x%x\n", val);

out_unlock:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

	p->unlock(p);
	return ret;
}

static ssize_t dbmdx_reg_show_long(struct device *dev,
				   u32 command, u32 command1,
				   struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	ret = dbmdx_send_cmd(p, command1, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %u error\n",
				__func__, command);
		goto out_unlock;
	}

	result = (u32)(val & 0xffff);
	dev_dbg(p->dev, "%s: val = 0x%08x\n", __func__, result);

	val = 0;
	ret = dbmdx_send_cmd(p, command, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: get reg %u error\n",
				__func__, command1);
		goto out_unlock;
	}

	result += ((u32)val << 16);
	dev_dbg(p->dev, "%s: val = 0x%08x\n", __func__, result);

	ret = sprintf(buf, "0x%x\n", result);

out_unlock:

	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

	p->unlock(p);
	return ret;
}

static ssize_t dbmdx_reg_store(struct device *dev, u32 command,
			       struct device_attribute *attr,
			       const char *buf, size_t size, int fw)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	if (fw != p->active_fw) {
		if (fw == DBMDX_FW_VA)
			ret = dbmdx_switch_to_va_firmware(p, 0);
		if (fw == DBMDX_FW_VQE)
			ret = dbmdx_switch_to_vqe_firmware(p, 0);
		if (ret)
			goto out_unlock;
	}

	if (p->active_fw == DBMDX_FW_VA) {
		if (command == DBMDX_VA_OPR_MODE) {
			if (val == 1) {
				/* default detection mode - VT, i.e. PHRASE */
				p->va_detection_mode = DETECTION_MODE_PHRASE;
				ret = dbmdx_trigger_detection(p);
			} else
				ret = dbmdx_set_mode(p, val);
			if (ret)
				size = ret;

			if ((val == 0 || val == 6) &&
				p->va_flags.pcm_streaming_active) {

				p->unlock(p);
				ret = dbmdx_suspend_pcm_streaming_work(p);
				if (ret < 0)
					dev_err(p->dev,
						"%s: Failed to suspend PCM Streaming Work\n",
						__func__);
				p->lock(p);
			}
			goto out_unlock;
		}
#if 0
		if (command == DBMDX_VA_AUDIO_BUFFER_CONVERSION) {
			u16 mode = val & 0x1000;

			ret = dbmdx_set_bytes_per_sample(p, mode);
			if (ret) {
				size = ret;
				goto out_pm_mode;
			}
			command = DBMDX_VA_AUDIO_HISTORY;
		}
#endif
		if (command == DBMDX_VA_AUDIO_HISTORY) {
			ret = dbmdx_set_backlog_len(p, val);
			if (ret < 0) {
				dev_err(p->dev, "%s: set history error\n",
					__func__);
				size = ret;
				goto out_pm_mode;
			}
		}

		ret = dbmdx_send_cmd(p, command | (u32)val, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: set VA reg error\n", __func__);
			size = ret;
			goto out_pm_mode;
		}

		if (command == DBMDX_VA_DIGITAL_GAIN)
			p->va_cur_digital_gain = (int)val;
		else if (command == DBMDX_VA_ANALOG_MIC_GAIN)
			p->va_cur_analog_gain = (int)val;
	}

	if (p->active_fw == DBMDX_FW_VQE) {
		ret = dbmdx_send_cmd(p, command | (u32)val, NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: set VQE reg error\n", __func__);
			size = ret;
			goto out_pm_mode;
		}
	}

out_pm_mode:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

out_unlock:
	p->unlock(p);
	return size;
}

static ssize_t dbmdx_reg_store_long(struct device *dev, u32 command,
				    u32 command1,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);
	ret = dbmdx_send_cmd(p, command1 | (val & 0xffff), NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: set reg error\n", __func__);
		size = ret;
		goto out_unlock;
	}

	ret = dbmdx_send_cmd(p, command | (val >> 16), NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: set reg error\n", __func__);
		size = ret;
		goto out_unlock;
	}

out_unlock:

	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

	p->unlock(p);
	return size;
}

static ssize_t dbmdx_fw_ver_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	/* FIXME: add support for VQE */
	return dbmdx_reg_show(dev, DBMDX_VA_GET_FW_VER, attr, buf);
}

static ssize_t dbmdx_va_opmode_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_OPR_MODE, attr, buf);
}

static ssize_t dbmdx_opr_mode_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_OPR_MODE, attr,
			       buf, size, DBMDX_FW_VA);
}

static ssize_t dbmdx_va_clockcfg_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_CLK_CFG, attr, buf);
}

static ssize_t dbmdx_va_clockcfg_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_CLK_CFG, attr,
			       buf, size, DBMDX_FW_VA);
}


static ssize_t dbmdx_reboot_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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

	p->wakeup_release(p);

	ret = dbmdx_request_and_load_fw(p, va, vqe, non_overlay);
	if (ret != 0)
		return -EIO;

	return size;
}

static ssize_t dbmdx_va_debug_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret = -EINVAL;

	if (!strncmp(buf, "clk_output", min_t(int, size, 10)))
		ret = dbmdx_set_fw_debug_mode(p, FW_DEBUG_OUTPUT_CLK);
	else if (!strncmp(buf, "uart_dbg", min_t(int, size, 8)))
		ret = dbmdx_set_fw_debug_mode(p, FW_DEBUG_OUTPUT_UART);
	else if (!strncmp(buf, "clk_uart_output", min_t(int, size, 15)))
		ret = dbmdx_set_fw_debug_mode(p, FW_DEBUG_OUTPUT_CLK_UART);
	else if (!strncmp(buf, "disable_dbg", min_t(int, size, 11)))
		ret = dbmdx_set_fw_debug_mode(p, FW_DEBUG_OUTPUT_NONE);
	else if (!strncmp(buf, "help", min_t(int, size, 4))) {
		dev_info(p->dev,
			"%s: Commands: clk_output | uart_dbg | clk_uart_output | help\n",
			__func__);
		ret = 0;
	}


	if (ret)
		return ret;

	return size;
}

static ssize_t dbmdx_va_debug_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;

	ret = sprintf(buf,
		"Supported Commands: [clk_output | uart_dbg | clk_uart_output | help]\n");

	return ret;
}

static ssize_t dbmdx_va_speed_cfg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	struct dbmdx_platform_data *pdata;
	char *str_p;
	char *args =  (char *)buf;
	unsigned long val;
	u32 index = 0;
	u32 type = 0;
	u32 new_value = 0;
	bool index_set = false, type_set = false, value_set = false;
	int i;

	int ret = -EINVAL;

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	while ((str_p = strsep(&args, " \t")) != NULL) {

		if (!*str_p)
			continue;

		if (strncmp(str_p, "index=", 6) == 0) {
			ret = kstrtoul((str_p+6), 0, &val);
			if (ret) {
				dev_err(p->dev, "%s: bad index\n", __func__);
				ret = -EINVAL;
				goto print_usage;
			} else if (val > 2) {
				dev_err(p->dev, "%s: index out of range: %d\n",
					__func__, (int)val);
				ret = -EINVAL;
				goto print_usage;
			}
			index = (u32)val;
			index_set = true;
			continue;
		}
		if (strncmp(str_p, "type=", 5) == 0) {
			if (strncmp(str_p+5, "cfg", 3) == 0)
				type = 0;
			else if (strncmp(str_p+5, "uart", 4) == 0)
				type = 1;
			else if (strncmp(str_p+5, "i2c", 3) == 0)
				type = 2;
			else if (strncmp(str_p+5, "spi", 3) == 0)
				type = 3;
			else {
				dev_err(p->dev, "%s: invalid type\n",
					__func__);
				ret = -EINVAL;
				goto print_usage;
			}
			type_set = true;
			continue;
		}
		if (strncmp(str_p, "value=", 6) == 0) {
			ret = kstrtoul((str_p+6), 0, &val);
			if (ret) {
				dev_err(p->dev, "%s: bad value\n", __func__);
				ret = -EINVAL;
				goto print_usage;
			}

			new_value = (u32)val;
			value_set = true;
			continue;
		}
	}

	if (!index_set) {
		dev_err(p->dev, "%s: index is not set\n", __func__);
		ret = -EINVAL;
		goto print_usage;
	} else if (!type_set) {
		dev_err(p->dev, "%s: type is not set\n", __func__);
		ret = -EINVAL;
		goto print_usage;
	} else if (!value_set) {
		dev_err(p->dev, "%s: value is not set\n", __func__);
		ret = -EINVAL;
		goto print_usage;
	}

	p->lock(p);

	if (type == 0) {
		p->pdata->va_speed_cfg[index].cfg = new_value;
		dev_info(p->dev, "%s: va_speed_cfg[%u].cfg was set to %8.8x\n",
			__func__, index, new_value);
	} else if (type == 1) {
		p->pdata->va_speed_cfg[index].uart_baud = new_value;
		dev_info(p->dev, "%s: va_speed_cfg[%u].uart_baud was set to %u\n",
			__func__, index, new_value);
	} else if (type == 2) {
		p->pdata->va_speed_cfg[index].i2c_rate = new_value;
		dev_info(p->dev, "%s: va_speed_cfg[%u].i2c_rate was set to %u\n",
			__func__, index, new_value);
	} else if (type == 3) {
		p->pdata->va_speed_cfg[index].spi_rate = new_value;
		dev_info(p->dev, "%s: va_speed_cfg[%u].spi_rate was set to %u\n",
			__func__, index, new_value);
	}

	p->unlock(p);

	for (i = 0; i < DBMDX_VA_NR_OF_SPEEDS; i++)
		dev_info(dev, "%s: VA speed cfg %8.8x: 0x%8.8x %u %u %u\n",
			__func__,
			i,
			pdata->va_speed_cfg[i].cfg,
			pdata->va_speed_cfg[i].uart_baud,
			pdata->va_speed_cfg[i].i2c_rate,
			pdata->va_speed_cfg[i].spi_rate);

	return size;
print_usage:
	dev_info(p->dev,
		"%s: Usage: index=[0/1/2] type=[cfg/uart/i2c/spi] value=newval\n",
		__func__);
	return ret;
}

static ssize_t dbmdx_va_speed_cfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int off = 0;
	struct dbmdx_platform_data *pdata;
	int i;

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	for (i = 0; i < DBMDX_VA_NR_OF_SPEEDS; i++)
		off += sprintf(buf + off,
			"\tVA speed cfg %8.8x: 0x%8.8x %u %u %u\n",
			i,
			pdata->va_speed_cfg[i].cfg,
			pdata->va_speed_cfg[i].uart_baud,
			pdata->va_speed_cfg[i].i2c_rate,
			pdata->va_speed_cfg[i].spi_rate);

	return off;
}

static ssize_t dbmdx_va_cfg_values_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	struct dbmdx_platform_data *pdata;
	char *str_p;
	char *args =  (char *)buf;
	unsigned long val;
	u32 index = 0;
	u32 new_value = 0;
	bool index_set = false, value_set = false;
	int i;

	int ret = -EINVAL;

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	while ((str_p = strsep(&args, " \t")) != NULL) {

		if (!*str_p)
			continue;

		if (strncmp(str_p, "index=", 6) == 0) {
			ret = kstrtoul((str_p+6), 0, &val);
			if (ret) {
				dev_err(p->dev, "%s: bad index\n", __func__);
				ret = -EINVAL;
				goto print_usage;
			} else if (val >=  pdata->va_cfg_values) {
				dev_err(p->dev, "%s: index out of range: %d\n",
					__func__, (int)val);
				ret = -EINVAL;
				goto print_usage;
			}
			index = (u32)val;
			index_set = true;
			continue;
		}
		if (strncmp(str_p, "value=", 6) == 0) {
			ret = kstrtoul((str_p+6), 0, &val);
			if (ret) {
				dev_err(p->dev, "%s: bad value\n", __func__);
				ret = -EINVAL;
				goto print_usage;
			}

			new_value = (u32)val;
			value_set = true;
			continue;
		}
	}

	if (!index_set) {
		dev_err(p->dev, "%s: index is not set\n", __func__);
		ret = -EINVAL;
		goto print_usage;
	} else if (!value_set) {
		dev_err(p->dev, "%s: value is not set\n", __func__);
		ret = -EINVAL;
		goto print_usage;
	}
	p->lock(p);

	p->pdata->va_cfg_value[index] = new_value;

	dev_info(p->dev, "%s: va_cfg_value[%u] was set to %u\n",
		__func__, index, new_value);

	p->unlock(p);

	for (i = 0; i < pdata->va_cfg_values; i++)
		dev_dbg(dev, "%s: VA cfg %8.8x: 0x%8.8x\n",
			__func__, i, pdata->va_cfg_value[i]);

	return size;
print_usage:
	dev_info(p->dev,
		"%s: Usage: index=[0-%u] value=newval\n",
		__func__, (u32)(pdata->va_cfg_values));
	return ret;
}


static ssize_t dbmdx_va_cfg_values_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int off = 0;
	struct dbmdx_platform_data *pdata;
	int i;

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	for (i = 0; i < pdata->va_cfg_values; i++)
			off += sprintf(buf + off, "\tVA cfg %8.8x: 0x%8.8x\n",
				i, pdata->va_cfg_value[i]);

	return off;
}

static ssize_t dbmdx_va_mic_cfg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	struct dbmdx_platform_data *pdata;
	char *str_p;
	char *args =  (char *)buf;
	unsigned long val;
	u32 index = 0;
	u32 new_value = 0;
	bool index_set = false, value_set = false;
	int i;

	int ret = -EINVAL;

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	while ((str_p = strsep(&args, " \t")) != NULL) {

		if (!*str_p)
			continue;

		if (strncmp(str_p, "index=", 6) == 0) {
			ret = kstrtoul((str_p+6), 0, &val);
			if (ret) {
				dev_err(p->dev, "%s: bad index\n", __func__);
				ret = -EINVAL;
				goto print_usage;
			} else if (val >  2) {
				dev_err(p->dev, "%s: index out of range: %d\n",
					__func__, (int)val);
				ret = -EINVAL;
				goto print_usage;
			}
			index = (u32)val;
			index_set = true;
			continue;
		}
		if (strncmp(str_p, "value=", 6) == 0) {
			ret = kstrtoul((str_p+6), 0, &val);
			if (ret) {
				dev_err(p->dev, "%s: bad value\n", __func__);
				ret = -EINVAL;
				goto print_usage;
			}

			new_value = (u32)val;
			value_set = true;
			continue;
		}
	}

	if (!index_set) {
		dev_err(p->dev, "%s: index is not set\n", __func__);
		ret = -EINVAL;
		goto print_usage;
	} else if (!value_set) {
		dev_err(p->dev, "%s: value is not set\n", __func__);
		ret = -EINVAL;
		goto print_usage;
	}
	p->lock(p);

	p->pdata->va_mic_config[index] = new_value;

	dev_info(p->dev, "%s: va_mic_config[%u] was set to %u\n",
		__func__, index, new_value);

	p->unlock(p);

	for (i = 0; i < 3; i++)
		dev_dbg(dev, "%s: VA mic cfg %8.8x: 0x%8.8x\n",
			__func__, i, pdata->va_mic_config[i]);

	return size;
print_usage:
	dev_info(p->dev,
		"%s: Usage: index=[0-2] value=newval\n",
		__func__);
	return ret;
}


static ssize_t dbmdx_va_mic_cfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int off = 0;
	struct dbmdx_platform_data *pdata;
	int i;

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	for (i = 0; i < 3; i++)
		off += sprintf(buf + off, "\tVA mic cfg %8.8x: 0x%8.8x\n",
			i, pdata->va_mic_config[i]);

	return off;
}


static ssize_t dbmdx_va_trigger_level_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_SENS_TG_THRESHOLD, attr, buf);
}

static ssize_t dbmdx_va_trigger_level_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_SENS_TG_THRESHOLD, attr,
			       buf, size, DBMDX_FW_VA);
}

static ssize_t dbmdx_va_verification_level_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_SENS_VERIF_THRESHOLD, attr, buf);
}

static ssize_t dbmdx_va_verification_level_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_SENS_VERIF_THRESHOLD, attr, buf,
			      size, DBMDX_FW_VA);
}

static ssize_t dbmdx_va_digital_gain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_DIGITAL_GAIN, attr, buf);
}

static ssize_t dbmdx_va_digital_gain_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_DIGITAL_GAIN, attr,
			       buf, size, DBMDX_FW_VA);
}

static ssize_t dbmdx_io_addr_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show_long(dev, DBMDX_VA_IO_PORT_ADDR_HI,
				  DBMDX_VA_IO_PORT_ADDR_LO, attr, buf);
}

static ssize_t dbmdx_io_addr_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	return dbmdx_reg_store_long(dev, DBMDX_VA_IO_PORT_ADDR_HI,
				   DBMDX_VA_IO_PORT_ADDR_LO, attr, buf, size);
}

static int dbmdx_va_amodel_build_load(struct dbmdx_private *p,
			const char *dbmdx_gram_name,
			const char *dbmdx_net_name,
			enum dbmdx_load_amodel_mode amode)
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

	if (!dbmdx_gram_name[0] || !dbmdx_net_name[0]) {
		dev_err(p->dev, "%s: Unknown amodel file name\n",
				__func__);
			return -EIO;
	}

	dev_dbg(p->dev, "%s: loading %s %s\n", __func__,
		dbmdx_gram_name, dbmdx_net_name);

	ret = request_firmware((const struct firmware **)&va_gram_fw,
				dbmdx_gram_name,
				p->dbmdx_dev);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to request VA gram firmware(%d)\n",
			__func__, ret);
		return ret;
	}

	dev_info(p->dev, "%s: gram firmware requested\n", __func__);

	dev_dbg(p->dev, "%s gram=%zu bytes\n",
		__func__, va_gram_fw->size);

	if (!va_gram_fw->size) {
		dev_warn(p->dev, "%s gram size is 0. Ignore...\n",
			__func__);
		ret = -EIO;
		goto release;
	}

	ret = request_firmware((const struct firmware **)&va_net_fw,
				dbmdx_net_name,
				p->dbmdx_dev);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to request VA net firmware(%d)\n",
			__func__, ret);
		ret = -EIO;
		goto release;
	}

	dev_info(p->dev, "%s: net firmware requested\n", __func__);

	dev_dbg(p->dev, "%s net=%zu bytes\n",
		__func__, va_net_fw->size);

	if (!va_net_fw->size) {
		dev_warn(p->dev, "%s net size is 0. Ignore...\n",
			__func__);
		ret = -EIO;
		goto release;
	}

	p->lock(p);

	p->va_flags.amodel_len = 0;

	ret = dbmdx_acoustic_model_build(p,
					va_gram_fw->data,
					va_gram_fw->size,
					va_net_fw->data,
					va_net_fw->size);

	p->unlock(p);

	if (ret <= 0) {
		dev_err(p->dev,	"%s: amodel build failed: %d\n",
			__func__, ret);
		ret = -EIO;
		goto release;
	}

	if (p->va_flags.amodel_len > 0) {
		ret = dbmdx_acoustic_model_load(
			p, p->amodel_buf,
			p->va_flags.amodel_len,
			va_gram_fw->size,
			va_net_fw->size,
			amode);
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

static int dbmdx_va_amodel_update(struct dbmdx_private *p, int val)
{
	int ret;
	const char *fname_gram;
	const char *fname_net;

	if (val < 0 || val > DETECTION_MODE_MAX) {
		dev_err(p->dev,
		   "%s: Unsupported detection mode:%d\n", __func__, val);
		return -1;
	}

	dev_dbg(p->dev, "%s: val - %d\n", __func__, val);

	/* flush pending sv work if any */
	p->va_flags.buffering = 0;
	flush_work(&p->sv_work);

	ret = dbmdx_suspend_pcm_streaming_work(p);
	if (ret < 0)
		dev_err(p->dev, "%s: Failed to suspend PCM Streaming Work\n",
			__func__);

	if (val == 0) {

		if (p->active_fw != DBMDX_FW_VA) {
			dev_err(p->dev, "%s: VA firmware not active, error\n",
				__func__);
			return -EAGAIN;
		}
		p->lock(p);

		if (dbmdx_sleeping(p)) {
			p->unlock(p);
			return 0;
		}

		ret = dbmdx_set_mode(p, DBMDX_IDLE);
		if (ret) {
			dev_err(p->dev,
				"%s: failed to set device to idle mode\n",
				__func__);
			p->unlock(p);
			return -1;
		}

		if (p->va_flags.pcm_streaming_active) {

			ret = dbmdx_set_mode(p, DBMDX_STREAMING);

			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to set DBMDX_STREAMING mode\n",
					__func__);
				p->unlock(p);
				return -1;
			}

		}

		p->unlock(p);

		return 0;
	}

	if (p->active_fw == DBMDX_FW_VQE) {
		if (p->vqe_flags.in_call) {
			dev_err(p->dev,
				"%s: Switching to VA is not allowed when in CALL\n",
				__func__);
			return -EAGAIN;
		}

		dev_info(p->dev, "%s: VA firmware not active, switching\n",
			__func__);

		p->lock(p);
		ret = dbmdx_switch_to_va_firmware(p, 0);
		p->unlock(p);
		if (ret != 0) {
			dev_err(p->dev, "%s: Error switching to VA firmware\n",
				__func__);
			return -EIO;
		}
	}

	p->lock(p);

	switch (val) {
	case DETECTION_MODE_VOICE_COMMAND:
		p->va_capture_on_detect = false;
		p->va_detection_mode = val;
		fname_gram = DBMDX_VC_GRAM_NAME;
		fname_net = DBMDX_VC_NET_NAME;
		break;
	case DETECTION_MODE_DUAL:
		p->va_capture_on_detect = false;
		p->va_detection_mode = val;
		fname_gram = DBMDX_VT_GRAM_NAME;
		fname_net = DBMDX_VT_NET_NAME;
		break;
	case DETECTION_MODE_VOICE_ENERGY:
		p->va_capture_on_detect = true;
		p->va_detection_mode = val;
		ret = dbmdx_trigger_detection(p);
		p->unlock(p);
		return ret;
	case DETECTION_MODE_PHRASE:
		p->va_capture_on_detect = true;
		fname_gram = DBMDX_VT_GRAM_NAME;
		fname_net = DBMDX_VT_NET_NAME;
		p->va_detection_mode = val;
		break;
	case DETECTION_MODE_PHRASE_DONT_LOAD:
		/*
		 * special case - don't load a-model, simply enforce detection
		 * and exit
		 */
		dev_info(p->dev, "%s: direct detection requisted\n", __func__);
		p->va_detection_mode = DETECTION_MODE_PHRASE;
		ret = dbmdx_trigger_detection(p);
		p->unlock(p);
		return ret;
	default:
		dev_err(p->dev,
			"%s: Error unknown detection mode:%d", __func__, val);
		p->unlock(p);
		return -EINVAL;
	}

	dev_info(p->dev, "%s: detection mode:%d", __func__, val);
	p->unlock(p);

	if (p->va_detection_mode == DETECTION_MODE_DUAL) {
		ret = dbmdx_va_amodel_build_load(p, DBMDX_VC_GRAM_NAME,
			DBMDX_VC_NET_NAME, LOAD_AMODEL_2NDARY);
		if (ret < 0)
			dev_err(p->dev,
				"%s: Error loading acoustic model:%d\n",
				__func__, val);

		ret = dbmdx_va_amodel_build_load(p, fname_gram,
				fname_net, LOAD_AMODEL_PRIMARY);
		if (ret < 0)
			dev_err(p->dev,
				"%s: Error loading acoustic model:%d\n",
				__func__, val);
	} else {
		ret = dbmdx_va_amodel_build_load(p, fname_gram,
				fname_net, LOAD_AMODEL_PRIMARY);
		if (ret < 0)
			dev_err(p->dev,
				"%s: Error loading acoustic model:%d\n",
				__func__, val);
	}

	return ret;
}


static ssize_t dbmdx_va_acoustic_model_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;
	int val = dbmdx_buf_to_int(buf);

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = dbmdx_va_amodel_update(p, val);

	if (ret < 0 && !p->pdata->va_recovery_disabled) {

		int recovery_res;

		if (p->device_ready && (dbmdx_va_alive_with_lock(p) == 0)) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #1\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

		ret = dbmdx_va_amodel_update(p, val);

		if (ret == 0) {
			dev_err(p->dev,
				"%s: Amodel was loaded after succesfull recovery\n",
				__func__);
			goto out;
		}

		if (p->device_ready && (dbmdx_va_alive_with_lock(p) == 0)) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #2\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

	}
out:
	return (ret < 0 ? ret : size);
}

static ssize_t dbmdx_va_max_sample_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_LAST_MAX_SMP_VALUE, attr, buf);
}

static ssize_t dbmdx_io_value_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show_long(dev, DBMDX_VA_IO_PORT_VALUE_HI,
				  DBMDX_VA_IO_PORT_VALUE_LO, attr, buf);
}

static ssize_t dbmdx_io_value_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return dbmdx_reg_store_long(dev, DBMDX_VA_IO_PORT_VALUE_HI,
				   DBMDX_VA_IO_PORT_VALUE_LO, attr, buf, size);
}

static ssize_t dbmdx_va_buffer_size_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_AUDIO_BUFFER_SIZE, attr, buf);
}

static ssize_t dbmdx_va_buffer_size_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	return dbmdx_reg_store(dev,
			       DBMDX_VA_AUDIO_BUFFER_SIZE,
			       attr,
			       buf,
			       size,
			       DBMDX_FW_VA);
}

static ssize_t dbmdx_va_buffsmps_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_NUM_OF_SMP_IN_BUF, attr, buf);
}

static ssize_t dbmdx_va_capture_on_detect_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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

static ssize_t dbmdx_va_capture_on_detect_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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

static ssize_t dbmdx_va_audio_conv_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_AUDIO_BUFFER_CONVERSION, attr, buf);
}

static ssize_t dbmdx_va_audio_conv_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_AUDIO_BUFFER_CONVERSION, attr, buf,
			      size, DBMDX_FW_VA);
}

static ssize_t dbmdx_va_analog_micgain_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_ANALOG_MIC_GAIN, attr, buf);
}

static ssize_t dbmdx_va_analog_micgain_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_ANALOG_MIC_GAIN, attr,
			       buf, size, DBMDX_FW_VA);
}

static ssize_t dbmdx_va_backlog_size_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VA_AUDIO_HISTORY, attr, buf);
}

static ssize_t dbmdx_va_backlog_size_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf,
				       size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VA_AUDIO_HISTORY, attr, buf,
			      size, DBMDX_FW_VA);
}

static ssize_t dbmdx_reset_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;
	unsigned long val;

	if (!p)
		return -EAGAIN;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	dev_dbg(p->dev, "%s: Val = %d\n", __func__, (int)val);

	if (val == 0) {
		ret = dbmdx_perform_recovery(p);
		if (ret) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			return -EIO;
		}
	} else if (val == 1) {
		p->va_flags.buffering = 0;
		flush_work(&p->sv_work);

		ret = dbmdx_suspend_pcm_streaming_work(p);
		if (ret < 0)
			dev_err(p->dev,
				"%s: Failed to suspend PCM Streaming Work\n",
				__func__);

		p->device_ready = false;

		dbmdx_reset_set(p);

		dev_info(p->dev, "%s: DBMDX Chip is in Reset state\n",
			__func__);

	} else
		return -EINVAL;

	return size;
}

static ssize_t dbmdx_param_addr_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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
	ret = dbmdx_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		p->unlock(p);
		return ret;
	}

	if (p->active_fw == DBMDX_FW_VQE)
		ret = dbmdx_send_cmd(p,
			DBMDX_VQE_SET_INDIRECT_REG_ADDR_ACCESS_CMD | (u32)val,
			NULL);
	else
		ret = dbmdx_send_cmd(p,
				     DBMDX_VA_SET_PARAM_ADDR | (u32)val,
				     NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: set paramaddr error\n", __func__);
		p->unlock(p);
		return ret;
	}

	p->unlock(p);
	return size;
}

static ssize_t dbmdx_param_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;
	u16 val;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);
	ret = dbmdx_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		p->unlock(p);
		return ret;
	}

	if (p->active_fw == DBMDX_FW_VQE)
		ret = dbmdx_send_cmd(p,
			DBMDX_VQE_GET_INDIRECT_REG_DATA_ACCESS_CMD,
			&val);
	else
		ret = dbmdx_send_cmd(p, DBMDX_VA_GET_PARAM, &val);

	if (ret < 0) {
		dev_err(p->dev, "%s: get param error\n", __func__);
		p->unlock(p);
		return ret;
	}

	p->unlock(p);
	return sprintf(buf, "%d\n", (s16)val);
}

static ssize_t dbmdx_param_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = kstrtol(buf, 0, &val);
	if (ret)
		return -EINVAL;

	p->lock(p);
	ret = dbmdx_wake(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: unable to wake\n", __func__);
		p->unlock(p);
		return ret;
	}

	if (p->active_fw == DBMDX_FW_VQE)
		ret = dbmdx_send_cmd(p,
			DBMDX_VQE_SET_INDIRECT_REG_DATA_ACCESS_CMD | (u32)val,
			NULL);
	else
		ret = dbmdx_send_cmd(p, DBMDX_VA_SET_PARAM | (u32)val, NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: set param error\n", __func__);
		p->unlock(p);
		return ret;
	}

	p->unlock(p);
	return size;
}

static ssize_t dbmdx_va_mic_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = sprintf(buf, "%d\n",  p->va_active_mic_config);

	return ret;
}

static ssize_t dbmdx_va_mic_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtol(buf, 0, &val);
	if (ret)
		return -EINVAL;

	dev_dbg(p->dev, "%s: val - %d\n", __func__, (int)val);


	switch (val) {
	case DBMDX_MIC_MODE_DIGITAL_LEFT:
	case DBMDX_MIC_MODE_DIGITAL_RIGHT:
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
	case DBMDX_MIC_MODE_ANALOG:
	case DBMDX_MIC_MODE_DISABLE:
		ret = dbmdx_reconfigure_microphones(p, (int)(val));
		break;
	default:
		dev_err(p->dev, "%s: unsupported microphone mode %d\n",
			__func__, (int)(val));
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(p->dev, "%s: set microphone mode error\n", __func__);
		size = ret;
	}

	return size;
}

static ssize_t dbmdx_va_detection_buffer_channels_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = sprintf(buf, "%d\n",  p->pdata->detection_buffer_channels);

	return ret;
}

static ssize_t dbmdx_va_detection_buffer_channels_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtol(buf, 0, &val);
	if (ret)
		return -EINVAL;

	dev_dbg(p->dev, "%s: val - %d\n", __func__, (int)val);

	if (val > 2) {
		dev_err(p->dev,
			"%s: invalid detection_buffer_channels value %d\n",
			__func__, (int)(val));

		return -EINVAL;
	}

	if (val == p->pdata->detection_buffer_channels)
		return size;

	p->pdata->detection_buffer_channels = val;

	ret = dbmdx_reconfigure_microphones(p, p->va_active_mic_config);

	if (ret < 0) {
		dev_err(p->dev, "%s: set microphone mode error\n", __func__);
		size = ret;
	}

	return size;
}

static ssize_t dbmdx_va_min_samples_chunk_size_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = sprintf(buf, "%u\n",  p->pdata->min_samples_chunk_size);

	return ret;
}

static ssize_t dbmdx_va_min_samples_chunk_size_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	ret = kstrtol(buf, 0, &val);
	if (ret)
		return -EINVAL;

	dev_dbg(p->dev, "%s: val - %d\n", __func__, (int)val);

	if (val < 0) {
		dev_err(p->dev,
			"%s: invalid min_samples_chunk_size value %d\n",
			__func__, (int)(val));

		return -EINVAL;
	}

	p->pdata->min_samples_chunk_size = (u32)val;

	return size;
}

static ssize_t dbmdx_dump_state(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int off = 0;
	struct dbmdx_platform_data *pdata;
	int i;

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	off += sprintf(buf + off, "\tDBMDX Driver Ver:\t%s\n",
		DRIVER_VERSION);
	off += sprintf(buf + off, "\tActive firmware:\t%s\n",
		p->active_fw == DBMDX_FW_VQE ? "VQE" :
		p->active_fw == DBMDX_FW_VA ? "VA" : "PRE_BOOT");
	off += sprintf(buf + off, "\tPower mode:\t\t%s\n",
				dbmdx_power_mode_names[p->power_mode]);
	off += sprintf(buf + off, "\tActive Interface :\t%s\n",
			p->active_interface == DBMDX_INTERFACE_I2C ?
				"I2C" :
				p->active_interface ==
					DBMDX_INTERFACE_SPI ?
				"SPI" :
				p->active_interface ==
					DBMDX_INTERFACE_UART ?
				"UART" : "NONE");

	off += sprintf(buf + off, "\t=======VA Dump==========\n");
	off += sprintf(buf + off, "\t-------VA Current Settings------\n");
	off += sprintf(buf + off, "\tVA Backlog_length\t%d\n",
				p->va_backlog_length);
	off += sprintf(buf + off, "\tVA Active Microphone conf:\t%d\n",
				p->va_active_mic_config);
	off += sprintf(buf + off, "\tVA Cur Microphone conf:\t%d\n",
				p->va_current_mic_config);
	off += sprintf(buf + off, "\tVA Audio Channels:\t%d\n",
				p->pdata->va_audio_channels);
	off += sprintf(buf + off, "\tVA PCM Audio Channels:\t%d\n",
				p->audio_pcm_channels);
	off += sprintf(buf + off, "\tVA PCM Channel Operation:\t%d\n",
				p->pcm_achannel_op);
	off += sprintf(buf + off, "\tVA Detection Channel Operation:\t%d\n",
				p->detection_achannel_op);
	off += sprintf(buf + off, "\tVA Detection KFIFO size:\t%d\n",
					p->detection_samples_kfifo_buf_size);
	off += sprintf(buf + off, "\tVA Detection mode:\t%s\n",
			p->va_detection_mode == DETECTION_MODE_PHRASE ?
				"PASSPHRASE" :
				p->va_detection_mode ==
					DETECTION_MODE_VOICE_ENERGY ?
				"VOICE_ENERGY" :
				p->va_detection_mode ==
					DETECTION_MODE_VOICE_COMMAND ?
				"VOICE_COMMAND" : "VOICE_DUAL");
	off += sprintf(buf + off, "\tVA Capture On Detect:\t%s\n",
			p->va_capture_on_detect ? "ON" : "OFF");
	off += sprintf(buf + off, "\tSample size:\t\t%d\n",
				p->audio_mode);
	off += sprintf(buf + off, "\t-------VA Status------\n");
	off += sprintf(buf + off, "\tVA Device Ready:\t%s\n",
				p->device_ready ? "Yes" : "No");
	off += sprintf(buf + off, "\tVA Operational mode:\t%s\n",
				dbmdx_state_names[p->va_flags.mode]);
	off += sprintf(buf + off, "\tAcoustic model:\t\t%s\n",
				p->va_flags.a_model_loaded == 1 ?
					"Loaded" : "None");
	off += sprintf(buf + off, "\tAcoustic model size\t%d bytes\n",
				p->va_flags.amodel_len);
	off += sprintf(buf + off, "\tFW VAD TYPE:\t\t%d\n",
				p->fw_vad_type);
	off += sprintf(buf + off, "\tCurrent PCM rate:\t\t%d\n",
				p->current_pcm_rate);
	off += sprintf(buf + off, "\tBuffering:\t\t%s\n",
			p->va_flags.buffering ? "ON" : "OFF");
	off += sprintf(buf + off, "\tPCM Streaming Active:\t%s\n",
			p->va_flags.pcm_streaming_active ? "ON" : "OFF");
	off += sprintf(buf + off, "\tPCM Worker Active:	\t%s\n",
			p->va_flags.pcm_worker_active ? "ON" : "OFF");
	off += sprintf(buf + off, "\tIRQ In USE:\t\t%s\n",
			p->va_flags.irq_inuse ? "ON" : "OFF");

	off += sprintf(buf + off, "\t=======VQE Dump==========\n");
	off += sprintf(buf + off, "\tIn Call:\t\t%s\n",
			p->vqe_flags.in_call ? "Yes" : "No");
	off += sprintf(buf + off, "\tVQE Use Case:\t\t%d\n",
		p->vqe_flags.use_case);
	off += sprintf(buf + off, "\tVQE Speaker Vol Lvl:\t%d\n",
		p->vqe_flags.speaker_volume_level);
	off += sprintf(buf + off, "\tVQE VC syscfg:\t\t%d\n",
		p->vqe_vc_syscfg);

	off += sprintf(buf + off, "\t=======Interfaces Dump======\n");

	if (p->chip->dump)
		off += p->chip->dump(p, buf + off);


	off += sprintf(buf + off, "\t=======Initial VA Configuration======\n");

	off += sprintf(buf + off, "\tVA firmware name: %s\n",
			pdata->va_firmware_name);

	/* check for features */
	if (pdata->feature_va)
		off += sprintf(buf + off, "\tVA feature activated\n");
	else
		off += sprintf(buf + off, "\tVA feature not activated\n");

	for (i = 0; i < pdata->va_cfg_values; i++)
			off += sprintf(buf + off, "\tVA cfg %8.8x: 0x%8.8x\n",
				i, pdata->va_cfg_value[i]);

	for (i = 0; i < DBMDX_VA_NR_OF_SPEEDS; i++)
		off += sprintf(buf + off,
			"\tVA speed cfg %8.8x: 0x%8.8x %u %u %u\n",
			i,
			pdata->va_speed_cfg[i].cfg,
			pdata->va_speed_cfg[i].uart_baud,
			pdata->va_speed_cfg[i].i2c_rate,
			pdata->va_speed_cfg[i].spi_rate);

	for (i = 0; i < 3; i++)
		off += sprintf(buf + off, "\tVA mic cfg %8.8x: 0x%8.8x\n",
			i, pdata->va_mic_config[i]);

	off += sprintf(buf + off, "\tVA default mic config: 0x%8.8x\n",
		 pdata->va_initial_mic_config);

	off += sprintf(buf + off, "\tDefault backlog length of %d\n",
		p->va_backlog_length);

	off += sprintf(buf + off, "\tmaster-clk-rate of %dHZ\n",
		pdata->master_clk_rate);


	off += sprintf(buf + off, "\tuse_gpio_for_wakeup %d\n",
		 pdata->use_gpio_for_wakeup);

	off += sprintf(buf + off, "\tsend_wakeup_seq %d\n",
		 pdata->send_wakeup_seq);

	off += sprintf(buf + off, "\twakeup_set_value %d\n",
		 pdata->wakeup_set_value);

	off += sprintf(buf + off, "\tfirmware_id 0x%8x\n",
		 pdata->firmware_id);

	off += sprintf(buf + off, "\tauto_buffering %d\n",
		 pdata->auto_buffering);

	off += sprintf(buf + off, "\tauto_detection %d\n",
		 pdata->auto_detection);

	off += sprintf(buf + off, "\tsend_uevent_on_detection %d\n",
		 pdata->send_uevent_on_detection);

	off += sprintf(buf + off, "\tuart_low_speed_enabled %d\n",
		 pdata->uart_low_speed_enabled);

	off += sprintf(buf + off, "\tpcm streaming mode %d\n",
		 pdata->pcm_streaming_mode);

	off += sprintf(buf + off, "\tmin_samples_chunk_size %d\n",
		 pdata->min_samples_chunk_size);

	off += sprintf(buf + off, "\tdetection_buffer_channels %d\n",
		 pdata->detection_buffer_channels);

	off += sprintf(buf + off, "\tva_buffering_pcm_rate %u\n",
		pdata->va_buffering_pcm_rate);

	off += sprintf(buf + off, "\t=======Initial VQE Configuration======\n");

	off += sprintf(buf + off, "\tVQE firmware name: %s\n",
			pdata->vqe_firmware_name);

	if (pdata->feature_vqe)
		off += sprintf(buf + off, "\tVQE feature activated\n");
	else
		off += sprintf(buf + off, "\tVQE feature not activated\n");

	if (pdata->feature_fw_overlay)
		off += sprintf(buf + off, "\tFirmware overlay activated\n");
	else
		off += sprintf(buf + off, "\tFirmware overlay not activated\n");

	for (i = 0; i < pdata->vqe_cfg_values; i++)
		off += sprintf(buf + off, "\tVQE cfg %8.8x: 0x%8.8x\n",
			i, pdata->vqe_cfg_value[i]);

	for (i = 0; i < pdata->vqe_modes_values; i++)
		off += sprintf(buf + off, "\tVQE mode %8.8x: 0x%8.8x\n",
			i, pdata->vqe_modes_value[i]);

	off += sprintf(buf + off, "\tVQE TDM bypass config: 0x%8.8x\n",
		pdata->vqe_tdm_bypass_config);


	return off;
}

#define MAX_REGS_NUM 112 /*0x6F + 1*/
static ssize_t dbmdx_dump_reg_show(struct device *dev, u16 *reg_buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret, i;
	u16 val = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	for (i = 0; i < MAX_REGS_NUM; i++) {
		ret = dbmdx_send_cmd(p, (DBMDX_VA_CMD_MASK | i<<16), &val);
		if (ret < 0) {
			dev_err(p->dev, "%s: get reg %x error\n",
					__func__, DBMDX_VA_CMD_MASK | i);
			goto out_unlock;
		}

		reg_buf[i] = val;
	}

out_unlock:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

	p->unlock(p);
	return ret;
}


static ssize_t dbmdx_va_regs_dump_state(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int off = 0;
	int i;
	struct dbmdx_platform_data *pdata;
	u16 reg_buf[MAX_REGS_NUM];

	if (!p)
		return -EAGAIN;

	pdata = p->pdata;

	dbmdx_dump_reg_show(dev, &reg_buf[0]);

	off += sprintf(buf + off, "\n\n");
	off += sprintf(buf + off, "dbmdx:\n");
	off += sprintf(buf + off, "Registers Dump:\n");
	off += sprintf(buf + off, "register HEX(dec) : value HEX\n");
	off += sprintf(buf + off, "\n");

	for (i = 0; i < MAX_REGS_NUM; i++) {

		if (i == 0x40)
			off += sprintf(buf + off,
					"\nSV parameters direct access registers:\n");

		off += sprintf(buf + off,
				"0x%02X(%02i) : 0x%04X    ", i, i, reg_buf[i]);

		if (!((i+1)%4))
			off += sprintf(buf + off, "\n");
	}

	off += sprintf(buf + off, "\n\n");

	return off;
}

static ssize_t dbmdx_va_rxsize_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", p->rxsize);
}

static ssize_t dbmdx_va_rxsize_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct dbmdx_private *p = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (val % 16 != 0)
		return -EINVAL;

	p->rxsize = val;

	return size;
}

static ssize_t dbmdx_va_rsize_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	return sprintf(buf, "%u\n", p->chip->get_read_chunk_size(p));

}

static ssize_t dbmdx_va_rsize_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct dbmdx_private *p = dev_get_drvdata(dev);

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
	ret = p->chip->set_read_chunk_size(p, (u32)val);
	p->unlock(p);

	if (ret < 0)
		return -EINVAL;

	return size;
}

static ssize_t dbmdx_va_wsize_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	return sprintf(buf, "%u\n", p->chip->get_write_chunk_size(p));
}

static ssize_t dbmdx_va_wsize_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int ret;
	unsigned long val;
	struct dbmdx_private *p = dev_get_drvdata(dev);

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
	ret = p->chip->set_write_chunk_size(p, (u32)val);
	p->unlock(p);

	if (ret < 0)
		return -EINVAL;

	return size;
}

static ssize_t dbmdx_power_mode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);

	return sprintf(buf, "%s: %s (%d)\n",
		       p->active_fw == DBMDX_FW_VA ? "VA" : "VQE",
		       dbmdx_power_mode_names[p->power_mode],
		       p->power_mode);
}

static ssize_t dbmdx_cur_opmode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);

	return sprintf(buf, "%s (%x)\n", dbmdx_state_names[p->va_flags.mode],
		p->va_flags.mode);
}

static ssize_t dbmdx_vqe_ping_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s\n", __func__);

	if (p->active_fw != DBMDX_FW_VQE)
		return sprintf(buf, "VQE firmware not loaded\n");

	p->lock(p);
	ret = dbmdx_vqe_alive(p);
	p->unlock(p);
	if (ret != 0)
		ret = sprintf(buf, "VQE firmware dead\n");
	else
		ret = sprintf(buf, "VQE firmware alive\n");
	return ret;
}

static ssize_t dbmdx_vqe_use_case_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (p->active_fw == DBMDX_FW_VQE) {
		if (!p->vqe_flags.in_call)
			/* special value as used to terminate call */
			return sprintf(buf, "0x100");

		return dbmdx_reg_show(dev,
				      DBMDX_VQE_GET_USE_CASE_CMD,
				      attr,
				      buf);
	}

	dev_err(p->dev, "%s: VQE firmware not active\n", __func__);
	return -EIO;
}

static int dbmdx_vqe_activate_call(struct dbmdx_private *p, unsigned long val)
{
	int ret;

	dev_dbg(p->dev, "%s: val: 0x%04x\n", __func__, (u16)val);

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_SYSTEM_CONFIG_CMD |
				p->vqe_vc_syscfg,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_USE_CASE_CMD | val, NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}

	p->vqe_flags.in_call = 1;
	p->vqe_flags.use_case = val;

out_fail:
	return ret;
}

static int dbmdx_vqe_change_call_use_case(
		struct dbmdx_private *p, unsigned long val)
{
	int ret;

	dev_dbg(p->dev, "%s: val: 0x%04x\n", __func__, (u16)val);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_FADE_IN_OUT_CMD |
				DBMDX_VQE_SET_FADE_IN_OUT_FADE_OUT_EN,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}
	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_USE_CASE_CMD | val, NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error\n", __func__);
		goto out_fail;
	}

	p->vqe_flags.use_case = val;

out_fail:
	return ret;
}

static int dbmdx_vqe_terminate_call(struct dbmdx_private *p, unsigned long val)
{
	int ret;

	dev_dbg(p->dev, "%s: val: 0x%04x\n", __func__, (u16)val);

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_FADE_IN_OUT_CMD |
				DBMDX_VQE_SET_FADE_IN_OUT_FADE_OUT_EN,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error FADE_OUT_EN\n", __func__);
		goto out_fail;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_USE_CASE_CMD |
				DBMDX_VQE_SET_USE_CASE_CMD_IDLE,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error USE_CASE_CMD_IDLE\n", __func__);
		goto out_fail;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_SYSTEM_CONFIG_CMD |
				DBMDX_VQE_SET_SYSTEM_CONFIG_PRIMARY_CFG,
				NULL);
	if (ret < 0) {
		dev_err(p->dev, "%s: error _CONFIG_PRIMARY_CFG\n", __func__);
		goto out_fail;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VQE_SET_HW_TDM_BYPASS_CMD |
				DBMDX_VQE_SET_HW_TDM_BYPASS_MODE_1 |
				DBMDX_VQE_SET_HW_TDM_BYPASS_FIRST_PAIR_EN,
				NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: HW_TDM_BYPASS_MODE_1: sys is not ready\n",
				__func__);
		goto out_fail;
	}

	p->vqe_flags.in_call = 0;

	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

out_fail:
	return ret;
}

static ssize_t dbmdx_vqe_use_case_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret;
	struct dbmdx_private *p = dev_get_drvdata(dev);
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return -EINVAL;

	if (!p)
		return -EAGAIN;

	ret = dbmdx_vqe_mode_valid(p, (u32)val);
	if (!ret) {
		dev_err(p->dev, "%s: Invalid VQE mode 0x%x\n",
			__func__, (u32)val);
		return -EINVAL;
	}

	if (p->active_fw != DBMDX_FW_VQE) {
		dev_info(p->dev, "%s: VQE firmware not active, switching\n",
			__func__);
		if (p->va_flags.mode != DBMDX_IDLE) {
			p->lock(p);
			ret = dbmdx_set_mode(p, DBMDX_IDLE);
			p->unlock(p);
			if (ret)
				dev_err(p->dev,
					"%s: failed to set device to idle mode\n",
					__func__);
		}
		p->lock(p);
		ret = dbmdx_switch_to_vqe_firmware(p, 0);
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
	if (val & DBMDX_VQE_SET_USE_CASE_DE_ACT_MASK) {
		if (p->vqe_flags.in_call)
			ret = dbmdx_vqe_terminate_call(p, val);
		else
			/* simply re-ensure the sleep mode */
			ret = dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	} else if (p->vqe_flags.in_call)
		/* already in call */
		ret = dbmdx_vqe_change_call_use_case(p, val);
	else {
		ret = dbmdx_vqe_activate_call(p, val);
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



static ssize_t dbmdx_vqe_d2syscfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VQE_GET_SYSTEM_CONFIG_CMD,
					 attr, buf);
}

static ssize_t dbmdx_vqe_d2syscfg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VQE_SET_SYSTEM_CONFIG_CMD, attr,
				 buf, size, DBMDX_FW_VQE);
}

static ssize_t dbmdx_vqe_vc_syscfg_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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


static ssize_t dbmdx_vqe_vc_syscfg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
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


static ssize_t dbmdx_vqe_hwbypass_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VQE_SET_HW_TDM_BYPASS_CMD, attr,
				 buf, size, DBMDX_FW_VQE);
}

static ssize_t dbmdx_vqe_spkvollvl_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return dbmdx_reg_show(dev, DBMDX_VQE_GET_SPK_VOL_LVL_CMD,
					 attr, buf);
}

static ssize_t dbmdx_vqe_spkvollvl_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return dbmdx_reg_store(dev, DBMDX_VQE_SET_SPK_VOL_LVL_CMD, attr,
				 buf, size, DBMDX_FW_VQE);
}

static ssize_t dbmdx_wakeup_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);
	int ret;
	int gpio_val;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	if (!dbmdx_can_wakeup(p))
		ret = sprintf(buf, "No WakeUp GPIO\n");
	else {
		gpio_val = gpio_get_value(p->pdata->gpio_wakeup);
		ret = sprintf(buf, "WakeUp GPIO: %d\n", gpio_val);
	}

	return ret;
}

static ssize_t dbmdx_wakeup_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct dbmdx_private *p = dev_get_drvdata(dev);

	dbmdx_force_wake(p);

	return size;
}

static DEVICE_ATTR(fwver, S_IRUGO,
		   dbmdx_fw_ver_show, NULL);
static DEVICE_ATTR(paramaddr, S_IWUSR,
		   NULL, dbmdx_param_addr_store);
static DEVICE_ATTR(param, S_IRUGO | S_IWUSR,
		   dbmdx_param_show, dbmdx_param_store);
static DEVICE_ATTR(dump,  S_IRUGO,
		   dbmdx_dump_state, NULL);
static DEVICE_ATTR(io_addr, S_IRUGO | S_IWUSR,
		   dbmdx_io_addr_show, dbmdx_io_addr_store);
static DEVICE_ATTR(io_value, S_IRUGO | S_IWUSR,
		   dbmdx_io_value_show, dbmdx_io_value_store);
static DEVICE_ATTR(power_mode,  S_IRUGO,
		   dbmdx_power_mode_show, NULL);
static DEVICE_ATTR(reboot, S_IWUSR,
		   NULL, dbmdx_reboot_store);
static DEVICE_ATTR(reset, S_IWUSR,
		   NULL, dbmdx_reset_store);
static DEVICE_ATTR(va_dump_regs,  S_IRUGO,
		   dbmdx_va_regs_dump_state, NULL);
static DEVICE_ATTR(va_debug, S_IRUGO | S_IWUSR,
		   dbmdx_va_debug_show, dbmdx_va_debug_store);
static DEVICE_ATTR(va_speed_cfg, S_IRUGO | S_IWUSR,
		   dbmdx_va_speed_cfg_show, dbmdx_va_speed_cfg_store);
static DEVICE_ATTR(va_cfg_values, S_IRUGO | S_IWUSR,
		   dbmdx_va_cfg_values_show, dbmdx_va_cfg_values_store);
static DEVICE_ATTR(va_mic_cfg, S_IRUGO | S_IWUSR,
		   dbmdx_va_mic_cfg_show, dbmdx_va_mic_cfg_store);
static DEVICE_ATTR(va_audioconv, S_IRUGO | S_IWUSR,
		   dbmdx_va_audio_conv_show, dbmdx_va_audio_conv_store);
static DEVICE_ATTR(va_backlog_size, S_IRUGO | S_IWUSR,
		   dbmdx_va_backlog_size_show, dbmdx_va_backlog_size_store);
static DEVICE_ATTR(va_buffsize, S_IRUGO | S_IWUSR,
		   dbmdx_va_buffer_size_show, dbmdx_va_buffer_size_store);
static DEVICE_ATTR(va_buffsmps, S_IRUGO,
		   dbmdx_va_buffsmps_show, NULL);
static DEVICE_ATTR(va_capture_on_detect, S_IRUGO | S_IWUSR,
		   dbmdx_va_capture_on_detect_show,
		   dbmdx_va_capture_on_detect_store);
static DEVICE_ATTR(va_digital_gain, S_IRUGO | S_IWUSR,
		   dbmdx_va_digital_gain_show,
		   dbmdx_va_digital_gain_store);
static DEVICE_ATTR(va_load_amodel, S_IRUGO | S_IWUSR ,
		   NULL, dbmdx_va_acoustic_model_store);
static DEVICE_ATTR(va_max_sample, S_IRUGO,
		   dbmdx_va_max_sample_show, NULL);
static DEVICE_ATTR(va_analog_micgain, S_IRUGO | S_IWUSR,
		   dbmdx_va_analog_micgain_show, dbmdx_va_analog_micgain_store);
static DEVICE_ATTR(va_opmode,  S_IRUGO | S_IWUSR ,
		   dbmdx_va_opmode_show, dbmdx_opr_mode_store);
static DEVICE_ATTR(va_cur_opmode,  S_IRUGO,
		   dbmdx_cur_opmode_show, NULL);
static DEVICE_ATTR(va_mic_mode,  S_IRUGO | S_IWUSR ,
		   dbmdx_va_mic_mode_show, dbmdx_va_mic_mode_store);
static DEVICE_ATTR(va_clockcfg,  S_IRUGO | S_IWUSR ,
		   dbmdx_va_clockcfg_show, dbmdx_va_clockcfg_store);
static DEVICE_ATTR(va_rsize, S_IRUGO | S_IWUSR,
		   dbmdx_va_rsize_show, dbmdx_va_rsize_store);
static DEVICE_ATTR(va_rxsize, S_IRUGO | S_IWUSR,
		   dbmdx_va_rxsize_show, dbmdx_va_rxsize_store);
static DEVICE_ATTR(va_trigger_level, S_IRUGO | S_IWUSR,
		   dbmdx_va_trigger_level_show, dbmdx_va_trigger_level_store);
static DEVICE_ATTR(va_verif_level, S_IRUGO | S_IWUSR,
		   dbmdx_va_verification_level_show,
		   dbmdx_va_verification_level_store);
static DEVICE_ATTR(va_wsize, S_IRUGO | S_IWUSR,
		   dbmdx_va_wsize_show, dbmdx_va_wsize_store);
static DEVICE_ATTR(va_detection_buffer_channels, S_IRUGO | S_IWUSR,
		   dbmdx_va_detection_buffer_channels_show,
		   dbmdx_va_detection_buffer_channels_store);
static DEVICE_ATTR(va_min_samples_chunk_size, S_IRUGO | S_IWUSR,
		   dbmdx_va_min_samples_chunk_size_show,
		   dbmdx_va_min_samples_chunk_size_store);
static DEVICE_ATTR(vqe_ping,  S_IRUGO,
		   dbmdx_vqe_ping_show, NULL);
static DEVICE_ATTR(vqe_use_case, S_IRUGO | S_IWUSR,
		   dbmdx_vqe_use_case_show, dbmdx_vqe_use_case_store);
static DEVICE_ATTR(vqe_d2syscfg,  S_IRUGO | S_IWUSR ,
		   dbmdx_vqe_d2syscfg_show, dbmdx_vqe_d2syscfg_store);
static DEVICE_ATTR(vqe_vc_syscfg,  S_IRUGO | S_IWUSR ,
		   dbmdx_vqe_vc_syscfg_show, dbmdx_vqe_vc_syscfg_store);
static DEVICE_ATTR(vqe_hwbypass, S_IWUSR,
		   NULL, dbmdx_vqe_hwbypass_store);
static DEVICE_ATTR(vqe_spkvollvl, S_IRUGO | S_IWUSR ,
		   dbmdx_vqe_spkvollvl_show, dbmdx_vqe_spkvollvl_store);
static DEVICE_ATTR(wakeup, S_IRUGO | S_IWUSR,
		   dbmdx_wakeup_show, dbmdx_wakeup_store);

static struct attribute *dbmdx_attributes[] = {
	&dev_attr_fwver.attr,
	&dev_attr_paramaddr.attr,
	&dev_attr_param.attr,
	&dev_attr_dump.attr,
	&dev_attr_io_addr.attr,
	&dev_attr_io_value.attr,
	&dev_attr_power_mode.attr,
	&dev_attr_reboot.attr,
	&dev_attr_reset.attr,
	&dev_attr_va_debug.attr,
	&dev_attr_va_dump_regs.attr,
	&dev_attr_va_speed_cfg.attr,
	&dev_attr_va_cfg_values.attr,
	&dev_attr_va_mic_cfg.attr,
	&dev_attr_va_audioconv.attr,
	&dev_attr_va_backlog_size.attr,
	&dev_attr_va_buffsize.attr,
	&dev_attr_va_buffsmps.attr,
	&dev_attr_va_capture_on_detect.attr,
	&dev_attr_va_digital_gain.attr,
	&dev_attr_va_load_amodel.attr,
	&dev_attr_va_max_sample.attr,
	&dev_attr_va_analog_micgain.attr,
	&dev_attr_va_opmode.attr,
	&dev_attr_va_cur_opmode.attr,
	&dev_attr_va_mic_mode.attr,
	&dev_attr_va_clockcfg.attr,
	&dev_attr_va_rsize.attr,
	&dev_attr_va_rxsize.attr,
	&dev_attr_va_trigger_level.attr,
	&dev_attr_va_verif_level.attr,
	&dev_attr_va_wsize.attr,
	&dev_attr_va_detection_buffer_channels.attr,
	&dev_attr_va_min_samples_chunk_size.attr,
	&dev_attr_vqe_ping.attr,
	&dev_attr_vqe_use_case.attr,
	&dev_attr_vqe_vc_syscfg.attr,
	&dev_attr_vqe_d2syscfg.attr,
	&dev_attr_vqe_hwbypass.attr,
	&dev_attr_vqe_spkvollvl.attr,
	&dev_attr_wakeup.attr,
	NULL,
};

static const struct attribute_group dbmdx_attribute_group = {
	.attrs = dbmdx_attributes,
};

static int dbmdx_perform_recovery(struct dbmdx_private *p)
{
	int ret = 0;
	int active_fw = p->active_fw;
	int current_mode;
	int current_audio_channels;
	struct va_flags	saved_va_flags;

	dev_info(p->dev, "%s: active FW - %s\n", __func__,
			dbmdx_fw_type_to_str(active_fw));

	if (active_fw == DBMDX_FW_VA) {
		current_mode = p->va_flags.mode;
		current_audio_channels = p->pdata->va_audio_channels;
		p->va_flags.buffering = 0;
		flush_work(&p->sv_work);

		ret = dbmdx_suspend_pcm_streaming_work(p);
		if (ret < 0)
			dev_err(p->dev,
				"%s: Failed to suspend PCM Streaming Work\n",
				__func__);

		memcpy(&saved_va_flags, &(p->va_flags), sizeof(saved_va_flags));
		p->wakeup_release(p);
		ret = dbmdx_request_and_load_fw(p, 1, 0, 0);

	} else if (active_fw == DBMDX_FW_VQE) {
		p->wakeup_release(p);
		ret = dbmdx_request_and_load_fw(p, 0, 1, 0);
	} else {
		p->wakeup_release(p);
		ret = dbmdx_request_and_load_fw(p, 0, 1, 0);
	}

	if (ret != 0) {
		dev_err(p->dev, "%s: Recovery failure\n", __func__);
		return -EIO;
	}

	p->wakeup_release(p);

	p->lock(p);

	if (active_fw == DBMDX_FW_VA) {

		p->va_flags.pcm_streaming_active =
		saved_va_flags.pcm_streaming_active;

		p->va_flags.pcm_streaming_pushing_zeroes =
		saved_va_flags.pcm_streaming_pushing_zeroes;

		if (saved_va_flags.amodel_len > 0 &&
			saved_va_flags.a_model_loaded) {
			p->unlock(p);
			p->va_flags.auto_detection_disabled = true;
			ret = dbmdx_va_amodel_update(p, p->va_detection_mode);
			p->va_flags.auto_detection_disabled = false;

			if (ret != 0) {
				dev_err(p->dev,
					"%s: Failed to reload amodel\n",
					__func__);
			}

			p->lock(p);
			if (current_mode == DBMDX_DETECTION ||
				current_mode == DBMDX_DETECTION_AND_STREAMING) {

				ret = dbmdx_trigger_detection(p);
				if (ret) {
					dev_err(p->dev,
						"%s: Failed to trigger detection\n",
						__func__);
				}

			} else if (current_mode == DBMDX_STREAMING) {
				ret = dbmdx_set_mode(p, DBMDX_STREAMING);

				if (ret < 0) {
					dev_err(p->dev,
						"%s: Failed to set DBMDX_STREAMING mode\n",
						__func__);
				}
			} else
				dbmdx_set_power_mode(p,
					DBMDX_PM_FALLING_ASLEEP);
		}

	} else if (active_fw == DBMDX_FW_VQE) {

		if (p->vqe_flags.in_call &&
			p->vqe_flags.use_case) {
			ret = dbmdx_vqe_activate_call(p, p->vqe_flags.use_case);
			if (ret) {
				dev_err(p->dev,
					"%s: failed to activate call\n",
					__func__);
				goto out;
			}
		}
	}

	p->device_ready = true;

	ret = dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	if (ret)
		goto out;

out:
	p->unlock(p);
	return ret;
}

/* ------------------------------------------------------------------------
 * interface functions for platform driver
 * ------------------------------------------------------------------------ */

int dbmdx_get_samples(char *buffer, unsigned int samples)
{
	struct dbmdx_private *p = dbmdx_data;
	int avail = kfifo_len(&p->pcm_kfifo);
	int samples_avail = avail / p->bytes_per_sample;
	int ret;

#if 0
	pr_debug("%s Requested %u, Available %d\n",
			__func__, samples, avail);
#endif
	if (p->va_flags.pcm_streaming_pushing_zeroes)
		return -1;

	if (samples_avail < samples)
		return -1;

	ret = kfifo_out(&p->pcm_kfifo,
			buffer,
			samples * p->bytes_per_sample);

	return (ret == samples * p->bytes_per_sample ? 0 : -1);
}
EXPORT_SYMBOL(dbmdx_get_samples);

int dbmdx_codec_lock(void)
{
	if (!atomic_add_unless(&dbmdx_data->audio_owner, 1, 1))
		return -EBUSY;

	return 0;
}
EXPORT_SYMBOL(dbmdx_codec_lock);

int dbmdx_codec_unlock(void)
{
	atomic_dec(&dbmdx_data->audio_owner);
	return 0;
}
EXPORT_SYMBOL(dbmdx_codec_unlock);

#if defined(CONFIG_SND_SOC_DBMDX_SND_CAPTURE)

int dbmdx_start_pcm_streaming(struct snd_pcm_substream *substream)
{
	int ret;
	struct dbmdx_private *p = dbmdx_data;
	int required_mode = DBMDX_STREAMING;

	if (!p)
		return -EAGAIN;

	if (!p->pdata->auto_buffering) {
		dev_err(p->dev, "%s: auto_buffering is disabled\n", __func__);
		return -EIO;
	}

	/* Do not interfere buffering mode , wait till the end
		Just set the flag			*/
	if (p->va_flags.mode == DBMDX_BUFFERING) {
		dev_dbg(p->dev, "%s: Buffering mode\n", __func__);
		p->va_flags.pcm_streaming_active = 1;
		p->active_substream = substream;
		dev_dbg(p->dev,
			"%s: FW in Buffering mode, set the flag and leave\n",
			__func__);
		return 0;
	}

	p->lock(p);

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		ret = -EAGAIN;
		goto out_unlock;
	}

	if (p->active_fw != DBMDX_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		ret = -EAGAIN;
		p->unlock(p);
		goto out;
	}

	dev_dbg(p->dev, "%s:\n", __func__);

	p->va_flags.pcm_streaming_active = 0;

	p->unlock(p);

	/* Do not interfere buffering mode , wait till the end
		Just set the flag			*/
	if (p->va_flags.mode == DBMDX_BUFFERING) {
		p->va_flags.pcm_streaming_active = 1;
		p->active_substream = substream;
		dev_dbg(p->dev,
			"%s: FW in Buffering mode, set the flag and leave\n",
			__func__);
		return 0;
	} else if (p->va_flags.mode == DBMDX_DETECTION)
		required_mode = DBMDX_DETECTION_AND_STREAMING;
	else
		required_mode = DBMDX_STREAMING;

	dev_dbg(p->dev,
		"%s: New required streaming mode is %d\n",
		__func__, required_mode);

	/* flush pending buffering work if any */
	p->va_flags.buffering = 0;
	flush_work(&p->sv_work);
	p->va_flags.pcm_worker_active = 0;
	flush_work(&p->pcm_streaming_work);

	p->lock(p);

	ret = dbmdx_set_pcm_rate(p, p->audio_pcm_rate);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set pcm rate\n", __func__);
		ret = -EINVAL;
		goto out_unlock;
	}

	p->va_flags.pcm_streaming_active = 1;
	p->active_substream = substream;

	ret = dbmdx_set_mode(p, required_mode);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set mode %d\n",
			__func__, required_mode);

		dbmdx_set_pcm_rate(p, p->pdata->va_buffering_pcm_rate);
		p->va_flags.pcm_streaming_active = 0;
		ret = -EINVAL;
		goto out_unlock;
	}

out_unlock:
	p->unlock(p);

	if (ret < 0 && !p->pdata->va_recovery_disabled) {

		int recovery_res;

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #1\n", __func__);
		p->va_flags.pcm_streaming_active = 0;
		p->active_substream = NULL;

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

		p->lock(p);

		ret = dbmdx_set_pcm_rate(p, p->audio_pcm_rate);

		if (ret == 0) {

			p->va_flags.pcm_streaming_active = 1;
			p->active_substream = substream;

			ret = dbmdx_set_mode(p, required_mode);

			if (ret == 0) {
				dev_err(p->dev,
					"%s: PCM Streaming was started after succesfull recovery\n",
					__func__);
				p->unlock(p);
				goto out;
			}

		}

		p->unlock(p);

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #2\n", __func__);

		p->va_flags.pcm_streaming_active = 0;
		p->active_substream = NULL;

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

	}

out:
	return ret;
}
EXPORT_SYMBOL(dbmdx_start_pcm_streaming);

int dbmdx_stop_pcm_streaming(void)
{
	int ret;
	struct dbmdx_private *p = dbmdx_data;
	int required_mode;

	if (!p)
		return -EAGAIN;

	if (!p->pdata->auto_buffering) {
		dev_err(p->dev, "%s: auto_buffering is disabled\n", __func__);
		return -EIO;
	}

	/* Treat special case when buffering is active before lock */
	if (p->va_flags.mode == DBMDX_BUFFERING) {
		dev_dbg(p->dev, "%s: Buffering case\n", __func__);
		p->va_flags.pcm_streaming_active = 0;
		p->va_flags.pcm_worker_active = 0;
		flush_work(&p->pcm_streaming_work);
		p->active_substream = NULL;
		dev_dbg(p->dev,
			"%s: FW in Buffering mode, set the flag and leave\n",
			__func__);
		return 0;
	}

	p->lock(p);

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		ret = -EAGAIN;
		goto out_unlock;
	}

	if (p->active_fw != DBMDX_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		ret = -EAGAIN;
		p->unlock(p);
		return ret;
	}

	dev_dbg(p->dev, "%s:\n", __func__);

	p->va_flags.pcm_streaming_active = 0;

	p->unlock(p);

	/* flush pending work if any */
	p->va_flags.pcm_worker_active = 0;
	flush_work(&p->pcm_streaming_work);
	p->active_substream = NULL;

	/* Do not interfere buffering mode , wait till the end
		Just set the flag			*/
	if (p->va_flags.mode == DBMDX_BUFFERING) {
		p->va_flags.pcm_streaming_active = 0;
		dev_dbg(p->dev,
			"%s: FW in Buffering mode, set the flag and leave\n",
			__func__);
		return 0;
	} else if (p->va_flags.mode == DBMDX_DETECTION_AND_STREAMING)
		required_mode = DBMDX_DETECTION;
	else
		required_mode = DBMDX_IDLE;

	dev_dbg(p->dev,
		"%s: New required mode after streaming is stopped is %d\n",
			__func__, required_mode);

	p->lock(p);

	ret = dbmdx_set_mode(p, required_mode);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to set idle mode\n", __func__);
		dbmdx_set_pcm_rate(p, p->pdata->va_buffering_pcm_rate);
		ret = -EINVAL;
		goto out_unlock;
	}

	ret = dbmdx_set_pcm_rate(p, p->pdata->va_buffering_pcm_rate);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set pcm rate\n", __func__);
		ret = -EINVAL;
		goto out_unlock;
	}

out_unlock:
	p->unlock(p);
	return ret;
}
EXPORT_SYMBOL(dbmdx_stop_pcm_streaming);

#endif
/* ------------------------------------------------------------------------
 * codec driver section
 * ------------------------------------------------------------------------ */

#define DUMMY_REGISTER 0

static int dbmdx_dai_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret = 0;
	struct dbmdx_private *p = snd_soc_codec_get_drvdata(rtd->codec);
	int channels;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		ret = -EAGAIN;
		goto out;
	}

	if (p->active_fw != DBMDX_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		ret = -EAGAIN;
		goto out;
	}

	dev_dbg(p->dev, "%s:\n", __func__);

#if 0
	/* flush pending sv work if any */
	p->va_flags.buffering = 0;
	flush_work(&p->sv_work);

	p->lock(p);

	ret = dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set PM_ACTIVE\n", __func__);
		ret = -EINVAL;
		goto out_unlock;
	}

	/* set chip to idle mode */
	ret = dbmdx_set_mode(p, DBMDX_IDLE);
	if (ret) {
		dev_err(p->dev, "%s: failed to set device to idle mode\n",
			__func__);
		goto out_unlock;
	}
#endif

	channels = params_channels(params);
	p->audio_pcm_channels = channels;

	if (channels == p->pdata->va_audio_channels)
		p->pcm_achannel_op = AUDIO_CHANNEL_OP_COPY;
	else {
		if (channels == 1 && p->pdata->va_audio_channels == 2)
			p->pcm_achannel_op =
				AUDIO_CHANNEL_OP_TRUNCATE_PRIMARY;

		else if (channels == 2 && p->pdata->va_audio_channels == 1)
			p->pcm_achannel_op =
				AUDIO_CHANNEL_OP_DUPLICATE_MONO;
		else {
			dev_err(p->dev,
			 "%s: DAI channels %d not matching hw channels %d\n",
			 __func__, channels, p->pdata->va_audio_channels);
			ret = -EINVAL;
			goto out;
		}
	}

	dev_info(p->dev, "%s: DAI channels %d, Channel operation set to %d\n",
		__func__, channels, p->pcm_achannel_op);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
#if 0
		ret = dbmdx_set_bytes_per_sample(p, DBMDX_AUDIO_MODE_PCM);
#endif
		dev_dbg(p->dev, "%s: set pcm format: SNDRV_PCM_FORMAT_S16_LE\n",
			__func__);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret) {
		dev_err(p->dev, "%s: failed to set pcm format\n",
			__func__);
		goto out_unlock;
	}

	switch (params_rate(params)) {
	case 16000:
	case 32000:
	case 48000:
		p->audio_pcm_rate = params_rate(params);
		dev_dbg(p->dev, "%s: set pcm rate: %u\n",
			__func__, params_rate(params));
		break;
	default:
		ret = -EINVAL;
	}

	if (ret) {
		dev_err(p->dev, "%s: failed to set pcm rate: %u\n",
			__func__, params_rate(params));
		goto out_unlock;
	}

out_unlock:
#if 0
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	p->unlock(p);
#endif
out:
	return ret;
}

static struct snd_soc_dai_ops dbmdx_dai_ops = {
	.hw_params = dbmdx_dai_hw_params,
};

/* DBMDX codec DAI: */
static struct snd_soc_dai_driver dbmdx_dais[] = {
	{
		.name = "DBMDX_codec_dai",
		.capture = {
			.stream_name	= "vs_buffer",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_16000 |
					SNDRV_PCM_RATE_32000 |
					SNDRV_PCM_RATE_48000,
			.formats	= SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &dbmdx_dai_ops,
	},
	{
		.name = "dbmdx_i2s0",
		.id = DBMDX_I2S0,
		.playback = {
			.stream_name	= "I2S0 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S0 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.ops = &dbmdx_i2s_dai_ops,
	},
	{
		.name = "dbmdx_i2s1",
		.id = DBMDX_I2S1,
		.playback = {
			.stream_name	= "I2S1 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S1 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.ops = &dbmdx_i2s_dai_ops,
	},
	{
		.name = "dbmdx_i2s2",
		.id = DBMDX_I2S2,
		.playback = {
			.stream_name	= "I2S2 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S2 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.ops = &dbmdx_i2s_dai_ops,
	},
	{
		.name = "dbmdx_i2s3",
		.id = DBMDX_I2S3,
		.playback = {
			.stream_name	= "I2S3 Playback",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.capture = {
			.stream_name	= "I2S3 Capture",
			.channels_min	= 2,
			.channels_max	= 2,
			.rates		= DBMDX_I2S_RATES,
			.formats	= DBMDX_I2S_FORMATS,
		},
		.ops = &dbmdx_i2s_dai_ops,
	},
};

/* ASoC controls */
static unsigned int dbmdx_dev_read(struct snd_soc_codec *codec,
				   unsigned int reg)
{
	struct dbmdx_private *p = dbmdx_data;
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
	if (p->active_fw != DBMDX_FW_VA)
		goto out_unlock;

	if (reg == DUMMY_REGISTER)
		goto out_unlock;

	/* just return 0 - the user needs to wakeup first */
	if (dbmdx_sleeping(p)) {
		dev_err(p->dev, "%s: device sleeping\n", __func__);
		goto out_unlock;
	}

	if (p->va_flags.mode == DBMDX_DETECTION) {
		dev_dbg(p->dev, "%s: device in detection state\n", __func__);
		goto out_unlock;
	}

	dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);

	ret = dbmdx_send_cmd(p, reg, &val);
	if (ret < 0)
		dev_err(p->dev, "%s: read 0x%x error\n", __func__, reg);

	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);

out_unlock:
	p->unlock(p);
	return (unsigned int)val;
}

static int dbmdx_dev_write(struct snd_soc_codec *codec, unsigned int reg,
			   unsigned int val)
{
	struct dbmdx_private *p = dbmdx_data;
	int ret = -EIO;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}
#if 0
	if (!snd_soc_codec_writable_register(codec, reg)) {
		dev_err(p->dev, "%s: register not writable\n", __func__);
		return -EIO;
	}
#endif
	dev_dbg(p->dev, "%s: ------- VA control ------\n", __func__);
	if (p->active_fw != DBMDX_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	p->lock(p);

	ret = dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);
	if (reg == DUMMY_REGISTER)
		goto out_unlock;

	ret = dbmdx_send_cmd(p, (reg << 16) | (val & 0xffff), NULL);
	if (ret < 0)
		dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
			__func__, val, reg);

out_unlock:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	p->unlock(p);
out:
	return ret;
}

static int dbmdx_va_control_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned short val, reg = mc->reg;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	int ret;
	unsigned int va_reg = DBMDX_VA_CMD_MASK | ((reg & 0xff) << 16);
	struct dbmdx_private *p = dbmdx_data;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	/* VA controls */
	if (p->active_fw != DBMDX_FW_VA) {
		ucontrol->value.integer.value[0] = 0;
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out_unlock;
	}

	ret = dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set PM_ACTIVE\n", __func__);
		ret = -EINVAL;
		goto out_unlock;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VA_CMD_MASK | ((reg & 0xff) << 16),
				  &val);
	if (ret < 0)
		dev_err(p->dev, "%s: read 0x%x error\n", __func__, reg);

	val &= mask;

	if (va_reg == DBMDX_VA_DIGITAL_GAIN)
		val = (unsigned short)((short)val + DIGITAL_GAIN_TLV_SHIFT);

	ucontrol->value.integer.value[0] = val;

out_unlock:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	p->unlock(p);

	return 0;
}

static int dbmdx_va_update_reg(struct dbmdx_private *p,
				unsigned short reg, unsigned short val)
{
	int ret;
	unsigned int va_reg = DBMDX_VA_CMD_MASK | ((reg & 0xff) << 16);

	p->lock(p);

	ret = dbmdx_set_power_mode(p, DBMDX_PM_ACTIVE);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set PM_ACTIVE\n", __func__);
		ret = -EINVAL;
		goto out_unlock;
	}

	if (va_reg == DBMDX_VA_AUDIO_HISTORY) {
		ret = dbmdx_set_backlog_len(p, val);
		if (ret < 0) {
			dev_err(p->dev, "%s: set history error\n", __func__);
			ret = -EINVAL;
			goto out_unlock;
		}
	} else if (va_reg == DBMDX_VA_DIGITAL_GAIN) {
		short sval = (short)val - DIGITAL_GAIN_TLV_SHIFT;
		ret = dbmdx_send_cmd(p, va_reg | (sval & 0xffff), NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
				__func__, val, reg);
			ret = -EINVAL;
			goto out_unlock;
		}
		p->va_cur_digital_gain = (int)sval;
	}  else if (va_reg == DBMDX_VA_ANALOG_MIC_GAIN) {
		ret = dbmdx_send_cmd(p, va_reg | (val & 0xffff), NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
				__func__, val, reg);
			ret = -EINVAL;
			goto out_unlock;
		}
		p->va_cur_analog_gain = (int)val;
	} else {
		ret = dbmdx_send_cmd(p, va_reg | (val & 0xffff), NULL);
		if (ret < 0) {
			dev_err(p->dev, "%s: write 0x%x to 0x%x error\n",
				__func__, val, reg);
			ret = -EINVAL;
			goto out_unlock;
		}
	}

	ret = 0;

out_unlock:
	dbmdx_set_power_mode(p, DBMDX_PM_FALLING_ASLEEP);
	p->unlock(p);
	return ret;
}


static int dbmdx_va_control_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned short val = ucontrol->value.integer.value[0];
	unsigned short reg = mc->reg;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	int ret;
	struct dbmdx_private *p = dbmdx_data;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s: ------- VA control ------\n", __func__);
	if (p->active_fw != DBMDX_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		return -EAGAIN;
	}

	val &= mask;

	ret = dbmdx_va_update_reg(p, reg, val);

	if (ret < 0 && !p->pdata->va_recovery_disabled) {

		int recovery_res;

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #1\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

		ret = dbmdx_va_update_reg(p, reg, val);

		if (ret == 0) {
			dev_err(p->dev,
				"%s: Reg. was updated after succesfull recovery\n",
				__func__);
			goto out;
		}

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #2\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

	}
out:
	return ret;
}

static int dbmdx_vqe_use_case_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	unsigned short val;
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	if (p->active_fw != DBMDX_FW_VQE) {
		ucontrol->value.integer.value[0] = 5;
		dev_dbg(p->dev, "%s: VQE firmware not active\n", __func__);
		goto out_unlock;
	}

	if (dbmdx_sleeping(p)) {
		dev_dbg(p->dev, "%s: device sleeping\n", __func__);
		goto out_unlock;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VQE_GET_USE_CASE_CMD, &val);
	if (ret < 0)
		dev_err(p->dev, "%s: read 0x%x error\n",
			__func__, DBMDX_VQE_GET_USE_CASE_CMD);

	/* TODO: check this */
	ucontrol->value.integer.value[0] = (val == 0xffff ? 0 : val);

out_unlock:
	p->unlock(p);
	return 0;
}

static int dbmdx_vqe_use_case_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	unsigned short val = ucontrol->value.integer.value[0];
	int ret;
	int reg = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = dbmdx_vqe_mode_valid(p, (u32)val);
	if (!ret) {
		dev_err(p->dev, "%s: Invalid VQE mode 0x%x\n",
			__func__, (u32)val);
		return -EINVAL;
	}

	if (p->active_fw != DBMDX_FW_VQE) {
		dev_info(p->dev, "%s: VQE firmware not active, switching\n",
			__func__);
		p->lock(p);
		ret = dbmdx_switch_to_vqe_firmware(p, 0);
		p->unlock(p);
		if (ret != 0) {
			dev_info(p->dev,
				"%s: Error switching to VQE firmware\n",
				__func__);
			return -EIO;
		}

	}

	reg += (DBMDX_VQE_SET_CMD_OFFSET >> 16);

	p->lock(p);

	ret = dbmdx_vqe_set_use_case(p, val);
	if (ret == 0)
		ucontrol->value.integer.value[0] = val;

	p->unlock(p);

	return 0;
}

/* Operation modes */
static int dbmdx_operation_mode_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	unsigned short val;
	int ret = 0;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	p->lock(p);

	if (p->active_fw != DBMDX_FW_VA) {
		ucontrol->value.integer.value[0] = 6;
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out_unlock;
	}

	val = p->va_flags.mode;

	if (dbmdx_sleeping(p))
		goto out_report_mode;

	if (p->va_flags.mode == DBMDX_DETECTION) {
		dev_dbg(p->dev, "%s: device in detection state\n", __func__);
		goto out_report_mode;
	}

	if (p->va_flags.mode == DBMDX_STREAMING ||
		p->va_flags.mode == DBMDX_DETECTION_AND_STREAMING) {
		dev_dbg(p->dev,	"%s: Device in streaming mode\n", __func__);
		val = DBMDX_DETECTION;
		goto out_report_mode;
	}

	ret = dbmdx_send_cmd(p, DBMDX_VA_OPR_MODE, &val);
	if (ret < 0) {
		dev_err(p->dev, "%s: failed to read DBMDX_VA_OPR_MODE\n",
			__func__);
		goto out_unlock;
	}


out_report_mode:
	if (val == DBMDX_SLEEP_PLL_ON)
		ucontrol->value.integer.value[0] = 1;
	else if (val == DBMDX_SLEEP_PLL_OFF)
		ucontrol->value.integer.value[0] = 2;
	else if (val == DBMDX_HIBERNATE)
		ucontrol->value.integer.value[0] = 3;
	else if (val == DBMDX_DETECTION)
		ucontrol->value.integer.value[0] = 4;
	else if (val == DBMDX_BUFFERING)
		ucontrol->value.integer.value[0] = 5;
	else if (val == DBMDX_IDLE)
		ucontrol->value.integer.value[0] = 0;
	else
		dev_err(p->dev, "%s: unknown operation mode: %u\n",
			__func__, val);

out_unlock:
	p->unlock(p);

	return ret;
}

static int dbmdx_operation_mode_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	int ret = 0;
	bool to_suspend_pcm_streaming = true;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_dbg(p->dev, "%s: ------- VA control ------\n", __func__);
	if (p->active_fw != DBMDX_FW_VA) {
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	/* flush pending sv work if any */
	p->va_flags.buffering = 0;
	flush_work(&p->sv_work);

	p->lock(p);

	if (ucontrol->value.integer.value[0] == 0)
		ret = dbmdx_set_mode(p, DBMDX_IDLE);
	else if (ucontrol->value.integer.value[0] == 1)
		ret = dbmdx_set_mode(p, DBMDX_SLEEP_PLL_ON);
	else if (ucontrol->value.integer.value[0] == 2)
		ret = dbmdx_set_mode(p, DBMDX_SLEEP_PLL_OFF);
	else if (ucontrol->value.integer.value[0] == 3)
		ret = dbmdx_set_mode(p, DBMDX_HIBERNATE);
	else if (ucontrol->value.integer.value[0] == 4) {
		/* default detection mode - VT, i.e. PHRASE */
		p->va_detection_mode = DETECTION_MODE_PHRASE;
		to_suspend_pcm_streaming = false;
		ret = dbmdx_trigger_detection(p);
	} else if (ucontrol->value.integer.value[0] == 5) {
		ret = dbmdx_set_mode(p, DBMDX_BUFFERING);
		to_suspend_pcm_streaming = false;
	} else {
		ret = -EINVAL;
		p->unlock(p);
		return ret;
	}

	p->unlock(p);

	if (to_suspend_pcm_streaming) {

		int ret1;

		ret1 = dbmdx_suspend_pcm_streaming_work(p);

		if (ret < 0)
			dev_err(p->dev,
				"%s: Failed to suspend PCM Streaming Work\n",
				__func__);
	}

	if (ret < 0 && !p->pdata->va_recovery_disabled) {

		int recovery_res;

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #1\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

		p->lock(p);

		if (ucontrol->value.integer.value[0] == 0)
			ret = dbmdx_set_mode(p, DBMDX_IDLE);
		else if (ucontrol->value.integer.value[0] == 1)
			ret = dbmdx_set_mode(p, DBMDX_SLEEP_PLL_ON);
		else if (ucontrol->value.integer.value[0] == 2)
			ret = dbmdx_set_mode(p, DBMDX_SLEEP_PLL_OFF);
		else if (ucontrol->value.integer.value[0] == 3)
			ret = dbmdx_set_mode(p, DBMDX_HIBERNATE);
		else if (ucontrol->value.integer.value[0] == 4) {
			/* default detection mode - VT, i.e. PHRASE */
			p->va_detection_mode = DETECTION_MODE_PHRASE;
			ret = dbmdx_trigger_detection(p);
		} else if (ucontrol->value.integer.value[0] == 5)
			ret = dbmdx_set_mode(p, DBMDX_BUFFERING);

		p->unlock(p);

		if (ret == 0) {
			dev_err(p->dev,
				"%s: Op. Mode was updated after succesfull recovery\n",
				__func__);
			goto out;
		}

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #2\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

	}
out:
	return ret;
}

static const char *const dbmdx_vqe_use_case_texts[] = {
	"Idle",
	"HS_NB",
	"HS_WB",
	"HF_NB",
	"HF_WB",
	"Not_active",
};

static const struct soc_enum dbmdx_vqe_use_case_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmdx_vqe_use_case_texts),
			    dbmdx_vqe_use_case_texts);

static const char *const dbmdx_operation_mode_texts[] = {
	"Idle",
	"Sleep_pll_on",
	"Sleep_pll_off",
	"Hibernate",
	"Detection",
	"Buffering",
	"Not_active",
};

static const struct soc_enum dbmdx_operation_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmdx_operation_mode_texts),
			    dbmdx_operation_mode_texts);

static const unsigned int dbmdx_digital_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(1),
	DIGITAL_GAIN_TLV_MIN, DIGITAL_GAIN_TLV_MAX,
	TLV_DB_SCALE_ITEM(-6000, 50, 0),
};

static int dbmdx_amodel_load_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = DETECTION_MODE_MAX + 1;
	return 0;
}

static int dbmdx_amodel_load_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	unsigned short value = ucontrol->value.integer.value[0];
	int ret;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	ret = dbmdx_va_amodel_update(p, value);

	if (ret < 0 && !p->pdata->va_recovery_disabled) {

		int recovery_res;

		if (p->device_ready && (dbmdx_va_alive_with_lock(p) == 0)) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #1\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

		ret = dbmdx_va_amodel_update(p, value);

		if (ret == 0) {
			dev_err(p->dev,
				"%s: Amodel was loaded after succesfull recovery\n",
				__func__);
			goto out;
		}

		if (p->device_ready && (dbmdx_va_alive_with_lock(p) == 0)) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #2\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

	}
out:
	return ret;
}

static int dbmdx_wordid_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	int ret = 0;


	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	/* VA controls */
	if (p->active_fw != DBMDX_FW_VA) {
		ucontrol->value.integer.value[0] = 0;
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	ucontrol->value.integer.value[0] = p->va_last_word_id;
out:
	return ret;
}

static int dbmdx_wordid_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;

	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}

	dev_err(p->dev, "%s: WORDID is not writable register\n", __func__);

	return -EIO;
}

static int dbmdx_microphone_mode_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	int ret = 0;


	if (!p)
		return -EAGAIN;

	if (!p->device_ready) {
		dev_err(p->dev, "%s: device not ready\n", __func__);
		return -EAGAIN;
	}


	/* VA controls */
	if (p->active_fw != DBMDX_FW_VA) {
		ucontrol->value.integer.value[0] = DBMDX_MIC_MODE_DISABLE;
		dev_dbg(p->dev, "%s: VA firmware not active\n", __func__);
		goto out;
	}

	ucontrol->value.integer.value[0] = p->va_active_mic_config;
out:
	return ret;
}

static int dbmdx_microphone_mode_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	int ret = 0;

	dev_info(p->dev, "%s: value:%lu\n", __func__,
			ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case DBMDX_MIC_MODE_DIGITAL_LEFT:
	case DBMDX_MIC_MODE_DIGITAL_RIGHT:
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_LEFT:
	case DBMDX_MIC_MODE_DIGITAL_STEREO_TRIG_ON_RIGHT:
	case DBMDX_MIC_MODE_ANALOG:
	case DBMDX_MIC_MODE_DISABLE:
		ret = dbmdx_reconfigure_microphones(
				p, ucontrol->value.integer.value[0]);
		break;
	default:
		dev_err(p->dev, "%s: unsupported microphone mode %d\n",
			__func__, (int)(ucontrol->value.integer.value[0]));
		ret = -EINVAL;
		break;
	}

	if (ret < 0 && !p->pdata->va_recovery_disabled) {

		int recovery_res;

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #1\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

		ret = dbmdx_reconfigure_microphones(
				p, ucontrol->value.integer.value[0]);

		if (ret == 0) {
			dev_err(p->dev,
				"%s:Mic settings updated after succesfull recovery\n",
				__func__);
			goto out;
		}

		if (dbmdx_va_alive_with_lock(p) == 0) {
			dev_err(p->dev,
				"%s: DBMDX response has been verified\n",
				__func__);
			goto out;
		}

		dev_err(p->dev, "%s: Performing recovery #2\n", __func__);

		recovery_res = dbmdx_perform_recovery(p);

		if (recovery_res) {
			dev_err(p->dev, "%s: recovery failed\n", __func__);
			goto out;
		}

	}
out:

	return ret;
}

static int dbmdx_va_capture_on_detect_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
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

static int dbmdx_va_capture_on_detect_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct dbmdx_private *p = dbmdx_data;
	int ret = 0;

	p->lock(p);
	p->va_capture_on_detect = ucontrol->value.integer.value[0];
	p->unlock(p);

	return ret;
}

static const char * const dbmdx_microphone_mode_texts[] = {
	"DigitalLeft",
	"DigitalRight",
	"DigitalStereoTrigOnLeft",
	"DigitalStereoTrigOnRight",
	"Analog",
	"Disable",
};

static const struct soc_enum dbmdx_microphone_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmdx_microphone_mode_texts),
			    dbmdx_microphone_mode_texts);

static const char * const dbmdx_va_capture_on_detection_texts[] = {
	"OFF",
	"ON",
};

static const struct soc_enum dbmdx_va_capture_on_detect_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dbmdx_va_capture_on_detection_texts),
			    dbmdx_va_capture_on_detection_texts);

static const struct snd_kcontrol_new dbmdx_snd_controls[] = {
	/*
	 * VA mixer controls
	 */
	SOC_ENUM_EXT("Operation mode", dbmdx_operation_mode_enum,
		dbmdx_operation_mode_get, dbmdx_operation_mode_set),
	SOC_SINGLE_EXT_TLV("Digital gain", 0x04, 0, DIGITAL_GAIN_TLV_MAX, 0,
		dbmdx_va_control_get, dbmdx_va_control_put,
		dbmdx_digital_gain_tlv),
	SOC_SINGLE_EXT("Analog gain", 0x16, 0, 0xff, 0,
		dbmdx_va_control_get, dbmdx_va_control_put),
	SOC_SINGLE_EXT("Load acoustic model", 0, 0, DETECTION_MODE_MAX, 0,
		dbmdx_amodel_load_get, dbmdx_amodel_load_set),
	SOC_SINGLE_EXT("Word ID", 0, 0, 0x0fff, 0,
		dbmdx_wordid_get, dbmdx_wordid_put),
	SOC_SINGLE("Trigger Level",
		   VA_MIXER_REG(DBMDX_VA_SENS_TG_THRESHOLD),
		   0, 0xffff, 0),
	SOC_SINGLE("Verification Level",
		   VA_MIXER_REG(DBMDX_VA_SENS_VERIF_THRESHOLD),
		   0, 0xffff, 0),
	SOC_SINGLE_EXT("Backlog size", 0x12, 0, 0x0fff, 0,
		dbmdx_va_control_get, dbmdx_va_control_put),
	SOC_ENUM_EXT("Microphone mode", dbmdx_microphone_mode_enum,
		dbmdx_microphone_mode_get, dbmdx_microphone_mode_set),
	SOC_ENUM_EXT("Capture on detection", dbmdx_va_capture_on_detect_enum,
		dbmdx_va_capture_on_detect_get, dbmdx_va_capture_on_detect_set),

	/*
	 * VQE mixer controls
	 */
	SOC_SINGLE_EXT("Use case",
		       VQE_MIXER_REG(DBMDX_VQE_GET_USE_CASE_CMD),
		       0, 15, 0,
		       dbmdx_vqe_use_case_get,
		       dbmdx_vqe_use_case_put),
};

static int dbmdx_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	dev_dbg(codec->dev, "%s: level %d\n", __func__, (int)level);
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

static int dbmdx_dev_probe(struct snd_soc_codec *codec)
{
	codec->control_data = NULL;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int dbmdx_dev_remove(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);
	return 0;
}

#ifdef CONFIG_PM
static int dbmdx_dev_suspend(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);
	return 0;
}

static int dbmdx_dev_resume(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s\n", __func__);
	return 0;
}
#else
#define dbmdx_dev_suspend NULL
#define dbmdx_dev_resume NULL
#endif


#if 0
static int dbmdx_is_writeable_register(struct snd_soc_codec *codec,
				       unsigned int reg)
{
	int ret = 0;

	/* VA control registers */
	switch (reg) {
	case VA_MIXER_REG(DBMDX_VA_SENS_TG_THRESHOLD):
	case VA_MIXER_REG(DBMDX_VA_SENS_VERIF_THRESHOLD):
		ret = 1;
		break;
	default:
		break;
	}
	return ret;
}
#endif

static struct snd_soc_codec_driver soc_codec_dev_dbmdx = {
	.probe   = dbmdx_dev_probe,
	.remove  = dbmdx_dev_remove,
	.suspend = dbmdx_dev_suspend,
	.resume  = dbmdx_dev_resume,
	.set_bias_level = dbmdx_set_bias_level,
	.read = dbmdx_dev_read,
	.write = dbmdx_dev_write,
	.controls = dbmdx_snd_controls,
	.num_controls = ARRAY_SIZE(dbmdx_snd_controls),
#if 0
	.writable_register = dbmdx_is_writeable_register,
#endif
	.reg_cache_size = 0,
	.reg_word_size = 0,
	.reg_cache_default = NULL,
	.ignore_pmdown_time = true,
};

int dbmdx_remote_add_codec_controls(struct snd_soc_codec *codec)
{
	int rc;

	dev_dbg(codec->dev, "%s start\n", __func__);

	rc = snd_soc_add_codec_controls(codec, dbmdx_snd_controls,
				ARRAY_SIZE(dbmdx_snd_controls));

	if (rc)
		dev_err(codec->dev,
			"%s(): dbmdx_remote_add_codec_controls failed\n",
			__func__);

	return rc;
}
EXPORT_SYMBOL(dbmdx_remote_add_codec_controls);

void dbmdx_remote_register_event_callback(event_cb func)
{
	if (dbmdx_data)
		dbmdx_data->event_callback = func;
	else
		g_event_callback = func;
}
EXPORT_SYMBOL(dbmdx_remote_register_event_callback);

void dbmdx_remote_register_set_i2c_freq_callback(set_i2c_freq_cb func)
{
	if (dbmdx_data)
		dbmdx_data->set_i2c_freq_callback = func;
	else
		g_set_i2c_freq_callback = func;
}
EXPORT_SYMBOL(dbmdx_remote_register_set_i2c_freq_callback);

static void dbmdx_uevent_work(struct work_struct *work)
{
	struct dbmdx_private *p = container_of(
			work, struct dbmdx_private,	uevent_work);
	u16 event_id = 23;
	u16 score = 0;
	int ret;

	p->chip->transport_enable(p, true);

	/* Stop PCM streaming work */
	ret = dbmdx_suspend_pcm_streaming_work(p);
	if (ret < 0)
		dev_err(p->dev, "%s: Failed to suspend PCM Streaming Work\n",
			__func__);

	if (p->va_detection_mode == DETECTION_MODE_VOICE_ENERGY) {
		dev_info(p->dev, "%s: VOICE ENERGY\n", __func__);
		event_id = 0;
	} else if (p->va_detection_mode == DETECTION_MODE_PHRASE)  {
		dev_info(p->dev, "%s: PASSPHRASE\n", __func__);

		p->lock(p);
#if 0
		/* prepare anything if needed (e.g. increase speed) */
		ret = p->chip->prepare_buffering(p);
		if (ret)
			dev_warn(p->dev,
					"%s: failed switch to higher speed\n",
					__func__);
#endif
		ret = dbmdx_send_cmd(
				p, DBMDX_VA_SENS_ALTWORDID, &event_id);

		p->unlock(p);

		if (ret < 0) {
			dev_err(p->dev,
					"%s: failed reading WordID\n",
					__func__);
			return;
		}

		dev_info(p->dev,
			"%s: last WordID: %d, EventID: %d\n",
			__func__, event_id, (event_id & 0xff));

		event_id = (event_id & 0xff);

		if (event_id == 3) {
			/* as per SV Algo implementer's recommendation
			 * --> it 1 */
			dev_dbg(p->dev, "%s: fixing to 1\n", __func__);
			event_id = 1;
		}
	} else if (p->va_detection_mode == DETECTION_MODE_VOICE_COMMAND) {
		dev_info(p->dev, "%s: VOICE_COMMAND\n", __func__);

		p->lock(p);

		ret = dbmdx_send_cmd(
				p, DBMDX_VA_SENS_WORDID, &event_id);

		p->unlock(p);

		if (ret < 0) {
			dev_err(p->dev,
					"%s: failed reading WordID\n",
					__func__);
			return;
		}

		event_id = (event_id & 0xff);

		/*for determining voice command, mask should be up to 0x100 */
		event_id += 0x100;
		dev_info(p->dev, "%s: last WordID:%d\n", __func__, event_id);
	} else if (p->va_detection_mode == DETECTION_MODE_DUAL) {
		dev_info(p->dev, "%s: VOICE_DUAL\n", __func__);

		p->lock(p);

		ret = dbmdx_send_cmd(
				p, DBMDX_VA_SENS_WORDID, &event_id);

		p->unlock(p);

		if (ret < 0) {
			dev_err(p->dev,
					"%s: failed reading WordID\n",
					__func__);
			return;
		}

		dev_info(p->dev,
			"%s: last WordID: %d, EventID: %d\n",
			__func__, event_id, (event_id & 0xff));

		event_id = (event_id & 0xff);
	}

	p->lock(p);

	ret = dbmdx_send_cmd(
			p, DBMDX_VA_SENS_FINAL_SCORE, &score);

	p->unlock(p);

	if (ret < 0) {
		dev_err(p->dev,
				"%s: failed reading Score\n",
					__func__);
		return;
	}

	dev_info(p->dev, "%s: Score:%d\n", __func__, score);

	p->va_last_word_id = event_id;

	if (p->event_callback)
		p->event_callback(event_id);

	if (p->pdata->send_uevent_on_detection) {
		char uevent_buf[100];
		char * const envp[] = { uevent_buf, NULL };

		snprintf(uevent_buf, sizeof(uevent_buf),
			"VOICE_WAKEUP EVENT_ID=%d", event_id);

		dev_info(p->dev, "%s: Sending uevent: %s\n",
			__func__, uevent_buf);

		ret = kobject_uevent_env(&p->dev->kobj, KOBJ_CHANGE,
			(char **)envp);

		if (ret)
			dev_err(p->dev,
					"%s: Sending uevent failed %d\n",
						__func__, ret);
	}
}

static void dbmdx_sv_work(struct work_struct *work)
{
	struct dbmdx_private *p = container_of(
			work, struct dbmdx_private, sv_work);
	int ret;
	int bytes_per_sample = p->bytes_per_sample;
	unsigned int bytes_to_read;
	u16 nr_samples;
	size_t avail_samples;
	unsigned int total = 0;
	int kfifo_space = 0;
	int retries = 0;
	size_t data_offset;

	dev_dbg(p->dev,
		"%s HW Channels %d, Channel operation: %d\n",
		__func__,
		p->pdata->va_audio_channels,
		p->detection_achannel_op);

	if (p->va_detection_mode == DETECTION_MODE_DUAL) {
		dev_dbg(p->dev,
				"%s:dual mode: ->voice_command mode\n",
				__func__);
		return;
	}

	p->chip->transport_enable(p, true);

	p->va_flags.irq_inuse = 0;

	if (!p->va_capture_on_detect) {
		dev_dbg(p->dev, "%s: no capture requested, exit..\n", __func__);
		goto out;
	}

	/* Stop PCM streaming work */
	ret = dbmdx_suspend_pcm_streaming_work(p);
	if (ret < 0)
		dev_err(p->dev, "%s: Failed to suspend PCM Streaming Work\n",
			__func__);

	p->lock(p);

	/* flush fifo */
	kfifo_reset(&p->detection_samples_kfifo);

	/* prepare anything if needed (e.g. increase speed) */
	ret = p->chip->prepare_buffering(p);
	if (ret)
		goto out_fail_unlock;
#if 1
	ret = dbmdx_set_pcm_streaming_mode(p, 0);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set pcm streaming mode\n",
			__func__);
		goto out_fail_unlock;
	}

	/* Reset position to backlog start */
	ret = dbmdx_send_cmd(p,
		DBMDX_VA_AUDIO_HISTORY |
		(p->va_backlog_length | 0x1000),
		NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed start pcm streaming\n", __func__);
		goto out_fail_unlock;
	}
#endif
	p->unlock(p);

	p->va_flags.mode = DBMDX_BUFFERING;

	do {
		p->lock(p);
		bytes_to_read = 0;
		/* read number of samples available in audio buffer */
		if (dbmdx_send_cmd(p,
				   DBMDX_VA_NUM_OF_SMP_IN_BUF,
				   &nr_samples) < 0) {
			dev_err(p->dev,
				"%s: failed to read DBMDX_VA_NUM_OF_SMP_IN_BUF\n",
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

			kfifo_space = kfifo_avail(&p->detection_samples_kfifo);

			if (p->detection_achannel_op ==
				AUDIO_CHANNEL_OP_DUPLICATE_MONO)
				kfifo_space = kfifo_space / 2;

			if (bytes_to_read > kfifo_space)
				bytes_to_read = kfifo_space;

			/* recalculate number of samples */
			nr_samples = bytes_to_read / (8 * bytes_per_sample);

			if (!nr_samples) {
				/* not enough samples, wait some time */
				usleep_range(DBMDX_USLEEP_NO_SAMPLES,
					DBMDX_USLEEP_NO_SAMPLES + 1000);
				retries++;
				if (retries > MAX_RETRIES_TO_WRITE_TOBUF)
					break;
				continue;
			}
			/* get audio samples */
			p->lock(p);
			ret = p->chip->read_audio_data(p,
				p->read_audio_buf, nr_samples,
				false, &avail_samples, &data_offset);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to read block of audio data: %d\n",
					__func__, ret);
				p->unlock(p);
				break;
			} else if (ret > 0) {
				total += bytes_to_read;

				ret = dbmdx_add_audio_samples_to_kfifo(p,
					&p->detection_samples_kfifo,
					p->read_audio_buf + data_offset,
					bytes_to_read,
					p->detection_achannel_op);
			}

			retries = 0;

			p->unlock(p);
		} else {
			usleep_range(DBMDX_USLEEP_NO_SAMPLES,
				DBMDX_USLEEP_NO_SAMPLES + 1000);
			retries++;
			if (retries > MAX_RETRIES_TO_WRITE_TOBUF)
				break;
		}

	} while (p->va_flags.buffering);

	dev_info(p->dev, "%s: audio buffer read, total of %u bytes\n",
		__func__, total);

out:
	p->lock(p);
	p->va_flags.buffering = 0;
	p->va_flags.mode = DBMDX_IDLE;
	ret = dbmdx_set_mode(p, DBMDX_IDLE);
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

	if (p->va_flags.pcm_streaming_active) {

		ret = dbmdx_set_mode(p, DBMDX_STREAMING);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to set DBMDX_STREAMING mode\n",
					__func__);
			goto out_fail_unlock;
		}

	}

out_fail_unlock:
	p->unlock(p);
	dev_dbg(p->dev, "%s done\n", __func__);
}

#if defined(CONFIG_SND_SOC_DBMDX_SND_CAPTURE)

static void dbmdx_pcm_streaming_work_mod_0(struct work_struct *work)
{
	struct dbmdx_private *p = container_of(
			work, struct dbmdx_private, pcm_streaming_work);
	int ret;
	int bytes_per_sample = p->bytes_per_sample;
	unsigned int bytes_to_read;
	u16 nr_samples;
	size_t avail_samples;
	unsigned int total = 0;
	int kfifo_space = 0;
	int retries = 0;
	struct snd_pcm_substream *substream = p->active_substream;
	struct snd_pcm_runtime *runtime;
	unsigned long frame_in_bytes;
	u32 samples_chunk_size;
	bool direct_copy_enabled = false;
	u8 *local_samples_buf;
	u8 *read_samples_buf;
	unsigned int stream_buf_size;
	u32 pos;
	u32 missing_samples;
	u32 sleep_time_ms;
	size_t data_offset;

	if (substream == NULL) {
		dev_err(p->dev,	"%s Error No Active Substream\n", __func__);
		return;
	}

	dev_dbg(p->dev,
		"%s PCM Channels %d, HW Channels %d, Channel operation: %d\n",
		__func__,
		p->audio_pcm_channels,
		p->pdata->va_audio_channels,
		p->pcm_achannel_op);

	ret = dbmdx_set_pcm_timer_mode(substream, false);

	if (ret < 0)
		dev_err(p->dev,	"%s Failed to stop timer mode\n", __func__);

	p->va_flags.pcm_streaming_pushing_zeroes = false;

	runtime = substream->runtime;
	frame_in_bytes = frames_to_bytes(runtime, runtime->period_size);
	stream_buf_size = snd_pcm_lib_buffer_bytes(substream);

	samples_chunk_size =
		(u32)(frame_in_bytes * p->pdata->va_audio_channels) /
		(8 * bytes_per_sample * runtime->channels);

	if (((u32)(frame_in_bytes * p->pdata->va_audio_channels) %
		(8 * bytes_per_sample * runtime->channels)))
		samples_chunk_size++;
	else if (p->pcm_achannel_op == AUDIO_CHANNEL_OP_COPY)
		direct_copy_enabled = true;

	bytes_to_read = samples_chunk_size * 8 * bytes_per_sample;

	dev_dbg(p->dev,
		"%s Frame Size: %d, Dir. Copy Mode: %d, Samples TX Thr.: %d\n",
		__func__,
		(int)frame_in_bytes,
		(int)direct_copy_enabled,
		(int)samples_chunk_size);

	local_samples_buf = kmalloc(bytes_to_read + 8, GFP_KERNEL);
	if (!local_samples_buf) {
		dev_err(p->dev,	"%s Memory allocation error\n",	__func__);
		return;
	}

	read_samples_buf = local_samples_buf;

	p->lock(p);

	/* flush fifo */
	kfifo_reset(&p->pcm_kfifo);

	/* prepare anything if needed (e.g. increase speed) */
	ret = p->chip->prepare_buffering(p);
	if (ret)
		goto out_fail_unlock;

	/* Delay streaming start to stabilize the PLL and microphones */
	msleep(DBMDX_MSLEEP_PCM_STREAMING_WORK);

	ret = dbmdx_set_pcm_streaming_mode(p, 0);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set pcm streaming mode\n",
			__func__);
		goto out_fail_unlock;
	}

	/* Start PCM streaming, FW should be in detection mode */
	ret = dbmdx_send_cmd(p, DBMDX_VA_AUDIO_HISTORY | 0x1000, NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed start pcm streaming\n", __func__);
		goto out_fail_unlock;
	}

	p->unlock(p);

	do {
		p->lock(p);

		if (!(p->va_flags.pcm_worker_active)) {
			p->unlock(p);
			break;
		}

		/* read number of samples available in audio buffer */
		if (dbmdx_send_cmd(p,
				   DBMDX_VA_NUM_OF_SMP_IN_BUF,
				   &nr_samples) < 0) {
			dev_err(p->dev,
				"%s: failed to read DBMDX_VA_NUM_OF_SMP_IN_BUF\n",
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
		if (nr_samples >= samples_chunk_size) {

			if (nr_samples >= 2*samples_chunk_size)
				sleep_time_ms = 0;
			else {
				missing_samples = 2*samples_chunk_size -
						nr_samples;
				sleep_time_ms =
					(u32)(missing_samples * 8 * 1000) /
					p->current_pcm_rate;
			}

			nr_samples = samples_chunk_size;

			if (!direct_copy_enabled) {
				kfifo_space = kfifo_avail(&p->pcm_kfifo);

				if (p->pcm_achannel_op ==
					AUDIO_CHANNEL_OP_DUPLICATE_MONO)
					kfifo_space = kfifo_space / 2;

					if (bytes_to_read > kfifo_space)
						nr_samples = 0;
			}

			if (!nr_samples) {
				/* not enough samples, wait some time */
				usleep_range(DBMDX_USLEEP_NO_SAMPLES,
					DBMDX_USLEEP_NO_SAMPLES + 1000);
				retries++;
				if (retries > MAX_RETRIES_TO_WRITE_TOBUF)
					break;
				continue;
			}
			/* get audio samples */
			p->lock(p);

			ret = p->chip->read_audio_data(p,
				local_samples_buf, nr_samples,
				false, &avail_samples, &data_offset);

			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to read block of audio data: %d\n",
					__func__, ret);
				p->unlock(p);
				break;
			} else if (ret > 0) {
				total += bytes_to_read;

				pos = stream_get_position(substream);
				read_samples_buf = runtime->dma_area + pos;

				if (direct_copy_enabled)
					memcpy(read_samples_buf,
						local_samples_buf + data_offset,
						bytes_to_read);
				else {

					ret =
					dbmdx_add_audio_samples_to_kfifo(p,
						&p->pcm_kfifo,
						local_samples_buf + data_offset,
						bytes_to_read,
						p->pcm_achannel_op);

					ret =
					dbmdx_get_samples(read_samples_buf,
						runtime->channels *
						runtime->period_size);
					if (ret) {
						memset(read_samples_buf,
							0,
							frame_in_bytes);
						pr_debug("%s Inserting %d bytes of silence\n",
						__func__, (int)bytes_to_read);
					}
				}

				pos += frame_in_bytes;
				if (pos >= stream_buf_size)
					pos = 0;

				stream_set_position(substream, pos);

				snd_pcm_period_elapsed(substream);
			}

			retries = 0;

			p->unlock(p);

			if (sleep_time_ms > 0)
				msleep(sleep_time_ms);

		} else {
			u32 missing_samples = samples_chunk_size - nr_samples;
			u32 sleep_time_ms = (u32)(missing_samples * 8 * 1100) /
					p->current_pcm_rate;

			msleep(sleep_time_ms);

			retries++;
			if (retries > MAX_RETRIES_TO_WRITE_TOBUF)
				break;
		}

	} while (p->va_flags.pcm_worker_active);

	dev_info(p->dev, "%s: audio buffer read, total of %u bytes\n",
		__func__, total);

out:
	p->lock(p);
	p->va_flags.pcm_worker_active = 0;
	/* finish audio buffering (e.g. reduce speed) */
	ret = p->chip->finish_buffering(p);
	if (ret) {
		dev_err(p->dev, "%s: failed to finish buffering\n", __func__);
		goto out_fail_unlock;
	}
out_fail_unlock:
	kfree(local_samples_buf);

	p->unlock(p);
	dev_dbg(p->dev, "%s done\n", __func__);
}


#define DBMDX_EXTENDED_STREAMIN_DBG_INFO	0

static void dbmdx_pcm_streaming_work_mod_1(struct work_struct *work)
{
	struct dbmdx_private *p = container_of(
			work, struct dbmdx_private, pcm_streaming_work);
	int ret;
	int bytes_per_sample = p->bytes_per_sample;
	unsigned int bytes_to_read;
	size_t avail_samples;
	unsigned int total = 0;
	int bytes_in_kfifo;
	int retries = 0;
	struct snd_pcm_substream *substream = p->active_substream;
	struct snd_pcm_runtime *runtime;
	unsigned long frame_in_bytes;
	u32 samples_chunk_size;
	bool direct_copy_enabled = false;
	u8 *local_samples_buf;
	u8 *read_samples_buf;
	unsigned int stream_buf_size;
	u32 pos;
	u32 missing_samples;
	u32 sleep_time_ms;
	size_t data_offset;

	if (substream == NULL) {
		dev_err(p->dev,	"%s Error No Active Substream\n", __func__);
		return;
	}

	dev_dbg(p->dev,
		"%s PCM Channels %d, HW Channels %d, Channel operation: %d\n",
		__func__,
		p->audio_pcm_channels,
		p->pdata->va_audio_channels,
		p->pcm_achannel_op);

	ret = dbmdx_set_pcm_timer_mode(substream, false);

	if (ret < 0)
		dev_err(p->dev,	"%s Failed to stop timer mode\n", __func__);

	p->va_flags.pcm_streaming_pushing_zeroes = false;

	runtime = substream->runtime;
	frame_in_bytes = frames_to_bytes(runtime, runtime->period_size);
	stream_buf_size = snd_pcm_lib_buffer_bytes(substream);

	samples_chunk_size =
		(u32)(frame_in_bytes * p->pdata->va_audio_channels) /
		(8 * bytes_per_sample * runtime->channels);

	if (((u32)(frame_in_bytes * p->pdata->va_audio_channels) %
		(8 * bytes_per_sample * runtime->channels)))
		samples_chunk_size++;
	else if (p->pcm_achannel_op == AUDIO_CHANNEL_OP_COPY)
		direct_copy_enabled = true;

	bytes_to_read = samples_chunk_size * 8 * bytes_per_sample;

	if (p->pdata->va_audio_channels > 1 ||  runtime->channels >  1)
		p->va_flags.padded_cmds_disabled = true;

	dev_dbg(p->dev,
		"%s Frame Size: %d, Dir. Copy Mode: %d, Samples TX Thr.: %d\n",
		__func__,
		(int)frame_in_bytes,
		(int)direct_copy_enabled,
		(int)samples_chunk_size);

	/* Ensure that there enough space for metadata , add 8 bytes */
	local_samples_buf = kmalloc(bytes_to_read + 8, GFP_KERNEL);
	if (!local_samples_buf) {
		dev_err(p->dev,	"%s Memory allocation error\n",	__func__);
		return;
	}

	read_samples_buf = local_samples_buf;

	p->lock(p);

	/* flush fifo */
	kfifo_reset(&p->pcm_kfifo);

	/* prepare anything if needed (e.g. increase speed) */
	ret = p->chip->prepare_buffering(p);
	if (ret)
		goto out_fail_unlock;

	/* Delay streaming start to stabilize the PLL and microphones */
	msleep(DBMDX_MSLEEP_PCM_STREAMING_WORK);

	ret = dbmdx_set_pcm_streaming_mode(p, 1);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed to set pcm streaming mode\n",
			__func__);
		goto out_fail_unlock;
	}

	/* Start PCM streaming, FW should be in detection mode */
	ret = dbmdx_send_cmd(p, DBMDX_VA_AUDIO_HISTORY | 0x1000, NULL);

	if (ret < 0) {
		dev_err(p->dev, "%s: failed start pcm streaming\n", __func__);
		goto out_fail_unlock;
	}

	p->unlock(p);

	/* Ensure that we will sleep till the first chunk is available */
	avail_samples = 0;

	do {
		bytes_in_kfifo = kfifo_len(&p->pcm_kfifo);

		if (bytes_in_kfifo >= frame_in_bytes)	{

#if DBMDX_EXTENDED_STREAMIN_DBG_INFO
			dev_dbg(p->dev,
				"%s Bytes in KFIFO (%d) >= Frame Size(%d)\n",
				__func__,
				(int)bytes_in_kfifo,
				(int)frame_in_bytes);
#endif
			pos = stream_get_position(substream);

			read_samples_buf = runtime->dma_area + pos;

			ret = dbmdx_get_samples(read_samples_buf,
						runtime->channels *
						runtime->period_size);

			pos += frame_in_bytes;
			if (pos >= stream_buf_size)
				pos = 0;

			stream_set_position(substream, pos);

			snd_pcm_period_elapsed(substream);

			continue;
		}

		samples_chunk_size =
			(u32)((frame_in_bytes - bytes_in_kfifo) *
				p->pdata->va_audio_channels) /
			(8 * bytes_per_sample * runtime->channels);

		if (((u32)((frame_in_bytes - bytes_in_kfifo) *
			p->pdata->va_audio_channels) %
			(8 * bytes_per_sample * runtime->channels)))
			samples_chunk_size++;

		bytes_to_read = samples_chunk_size * 8 * bytes_per_sample;

		if (avail_samples >= samples_chunk_size)
				sleep_time_ms = 0;
		else {
			missing_samples = samples_chunk_size -
						avail_samples;
			sleep_time_ms =
				(u32)(missing_samples * 8 * 1000) /
				p->current_pcm_rate;
		}

#if DBMDX_EXTENDED_STREAMIN_DBG_INFO
		dev_dbg(p->dev,
			"%s Frame Size(%d), Kfifo Bytes (%d), Samples Chunk (%d), Bytes to Read (%d), Avail. Samples (%d), Sleep Time (%d)ms\n",
			__func__,
			(int)frame_in_bytes,
			(int)bytes_in_kfifo,
			(int)samples_chunk_size,
			(int)bytes_to_read,
			(int)avail_samples,
			(int)sleep_time_ms);
#endif

		if (sleep_time_ms > 0)
			msleep(sleep_time_ms);

		/* get audio samples */
		p->lock(p);

		if (!(p->va_flags.pcm_worker_active)) {
			p->unlock(p);
			break;
		}

		ret = p->chip->read_audio_data(p,
				local_samples_buf, samples_chunk_size,
				true, &avail_samples, &data_offset);

		p->unlock(p);

		if (ret < 0) {
			dev_err(p->dev,
				"%s: failed to read block of audio data: %d\n",
				__func__, ret);
			break;
		}

		if (avail_samples == 0xffff) {
			dev_info(p->dev,
				"%s: buffering mode ended - fw in idle mode",
				__func__);
			break;
		} else if (avail_samples >= samples_chunk_size) {

			/* If all requested samples were read */

#if DBMDX_EXTENDED_STREAMIN_DBG_INFO
			dev_dbg(p->dev,
				"%s Chunk was read (big enough): Avail. Samples (%d),Samples Chunk (%d), Kfifo Bytes (%d)\n",
				__func__,
				(int)avail_samples,
				(int)samples_chunk_size,
				(int)bytes_in_kfifo);
#endif
			total += bytes_to_read;

			pos = stream_get_position(substream);
			read_samples_buf = runtime->dma_area + pos;

			if (direct_copy_enabled && !bytes_in_kfifo)
				memcpy(read_samples_buf,
					local_samples_buf + data_offset,
					bytes_to_read);
			else {
				ret = dbmdx_add_audio_samples_to_kfifo(p,
						&p->pcm_kfifo,
						local_samples_buf + data_offset,
						bytes_to_read,
						p->pcm_achannel_op);

				ret = dbmdx_get_samples(read_samples_buf,
						runtime->channels *
						runtime->period_size);
				if (ret) {
					memset(read_samples_buf,
						0,
						frame_in_bytes);
					pr_debug("%s Inserting %d bytes of silence\n",
					__func__, (int)bytes_to_read);
				}
			}

			pos += frame_in_bytes;
			if (pos >= stream_buf_size)
				pos = 0;

			stream_set_position(substream, pos);

			snd_pcm_period_elapsed(substream);

			avail_samples -= samples_chunk_size;

			retries = 0;

		} else if (avail_samples > 0) {
			u32 bytes_read;
#if DBMDX_EXTENDED_STREAMIN_DBG_INFO
			dev_dbg(p->dev,
				"%s Chunk was read (not big enough): Avail. Samples (%d),Samples Chunk (%d), Kfifo Bytes (%d)\n",
				__func__,
				(int)avail_samples,
				(int)samples_chunk_size,
				(int)bytes_in_kfifo);
#endif

			bytes_read =
				(u32)(avail_samples * 8 * bytes_per_sample);

			total += bytes_read;

			ret = dbmdx_add_audio_samples_to_kfifo(p,
						&p->pcm_kfifo,
						local_samples_buf + data_offset,
						bytes_read,
						p->pcm_achannel_op);
			avail_samples = 0;

			retries = 0;

		} else {

#if DBMDX_EXTENDED_STREAMIN_DBG_INFO
		dev_dbg(p->dev,
			"%s Chunk was read (no samples): Avail. Samples (%d),Samples Chunk (%d), Kfifo Bytes (%d), Retries (%d)\n",
			__func__,
			(int)avail_samples,
			(int)samples_chunk_size,
			(int)bytes_in_kfifo,
			(int)retries+1);
#endif

			retries++;
			if (retries > MAX_RETRIES_TO_WRITE_TOBUF)
				break;
		}

	} while (p->va_flags.pcm_worker_active);

	dev_info(p->dev, "%s: audio buffer read, total of %u bytes\n",
		__func__, total);

	p->lock(p);
	p->va_flags.pcm_worker_active = 0;
	/* finish audio buffering (e.g. reduce speed) */
	ret = p->chip->finish_buffering(p);
	if (ret) {
		dev_err(p->dev, "%s: failed to finish buffering\n", __func__);
		goto out_fail_unlock;
	}
out_fail_unlock:
	kfree(local_samples_buf);
	p->va_flags.padded_cmds_disabled = false;
	p->unlock(p);
	dev_dbg(p->dev, "%s done\n", __func__);
}
#endif

static irqreturn_t dbmdx_sv_interrupt_hard(int irq, void *dev)
{
	struct dbmdx_private *p = (struct dbmdx_private *)dev;

	if (p && (p->device_ready) && (p->va_flags.irq_inuse))

		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}


static irqreturn_t dbmdx_sv_interrupt_soft(int irq, void *dev)
{
	struct dbmdx_private *p = (struct dbmdx_private *)dev;

	dev_dbg(p->dev, "%s\n", __func__);

	if ((p->device_ready) && (p->va_flags.irq_inuse)) {
		wake_lock_timeout(&p->ps_nosuspend_wl, 5 * HZ);
		schedule_work(&p->uevent_work);

		p->va_flags.buffering = 1;
		schedule_work(&p->sv_work);

		dev_info(p->dev, "%s - SV EVENT\n", __func__);
	}

	return IRQ_HANDLED;
}

static int dbmdx_fw_load_thread(void *data)
{
	struct dbmdx_private *p = (struct dbmdx_private *)data;

	if (p->pdata->feature_va && p->pdata->feature_vqe &&
	    p->pdata->feature_fw_overlay)
		return dbmdx_request_and_load_fw(p, 1, 0, 1);
	else if (p->pdata->feature_va && p->pdata->feature_vqe)
		return dbmdx_request_and_load_fw(p, 1, 1, 0);
	else if (p->pdata->feature_vqe)
		return dbmdx_request_and_load_fw(p, 0, 1, 0);
	else if (p->pdata->feature_va)
		return dbmdx_request_and_load_fw(p, 1, 0, 0);
	return -EINVAL;
}


static int dbmdx_common_probe(struct dbmdx_private *p)
{
#ifdef CONFIG_OF
	struct device_node *np = p->dev->of_node;
#endif
	struct task_struct *boot_thread;
	int ret = 0;
	int fclk;

	dbmdx_data = p;
	dev_set_drvdata(p->dev, p);

	/* enable constant clock */
	p->clk_enable(p, DBMDX_CLK_CONSTANT);

	/* enable regulator if defined */
	if (p->vregulator) {
		ret = regulator_enable(p->vregulator);
		if (ret != 0) {
			dev_err(p->dev, "%s: Failed to enable regulator: %d\n",
				__func__, ret);
			goto err;
		}
	}

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

	ret = gpio_request(p->pdata->gpio_reset, "DBMDX reset");
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

	ret = gpio_request(p->pdata->gpio_sv, "DBMDX sv");
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
		ret = gpio_request(p->pdata->gpio_d2strap1, "DBMDX STRAP 1");
		if (ret < 0) {
			dev_err(p->dev, "%s: error requesting d2 strap1 gpio\n",
				__func__);
			goto err_gpio_free2;
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
		ret = gpio_request(p->pdata->gpio_wakeup, "DBMDX wakeup");
		if (ret < 0) {
			dev_err(p->dev, "%s: error requesting wakeup gpio\n",
				__func__);
			goto err_gpio_free3;
		}
		/* keep the wakeup pin high */
		gpio_direction_output(p->pdata->gpio_wakeup, 1);
	}

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "pcm_streaming_mode",
		&p->pdata->pcm_streaming_mode);
	if ((ret && ret != -EINVAL) ||
			(p->pdata->pcm_streaming_mode != 0 &&
				p->pdata->pcm_streaming_mode != 1)) {
		dev_err(p->dev, "%s: invalid 'pcm_streaming_mode'\n", __func__);
		goto err_gpio_free4;
	}
#endif

#if defined(CONFIG_SND_SOC_DBMDX_SND_CAPTURE)
	if (p->pdata->pcm_streaming_mode == 0)
		INIT_WORK(&p->pcm_streaming_work,
			dbmdx_pcm_streaming_work_mod_0);
	else
		INIT_WORK(&p->pcm_streaming_work,
			dbmdx_pcm_streaming_work_mod_1);

#endif

	INIT_WORK(&p->sv_work, dbmdx_sv_work);
	INIT_WORK(&p->uevent_work, dbmdx_uevent_work);
	mutex_init(&p->p_lock);
	wake_lock_init(&p->ps_nosuspend_wl, WAKE_LOCK_SUSPEND,
		"dbmdx_nosuspend_wakelock");

	ret = kfifo_alloc(&p->pcm_kfifo,
		MAX_KFIFO_BUFFER_SIZE_MONO,
		GFP_KERNEL);

	if (ret)  {
		dev_err(p->dev, "%s: no kfifo memory\n", __func__);
		goto err_gpio_free4;
	}

	p->detection_samples_kfifo_buf_size = MAX_KFIFO_BUFFER_SIZE_STEREO;

	p->detection_samples_kfifo_buf =
	kmalloc(MAX_KFIFO_BUFFER_SIZE_STEREO, GFP_KERNEL);

	if (!p->detection_samples_kfifo_buf) {
		dev_err(p->dev, "%s: no kfifo memory\n", __func__);
		goto err_kfifo_pcm_free;
	}

	kfifo_init(&p->detection_samples_kfifo,
		p->detection_samples_kfifo_buf, MAX_KFIFO_BUFFER_SIZE_STEREO);

	fclk = p->clk_get_rate(p, DBMDX_CLK_MASTER);

	p->master_pll_rate =  (unsigned int)((fclk / 6) * 17);

	switch (fclk) {
	case 32768:
		p->clk_type = DBMD2_XTAL_FREQ_32K_IMG7;
		p->master_pll_rate = 60000000;
		break;
	case 9600000:
		p->clk_type = DBMD2_XTAL_FREQ_9M_IMG4;
		break;
	case 24000000:
	default:
		p->clk_type = DBMD2_XTAL_FREQ_24M_IMG1;
	}

	/* Switch ON capture backlog by default */
	p->va_capture_on_detect = true;

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "use_gpio_for_wakeup",
		&p->pdata->use_gpio_for_wakeup);
	if ((ret && ret != -EINVAL)) {
		dev_err(p->dev, "%s: invalid 'use_gpio_for_wakeup'\n",
			__func__);
		goto err_kfifo_free;
	} else if (ret ==  -EINVAL) {
		dev_info(p->dev,
			"%s: no use_gpio_for_wakeup definition in device-tree\n",
			__func__);
		p->pdata->use_gpio_for_wakeup = 1;
	} else {

		if (p->pdata->use_gpio_for_wakeup > 1)
			p->pdata->use_gpio_for_wakeup = 1;

		dev_info(p->dev,
			"%s: using use_gpio_for_wakeup of %d from device-tree\n",
			 __func__, p->pdata->use_gpio_for_wakeup);

	}

	ret = of_property_read_u32(np, "send_wakeup_seq",
		&p->pdata->send_wakeup_seq);
	if ((ret && ret != -EINVAL)) {
		dev_err(p->dev, "%s: invalid 'send_wakeup_seq'\n",
			__func__);
		goto err_kfifo_free;
	} else if (ret ==  -EINVAL) {
		dev_info(p->dev,
			"%s: no send_wakeup_seq definition in device-tree\n",
			__func__);
		p->pdata->send_wakeup_seq = 0;
	} else {

		if (p->pdata->send_wakeup_seq > 1)
			p->pdata->send_wakeup_seq = 1;

		dev_info(p->dev,
			"%s: using send_wakeup_seq of %d from device-tree\n",
			 __func__, p->pdata->send_wakeup_seq);

	}

	ret = of_property_read_u32(np, "wakeup_set_value",
		&p->pdata->wakeup_set_value);
	if ((ret && ret != -EINVAL)) {
		dev_err(p->dev, "%s: invalid 'wakeup_set_value'\n",
			__func__);
		goto err_kfifo_free;
	} else if (ret ==  -EINVAL) {
		dev_info(p->dev,
			"%s: no wakeup_set_value definition in device-tree\n",
			__func__);
		p->pdata->wakeup_set_value = 1;
	} else {

		if (p->pdata->wakeup_set_value > 1)
			p->pdata->wakeup_set_value = 1;

		dev_info(p->dev,
			"%s: using wakeup_set_value of %d from device-tree\n",
			 __func__, p->pdata->wakeup_set_value);

	}

	ret = of_property_read_u32(np, "firmware_id",
		&p->pdata->firmware_id);
	if (ret && ret != -EINVAL) {
		dev_err(p->dev, "%s: invalid 'firmware_id'\n", __func__);
		goto err_kfifo_free;
	} else if (ret ==  -EINVAL) {
		dev_info(p->dev,
			"%s: no firmware_id definition in device-tree. assuming d2\n",
			__func__);
		p->pdata->firmware_id = DBMDX_FIRMWARE_ID_DBMD2;
	} else {
		dev_info(p->dev,
			"%s: using firmware_id of 0x%8x from device-tree\n",
			 __func__, p->pdata->firmware_id);
	}

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

	ret = of_property_read_u32(np, "send_uevent_on_detection",
		&p->pdata->send_uevent_on_detection);
	if ((ret && ret != -EINVAL) ||
			(p->pdata->send_uevent_on_detection != 0 &&
				p->pdata->send_uevent_on_detection != 1)) {
		dev_err(p->dev, "%s: invalid 'send_uevent_on_detection'\n",
			__func__);
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

	ret = of_property_read_u32(np, "detection_buffer_channels",
		&p->pdata->detection_buffer_channels);
	if ((ret && ret != -EINVAL) ||
			(p->pdata->detection_buffer_channels > 2)) {
		dev_err(p->dev, "%s: detection_buffer_channels'\n",
			__func__);
		goto err_kfifo_free;
	}

	ret = of_property_read_u32(np, "min_samples_chunk_size",
		&p->pdata->min_samples_chunk_size);
	if ((ret && ret != -EINVAL)) {
		dev_err(p->dev, "%s: min_samples_chunk_size'\n",
			__func__);
		goto err_kfifo_free;
	}

	dev_info(p->dev, "%s: using min_samples_chunk_size of %d\n",
		 __func__, p->pdata->min_samples_chunk_size);

	ret = of_property_read_u32(np, "va_buffering_pcm_rate",
		&p->pdata->va_buffering_pcm_rate);
	if (ret && ret != -EINVAL) {
		dev_err(p->dev, "%s: invalid 'va_buffering_pcm_rate'\n",
			__func__);
		goto err_kfifo_free;
	} else if (ret ==  -EINVAL) {
		dev_info(p->dev,
			"%s: no va buffering pcm rate, setting to 16000\n",
			__func__);
		p->pdata->va_buffering_pcm_rate = 16000;
	} else {
		if (p->pdata->va_buffering_pcm_rate != 16000 &&
			p->pdata->va_buffering_pcm_rate != 32000)
			p->pdata->va_buffering_pcm_rate = 16000;

		dev_info(p->dev,
			"%s: using va_buffering_pcm_rate of %u from device-tree\n",
			 __func__, p->pdata->va_buffering_pcm_rate);
	}

#endif

	/* default mode is PCM mode */
	p->audio_mode = DBMDX_AUDIO_MODE_PCM;
	p->audio_pcm_rate = 16000;
	p->bytes_per_sample = 2;

	p->va_flags.irq_inuse = 0;
	ret = request_threaded_irq(p->sv_irq,
				  dbmdx_sv_interrupt_hard,
				  dbmdx_sv_interrupt_soft,
				  IRQF_TRIGGER_RISING,
				  "dbmdx_sv", p);


	if (ret < 0) {
		dev_err(p->dev, "%s: cannot get irq\n", __func__);
		goto err_kfifo_free;
	}

	ret = irq_set_irq_wake(p->sv_irq, 1);
	if (ret < 0) {
		dev_err(p->dev, "%s: cannot set irq_set_irq_wake\n", __func__);
		goto err_free_irq;
	}

	p->ns_class = class_create(THIS_MODULE, "voicep");
	if (IS_ERR(p->ns_class)) {
		dev_err(p->dev, "%s: failed to create class\n", __func__);
		goto err_free_irq;
	}

	p->dbmdx_dev = device_create(p->ns_class, NULL, 0, p, "dbmdx");
	if (IS_ERR(p->dbmdx_dev)) {
		dev_err(p->dev, "%s: could not create device\n", __func__);
		goto err_class_destroy;
	}

	ret = sysfs_create_group(&p->dbmdx_dev->kobj, &dbmdx_attribute_group);
	if (ret) {
		dev_err(p->dbmdx_dev, "%s: failed to create sysfs group\n",
			__func__);
		goto err_device_unregister;
	}

	ret = dbmdx_register_cdev(p);
	if (ret)
		goto err_sysfs_remove_group;

	/* register the codec */
	ret = snd_soc_register_codec(p->dev,
				     &soc_codec_dev_dbmdx,
				     dbmdx_dais,
				     ARRAY_SIZE(dbmdx_dais));
	if (ret != 0) {
		dev_err(p->dev,
			"%s: Failed to register codec and its DAI: %d\n",
			__func__, ret);
		goto err_device_unregister2;
	}

	boot_thread = kthread_run(dbmdx_fw_load_thread,
				  (void *)p,
				  "dbmdx probe thread");
	if (IS_ERR_OR_NULL(boot_thread)) {
		dev_err(p->dev,
			"%s: Cannot create DBMDX boot thread\n", __func__);
		ret = -EIO;
		goto err_codec_unregister;
	}

	dev_info(p->dev, "%s: registered DBMDX codec driver version = %s\n",
		__func__, DRIVER_VERSION);

	return 0;

err_codec_unregister:
	snd_soc_unregister_codec(p->dev);
err_device_unregister2:
	device_unregister(p->record_dev);
err_sysfs_remove_group:
	sysfs_remove_group(&p->dev->kobj, &dbmdx_attribute_group);
err_device_unregister:
	device_unregister(p->dbmdx_dev);
err_class_destroy:
	class_destroy(p->ns_class);
err_free_irq:
	p->va_flags.irq_inuse = 0;
	irq_set_irq_wake(p->sv_irq, 0);
	free_irq(p->sv_irq, p);
err_kfifo_free:
	kfifo_free(&p->detection_samples_kfifo);
err_kfifo_pcm_free:
	kfifo_free(&p->pcm_kfifo);
err_gpio_free4:
	if (p->pdata->gpio_wakeup >= 0)
		gpio_free(p->pdata->gpio_wakeup);
	wake_lock_destroy(&p->ps_nosuspend_wl);
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
	p->clk_disable(p, DBMDX_CLK_CONSTANT);
	return ret;
}

static void dbmdx_common_remove(struct dbmdx_private *p)
{
	flush_workqueue(p->dbmdx_workq);
	destroy_workqueue(p->dbmdx_workq);
	snd_soc_unregister_codec(p->dev);
	dbmdx_deregister_cdev(p);
	sysfs_remove_group(&p->dev->kobj, &dbmdx_attribute_group);
	device_unregister(p->dbmdx_dev);
	class_destroy(p->ns_class);
	kfifo_free(&p->pcm_kfifo);
	kfifo_free(&p->detection_samples_kfifo);
	p->va_flags.irq_inuse = 0;
	disable_irq(p->sv_irq);
	irq_set_irq_wake(p->sv_irq, 0);
	free_irq(p->sv_irq, p);
	if (p->pdata->gpio_wakeup >= 0)
		gpio_free(p->pdata->gpio_wakeup);
	gpio_free(p->pdata->gpio_reset);
	gpio_free(p->pdata->gpio_sv);
	vfree(p->amodel_buf);
	wake_lock_destroy(&p->ps_nosuspend_wl);
#ifdef CONFIG_OF
	if (p->pdata->vqe_cfg_values)
		kfree(p->pdata->vqe_cfg_value);
	if (p->pdata->vqe_modes_values)
		kfree(p->pdata->vqe_modes_value);
	if (p->pdata->va_cfg_values)
		kfree(p->pdata->va_cfg_value);
#endif
	/* disable constant clock */
	p->clk_disable(p, DBMDX_CLK_CONSTANT);
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

static int dbmdx_get_devtree_pdata(struct device *dev,
		struct dbmdx_private *p)
{
	struct dbmdx_platform_data *pdata = p->pdata;
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
		if (len < DBMDX_VA_NR_OF_SPEEDS * 4) {
			dev_err(dev,
				"%s: VA speed configuration table too short\n",
				__func__);
			ret = -ENOMEM;
			goto err_kfree2;
		}
		ret = of_property_read_u32_array(np,
						 "va-speeds",
						 (u32 *)&pdata->va_speed_cfg,
						 DBMDX_VA_NR_OF_SPEEDS * 4);
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
			DBMDX_VA_NR_OF_SPEEDS);
		for (i = 0; i < DBMDX_VA_NR_OF_SPEEDS; i++)
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
	p->master_clk = clk_get(p->dev, "dbmdx_master_clk");
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

	p->constant_clk = clk_get(p->dev, "dbmdx_constant_clk");
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
static int dbmdx_name_match(struct device *dev, void *dev_name)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (!pdev || !pdev->name)
		return 0;

	return !strcmp(pdev->name, dev_name);
}

static int dbmdx_spi_name_match(struct device *dev, void *dev_name)
{
	struct spi_device *spi_dev = to_spi_device(dev);

	if (!spi_dev || !spi_dev->modalias)
		return 0;

	return !strcmp(spi_dev->modalias, dev_name);
}

static int dbmdx_i2c_name_match(struct device *dev, void *dev_name)
{
	struct i2c_client *i2c_dev = to_i2c_client(dev);

	if (!i2c_dev || !i2c_dev->name)
		return 0;

	return !strcmp(i2c_dev->name, dev_name);
}

struct i2c_client *dbmdx_find_i2c_device_by_name(char *dev_name)
{
	struct device *dev;

	dev = bus_find_device(&i2c_bus_type, NULL, dev_name,
					 dbmdx_i2c_name_match);

	return dev ? to_i2c_client(dev) : NULL;
}

struct spi_device *dbmdx_find_spi_device_by_name(char *dev_name)
{
	struct device *dev;

	dev = bus_find_device(&spi_bus_type, NULL,
				dev_name, dbmdx_spi_name_match);
	return dev ? to_spi_device(dev) : NULL;
}

struct platform_device *dbmdx_find_platform_device_by_name(char *dev_name)
{
	struct device *dev;

	dev = bus_find_device(&platform_bus_type, NULL,
				dev_name, dbmdx_name_match);
	return dev ? to_platform_device(dev) : NULL;
}

static int verify_platform_data(struct device *dev,
		struct dbmdx_private *p)
{
	struct dbmdx_platform_data *pdata = p->pdata;
	int i;

	dev_info(dev, "%s: VA firmware name: %s\n",
			 __func__, pdata->va_firmware_name);

	dev_info(dev, "%s: VQE firmware name: %s\n",
			__func__, pdata->vqe_firmware_name);

	dev_info(dev, "%s: VQE non-overlay firmware name: %s\n",
		 __func__, pdata->vqe_non_overlay_firmware_name);

	/* check for features */
	if (pdata->feature_va)
		dev_info(dev, "%s: VA feature activated\n", __func__);
	else
		dev_info(dev, "%s: VA feature not activated\n", __func__);

	if (pdata->feature_vqe)
		dev_info(dev, "%s: VQE feature activated\n", __func__);
	else
		dev_info(dev, "%s: VQE feature not activated\n", __func__);

	if (pdata->feature_fw_overlay)
		dev_info(dev, "%s: Firmware overlay activated\n", __func__);
	else
		dev_info(dev, "%s: Firmware overlay not activated\n", __func__);

	/* check if enabled features make sense */
	if (!pdata->feature_va && !pdata->feature_vqe) {
		dev_err(dev, "%s: No feature activated\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < pdata->va_cfg_values; i++)
			dev_dbg(dev, "%s: VA cfg %8.8x: 0x%8.8x\n",
				__func__, i, pdata->va_cfg_value[i]);

	for (i = 0; i < DBMDX_VA_NR_OF_SPEEDS; i++)
		dev_dbg(dev, "%s: VA speed cfg %8.8x: 0x%8.8x %u %u %u\n",
			__func__,
			i,
			pdata->va_speed_cfg[i].cfg,
			pdata->va_speed_cfg[i].uart_baud,
			pdata->va_speed_cfg[i].i2c_rate,
			pdata->va_speed_cfg[i].spi_rate);

	for (i = 0; i < pdata->vqe_cfg_values; i++)
		dev_dbg(dev, "%s: VQE cfg %8.8x: 0x%8.8x\n",
			__func__, i, pdata->vqe_cfg_value[i]);

	for (i = 0; i < pdata->vqe_modes_values; i++)
		dev_dbg(dev, "%s: VQE mode %8.8x: 0x%8.8x\n",
			__func__, i, pdata->vqe_modes_value[i]);

	dev_info(dev, "%s: VQE TDM bypass config: 0x%8.8x\n",
		__func__,
		pdata->vqe_tdm_bypass_config);

	pdata->vqe_tdm_bypass_config &= 0x7;

	for (i = 0; i < 3; i++)
		dev_dbg(dev, "%s: VA mic cfg %8.8x: 0x%8.8x\n",
			__func__, i, pdata->va_mic_config[i]);

	dev_info(dev, "%s: VA default mic config: 0x%8.8x\n",
		 __func__, pdata->va_initial_mic_config);

	if (p->va_backlog_length > 0xfff)
		p->va_backlog_length = 0xfff;
	dev_info(dev,
		"%s: using backlog length of %d\n",
		 __func__, p->va_backlog_length);

	dev_info(dev,
		"%s: using master-clk-rate of %dHZ\n",
		__func__, pdata->master_clk_rate);

	/* gather clock information */
	p->master_clk = clk_get(p->dev, "dbmdx_master_clk");
	if (IS_ERR(p->master_clk)) {
		dev_info(dev,
			"%s: no master-clk definition\n",
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


	dev_info(dev,
		"%s: using constant-clk-rate %dHZ\n",
		 __func__, pdata->constant_clk_rate);

	p->constant_clk = clk_get(p->dev, "dbmdx_constant_clk");
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

	if (pdata->use_gpio_for_wakeup > 1)
		pdata->use_gpio_for_wakeup = 1;

	dev_info(dev,
		"%s: using use_gpio_for_wakeup of %d\n",
		 __func__, pdata->use_gpio_for_wakeup);

	if (pdata->send_wakeup_seq > 1)
		pdata->send_wakeup_seq = 1;

	dev_info(dev,
		"%s: using send_wakeup_seq of %d\n",
		 __func__, pdata->send_wakeup_seq);

	if (pdata->wakeup_set_value > 1)
		pdata->wakeup_set_value = 1;

	dev_info(dev,
		"%s: using wakeup_set_value of %d\n",
		 __func__, pdata->wakeup_set_value);

	dev_info(dev,
		"%s: using firmware_id of 0x%8x\n",
		 __func__, pdata->firmware_id);

	if (pdata->auto_buffering != 0 &&
	    pdata->auto_buffering != 1)
		pdata->auto_buffering = 0;

	dev_info(dev,
		"%s: using auto_buffering of %d\n",
		 __func__, pdata->auto_buffering);

	if (pdata->auto_detection != 0 &&
		pdata->auto_detection != 1)
		pdata->auto_detection = 1;

	dev_info(dev,
		"%s: using auto_detection of %d\n",
		 __func__, pdata->auto_detection);

	if (pdata->send_uevent_on_detection != 0 &&
		pdata->send_uevent_on_detection != 1)
		pdata->send_uevent_on_detection = 0;

	dev_info(dev,
		"%s: using send_uevent_on_detection of %d\n",
		 __func__, pdata->send_uevent_on_detection);

	if (pdata->uart_low_speed_enabled != 0 &&
	pdata->uart_low_speed_enabled != 1)
		pdata->uart_low_speed_enabled = 0;

	dev_info(p->dev,
		"%s: using uart_low_speed_enabled of %d\n",
		 __func__, pdata->uart_low_speed_enabled);

	dev_info(p->dev,
		"%s: using min_samples_chunk_size of %d\n",
		 __func__, pdata->min_samples_chunk_size);

	if (pdata->pcm_streaming_mode != 0 &&
		pdata->pcm_streaming_mode != 1)
		pdata->pcm_streaming_mode = 0;

	dev_info(dev,
		"%s: using pcm_streaming_mode of %d\n",
		 __func__, pdata->pcm_streaming_mode);

	if (pdata->detection_buffer_channels < 0 ||
	pdata->detection_buffer_channels > 2)
		pdata->detection_buffer_channels = 0;

	dev_info(p->dev,
		"%s: using detection_buffer_channels of %d\n",
		 __func__, pdata->detection_buffer_channels);

	if (pdata->va_buffering_pcm_rate != 16000 &&
		pdata->va_buffering_pcm_rate != 32000)
		pdata->va_buffering_pcm_rate = 16000;

	dev_info(p->dev,
		"%s: using va_buffering_pcm_rate of %u\n",
		__func__, pdata->va_buffering_pcm_rate);

	return 0;
}

#endif

static int dbmdx_platform_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct device_node *np = pdev->dev.of_node;
	struct device_node *cmd_interface_np;
#endif
	struct dbmdx_platform_data *pdata;
	struct dbmdx_private *p;
	int ret = 0;
	struct spi_device *spi_dev;
	struct i2c_client *i2c_client;
	struct platform_device *uart_client;
	struct chip_interface *chip = NULL;
	enum dbmdx_active_interface active_interface = DBMDX_INTERFACE_NONE;

	dev_info(&pdev->dev, "%s: DBMDX codec driver version = %s\n",
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
	i2c_client = dbmdx_find_i2c_device_by_name("dbmdx-i2c");

	if (!i2c_client)
		i2c_client = dbmdx_find_i2c_device_by_name("dbmd4-i2c");

	if (!i2c_client)
		i2c_client = dbmdx_find_i2c_device_by_name("dbmd2-i2c");

#endif
	if (i2c_client) {
		/* got I2C command interface */
		chip = i2c_get_clientdata(i2c_client);
		if (!chip)
			return -EPROBE_DEFER;

		active_interface = DBMDX_INTERFACE_I2C;
	}
#ifdef CONFIG_OF
	uart_client = of_find_platform_device_by_node(cmd_interface_np);
#else
	uart_client = dbmdx_find_platform_device_by_name("dbmdx-uart");

	if (!uart_client)
		uart_client = dbmdx_find_platform_device_by_name("dbmd4-uart");

	if (!uart_client)
		uart_client = dbmdx_find_platform_device_by_name("dbmd2-uart");

#endif
	if (uart_client) {
		/* got UART command interface */
		chip = dev_get_drvdata(&uart_client->dev);
		if (!chip)
			return -EPROBE_DEFER;

		active_interface = DBMDX_INTERFACE_UART;
	}

#ifdef CONFIG_OF
	spi_dev = of_find_spi_device_by_node(cmd_interface_np);
#else
	spi_dev = dbmdx_find_spi_device_by_name("dbmdx-spi");

	if (!spi_dev)
		spi_dev = dbmdx_find_spi_device_by_name("dbmd4-spi");

	if (!spi_dev)
		spi_dev = dbmdx_find_spi_device_by_name("dbmd2-spi");

#endif
	if (spi_dev) {
		/* got spi command interface */
			dev_info(&pdev->dev,
				"%s: spi interface node %p\n",
				__func__, spi_dev);

		chip = spi_get_drvdata(spi_dev);
		if (!chip)
			return -EPROBE_DEFER;

		active_interface = DBMDX_INTERFACE_SPI;
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
	pdata = kzalloc(sizeof(struct dbmdx_platform_data), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto pfree_out;
	}

	p->pdata = pdata;

	ret = dbmdx_get_devtree_pdata(&pdev->dev, p);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to find device tree\n",
			__func__);
		kfree(pdata);
		goto pfree_out;
	}
#else
	pdata = dev_get_platdata(&pdev->dev);

	if (pdata == NULL) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "%s: Failed to get platform data\n",
			__func__);
		goto pfree_out;
	}

	p->pdata = pdata;

	ret = verify_platform_data(&pdev->dev, p);
	if (ret) {
		dev_err(&pdev->dev, "%s: Failed to verify platform data\n",
			__func__);
		goto pfree_out;
	}
#endif

	p->dev = &pdev->dev;

	p->vregulator = devm_regulator_get(&pdev->dev, "dbmdx_regulator");
	if (IS_ERR(p->vregulator)) {
		dev_info(&pdev->dev, "%s: Can't get voltage regulator\n",
			__func__);
		p->vregulator = NULL;
	}

	/* set initial mic as it appears in the platform data */
	p->va_current_mic_config = pdata->va_initial_mic_config;
	p->va_active_mic_config = pdata->va_initial_mic_config;

	p->va_cur_analog_gain = 0x1000;
	p->va_cur_digital_gain = 0x1000;

	p->vqe_vc_syscfg = DBMDX_VQE_SET_SYSTEM_CONFIG_SECONDARY_CFG;

	/* initialize delayed pm work */
	INIT_DELAYED_WORK(&p->delayed_pm_work,
			  dbmdx_delayed_pm_work_hibernate);
	p->dbmdx_workq = create_workqueue("dbmdx-wq");
	if (!p->dbmdx_workq) {
		dev_err(&pdev->dev, "%s: Could not create workqueue\n",
			__func__);
		ret = -EIO;
		goto pfree_out;
	}

	/* set helper functions */
	p->reset_set = dbmdx_reset_set;
	p->reset_release = dbmdx_reset_release;
	p->reset_sequence = dbmdx_reset_sequence;
	p->wakeup_set = dbmdx_wakeup_set;
	p->wakeup_release = dbmdx_wakeup_release;
	p->lock = dbmdx_lock;
	p->unlock = dbmdx_unlock;
	p->verify_checksum = dbmdx_verify_checksum;
	p->va_set_speed = dbmdx_va_set_speed;
	p->clk_get_rate = dbmdx_clk_get_rate;
	p->clk_enable = dbmdx_clk_enable;
	p->clk_disable = dbmdx_clk_disable;

	/* set callbacks (if already set externally) */
	if (g_set_i2c_freq_callback)
		p->set_i2c_freq_callback = g_set_i2c_freq_callback;
	if (g_event_callback)
		p->event_callback = g_event_callback;

	/* set chip interface */
	p->chip = chip;

	p->rxsize = MAX_REQ_SIZE;

	ret = dbmdx_common_probe(p);
	if (ret < 0) {
		dev_err(p->dev, "%s: probe failed\n", __func__);
		goto pfree_out;
	}

	p->active_interface = active_interface;

	dev_info(p->dev, "%s: successfully probed\n", __func__);
	return 0;
pfree_out:
	kfree(p);
out:
	return ret;
}

static int dbmdx_platform_remove(struct platform_device *pdev)
{
	struct dbmdx_private *p = dev_get_drvdata(&pdev->dev);

	dbmdx_common_remove(p);

	return 0;
}

static struct of_device_id dbmdx_of_match[] = {
	{ .compatible = "dspg,dbmdx-codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(of, dbmdx_of_match);

static struct platform_driver dbmdx_platform_driver = {
	.driver = {
		.name = "dbmdx-codec",
		.owner = THIS_MODULE,
		.of_match_table = dbmdx_of_match,
	},
	.probe =    dbmdx_platform_probe,
	.remove =   dbmdx_platform_remove,
};

static int __init dbmdx_modinit(void)
{
	return platform_driver_register(&dbmdx_platform_driver);
}
module_init(dbmdx_modinit);

static void __exit dbmdx_exit(void)
{
	platform_driver_unregister(&dbmdx_platform_driver);
}
module_exit(dbmdx_exit);

MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION("DSPG DBMDX codec driver");
MODULE_LICENSE("GPL");
