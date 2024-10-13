/*
 * Calibration support for Cirrus Logic Smart Amplifiers
 *
 * Copyright 2021 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

#include <sound/cirrus/core.h>
#include <sound/cirrus/calibration.h>

#include "wmfw.h"
#include "wm_adsp.h"


#define CIRRUS_CAL_COMPLETE_DELAY_MS	1250
#define CIRRUS_CAL_RETRIES		2
#define CIRRUS_CAL_AMBIENT_DEFAULT	23

#define CIRRUS_CAL_CONFIG_FILENAME_SUFFIX	"-dsp1-spk-prot-calib.bin"
#define CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX	"-dsp1-spk-prot.bin"

enum cirrus_cspl_mboxstate {
	CSPL_MBOX_STS_RUNNING = 0,
	CSPL_MBOX_STS_PAUSED = 1,
	CSPL_MBOX_STS_RDY_FOR_REINIT = 2,
};

enum cirrus_cspl_mboxcmd {
	CSPL_MBOX_CMD_NONE = 0,
	CSPL_MBOX_CMD_PAUSE = 1,
	CSPL_MBOX_CMD_RESUME = 2,
	CSPL_MBOX_CMD_REINIT = 3,
	CSPL_MBOX_CMD_STOP_PRE_REINIT = 4,
	CSPL_MBOX_CMD_UNKNOWN_CMD = -1,
	CSPL_MBOX_CMD_INVALID_SEQUENCE = -2,
};

static unsigned long long cirrus_cal_rdc_to_ohms(unsigned long rdc)
{
	return ((rdc * CIRRUS_CAL_AMP_CONSTANT_NUM) /
		CIRRUS_CAL_AMP_CONSTANT_DENOM);
}

static unsigned int cirrus_cal_vpk_to_mv(unsigned int vpk)
{
	return (vpk * CIRRUS_CAL_VFS_MV) >> 19;
}

static int32_t cirrus_cal_sign_extend(uint32_t in)
{
		uint8_t shift = 8;

		return (int32_t)(in << shift) >> shift;
}

int cirrus_cal_logger_get_variable(struct cirrus_amp *amp, unsigned int id,
				   unsigned int *result)
{
	unsigned int state;
	int retries = 100;

	cirrus_amp_write_ctl(amp, "RTLOG_COUNT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "RTLOG_VARIABLE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, id);
	cirrus_amp_write_ctl(amp, "RTLOG_COUNT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 1);
	cirrus_amp_write_ctl(amp, "RTLOG_STATE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "RTLOG_ENABLE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 1);

	do {
		usleep_range(20, 50);
		cirrus_amp_read_ctl(amp, "RTLOG_STATE", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &state);
	} while (state == 0 && --retries > 0);

	if (retries == 0) {
		dev_err(amp_group->cal_dev, "variable read failed\n");
		return -1;
	}

	cirrus_amp_read_ctl(amp, "RTLOG_DATA", WMFW_ADSP2_XM,
			    CIRRUS_AMP_ALG_ID_CSPL, result);
	cirrus_amp_write_ctl(amp, "RTLOG_ENABLE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);

	return 0;
}

static int cirrus_cal_load_config(const char *file, struct cirrus_amp *amp)
{
	struct wm_adsp *dsp = snd_soc_component_get_drvdata(amp->component);
	int ret;

	dsp->firmwares[dsp->fw].fullname  = true;
	dsp->firmwares[dsp->fw].binfile = file;

	ret = wm_adsp_load_coeff(dsp);

	dsp->firmwares[dsp->fw].fullname  = false;
	dsp->firmwares[dsp->fw].binfile = NULL;

	return ret;
}

static int cirrus_cal_get_power_temp(void)
{
	union power_supply_propval value = {0};
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_warn(amp_group->cal_dev,
			 "failed to get battery, assuming %d\n",
			 CIRRUS_CAL_AMBIENT_DEFAULT);
		return CIRRUS_CAL_AMBIENT_DEFAULT;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);
	if (ret) {
		dev_warn(amp_group->cal_dev,
			 "failed to get battery temp prop (%d), assuming %d\n",
			 ret, CIRRUS_CAL_AMBIENT_DEFAULT);
		return CIRRUS_CAL_AMBIENT_DEFAULT;
	}

	return DIV_ROUND_CLOSEST(value.intval, 10);
}

static int cirrus_cal_cspl_start(void);

static void cirrus_cal_cspl_complete(void)
{
	struct cirrus_amp *amp;
	struct reg_sequence *post_config;
	struct regmap *regmap;
	const char *dsp_part_name;
	char *playback_config_filename;
	unsigned long long ohms;
	unsigned int cal_state, mbox_cmd, mbox_sts;
	int rdc, status, checksum, temp, vsc, isc, timeout = 100, i;
	int delay = msecs_to_jiffies(CIRRUS_CAL_COMPLETE_DELAY_MS);
	bool vsc_in_range, isc_in_range;
	bool cal_retry = false;

	mutex_lock(&amp_group->cal_lock);

	for (i = 0; i < amp_group->num_amps; i++) {
		amp = &amp_group->amps[i];
		if (amp->calibration_disable)
			continue;

		regmap = amp->regmap;
		dsp_part_name = amp->dsp_part_name;
		post_config = amp->post_config;
		mbox_cmd = amp->mbox_cmd;
		mbox_sts = amp->mbox_sts;

		playback_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
		snprintf(playback_config_filename, PAGE_SIZE, "%s%s",
			 dsp_part_name, CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX);

		cirrus_amp_read_ctl(amp, "CAL_STATUS", WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &status);
		cirrus_amp_read_ctl(amp, "CAL_R", WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &rdc);
		cirrus_amp_read_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &temp);
		cirrus_amp_read_ctl(amp, "CAL_CHECKSUM", WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &checksum);

		ohms = cirrus_cal_rdc_to_ohms((unsigned long)rdc);

		cirrus_amp_read_ctl(amp, "CSPL_STATE", WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
		if (cal_state == CSPL_STATE_ERROR) {
			dev_err(amp_group->cal_dev,
				  "Error during ReDC cal, invalidating results\n");
			rdc = status = checksum = 0;
		}

		if (amp->perform_vimon_cal) {
			cirrus_amp_read_ctl(amp, "VSC", WMFW_ADSP2_XM,
						amp->vimon_alg_id, &vsc);
			cirrus_amp_read_ctl(amp, "ISC", WMFW_ADSP2_XM,
						amp->vimon_alg_id, &isc);
			cirrus_amp_read_ctl(amp, "VIMON_CAL_STATE",
						WMFW_ADSP2_XM, amp->vimon_alg_id,
						&cal_state);
			if (cal_state == CIRRUS_CAL_VIMON_STATUS_INVALID ||
				cal_state == 0) {
				dev_err(amp_group->cal_dev,
					  "Error during VIMON cal, invalidating results\n");
				rdc = status = checksum = 0;
			}

			vsc_in_range = ((vsc <= amp->cal_vsc_ub) ||
					(vsc >= amp->cal_vsc_lb && vsc <= 0x00FFFFFF));

			isc_in_range = ((isc <= amp->cal_isc_ub) ||
					(isc >= amp->cal_isc_lb && isc <= 0x00FFFFFF));

			if (!vsc_in_range)
				dev_err(amp_group->cal_dev, "VIMON Cal %s (%s): VSC out of range (%x)\n",
					amp->dsp_part_name, amp->mfd_suffix, vsc);
			if (!isc_in_range)
				dev_err(amp_group->cal_dev, "VIMON Cal %s (%s): ISC out of range (%x)\n",
					amp->dsp_part_name, amp->mfd_suffix, isc);
			if (!vsc_in_range || !isc_in_range) {
				dev_err(amp_group->cal_dev, "VIMON cal out of range, invalidating results\n");
				rdc = status = checksum = 0;

				cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE",
					WMFW_ADSP2_XM, amp->vimon_alg_id,
					CIRRUS_CAL_VIMON_STATUS_INVALID);

				if (amp_group->cal_retry < CIRRUS_CAL_RETRIES) {
					dev_info(amp_group->cal_dev, "Retry Calibration\n");
					cal_retry = true;
				}
			}
		} else {
			vsc = 0;
			isc = 0;
			cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE",
				WMFW_ADSP2_XM, amp->vimon_alg_id,
				CIRRUS_CAL_VIMON_STATUS_INVALID);
		}

		dev_info(amp_group->cal_dev,
				"Calibration finished: %s (%s)\n",
					amp->dsp_part_name, amp->mfd_suffix);
		dev_info(amp_group->cal_dev, "Duration:\t%d ms\n",
			 CIRRUS_CAL_COMPLETE_DELAY_MS);
		dev_info(amp_group->cal_dev, "Status:\t%d\n", status);
		if (status == CSPL_STATUS_OUT_OF_RANGE)
			dev_err(amp_group->cal_dev,
				"Calibration out of range\n");
		if (status == CSPL_STATUS_INCOMPLETE)
			dev_err(amp_group->cal_dev, "Calibration incomplete\n");
		dev_info(amp_group->cal_dev, "R :\t\t%d (%llu.%04llu Ohms)\n",
			 rdc, ohms >> CIRRUS_CAL_RDC_RADIX,
			 (ohms & (((1 << CIRRUS_CAL_RDC_RADIX) - 1))) *
			 10000 / (1 << CIRRUS_CAL_RDC_RADIX));
		dev_info(amp_group->cal_dev, "Checksum:\t%d\n", checksum);
		dev_info(amp_group->cal_dev, "Ambient:\t%d\n", temp);

		usleep_range(5000, 5500);

		/* Send STOP_PRE_REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_STOP_PRE_REINIT);
		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT ready...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);
		} while ((cal_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
				--timeout > 0);

		msleep(100);

		cirrus_cal_load_config(playback_config_filename, amp);

		cirrus_amp_write_ctl(amp, "CAL_STATUS", WMFW_ADSP2_XM,
					 CIRRUS_AMP_ALG_ID_CSPL, status);
		cirrus_amp_write_ctl(amp, "CAL_R", WMFW_ADSP2_XM,
					 CIRRUS_AMP_ALG_ID_CSPL, rdc);
		cirrus_amp_write_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
					 CIRRUS_AMP_ALG_ID_CSPL, temp);
		cirrus_amp_write_ctl(amp, "CAL_CHECKSUM", WMFW_ADSP2_XM,
					 CIRRUS_AMP_ALG_ID_CSPL, checksum);

		/* Send REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_REINIT);
		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT done...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RUNNING) &&
				--timeout > 0);

		msleep(100);

		cirrus_amp_read_ctl(amp, "CSPL_STATE", WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
		if (cal_state == CSPL_STATE_ERROR)
			dev_err(amp_group->cal_dev,
				"Playback config load error\n");

		regmap_multi_reg_write(regmap, post_config,
					   amp->num_post_configs);

		amp->cal.efs_cache_rdc = rdc;
		amp->cal.efs_cache_vsc = vsc;
		amp->cal.efs_cache_isc = isc;
		amp_group->efs_cache_temp = temp;
		amp->cal.efs_cache_valid = 1;

		kfree(playback_config_filename);
	}

	if (cal_retry == true) {
		cirrus_cal_cspl_start();
		queue_delayed_work(system_unbound_wq,
			&amp_group->cal_complete_work,
			delay);
		amp_group->cal_retry++;
	} else {
		amp_group->cal_running = 0;
	}

	dev_dbg(amp_group->cal_dev, "Calibration complete\n");
	mutex_unlock(&amp_group->cal_lock);
}


static void cirrus_cal_v_val_complete(struct cirrus_amp *amps, int num_amps,
					bool separate)
{
	struct regmap *regmap;
	struct reg_sequence *post_config;
	const char *dsp_part_name;
	char *playback_config_filename;
	unsigned int mbox_cmd, mbox_sts, cal_state;
	int timeout = 100, amp;

	for (amp = 0; amp < num_amps; amp++) {
		if (amps[amp].v_val_separate && !separate)
			continue;

		regmap = amps[amp].regmap;
		dsp_part_name = amps[amp].dsp_part_name;
		post_config = amps[amp].post_config;
		mbox_cmd = amps[amp].mbox_cmd;
		mbox_sts = amps[amp].mbox_sts;

		playback_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
		snprintf(playback_config_filename, PAGE_SIZE, "%s%s",
			 dsp_part_name, CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX);

		/* Send STOP_PRE_REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_STOP_PRE_REINIT);
		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT ready...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
				--timeout > 0);

		msleep(100);

		cirrus_cal_load_config(playback_config_filename,
					   &amp_group->amps[amp]);

		/* Send REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_REINIT);

		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT done...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RUNNING) &&
				--timeout > 0);

		msleep(100);

		cirrus_amp_read_ctl(&amp_group->amps[amp], "CSPL_STATE",
					WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL,
					&cal_state);
		if (cal_state == CSPL_STATE_ERROR)
			dev_err(amp_group->cal_dev,
				"Playback config load error\n");

		regmap_multi_reg_write(regmap, post_config,
					   amps[amp].num_post_configs);

		kfree(playback_config_filename);
	}

	dev_info(amp_group->cal_dev, "V validation complete\n");
}


static void cirrus_cal_cspl_vimon_cal_start(struct cirrus_amp *amp)
{

	cirrus_amp_write_ctl(amp, "VIMON_CLASS_H_CAL_DELAY", WMFW_ADSP2_XM,
				 amp->vimon_alg_id, CIRRUS_CAL_CLASSH_DELAY_50MS);
	cirrus_amp_write_ctl(amp, "VIMON_CLASS_D_CAL_DELAY", WMFW_ADSP2_XM,
				 amp->vimon_alg_id, CIRRUS_CAL_CLASSD_DELAY_50MS);

	cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
				 amp->vimon_alg_id, 0);
	cirrus_amp_write_ctl(amp, "HALO_HEARTBEAT", WMFW_ADSP2_XM,
				 amp->halo_alg_id, 0);
}

static int cirrus_cal_cspl_vimon_cal_complete(struct cirrus_amp *amp)
{
	unsigned int vimon_cal, vsc, isc;
	bool vsc_in_range, isc_in_range;

	cirrus_amp_read_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
				amp->vimon_alg_id, &vimon_cal);
	cirrus_amp_read_ctl(amp, "VSC", WMFW_ADSP2_XM, amp->vimon_alg_id, &vsc);
	cirrus_amp_read_ctl(amp, "ISC", WMFW_ADSP2_XM, amp->vimon_alg_id, &isc);

	dev_info(amp_group->cal_dev,
		 "VIMON Cal results %s (%s), status=%d vsc=%x isc=%x\n",
		 amp->dsp_part_name, amp->mfd_suffix, vimon_cal, vsc, isc);

	vsc_in_range = ((vsc <= amp->cal_vsc_ub) ||
			(vsc >= amp->cal_vsc_lb && vsc <= 0x00FFFFFF));

	isc_in_range = ((isc <= amp->cal_isc_ub) ||
			(isc >= amp->cal_isc_lb && isc <= 0x00FFFFFF));

	if (!vsc_in_range || !isc_in_range)
		vimon_cal = CIRRUS_CAL_VIMON_STATUS_INVALID;

	return vimon_cal;
}


static int cirrus_cal_wait_for_active(struct cirrus_amp *amp)
{
	struct regmap *regmap = amp->regmap;
	unsigned int global_en;
	unsigned int halo_state;
	int timeout = 50;

	regmap_read(regmap, amp->global_en, &global_en);

	while ((global_en & amp->global_en_mask) == 0) {
		usleep_range(1000, 1500);
		regmap_read(regmap, amp->global_en, &global_en);
	}

	do {
		dev_info(amp_group->cal_dev, "waiting for HALO start...\n");

		usleep_range(16000, 16100);

		cirrus_amp_read_ctl(amp, "HALO_STATE", WMFW_ADSP2_XM,
					amp->halo_alg_id, &halo_state);
		timeout--;
	} while ((halo_state == 0) && timeout > 0);

	if (timeout == 0) {
		dev_err(amp_group->cal_dev, "Failed to setup calibration\n");
		return -EINVAL;
	}

	return 0;
}

static void cirrus_cal_cspl_redc_start(struct cirrus_amp *amp)
{
	struct regmap *regmap = amp->regmap;
	const char *dsp_part_name = amp->dsp_part_name;
	char *cal_config_filename;
	unsigned int halo_state;
	int timeout = 50;
	int ambient;

	cal_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
	snprintf(cal_config_filename, PAGE_SIZE, "%s%s", dsp_part_name,
		 CIRRUS_CAL_CONFIG_FILENAME_SUFFIX);

	dev_info(amp_group->cal_dev, "ReDC Calibration load start\n");

	/* Send STOP_PRE_REINIT command and poll for response */
	regmap_write(regmap, amp->mbox_cmd, CSPL_MBOX_CMD_STOP_PRE_REINIT);
	timeout = 100;
	do {
		dev_info(amp_group->cal_dev, "waiting for REINIT ready...\n");
		usleep_range(1000, 1500);
		regmap_read(regmap, amp->mbox_sts, &halo_state);
	} while ((halo_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
			--timeout > 0);

	if (timeout == 0)
		dev_err(amp->component->dev, "REINIT ready not found\n");

	msleep(100);

	dev_dbg(amp_group->cal_dev, "load %s\n", dsp_part_name);
	cirrus_cal_load_config(cal_config_filename, amp);

	ambient = cirrus_cal_get_power_temp();
	cirrus_amp_write_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
				 CIRRUS_AMP_ALG_ID_CSPL, ambient);

	/* Send REINIT command and poll for response */
	regmap_write(regmap, amp->mbox_cmd, CSPL_MBOX_CMD_REINIT);
	timeout = 100;
	do {
		dev_info(amp_group->cal_dev, "waiting for REINIT done...\n");
		usleep_range(1000, 1500);
		regmap_read(regmap, amp->mbox_sts, &halo_state);
	} while ((halo_state != CSPL_MBOX_STS_RUNNING) &&
			--timeout > 0);

	if (timeout == 0)
		dev_err(amp->component->dev, "REINIT done not found\n");

	kfree(cal_config_filename);
}

