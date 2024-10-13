/*
 * Calibration support for CS35L43 Cirrus Logic Smart Amplifier
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
#include <linux/pm_runtime.h>

#include <sound/cirrus/core.h>
#include <sound/cirrus/calibration.h>

#include "wm_adsp.h"
#include "wmfw.h"

#define CIRRUS_CAL_COMPLETE_DELAY_MS	1250
#define CIRRUS_CAL_RETRIES		2
#define CIRRUS_CAL_AMBIENT_DEFAULT	23

#define CIRRUS_CAL_CS35L43_ALG_ID_PROT		0x5f210
#define CIRRUS_CAL_CS35L43_ALG_ID_SURFACE	0x5f222

#define CS35L43_CAL_MBOX_CMD_AUDIO_REINIT	0x0B000003

#define CIRRUS_CAL_CS35L43_TEMP_RADIX	14
#define CIRRUS_CAL_CS35L43_SURFACE_TEMP_RADIX	16

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

static int cirrus_cal_cs35l43_start(void);

static void cirrus_cal_cs35l43_complete(void)
{
	struct cirrus_amp *amp;
	struct reg_sequence *post_config;
	struct regmap *regmap;
	unsigned long long ohms;
	unsigned int mbox_cmd, mbox_sts;
	int rdc, status, checksum, temp, cal_state, vsc, isc, i;
	int delay = msecs_to_jiffies(CIRRUS_CAL_COMPLETE_DELAY_MS);
	bool vsc_in_range, isc_in_range;
	bool cal_retry = false;


	dev_info(amp_group->cal_dev, "Complete Calibration\n");

	mutex_lock(&amp_group->cal_lock);

	for (i = 0; i < amp_group->num_amps; i++) {
		amp = &amp_group->amps[i];
		if (amp->calibration_disable)
			continue;

		regmap = amp->regmap;
		post_config = amp->post_config;
		mbox_cmd = amp->mbox_cmd;
		mbox_sts = amp->mbox_sts;

		cirrus_amp_read_ctl(amp, "CAL_STATUS", WMFW_ADSP2_XM,
					CIRRUS_CAL_CS35L43_ALG_ID_PROT, &status);
		cirrus_amp_read_ctl(amp, "CAL_R", WMFW_ADSP2_XM,
					CIRRUS_CAL_CS35L43_ALG_ID_PROT, &rdc);
		cirrus_amp_read_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
					CIRRUS_CAL_CS35L43_ALG_ID_PROT, &temp);
		cirrus_amp_read_ctl(amp, "CAL_CHECKSUM", WMFW_ADSP2_XM,
					CIRRUS_CAL_CS35L43_ALG_ID_PROT, &checksum);

		ohms = cirrus_cal_rdc_to_ohms((unsigned long)rdc);

		if (amp->perform_vimon_cal) {
			cirrus_amp_read_ctl(amp, "VIMON_CAL_VSC", WMFW_ADSP2_XM,
						amp->vimon_alg_id, &vsc);
			cirrus_amp_read_ctl(amp, "VIMON_CAL_ISC", WMFW_ADSP2_XM,
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

		cirrus_amp_write_ctl(amp, "CAL_EN", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0);
		cirrus_amp_write_ctl(amp, "PILOT_THRESH", WMFW_ADSP2_XM,
					 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0x611);
		cirrus_amp_write_ctl(amp, "CAL_R_SEL", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0xFFFFFF);

		regmap_write(amp->regmap, amp->mbox_cmd, CS35L43_CAL_MBOX_CMD_AUDIO_REINIT);

		regmap_multi_reg_write(regmap, post_config,
					   amp->num_post_configs);

		amp->cal.efs_cache_rdc = rdc;
		amp->cal.efs_cache_vsc = vsc;
		amp->cal.efs_cache_isc = isc;
		amp_group->efs_cache_temp = temp;
		amp->cal.efs_cache_valid = 1;
	}

	if (cal_retry == true) {
		cirrus_cal_cs35l43_start();
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


static void cirrus_cal_cs35l43_v_val_complete(struct cirrus_amp *amps, int num_amps,
					bool separate)
{
	struct regmap *regmap;
	struct reg_sequence *post_config;
	const char *dsp_part_name;
	unsigned int mbox_cmd, mbox_sts;
	int amp;


	dev_info(amp_group->cal_dev, "Complete v-val\n");

	for (amp = 0; amp < num_amps; amp++) {
		if (amps[amp].v_val_separate && !separate)
			continue;

		regmap = amps[amp].regmap;
		dsp_part_name = amps[amp].dsp_part_name;
		post_config = amps[amp].post_config;
		mbox_cmd = amps[amp].mbox_cmd;
		mbox_sts = amps[amp].mbox_sts;

		cirrus_amp_write_ctl(&amps[amp], "CAL_EN", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0);
		cirrus_amp_write_ctl(&amps[amp], "PILOT_THRESH", WMFW_ADSP2_XM,
					 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0x611);
		cirrus_amp_write_ctl(&amps[amp], "CAL_R_SEL", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0xFFFFFF);

		regmap_write(regmap, mbox_cmd, CS35L43_CAL_MBOX_CMD_AUDIO_REINIT);

		regmap_multi_reg_write(regmap, post_config,
					   amps[amp].num_post_configs);
	}

	dev_info(amp_group->cal_dev, "V validation complete\n");
}


static void cirrus_cal_cs35l43_vimon_cal_start(struct cirrus_amp *amp)
{

	cirrus_amp_write_ctl(amp, "VIMON_CLS_H_DELY", WMFW_ADSP2_XM,
				 amp->vimon_alg_id, CIRRUS_CAL_CLASSH_DELAY_50MS);
	cirrus_amp_write_ctl(amp, "VIMON_CLS_D_DELY", WMFW_ADSP2_XM,
				 amp->vimon_alg_id, CIRRUS_CAL_CLASSD_DELAY_50MS);

	/* CS35L43_DSP1RX3_INPUT = CS35L43_INPUT_SRC_VBSTMON */
	regmap_write(amp->regmap, 0x0004C48, 0x29);

	cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
				 amp->vimon_alg_id, 0);

	regmap_write(amp->regmap, amp->mbox_cmd, CS35L43_CAL_MBOX_CMD_AUDIO_REINIT);
}

