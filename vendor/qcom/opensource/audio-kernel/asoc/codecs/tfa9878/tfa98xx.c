/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "%s(): " fmt, __func__

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include "inc/config.h"
#include "inc/tfa98xx.h"
#include "inc/tfa.h"
#include "inc/tfa_internal.h"

#include "inc/tfa98xx_tfafieldnames.h"

#define TFA98XX_VERSION	TFA98XX_API_REV_STR

#define I2C_RETRIES 50
#define I2C_RETRY_DELAY 5 /* ms */
#define TFA_RESET_DELAY 5 /* ms */

#include <linux/power_supply.h>
#define REF_TEMP_DEVICE_NAME "battery"

#define TFA98XX_VERSION	TFA98XX_API_REV_STR

/* Change volume selection behavior:
 * Uncomment following line to generate a profile change when updating
 * a volume control (also changes to the profile of the modified volume
 * control)
 */
/*#define TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL	1*/

/* Supported rates and data formats */
/* if post-conversion is assumed */
#define TFA98XX_RATES SNDRV_PCM_RATE_8000_192000

static unsigned int sr_converted = 48000;

#define TFA98XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

/* data accessible by all instances */
/* Memory pool used for DSP messages */
static struct kmem_cache *tfa98xx_cache;
/* Mutex protected data */
static DEFINE_MUTEX(tfa98xx_mutex);
static DEFINE_MUTEX(probe_lock);
static LIST_HEAD(tfa98xx_device_list);
static int tfa98xx_device_count;
static int tfa98xx_sync_count;
static int tfa98xx_monitor_count;
#define MONITOR_COUNT_MAX 5
static int tfa98xx_cnt_reload;

static LIST_HEAD(profile_list); /* list of user selectable profiles */
static int tfa98xx_mixer_profiles; /* number of user selectable profiles */
static int tfa98xx_mixer_profile; /* current mixer profile */
static struct snd_kcontrol_new *tfa98xx_controls;
static struct tfa_container *tfa98xx_container;

static int buf_pool_size[POOL_MAX_INDEX] = {
	64 * 1024,
	64 * 1024,
	64 * 1024,
	64 * 1024,
	64 * 1024,
	8 * 1024
};

static int tfa98xx_kmsg_regs;
static int tfa98xx_ftrace_regs;

static char *fw_name = "tfa98xx.cnt";
module_param(fw_name, charp, 0644);
MODULE_PARM_DESC(fw_name, "TFA98xx DSP firmware (container file) name.");

static int trace_level;
module_param(trace_level, int, 0444);
MODULE_PARM_DESC(trace_level, "TFA98xx debug trace level (0=off, bits:1=verbose,2=regdmesg,3=regftrace,4=timing).");

static char *dflt_prof_name = "";
module_param(dflt_prof_name, charp, 0444);

static int no_start;
module_param(no_start, int, 0444);
MODULE_PARM_DESC(no_start, "do not start the work queue; for debugging via user\n");

static int no_reset;
module_param(no_reset, int, 0444);
MODULE_PARM_DESC(no_reset, "do not use the reset line; for debugging via user\n");

static int pcm_sample_format = -1;
/*
 * Be careful. Setting pcm_sample_format to 3 means
 * TDM settings will be dynamically adapted,
 * If there's TDM setting in container file (cnt),
 * it's to be overwritten with what's specified by hw_params.
 */
module_param(pcm_sample_format, int, 0444);
MODULE_PARM_DESC(pcm_sample_format, "PCM sample format: 0=S16_LE, 1=S24_LE, 2=S32_LE, -1=all\n");

static int pcm_no_constraint = 1;
module_param(pcm_no_constraint, int, 0444);
MODULE_PARM_DESC(pcm_no_constraint, "do not use constraints for PCM parameters\n");

static void tfa98xx_dsp_init(struct tfa98xx *tfa98xx);

static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable);

static int get_profile_from_list(char *buf, int id);
static int get_profile_id_for_sr(int id, unsigned int rate);

static int _tfa98xx_mute(struct tfa98xx *tfa98xx, int mute, int stream);
static int _tfa98xx_stop(struct tfa98xx *tfa98xx);

static void tfa98xx_check_calibration(struct tfa98xx *tfa98xx);
static int tfa98xx_run_calibration(struct tfa98xx *tfa98xx);

static void tfa98xx_set_dsp_configured(struct tfa98xx *tfa98xx);

static void tfa98xx_container_loaded
	(const struct firmware *cont, void *context);

struct tfa98xx_rate {
	unsigned int rate;
	unsigned int fssel;
};

static const struct tfa98xx_rate rate_to_fssel[] = {
	{8000, 0},
	{11025, 1},
	{12000, 2},
	{16000, 3},
	{22050, 4},
	{24000, 5},
	{32000, 6},
	{44100, 7},
	{48000, 8},
/* out of range */
	{64000, 9},
	{88200, 10},
	{96000, 11},
	{176400, 12},
	{192000, 13},
};

static const unsigned int index_to_rate[] = {
	5512, 8000, 11025, 16000, 22050, 32000, 44100, 48000
};

static inline char *_tfa_cont_profile_name
(struct tfa98xx *tfa98xx, int prof_idx)
{
	if (tfa98xx->tfa->cnt == NULL)
		return NULL;

	return tfa_cont_profile_name(tfa98xx->tfa->cnt,
		tfa98xx->tfa->dev_idx, prof_idx);
}

static enum tfa_error tfa98xx_write_re25(struct tfa_device *tfa, int value)
{
	enum tfa_error err;

	/* clear MTPEX */
	err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 0);
	if (err == tfa_error_ok) {
		/* set RE25 in shadow regiser */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_RE25_PRIM, value);
	}
	if (err == tfa_error_ok) {
		/* set MTPEX to copy RE25 into MTP */
		err = tfa_dev_mtp_set(tfa, TFA_MTP_EX, 1);
	}

	return err;
}

/* Wrapper for tfa start */
static enum tfa_error
tfa98xx_tfa_start(struct tfa98xx *tfa98xx, int next_profile, int vstep)
{
	enum tfa_error err;
	ktime_t start_time = 0;
	ktime_t stop_time = 0;
	u64 delta_time;

	if (trace_level & 8)
		start_time = ktime_get_boottime();

	err = tfa_dev_start(tfa98xx->tfa, next_profile, vstep);

	if (trace_level & 8) {
		stop_time = ktime_get_boottime();
		delta_time = ktime_to_ns(ktime_sub(stop_time, start_time));
		do_div(delta_time, 1000);
		dev_dbg(tfa98xx->dev, "tfa_dev_start(%d,%d) time = %lld us\n",
			next_profile, vstep, delta_time);
	}

	if ((err == tfa_error_ok) && (tfa98xx->set_mtp_cal)) {
		enum tfa_error err_cal = tfa_error_ok;

		if (tfa98xx->tfa->mtpex != 1)
			err_cal = tfa98xx_write_re25(tfa98xx->tfa,
				tfa98xx->cal_data);
		tfa98xx->tfa->mtpex = tfa_dev_mtp_get(tfa98xx->tfa,
			TFA_MTP_EX);
		if (err_cal != tfa_error_ok) {
			pr_err("%s: Error, setting MTPEX on dev %d by force, err=%d\n",
				__func__, tfa98xx->tfa->dev_idx, err_cal);
		} else {
			tfa98xx->set_mtp_cal = false;
			pr_info("%s: MTPEX: %d on dev %d\n",
				__func__, tfa98xx->tfa->mtpex,
				tfa98xx->tfa->dev_idx);
		}
	}

	/* Remove sticky bit by reading it once */
	tfa_get_noclk(tfa98xx->tfa);

	/* A cold start erases the configuration, including interrupts setting.
	 * Restore it if required
	 */
	tfa98xx_interrupt_enable(tfa98xx, true);

	return err;
}

#if defined(CONFIG_DEBUG_FS)
/* OTC reporting
 * Returns the MTP0 OTC bit value
 */
static int tfa98xx_dbgfs_otc_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_OTC);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to access MTPOTC: %d\n",
			tfa98xx->i2c->addr, value);
		return -EIO;
	}

	*val = value;
	pr_debug("[0x%x] OTC : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_otc_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (val != 0 && val != 1) {
		pr_err("[0x%x] Unexpected value %llu\n",
			tfa98xx->i2c->addr, val);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_OTC, val);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to access MTPOTC: err %d\n",
			tfa98xx->i2c->addr, err);
		return -EIO;
	}

	pr_debug("[0x%x] OTC < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value < 0) {
		pr_err("[0x%x] Unable to access MTPEX: %d\n",
			tfa98xx->i2c->addr, value);
		return -EIO;
	}

	*val = value;
	pr_debug("[0x%x] MTPEX : %d\n", tfa98xx->i2c->addr, value);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error err;
	enum tfa98xx_error ret;
	u16 temp_val = DEFAULT_REF_TEMP;
	int idx, ndev;
	struct tfa_device *ntfa = NULL;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (val != 0) {
		pr_err("[0x%x] Can only clear MTPEX (0 value expected)\n",
			tfa98xx->i2c->addr);
		return -EINVAL;
	}

	/* EXT_TEMP */
	ret = tfa98xx_read_reference_temp(&temp_val);
	if (ret)
		pr_err("error in reading reference temp\n");

	ndev = tfa98xx->tfa->dev_count;
	for (idx = 0; idx < ndev; idx++) {
		ntfa = tfa98xx_get_tfa_device_from_index(idx);
		if (ntfa == NULL)
			continue;
		tfa98xx_set_exttemp(ntfa, (short)temp_val);
	}

	mutex_lock(&tfa98xx->dsp_lock);
	err = tfa_dev_mtp_set(tfa98xx->tfa, TFA_MTP_EX, 0);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != tfa_error_ok) {
		pr_err("[0x%x] Unable to access MTPEX: err %d (suspended)\n",
			tfa98xx->i2c->addr, err);
		/* suspend until TFA98xx is active */
		tfa98xx->tfa->reset_mtpex = 1;
		return -EIO;
	}

	pr_debug("[0x%x] MTPEX < 0\n", tfa98xx->i2c->addr);

	return 0;
}

static int tfa98xx_dbgfs_temp_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	*val = tfa98xx_get_exttemp(tfa98xx->tfa);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP : %llu\n", tfa98xx->i2c->addr, *val);

	return 0;
}

static int tfa98xx_dbgfs_temp_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	tfa98xx_set_exttemp(tfa98xx->tfa, (short)val);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static ssize_t tfa98xx_dbgfs_start_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;
	int ret = 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	tfa98xx_check_calibration(tfa98xx);

	if (tfa98xx->calibrate_done) {
		pr_info("[0x%x] Calibration Success\n", tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "Success\n");
		ret = sizeof("Success");
	} else {
		pr_info("[0x%x] Calibration Fail\n", tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "Fail\n");
		ret = sizeof("Fail");
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
	kfree(str);

	return ret;
}

static ssize_t tfa98xx_dbgfs_start_set(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char buf[32];
	int buf_size;
	static const char ref[] = "1"; /* "please calibrate now" */

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count > sizeof(ref) + 1 || count < (sizeof(ref) - 1))
		return -EINVAL;

	buf_size = min(count, (size_t)(sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (strncmp(buf, ref, sizeof(ref) - 1)) {
		pr_info("[0x%x] %s: calibration is triggered with %s!\n",
			tfa98xx->i2c->addr, __func__, ref);
		return -EINVAL;
	}

	ret = tfa98xx_run_calibration(tfa98xx);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t tfa98xx_dbgfs_r_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;
	uint16_t status;
	int ret;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);

	/* Need to ensure DSP is access-able, use mtp read access for this
	 * purpose
	 */
	ret = tfa98xx_get_mtp(tfa98xx->tfa, &status);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] MTP read failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	ret = tfa_run_speaker_calibration(tfa98xx->tfa);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] calibration failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	if (tfa98xx->tfa->spkr_count > 1) {
		ret = snprintf(str, PAGE_SIZE,
			"Prim:%d mOhms, Sec:%d mOhms\n",
			tfa98xx->tfa->mohm[0],
			tfa98xx->tfa->mohm[1]);
	} else {
		ret = snprintf(str, PAGE_SIZE,
			"Prim:%d mOhms\n",
			tfa98xx->tfa->mohm[0]);
	}

	pr_debug("[0x%x] calib_done: %s", tfa98xx->i2c->addr, str);

	if (ret < 0)
		goto r_err;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);

r_err:
	kfree(str);
r_c_err:
	mutex_unlock(&tfa98xx->dsp_lock);
	return ret;
}