int cirrus_cal_cspl_cal_apply(struct cirrus_amp *amp)
{
	unsigned int temp, rdc, status, checksum, vsc = 0, isc = 0;
	unsigned int vimon_cal_status = CIRRUS_CAL_VIMON_STATUS_SUCCESS;
	int ret = 0;

	if (!amp)
		return 0;

	if (amp->cal.efs_cache_valid == 1) {
		rdc = amp->cal.efs_cache_rdc;
		vsc = amp->cal.efs_cache_vsc;
		isc = amp->cal.efs_cache_isc;
		vimon_cal_status = CIRRUS_CAL_VIMON_STATUS_SUCCESS;
		temp = amp_group->efs_cache_temp;
	} else if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
			(!amp->cal.efs_cache_vsc && !amp->cal.efs_cache_isc)) {
		dev_info(amp_group->cal_dev,
				"No VIMON, writing RDC only\n");
		rdc = amp->cal.efs_cache_rdc;
		temp = amp_group->efs_cache_temp;
		vimon_cal_status = CIRRUS_CAL_VIMON_STATUS_INVALID;
	} else {

		dev_info(amp_group->cal_dev,
				"No saved EFS, writing defaults\n");
		rdc = amp->default_redc;
		temp = CIRRUS_CAL_AMBIENT_DEFAULT;
		vimon_cal_status = CIRRUS_CAL_VIMON_STATUS_INVALID;
		amp->cal.efs_cache_rdc = rdc;
		amp_group->efs_cache_temp = temp;
	}

	status = 1;
	checksum = status + rdc;

	dev_info(amp_group->cal_dev, "Writing calibration to %s (%s)\n",
				amp->dsp_part_name, amp->mfd_suffix);

	dev_info(amp_group->cal_dev,
		 "RDC = %d, Temp = %d, Status = %d Checksum = %d\n",
		 rdc, temp, status, checksum);

	cirrus_amp_write_ctl(amp, "CAL_STATUS", WMFW_ADSP2_XM,
				 CIRRUS_AMP_ALG_ID_CSPL, status);
	cirrus_amp_write_ctl(amp, "CAL_R", WMFW_ADSP2_XM,
				 CIRRUS_AMP_ALG_ID_CSPL, rdc);
	cirrus_amp_write_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
				 CIRRUS_AMP_ALG_ID_CSPL, temp);
	cirrus_amp_write_ctl(amp, "CAL_CHECKSUM", WMFW_ADSP2_XM,
				 CIRRUS_AMP_ALG_ID_CSPL, checksum);

	if (!amp->perform_vimon_cal) {
		cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
					 amp->vimon_alg_id,
					 CIRRUS_CAL_VIMON_STATUS_INVALID);
		goto skip_vimon_cal;
	}

	cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
				 amp->vimon_alg_id, vimon_cal_status);

	if (amp->perform_vimon_cal &&
		vimon_cal_status != CIRRUS_CAL_VIMON_STATUS_INVALID) {
		dev_info(amp_group->cal_dev,
			 "VIMON Cal status=%d vsc=0x%x isc=0x%x\n",
			 vimon_cal_status, vsc, isc);
		cirrus_amp_write_ctl(amp, "VSC", WMFW_ADSP2_XM,
					 amp->vimon_alg_id, vsc);
		cirrus_amp_write_ctl(amp, "ISC", WMFW_ADSP2_XM,
					 amp->vimon_alg_id, isc);
	} else {
		dev_info(amp_group->cal_dev, "VIMON Cal status invalid\n");
	}