static int cirrus_cal_cs35l43_vimon_cal_complete(struct cirrus_amp *amp)
{
	unsigned int vimon_cal, vsc, isc;
	bool vsc_in_range, isc_in_range;

	cirrus_amp_read_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
				amp->vimon_alg_id, &vimon_cal);
	cirrus_amp_read_ctl(amp, "VIMON_CAL_VSC", WMFW_ADSP2_XM, amp->vimon_alg_id, &vsc);
	cirrus_amp_read_ctl(amp, "VIMON_CAL_ISC", WMFW_ADSP2_XM, amp->vimon_alg_id, &isc);

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
	int timeout = 50;

	regmap_read(regmap, amp->global_en, &global_en);

	while ((global_en & amp->global_en_mask) == 0) {
		usleep_range(1000, 1500);
		regmap_read(regmap, amp->global_en, &global_en);
	}

	if (timeout == 0) {
		dev_err(amp_group->cal_dev, "Failed to setup calibration\n");
		return -EINVAL;
	}

	return 0;
}

static void cirrus_cal_cs35l43_redc_start(struct cirrus_amp *amp)
{
	int ambient;

	dev_info(amp_group->cal_dev, "ReDC Calibration\n");

	cirrus_amp_write_ctl(amp, "CAL_EN", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 1);
	cirrus_amp_write_ctl(amp, "PILOT_THRESH", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0);

	cirrus_amp_write_ctl(amp, "CALIB_FIRST_RUN", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, 1);

	ambient = cirrus_cal_get_power_temp();
	cirrus_amp_write_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, ambient);

	regmap_write(amp->regmap, amp->mbox_cmd, CS35L43_CAL_MBOX_CMD_AUDIO_REINIT);

}