static ssize_t tfa98xx_dbgfs_version_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	char str[] = TFA98XX_VERSION "\n";
	int ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, sizeof(str));

	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	switch (tfa98xx->dsp_init) {
	case TFA98XX_DSP_INIT_STOPPED:
		str = "Stopped\n";
		break;
	case TFA98XX_DSP_INIT_RECOVER:
		str = "Recover requested\n";
		break;
	case TFA98XX_DSP_INIT_FAIL:
		str = "Failed init\n";
		break;
	case TFA98XX_DSP_INIT_PENDING:
		str = "Pending init\n";
		break;
	case TFA98XX_DSP_INIT_DONE:
		str = "Init complete\n";
		break;
	default:
		str = "Invalid\n";
		break;
	}

	pr_debug("[0x%x] dsp_state : %s\n", tfa98xx->i2c->addr, str);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_set(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error ret;
	char buf[32];
	static const char start_cmd[] = "start";
	static const char stop_cmd[] = "stop";
	static const char mon_start_cmd[] = "monitor start";
	static const char mon_stop_cmd[] = "monitor stop";
	int buf_size;

	buf_size = min(count, (size_t)(sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* Compare strings, excluding the trailing \0 */
	if (!strncmp(buf, start_cmd, sizeof(start_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp start...\n",
			tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start(tfa98xx,
			tfa98xx->profile, tfa98xx->vstep);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_start complete: %d\n",
			tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, stop_cmd, sizeof(stop_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp stop...\n",
			tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa_dev_stop(tfa98xx->tfa);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_dev_stop complete: %d\n",
			tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, mon_start_cmd,
		sizeof(mon_start_cmd) - 1)) {
		pr_info("[0x%x] Manual start of monitor thread...\n",
			tfa98xx->i2c->addr);
		tfa98xx_monitor_count = -1;
		queue_delayed_work(tfa98xx->tfa98xx_wq,
			&tfa98xx->monitor_work, HZ);
	} else if (!strncmp(buf, mon_stop_cmd,
		sizeof(mon_stop_cmd) - 1)) {
		pr_info("[0x%x] Manual stop of monitor thread...\n",
			tfa98xx->i2c->addr);
		cancel_delayed_work_sync(&tfa98xx->monitor_work);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_fw_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;

	switch (tfa98xx->dsp_fw_state) {
	case TFA98XX_DSP_FW_NONE:
		str = "None\n";
		break;
	case TFA98XX_DSP_FW_PENDING:
		str = "Pending\n";
		break;
	case TFA98XX_DSP_FW_FAIL:
		str = "Fail\n";
		break;
	case TFA98XX_DSP_FW_OK:
		str = "Ok\n";
		break;
	default:
		str = "Invalid\n";
		break;
	}

	pr_debug("[0x%x] fw_state : %s", tfa98xx->i2c->addr, str);

	return simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
}

static ssize_t tfa98xx_dbgfs_rpc_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	uint8_t *buffer;
	enum tfa98xx_error error;
	struct tfa_device *tfa0 = NULL;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	tfa0 = tfa98xx->tfa;

	pr_info("%s called (count %d)\n", __func__, (int)count);
	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg_read(tfa0, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error != TFA98XX_ERROR_OK) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	ret = copy_to_user(user_buf, buffer, count);
	kfree(buffer);
	if (ret)
		return -EFAULT;

	*ppos += count;
	return count;
}

static ssize_t tfa98xx_dbgfs_rpc_send(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	struct tfa_file_dsc *msg_file;
	enum tfa98xx_error error;
	struct tfa_device *tfa0 = NULL;
	int ret = 0;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	/* msg_file.name is not used */
	msg_file = kmalloc(count + sizeof(struct tfa_file_dsc), GFP_KERNEL);
	if (msg_file == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}
	msg_file->size = (uint32_t)count;

	tfa0 = tfa98xx->tfa;

	if (copy_from_user(msg_file->data, user_buf, count)) {
		kfree(msg_file);
		return -EFAULT;
	}

	pr_info("%s called\n", __func__);
	mutex_lock(&tfa98xx->dsp_lock);
	tfa0->individual_msg = 1;
	if ((msg_file->data[0] == 'M') && (msg_file->data[1] == 'G')) {
		/* int vstep_idx, int vstep_msg_idx both 0 */
		error = tfa_cont_write_file(tfa0,
			msg_file, 0, 0);
		if (error != TFA98XX_ERROR_OK) {
			pr_debug("[0x%x] tfa_cont_write_file error: %d\n",
				tfa98xx->i2c->addr, error);
			ret = -EIO;
		}
	} else {
		error = dsp_msg(tfa0, msg_file->size, msg_file->data);
		if (error != TFA98XX_ERROR_OK) {
			pr_debug("[0x%x] dsp_msg error: %d\n",
				tfa98xx->i2c->addr, error);
			ret = -EIO;
		}
	}
	mutex_unlock(&tfa98xx->dsp_lock);

	kfree(msg_file);

	if (ret)
		return ret;

	return count;
}
/* -- RPC */

/* ++ DSP message fops */
static ssize_t tfa98xx_dbgfs_dsp_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error error;
	int ret = 0;
	uint8_t *buffer;
	struct tfa_device *tfa0 = NULL;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	tfa0 = tfa98xx->tfa;

	pr_info("%s called (count %d)\n", __func__, (int)count);
	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg_read(tfa0, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	/* ret = simple_read_from_buffer
	 * (user_buf, count, ppos, buffer, count);
	 */
	ret = copy_to_user(user_buf, buffer, count);
	if (ret) {
		pr_debug("[0x%x] cannot copy buffer to user: %d\n",
			tfa98xx->i2c->addr, ret);
		kfree(buffer);
		return -EFAULT;
	}

	kfree(buffer);

	return count;
}

static ssize_t tfa98xx_dbgfs_dsp_write(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error error;
	int ret = 0;
	struct tfa_device *tfa0 = NULL;
	uint8_t *buffer;

	if (tfa98xx->tfa == NULL) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (tfa98xx->pstream == 0
		|| tfa98xx->tfa->is_configured <= 0) {
		pr_info("%s: skipped - tfadsp is not active!\n",
			__func__);
		return -EIO;
	}

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	tfa0 = tfa98xx->tfa;

	ret = copy_from_user(buffer, user_buf, count);
	if (ret) {
		pr_debug("[0x%x] cannot copy buffer from user: %d\n",
			tfa98xx->i2c->addr, ret);
		kfree(buffer);
		return -EFAULT;
	}

	pr_info("%s called\n", __func__);
	mutex_lock(&tfa98xx->dsp_lock);
	tfa0->individual_msg = 1;
	error = dsp_msg(tfa0, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error) {
		pr_debug("[0x%x] dsp_msg error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	kfree(buffer);

	return count;
}
/* -- DSP */

static ssize_t tfa98xx_dbgfs_spkr_damaged_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	str = kmalloc(count, GFP_KERNEL);
	if (str == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	scnprintf(str, PAGE_SIZE, "%s\n",
		(tfa98xx->tfa->spkr_damaged == 1)
		? "damaged" : "ready");

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));

	kfree(str);

	return ret;
}

static int tfa98xx_dbgfs_pga_gain_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	unsigned int value;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	value = tfa_get_pga_gain(tfa98xx->tfa);
	if (value < 0)
		return -EINVAL;
	*val = value;

	return 0;
}

static int tfa98xx_dbgfs_pga_gain_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	uint16_t value;
	int err;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	value = val & 0xffff;
	if (value > 7)
		return -EINVAL;

	err = tfa_set_pga_gain(tfa98xx->tfa, value);
	if (err < 0)
		return -EINVAL;

	return 0;
}

static ssize_t tfa98xx_dbgfs_trace_level_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	char out_buf[4] = {0};
	int ret = 0;

	if (count < 1) {
		pr_err("%s: read size exceeds buf size %zd\n", __func__, count);
		return 0;
	}
	snprintf(out_buf, 4, "%d\n", trace_level);

	ret = simple_read_from_buffer(user_buf,
		count, ppos, out_buf, sizeof(out_buf));

	return ret;
}

static ssize_t tfa98xx_dbgfs_trace_level_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int tl = 0;
	char buf[2] = {0};

	if (copy_from_user(buf, user_buf, 1))
		return -EFAULT;

	tl = buf[0] - 48;
	pr_info("%s: trace_level = %d\n", __func__, tl);
	if (tl < 0 || tl > 15)
		return -EFAULT;

	trace_level = tl;
	tfa98xx_kmsg_regs = trace_level & 2;
	tfa98xx_ftrace_regs = trace_level & 4;
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (tfa98xx && tfa98xx->tfa)
			tfa98xx->tfa->verbose = trace_level & 1;
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_show_cal_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	int mtp = 0;
	int mtpex = 0;
	char out_buf[64] = {0};

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	if (count < 1) {
		pr_err("%s: read size exceeds buf size %zd\n", __func__, count);
		return 0;
	}

	mtp = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_RE25);
	mtpex = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);

	snprintf(out_buf, 64, "[%s] MTPEX: %d, MTP: %d mOhm\n",
		tfa_cont_device_name(tfa98xx->tfa->cnt, tfa98xx->tfa->dev_idx),
		mtpex, mtp);

	return simple_read_from_buffer(user_buf,
		count, ppos, out_buf, sizeof(out_buf));
}

/* Direct registers access - provide register address in hex */
#define TFA98XX_DEBUGFS_REG_SET(__reg)	\
static int tfa98xx_dbgfs_reg_##__reg##_set(void *data, u64 val)\
{\
	struct i2c_client *i2c = (struct i2c_client *)data;\
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);\
	unsigned int ret, value;\
\
	ret = regmap_write(tfa98xx->regmap, 0x##__reg, (val & 0xffff));\
	value = val & 0xffff;\
	return 0;\
} \
static int tfa98xx_dbgfs_reg_##__reg##_get(void *data, u64 *val)\
{\
	struct i2c_client *i2c = (struct i2c_client *)data;\
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);\
	unsigned int value;\
	int ret;\
\
	ret = regmap_read(tfa98xx->regmap, 0x##__reg, &value);\
	*val = value;\
	return 0;\
} \
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_reg_##__reg##_fops,\
	tfa98xx_dbgfs_reg_##__reg##_get,\
	tfa98xx_dbgfs_reg_##__reg##_set, "0x%llx\n")

#define VAL(str) #str
#define TOSTRING(str) VAL(str)
#define TFA98XX_DEBUGFS_REG_CREATE_FILE(__reg, __name, dbg_dir, i2c)	\
	debugfs_create_file(TOSTRING(__reg) "-" TOSTRING(__name),\
		0664, dbg_dir, i2c,\
		&tfa98xx_dbgfs_reg_##__reg##_fops)

TFA98XX_DEBUGFS_REG_SET(00);
TFA98XX_DEBUGFS_REG_SET(01);
TFA98XX_DEBUGFS_REG_SET(02);
TFA98XX_DEBUGFS_REG_SET(03);
TFA98XX_DEBUGFS_REG_SET(04);
TFA98XX_DEBUGFS_REG_SET(05);
TFA98XX_DEBUGFS_REG_SET(06);
TFA98XX_DEBUGFS_REG_SET(07);
TFA98XX_DEBUGFS_REG_SET(08);
TFA98XX_DEBUGFS_REG_SET(09);
TFA98XX_DEBUGFS_REG_SET(0A);
TFA98XX_DEBUGFS_REG_SET(0B);
TFA98XX_DEBUGFS_REG_SET(0F);
TFA98XX_DEBUGFS_REG_SET(10);
TFA98XX_DEBUGFS_REG_SET(11);
TFA98XX_DEBUGFS_REG_SET(12);
TFA98XX_DEBUGFS_REG_SET(13);
TFA98XX_DEBUGFS_REG_SET(22);
TFA98XX_DEBUGFS_REG_SET(25);

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_otc_fops,
	tfa98xx_dbgfs_otc_get,
	tfa98xx_dbgfs_otc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_mtpex_fops,
	tfa98xx_dbgfs_mtpex_get,
	tfa98xx_dbgfs_mtpex_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_temp_fops,
	tfa98xx_dbgfs_temp_get,
	tfa98xx_dbgfs_temp_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_pga_gain_fops,
	tfa98xx_dbgfs_pga_gain_get,
	tfa98xx_dbgfs_pga_gain_set, "%llu\n");

static const struct file_operations tfa98xx_dbgfs_calib_start_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_start_get,
	.write = tfa98xx_dbgfs_start_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_r_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_r_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_version_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_version_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_state_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_state_get,
	.write = tfa98xx_dbgfs_dsp_state_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_fw_state_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_fw_state_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_rpc_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_rpc_read,
	.write = tfa98xx_dbgfs_rpc_send,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_read,
	.write = tfa98xx_dbgfs_dsp_write,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_spkr_damaged_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_spkr_damaged_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_trace_level_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_trace_level_read,
	.write = tfa98xx_dbgfs_trace_level_write,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_show_cal_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = tfa98xx_dbgfs_show_cal_read,
	.llseek = default_llseek,
};

static void tfa98xx_debug_init(struct tfa98xx *tfa98xx, struct i2c_client *i2c)
{
	char name[50];

	scnprintf(name, MAX_CONTROL_NAME, "%s-%x", i2c->name, i2c->addr);
	tfa98xx->dbg_dir = debugfs_create_dir(name, NULL);
	debugfs_create_file("OTC", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_otc_fops);
	debugfs_create_file("MTPEX", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_mtpex_fops);
	debugfs_create_file("R", 0444, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_r_fops);
	debugfs_create_file("TEMP", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_temp_fops);
	debugfs_create_file("calibrate", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_calib_start_fops);
	debugfs_create_file("version", 0444, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_version_fops);
	debugfs_create_file("dsp-state", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_dsp_state_fops);
	debugfs_create_file("fw-state", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_fw_state_fops);
	debugfs_create_file("rpc", 0664, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_rpc_fops);
	debugfs_create_file("dsp", 0644, tfa98xx->dbg_dir,
		i2c, &tfa98xx_dbgfs_dsp_fops);

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		dev_dbg(tfa98xx->dev, "Adding pga_gain debug interface\n");
		debugfs_create_file("pga_gain", 0444, tfa98xx->dbg_dir,
			tfa98xx->i2c,
			&tfa98xx_dbgfs_pga_gain_fops);
	}

	debugfs_create_file("trace-level", 0644,
		tfa98xx->dbg_dir,
		tfa98xx->i2c,
		&tfa98xx_dbgfs_trace_level_fops);

	debugfs_create_file("mtp", 0644,
		tfa98xx->dbg_dir,
		tfa98xx->i2c,
		&tfa98xx_dbgfs_show_cal_fops);
}

static void tfa98xx_debug_remove(struct tfa98xx *tfa98xx)
{
	debugfs_remove_recursive(tfa98xx->dbg_dir);
}
#endif /* CONFIG_DEBUG_FS */

static void tfa98xx_check_calibration(struct tfa98xx *tfa98xx)
{
	unsigned short value = 0;

	mutex_lock(&tfa98xx->dsp_lock);
	value = tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (value >= 0) {
		tfa98xx->calibrate_done =
			(value) ? 1 : 0;
		pr_info("[0x%x] calibrate_done = MTPEX (%d)\n",
			tfa98xx->i2c->addr, tfa98xx->calibrate_done);

	} else {
		pr_info("[0x%x] error in reading MTPEX\n",
			tfa98xx->i2c->addr);
		tfa98xx->calibrate_done = 0;
	}
}

static int tfa98xx_run_calibration(struct tfa98xx *tfa98xx0)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa;
	enum tfa_error ret, cal_err = tfa_error_ok;
	int idx, ndev = tfa98xx_device_count;
	int cal_profile = 0;
	u16 temp_val = DEFAULT_REF_TEMP; /* default */
	int temp_calflag = 0;
	int ramp_steps;

	pr_info("%s: begin\n", __func__);

	if (tfa98xx0->pstream == 0) {
		pr_info("[0x%x] %s: calibration is available only when channel is enabled!\n",
			tfa98xx0->i2c->addr, __func__);
		return -EIO;
	}

	/* EXT_TEMP */
	ret = tfa98xx_read_reference_temp(&temp_val);
	if (ret) {
		pr_err("%s: error in reading reference temp\n",
			__func__);
		temp_val = DEFAULT_REF_TEMP; /* default */
	}

	for (idx = 0; idx < ndev; idx++) {
		tfa = tfa98xx_get_tfa_device_from_index(idx);
		if (tfa == NULL)
			continue;

		pr_info("%s: dev %d - force to enable auto calibration (%s -> enabled)",
			__func__, idx,
			(tfa->disable_auto_cal) ? "disabled" : "enabled");
		/* enable auto calibration */
		temp_calflag |= tfa->disable_auto_cal;
		tfa->disable_auto_cal = 0;

		/* force to enable all the devices */
		if (tfa->dev_count <= MAX_CHANNELS)
			tfa->set_active = 1;

		/* force to mute amplifier to flush buffer */
		ramp_steps = tfa->ramp_steps;
		tfa->ramp_steps = RAMPDOWN_SHORT;
		tfa_run_mute(tfa);
		tfa->ramp_steps = ramp_steps;
	}

	/* wait before restarting for calibration */
	msleep_interruptible(10);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		pr_info("%s: dev %d - stopping devices\n",
			__func__, tfa98xx->tfa->dev_idx);

		_tfa98xx_stop(tfa98xx);

		mutex_lock(&tfa98xx->dsp_lock);

		tfa98xx->calibrate_done = 0;

		tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;
		tfa98xx_set_dsp_configured(tfa98xx);

		mutex_unlock(&tfa98xx->dsp_lock);
	}

	pr_info("%s: calibration started!\n", __func__);

	for (idx = 0; idx < ndev; idx++) {
		tfa = tfa98xx_get_tfa_device_from_index(idx);
		if (tfa == NULL)
			continue;

		tfa98xx = (struct tfa98xx *)tfa->data;
		pr_info("%s: dev %d - starting devices for calibration\n",
			__func__, idx);

		mutex_lock(&tfa98xx->dsp_lock);

		cal_profile = tfa_cont_get_cal_profile(tfa98xx->tfa);
		if (cal_profile < 0) {
			pr_warn("[0x%x] no cal profile is defined!\n",
				tfa98xx->i2c->addr);
			/* use current profile if there's no cal profile */
			cal_profile = tfa98xx->profile;
		}

		ret = tfa98xx_tfa_start(tfa98xx, cal_profile, tfa98xx->vstep);
		if (ret != tfa_error_ok) {
			pr_warn("[0x%x] failure in starting device for calibration! (err %d)\n",
				tfa98xx->i2c->addr, ret);
			cal_err |= ret;
		}

		pr_debug("%s: [%d] force UNMUTE before calibration\n",
			__func__, tfa->dev_idx);
		tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 1);

		tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
		tfa98xx_set_dsp_configured(tfa98xx);

		mutex_unlock(&tfa98xx->dsp_lock);
	}

	pr_info("%s: restore flag for auto calibration (enabled -> %s)",
		__func__,
		(temp_calflag) ? "disabled" : "enabled");
	for (idx = 0; idx < ndev; idx++) {
		tfa = tfa98xx_get_tfa_device_from_index(idx);
		if (tfa == NULL)
			continue;

		/* restore flag for auto calibration */
		tfa->disable_auto_cal = temp_calflag;
	}

	if (cal_err) {
		pr_info("%s: calibration failed! (err %d)\n",
			__func__, cal_err);
		return -EIO;
	}

	pr_info("%s: calibration triggered!\n", __func__);
	pr_info("%s: end\n", __func__);

	return 0;
}

enum tfa98xx_error tfa98xx_read_reference_temp(short *value)
{
	struct power_supply *psy = NULL;
	union power_supply_propval prop_read = {0};
	int ret = 0;

	/* get power supply of "battery" */
	/* value is preserved with default when error happens */
	psy = power_supply_get_by_name(REF_TEMP_DEVICE_NAME);
	if (!psy) {
		pr_err("%s: failed to get power supply\n", __func__);
		return TFA98XX_ERROR_FAIL;
	}

	ret = power_supply_get_property(psy,
		POWER_SUPPLY_PROP_TEMP, &prop_read);
	if (ret) {
		pr_err("%s: failed to get temp property\n", __func__);
		if (psy)
			power_supply_put(psy);
		return TFA98XX_ERROR_FAIL;
	}

	*value = (short)(prop_read.intval / 10); /* in degC */
	pr_info("%s: read temp (%d) from %s\n",
		__func__, *value, REF_TEMP_DEVICE_NAME);
	if (psy)
		power_supply_put(psy);

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa98xx_read_reference_temp);

