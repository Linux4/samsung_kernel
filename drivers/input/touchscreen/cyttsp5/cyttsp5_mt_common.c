/*
 * cyttsp5_mt_common.c
 * Cypress TrueTouch(TM) Standard Product V5 Multi-touch module.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp5_mt_common.h"

/*#define CYTTSP5_MT_DEBUG*/

static void cyttsp5_mt_lift_all(struct cyttsp5_mt_data *md)
{
	int max = md->si->tch_abs[CY_TCH_T].max;

	md->hover_active = false;

	if (md->num_prv_tch != 0) {
		if (md->mt_function.report_slot_liftoff)
			md->mt_function.report_slot_liftoff(md, max);
		input_sync(md->input);
		md->num_prv_tch = 0;
	}
}

static void cyttsp5_get_touch_axis(struct cyttsp5_mt_data *md,
	int *axis, int size, int max, u8 *xy_data, int bofs)
{
	int nbyte;
	int next;

	for (nbyte = 0, *axis = 0, next = 0; nbyte < size; nbyte++) {
#ifdef CYTTSP5_MT_DEBUG
		dev_vdbg(&md->ttsp->dev,
			"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
			" xy_data[%d]=%02X(%d) bofs=%d\n",
			__func__, *axis, *axis, size, max, xy_data, next,
			xy_data[next], xy_data[next], bofs);
#endif
		*axis = *axis + ((xy_data[next] >> bofs) << (nbyte * 8));
		next++;
	}

	*axis &= max - 1;
#ifdef CYTTSP5_MT_DEBUG
	dev_vdbg(&md->ttsp->dev,
		"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
		" xy_data[%d]=%02X(%d)\n",
		__func__, *axis, *axis, size, max, xy_data, next,
		xy_data[next], xy_data[next]);
#endif
}

static void cyttsp5_get_touch_hdr(struct cyttsp5_mt_data *md,
	struct cyttsp5_touch *touch, u8 *xy_mode)
{
#ifdef CYTTSP5_MT_DEBUG
	struct device *dev = &md->ttsp->dev;
#endif
	struct cyttsp5_sysinfo *si = md->si;
	enum cyttsp5_tch_hdr hdr;

	for (hdr = CY_TCH_TIME; hdr < CY_TCH_NUM_HDR; hdr++) {
		if (!si->tch_hdr[hdr].report)
			continue;
		cyttsp5_get_touch_axis(md, &touch->hdr[hdr],
			si->tch_hdr[hdr].size,
			si->tch_hdr[hdr].max,
			xy_mode + si->tch_hdr[hdr].ofs,
			si->tch_hdr[hdr].bofs);
#ifdef CYTTSP5_MT_DEBUG
		dev_vdbg(dev, "%s: get %s=%04X(%d)\n", __func__,
			cyttsp5_tch_hdr_string[hdr],
			touch->hdr[hdr], touch->hdr[hdr]);
#endif
	}
#ifdef CYTTSP5_MT_DEBUG
	dev_dbg(dev,
		"%s: time=%X tch_num=%d lo=%d noise=%d counter=%d\n",
		__func__,
		touch->hdr[CY_TCH_TIME],
		touch->hdr[CY_TCH_NUM],
		touch->hdr[CY_TCH_LO],
		touch->hdr[CY_TCH_NOISE],
		touch->hdr[CY_TCH_COUNTER]);
#endif
}

static void cyttsp5_get_touch(struct cyttsp5_mt_data *md,
	struct cyttsp5_touch *touch, u8 *xy_data)
{
#ifdef CYTTSP5_MT_DEBUG
	struct device *dev = &md->ttsp->dev;
#endif
	struct cyttsp5_sysinfo *si = md->si;
	enum cyttsp5_tch_abs abs;
	int tmp;
	bool flipped;