skip_vimon_cal:
	return ret;
}

int cirrus_cal_cspl_read_temp(struct cirrus_amp *amp)
{
	int reg = 0, ret;
	unsigned int halo_state;
	unsigned int global_en;

	if (!amp)
		goto err;

	regmap_read(amp->regmap, amp->global_en, &global_en);

	if ((global_en & amp->global_en_mask) == 0)
		goto err;

	regmap_read(amp->regmap, amp->mbox_sts, &halo_state);

	if (halo_state != CSPL_MBOX_STS_RUNNING)
		goto err;

	if (amp_group->cal_running)
		goto err;

	ret = cirrus_cal_logger_get_variable(amp,
			CIRRUS_CAL_RTLOG_ID_TEMP,
			&reg);
	if (ret == 0) {
		if (reg == 0)
			cirrus_cal_logger_get_variable(amp,
				CIRRUS_CAL_RTLOG_ID_TEMP,
				&reg);

		reg = cirrus_cal_sign_extend(reg);
		dev_info(amp_group->cal_dev,
			"Read temp: %d.%04d degrees C\n",
			reg >> CIRRUS_CAL_RTLOG_RADIX_TEMP,
			(reg & (((1 << CIRRUS_CAL_RTLOG_RADIX_TEMP) - 1))) *
			10000 / (1 << CIRRUS_CAL_RTLOG_RADIX_TEMP));
		return (reg >> CIRRUS_CAL_RTLOG_RADIX_TEMP);
	}
err:
	return -1;
}