static void tfa98xx_set_dsp_configured(struct tfa98xx *tfa98xx)
{
	int is_configured; /* reset by default */

	is_configured = tfa98xx->tfa->is_configured;

	switch (tfa98xx->dsp_init) {
	case TFA98XX_DSP_INIT_DONE:
	case TFA98XX_DSP_INIT_RECOVER:
		/* set working if already running */
		is_configured = 1;
		break;
	case TFA98XX_DSP_INIT_INVALIDATED:
		/* preserve state except pstream off */
		if (tfa98xx->pstream == 0)
			is_configured = 0;
		break;
	case TFA98XX_DSP_INIT_STOPPED:
		if (tfa98xx->tfa->is_probus_device) {
			/* preserve state except pstream off */
			if (tfa98xx->pstream == 0)
				is_configured = 0;
		} else {
			/* reset for embedded DSP case */
			is_configured = 0;
		}
		break;
	case TFA98XX_DSP_INIT_FAIL:
		is_configured = -1;
		break;
	case TFA98XX_DSP_INIT_PENDING:
	default:
		is_configured = 0;
		break;
	}

	pr_debug("[0x%x] dsp_init %d, is_configured %d\n",
		tfa98xx->i2c->addr,
		tfa98xx->dsp_init, is_configured);

	tfa98xx->tfa->is_configured = is_configured;
}

static int tfa98xx_get_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
	int mixer_profile = kcontrol->private_value;
	int ret = 0;
	int profile;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int vstep = tfa98xx->prof_vsteps[profile];

		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx]
			= tfa_cont_get_max_vstep(tfa98xx->tfa, profile)
			- vstep - 1;
	}
	mutex_unlock(&tfa98xx_mutex);

	return ret;
}

static int tfa98xx_set_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
	int mixer_profile = kcontrol->private_value;
	int profile;
	int err = 0;
	int change = 0;

	if (no_start != 0)
		return 0;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	/* wait until when DSP is ready for initialization */
	if (tfa98xx->pstream == 0 && tfa98xx->samstream == 0) {
		pr_info("%s: tfa_start is suspended unless p/samstream is on\n",
			__func__);
		return 0;
	}

	mutex_lock(&tfa98xx_mutex);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int vstep, vsteps;
		int ready = 0;
		int new_vstep;
		int value = ucontrol->value
			.integer.value[tfa98xx->tfa->dev_idx];

		vstep = tfa98xx->prof_vsteps[profile];
		vsteps = tfa_cont_get_max_vstep(tfa98xx->tfa, profile);

		if (vstep == vsteps - value - 1)
			continue;

		new_vstep = vsteps - value - 1;

		if (new_vstep < 0)
			new_vstep = 0;

		tfa98xx->prof_vsteps[profile] = new_vstep;

		if (profile == tfa98xx->profile) {
			/* this is the active profile, program the new vstep */
			tfa98xx->vstep = new_vstep;
			mutex_lock(&tfa98xx->dsp_lock);
			tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);

			if (ready) {
				err = tfa98xx_tfa_start(tfa98xx,
					tfa98xx->profile, tfa98xx->vstep);
				if (err) {
					pr_err("Write vstep error: %d\n", err);
				} else {
					pr_debug("Successfully changed vstep index!\n");
					change = 1;
				}
			}

			/* Set DSP status as profile change may invalidate
			 * current DSP configuration. Next stream start can
			 * trigger a tfa_dev_start.
			 */
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_INVALIDATED;
			tfa98xx_set_dsp_configured(tfa98xx);
			mutex_unlock(&tfa98xx->dsp_lock);
		}
		pr_debug("%d: vstep:%d, (control value: %d) - profile %d\n",
			tfa98xx->tfa->dev_idx, new_vstep, value, profile);
	}

	if (!change) {
		mutex_unlock(&tfa98xx_mutex);
		return change;
	}

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
		if (tfa98xx->tfa->spkgain != -1) {
			pr_info("%s: set speaker gain 0x%x\n",
				__func__, tfa98xx->tfa->spkgain);
			TFA7x_SET_BF(tfa98xx->tfa, TDMSPKG,
				tfa98xx->tfa->spkgain);
		}

		pr_info("%s: UNMUTE dev %d\n",
			__func__, tfa98xx->tfa->dev_idx);
		tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	mutex_unlock(&tfa98xx_mutex);

	return change;
}

static int tfa98xx_info_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
	int mixer_profile = tfa98xx_mixer_profile;
	int profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);

	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max
		= max(0, tfa_cont_get_max_vstep(tfa98xx->tfa, profile) - 1);
	pr_debug("vsteps count: %d [prof=%d]\n",
		tfa_cont_get_max_vstep(tfa98xx->tfa, profile),
		profile);
	return 0;
}

static int tfa98xx_get_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&tfa98xx_mutex);
	ucontrol->value.integer.value[0] = tfa98xx_mixer_profile;
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component
		= snd_soc_kcontrol_component(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
	int err = 0;
	int change = 0;
	int new_profile;
	int prof_idx, cur_prof_idx;
	int profile_count = tfa98xx_mixer_profiles;
	int profile = tfa98xx_mixer_profile;

	if (no_start != 0)
		return 0;

	new_profile = ucontrol->value.integer.value[0];
	if (new_profile == profile)
		return 0;

	if ((new_profile < 0) || (new_profile >= profile_count)) {
		pr_err("not existing profile (%d)\n", new_profile);
		return -EINVAL;
	}

	/* get the container profile for the requested sample rate */
	prof_idx = get_profile_id_for_sr(new_profile, tfa98xx->rate);
	cur_prof_idx = get_profile_id_for_sr(profile, tfa98xx->rate);
	if (prof_idx < 0 || cur_prof_idx < 0) {
		pr_err("%s: sample rate [%d] not supported for this mixer profile [%d -> %d]\n",
			__func__, tfa98xx->rate, profile, new_profile);
		return 0;
	}
	pr_info("%s: selected container profile [%d -> %d]\n",
		__func__, cur_prof_idx, prof_idx);
	pr_debug("%s: switch profile [%s -> %s]\n", __func__,
		_tfa_cont_profile_name(tfa98xx, cur_prof_idx),
		_tfa_cont_profile_name(tfa98xx, prof_idx));

	/* update mixer profile */
	tfa98xx_mixer_profile = new_profile;

	/* wait until when DSP is ready for initialization */
	if (tfa98xx->pstream == 0 && tfa98xx->samstream == 0) {
		pr_info("%s: tfa_start is suspended unless p/samstream is on\n",
			__func__);
		return 0;
	}

	mutex_lock(&tfa98xx_mutex);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int ready = 0;

		/* update 'real' profile (container profile) */
		tfa98xx->profile = prof_idx;
		tfa98xx->vstep = tfa98xx->prof_vsteps[prof_idx];

		mutex_lock(&tfa98xx->dsp_lock);
		/* Set ready by force, for selective channel control */
		ready = 1;
		if (ready) {
			/* Also re-enables the interrupts */
			pr_info("%s: trigger [dev %d - prof %d]\n", __func__,
				tfa98xx->tfa->dev_idx, prof_idx);
			err = tfa98xx_tfa_start(tfa98xx,
				prof_idx, tfa98xx->vstep);
			if (err) {
				pr_info("Write profile error: %d\n", err);
			} else {
				pr_debug("Changed to profile %d (vstep = %d)\n",
					prof_idx, tfa98xx->vstep);
				change = 1;
			}
		}

		/* Set DSP status as profile change may invalidate
		 * current DSP configuration. Next stream start can
		 * trigger a tfa_dev_start.
		 */
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_INVALIDATED;
		tfa98xx_set_dsp_configured(tfa98xx);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	if (!change) {
		mutex_unlock(&tfa98xx_mutex);
		return change;
	}

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
		if (tfa98xx->tfa->spkgain != -1) {
			pr_info("%s: set speaker gain 0x%x\n",
				__func__, tfa98xx->tfa->spkgain);
			TFA7x_SET_BF(tfa98xx->tfa, TDMSPKG,
				tfa98xx->tfa->spkgain);
		}

		pr_info("%s: UNMUTE dev %d\n",
			__func__, tfa98xx->tfa->dev_idx);
		tfa_dev_set_state(tfa98xx->tfa, TFA_STATE_UNMUTE, 0);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	mutex_unlock(&tfa98xx_mutex);

	return change;
}

/* copies the profile basename (i.e. part until .) into buf */
static void get_profile_basename(char *buf, char *profile)
{
	int cp_len = 0, idx = 0;
	char *pch;

	pch = strnchr(profile, strlen(profile), '.');
	idx = pch - profile;
	cp_len = (pch != NULL) ? idx : (int)strlen(profile);
	memcpy(buf, profile, cp_len);
	buf[cp_len] = 0;
}

/* return the profile name accociated with id from the profile list */
static int get_profile_from_list(char *buf, int id)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (bprof->item_id == id) {
			strlcpy(buf, bprof->basename, MAX_CONTROL_NAME);
			return 0;
		}
	}

	return TFA_ERROR;
}

/* search for the profile in the profile list */
static int is_profile_in_list(char *profile, int len)
{
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len)
			&& (strncmp(bprof->basename, profile, len) == 0))
			return 1;
	}

	return 0;
}

/*
 * for the profile with id, look if the requested samplerate is
 * supported, if found return the (container)profile for this
 * samplerate, on error or if not found return -1
 */
static int get_profile_id_for_sr(int id, unsigned int rate)
{
	int idx = 0;
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (id == bprof->item_id) {
			idx = tfa98xx_get_fssel(rate);
			if (idx < 0) {
				/* samplerate not supported */
				return TFA_ERROR;
			}

			return bprof->sr_rate_sup[idx];
		}
	}

	/* profile not found */
	return TFA_ERROR;
}

/* check if this profile is a calibration profile */
static int is_calibration_profile(char *profile)
{
	if (strnstr(profile, ".cal", strlen(profile)) != NULL)
		return 1;
	return 0;
}

/*
 * adds the (container)profile index of the samplerate found in
 * the (container)profile to a fixed samplerate table in the (mixer)profile
 */
static int add_sr_to_profile(struct tfa98xx *tfa98xx,
	char *basename, int len, int profile)
{
	struct tfa98xx_baseprofile *bprof;
	int idx = 0;
	unsigned int sr = 0;
	unsigned int sr0 = 0xff;
	static int sr_hit;

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len)
			&& (strncmp(bprof->basename, basename, len) == 0)) {
			/* add supported samplerate for this profile */
			sr = tfa98xx_get_profile_sr(tfa98xx->tfa, profile);
			if (!sr) {
				pr_err("unable to identify supported sample rate for %s\n",
					bprof->basename);
				return TFA_ERROR;
			}
			sr0 = (sr0 == 0xff) ? sr : sr0; /* default rate */
			if (sr_converted == sr) {
				pr_debug("sr_converted: hits (%d)!\n",
					sr_converted);
				sr_hit = 1;
			}

			/* get the index for this samplerate */
			idx = tfa98xx_get_fssel(sr);
			if (idx < 0 || idx >= TFA98XX_NUM_RATES) {
				pr_err("invalid index for samplerate %d\n",
					idx);
				return TFA_ERROR;
			}

			/* enter the (container)profile for this samplerate
			 * at the corresponding index
			 */
			bprof->sr_rate_sup[idx] = profile;

			pr_debug("added profile:samplerate = [%d:%d] for mixer profile: %s\n",
				profile, sr, bprof->basename);
		}
	}

	if (!sr_hit && sr0 != 0xff) {
		pr_info("sr_converted: use %d, as %d does not exist\n",
			sr0, sr_converted);
		sr_converted = sr0;
	}

	return 0;
}

static int tfa98xx_info_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	char profile_name[MAX_CONTROL_NAME] = {0};
	int count = tfa98xx_mixer_profiles, err = -1;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	err = get_profile_from_list(profile_name,
		uinfo->value.enumerated.item);
	if (err != 0)
		return -EINVAL;

	strlcpy(uinfo->value.enumerated.name,
		profile_name, MAX_CONTROL_NAME);

	return 0;
}