	for (abs = CY_TCH_X; abs < CY_TCH_NUM_ABS; abs++) {
		if (!si->tch_abs[abs].report)
			continue;
		cyttsp5_get_touch_axis(md, &touch->abs[abs],
			si->tch_abs[abs].size,
			si->tch_abs[abs].max,
			xy_data + si->tch_abs[abs].ofs,
			si->tch_abs[abs].bofs);
#ifdef CYTTSP5_MT_DEBUG
		dev_vdbg(dev, "%s: get %s=%04X(%d)\n", __func__,
			cyttsp5_tch_abs_string[abs],
			touch->abs[abs], touch->abs[abs]);
#endif
	}

	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		tmp = touch->abs[CY_TCH_X];
		touch->abs[CY_TCH_X] = touch->abs[CY_TCH_Y];
		touch->abs[CY_TCH_Y] = tmp;
		flipped = true;
	} else
		flipped = false;

	if (md->pdata->flags & CY_MT_FLAG_INV_X) {
		if (flipped)
			touch->abs[CY_TCH_X] = si->sensing_conf_data.res_y -
				touch->abs[CY_TCH_X];
		else
			touch->abs[CY_TCH_X] = si->sensing_conf_data.res_x -
				touch->abs[CY_TCH_X];
	}
	if (md->pdata->flags & CY_MT_FLAG_INV_Y) {
		if (flipped)
			touch->abs[CY_TCH_Y] = si->sensing_conf_data.res_x -
				touch->abs[CY_TCH_Y];
		else
			touch->abs[CY_TCH_Y] = si->sensing_conf_data.res_y -
				touch->abs[CY_TCH_Y];
	}
#ifdef CYTTSP5_MT_DEBUG
	dev_vdbg(dev, "%s: flip=%s inv-x=%s inv-y=%s x=%04X(%d) y=%04X(%d)\n",
		__func__, flipped ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_X ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_Y ? "true" : "false",
		touch->abs[CY_TCH_X], touch->abs[CY_TCH_X],
		touch->abs[CY_TCH_Y], touch->abs[CY_TCH_Y]);
#endif
}

static void cyttsp5_get_mt_touches(struct cyttsp5_mt_data *md,
		struct cyttsp5_touch *tch, int num_cur_tch)
{
	struct device *dev = &md->ttsp->dev;
	struct cyttsp5_sysinfo *si = md->si;
	int sig;
	int i, j, t = 0;
	DECLARE_BITMAP(ids, MAX_TOUCH_NUMBER);
	int mt_sync_count = 0;

	bitmap_zero(ids, MAX_TOUCH_NUMBER);
	memset(tch->abs, 0, sizeof(tch->abs));