int cirrus_cal_cs35l43_cal_apply(struct cirrus_amp *amp)
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

	cirrus_amp_write_ctl(amp, "CAL_R_SEL", WMFW_ADSP2_XM,
				 CIRRUS_CAL_CS35L43_ALG_ID_PROT, rdc);

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
		cirrus_amp_write_ctl(amp, "VIMON_CAL_VSC", WMFW_ADSP2_XM,
					 amp->vimon_alg_id, vsc);
		cirrus_amp_write_ctl(amp, "VIMON_CAL_ISC", WMFW_ADSP2_XM,
					 amp->vimon_alg_id, isc);
	} else {
		dev_info(amp_group->cal_dev, "VIMON Cal status invalid\n");
	}

skip_vimon_cal:
	return ret;
}

int cirrus_cal_cs35l43_read_temp(struct cirrus_amp *amp)
{
	int reg = 0, ret;
	struct wm_adsp *adsp;
	unsigned int global_en;
	unsigned int halo_alg_id;

	if (!amp)
		goto err;

	if (pm_runtime_enabled(amp->component->dev) &&
			pm_runtime_status_suspended(amp->component->dev))
		goto err;

	regmap_read(amp->regmap, amp->global_en, &global_en);

	if ((global_en & amp->global_en_mask) == 0)
		goto err;

	if (amp_group->cal_running)
		goto err;

	adsp = snd_soc_component_get_drvdata(amp->component);
	halo_alg_id = adsp->fw_id;

	ret = cirrus_amp_read_ctl(amp, "HALO_HEARTBEAT", WMFW_ADSP2_XM,
			halo_alg_id, &reg);

	if (reg == 0)
		goto err;

	ret = cirrus_amp_read_ctl(amp, "AUDIO_STATE", WMFW_ADSP2_XM,
			0x5f212, &reg);

	if (reg == 0)
		goto err;

	ret = cirrus_amp_read_ctl(amp, "TEMP_COIL_C", WMFW_ADSP2_XM,
			CIRRUS_CAL_CS35L43_ALG_ID_PROT, &reg);

	reg = cirrus_cal_sign_extend(reg);

	if (ret == 0) {
		dev_info(amp_group->cal_dev,
			"Read temp: %d.%04d degrees C\n",
			reg >> CIRRUS_CAL_CS35L43_TEMP_RADIX,
			(reg & (((1 << CIRRUS_CAL_CS35L43_TEMP_RADIX) - 1))) *
			10000 / (1 << CIRRUS_CAL_CS35L43_TEMP_RADIX));
		return (reg >> CIRRUS_CAL_CS35L43_TEMP_RADIX);
	}
err:
	return -1;
}

int cirrus_cal_cs35l43_set_surface_temp(struct cirrus_amp *amp, int temperature)
{
	unsigned int global_en;

	if (!amp)
		return -EINVAL;

	if (pm_runtime_enabled(amp->component->dev) &&
		pm_runtime_status_suspended(amp->component->dev))
		return -EINVAL;

	regmap_read(amp->regmap, amp->global_en, &global_en);

	if ((global_en & amp->global_en_mask) == 0)
		return -EINVAL;

	if (temperature < 0) {
		dev_info(amp->component->dev, "Input surface temp: %d degrees\n", temperature);
		temperature = 0;
	}

	dev_info(amp->component->dev, "Set surface temp: %d degrees\n", temperature);
	cirrus_amp_write_ctl(amp, "SURFACE_TEMP", WMFW_ADSP2_XM,
				CIRRUS_CAL_CS35L43_ALG_ID_SURFACE,
				temperature <<  CIRRUS_CAL_CS35L43_SURFACE_TEMP_RADIX);
	return 0;
}