static int tfa98xx_info_device_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_device_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		ucontrol->value.integer.value[tfa->dev_idx] = tfa->set_active;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_device_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	enum tfa_error err;
	int dev;
	int request;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		request = ucontrol->value.integer.value[dev];

		pr_info("%s: [%d] set active %d\n",
			__func__, dev, request);
		tfa->set_active = request; /* store for the next sessions */
	}

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;

		/* exit if stream is not ready for immediate action */
		if (tfa98xx->pstream == 0
			&& tfa98xx->samstream == 0) {
			pr_info("%s: [%d] store set active unless p/samstream is on\n",
				__func__, dev);
			continue;
		}

		switch (tfa->set_active) {
		case 0: /* deactivate immediately */
			if (tfa->pause_state == 1) {
				pr_info("%s: [%d] already paused; no need to deactivate\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] deactivate channel\n",
				__func__, dev);
			cancel_delayed_work_sync(&tfa98xx->monitor_work);
			_tfa98xx_stop(tfa98xx);
			break;
		case 1: /* activate immediately */
			if (tfa->pause_state == 0) {
				pr_info("%s: [%d] already resumed; no need to activate\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] activate channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			pr_info("%s: trigger [dev %d - prof %d]\n", __func__,
				dev, tfa98xx->profile);
			err = tfa98xx_tfa_start(tfa98xx,
				tfa98xx->profile, tfa98xx->vstep);
			if (err) {
				pr_info("error in activation: %d\n", err);
				mutex_unlock(&tfa98xx->dsp_lock);
				break;
			}

			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
			tfa98xx_set_dsp_configured(tfa98xx);

			if (tfa->spkgain != -1) {
				pr_info("%s: set speaker gain 0x%x\n",
					__func__, tfa->spkgain);
				TFA7x_SET_BF(tfa, TDMSPKG,
					tfa->spkgain);
			}

			pr_info("%s: UNMUTE dev %d\n",
				__func__, dev);
			tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 0);
			mutex_unlock(&tfa98xx->dsp_lock);
			break;
		default:
			pr_info("%s: [%d] wrong request\n",
					__func__, dev);
			break;
		}
	}

	/* reset counter */
	tfa = tfa98xx_get_tfa_device_from_index(0);
	tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);

	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx] = 0;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		int ready = 0;
		int i = tfa98xx->tfa->dev_idx;

		pr_debug("%d: %ld\n", i, ucontrol->value.integer.value[i]);

		tfa98xx_dsp_system_stable(tfa98xx->tfa, &ready);

		if ((ucontrol->value.integer.value[i] != 0) && ready) {
			cancel_delayed_work_sync(&tfa98xx->monitor_work);
			_tfa98xx_stop(tfa98xx);
		}

		ucontrol->value.integer.value[i] = 0;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_mute_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_mute_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		ucontrol->value.integer.value[tfa->dev_idx] = tfa->mute_state;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_mute_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	int dev;
	int request;
	int cur_mute_state[MAX_HANDLES] = {0};

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		request = ucontrol->value.integer.value[dev];

		pr_info("%s: [%d] set mute %d\n",
			__func__, dev, request);
		cur_mute_state[dev] = tfa->mute_state;
		tfa->mute_state = request;
	}

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;

		/* exit if stream is not ready for initialization */
		if (tfa98xx->pstream == 0
			&& tfa98xx->samstream == 0) {
			pr_info("%s: [%d] only store request (%s), unless p/samstream is on\n",
				__func__, dev,
				(tfa->mute_state == 1) ? "mute" : "unmute");
			continue;
		}

		switch (tfa->mute_state) {
		case 0: /* unmute */
			if (cur_mute_state[dev] == 0) {
				pr_info("%s: [%d] already unmuted, skip the request\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] unmute channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			tfa_run_unmute(tfa);
			mutex_unlock(&tfa98xx->dsp_lock);
			break;
		case 1: /* mute */
			if (cur_mute_state[dev] == 1) {
				pr_info("%s: [%d] already muted, skip the request\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] mute channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			tfa_run_mute(tfa);
			mutex_unlock(&tfa98xx->dsp_lock);
			break;
		default:
			pr_info("%s: [%d] wrong request\n",
					__func__, dev);
			break;
		}
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_pause_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int tfa98xx_get_pause_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		ucontrol->value.integer.value[tfa->dev_idx] = tfa->pause_state;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_pause_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	enum tfa_error err;
	int dev;
	int request;
	int cur_pause_state[MAX_HANDLES] = {0};

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		request = ucontrol->value.integer.value[dev];

		pr_info("%s: [%d] set pause %d\n",
			__func__, dev, request);
		cur_pause_state[dev] = tfa->pause_state;
		tfa->pause_state = request;
	}

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;

		switch (tfa->pause_state) {
		case 0: /* resume */
			if (cur_pause_state[dev] == 0) {
				pr_info("%s: [%d] already resumed, skip the request\n",
					__func__, dev);
				break;
			}
			/* exit if stream is not ready for initialization */
			if (tfa98xx->pstream == 0
				&& tfa98xx->samstream == 0) {
				pr_info("%s: [%d] cannot resume unless p/samstream is on\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] resume channel\n",
				__func__, dev);
			mutex_lock(&tfa98xx->dsp_lock);
			pr_info("%s: trigger [dev %d - prof %d]\n", __func__,
				dev, tfa98xx->profile);
			err = tfa98xx_tfa_start(tfa98xx,
				tfa98xx->profile, tfa98xx->vstep);
			if (err) {
				pr_info("error in activation: %d\n", err);
				mutex_unlock(&tfa98xx->dsp_lock);
				break;
			}

			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
			tfa98xx_set_dsp_configured(tfa98xx);

			if (tfa->spkgain != -1) {
				pr_info("%s: set speaker gain 0x%x\n",
					__func__, tfa->spkgain);
				TFA7x_SET_BF(tfa, TDMSPKG,
					tfa->spkgain);
			}

			pr_info("%s: UNMUTE dev %d\n",
				__func__, dev);
			tfa_dev_set_state(tfa, TFA_STATE_UNMUTE, 0);
			mutex_unlock(&tfa98xx->dsp_lock);
			break;
		case 1: /* pause */
			if (cur_pause_state[dev] == 1) {
				pr_info("%s: [%d] already paused, skip the request\n",
					__func__, dev);
				break;
			}

			pr_info("%s: [%d] pause channel\n",
				__func__, dev);
			cancel_delayed_work_sync(&tfa98xx->monitor_work);
			_tfa98xx_stop(tfa98xx);
			break;
		default:
			pr_info("%s: [%d] wrong request\n",
					__func__, dev);
			break;
		}
	}

	/* reset counter */
	tfa = tfa98xx_get_tfa_device_from_index(0);
	tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);

	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_spkgain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0x0;
	uinfo->value.integer.max = 0xf;

	return 0;
}

static int tfa98xx_get_spkgain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	int spkgain;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		spkgain = tfa->spkgain;
		if (spkgain == -1) {
			spkgain = TFA7x_GET_BF(tfa, TDMSPKG);
			pr_info("%s: [%d] read current speaker gain 0x%x\n",
				__func__, tfa->dev_idx, spkgain);
		}

		ucontrol->value.integer.value[tfa->dev_idx] = spkgain;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_spkgain(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa = NULL;
	enum tfa98xx_error err;
	int dev;
	int cur_spkgain;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		dev = tfa->dev_idx;
		tfa->spkgain = ucontrol->value.integer.value[dev];

		cur_spkgain = TFA7x_GET_BF(tfa, TDMSPKG);
		pr_info("%s: [%d] set spekaer gain 0x%x (currently, 0x%x)\n",
			__func__, dev, tfa->spkgain, cur_spkgain);
		err = TFA7x_SET_BF(tfa, TDMSPKG, tfa->spkgain);
		if (err)
			pr_err("%s: [%d] failed to set speaker gain\n",
				__func__, dev);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_ipcid(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0x0;
	uinfo->value.integer.max = 0xffffffff;

	return 0;
}

static int tfa98xx_get_ipcid(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	uint32_t ipcid_value = 0;

	if (tfa == NULL)
		return 0;

	mutex_lock(&tfa98xx_mutex);
	ipcid_value = (uint32_t)(((tfa->ipcid[0] & 0xffff) << 16)
		| (tfa->ipcid[1] & 0xffff));
	ucontrol->value.integer.value[0] = ipcid_value;
	ucontrol->value.integer.value[1] = tfa->ipcid[2];
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_set_ipcid(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	uint32_t ipcid_value = 0;

	if (tfa == NULL)
		return 1;

	mutex_lock(&tfa98xx_mutex);
	ipcid_value = (uint32_t)ucontrol->value.integer.value[0];
	tfa->ipcid[0] = (ipcid_value >> 16) & 0xffff;
	tfa->ipcid[1] = ipcid_value & 0xffff;
	tfa->ipcid[2] = (uint32_t)ucontrol->value.integer.value[1];

	pr_info("%s: set_ipcid PCM%03d:%d:0x%08x\n",
		__func__, tfa->ipcid[0], tfa->ipcid[1], tfa->ipcid[2]);
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_info_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa98xx_mutex);
	uinfo->count = tfa98xx_device_count;
	mutex_unlock(&tfa98xx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff; /* 16 bit value */

	return 0;
}

static int tfa98xx_set_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		enum tfa_error err;
		int i = tfa98xx->tfa->dev_idx;

		tfa98xx->cal_data = (uint16_t)ucontrol->value.integer.value[i];

		mutex_lock(&tfa98xx->dsp_lock);
		err = tfa98xx_write_re25(tfa98xx->tfa, tfa98xx->cal_data);
		tfa98xx->set_mtp_cal = (err != tfa_error_ok);
		if (tfa98xx->set_mtp_cal == false)
			pr_info("Calibration value (%d) set in mtp\n",
				tfa98xx->cal_data);
		mutex_unlock(&tfa98xx->dsp_lock);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 1;
}

static int tfa98xx_get_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&tfa98xx->dsp_lock);
		ucontrol->value.integer.value[tfa98xx->tfa->dev_idx]
			= tfa_dev_mtp_get(tfa98xx->tfa, TFA_MTP_RE25_PRIM);
		mutex_unlock(&tfa98xx->dsp_lock);
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_info_cnt_reload(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0x0;
	uinfo->value.integer.max = 0xffffffff;

	return 0;
}

static int tfa98xx_get_cnt_reload(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&probe_lock);
	ucontrol->value.integer.value[0] = tfa98xx_cnt_reload;
	mutex_unlock(&probe_lock);

	return 0;
}

static int tfa98xx_set_cnt_reload(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tfa98xx *tfa98xx;
	int tries, ret;

	if (ucontrol != NULL)
		if (ucontrol->value.integer.value[0] == 0)
			return 1; /* do nothing */

	/* free previously loaded one */
	mutex_lock(&tfa98xx_mutex);
	if (tfa98xx_container) {
		kfree(tfa98xx_container);
		tfa98xx_container = NULL;
	}
	mutex_unlock(&tfa98xx_mutex);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		mutex_lock(&probe_lock);
		if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK) {
			pr_info("%s: Reload continer file (previously %d) - dev %d\n",
				__func__, tfa98xx_cnt_reload,
				tfa98xx->tfa->dev_idx);
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_RELOADING;
		} else {
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_PENDING;
		}
		mutex_unlock(&probe_lock);

		tries = 0;

		do {
			ret = request_firmware_nowait(THIS_MODULE,
				FW_ACTION_UEVENT,
				fw_name, tfa98xx->dev, GFP_KERNEL,
				tfa98xx, tfa98xx_container_loaded);
			/* wait until driver completes loading */
			msleep_interruptible(20);
			if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK)
				break;

			msleep_interruptible(80);
			tries++;
		} while (tries < TFA98XX_LOADFW_NTRIES
			&& tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK);

		if ((ret != 0)
			|| (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK))
			/* skip preloading if it's not loaded yet */
			continue;

		/* Preload settings using internal clock on TFA2 */
		if (tfa98xx->tfa->tfa_family == 2) {
			mutex_lock(&tfa98xx->dsp_lock);
			/* reload by force */
			tfa98xx->tfa->first_after_boot = 1;
			tfa98xx_set_stream_state(tfa98xx->tfa, 0);
			ret = tfa98xx_tfa_start(tfa98xx,
				tfa98xx->profile, tfa98xx->vstep);
			if (ret == TFA98XX_ERROR_NOT_SUPPORTED) {
				tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
			} else {
				/* reset cold amp state */
				if (tfa98xx->tfa->tfa_family == 2)
					TFA_SET_BF(tfa98xx->tfa, MANSCONF, 1);
			}
			tfa_set_status_flag(tfa98xx->tfa, TFA_SET_DEVICE, 0);
			mutex_unlock(&tfa98xx->dsp_lock);
		}
	}

	return 1;
}

static int tfa98xx_create_controls(struct tfa98xx *tfa98xx)
{
	int prof, nprof, mix_index = 0;
	int nr_controls = 0, id = 0;
	char *name;
	struct tfa98xx_baseprofile *bprofile;
	struct device *cdev;
	int ret;
	static int is_control_created;

	if (is_control_created) {
		pr_info("%s: Already created\n", __func__);
		return 0;
	}

	cdev = tfa98xx->component->dev;

	/* Create the following controls:
	 * - enum control to select the active profile
	 * - one volume control for each profile hosting a vstep
	 * - Stop control on TFA1 devices
	 */

	nr_controls = 2; /* Profile and stop control */

	nr_controls += 1; /* set active */
	nr_controls += 1; /* set mute */
	nr_controls += 1; /* set pause */
	nr_controls += 1; /* set speaker gain */
	nr_controls += 1; /* set ipcid */
	nr_controls += 1; /* set cnt reload */

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL)
		nr_controls += 1; /* calibration */

	/* allocate the tfa98xx_controls base on the nr of profiles */
	nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);
	for (prof = 0; prof < nprof; prof++) {
		if (tfa_cont_get_max_vstep(tfa98xx->tfa, prof))
			nr_controls++; /* Playback Volume control */
	}

	tfa98xx_controls = devm_kzalloc(cdev,
		nr_controls * sizeof(tfa98xx_controls[0]), GFP_KERNEL);
	if (!tfa98xx_controls)
		return -ENOMEM;

	/* Create a mixer item for selecting the active profile */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Profile", tfa98xx->fw.name);
	pr_info("%s: Mixer Control Name = %s\n", __func__, name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_profile;
	tfa98xx_controls[mix_index].get = tfa98xx_get_profile;
	tfa98xx_controls[mix_index].put = tfa98xx_set_profile;
	/* tfa98xx_controls[mix_index].private_value = profs; */
	/* save number of profiles */
	mix_index++;

	/* create mixer items for each profile that has volume */
	for (prof = 0; prof < nprof; prof++) {
		/* create an new empty profile */
		bprofile = devm_kzalloc(cdev,
			sizeof(*bprofile), GFP_KERNEL);
		if (!bprofile)
			return -ENOMEM;

		bprofile->len = 0;
		bprofile->item_id = -1;
		INIT_LIST_HEAD(&bprofile->list);

		/* copy profile name into basename until the . */
		get_profile_basename(bprofile->basename,
			_tfa_cont_profile_name(tfa98xx, prof));
		bprofile->len = strlen(bprofile->basename);

		/*
		 * search the profile list for a profile with basename,
		 * if it is not found then
		 * add it to the list and add a new mixer control (with vsteps)
		 * also, if it is a calibration profile,
		 * do not add it to the list
		 */
		if ((is_profile_in_list(bprofile->basename, bprofile->len) == 0)
			&& is_calibration_profile
			(_tfa_cont_profile_name(tfa98xx, prof)) == 0) {
			/* the profile is not present, add it to the list */
			list_add(&bprofile->list, &profile_list);
			bprofile->item_id = id++;

			pr_debug("profile added [%d]: %s\n",
				bprofile->item_id, bprofile->basename);

			if (tfa_cont_get_max_vstep(tfa98xx->tfa, prof)) {
				name = devm_kzalloc(cdev,
					MAX_CONTROL_NAME, GFP_KERNEL);
				if (!name)
					return -ENOMEM;

				scnprintf(name, MAX_CONTROL_NAME,
					"%s %s Playback Volume",
					tfa98xx->fw.name, bprofile->basename);
				tfa98xx_controls[mix_index].name = name;
				tfa98xx_controls[mix_index].iface
					= SNDRV_CTL_ELEM_IFACE_MIXER;
				tfa98xx_controls[mix_index].info
					= tfa98xx_info_vstep;
				tfa98xx_controls[mix_index].get
					= tfa98xx_get_vstep;
				tfa98xx_controls[mix_index].put
					= tfa98xx_set_vstep;
				tfa98xx_controls[mix_index].private_value
					= bprofile->item_id;
				/* save profile index */
				mix_index++;
			}
		}

		/* look for the basename profile in the list of mixer profiles
		 * and add the container profile index
		 * to the supported samplerates of this mixer profile
		 */
		add_sr_to_profile(tfa98xx, bprofile->basename,
			bprofile->len, prof);
	}

	/* set the number of user selectable profiles in the mixer */
	if (id > 0) /* if any profile to be registered */
		tfa98xx_mixer_profiles = id;
	else if (tfa98xx_mixer_profiles == 0)
		tfa98xx_mixer_profiles = nprof;

	/* set active device for the following sessions */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Active", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_device_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_device_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_device_ctl;
	mix_index++;

	/* Create a mixer item for stop control on TFA1 */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Stop", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_stop_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_stop_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_stop_ctl;
	mix_index++;

	/* set mute / unmute by force */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Mute", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_mute_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_mute_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_mute_ctl;
	mix_index++;

	/* set pause / resume by force */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Pause", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_pause_ctl;
	tfa98xx_controls[mix_index].get = tfa98xx_get_pause_ctl;
	tfa98xx_controls[mix_index].put = tfa98xx_set_pause_ctl;
	mix_index++;

	/* set speaker gain by force */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Gain", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_spkgain;
	tfa98xx_controls[mix_index].get = tfa98xx_get_spkgain;
	tfa98xx_controls[mix_index].put = tfa98xx_set_spkgain;
	mix_index++;

	/* get connection for IPC (PCM_ID:VMIXER_CARD:INSTANCE_ID) */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s IPC_ID", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_ipcid;
	tfa98xx_controls[mix_index].get = tfa98xx_get_ipcid;
	tfa98xx_controls[mix_index].put = tfa98xx_set_ipcid;
	mix_index++;

	/* reload container file to memory */
	name = devm_kzalloc(cdev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	scnprintf(name, MAX_CONTROL_NAME, "%s Reload", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_cnt_reload;
	tfa98xx_controls[mix_index].get = tfa98xx_get_cnt_reload;
	tfa98xx_controls[mix_index].put = tfa98xx_set_cnt_reload;
	mix_index++;

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL) {
		name = devm_kzalloc(cdev,
			MAX_CONTROL_NAME, GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		scnprintf(name, MAX_CONTROL_NAME,
			"%s Calibration", tfa98xx->fw.name);
		tfa98xx_controls[mix_index].name = name;
		tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		tfa98xx_controls[mix_index].info = tfa98xx_info_cal_ctl;
		tfa98xx_controls[mix_index].get = tfa98xx_get_cal_ctl;
		tfa98xx_controls[mix_index].put = tfa98xx_set_cal_ctl;
		mix_index++;
	}

	ret = snd_soc_add_component_controls(tfa98xx->component,
		tfa98xx_controls, mix_index);

	if (!ret)
		is_control_created = 1;

	return ret;
}

static void *tfa98xx_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);

	if (!str)
		return str;
	memcpy(str, buf, strlen(buf));

	return str;
}