	for (i = 0; i < num_cur_tch; i++) {
		cyttsp5_get_touch(md, tch, si->xy_data +
			(i * si->desc.tch_record_size));
		if ((tch->abs[CY_TCH_T] < md->pdata->frmwrk->abs
			[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MIN_OST]) ||
			(tch->abs[CY_TCH_T] > md->pdata->frmwrk->abs
			[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MAX_OST])) {
			dev_err(dev, "%s: tch=%d -> bad trk_id=%d max_id=%d\n",
				__func__, i, tch->abs[CY_TCH_T],
				md->pdata->frmwrk->abs[(CY_ABS_ID_OST *
				CY_NUM_ABS_SET) + CY_MAX_OST]);
			if (md->mt_function.input_sync)
				md->mt_function.input_sync(md->input);
			mt_sync_count++;
			continue;
		}

		/* use 0 based track id's */
		sig = md->pdata->frmwrk->abs
			[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + 0];
		if (sig != CY_IGNORE_VALUE) {
			t = tch->abs[CY_TCH_T] - md->pdata->frmwrk->abs
				[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MIN_OST];

			if (md->hover_active) {
				dev_dbg(dev, "%s: Hover active, ID:%d, t:%d\n",
					__func__, md->hover_tch_id, t);

				if (md->hover_tch_id == t) {
					dev_dbg(dev, "%s: Object:%1d Event:%1d\n",
						__func__, tch->abs[CY_TCH_O],
						tch->abs[CY_TCH_E]);
					/* Check if touch type changed for touch id
					 * which has been tracked for hover
					 */
					if (tch->abs[CY_TCH_O] != CY_OBJ_HOVER) {
						dev_vdbg(dev, "%s: Reporting Lift-off for hover\n",
							__func__);
						md->hover_active = false;
						/* Report lift-off for hover */
						md->mt_function.report_liftoff(md, t);
					} else if (tch->abs[CY_TCH_E] == CY_EV_LIFTOFF) {
						dev_vdbg(dev, "%s: Hover lift-off\n",
							__func__);
						md->hover_active = false;
					}
				}
			} else if (tch->abs[CY_TCH_O] == CY_OBJ_HOVER
					&& tch->abs[CY_TCH_E] != CY_EV_LIFTOFF) {
				dev_vdbg(dev, "%s: Hover started\n", __func__);
				md->hover_active = true;
				md->hover_tch_id = t;
			}

			if (tch->abs[CY_TCH_E] == CY_EV_LIFTOFF) {
#ifdef CYTTSP5_MT_DEBUG
				dev_dbg(dev, "%s: t=%d e=%d lift-off\n",
					__func__, t, tch->abs[CY_TCH_E]);
#endif
				goto cyttsp5_get_mt_touches_pr_tch;
			}
			if (md->mt_function.input_report)
				md->mt_function.input_report(md->input, sig,
						t, tch->abs[CY_TCH_O]);
			__set_bit(t, ids);
		}

		/* all devices: position and pressure fields */
		for (j = 0; j <= CY_ABS_W_OST ; j++) {
			if (!si->tch_abs[j].report)
				continue;
			sig = md->pdata->frmwrk->abs[((CY_ABS_X_OST + j) *
				CY_NUM_ABS_SET) + 0];
			/* Special handling of pressure when object type is hover */
			if (j == CY_ABS_P_OST) {
				if (tch->abs[CY_TCH_O] == CY_OBJ_HOVER) {
					/* Report ABS_MT_DISTANCE */
					input_report_abs(md->input,
						ABS_MT_DISTANCE,
						md->si->sensing_conf_data.max_z
						- tch->abs[CY_TCH_P]);
					/* Set pressure as zero */
					tch->abs[CY_TCH_P] = 0;
				} else {
					/* Report ABS_MT_DISTANCE as zero */
					input_report_abs(md->input,
						ABS_MT_DISTANCE, 0);
				}
			}
			if (sig != CY_IGNORE_VALUE)
				input_report_abs(md->input, sig,
					tch->abs[CY_TCH_X + j]);
		}

		if (tch->abs[CY_TCH_P] > 0 && tch->abs[CY_TCH_MAJ] == 0)
			tch->abs[CY_TCH_MAJ] = tch->abs[CY_TCH_MIN] = 1;

		/* Get the extended touch fields */
		for (j = 0; j < CY_NUM_EXT_TCH_FIELDS; j++) {
			if (!si->tch_abs[j].report)
				continue;
			sig = md->pdata->frmwrk->abs
				[((CY_ABS_MAJ_OST + j) *
				CY_NUM_ABS_SET) + 0];
			if (sig != CY_IGNORE_VALUE)
				input_report_abs(md->input, sig,
					tch->abs[CY_TCH_MAJ + j]);
		}
		if (md->mt_function.input_sync)
			md->mt_function.input_sync(md->input);
		mt_sync_count++;

cyttsp5_get_mt_touches_pr_tch:
#ifdef CYTTSP5_MT_DEBUG
		dev_dbg(dev,
			"%s: t=%d x=%d y=%d z=%d M=%d m=%d o=%d e=%d obj=%d tip=%d\n",
			__func__, t,
			tch->abs[CY_TCH_X],
			tch->abs[CY_TCH_Y],
			tch->abs[CY_TCH_P],
			tch->abs[CY_TCH_MAJ],
			tch->abs[CY_TCH_MIN],
			tch->abs[CY_TCH_OR],
			tch->abs[CY_TCH_E]2,
			tch->abs[CY_TCH_O],
			tch->abs[CY_TCH_TIP]);
#else
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		if (tch->abs[CY_TCH_E] == CY_EV_TOUCHDOWN)
			dev_dbg(dev, "P [%d]\n", t);
		else if (tch->abs[CY_TCH_E] == CY_EV_LIFTOFF)
			dev_dbg(dev, "R [%d]\n", t);
#else
		if (tch->abs[CY_TCH_E] == CY_EV_TOUCHDOWN)
			dev_dbg(dev, "P [%d] x=%d y=%d z=%d M=%d m=%d\n",
				t, tch->abs[CY_TCH_X],
				tch->abs[CY_TCH_Y],
				tch->abs[CY_TCH_P],
				tch->abs[CY_TCH_MAJ],
				tch->abs[CY_TCH_MIN]);
		else if (tch->abs[CY_TCH_E] == CY_EV_LIFTOFF)
			dev_dbg(dev, "R [%d] x=%d y=%d z=%d M=%d m=%d\n",
				t, tch->abs[CY_TCH_X],
				tch->abs[CY_TCH_Y],
				tch->abs[CY_TCH_P],
				tch->abs[CY_TCH_MAJ],
				tch->abs[CY_TCH_MIN]);
#endif
#endif
	}