int cirrus_cal_cspl_set_surface_temp(struct cirrus_amp *amp, int temperature)
{
	unsigned int global_en;

	if (!amp)
		return -EINVAL;

	regmap_read(amp->regmap, amp->global_en, &global_en);

	if ((global_en & amp->global_en_mask) == 0)
		return -EINVAL;

	dev_info(amp->component->dev, "Set surface temp: %d degrees\n", temperature);
	cirrus_amp_write_ctl(amp, "CSPL_SURFACE_TEMP", WMFW_ADSP2_XM,
				CIRRUS_AMP_ALG_ID_CSPL, temperature);
	return 0;
}

static int cirrus_cal_cspl_start(void)
{
	int redc_cal_start_retries, vimon_cal_retries = 0;
	bool vimon_calibration_failed = false;
	unsigned int cal_state;
	int amp;
	struct reg_sequence *config;
	struct regmap *regmap;
	int ret = 0;

	for (amp = 0; amp < amp_group->num_amps; amp++) {
		if (amp_group->amps[amp].calibration_disable)
			continue;

		regmap = amp_group->amps[amp].regmap;

		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_STATUS",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_R",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_AMBIENT",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_CHECKSUM",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		if (amp_group->amps[amp].perform_vimon_cal) {
			cirrus_amp_write_ctl(&amp_group->amps[amp], "VSC",
				WMFW_ADSP2_XM,
				amp_group->amps[amp].vimon_alg_id, 0);
			cirrus_amp_write_ctl(&amp_group->amps[amp], "ISC",
				WMFW_ADSP2_XM,
				amp_group->amps[amp].vimon_alg_id, 0);
		}

		ret = cirrus_cal_wait_for_active(&amp_group->amps[amp]);
		if (ret < 0) {
			dev_err(amp_group->cal_dev,
				"Could not start amp%s (%d)\n",
				amp_group->amps[amp].mfd_suffix, ret);
			return -ETIMEDOUT;
		}
	}

	do {
		vimon_calibration_failed = false;

		for (amp = 0; amp < amp_group->num_amps; amp++) {
			if (amp_group->amps[amp].calibration_disable)
				continue;

			regmap = amp_group->amps[amp].regmap;
			config = amp_group->amps[amp].pre_config;

			regmap_multi_reg_write(regmap, config,
					       amp_group->amps[amp].num_pre_configs);
			if (amp_group->amps[amp].perform_vimon_cal)
				cirrus_cal_cspl_vimon_cal_start(&amp_group->amps[amp]);
		}


		msleep(112);

		for (amp = 0; amp < amp_group->num_amps; amp++) {
			if (amp_group->amps[amp].calibration_disable)
				continue;

			if (amp_group->amps[amp].perform_vimon_cal) {
				ret = cirrus_cal_cspl_vimon_cal_complete(
							&amp_group->amps[amp]);

				if (ret != CIRRUS_CAL_VIMON_STATUS_SUCCESS) {
					vimon_calibration_failed = true;
					dev_info(amp_group->cal_dev,
					  "VIMON Calibration Error %s (%s)\n",
					  amp_group->amps[amp].dsp_part_name,
					  amp_group->amps[amp].mfd_suffix);
				}
			}
		}

		vimon_cal_retries--;
	} while (vimon_cal_retries >= 0 && vimon_calibration_failed);

	for (amp = 0; amp < amp_group->num_amps; amp++) {
		if (amp_group->amps[amp].calibration_disable)
			continue;

		regmap = amp_group->amps[amp].regmap;

		cirrus_cal_cspl_redc_start(&amp_group->amps[amp]);

		cirrus_amp_read_ctl(&amp_group->amps[amp], "CSPL_STATE",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);

		redc_cal_start_retries = 5;
		while (cal_state == CSPL_STATE_ERROR &&
						redc_cal_start_retries > 0) {
			if (cal_state == CSPL_STATE_ERROR)
				dev_err(amp_group->cal_dev,
					"Calibration load error\n");

			cirrus_cal_cspl_redc_start(&amp_group->amps[amp]);

			cirrus_amp_read_ctl(&amp_group->amps[amp], "CSPL_STATE",
				WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
			redc_cal_start_retries--;
		}

		if (redc_cal_start_retries == 0) {
			config = amp_group->amps[amp].post_config;

			dev_err(amp_group->cal_dev,
				"Calibration setup fail amp%s (%d)\n",
				amp_group->amps[amp].mfd_suffix, ret);

			regmap_multi_reg_write(regmap, config,
					amp_group->amps[amp].num_post_configs);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

int cirrus_cal_cspl_v_val_start(struct cirrus_amp *amps, int num_amps, bool separate)
{
	struct reg_sequence *config;
	struct regmap *regmap;
	unsigned int vmax[CIRRUS_MAX_AMPS];
	unsigned int vmin[CIRRUS_MAX_AMPS];
	unsigned int cal_state;
	int ret = 0, i, j, reg, retries, count;

	for (i = 0; i < amp_group->num_amps; i++) {
		if (amps[i].v_val_separate && !separate)
			continue;

		regmap = amps[i].regmap;
		config = amps[i].pre_config;

		vmax[i] = 0;
		vmin[i] = INT_MAX;

		ret = cirrus_cal_wait_for_active(&amps[i]);
		if (ret < 0) {
			dev_err(amp_group->cal_dev,
				"Could not start amp%s\n",
				amps[i].mfd_suffix);
			goto err;
		}

		regmap_multi_reg_write(regmap, config,
				       amps[i].num_pre_configs);
		cirrus_cal_cspl_redc_start(&amps[i]);

		cirrus_amp_read_ctl(&amps[i], "CSPL_STATE",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);

		retries = 5;
		while (cal_state == CSPL_STATE_ERROR && retries > 0) {
			if (cal_state == CSPL_STATE_ERROR)
				dev_err(amp_group->cal_dev,
					"Calibration load error\n");

			cirrus_cal_cspl_redc_start(&amps[i]);

			cirrus_amp_read_ctl(&amps[i], "CSPL_STATE",
				WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
			retries--;
		}

		if (retries == 0) {
			config = amps[i].post_config;
			dev_err(amp_group->cal_dev,
				"Calibration setup fail @ %d\n", i);
			regmap_multi_reg_write(regmap, config,
					amps[i].num_post_configs);
			goto err;
		}
	}

	dev_info(amp_group->cal_dev, "V validation prepare complete\n");

	for (i = 0; i < 1000; i++) {
		count = 0;
		for (j = 0; j < num_amps; j++) {
			if (amps[j].v_val_separate && !separate)
				continue;

			regmap = amps[j].regmap;
			cirrus_cal_logger_get_variable(&amps[j],
						amps[j].cal_vpk_id,
						&reg);
			if (reg > vmax[j])
				vmax[j] = reg;
			if (reg < vmin[j])
				vmin[j] = reg;

			cirrus_amp_read_ctl(&amp_group->amps[j], "CAL_STATUS",
					WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &reg);

			if (reg != 0 && reg != CSPL_STATUS_INCOMPLETE)
				count++;
		}

		if (count == num_amps) {
			dev_info(amp_group->cal_dev,
			"V Validation complete (%d)\n", i);
			break;
		}
	}

	for (i = 0; i < num_amps; i++) {
		if (amps[i].v_val_separate && !separate)
			continue;

		dev_info(amp_group->cal_dev,
			"V Validation results for amp%s\n",
			amps[i].mfd_suffix);

		dev_dbg(amp_group->cal_dev, "V Max: 0x%x\n", vmax[i]);
		vmax[i] = cirrus_cal_vpk_to_mv(vmax[i]);
		dev_info(amp_group->cal_dev, "V Max: %d mV\n", vmax[i]);

		dev_dbg(amp_group->cal_dev, "V Min: 0x%x\n", vmin[i]);
		vmin[i] = cirrus_cal_vpk_to_mv(vmin[i]);
		dev_info(amp_group->cal_dev, "V Min: %d mV\n", vmin[i]);

		if (vmax[i] < CIRRUS_CAL_V_VAL_UB_MV &&
		    vmax[i] > CIRRUS_CAL_V_VAL_LB_MV) {
			amps[i].cal.v_validation = 1;
			dev_info(amp_group->cal_dev,
				 "V validation success\n");
		} else {
			amps[i].cal.v_validation = 0xCC;
			dev_err(amp_group->cal_dev,
				"V validation failed\n");
		}

	}

	cirrus_cal_v_val_complete(amps, num_amps, separate);
	return 0;

err:
	return -1;
}

struct cirrus_cal_ops cirrus_cspl_cal_ops = {
	.controls = {
	 	{ "CAL_R", CIRRUS_AMP_ALG_ID_CSPL},
		{ "CAL_CHECKSUM", CIRRUS_AMP_ALG_ID_CSPL},
		{ "CAL_SET_STATUS", CIRRUS_AMP_ALG_ID_CSPL},
	},
	.cal_start = cirrus_cal_cspl_start,
	.cal_complete = cirrus_cal_cspl_complete,
	.v_val = cirrus_cal_cspl_v_val_start,
	.cal_apply = cirrus_cal_cspl_cal_apply,
	.read_temp = cirrus_cal_cspl_read_temp,
	.set_temp = cirrus_cal_cspl_set_surface_temp,
};