static int tfa98xx_append_i2c_address(struct device *dev,
	struct i2c_client *i2c,
	struct snd_soc_dapm_widget *widgets,
	int num_widgets,
	struct snd_soc_dai_driver *dai_drv,
	int num_dai)
{
	char buf[50], name[50] = {0};
	int i;
	int i2cbus = i2c->adapter->nr;
	int addr = i2c->addr;

	if (dai_drv && num_dai > 0)
		for (i = 0; i < num_dai; i++) {
			memcpy(name, dai_drv[i].name,
				strlen(dai_drv[i].name));
			snprintf(buf, 50, "%s-%d-%x", name, i2cbus, addr);
			dai_drv[i].name = tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].name=%s\n", i, dai_drv[i].name);

			memcpy(name, dai_drv[i].playback.stream_name,
				strlen(dai_drv[i].playback.stream_name));
			snprintf(buf, 50, "%s-%d-%x", name, i2cbus, addr);
			dai_drv[i].playback.stream_name
				= tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].playback.stream_name=%s\n",
				i, dai_drv[i].playback.stream_name);

			memcpy(name, dai_drv[i].capture.stream_name,
				strlen(dai_drv[i].capture.stream_name));
			snprintf(buf, 50, "%s-%d-%x", name, i2cbus, addr);
			dai_drv[i].capture.stream_name
				= tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].capture.stream_name=%s\n",
				i, dai_drv[i].capture.stream_name);
		}

	/* the idea behind this is convert:
	 * SND_SOC_DAPM_AIF_IN
	 *  ("AIF IN","AIF Playback",0,SND_SOC_NOPM,0,0),
	 * into:
	 * SND_SOC_DAPM_AIF_IN
	 *  ("AIF IN","AIF Playback-2-36",0,SND_SOC_NOPM,0,0),
	 */
	if (widgets && num_widgets > 0)
		for (i = 0; i < num_widgets; i++) {
			if (!widgets[i].sname)
				continue;
			if ((widgets[i].id == snd_soc_dapm_aif_in)
				|| (widgets[i].id == snd_soc_dapm_aif_out)) {
				memcpy(name, widgets[i].sname,
					strlen(widgets[i].sname));
				snprintf(buf, 50, "%s-%d-%x",
					name, i2cbus, addr);
				widgets[i].sname
					= tfa98xx_devm_kstrdup(dev, buf);
				pr_info("widgets[%d].sname=%s\n",
					i, widgets[i].sname);
			}
		}

	return 0;
}

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_common[] = {
	/* Stream widgets */
	SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_INPUT("AEC Loopback"),
};

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_stereo[] = {
	SND_SOC_DAPM_OUTPUT("OUTR"),
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_common[] = {
	{"OUTL", NULL, "AIF IN"},
	{"AIF OUT", NULL, "AEC Loopback"},
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_stereo[] = {
	{"OUTR", NULL, "AIF IN"},
};

static void tfa98xx_add_widgets(struct tfa98xx *tfa98xx)
{
	struct snd_soc_dapm_context *dapm
		= snd_soc_component_get_dapm(tfa98xx->component);
	unsigned int num_dapm_widgets
		= ARRAY_SIZE(tfa98xx_dapm_widgets_common);
	struct snd_soc_dapm_widget *widgets;

	widgets = devm_kzalloc(tfa98xx->dev,
		sizeof(struct snd_soc_dapm_widget)
		* ARRAY_SIZE(tfa98xx_dapm_widgets_common),
		GFP_KERNEL);
	if (!widgets)
		return;
	memcpy(widgets, tfa98xx_dapm_widgets_common,
		sizeof(struct snd_soc_dapm_widget)
		* ARRAY_SIZE(tfa98xx_dapm_widgets_common));

	tfa98xx_append_i2c_address(tfa98xx->dev,
		tfa98xx->i2c,
		widgets,
		num_dapm_widgets,
		NULL,
		0);

	snd_soc_dapm_new_controls(dapm, widgets,
		ARRAY_SIZE(tfa98xx_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, tfa98xx_dapm_routes_common,
		ARRAY_SIZE(tfa98xx_dapm_routes_common));

	snd_soc_dapm_ignore_suspend(dapm, "AIF IN");
	snd_soc_dapm_ignore_suspend(dapm, "OUTL");
	snd_soc_dapm_ignore_suspend(dapm, "AIF OUT");
	snd_soc_dapm_ignore_suspend(dapm, "AEC Loopback");

	if (tfa98xx->flags & TFA98XX_FLAG_STEREO_DEVICE) {
		snd_soc_dapm_new_controls
			(dapm, tfa98xx_dapm_widgets_stereo,
			ARRAY_SIZE(tfa98xx_dapm_widgets_stereo));
		snd_soc_dapm_add_routes
			(dapm, tfa98xx_dapm_routes_stereo,
			ARRAY_SIZE(tfa98xx_dapm_routes_stereo));

		snd_soc_dapm_ignore_suspend(dapm, "OUTR");
	}
}

/* I2C wrapper functions */
enum tfa98xx_error tfa98xx_write_register16(struct tfa_device *tfa,
	unsigned char subaddress,
	unsigned short value)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

retry:
	ret = regmap_write(tfa98xx->regmap, subaddress, value);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return TFA98XX_ERROR_FAIL;
	}

	if (tfa98xx_kmsg_regs)
		dev_dbg(tfa98xx->dev,
			"WR reg=0x%02x, val=0x%04x %s\n",
			subaddress, value,
			ret < 0 ? "Error!!" : "");

	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk
			("WR reg=0x%02x, val=0x%04x %s\n",
			subaddress, value,
			ret < 0 ? "Error!!" : "");

	return error;
}

enum tfa98xx_error tfa98xx_read_register16(struct tfa_device *tfa,
	unsigned char subaddress,
	unsigned short *val)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	unsigned int value;
	int retries = I2C_RETRIES;
	int ret;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (!tfa98xx || !tfa98xx->regmap) {
		pr_err("No tfa98xx regmap available\n");
		return TFA98XX_ERROR_BAD_PARAMETER;
	}

retry:
	ret = regmap_read(tfa98xx->regmap, subaddress, &value);
	if (ret < 0) {
		pr_warn("i2c error at subaddress 0x%x, retries left: %d\n",
			subaddress, retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return TFA98XX_ERROR_FAIL;
	}
	*val = value & 0xffff;

	if (tfa98xx_kmsg_regs)
		dev_dbg(tfa98xx->dev,
			"RD reg=0x%02x, val=0x%04x %s\n",
			subaddress, *val,
			ret < 0 ? "Error!!" : "");
	if (tfa98xx_ftrace_regs)
		tfa98xx_trace_printk
			("RD reg=0x%02x, val=0x%04x %s\n",
			subaddress, *val,
			ret < 0 ? "Error!!" : "");

	return error;
}

/*
 * init external dsp
 */
enum tfa98xx_error tfa98xx_init_dsp(struct tfa_device *tfa)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

int tfa98xx_get_dsp_status(struct tfa_device *tfa)
{
	return 0;
}

/*
 * write external dsp message
 */
int tfa98xx_write_dsp(void *tfa,
	int num_bytes, const char *command_buffer)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

/*
 * read external dsp message
 */
int tfa98xx_read_dsp(void *tfa,
	int num_bytes, unsigned char *result_buffer)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}
/*
 * write/read external dsp message
 */
int tfa98xx_writeread_dsp(void *tfa,
	int command_length, void *command_buffer,
	int result_length, void *result_buffer)
{
	return TFA98XX_ERROR_NOT_SUPPORTED;
}

enum tfa98xx_error tfa98xx_read_data(struct tfa_device *tfa,
	unsigned char reg,
	int len, unsigned char value[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	struct i2c_client *tfa98xx_client;
	int err;
	int tries = 0;
	unsigned char *reg_buf = NULL;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.len = 1,
			.buf = NULL,
		}, {
			.flags = I2C_M_RD,
			.len = len,
			.buf = value,
		},
	};

	reg_buf = (unsigned char *)
		kmalloc(sizeof(reg), GFP_DMA); /* GRP_KERNEL also works */
	if (!reg_buf)
		return -ENOMEM;

	*reg_buf = reg;
	msgs[0].buf = reg_buf;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;
	if (tfa98xx->i2c) {
		tfa98xx_client = tfa98xx->i2c;
		msgs[0].addr = tfa98xx_client->addr;
		msgs[1].addr = tfa98xx_client->addr;

		do {
			err = i2c_transfer(tfa98xx_client->adapter, msgs,
				ARRAY_SIZE(msgs));
			if (err != ARRAY_SIZE(msgs))
				msleep_interruptible(I2C_RETRY_DELAY);
		} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

		if (err != ARRAY_SIZE(msgs)) {
			dev_err(&tfa98xx_client->dev,
				"read transfer error %d\n", err);
			error = TFA98XX_ERROR_FAIL;
		}

		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx_client->dev,
				"RD-DAT reg=0x%02x, len=%d\n",
				reg, len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk
				("RD-DAT reg=0x%02x, len=%d\n",
				reg, len);
	} else {
		pr_err("No device available\n");
		error = TFA98XX_ERROR_FAIL;
	}

	kfree(reg_buf);

	return error;
}

enum tfa98xx_error tfa98xx_write_raw(struct tfa_device *tfa,
	int len,
	const unsigned char data[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa == NULL) {
		pr_err("No device available\n");
		return TFA98XX_ERROR_FAIL;
	}

	tfa98xx = (struct tfa98xx *)tfa->data;

retry:
	ret = i2c_master_send(tfa98xx->i2c, data, len);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	if (ret == len) {
		if (tfa98xx_kmsg_regs)
			dev_dbg(tfa98xx->dev,
				"WR-RAW len=%d\n", len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk
				("WR-RAW len=%d\n", len);
		return TFA98XX_ERROR_OK;
	}
	pr_err("WR-RAW (len=%d) Error I2C send size mismatch %d\n",
		len, ret);
	error = TFA98XX_ERROR_FAIL;

	return error;
}

int tfa_ext_register(dsp_send_message_t tfa_send_message,
	dsp_read_message_t tfa_read_message,
	tfa_event_handler_t *tfa_event_handler)
{
	struct tfa98xx *tfa98xx;
	int dirt = 0;

	mutex_lock(&tfa98xx_mutex);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa98xx->tfa->ext_dsp = 1;
		tfa98xx->tfa->is_probus_device = 1;
		tfa98xx->tfa->is_cold = 1;

		if (tfa_send_message != NULL) {
			dirt |= 0x1;
			tfa98xx->tfa->dev_ops.dsp_msg = tfa_send_message;
		}
		if (tfa_read_message != NULL) {
			dirt |= 0x2;
			tfa98xx->tfa->dev_ops.dsp_msg_read = tfa_read_message;
		}
	}

	if (tfa_event_handler != NULL)
		tfa_event_handler
			= (tfa_event_handler_t *)tfa_ext_event_handler;

	if (dirt == 0x3)
		tfa_set_ipc_loaded(1);

	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

/* Interrupts management */
static void tfa98xx_interrupt_enable_tfa2(struct tfa98xx *tfa98xx, bool enable)
{
	tfa_irq_init(tfa98xx->tfa);
}

/* global enable / disable interrupts */
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable)
{
	if (tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)
		return;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_interrupt_enable_tfa2(tfa98xx, enable);
}

/* Firmware management */
static void tfa98xx_container_loaded
	(const struct firmware *cont, void *context)
{
	struct tfa_container *container;
	struct tfa98xx *tfa98xx = context;
	enum tfa_error tfa_err;
	int container_size;
	int ret;
	unsigned int value;

	mutex_lock(&probe_lock);

	if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK) {
		pr_info("%s: Already loaded\n", __func__);
		mutex_unlock(&probe_lock);
		return;
	}

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_RELOADING)
		tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;

	if (!cont) {
		pr_err("Failed to read %s\n", fw_name);
		tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
		mutex_unlock(&probe_lock);
		return;
	}

	pr_debug("loaded %s - size: %zu\n", fw_name, cont->size);

	mutex_lock(&tfa98xx_mutex);
	if (tfa98xx_container == NULL) {
		container = kzalloc(cont->size, GFP_KERNEL);
		if (container == NULL) {
			mutex_unlock(&tfa98xx_mutex);
			release_firmware(cont);
			pr_err("Error allocating memory\n");
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
			mutex_unlock(&probe_lock);
			return;
		}

		container_size = cont->size;
		memcpy(container, cont->data, container_size);
		release_firmware(cont);

		pr_debug("%.2s%.2s\n", container->version,
			container->subversion);
		pr_debug("%.8s\n", container->customer);
		pr_debug("%.8s\n", container->application);
		pr_debug("%.8s\n", container->type);
		pr_debug("%d ndev\n", container->ndev);
		pr_debug("%d nprof\n", container->nprof);

		tfa_err = tfa_load_cnt(container, container_size);
		if (tfa_err != tfa_error_ok) {
			mutex_unlock(&tfa98xx_mutex);
			kfree(container);
			dev_err(tfa98xx->dev, "Cannot load container file, aborting\n");
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
			mutex_unlock(&probe_lock);
			return;
		}

		tfa98xx_container = container;
	} else {
		pr_debug("container file already loaded...\n");
		container = tfa98xx_container;
		release_firmware(cont);
	}
	mutex_unlock(&tfa98xx_mutex);

	tfa98xx->tfa->cnt = container;

	if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_RELOADING) {
		if (tfa98xx->tfa->dev_idx == 0)
			tfa98xx_cnt_reload++; /* increase reload counter */
		pr_info("%s: Reloaded (%d) - dev %d\n",
			__func__, tfa98xx_cnt_reload, tfa98xx->tfa->dev_idx);
		tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_OK;
		mutex_unlock(&probe_lock);
		return;
	}

	/*
	 * i2c transaction limited to 64k
	 * (Documentation/i2c/writing-clients)
	 */
	tfa98xx->tfa->buffer_size = 65536;

	/* DSP messages via i2c */
	tfa98xx->tfa->has_msg = 0;

	if (tfa_dev_probe(tfa98xx->i2c->addr, tfa98xx->tfa) != 0) {
		dev_err(tfa98xx->dev,
			"Failed to probe TFA98xx @ 0x%.2x\n",
			tfa98xx->i2c->addr);
		mutex_unlock(&probe_lock);
		return;
	}

/* TEMPORARY, until TFA device is probed before tfa_ext is called */
	if (tfa98xx->tfa->is_probus_device) {
		/* Q_PLATFORM: IPC ON PAL TO COMMUNICATE BETWEEN HAL AND ADSP */
		tfa98xx->tfa->dev_ops.dsp_msg
			= (dsp_send_message_t)NULL;
		tfa98xx->tfa->dev_ops.dsp_msg_read
			= (dsp_read_message_t)NULL;
		tfa_set_ipc_loaded(1);
	} else {
		/* DSP solution: non-probus */
		tfa98xx->tfa->dev_ops.dsp_msg
			= (dsp_send_message_t)tfa_dsp_msg_rpc;
		tfa98xx->tfa->dev_ops.dsp_msg_read
			= (dsp_read_message_t)tfa_dsp_msg_read_rpc;
		tfa_set_ipc_loaded(1);
	}

	/* Enable debug traces */
	tfa98xx->tfa->verbose = trace_level & 1;

	/* prefix is the application name from the cnt */
	tfa_cont_get_app_name(tfa98xx->tfa, tfa98xx->fw.name);

	/* set default profile/vstep */
	tfa98xx->profile = 0;
	tfa98xx->vstep = 0;

	/* Override default profile if requested */
	if (strcmp(dflt_prof_name, "")) {
		unsigned int i;
		int nprof = tfa_cnt_get_dev_nprof(tfa98xx->tfa);

		for (i = 0; i < nprof; i++) {
			if (strcmp(_tfa_cont_profile_name(tfa98xx, i),
				dflt_prof_name) == 0) {
				tfa98xx->profile = i;
				dev_info(tfa98xx->dev,
					"changing default profile to %s (%d)\n",
					dflt_prof_name, tfa98xx->profile);
				break;
			}
		}
		if (i >= nprof)
			dev_info(tfa98xx->dev,
				"Default profile override failed (%s profile not found)\n",
				dflt_prof_name);
	}

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_OK;

	/* snd_soc_component_read32, not supported later than 5.8 */
	value = snd_soc_component_read(tfa98xx->component,
		TFA98XX_KEY2_PROTECTED_MTP0);
	if (value != -1) {
		tfa98xx->calibrate_done =
			(value & TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK) ? 1 : 0;
		pr_info("[0x%x] calibrate_done = MTPEX (%d) 0x%04x\n",
			tfa98xx->i2c->addr, tfa98xx->calibrate_done, value);
	} else {
		pr_info("[0x%x] error in reading MTPEX\n", tfa98xx->i2c->addr);
		tfa98xx->calibrate_done = 0;
	}

	pr_debug("Firmware init complete\n");

	/* allocate buffer_pool */
	if (tfa98xx->tfa->dev_idx == 0) {
		int index = 0;

		pr_info("Allocate buffer_pool\n");
		for (index = 0; index < POOL_MAX_INDEX; index++)
			tfa_buffer_pool(tfa98xx->tfa, index,
				buf_pool_size[index], POOL_ALLOC);
	}

	if (no_start != 0) {
		mutex_unlock(&probe_lock);
		return;
	}

	/* Only controls for main device */
	/* for the first device */
	if (tfa98xx->tfa->dev_idx == 0)
		tfa98xx_create_controls(tfa98xx);

	if (tfa_is_cold(tfa98xx->tfa) == 0) {
		pr_debug("Warning: device 0x%.2x is still warm\n",
			tfa98xx->i2c->addr);
		tfa_reset(tfa98xx->tfa);
	}

	/* Preload settings using internal clock on TFA2 */
	if (tfa98xx->tfa->tfa_family == 2) {
		mutex_lock(&tfa98xx->dsp_lock);
		tfa98xx_set_stream_state(tfa98xx->tfa, 0);
		ret = tfa98xx_tfa_start(tfa98xx,
			tfa98xx->profile, tfa98xx->vstep);
		if (ret == TFA98XX_ERROR_NOT_SUPPORTED) {
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
		} else {
			/* reset cold amp state */
			if (tfa98xx->tfa->tfa_family == 2)
				TFA_SET_BF(tfa98xx->tfa, MANSCONF, 1);
		}
		tfa_set_status_flag(tfa98xx->tfa, TFA_SET_DEVICE, 0);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	if (!tfa98xx->calibrate_done) {
		/* set MTP_EX and MTP_RE25 by force */
		tfa98xx->set_mtp_cal = true;
		tfa98xx->cal_data = DUMMY_CALIBRATION_DATA;
	}

	tfa98xx_interrupt_enable(tfa98xx, true);

	mutex_unlock(&probe_lock);
}