	if (md->mt_function.final_sync)
		md->mt_function.final_sync(md->input, MAX_TOUCH_NUMBER,
				si->tch_abs[CY_TCH_T].max, ids);

	md->num_prv_tch = num_cur_tch;

	return;
}

/* read xy_data for all current touches */
static int cyttsp5_xy_worker(struct cyttsp5_mt_data *md)
{
	struct device *dev = &md->ttsp->dev;
	struct cyttsp5_sysinfo *si = md->si;
	struct cyttsp5_touch tch;
	u8 num_cur_tch;
	int rc = 0;

	cyttsp5_get_touch_hdr(md, &tch, si->xy_mode + 3);

	num_cur_tch = tch.hdr[CY_TCH_NUM];
	if (num_cur_tch > MAX_TOUCH_NUMBER) {
		dev_err(dev, "%s: Num touch err detected (n=%d)\n",
			__func__, num_cur_tch);
		num_cur_tch = MAX_TOUCH_NUMBER;
	}

	if (tch.hdr[CY_TCH_LO]) {
		dev_dbg(dev, "%s: Large area detected\n", __func__);
		if (md->pdata->flags & CY_MT_FLAG_NO_TOUCH_ON_LO)
			num_cur_tch = 0;
	}

	/* extract xy_data for all currently reported touches */
#ifdef CYTTSP5_MT_DEBUG
	dev_vdbg(dev, "%s: extract data num_cur_tch=%d\n", __func__,
		num_cur_tch);
#endif
	if (num_cur_tch)
		cyttsp5_get_mt_touches(md, &tch, num_cur_tch);
	else
		cyttsp5_mt_lift_all(md);

	rc = 0;

	return rc;
}

static int cyttsp5_mt_attention(struct cyttsp5_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp5_mt_data *md = dev_get_drvdata(dev);
	int rc = 0;

	if (md->si->xy_mode[2] !=  md->si->desc.tch_report_id)
		return 0;

	/* core handles handshake */
	mutex_lock(&md->mt_lock);
	rc = cyttsp5_xy_worker(md);
	mutex_unlock(&md->mt_lock);
	if (rc < 0)
		dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp5_startup_attention(struct cyttsp5_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp5_mt_data *md = dev_get_drvdata(dev);
	int rc = 0;

	mutex_lock(&md->mt_lock);
	cyttsp5_mt_lift_all(md);
	mutex_unlock(&md->mt_lock);

	return rc;
}

static int cyttsp5_mt_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp5_device *ttsp =
		container_of(dev, struct cyttsp5_device, dev);

	pm_runtime_get_sync(dev);

	dev_vdbg(dev, "%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	cyttsp5_subscribe_attention(ttsp, CY_ATTEN_IRQ,
		cyttsp5_mt_attention, CY_MODE_OPERATIONAL);

	/* set up startup call back */
	cyttsp5_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp5_startup_attention, 0);

	return 0;
}