static int cirrus_cal_cs35l43_start(void)
{
	bool vimon_calibration_failed = false;
	int amp;
	struct reg_sequence *config;
	struct regmap *regmap;
	int ret, vimon_cal_retries = 5;

	for (amp = 0; amp < amp_group->num_amps; amp++) {
		if (amp_group->amps[amp].calibration_disable)
			continue;

		regmap = amp_group->amps[amp].regmap;

		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_STATUS",
			WMFW_ADSP2_XM, CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_R",
			WMFW_ADSP2_XM, CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_AMBIENT",
			WMFW_ADSP2_XM, CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_CHECKSUM",
			WMFW_ADSP2_XM, CIRRUS_CAL_CS35L43_ALG_ID_PROT, 0);
		if (amp_group->amps[amp].perform_vimon_cal) {
			cirrus_amp_write_ctl(&amp_group->amps[amp], "VIMON_CAL_VSC",
				WMFW_ADSP2_XM,
				amp_group->amps[amp].vimon_alg_id, 0);
			cirrus_amp_write_ctl(&amp_group->amps[amp], "VIMON_CAL_ISC",
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
				cirrus_cal_cs35l43_vimon_cal_start(&amp_group->amps[amp]);
		}


		msleep(300);

		for (amp = 0; amp < amp_group->num_amps; amp++) {
			if (amp_group->amps[amp].calibration_disable)
				continue;

			if (amp_group->amps[amp].perform_vimon_cal) {
				ret = cirrus_cal_cs35l43_vimon_cal_complete(
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

		cirrus_cal_cs35l43_redc_start(&amp_group->amps[amp]);
	}

	return 0;
}

int cirrus_cal_cs35l43_v_val_start(struct cirrus_amp *amps, int num_amps, bool separate)
{
	struct reg_sequence *config;
	struct regmap *regmap;
	unsigned int vmax[CIRRUS_MAX_AMPS];
	unsigned int vmin[CIRRUS_MAX_AMPS];
	int ret = 0, i, j, reg, count;

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
		cirrus_cal_cs35l43_redc_start(&amps[i]);

	}

	dev_info(amp_group->cal_dev, "V validation prepare complete\n");

	for (i = 0; i < 1000; i++) {
		count = 0;
		for (j = 0; j < num_amps; j++) {
			if (amps[j].v_val_separate && !separate)
				continue;

			regmap = amps[j].regmap;

			cirrus_amp_read_ctl(&amps[j], "CAL_VOLTAGE_PEAK", WMFW_ADSP2_XM,
				CIRRUS_CAL_CS35L43_ALG_ID_PROT, &reg);
			if (reg > vmax[j])
				vmax[j] = reg;
			if (reg < vmin[j])
				vmin[j] = reg;

			cirrus_amp_read_ctl(&amps[j], "CAL_STATUS",
					WMFW_ADSP2_XM,
					CIRRUS_CAL_CS35L43_ALG_ID_PROT, &reg);

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

	cirrus_cal_cs35l43_v_val_complete(amps, num_amps, separate);
	return 0;

err:
	return -1;
}

struct cirrus_cal_ops cirrus_cs35l43_cal_ops = {
	.controls = {
	 	{ "CAL_R", CIRRUS_CAL_CS35L43_ALG_ID_PROT},
		{ "CAL_CHECKSUM", CIRRUS_CAL_CS35L43_ALG_ID_PROT},
		{ "CAL_R_INIT", CIRRUS_CAL_CS35L43_ALG_ID_PROT},
	},
	.cal_start = cirrus_cal_cs35l43_start,
	.cal_complete = cirrus_cal_cs35l43_complete,
	.v_val = cirrus_cal_cs35l43_v_val_start,
	.cal_apply = cirrus_cal_cs35l43_cal_apply,
	.read_temp = cirrus_cal_cs35l43_read_temp,
	.set_temp = cirrus_cal_cs35l43_set_surface_temp,
};