static int tfa98xx_load_container(struct tfa98xx *tfa98xx)
{
	int tries = 0, ret;

	mutex_lock(&probe_lock);
	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_PENDING;
	mutex_unlock(&probe_lock);

	do {
		ret = request_firmware_nowait(THIS_MODULE,
			FW_ACTION_UEVENT,
			fw_name, tfa98xx->dev, GFP_KERNEL,
			tfa98xx, tfa98xx_container_loaded);
		/* wait until driver completes loading */
		msleep_interruptible(20);
		if (tfa98xx->dsp_fw_state == TFA98XX_DSP_FW_OK)
			break;

		msleep_interruptible(80);
		tries++;
	} while (tries < TFA98XX_LOADFW_NTRIES
		&& tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK);

	return ret;
}

static void tfa98xx_monitor(struct work_struct *work)
{
	struct tfa98xx *tfa98xx;
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	int handle = -1, is_active = 0;
	unsigned int val;
	int ret;

	mutex_lock(&probe_lock);

	tfa98xx = container_of(work, struct tfa98xx, monitor_work.work);

	pr_info("%s: [%d] - profile = %d: %s\n", __func__,
		tfa98xx->tfa->dev_idx, tfa98xx->profile,
		_tfa_cont_profile_name(tfa98xx, tfa98xx->profile));

	if (tfa98xx->tfa->active_count == -1)
		tfa_set_active_handle(tfa98xx->tfa, tfa98xx->profile);

	is_active = tfa_is_active_device(tfa98xx->tfa);
	if (is_active) {
		handle = tfa98xx->tfa->dev_idx;
		pr_info("%s: profile = %d, active handle [%s]: %d\n",
			__func__, tfa98xx->profile,
			tfa_cont_device_name(tfa98xx->tfa->cnt, handle),
			tfa98xx->tfa->active_handle);
	} else {
		goto tfa_monitor_exit;
	}

	/* Check for tap-detection - bypass monitor if it is active */
	if (tfa98xx->input)
		goto tfa_monitor_exit;

	mutex_lock(&tfa98xx->dsp_lock);
	error = tfa7x_status(tfa98xx->tfa);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (error == TFA98XX_ERROR_DSP_NOT_RUNNING) {
		if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_RECOVER;
			tfa98xx_set_dsp_configured(tfa98xx);
			pr_info("%s: dsp_init (direct) with device %d, profile %d\n",
				__func__,
				tfa98xx->tfa->dev_idx,
				tfa98xx->profile);
			tfa98xx_dsp_init(tfa98xx);
		}
	}

	/* for debugging */
	mutex_lock(&tfa98xx->dsp_lock);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_SYS_CONTROL0, &val);
	if (!ret)
		pr_debug("[%d] SYS_CONTROL0: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_SYS_CONTROL1, &val);
	if (!ret)
		pr_debug("[%d] SYS_CONTROL1: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_SYS_CONTROL2, &val);
	if (!ret)
		pr_debug("[%d] SYS_CONTROL2: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_CLOCK_CONTROL, &val);
	if (!ret)
		pr_debug("[%d] CLOCK_CONTROL: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS0, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG0: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS1, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG1: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS3, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG3: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_STATUS_FLAGS4, &val);
	if (!ret)
		pr_debug("[%d] STATUS_FLAG4: 0x%04x\n", handle, val);
	ret = regmap_read(tfa98xx->regmap, TFA98XX_TDM_CONFIG0, &val);
	if (!ret)
		pr_debug("[%d] TDM_CONFIG0: 0x%04x\n", handle, val);
	mutex_unlock(&tfa98xx->dsp_lock);

tfa_monitor_exit:
	pr_info("%s: exit\n", __func__);

	mutex_unlock(&probe_lock);

	if (!tfa98xx->tfa->verbose)
		return;

	if (tfa98xx_monitor_count != -1)
		if (++tfa98xx_monitor_count > MONITOR_COUNT_MAX)
			return;

	/* reschedule */
	queue_delayed_work(tfa98xx->tfa98xx_wq,
		&tfa98xx->monitor_work, 5 * HZ);
}

static void tfa98xx_dsp_init(struct tfa98xx *tfa98xx)
{
	int ret;
	static bool failed;
	bool reschedule = false;
	bool sync = false;
	bool do_sync;
	int active_device_count = tfa98xx_device_count;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		pr_debug("Skipping tfa_dev_start (no FW: %d)\n",
			tfa98xx->dsp_fw_state);
		return;
	}

	if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
		pr_debug("Stream already started, skipping DSP power-on\n");
		return;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	pr_info("%s: ...\n", __func__);

	tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;

	/* directly try to start DSP */
	ret = tfa98xx_tfa_start(tfa98xx,
		tfa98xx->profile, tfa98xx->vstep);
	if (ret == TFA98XX_ERROR_NOT_SUPPORTED) {
		tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
		dev_err(tfa98xx->dev, "Failed in starting device\n");
		failed = true;
	} else if (ret != TFA98XX_ERROR_OK) {
		dev_err(tfa98xx->dev,
			"Failed in starting device (err %d; count %d)\n",
			ret, tfa98xx->init_count);
		failed = true;
		sync = true; /* unmute by force, even if it fails */
		tfa98xx->init_count = 0;
	} else {
		sync = true;
		failed = false;

		/* Subsystem ready, tfa init complete */
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
		dev_dbg(tfa98xx->dev,
			"tfa_dev_start succeeded! (%d)\n",
			tfa98xx->init_count);
		tfa98xx->init_count = 0;
	}

	mutex_unlock(&tfa98xx->dsp_lock);

	if (reschedule) {
		struct tfa98xx *ntfa98xx;

		failed = false;

		/* reschedule this init work for later */
		list_for_each_entry(ntfa98xx, &tfa98xx_device_list, list) {
			ntfa98xx->init_count++;
			pr_info("%s: dsp_init (direct) with device %d, profile %d\n",
				__func__,
				ntfa98xx->tfa->dev_idx,
				ntfa98xx->profile);
			tfa98xx_dsp_init(ntfa98xx);
		}
	}

	if (!sync)
		return;

	if (tfa98xx->tfa->active_count == -1)
		tfa_set_active_handle(tfa98xx->tfa, tfa98xx->profile);

	/* check if all devices have started */
	mutex_lock(&tfa98xx_mutex);
	active_device_count = tfa98xx->tfa->active_count;
	if (tfa98xx_sync_count < active_device_count)
		tfa98xx_sync_count++;

	do_sync = (tfa98xx_sync_count >= active_device_count);

	/* when all devices have started then unmute */
	if (do_sync) {
		tfa98xx_sync_count = 0;

		list_for_each_entry(tfa98xx,
			&tfa98xx_device_list, list) {
			struct tfa_device *ntfa = tfa98xx->tfa;

			mutex_lock(&tfa98xx->dsp_lock);
			if (failed)
				tfa98xx->dsp_init
					= TFA98XX_DSP_INIT_FAIL;
			tfa98xx_set_dsp_configured(tfa98xx);
			mutex_unlock(&tfa98xx->dsp_lock);

			if (!tfa_is_active_device(ntfa))
				continue;

			pr_info("%s: profile = %d, active handle [%s]: %d\n",
				__func__, tfa98xx->profile,
				tfa_cont_device_name(ntfa->cnt,
				ntfa->dev_idx),
				ntfa->active_handle);

			if (failed) {
				tfa_handle_damaged_speakers(ntfa);
				continue;
			}

			mutex_lock(&tfa98xx->dsp_lock);
			if (ntfa->spkgain != -1) {
				pr_info("%s: set speaker gain 0x%x\n",
					__func__, ntfa->spkgain);
				TFA7x_SET_BF(ntfa, TDMSPKG,
					ntfa->spkgain);
			}
			pr_info("%s: UNMUTE dev %d\n",
				__func__, ntfa->dev_idx);
			tfa_dev_set_state(ntfa,
				TFA_STATE_UNMUTE, 0);

			/*
			 * start monitor thread to check IC status bit
			 * periodically, and re-init IC to recover if
			 * needed.
			 */
			tfa98xx_monitor_count = 0;
			queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->monitor_work,
				1 * HZ);
			mutex_unlock(&tfa98xx->dsp_lock);
		}

		failed = false;
	}
	mutex_unlock(&tfa98xx_mutex);
}

static void tfa98xx_interrupt(struct work_struct *work)
{
	struct tfa98xx *tfa98xx0
		= container_of(work, struct tfa98xx, interrupt_work.work);
	struct tfa98xx *tfa98xx;
	struct tfa_device *tfa;
	int irq_gpio = tfa98xx0->irq_gpio;
	int value = 0;

	pr_info("%s: triggered: dev %d\n",
		__func__, tfa98xx0->tfa->dev_idx);

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (tfa98xx->tfa == NULL) {
			pr_debug("[0x%x] device is not available\n",
				tfa98xx->i2c->addr);
			continue;
		}
		tfa = tfa98xx->tfa;
		value = TFA7x_READ_REG(tfa,
			VDDS); /* STATUS_FLAGS0 */
		pr_info("%s: [%d] status flags: 0x%04x\n",
			__func__, tfa->dev_idx, value);

		if (irq_gpio != tfa98xx->irq_gpio)
			continue;

		pr_info("%s: status check on dev %d\n", __func__,
			tfa->dev_idx);

		mutex_lock(&tfa98xx->dsp_lock);
		tfa_irq_report(tfa);
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	/* unmask interrupts masked in IRQ handler */
	tfa_irq_unmask(tfa98xx0->tfa);
}

static int tfa98xx_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
	int idx = 0;
	struct device *cdev;

	cdev = component->dev;

	/*
	 * Support CODEC to CODEC links,
	 * these are called with a NULL runtime pointer.
	 */
	if (!substream->runtime)
		return 0;

	if (pcm_no_constraint != 0)
		return 0;

	pr_info("%s: skip setting format constraint, assuming configurable format\n",
		__func__);

	if (no_start != 0)
		return 0;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		dev_info(cdev, "Container file not loaded\n");
		return -EINVAL;
	}

	tfa98xx->rate_constraint.list = &tfa98xx->rate_constraint_list[0];
	tfa98xx->rate_constraint.count = 0;

	pr_info("%s: add all the rates in the list\n", __func__);
	for (idx = 0; idx < (int)ARRAY_SIZE(rate_to_fssel); idx++) {
		tfa98xx->rate_constraint_list[idx] = rate_to_fssel[idx].rate;
		tfa98xx->rate_constraint.count += 1;
	}

	pr_info("%s: skip setting rate constraint, assuming fixed format\n",
		__func__);

	return 0;
}

static int tfa98xx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(codec_dai->component);

	tfa98xx->sysclk = freq;
	return 0;
}

static int tfa98xx_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int slot_width)
{
	pr_debug("\n");
	return 0;
}

static int tfa98xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(dai->component);
	struct snd_soc_component *component = dai->component;
	struct device *cdev;

	cdev = component->dev;

	pr_debug("fmt=0x%x\n", fmt);

	/* Supported mode: I2S, PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_DSP_A:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK)
			!= SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(cdev, "Invalid Codec main mode\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_PDM:
		break;
	default:
		dev_err(cdev, "Unsupported DAI format %d\n",
			fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	tfa98xx->audio_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	return 0;
}

int tfa98xx_get_fssel(unsigned int rate)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(rate_to_fssel); i++)
		if (rate_to_fssel[i].rate == rate)
			return rate_to_fssel[i].fssel;

	return -EINVAL;
}

static int tfa98xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(component);
	unsigned int rate;
	int prof_idx;
	int sample_size;
	int slot_size;

	/* Supported */
	rate = params_rate(params);
	sample_size = snd_pcm_format_width(params_format(params));
	slot_size = snd_pcm_format_physical_width(params_format(params));
	pr_info("%s: requested rate: %d, sample size: %d, physical size: %d\n",
		__func__, rate, sample_size, slot_size);

	if (no_start != 0)
		return 0;

	pr_info("%s: forced to change rate: %d to %d\n",
		__func__, rate, sr_converted);
	rate = sr_converted;

	/* check if samplerate is supported for this mixer profile */
	prof_idx = get_profile_id_for_sr(tfa98xx_mixer_profile, rate);
	if (prof_idx < 0) {
		pr_err("tfa98xx: invalid sample rate %d.\n", rate);
		return -EINVAL;
	}
	pr_debug("mixer profile:container profile = [%d:%d]\n",
		tfa98xx_mixer_profile, prof_idx);

	/* update 'real' profile (container profile) */
	tfa98xx->profile = prof_idx;

	pr_info("%s: tfa98xx_profile %d\n", __func__, tfa98xx->profile);

	/* update to new rate */
	tfa98xx->rate = rate;

	return 0;
}

static int tfa98xx_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_component *component = dai->component;
	struct tfa98xx *tfa98xx
		= snd_soc_component_get_drvdata(component);

	dev_dbg(tfa98xx->dev, "%s: state: %d (stream = %d)\n",
		__func__, mute, stream);

	if (no_start) {
		pr_debug("no_start parameter set no tfa_dev_start or tfa_dev_stop, returning\n");
		return 0;
	}

	_tfa98xx_mute(tfa98xx, mute, stream);

	return 0;
}

static int _tfa98xx_mute(struct tfa98xx *tfa98xx, int mute, int stream)
{
	if (mute) {
		/* stop DSP only when both playback and capture streams
		 * are deactivated
		 */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (tfa98xx->pstream == 0) {
				pr_debug("mute:%d [pstream duplicated]\n",
					mute);
				return 0;
			}
			tfa98xx->pstream = 0;
		} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (tfa98xx->cstream == 0) {
				pr_debug("mute:%d [cstream duplicated]\n",
					mute);
				return 0;
			}
			tfa98xx->cstream = 0;
		}
		mutex_lock(&tfa98xx->dsp_lock);
		pr_info("mute:%d [pstream %d, cstream %d, samstream %d]\n",
			mute,
			tfa98xx->pstream, tfa98xx->cstream, tfa98xx->samstream);

		tfa98xx_set_stream_state(tfa98xx->tfa,
			(tfa98xx->pstream & BIT_PSTREAM)
			|((tfa98xx->cstream<<1) & BIT_CSTREAM)
			|((tfa98xx->samstream<<2) & BIT_SAMSTREAM));
		mutex_unlock(&tfa98xx->dsp_lock);

		/* case: both p/cstream (either) and samstream are off
		 * if (!(tfa98xx->pstream == 0 || tfa98xx->cstream == 0)
		 *  || (tfa98xx->samstream != 0)) {
		 *  pr_info("mute is suspended until playback/saam are off\n");
		 *  return 0;
		 * }
		 */
		/* wait until both main streams (pstream / samstream) are off */
		if ((tfa98xx->pstream == 0)
			&& (tfa98xx->samstream == 0)) {
			pr_info("mute is triggered\n");
		} else {
			pr_info("mute is suspended when only cstream is off\n");
			return 0;
		}

		mutex_lock(&tfa98xx_mutex);
		tfa98xx_sync_count = 0;
		mutex_unlock(&tfa98xx_mutex);

		cancel_delayed_work_sync(&tfa98xx->monitor_work);
		_tfa98xx_stop(tfa98xx);
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			tfa98xx->pstream = 1;
		else if (stream == SNDRV_PCM_STREAM_CAPTURE)
			tfa98xx->cstream = 1;
		mutex_lock(&tfa98xx->dsp_lock);
		pr_info("mute:%d [pstream %d, cstream %d, samstream %d]\n",
			mute,
			tfa98xx->pstream, tfa98xx->cstream, tfa98xx->samstream);
		tfa98xx_set_stream_state(tfa98xx->tfa,
			(tfa98xx->pstream & BIT_PSTREAM)
			|((tfa98xx->cstream<<1) & BIT_CSTREAM)
			|((tfa98xx->samstream<<2) & BIT_SAMSTREAM));
		mutex_unlock(&tfa98xx->dsp_lock);

		if (tfa98xx->tfa->set_active == 0) {
			pr_info("%s: skip unmuting device %d, if it's forced to set inactive\n",
				__func__, tfa98xx->tfa->dev_idx);
			return 0;
		}

		/* case: either p/cstream (both) or samstream is on
		 * if ((tfa98xx->pstream != 0 && tfa98xx->cstream != 0)
		 *  || tfa98xx->samstream != 0) {
		 */
		/* wait until DSP is ready for initialization */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK
			|| stream == SNDRV_PCM_STREAM_SAAM) {
			pr_info("unmute is triggered\n");
		} else {
			pr_info("unmute is suspended when only cstream is on\n");
			return 0;
		}

		pr_debug("%s: unmute with profile %d\n",
			__func__, tfa98xx->profile);

		/* check index if it needs dsp init only for main device */

		/* Start DSP */
		pr_info("%s: start tfa amp\n", __func__);
		pr_info("%s: dsp_init (direct) with device %d, profile %d\n",
			__func__, tfa98xx->tfa->dev_idx, tfa98xx->profile);
		tfa98xx_dsp_init(tfa98xx);
	}

	return 0;
}