static void cyttsp5_mt_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp5_device *ttsp =
		container_of(dev, struct cyttsp5_device, dev);

	cyttsp5_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
		cyttsp5_mt_attention, CY_MODE_OPERATIONAL);

	cyttsp5_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp5_startup_attention, 0);

	pm_runtime_put(dev);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp5_mt_early_suspend(struct early_suspend *h)
{
	struct cyttsp5_mt_data *md =
		container_of(h, struct cyttsp5_mt_data, es);
	struct device *dev = &md->ttsp->dev;

	pm_runtime_put_sync(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = true;
	mutex_unlock(&md->mt_lock);
}

static void cyttsp5_mt_late_resume(struct early_suspend *h)
{
	struct cyttsp5_mt_data *md =
		container_of(h, struct cyttsp5_mt_data, es);
	struct device *dev = &md->ttsp->dev;

	pm_runtime_get(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = false;
	mutex_unlock(&md->mt_lock);
}

static void cyttsp5_setup_early_suspend(struct cyttsp5_mt_data *md)
{
	md->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	md->es.suspend = cyttsp5_mt_early_suspend;
	md->es.resume = cyttsp5_mt_late_resume;

	register_early_suspend(&md->es);
}
#endif

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int cyttsp5_mt_suspend(struct device *dev)
{
	struct cyttsp5_mt_data *md = dev_get_drvdata(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = true;
	if (md->si)
		cyttsp5_mt_lift_all(md);
	mutex_unlock(&md->mt_lock);
	return 0;
}

static int cyttsp5_mt_resume(struct device *dev)
{
	struct cyttsp5_mt_data *md = dev_get_drvdata(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = false;
	mutex_unlock(&md->mt_lock);
	return 0;
}
#endif

static const struct dev_pm_ops cyttsp5_mt_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cyttsp5_mt_suspend, cyttsp5_mt_resume)
	SET_RUNTIME_PM_OPS(cyttsp5_mt_suspend, cyttsp5_mt_resume, NULL)
};

static int cyttsp5_setup_input_device(struct cyttsp5_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp5_mt_data *md = dev_get_drvdata(dev);
	int signal = CY_IGNORE_VALUE;
	int max_x, max_y, max_p, min, max;
	int max_x_tmp, max_y_tmp;
	int i;
	int rc;

	dev_vdbg(dev, "%s: Initialize event signals\n", __func__);
	__set_bit(EV_ABS, md->input->evbit);
	__set_bit(EV_REL, md->input->evbit);
	__set_bit(EV_KEY, md->input->evbit);
#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, md->input->propbit);
#endif

	/* If virtualkeys enabled, don't use all screen */
	if (md->pdata->flags & CY_MT_FLAG_VKEYS) {
		max_x_tmp = md->pdata->vkeys_x;
		max_y_tmp = md->pdata->vkeys_y;
	} else {
		max_x_tmp = md->si->sensing_conf_data.res_x;
		max_y_tmp = md->si->sensing_conf_data.res_y;
	}

	/* get maximum values from the sysinfo data */
	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		max_x = max_y_tmp - 1;
		max_y = max_x_tmp - 1;
	} else {
		max_x = max_x_tmp - 1;
		max_y = max_y_tmp - 1;
	}
	max_p = md->si->sensing_conf_data.max_z;

	/* set event signal capabilities */
	for (i = 0; i < (md->pdata->frmwrk->size / CY_NUM_ABS_SET); i++) {
		signal = md->pdata->frmwrk->abs
			[(i * CY_NUM_ABS_SET) + CY_SIGNAL_OST];
		if (signal != CY_IGNORE_VALUE) {
			__set_bit(signal, md->input->absbit);
			min = md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_MIN_OST];
			max = md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_MAX_OST];
			if (i == CY_ABS_ID_OST) {
				/* shift track ids down to start at 0 */
				max = max - min;
				min = min - min;
			} else if (i == CY_ABS_X_OST)
				max = max_x;
			else if (i == CY_ABS_Y_OST)
				max = max_y;
			else if (i == CY_ABS_P_OST) {
				max = max_p;
				/* Register ABS_MT_DISTANCE for hover */
				input_set_abs_params(md->input,
					ABS_MT_DISTANCE, 0, max, 0, 0);
				dev_dbg(dev, "%s: register signal=%02X min=%d max=%d\n",
					__func__, ABS_MT_DISTANCE, 0, max);
			}
			input_set_abs_params(md->input, signal, min, max,
				md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_FUZZ_OST],
				md->pdata->frmwrk->abs
				[(i * CY_NUM_ABS_SET) + CY_FLAT_OST]);
			dev_dbg(dev, "%s: register signal=%02X min=%d max=%d\n",
				__func__, signal, min, max);
		}
	}

	rc = md->mt_function.input_register_device(md->input,
			md->si->tch_abs[CY_TCH_T].max);
	if (rc < 0)
		dev_err(dev, "%s: Error, failed register input device r=%d\n",
			__func__, rc);
	else
		md->input_device_registered = true;

	return rc;
}