static int _tfa98xx_stop(struct tfa98xx *tfa98xx)
{
	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK)
		return 0;

	mutex_lock(&tfa98xx->dsp_lock);
	tfa_dev_stop(tfa98xx->tfa);
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	tfa98xx_set_dsp_configured(tfa98xx);
	mutex_unlock(&tfa98xx->dsp_lock);

	return 0;
}

static const struct snd_soc_dai_ops tfa98xx_dai_ops = {
	.startup = tfa98xx_startup,
	.set_fmt = tfa98xx_set_fmt,
	.set_sysclk = tfa98xx_set_dai_sysclk,
	.set_tdm_slot = tfa98xx_set_tdm_slot,
	.hw_params = tfa98xx_hw_params,
	.mute_stream = tfa98xx_mute,
};

static struct snd_soc_dai_driver tfa98xx_dai[] = {
	{
		.name = "tfa98xx-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = MAX_HANDLES,
			.rates = TFA98XX_RATES,
			.formats = TFA98XX_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = MAX_HANDLES,
			.rates = TFA98XX_RATES,
			.formats = TFA98XX_FORMATS,
		},
		.ops = &tfa98xx_dai_ops,
		.symmetric_rate = 1,
		.symmetric_channels = 0,
		.symmetric_sample_bits = 0,
	},
};

static int tfa98xx_probe(struct snd_soc_component *component)
{
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);
	int ret = 0;
	struct device *cdev;

	cdev = component->dev;

	pr_debug("%s:\n", __func__);

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa98xx->tfa98xx_wq = create_singlethread_workqueue("tfa98xx");
	if (!tfa98xx->tfa98xx_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&tfa98xx->monitor_work, tfa98xx_monitor);
	INIT_DELAYED_WORK(&tfa98xx->interrupt_work, tfa98xx_interrupt);

	tfa98xx->component = component;

	snd_soc_component_init_regmap(component, tfa98xx->regmap);

	ret = tfa98xx_load_container(tfa98xx);
	pr_debug("Container loading requested: %d\n", ret);

	tfa98xx_add_widgets(tfa98xx);

	dev_info(cdev, "tfa98xx codec registered (%s)",
		tfa98xx->fw.name);

	return ret;
}

static void tfa98xx_remove(struct snd_soc_component *component)
{
	struct tfa98xx *tfa98xx = snd_soc_component_get_drvdata(component);

	pr_debug("%s:\n", __func__);

	if (tfa98xx->tfa == NULL)
		return;

	tfa98xx_interrupt_enable(tfa98xx, false);

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);

	if (tfa98xx->tfa98xx_wq)
		destroy_workqueue(tfa98xx->tfa98xx_wq);

	/* deallocate buffer_pool */
	if (tfa98xx->tfa->dev_idx == 0) {
		int index = 0;

		pr_info("Deallocate buffer_pool\n");
		for (index = 0; index < POOL_MAX_INDEX; index++)
			tfa_buffer_pool(tfa98xx->tfa,
				index, 0, POOL_FREE);
	}
}

static const struct snd_soc_component_driver soc_component_dev_tfa98xx = {
	.probe = tfa98xx_probe,
	.remove = tfa98xx_remove,
};

static bool tfa98xx_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_volatile_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static const struct regmap_config tfa98xx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TFA98XX_MAX_REGISTER,
	.writeable_reg = tfa98xx_writeable_register,
	.readable_reg = tfa98xx_readable_register,
	.volatile_reg = tfa98xx_volatile_register,
	.cache_type = REGCACHE_NONE,
};

static void tfa98xx_irq_tfa2(struct tfa98xx *tfa98xx)
{
	/*
	 * mask interrupts
	 * will be unmasked after handling interrupts in workqueue
	 */
	tfa_irq_mask(tfa98xx->tfa);
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->interrupt_work, 0);
}

static irqreturn_t tfa98xx_irq(int irq, void *data)
{
	struct tfa98xx *tfa98xx = data;

	if (tfa98xx->tfa->tfa_family == 2)
		tfa98xx_irq_tfa2(tfa98xx);

	return IRQ_HANDLED;
}

static int tfa98xx_ext_reset(struct tfa98xx *tfa98xx)
{
	if (tfa98xx && gpio_is_valid(tfa98xx->reset_gpio)) {
		int reset = tfa98xx->reset_polarity;

		gpio_set_value_cansleep((unsigned int)tfa98xx->reset_gpio,
			reset);
		msleep(TFA_RESET_DELAY);
		gpio_set_value_cansleep((unsigned int)tfa98xx->reset_gpio,
			!reset);
		msleep(TFA_RESET_DELAY);
	}

	return 0;
}

static int tfa98xx_parse_dt(struct device *dev,
	struct tfa98xx *tfa98xx, struct device_node *np)
{
	u32 value;
	int ret;

	tfa98xx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (tfa98xx->reset_gpio < 0)
		dev_dbg(dev, "No reset GPIO provided, will not HW reset device\n");

	tfa98xx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (tfa98xx->irq_gpio < 0)
		dev_dbg(dev, "No IRQ GPIO provided.\n");
	else
		dev_info(dev, "IRQ GPIO: %d\n", tfa98xx->irq_gpio);

	ret = of_property_read_u32(np, "reset-polarity", &value);
	if (ret < 0) /* when it fails to read from device tree */
		tfa98xx->reset_polarity = LOW; /* RSTN */
	else
		tfa98xx->reset_polarity = (value == 0) ? LOW : HIGH;

	dev_info(dev, "reset-polarity:%d\n", tfa98xx->reset_polarity);

	return 0;
}

static int tfa98xx_parse_limit_cal_dt(struct device *dev,
	struct tfa98xx *tfa98xx, struct device_node *np)
{
	u32 value;
	int err_lower, err_upper;

	err_lower = of_property_read_u32(np, "lower-limit-cal", &value);
	if ((err_lower < 0)
		|| (value < MIN_CALIBRATION_DATA))
		tfa98xx->tfa->lower_limit_cal = MIN_CALIBRATION_DATA;
	else
		tfa98xx->tfa->lower_limit_cal = value;
	pr_info("[0x%x] lower limit cal : %d\n",
		tfa98xx->i2c->addr, tfa98xx->tfa->lower_limit_cal);

	err_upper = of_property_read_u32(np, "upper-limit-cal", &value);
	if ((err_upper < 0)
		|| (value > MAX_CALIBRATION_DATA))
		tfa98xx->tfa->upper_limit_cal = MAX_CALIBRATION_DATA;
	else
		tfa98xx->tfa->upper_limit_cal = value;
	pr_info("[0x%x] upper limit cal : %d\n",
		tfa98xx->i2c->addr, tfa98xx->tfa->upper_limit_cal);

	return ((err_lower == 0 && err_upper == 0) ? 0 : -1);
}

static int tfa98xx_parse_inchannel_dt(struct device *dev,
	struct tfa98xx *tfa98xx, struct device_node *np)
{
	u32 value;
	int err;

	err = of_property_read_u32(np, "inchannel", &value);
	if (err < 0) {
		tfa98xx->tfa->inchannel = -1; /* to use default INDEX_0/1 */
		return TFA_NOT_FOUND;
	}

	if (value < 0 || value >= MAX_CHANNELS)
		tfa98xx->tfa->inchannel = -1; /* to use default INDEX_0/1 */
	else
		tfa98xx->tfa->inchannel = value;
	pr_info("[0x%x] inchannel : %d\n",
		tfa98xx->i2c->addr, tfa98xx->tfa->inchannel);

	return 0;
}

static ssize_t tfa98xx_reg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	if (count != 1) {
		pr_debug("invalid register address");
		return -EINVAL;
	}

	pr_info("i2c set reg: 0x%x\n", tfa98xx->reg);

	tfa98xx->reg = buf[0];

	return 1;
}

static ssize_t tfa98xx_rw_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	size_t write_count = (count > 2) ? 2 : count; /* 16-bit */
	u8 *data;
	int ret = 0;
	int retries = I2C_RETRIES;

	data = kmalloc(write_count + 1, GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		pr_debug("can not allocate memory\n");
		return ret;
	}

	data[0] = tfa98xx->reg;
	memcpy(&data[1], buf, write_count);

	pr_debug("i2c rw write: 0x%x (offset %d, write_count %d, count %d)\n",
		tfa98xx->reg, (int)off, (int)write_count, (int)count);
retry:
	ret = i2c_master_send(tfa98xx->i2c, data, write_count + 1);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	kfree(data);

	/* the number of data bytes written without the register address */
	return ((ret > 1) ? count : -EIO);
}

static ssize_t tfa98xx_rw_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	size_t read_count = (count > 2) ? 2 : count; /* 16-bit */
	struct i2c_msg msgs[] = {
		{
			.addr = tfa98xx->i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &tfa98xx->reg,
		},
		{
			.addr = tfa98xx->i2c->addr,
			.flags = I2C_M_RD,
			.len = read_count,
			.buf = buf,
		},
	};
	int ret;
	int retries = I2C_RETRIES;

	if (count >= PAGE_SIZE) {
		pr_info("%s: blocked anonymous read!\n", __func__);
		return 0;
	}

	pr_debug("i2c rw read: 0x%x (offset %d, read_count %d, count %d)\n",
		tfa98xx->reg, (int)off, (int)read_count, (int)count);

	memset(buf, 0, count);
retry:
	ret = i2c_transfer(tfa98xx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return ret;
	}

	/* ret contains the number of i2c transaction */
	/* return the number of bytes read */
	return ((ret > 1) ? count : 0);
}

static ssize_t tfa98xx_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	int count = 0;
	int spkgain, ampgain;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	spkgain = TFA7x_GET_BF(tfa98xx->tfa, TDMSPKG);
	ampgain = TFA7x_GET_BF(tfa98xx->tfa, AMPGAIN);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (spkgain < 0 || ampgain < 0) {
		pr_err("[0x%x] Unable to access TDMSPKG / AMPGAIN: (%d, %d)\n",
			tfa98xx->i2c->addr, spkgain, ampgain);
		return -EIO;
	}

	pr_debug("[0x%x] TDMSPKG : %d, AMPGAIN : %d\n",
		tfa98xx->i2c->addr, spkgain, ampgain);
	count = snprintf(buf, PAGE_SIZE, "[0x%02x] TDMSPKG %d, AMAPGAIN %d\n",
		tfa98xx->i2c->addr, spkgain, ampgain);

	return count;
}

static ssize_t tfa98xx_gain_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	/* to prevent attack to tfa98xx_gain_store */
	return count;
}

static ssize_t tfa98xx_autocal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int count = 0;

	tfa = tfa98xx->tfa;
	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	pr_debug("[0x%x] autocal : %s\n",
		tfa98xx->i2c->addr,
		(tfa->disable_auto_cal) ? "disabled" : "enabled");
	count = snprintf(buf, PAGE_SIZE, "%s\n",
		(tfa->disable_auto_cal) ? "0 (disabled)" : "1 (enabled)");

	return count;
}

static ssize_t tfa98xx_autocal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int enable = 0;

	if (tfa98xx->tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count < 1)
		return -EINVAL;

	if (!strncmp(buf, "1", 1))
		enable = 1;
	else if (!strncmp(buf, "0", 1))
		enable = 0;
	else {
		pr_info("%s: autocal is triggered with %s!\n", __func__, buf);
		return -EINVAL;
	}

	pr_info("%s: autocal < %d\n", __func__, enable);

	mutex_lock(&tfa98xx_mutex);
	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		tfa = tfa98xx->tfa;
		if (tfa == NULL)
			continue;

		tfa->disable_auto_cal = (enable) ? 0 : 1;
	}
	mutex_unlock(&tfa98xx_mutex);

	return count;
}

static ssize_t tfa98xx_reinit_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int count = 0, init_requests = -1;

	tfa = tfa98xx->tfa;
	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	init_requests = tfa98xx_cnt_reload;

	pr_debug("[0x%x] reinit : counter %d\n",
		tfa98xx->i2c->addr, init_requests);
	count = snprintf(buf, PAGE_SIZE, "reinit requested: %d\n",
		init_requests);

	return count;
}

static ssize_t tfa98xx_reinit_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int reinit = 0;

	tfa = tfa98xx->tfa;
	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count < 1)
		return -EINVAL;

	if (!strncmp(buf, "1", 1))
		reinit = 1;
	else if (!strncmp(buf, "0", 1))
		reinit = 0;
	else {
		pr_info("%s: reinit is triggered with %s!\n", __func__, buf);
		return -EINVAL;
	}

	pr_info("%s: reinit < %d\n", __func__, reinit);

	if (reinit) {
		pr_info("%s: started reloading / reinitializing (counter %d)\n",
			__func__, tfa98xx_cnt_reload + 1);
		tfa98xx_set_cnt_reload(NULL, NULL);
	}

	return count;
}

static ssize_t tfa98xx_ramp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int count = 0, value;

	tfa = tfa98xx->tfa;
	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	value = (tfa->ramp_steps == 0) ? RAMPDOWN_DEFAULT : tfa->ramp_steps;
	pr_debug("[0x%x] ramp_steps : %d\n",
		tfa98xx->i2c->addr, value);
	count = snprintf(buf, PAGE_SIZE, "%d\n", value);

	return count;
}

static ssize_t tfa98xx_ramp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct tfa_device *tfa = NULL;
	int ret = 0, value;

	tfa = tfa98xx->tfa;
	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0) {
		pr_err("[0x%x] %s: system is not initialized: not probed yet!\n",
			tfa98xx->i2c->addr, __func__);
		return -EIO;
	}

	/* check string length, and account for eol */
	if (count < 1)
		return -EINVAL;

	ret = kstrtou32(buf, 10, &value);

	pr_info("%s: ramp_steps < %d\n", __func__, value);

	mutex_lock(&tfa98xx_mutex);
	tfa->ramp_steps = value;
	mutex_unlock(&tfa98xx_mutex);

	return count;
}

static struct bin_attribute dev_attr_rw = {
	.attr = {
		.name = "rw",
		.mode = 0600,
	},
	.size = 0,
	.read = tfa98xx_rw_read,
	.write = tfa98xx_rw_write,
};

static struct bin_attribute dev_attr_reg = {
	.attr = {
		.name = "reg",
		.mode = 0200,
	},
	.size = 0,
	.read = NULL,
	.write = tfa98xx_reg_write,
};

static struct device_attribute dev_attr_gain = {
	.attr = {
		.name = "gain",
		.mode = 0600,
	},
	.show = tfa98xx_gain_show,
	.store = tfa98xx_gain_store,
};

static struct device_attribute dev_attr_autocal = {
	.attr = {
		.name = "autocal",
		.mode = 0600,
	},
	.show = tfa98xx_autocal_show,
	.store = tfa98xx_autocal_store,
};

static struct device_attribute dev_attr_reinit = {
	.attr = {
		.name = "reinit",
		.mode = 0600,
	},
	.show = tfa98xx_reinit_show,
	.store = tfa98xx_reinit_store,
};

static struct device_attribute dev_attr_ramp = {
	.attr = {
		.name = "ramp",
		.mode = 0600,
	},
	.show = tfa98xx_ramp_show,
	.store = tfa98xx_ramp_store,
};

struct tfa_device *tfa98xx_get_tfa_device_from_index(int index)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *ntfa = NULL;
	static struct tfa_device *tfadevset[MAX_HANDLES];

	if (index < 0 || index >= MAX_HANDLES)
		return NULL;

	if (tfadevset[index] != NULL)
		return tfadevset[index];

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (tfa98xx->tfa->dev_idx == index) {
			ntfa = tfa98xx->tfa;
			tfadevset[index] = ntfa;
			break;
		}
	}

	return ntfa;
}
EXPORT_SYMBOL(tfa98xx_get_tfa_device_from_index);

struct tfa_device *tfa98xx_get_tfa_device_from_channel(int channel)
{
	struct tfa98xx *tfa98xx;
	struct tfa_device *ntfa = NULL;
	static struct tfa_device *tfachnset[MAX_CHANNELS];
	int nchannel = -1;

	if (channel < 0 || channel >= MAX_CHANNELS)
		return NULL;

	if (tfachnset[channel] != NULL)
		return tfachnset[channel];

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		nchannel = tfa98xx_get_cnt_bitfield(tfa98xx->tfa,
			TFA7x_FAM(tfa98xx->tfa, TDMSPKS));
		if (nchannel == channel) {
			ntfa = tfa98xx->tfa;
			tfachnset[channel] = ntfa;
			break;
		}
	}

	return ntfa;
}
EXPORT_SYMBOL(tfa98xx_get_tfa_device_from_channel);