static int cyttsp5_setup_input_attention(struct cyttsp5_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp5_mt_data *md = dev_get_drvdata(dev);
	int rc = 0;

	md->si = cyttsp5_request_sysinfo(ttsp);
	if (!md->si)
		return -1;

	rc = cyttsp5_setup_input_device(ttsp);

	cyttsp5_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp5_setup_input_attention, 0);

	return rc;
}

static int cyttsp5_mt_release(struct cyttsp5_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp5_mt_data *md = dev_get_drvdata(dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	/*
	 * This check is to prevent pm_runtime usage_count drop below zero
	 * because of removing the module while in suspended state
	 */
	if (md->is_suspended)
		pm_runtime_get_noresume(dev);

	unregister_early_suspend(&md->es);
#endif

	if (md->input_device_registered) {
		input_unregister_device(md->input);
	} else {
		input_free_device(md->input);
		cyttsp5_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
			cyttsp5_setup_input_attention, 0);
	}

	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

	dev_set_drvdata(dev, NULL);
	kfree(md);
	return 0;
}

static int cyttsp5_mt_probe(struct cyttsp5_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp5_mt_data *md;
	struct cyttsp5_mt_platform_data *pdata = dev_get_platdata(dev);
	int rc = 0;

	if (pdata == NULL) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (md == NULL) {
		dev_err(dev, "%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	cyttsp5_init_function_ptrs(md);

	mutex_init(&md->mt_lock);
	md->ttsp = ttsp;
	md->pdata = pdata;
	dev_set_drvdata(dev, md);
	/* Create the input device and register it. */
	dev_vdbg(dev, "%s: Create the input device and register it\n",
		__func__);
	md->input = input_allocate_device();
	if (md->input == NULL) {
		dev_err(dev, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	md->input->name = (md->pdata->inp_dev_name) ?
			md->pdata->inp_dev_name : ttsp->name;
	scnprintf(md->phys, sizeof(md->phys)-1, "%s", dev_name(dev));
	md->input->phys = md->phys;
	md->input->dev.parent = &md->ttsp->dev;
	md->input->open = cyttsp5_mt_open;
	md->input->close = cyttsp5_mt_close;
	input_set_drvdata(md->input, md);

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	/* get sysinfo */
	md->si = cyttsp5_request_sysinfo(ttsp);
	pm_runtime_put(dev);

	if (md->si) {
		rc = cyttsp5_setup_input_device(ttsp);
		if (rc)
			goto error_init_input;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, md->si);
		cyttsp5_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
			cyttsp5_setup_input_attention, 0);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	cyttsp5_setup_early_suspend(md);
#endif

	return 0;

error_init_input:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	input_free_device(md->input);
error_alloc_failed:
	dev_set_drvdata(dev, NULL);
	kfree(md);
error_alloc_data_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);
	return rc;
}

struct cyttsp5_driver cyttsp5_mt_driver = {
	.probe = cyttsp5_mt_probe,
	.remove = cyttsp5_mt_release,
	.driver = {
		.name = CYTTSP5_MT_NAME,
		.bus = &cyttsp5_bus_type,
		/*.pm = &cyttsp5_mt_pm_ops,*/
	},
};