int tfa98xx_count_active_stream(int stream_flag)
{
	struct tfa98xx *tfa98xx;
	int stream_counter = 0;

	list_for_each_entry(tfa98xx, &tfa98xx_device_list, list) {
		if (tfa98xx->tfa->stream_state & stream_flag)
			stream_counter++;
	}

	return stream_counter;
}

enum tfa98xx_error tfa_run_cal(int index, uint16_t *value)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	struct tfa98xx *tfa98xx;
	int ret = 0;

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;

	tfa98xx = (struct tfa98xx *)tfa->data;

	/* check if calibration already runs */
	tfa_wait_until_calibration_done(tfa);

	ret = tfa98xx_run_calibration(tfa98xx);
	if (ret < 0)
		return TFA98XX_ERROR_FAIL;

	if (value == NULL)
		return TFA98XX_ERROR_BAD_PARAMETER;

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_run_cal);

void tfa_restore_after_cal(int index, int cal_err)
{
	enum tfa98xx_error err = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	struct tfa_device *ntfa;
	int i;
	int need_restore = 0;
	int active_profile = -1;

	pr_info("%s: enter with dev_idx %d (cal_err %d)\n",
		__func__, tfa->dev_idx, cal_err);

	/* mute and check if it needs restoring */
	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;
		if (!tfa_is_active_device(ntfa))
			continue;

		/* force MUTE after calibration, to set UNMUTE at sync later */
		pr_debug("%s: [%d] force MUTE after calibration\n",
			__func__, ntfa->dev_idx);
		tfa_dev_set_state(ntfa, TFA_STATE_MUTE, 1);

		active_profile = tfa_dev_get_swprof(ntfa);
		if (ntfa->next_profile == active_profile
			|| ntfa->next_profile < 0)
			continue;

		need_restore = 1;
	}

	/* restore by switching profile */
	if (need_restore) {
		/* reset counter */
		tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);
		tfa_set_status_flag(tfa, TFA_SET_CONFIG, -1);

		/*
		 * restore profile after calibration.
		 * typically, when calibration is done,
		 * profile should be updated in warm state.
		 */
		for (i = 0; i < tfa->dev_count; i++) {
			ntfa = tfa98xx_get_tfa_device_from_index(i);

			if (ntfa == NULL)
				continue;
			if (!tfa_is_active_device(ntfa))
				continue;

			tfa_set_status_flag(ntfa, TFA_SET_DEVICE, 1);

			active_profile = tfa_dev_get_swprof(ntfa);
			pr_info("%s: [%d] restore profile after calibration (active %d; next %d)\n",
				__func__, ntfa->dev_idx,
				active_profile, ntfa->next_profile);

			/* switch profile */
			pr_info("%s: apply the whole profile setting\n",
				__func__);

			err = tfa_dev_switch_profile(ntfa,
				ntfa->next_profile, ntfa->vstep);
			if (err != TFA98XX_ERROR_OK)
				pr_err("%s: error in switch profile (%d)\n",
					__func__, err);
		}

		/* reset counter */
		tfa_set_status_flag(tfa, TFA_SET_DEVICE, -1);
		tfa_set_status_flag(tfa, TFA_SET_CONFIG, -1);
	}

	/* set gain and unmute */
	for (i = 0; i < tfa->dev_count; i++) {
		ntfa = tfa98xx_get_tfa_device_from_index(i);

		if (ntfa == NULL)
			continue;
		if (!tfa_is_active_device(ntfa))
			continue;

		if (cal_err != TFA98XX_ERROR_OK
			|| ntfa->spkr_damaged) {
			tfa_handle_damaged_speakers(ntfa);
			continue;
		}

		if (ntfa->spkgain != -1) {
			pr_info("%s: set speaker gain 0x%x\n",
				__func__, ntfa->spkgain);
			TFA7x_SET_BF(ntfa, TDMSPKG,
				ntfa->spkgain);
		}
		/* force UNMUTE after processing calibration */
		pr_debug("%s: [%d] force UNMUTE after processing calibration\n",
			__func__, ntfa->dev_idx);
		tfa_dev_set_state(ntfa, TFA_STATE_UNMUTE, 1);
	}
}
EXPORT_SYMBOL(tfa_restore_after_cal);

int tfa98xx_set_blackbox(int enable)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;
	if (tfa->tfa_family == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	pr_info("%s: blackbox is not implemented\n", __func__);

	return ret;
}

int tfa98xx_get_blackbox_data(int dev, int *data)
{
	enum tfa98xx_error ret = TFA98XX_ERROR_OK;
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	int ndev = 0;

	if (!tfa)
		return TFA98XX_ERROR_NOT_OPEN;
	if (tfa->tfa_family == 0)
		return TFA98XX_ERROR_NOT_OPEN;

	ndev = tfa->dev_count;
	if (ndev < 1)
		return TFA98XX_ERROR_NOT_OPEN;
	if (dev < 0 || dev >= ndev)
		return TFA98XX_ERROR_BAD_PARAMETER;

	pr_info("%s: blackbox is not implemented\n", __func__);

	return ret;
}

int tfa98xx_get_blackbox_data_index(int dev, int index, int reset)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(0);
	int ndev = 0;
	int value = 0;

	if (!tfa)
		return -ENODEV;
	if (tfa->tfa_family == 0)
		return -ENODEV;

	if (index < 0 || index >= ID_BLACKBOX_MAX)
		return -EINVAL;

	ndev = tfa->dev_count;
	if (ndev < 1)
		return -EINVAL;
	if (dev < 0 || dev >= ndev)
		return -EINVAL;

	pr_info("%s: blackbox is not implemented\n", __func__);

	return value;
}

int tfa98xx_get_blackbox_data_index_channel(int channel,
	int index, int reset)
{
	int dev = tfa_get_dev_idx_from_inchannel(channel);

	return tfa98xx_get_blackbox_data_index(dev, index, reset);
}

int tfa_get_power_state(int index)
{
	struct tfa_device *tfa = tfa98xx_get_tfa_device_from_index(index);
	int pm = 0;
	int state = 0, control = 0;

	if (tfa == NULL)
		return 0; /* unused device */

	state = TFA7x_GET_BF(tfa, LP0);
	control = TFA7x_GET_BF(tfa, IPM);
	if ((control == 0x0 || control == 0x3)
		&& (state == 0x1))
		pm |= 0x2; /* idle power */
	switch (tfa->rev & 0xff) {
	case 0x78:
	case 0x74:
	case 0x72:
	case 0x94:
		state = TFA7x_GET_BF(tfa, LP1);
		control = TFA7x_GET_BF(tfa, LPM1MODE);
		if ((control == 0x0 || control == 0x3)
			&& (state == 0x1))
			pm |= 0x1; /* low power */
		break;
	default:
		/* neither TFA987x */
		break;
	}

	state = TFA_GET_BF(tfa, PWDN);
	if (state == 1)
		pm |= 0x4; /* power down */

	return pm;
}
EXPORT_SYMBOL(tfa_get_power_state);

static int tfa98xx_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct tfa98xx *tfa98xx;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags;
	unsigned int reg;
	int ret;

	pr_info("%s: start probing\n", __func__);
	pr_info("addr=0x%x\n", i2c->addr);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	tfa98xx = devm_kzalloc(&i2c->dev,
		sizeof(struct tfa98xx), GFP_KERNEL);
	if (tfa98xx == NULL)
		return -ENOMEM;

	tfa98xx->dev = &i2c->dev;
	tfa98xx->i2c = i2c;
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	tfa98xx->rate = 48000; /* init to the default sample rate (48kHz) */
	tfa98xx->tfa = NULL;

	tfa98xx->regmap = devm_regmap_init_i2c(i2c, &tfa98xx_regmap);
	if (IS_ERR(tfa98xx->regmap)) {
		ret = PTR_ERR(tfa98xx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	i2c_set_clientdata(i2c, tfa98xx);
	mutex_init(&tfa98xx->dsp_lock);
	init_waitqueue_head(&tfa98xx->wq);

	if (np) {
		ret = tfa98xx_parse_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev, "Failed to parse DT node\n");
			return ret;
		}
		if (no_start)
			tfa98xx->irq_gpio = -1;
		if (no_reset)
			tfa98xx->reset_gpio = -1;
	} else {
		tfa98xx->reset_gpio = -1;
		tfa98xx->irq_gpio = -1;
	}

	if (gpio_is_valid(tfa98xx->reset_gpio)) {
		ret = gpio_request_one((unsigned int)tfa98xx->reset_gpio,
			GPIOF_OUT_INIT_HIGH, "TFA98XX_RSTN"); /* RSTN */
		if (ret)
			return ret;
	}

	if (gpio_is_valid(tfa98xx->irq_gpio)) {
		ret = gpio_request_one((unsigned int)tfa98xx->irq_gpio,
			GPIOF_DIR_IN, "TFA98XX_INT");
		if (ret)
			return ret;
	}

	/* Power up! */
	tfa98xx_ext_reset(tfa98xx);

	if ((no_start == 0) && (no_reset == 0)) {
		ret = regmap_read(tfa98xx->regmap,
			TFA98XX_DEVICE_REVISION, &reg);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to read Revision register: %d\n",
				ret);
			return -EIO;
		}

		switch (reg & 0xff) {
		case 0x72: /* tfa9872 */
			pr_info("TFA9872 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_REMOVE_PLOP_NOISE;
			/* tfa98xx->flags |= TFA98XX_FLAG_LP_MODES; */
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x74: /* tfa9874 */
			pr_info("TFA9874 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x78: /* tfa9878 */
			pr_info("TFA9878 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x88: /* tfa9888 */
			pr_info("TFA9888 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_STEREO_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x13: /* tfa9912 */
			pr_info("TFA9912 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			/* tfa98xx->flags |= TFA98XX_FLAG_TAPDET_AVAILABLE; */
			break;
		case 0x94: /* tfa9894 */
			pr_info("TFA9894 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x80: /* tfa9890 */
		case 0x81: /* tfa9890 */
			pr_info("TFA9890 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x92: /* tfa9891 */
			pr_info("TFA9891 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SAAM_AVAILABLE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x12: /* tfa9895 */
			pr_info("TFA9895 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			break;
		case 0x97:
			pr_info("TFA9897 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x96:
			pr_info("TFA9896 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		default:
			pr_info("Unsupported device revision (0x%x)\n",
				reg & 0xff);
			return -EINVAL;
		}
	}

	tfa98xx->tfa = devm_kzalloc(&i2c->dev,
		sizeof(struct tfa_device), GFP_KERNEL);
	if (tfa98xx->tfa == NULL)
		return -ENOMEM;

	tfa98xx->tfa->data = (void *)tfa98xx;
	tfa98xx->tfa->cachep = tfa98xx_cache;

	if (np) {
		ret = tfa98xx_parse_limit_cal_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev,
				"Failed to parse DT node for cal range\n");
			/* set default range instead */
		}
		ret = tfa98xx_parse_inchannel_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev,
				"Failed to parse DT node for inchannel\n");
			/* set default value instead */
		}
	}

	/* Modify the stream names, by appending the i2c device address.
	 * This is used with multicodec, in order to discriminate devices.
	 * Stream names appear in the dai definition and in the stream.
	 * We create copies of original structures because each device will
	 * have its own instance of this structure, with its own address.
	 */
	dai = devm_kzalloc(&i2c->dev, sizeof(tfa98xx_dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;
	memcpy(dai, tfa98xx_dai, sizeof(tfa98xx_dai));

	tfa98xx_append_i2c_address(&i2c->dev,
		i2c,
		NULL,
		0,
		dai,
		ARRAY_SIZE(tfa98xx_dai));

	ret = snd_soc_register_component(&i2c->dev,
		&soc_component_dev_tfa98xx, dai,
		ARRAY_SIZE(tfa98xx_dai));

	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register TFA98xx: %d\n", ret);
		return ret;
	}

	if (gpio_is_valid(tfa98xx->irq_gpio) &&
		!(tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
			gpio_to_irq((unsigned int)tfa98xx->irq_gpio),
			NULL, tfa98xx_irq, irq_flags,
			"tfa98xx", tfa98xx);
		if (ret != 0) {
			dev_err(&i2c->dev, "Failed to request IRQ %d: %d\n",
				gpio_to_irq((unsigned int)tfa98xx->irq_gpio),
				ret);
			return ret;
		}
	} else {
		dev_info(&i2c->dev, "Skipping IRQ registration\n");
		/* disable feature support if gpio was invalid */
		tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
	}

#if defined(CONFIG_DEBUG_FS)
	if (no_start == 0)
		tfa98xx_debug_init(tfa98xx, i2c);
#endif
	/* Register the sysfs files for climax backdoor access */
	ret = sysfs_create_bin_file(&i2c->dev.kobj, &dev_attr_rw);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, rw\n");
	ret = sysfs_create_bin_file(&i2c->dev.kobj, &dev_attr_reg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, reg\n");

	ret = device_create_file(&i2c->dev, &dev_attr_gain);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, gain\n");

	ret = device_create_file(&i2c->dev, &dev_attr_autocal);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, autocal\n");

	ret = device_create_file(&i2c->dev, &dev_attr_reinit);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, reinit\n");

	ret = device_create_file(&i2c->dev, &dev_attr_ramp);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs node, ramp\n");

	pr_info("%s Probe completed successfully!\n", __func__);

	INIT_LIST_HEAD(&tfa98xx->list);

	mutex_lock(&tfa98xx_mutex);
	tfa98xx_device_count++;
	list_add(&tfa98xx->list, &tfa98xx_device_list); /* stack */
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static int tfa98xx_i2c_remove(struct i2c_client *i2c)
{
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	pr_debug("addr=0x%x\n", i2c->addr);

	tfa98xx_interrupt_enable(tfa98xx, false);

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);

	sysfs_remove_bin_file(&i2c->dev.kobj, &dev_attr_reg);
	sysfs_remove_bin_file(&i2c->dev.kobj, &dev_attr_rw);
#if defined(CONFIG_DEBUG_FS)
	tfa98xx_debug_remove(tfa98xx);
#endif

	snd_soc_unregister_component(&i2c->dev);

	if (gpio_is_valid(tfa98xx->irq_gpio))
		gpio_free((unsigned int)tfa98xx->irq_gpio);
	if (gpio_is_valid(tfa98xx->reset_gpio))
		gpio_free((unsigned int)tfa98xx->reset_gpio);

	mutex_lock(&tfa98xx_mutex);
	list_del(&tfa98xx->list);
	tfa98xx_device_count--;
	if (tfa98xx_device_count == 0) {
		kfree(tfa98xx_container);
		tfa98xx_container = NULL;
	}
	mutex_unlock(&tfa98xx_mutex);

	return 0;
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{"tfa98xx", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tfa98xx_dt_match[] = {
	{.compatible = "tfa,tfa98xx"},
	{.compatible = "tfa,tfa9872"},
	{.compatible = "tfa,tfa9874"},
	{.compatible = "tfa,tfa9878"},
	{.compatible = "tfa,tfa9888"},
	{.compatible = "tfa,tfa9890"},
	{.compatible = "tfa,tfa9891"},
	{.compatible = "tfa,tfa9894"},
	{.compatible = "tfa,tfa9895"},
	{.compatible = "tfa,tfa9896"},
	{.compatible = "tfa,tfa9897"},
	{.compatible = "tfa,tfa9912"},
	{},
};
#endif

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa98xx_dt_match),
	},
	.probe = tfa98xx_i2c_probe,
	.remove = tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
};

static int __init tfa98xx_i2c_init(void)
{
	int ret = 0;

	pr_info("TFA98XX driver version %s\n", TFA98XX_VERSION);

	/* Enable debug traces */
	tfa98xx_kmsg_regs = trace_level & 2;
	tfa98xx_ftrace_regs = trace_level & 4;

	/* Initialize kmem_cache */
	/* Cache name /proc/slabinfo */
	tfa98xx_cache = kmem_cache_create("tfa98xx_cache",
		PAGE_SIZE, /* Structure size, we should fit in single page */
		0, /* Structure alignment */
		(SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT |
		SLAB_MEM_SPREAD), /* Cache property */
		NULL); /* Object constructor */
	if (!tfa98xx_cache) {
		pr_err("tfa98xx can't create memory pool\n");
		ret = -ENOMEM;
	}

	ret = i2c_add_driver(&tfa98xx_i2c_driver);

	return ret;
}
module_init(tfa98xx_i2c_init);

static void __exit tfa98xx_i2c_exit(void)
{
	i2c_del_driver(&tfa98xx_i2c_driver);
	kmem_cache_destroy(tfa98xx_cache);
}
module_exit(tfa98xx_i2c_exit);

MODULE_DESCRIPTION("ASoC TFA98XX driver");
MODULE_LICENSE("GPL");

