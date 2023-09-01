/*
 * Copyright (C) 2018 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "tfa9xxx.h"
#include "tfa2_dev.h"
#include "tfa2_haptic.h"
#if defined(TFA_USE_GPIO_FOR_MCLK)
#include <linux/gpio.h>
#endif

#define TFA_HAP_MAX_TIMEOUT (0x7fffff)
#define DEFAULT_PLL_OFF_DELAY 100 /* msec */
#define TIMEOUT 10000
#define MAX_INTENSITY 10000
#define NOMINAL_DURATION 1000

#define USE_SET_TIMER_IN_WORK
#define USE_IMMEDIATE_STOP
#undef USE_SERIALIZED_START
#define USE_EXCLUSIVE_SESSION
#define USE_HAPTIC_WORKQUEUE

static const char *haptic_feature_id_str[]
	= {"wave", "sine", "silence", "illegal"};

#define OFFSET_SET_INDEX	10
#define MAX_NUM_SET_INDEX	70
#define DEFAULT_TONE_SET_INDEX	1
#define SILENCE_SET_INDEX	6
#define LOW_TEMP_INDEX	34 /* special index for low temperature */
#define LOW_TEMP_SET_INDEX	11
static const int set_index_tbl[MAX_NUM_SET_INDEX] = {
	 2,  3, 14, 15, 16,  4, 18, 19, 20, 21,
	22, 23, 24,  2,  2,  6,  6,  6,  6,  6,
	 6,  2,  2,  2,  6,  2,  6,  6,  6,  6,
	 6,  2,  2,  2,  6,  6,  2,  2,  2,  2,
	 2,  2,  2,  2,  2, 16,  6,  6,  2,  2,
	 2,  2,  6,  6,  6, 26, 27, 28, 29,  6,
	 6,  6,  6,  6,  6,  6,  6,  6,  6,  6
};

static const char *haptic_feature_id(int id)
{
	if (id > 3)
		id = 3;

	return haptic_feature_id_str[id];
}

static void haptic_dump_obj(struct seq_file *f, struct haptic_tone_object *o)
{
	switch (o->type) {
	case OBJECT_WAVE:
		seq_printf(f, "\t type: %s\n"
			"\t offset: %d\n"
			"\t level: %d%%\n"
			"\t duration_cnt_max: %d (%dms)\n"
			"\t up_samp_sel: %d\n",
			haptic_feature_id(o->type),
			o->freq,
			/* Q1.23, percentage of max */
			(100 * o->level) / 0x7fffff,
			o->duration_cnt_max,
			/* sapmples in time : 1/FS ms */
			o->duration_cnt_max / 48,
			o->boost_brake_on);
		break;
	case OBJECT_TONE:
		seq_printf(f, "\t type: %s\n"
			"\t freq: %dHz\n"
			"\t level: %d%%\n"
			"\t duration_cnt_max: %dms\n"
			"\t boost_brake_on: %d\n"
			"\t tracker_on: %d\n"
			"\t boost_length: %d\n",
			haptic_feature_id(o->type),
			o->freq >> 11, /* Q13.11 */
			/* Q1.23, percentage of max */
			(100 * o->level) / 0x7fffff,
			/* sapmples in time : 1/FS ms */
			o->duration_cnt_max / 48,
			o->boost_brake_on,
			o->tracker_on,
			o->boost_length);
		break;
	case OBJECT_SILENCE:
		seq_printf(f, "\t type: %s\n"
			"\t duration_cnt_max: %dms\n",
			haptic_feature_id(o->type),
			/* sapmples in time : 1/FS ms */
			o->duration_cnt_max / 48);
		break;
	default:
		seq_printf(f, "wrong feature id (%d) in object!\n",
			o->type);
		break;
	}
}

int tfa9xxx_haptic_dump_objs(struct seq_file *f, void *p)
{
	struct tfa9xxx *drv = f->private;
	int i, offset;

	offset = 1;

	for (i = 0; i < FW_XMEM_NR_OBJECTS1; i++) {
		struct haptic_tone_object *o
			= (struct haptic_tone_object *)
			&drv->tfa->hap_data.object_table1_cache[i][0];

		if (o->duration_cnt_max == 0) /* assume object is empty */
			continue;

		seq_printf(f, "object[%d]---------\n", offset + i);
		haptic_dump_obj(f, o);
	}

	offset += i;

	for (i = 0; i < FW_XMEM_NR_OBJECTS2; i++) {
		struct haptic_tone_object *o
			= (struct haptic_tone_object *)
			&drv->tfa->hap_data.object_table2_cache[i][0];

		if (o->duration_cnt_max == 0) /* assume object is empty */
			continue;

		seq_printf(f, "object[%d]---------\n", offset + i);
		haptic_dump_obj(f, o);
	}

	seq_printf(f, "\ndelay_attack: %dms\n",
		drv->tfa->hap_data.delay_attack);

	return 0;
}

static enum hrtimer_restart tfa_haptic_pbq_timer(struct hrtimer *timer)
{
	struct tfa9xxx *drv =
		container_of(timer, struct tfa9xxx, pbq_timer);

	dev_info(&drv->i2c->dev, "%s: trigger queue sleep timer expired\n", __func__);
	drv->pbq_state = TFA_PBQ_STATE_TIMEOUT;
	wake_up_interruptible(&drv->waitq_pbq);

	return HRTIMER_NORESTART;
}
static enum hrtimer_restart tfa_haptic_timer_func(struct hrtimer *timer)
{
	struct drv_object *obj = container_of(timer,
		struct drv_object, active_timer);
	struct tfa9xxx *drv = container_of(obj,
		struct tfa9xxx, tone_object);
#if defined(USE_IMMEDIATE_STOP)
	struct tfa2_device *tfa = drv->tfa;
#endif

#if defined(PARALLEL_OBJECTS)
	if (obj->type == OBJECT_TONE)
		drv = container_of(obj, struct tfa9xxx, tone_object);
	else
		drv = container_of(obj, struct tfa9xxx, wave_object);
#endif

	dev_info(&drv->i2c->dev, "%s: type=%d\n", __func__, (int)obj->type);

	obj->state = STATE_STOP;
	drv->is_index_triggered = false;
	drv->running_index = -1; /* reset */
#if defined(USE_IMMEDIATE_STOP)
	tfa->hap_state = TFA_HAP_STATE_STOPPED;
#endif

	/* sync */
#if !defined(USE_HAPTIC_WORKQUEUE)
	dev_dbg(&drv->i2c->dev, "%s: schedule_work (object state=%d)\n",
		__func__, obj->state);
	schedule_work(&obj->update_work);
#else
	dev_dbg(&drv->i2c->dev, "%s: queue_work (object state=%d)\n",
		__func__, obj->state);
	queue_work(drv->tfa9xxx_hap_wq, &obj->update_work);
#endif

	return HRTIMER_NORESTART;
}

static void tfa_haptic_pll_off(struct work_struct *work)
{
	struct tfa9xxx *drv = container_of(work,
		struct tfa9xxx, pll_off_work.work);

	dev_info(&drv->i2c->dev, "%s\n", __func__);

#if defined(TFA_CONTROL_MCLK)
	if (drv->clk_users > 0) {
		dev_dbg(&drv->i2c->dev,
			"%s: skip disabling amplifier / mclk - clk_users %d\n",
			__func__, drv->clk_users);
		return;
	}
#endif /* TFA_CONTROL_MCLK */

	mutex_lock(&drv->dsp_lock);
	/* turn off PLL */
	tfa2_dev_stop(drv->tfa);
	tfa9xxx_mclk_disable(drv);
	mutex_unlock(&drv->dsp_lock);
}

static void tfa9xxx_haptic_clock(struct tfa9xxx *drv, bool on)
{
	int ret;
#if defined(USE_EXCLUSIVE_SESSION)
	static bool clk_by_haptic;
#endif

	dev_dbg(&drv->i2c->dev, "%s: on=%d, clk_users=%d\n",
		__func__, (int)on, drv->clk_users);

	/* cancel delayed turning off of the PLL */
	cancel_delayed_work_sync(&drv->pll_off_work);

	if (on) {
		mutex_lock(&drv->dsp_lock);
		ret = tfa9xxx_mclk_enable(drv);
		if (ret == 0) {
			/* moved update of clock_users from tfa9xxx_haptic */
#if !defined(USE_EXCLUSIVE_SESSION)
			drv->clk_users++;
#else
			/* disallow to increase clk_users when overlapped */
			if (!clk_by_haptic)
				drv->clk_users++;
#endif
		}

		/* turn on PLL */
#if !defined(USE_EXCLUSIVE_SESSION)
		if (drv->clk_users == 1) {
#else
		if ((drv->clk_users == 1) && (!clk_by_haptic)) {
#endif
			ret = tfa2_dev_set_state(drv->tfa, TFA_STATE_POWERUP);
			if (ret < 0) {
				dev_err(&drv->i2c->dev, "%s: error in powering up (%d)\n",
					__func__, ret);
				drv->clk_users = 0;
				mutex_unlock(&drv->dsp_lock);
				return;
			}
		}
#if defined(USE_EXCLUSIVE_SESSION)
		clk_by_haptic = true;
#endif
		mutex_unlock(&drv->dsp_lock);
	} else if (drv->clk_users > 0) {
		mutex_lock(&drv->dsp_lock);
#if !defined(USE_EXCLUSIVE_SESSION)
		drv->clk_users--;
#else
		/* allow to decrease clk_users when called in pair */
		if (clk_by_haptic)
			drv->clk_users--;
		clk_by_haptic = false;
#endif
		if (drv->clk_users <= 0) {
			drv->clk_users = 0;
			/* turn off PLL with a delay */
#if !defined(USE_HAPTIC_WORKQUEUE)
			schedule_delayed_work(&drv->pll_off_work,
				msecs_to_jiffies(drv->pll_off_delay));
#else
			/* use tfa9xxx_wq to separate from tfa9xxx_hap_wq */
			queue_delayed_work(drv->tfa9xxx_wq,
				&drv->pll_off_work,
				msecs_to_jiffies(drv->pll_off_delay));
#endif
		}
		mutex_unlock(&drv->dsp_lock);
	}
}

int tfa9xxx_disable_f0_tracking(struct tfa9xxx *drv, bool disable)
{
	int ret = 0;

	dev_dbg(&drv->i2c->dev, "%s: disable=%d, f0_trc_users=%d\n",
		__func__, (int)disable, drv->f0_trc_users);

	if (disable) {
		if (drv->f0_trc_users == 0)
			/* disable*/
			ret = tfa2_haptic_disable_f0_trc(drv->i2c, 1);
		drv->f0_trc_users++;
	} else if (drv->f0_trc_users > 0) {
		drv->f0_trc_users--;
		if (drv->f0_trc_users == 0)
			/* enable again */
			ret = tfa2_haptic_disable_f0_trc(drv->i2c, 0);
	}

	return ret;
}

static int tfa9xxx_update_duration(struct tfa9xxx *drv, int index)
{
	int rc;
	struct tfa2_device *tfa = drv->tfa;

	drv->update_object_index = false;

	if ((index > 0)
		&& (index <= FW_HB_SEQ_OBJ + tfa->hap_data.seq_max)) {
		/*
		 * If the value is less or equal than number of objects in table
		 * plus the number of virtual objetcs, we assume it is an
		 * object index from App layer. Note that Android is not able
		 * too pass a 0 value, so the object index has an offset of 1
		 */
		drv->object_index = index - 1;
		drv->update_object_index = true;
		drv->is_index_triggered = true;
		tfa->hap_data.index = drv->object_index;
		dev_dbg(&drv->i2c->dev, "%s: obj_index is %d\n",
			__func__, drv->object_index);
	} else if (index > FW_HB_SEQ_OBJ + tfa->hap_data.seq_max) {
		dev_dbg(&drv->i2c->dev, "%s: duration = %d\n", __func__, index);
		rc = tfa2_haptic_update_duration(&tfa->hap_data, index);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static void tfa9xxx_update_led_class(struct work_struct *work)
{
	struct drv_object *obj = container_of(work,
		struct drv_object, update_work);
	struct tfa9xxx *drv = container_of(obj,
		struct tfa9xxx, tone_object);
	struct haptic_data *data;

#if defined(PARALLEL_OBJECTS)
	if (obj->type == OBJECT_TONE)
		drv = container_of(obj, struct tfa9xxx, tone_object);
	else
		drv = container_of(obj, struct tfa9xxx, wave_object);
#endif

	/* should have been checked before scheduling this work */
	BUG_ON(!drv->patch_loaded);

	data = &drv->tfa->hap_data;

	dev_info(&drv->i2c->dev, "[VIB] %s: type: %d, index: %d, state: %d\n",
		__func__, (int)obj->type, obj->index, obj->state);

	if (obj->state != STATE_STOP) {
#if !defined(USE_IMMEDIATE_STOP)
		if (obj->state == STATE_RESTART) {
			drv->running_index = -1; /* reset */
			tfa2_haptic_stop(drv->tfa, data, obj->index); /* sync */
		} else {
			tfa9xxx_haptic_clock(drv, true);
		}
#else
		if (obj->state == STATE_RESTART)
			drv->running_index = -1; /* reset */
		tfa9xxx_haptic_clock(drv, true);
#endif

#if defined(USE_SET_TIMER_IN_WORK)
		/* run ms timer */
		hrtimer_start(&obj->active_timer,
			ktime_set(drv->object_duration / 1000,
			(drv->object_duration % 1000) * 1000000),
			HRTIMER_MODE_REL);

		dev_dbg(&drv->i2c->dev, "%s: start active %d timer of %d msecs\n",
			__func__, (int)obj->type, drv->object_duration);
#endif /* USE_SET_TIMER_IN_WORK */

		if (drv->update_object_intensity)
			data->amplitude = drv->object_intensity;
		drv->running_index = obj->index;
		if (tfa2_haptic_start(drv->tfa, data, obj->index))
			dev_err(&drv->i2c->dev, "[VIB] %s: problem when starting\n",
				__func__);
	} else {
		drv->update_object_intensity = false;
		drv->running_index = -1; /* reset */
		tfa2_haptic_stop(drv->tfa, data, obj->index);
		tfa9xxx_haptic_clock(drv, false);
	}
}

static void tfa9xxx_set_intensity(struct tfa9xxx *drv,
	enum led_brightness brightness)
{
	dev_dbg(&drv->i2c->dev, "%s: brightness = %d\n",
		__func__, brightness);

	/* set amplitude scaled to 0 - 100% */
	drv->update_object_intensity = true;
	drv->object_intensity = (100 * brightness + LED_HALF) / LED_FULL;
}

static ssize_t tfa9xxx_store_value(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct tfa9xxx *drv = container_of(cdev, struct tfa9xxx, led_dev);
	struct tfa2_device *tfa = drv->tfa;
	struct haptic_data *data = &tfa->hap_data;
	long value = 0;
	int ret;

	ret = kstrtol(buf, 10, &value);
	if (ret)
		return ret;

	ret = tfa2_haptic_parse_value(data, value);
	if (ret)
		return -EINVAL;

	dev_info(&drv->i2c->dev, "%s: index=%d, amplitude=%d, frequency=%d\n",
		__func__, data->index, data->amplitude, data->frequency);

	return count;
}

static ssize_t tfa9xxx_show_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct tfa9xxx *drv = container_of(cdev, struct tfa9xxx, led_dev);
	struct tfa2_device *tfa = drv->tfa;
	int size;

	size = snprintf(buf, 80,
		"index=%d, amplitude=%d, frequency=%d, duration=%d\n",
		tfa->hap_data.index + 1, tfa->hap_data.amplitude,
		tfa->hap_data.frequency, tfa->hap_data.duration);

	return size;
}

static ssize_t tfa9xxx_store_state(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t tfa9xxx_show_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct tfa9xxx *drv = container_of(cdev, struct tfa9xxx, led_dev);
	int state = (drv->tone_object.state != STATE_STOP);
	int size;

#if defined(PARALLEL_OBJECTS)
	state = state || (drv->wave_object.state != STATE_STOP);
#endif

	size = snprintf(buf, 10, "%d\n", state);

	return size;
}

static int tfa9xxx_pbq_pair_launch(struct tfa9xxx *drv, int pbq_index)
{

	struct drv_object *obj = &drv->tone_object;
	struct haptic_data *data;
	int duration = 0;
	struct tfa2_device *tfa = drv->tfa;
	unsigned int index = 0, set_index = 0;
#if defined(TFA_SEPARATE_WAVE_OBJECT)
	int use_fscale = 0, set_intensity = 0;
	int max_intensity = 0;
#endif
	int sleep_duration = 0, rc = 0;
	
	data = &drv->tfa->hap_data;

	/* 1 get qeue index */
	index = drv->pbq_pairs[pbq_index].tag;
	/* 2.1 check Sleep operation */
	if (index == TFA9XXX_PBQ_TAG_SILENCE) {
		/* 2.2 get sleep duration */
		sleep_duration = drv->pbq_pairs[pbq_index].mag;
		/* run ms timer */
		pr_info("[VIB] %s %d sleep timer schedule \n",__func__, sleep_duration);
		hrtimer_start(&drv->pbq_timer,
			ktime_set(sleep_duration / 1000,
			(sleep_duration % 1000) * 1000000),
			HRTIMER_MODE_REL);
		
		rc = wait_event_interruptible(drv->waitq_pbq,
			drv->pbq_state == TFA_PBQ_STATE_TIMEOUT);
		pr_info("[VIB] %s %d sleep wait complete: rc = %d, pbq_state=%d \n",
			__func__, sleep_duration, rc, drv->pbq_state);
		drv->pbq_state = TFA_PBQ_STATE_IDLE;	

	} /* vibration working case */
	else {
		/* 2.2 get queue intensity */
		drv->intensity = drv->pbq_pairs[pbq_index].mag;		                                                                                                                   


		if (index == 0) {
			set_index = DEFAULT_TONE_SET_INDEX; /* default vibration */
		} else if (index >= OFFSET_SET_INDEX + MAX_NUM_SET_INDEX) {
			set_index = SILENCE_SET_INDEX; /* silence */
		} else if (index >= OFFSET_SET_INDEX
			&& index < OFFSET_SET_INDEX + MAX_NUM_SET_INDEX) {
			set_index = set_index_tbl[index - OFFSET_SET_INDEX];
#if defined(TFA_SEPARATE_WAVE_OBJECT)
			use_fscale = (tfa2_haptic_object_type(&tfa->hap_data,
				set_index) == OBJECT_WAVE);
#endif
		}

		pr_info("%s : pbq_index = %d , index = (0x%x/0x%x), intensity=0x%x\n",
			__func__,pbq_index, index, set_index, drv->intensity);

		tfa9xxx_update_duration(drv, set_index);
		drv->cp_trigger_index = index;

#if defined(TFA_SEPARATE_WAVE_OBJECT)
		max_intensity = (use_fscale) ? 255 : drv->tfa2_fw_intensity_max;
		set_intensity = (max_intensity * drv->intensity) / 100;
		tfa9xxx_set_intensity(drv, set_intensity);
#endif
		dev_dbg(&drv->i2c->dev, "%s: update_object_index=%d, index=%d\n",
			__func__, (int)drv->update_object_index, set_index);

		if (drv->update_object_index == true) {
			pr_info("[VIB] %s: index is updated\n", __func__);
			drv->update_object_index = false;
			obj->index = drv->object_index;
		}

		/* run vibration */
		if (obj->state == STATE_STOP) {
			pr_info("%s obj->state is STOP \n", __func__);
			return 0;
		} else {
			/* Must use sequcer vibration during trigger queue running */
			/* This is trigger queue concept */
			if (drv->update_object_intensity)
				data->amplitude = drv->object_intensity;
			drv->running_index = obj->index;

#if !defined(TFA_SEPARATE_WAVE_OBJECT)
			if (drv->object_index == DEFAULT_TONE_SET_INDEX - 1) {
				/* default tone - SS service call */
#else
			if ((drv->object_index >= 0 &&
				drv->object_index < FW_HB_SEQ_OBJ)
				&& !use_fscale) {
#endif
				/* this index need to working time */
				/* but nxp can't support this type of vibation */
				printk("[VIB] %s :PBQ error(%d UX index/%d index) need working time\n",
					__func__, index, drv->object_index);
				goto err;
			} else {
				/* object duration */
				duration = tfa2_haptic_get_duration
					(tfa, obj->index);
				/* clip value to max */
				duration = (duration > drv->timeout)
					? drv->timeout : duration;
			}

			/* Sequencer is blocking but non sequencer need wait logic */ 
			if (drv->object_index < FW_HB_SEQ_OBJ) {
				if (tfa2_haptic_start(drv->tfa, data, obj->index))
					dev_err(&drv->i2c->dev, "[VIB] %s: problem when starting\n",
						__func__);
				else {
					pr_info("[VIB] %s %d waiting timer scheduled \n",__func__, duration);
					hrtimer_start(&drv->pbq_timer,
						ktime_set(duration / 1000,
						(duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
					
					rc = wait_event_interruptible(drv->waitq_pbq,
						drv->pbq_state == TFA_PBQ_STATE_TIMEOUT);
					pr_info("[VIB] %s %d sleep wait complete: rc = %d, pbq_state=%d \n"
						,__func__, sleep_duration, rc, drv->pbq_state);
					drv->pbq_state == TFA_PBQ_STATE_IDLE;
				}
			}
			else if (obj->index < FW_HB_SEQ_OBJ + data->seq_max) {
				if (tfa2_haptic_start(drv->tfa, data, obj->index))
					dev_err(&drv->i2c->dev, "[VIB] %s: problem when starting\n",
						__func__);
			}
		}
	}
	return 0;
err:
	return -1;

}

static int vibrator_enable(struct tfa9xxx *drv, int val)
{
	struct drv_object *obj = &drv->tone_object;
	struct tfa2_device *tfa = drv->tfa;
	int state;
	int duration;
#if defined(PARALLEL_OBJECTS)
	bool object_is_tone;
#endif
#if defined(TFA_SEPARATE_WAVE_OBJECT)
	int use_fscale = 0;
#endif

	pr_info("[VIB] %s: value: %d ms\n", __func__, val);

	if (!drv->patch_loaded) {
		dev_err(&drv->i2c->dev, "%s: patch not loaded\n", __func__);
		return -EINVAL;
	}

	hrtimer_cancel(&obj->active_timer);

	state = (val != 0);

	dev_dbg(&drv->i2c->dev, "%s: update_object_index=%d, val=%d\n",
		__func__, (int)drv->update_object_index, val);

#if defined(PARALLEL_OBJECTS)
	object_is_tone = (tfa2_haptic_object_type(&tfa->hap_data,
		drv->object_index) == OBJECT_TONE);
	if (object_is_tone)
		obj = &drv->tone_object;
	else
		obj = &drv->wave_object;
#endif
#if defined(TFA_SEPARATE_WAVE_OBJECT)
	use_fscale = (tfa2_haptic_object_type(&tfa->hap_data,
		drv->object_index) == OBJECT_WAVE);
#endif

	if (drv->update_object_index == true) {
		pr_info("[VIB] %s: index is updated\n", __func__);
		drv->update_object_index = false;
		obj->index = drv->object_index;
		return 0;
	}

#if 0
	/* when no bck and no mclk, we can not play vibrations */
#if (defined(TFA_CONTROL_MCLK) && defined(TFA_USE_GPIO_FOR_MCLK))
	if (!drv->bck_running) {
		if (!gpio_is_valid(drv->mclk_gpio)) {
			dev_warn(&drv->i2c->dev,
				"%s: no active clock\n", __func__);
			return -ENODEV;
		}

		if (gpio_get_value_cansleep(drv->mclk_gpio) == 0) {
			dev_warn(&drv->i2c->dev,
				"%s: no active clock\n", __func__);
			return -ENODEV;
		}

		dev_dbg(&drv->i2c->dev, "%s: mclk is active\n", __func__);
	} else {
		dev_dbg(&drv->i2c->dev, "%s: bck is active\n", __func__);
	}
#else
	if (!drv->bck_running && !drv->mclk) {
		dev_warn(&drv->i2c->dev, "%s: no active clock\n", __func__);
		return -ENODEV;
	}
#endif
#endif

	dev_dbg(&drv->i2c->dev, "%s: state=%d, object state=%d\n",
		__func__, state, obj->state);

	if (state == 0) {
		if (obj->state == STATE_STOP) {
			pr_info("[VIB]: OFF\n");
			drv->is_index_triggered = false;
			return 0;
		}
		obj->state = STATE_STOP;
	} else {
		if (obj->state == STATE_STOP) {
			obj->state = STATE_START;
		} else {
			dev_dbg(&drv->i2c->dev,
				"%s: restart, clk_users=%d\n",
				__func__, drv->clk_users);
			obj->state = STATE_RESTART;
#if defined(USE_IMMEDIATE_STOP)
			/* reset later in work */
			tfa2_haptic_stop(tfa,
				&tfa->hap_data, obj->index); /* async */
#endif
		}
	}

	if (obj->state != STATE_STOP && drv->cp_trigger_index != TFA9XXX_INDEX_PBQ) {
		dev_dbg(&drv->i2c->dev,
			"%s: state=%d, running index=%d\n",
			__func__, obj->state, drv->running_index);
#if defined(USE_SERIALIZED_START)
		if (drv->running_index >= 0
			&& drv->running_index < FW_HB_SEQ_OBJ) {
			dev_dbg(&drv->i2c->dev,
				"%s: flush queued work\n", __func__);
			/* serialize non-blocking objects */
#if !defined(USE_HAPTIC_WORKQUEUE)
			flush_work(&obj->update_work);
#else
			flush_workqueue(drv->tfa9xxx_hap_wq);
#endif
		}
#endif

#if !defined(TFA_SEPARATE_WAVE_OBJECT)
		if (drv->object_index == DEFAULT_TONE_SET_INDEX - 1) {
			/* default tone - SS service call */
#else
		if ((drv->object_index >= 0
			&& drv->object_index < FW_HB_SEQ_OBJ)
			&& !use_fscale) {
			/* tone object in table - SS service call */
#endif
			duration = val;
			duration = (duration > TIMEOUT)
				? TIMEOUT : duration;
			dev_dbg(&drv->i2c->dev,
				"%s: update duration=%d\n", __func__, duration);
			tfa2_haptic_update_duration(&tfa->hap_data, duration);
		} else {
			/* wave object in table & sequencer */
			/* object duration + 5 msecs */
			duration = tfa2_haptic_get_duration
				(tfa, obj->index) + 5;
			/* clip value to max */
			duration = (duration > drv->timeout)
				? drv->timeout : duration;
		}

#if defined(USE_SET_TIMER_IN_WORK)
		drv->object_duration = duration;
#else
		/* run ms timer */
		hrtimer_start(&obj->active_timer,
			ktime_set(duration / 1000,
			(duration % 1000) * 1000000),
			HRTIMER_MODE_REL);

		dev_dbg(&drv->i2c->dev, "%s: start active %d timer of %d msecs\n",
			__func__, (int)obj->type, duration);
#endif /* USE_SET_TIMER_IN_WORK */
	}

#if defined(USE_IMMEDIATE_STOP)
	if (obj->state == STATE_STOP) {
		dev_dbg(&drv->i2c->dev, "%s: stop (object state=%d)\n",
			__func__, obj->state);

		drv->update_object_intensity = false;
		drv->running_index = -1; /* reset */
		if (drv->cp_trigger_index == TFA9XXX_INDEX_PBQ) {
			drv->pbq_state == TFA_PBQ_STATE_TIMEOUT;
			wake_up_interruptible(&drv->waitq_pbq);
		}
		tfa2_haptic_stop(tfa, &tfa->hap_data, obj->index); /* async */
		tfa9xxx_haptic_clock(drv, false);

		return 0;
	}
#endif /* USE_IMMEDIATE_STOP */

#if !defined(USE_HAPTIC_WORKQUEUE)
	dev_dbg(&drv->i2c->dev, "%s: schedule_work (object state=%d)\n",
		__func__, obj->state);
	schedule_work(&obj->update_work);
#else
	dev_dbg(&drv->i2c->dev, "%s: queue_work (object state=%d, cp_trigger_index=0x%x)\n",
		__func__, obj->state, drv->cp_trigger_index);
	if (drv->cp_trigger_index == TFA9XXX_INDEX_PBQ)
		queue_work(drv->tfa9xxx_hap_wq, &drv->vibe_pbq_work);
	else
		queue_work(drv->tfa9xxx_hap_wq, &obj->update_work);
#endif

	return 0;
}

static ssize_t enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	u32 val;
	int rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	vibrator_enable(drv, val);

	return count;
}

int tfa9xxx_bck_starts(struct tfa9xxx *drv)
{
#if defined(TFA_USE_GPIO_FOR_MCLK)
	dev_dbg(&drv->i2c->dev, "%s: bck_running=%d, mclk_gpio=%d\n",
		__func__, (int)drv->bck_running,
		(int)gpio_is_valid(drv->mclk_gpio));
#else
	dev_dbg(&drv->i2c->dev, "%s: bck_running=%d, mclk=%p\n", __func__,
		(int)drv->bck_running, drv->mclk);
#endif

	if (drv->bck_running)
		return 0;

	dev_info(&drv->i2c->dev, "%s: clk_users %d\n",
		__func__, drv->clk_users);

	/* clock enabling is handled in tfa_start(), so update reference count
	 * instead of calling tfa9xxx_haptic_clock(drv, true)
	 */
	/* moved the update of clock_users to tfa9xxx */
#if !defined(TFA_CONTROL_MCLK)
	drv->clk_users++;
#endif

	drv->bck_running = true;

	return 0;
}

int tfa9xxx_bck_stops(struct tfa9xxx *drv)
{
	struct drv_object *obj;

#if defined(TFA_USE_GPIO_FOR_MCLK)
	dev_dbg(&drv->i2c->dev, "%s: bck_running=%d, mclk_gpio=%d\n",
		__func__, (int)drv->bck_running,
		(int)gpio_is_valid(drv->mclk_gpio));
#else
	dev_dbg(&drv->i2c->dev, "%s: bck_running=%d, mclk=%p\n", __func__,
		(int)drv->bck_running, drv->mclk);
#endif

	if (!drv->bck_running)
		return 0;

	dev_info(&drv->i2c->dev, "%s: clk_users %d\n",
		__func__, drv->clk_users);

	drv->bck_running = false;

	/* clock disabling is handled in tfa_stop(), so update reference count
	 * instead of calling tfa9xxx_haptic_clock(drv, false)
	 */
	/* moved the update of clock_users to tfa9xxx */
#if !defined(TFA_CONTROL_MCLK)
	drv->clk_users--;
	if (drv->clk_users < 0)
		drv->clk_users = 0;
#endif

#if defined(TFA_USE_GPIO_FOR_MCLK)
	if (gpio_is_valid(drv->mclk_gpio))
		return 0;
#else
	if (drv->mclk)
		return 0;
#endif

	/* When there is no mclk, stop running objects */
#if defined(PARALLEL_OBJECTS)
	obj = &drv->wave_object;
	if (obj->state != STATE_STOP) {
		obj->state = STATE_STOP;
		hrtimer_cancel(&obj->active_timer);
#if !defined(USE_HAPTIC_WORKQUEUE)
		schedule_work(&obj->update_work);
		flush_work(&obj->update_work);
#else
		queue_work(drv->tfa9xxx_hap_wq, &obj->update_work);
		flush_workqueue(drv->tfa9xxx_hap_wq);
#endif
	}
#endif
	obj = &drv->tone_object;
	if (obj->state != STATE_STOP) {
		obj->state = STATE_STOP;
		hrtimer_cancel(&obj->active_timer);
#if !defined(USE_HAPTIC_WORKQUEUE)
		schedule_work(&obj->update_work);
		flush_work(&obj->update_work);
#else
		queue_work(drv->tfa9xxx_hap_wq, &obj->update_work);
		flush_workqueue(drv->tfa9xxx_hap_wq);
#endif
	}

	return 0;
}

static void tfa9xxx_vibe_pbq_worker(struct work_struct *work)
{
	struct tfa9xxx *drv = container_of(work,
		struct tfa9xxx, vibe_pbq_work);
	struct drv_object *obj = &drv->tone_object;

	unsigned int tag, mag;
	int i;
	int rc;

	pr_info("[VIB] %s start worker \n",__func__);

	if (obj->state == STATE_RESTART)
		drv->running_index = -1; /* reset */
	tfa9xxx_haptic_clock(drv, true);

	drv->pbq_remain = drv->pbq_repeat;
	drv->pbq_index = 0;

	do {
		/* restart queue as necessary */
		if (drv->pbq_index == drv->pbq_depth) {
			drv->pbq_index = 0;
			for (i = 0; i < drv->pbq_depth; i++)
				drv->pbq_pairs[i].remain =
						drv->pbq_pairs[i].repeat;
			pr_info("%s : drv->pqb_remain = %d \n",__func__,drv->pbq_remain);
			switch (drv->pbq_remain) {
			case -1:
				/* loop until stopped */
				break;
			case 0:
				/* queue is finished */
				if (obj->state != STATE_STOP) {
					dev_dbg(&drv->i2c->dev, "%s: stop (object state=%d)\n",
						__func__, obj->state);

					drv->update_object_intensity = false;
					drv->running_index = -1; /* reset */
					tfa2_haptic_stop(drv->tfa, &drv->tfa->hap_data, obj->index); /* async */
					tfa9xxx_haptic_clock(drv, false);
				}
				goto exit;
			default:
				/* loop once more */
				drv->pbq_remain--;
			}
		}

		tag = drv->pbq_pairs[drv->pbq_index].tag;
		mag = drv->pbq_pairs[drv->pbq_index].mag;

		switch (tag) {
			pr_info("[VIB] %s: tag: %d, mag: %d, pbq_index: %d\n",
			__func__, tag, mag, drv->pbq_index);

		case TFA9XXX_PBQ_TAG_START:
			drv->pbq_index++;
			break;
		case TFA9XXX_PBQ_TAG_STOP:
			if (drv->pbq_pairs[drv->pbq_index].remain) {
				drv->pbq_pairs[drv->pbq_index].remain--;
				drv->pbq_index = mag;
			} else {
				drv->pbq_index++;
			}
			break;
		case TFA9XXX_PBQ_TAG_SILENCE:
		default:			
			rc = tfa9xxx_pbq_pair_launch(drv,drv->pbq_index);
			if (rc < 0)
				break;
			drv->pbq_index++;
		}
	} while (obj->state != STATE_STOP);
exit:
	pr_info("[VIB] %s start worker exit \n",__func__);
	
}

static ssize_t tfa9xxx_motor_type_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);

	pr_info("%s: %s\n", __func__, drv->vib_type);
	return snprintf(buf, MAX_LEN_VIB_TYPE, "%s\n", drv->vib_type);
}
static DEVICE_ATTR(motor_type, 0660, tfa9xxx_motor_type_show, NULL);

static ssize_t tfa9xxx_cp_trigger_index_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", drv->cp_trigger_index);
}

static ssize_t tfa9xxx_cp_trigger_index_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	struct drv_object *obj = &drv->tone_object;
#if defined(PARALLEL_OBJECTS) || defined(TFA_SEPARATE_WAVE_OBJECT)
	struct tfa2_device *tfa = drv->tfa;
#endif
	int ret;
	unsigned int index, set_index;
#if defined(TFA_SEPARATE_WAVE_OBJECT)
	int use_fscale = 0, set_intensity = 0;
	int max_intensity;
#endif

	ret = kstrtou32(buf, 10, &index);
	if (ret)
		return -EINVAL;

	if (index == LOW_TEMP_INDEX) {
		pr_info("%s low temperature vibration pattern\n", __func__);
		set_index = LOW_TEMP_SET_INDEX;
	} else if (index == 0) {
		set_index = DEFAULT_TONE_SET_INDEX; /* default vibration */
	} else if (index == TFA9XXX_INDEX_PBQ) { /* triger queue case */
		drv->cp_trigger_index = index;
		drv->object_index = index;
		pr_info("%s cp trigger queue vibration pattern\n", __func__);
		return count;
	} else if (index >= OFFSET_SET_INDEX + MAX_NUM_SET_INDEX) {
		set_index = SILENCE_SET_INDEX; /* silence */
	} else if (index >= OFFSET_SET_INDEX
		&& index < OFFSET_SET_INDEX + MAX_NUM_SET_INDEX) {
		set_index = set_index_tbl[index - OFFSET_SET_INDEX];
#if defined(TFA_SEPARATE_WAVE_OBJECT)
		use_fscale = (tfa2_haptic_object_type(&tfa->hap_data,
			set_index) == OBJECT_WAVE);
#endif
	}

	pr_info("%s service_index: %u table_index: %u \n", __func__, index, set_index);

	tfa9xxx_update_duration(drv, set_index);
	drv->cp_trigger_index = index;

#if defined(TFA_SEPARATE_WAVE_OBJECT)
	max_intensity = (use_fscale) ? 255 : drv->tfa2_fw_intensity_max;
	set_intensity = (max_intensity * drv->intensity) / MAX_INTENSITY;

	dev_dbg(&drv->i2c->dev,
		"[VIB] %s intensity : %d brightness : %d / %d, index=%d, use_fscale=%d\n",
		__func__, drv->intensity, set_intensity,
		max_intensity, set_index, use_fscale);

	tfa9xxx_set_intensity(drv, set_intensity);
#endif

	dev_dbg(&drv->i2c->dev, "%s: update_object_index=%d, index=%d\n",
		__func__, (int)drv->update_object_index, set_index);

#if defined(PARALLEL_OBJECTS)
	object_is_tone = (tfa2_haptic_object_type(&tfa->hap_data,
		drv->object_index) == OBJECT_TONE);
	if (object_is_tone)
		obj = &drv->tone_object;
	else
		obj = &drv->wave_object;
#endif

	if (drv->update_object_index == true) {
		pr_info("[VIB] %s: index is updated\n", __func__);
		drv->update_object_index = false;
		obj->index = drv->object_index;
	}

	return count;
}

static DEVICE_ATTR(cp_trigger_index, 0660, tfa9xxx_cp_trigger_index_show, tfa9xxx_cp_trigger_index_store);

static ssize_t enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	int size;

#if defined(PARALLEL_OBJECTS)
	int state = (drv->tone_object.state != STATE_STOP) ||
		(drv->wave_object.state != STATE_STOP);
#else
	int state = (drv->tone_object.state != STATE_STOP);
#endif

	size = snprintf(buf, 10, "%d\n", state);

	return size;
}

static DEVICE_ATTR(enable, 0660, enable_show, enable_store);

/* For index queue working */
static ssize_t tfa9xxx_cp_trigger_queue_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	ssize_t len = 0;
	int i;

	if (drv->pbq_depth == 0)
		return -ENODATA;

	for (i = 0; i < drv->pbq_depth; i++) {
		switch (drv->pbq_pairs[i].tag) {
		case TFA9XXX_PBQ_TAG_SILENCE:
			len += snprintf(buf + len, PAGE_SIZE - len, "%d",
					drv->pbq_pairs[i].mag);
			break;
		case TFA9XXX_PBQ_TAG_START:
			len += snprintf(buf + len, PAGE_SIZE - len, "!!");
			break;
		case TFA9XXX_PBQ_TAG_STOP:
			len += snprintf(buf + len, PAGE_SIZE - len, "%d!!",
					drv->pbq_pairs[i].repeat);
			break;
		default:
			len += snprintf(buf + len, PAGE_SIZE - len, "%d.%d",
					drv->pbq_pairs[i].tag,
					drv->pbq_pairs[i].mag);
		}

		if (i < (drv->pbq_depth - 1))
			len += snprintf(buf + len, PAGE_SIZE - len, ", ");
	}

	switch (drv->pbq_repeat) {
	case -1:
		len += snprintf(buf + len, PAGE_SIZE - len, ", ~\n");
		break;
	case 0:
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
		break;
	default:
		len += snprintf(buf + len, PAGE_SIZE - len, ", %d!\n",
				drv->pbq_repeat);
	}
	return len;
}

static ssize_t tfa9xxx_cp_trigger_queue_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	char *pbq_str_alloc, *pbq_str, *pbq_str_tok;
	char *pbq_seg_alloc, *pbq_seg, *pbq_seg_tok;
	size_t pbq_seg_len;
	unsigned int pbq_depth = 0;
	unsigned int val, num_empty;
	int pbq_marker = -1;
	int ret;

	pbq_str_alloc = kzalloc(count, GFP_KERNEL);
	if (!pbq_str_alloc)
		return -ENOMEM;

	pbq_seg_alloc = kzalloc(TFA9XXX_PBQ_SEG_LEN_MAX + 1, GFP_KERNEL);
	if (!pbq_seg_alloc) {
		kfree(pbq_str_alloc);
		return -ENOMEM;
	}

	drv->pbq_depth = 0;
	drv->pbq_repeat = 0;
	drv->pbq_index = 0;

	pbq_str = pbq_str_alloc;
	strlcpy(pbq_str, buf, count);

	pbq_str_tok = strsep(&pbq_str, ",");

	while (pbq_str_tok) {
		pr_info("[VIB] %s: pbq_str_tok: %s\n", pbq_str_tok);
		pbq_seg = pbq_seg_alloc;
		pbq_seg_len = strlcpy(pbq_seg, strim(pbq_str_tok),
				TFA9XXX_PBQ_SEG_LEN_MAX + 1);
		if (pbq_seg_len > TFA9XXX_PBQ_SEG_LEN_MAX) {
			ret = -E2BIG;
			goto err_mutex;
		}		
		/* waveform specifier */
		if (strnchr(pbq_seg, TFA9XXX_PBQ_SEG_LEN_MAX, '.')) {
			
			/* index */
			pbq_seg_tok = strsep(&pbq_seg, ".");
			
			ret = kstrtou32(pbq_seg_tok, 10, &val);
			if (ret) {
				ret = -EINVAL;
				goto err_mutex;
			}
			if (val == 0 || val >= OFFSET_SET_INDEX + MAX_NUM_SET_INDEX) {
				ret = -EINVAL;
				goto err_mutex;
			}
			drv->pbq_pairs[pbq_depth].tag = val;

			/* scale */
			pbq_seg_tok = strsep(&pbq_seg, ".");

			ret = kstrtou32(pbq_seg_tok, 10, &val);
			if (ret) {
				ret = -EINVAL;
				goto err_mutex;
			}
			if (val == 0 || val > TFA9XXX_PBQ_SCALE_MAX) {
				ret = -EINVAL;
				goto err_mutex;
			}
			drv->pbq_pairs[pbq_depth++].mag = val;

		/* repetition specifier */
		} else if (strnchr(pbq_seg, TFA9XXX_PBQ_SEG_LEN_MAX, '!')) {
			val = 0;
			num_empty = 0;

			pbq_seg_tok = strsep(&pbq_seg, "!");

			while (pbq_seg_tok) {
				if (strnlen(pbq_seg_tok,
						TFA9XXX_PBQ_SEG_LEN_MAX)) {
					ret = kstrtou32(pbq_seg_tok, 10, &val);
					if (ret) {
						ret = -EINVAL;
						goto err_mutex;
					}
					if (val > TFA9XXX_PBQ_REPEAT_MAX) {
						ret = -EINVAL;
						goto err_mutex;
					}
				} else {
					num_empty++;
				}

				pbq_seg_tok = strsep(&pbq_seg, "!");
			}

			/* number of empty tokens reveals specifier type */
			switch (num_empty) {
			case 1:	/* outer loop: "n!" or "!n" */
				if (drv->pbq_repeat) {
					ret = -EINVAL;
					goto err_mutex;
				}
				drv->pbq_repeat = val;
				break;

			case 2:	/* inner loop stop: "n!!" or "!!n" */
				if (pbq_marker < 0) {
					ret = -EINVAL;
					goto err_mutex;
				}

				drv->pbq_pairs[pbq_depth].tag =
						TFA9XXX_PBQ_TAG_STOP;
				drv->pbq_pairs[pbq_depth].mag = pbq_marker;
				drv->pbq_pairs[pbq_depth++].repeat = val;
				pbq_marker = -1;
				break;

			case 3:	/* inner loop start: "!!" */
				if (pbq_marker >= 0) {
					ret = -EINVAL;
					goto err_mutex;
				}
				drv->pbq_pairs[pbq_depth].tag =
						TFA9XXX_PBQ_TAG_START;
				pbq_marker = pbq_depth++;
				break;

			default:
				ret = -EINVAL;
				goto err_mutex;
			}
		/* loop specifier */
		} else if (strnchr(pbq_seg, TFA9XXX_PBQ_SEG_LEN_MAX, '~')) {
			if (drv->pbq_repeat) {
				ret = -EINVAL;
				goto err_mutex;
			}
			drv->pbq_repeat = -1;
		/* duration specifier */
		} else {
			drv->pbq_pairs[pbq_depth].tag =
					TFA9XXX_PBQ_TAG_SILENCE;

			ret = kstrtou32(pbq_seg, 10, &val);
			if (ret) {
				ret = -EINVAL;
				goto err_mutex;
			}
			if (val > TFA9XXX_PBQ_DELAY_MAX) {
				ret = -EINVAL;
				goto err_mutex;
			}
			drv->pbq_pairs[pbq_depth++].mag = val;
		}

		if (pbq_depth == TFA9XXX_PBQ_DEPTH_MAX) {
			ret = -E2BIG;
			goto err_mutex;
		}
		pbq_str_tok = strsep(&pbq_str, ",");
	}

#ifdef USE_FSA9XXX_PBQ_DEBUG_FEATURE
	if(count > DEBUG_PRINT_PLAYBACK_QUEUE) {
		strlcpy(pbq_str_alloc, buf, DEBUG_PRINT_PLAYBACK_QUEUE);
		pr_info("%s pbq_str=%s... (depth:%d)\n", __func__, pbq_str_alloc, pbq_depth);
	} else {
		strlcpy(pbq_str_alloc, buf, count);
		pr_info("%s pbq_str=%s (depth:%d)\n", __func__, pbq_str_alloc, pbq_depth);
	}
#endif
	drv->pbq_depth = pbq_depth;
	ret = count;

err_mutex:

	kfree(pbq_str_alloc);
	kfree(pbq_seg_alloc);

	return ret;
}

static DEVICE_ATTR(cp_trigger_queue, 0660, tfa9xxx_cp_trigger_queue_show,
		tfa9xxx_cp_trigger_queue_store);

static ssize_t intensity_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	int ret = 0, set_intensity = 0;
#if !defined(TFA_SEPARATE_WAVE_OBJECT)
	int max_intensity = drv->tfa2_fw_intensity_max;
#endif

	ret = kstrtoint(buf, 0, &set_intensity);
	if (ret) {
		pr_err("[VIB]: %s failed to get intensity", __func__);
		return ret;
	}

	if ((set_intensity < 0) || (set_intensity > MAX_INTENSITY)) {
		pr_err("[VIB]: %sout of rage\n", __func__);
		return -EINVAL;
	}
	printk("[VIB] %s : set_intensity = %d / drv->intensity = %d \n",__func__, set_intensity, drv->intensity);

	drv->intensity = set_intensity;
#if !defined(TFA_SEPARATE_WAVE_OBJECT)
	set_intensity = (max_intensity * drv->intensity) / MAX_INTENSITY;
	pr_info("[VIB] %s intensity : %d brightness : %d, max_brightness: %d\n",
	  __func__, drv->intensity, set_intensity, max_intensity);

	tfa9xxx_set_intensity(drv, set_intensity);
#endif

	return count;
}

static ssize_t intensity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);

	return sprintf(buf, "intensity: %u\n", drv->intensity);
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static ssize_t tfa9xxx_store_duration(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int rc;
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct tfa9xxx *drv = container_of(cdev, struct tfa9xxx, led_dev);

	rc = kstrtoint(buf, 0, &val);
	if (rc < 0)
		return rc;

	dev_dbg(&drv->i2c->dev, "%s: val=%d\n", __func__, val);

	if (val < 0) {
		rc = tfa9xxx_store_value(dev, attr, buf, count);
		if (rc < 0)
			return rc;
	} else {
		tfa9xxx_update_duration(drv, val);
	}

	return count;
}

static ssize_t tfa9xxx_show_duration(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return tfa9xxx_show_value(dev, attr, buf);
}

static ssize_t tfa9xxx_show_f0(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	int ret, f0;
	int size;

	ret = tfa2_haptic_read_f0(drv->i2c, &f0);
	if (ret)
		return -EIO;

	size = snprintf(buf, 15, "%d.%03d\n",
		TFA2_HAPTIC_FP_INT(f0, FW_XMEM_F0_SHIFT),
		TFA2_HAPTIC_FP_FRAC(f0, FW_XMEM_F0_SHIFT));

	return size;
}

static ssize_t tfa9xxx_store_pll_off_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	int err;

	err = kstrtouint(buf, 10, &drv->pll_off_delay);
	if (err)
		return err;

	return count;
}

static ssize_t tfa9xxx_show_pll_off_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tfa9xxx *drv = dev_get_drvdata(dev);
	int size;

	size = snprintf(buf, 10, "%u\n", drv->pll_off_delay);

	return size;
}

static struct device_attribute tfa9xxx_haptic_attrs[] = {
	__ATTR(value, 0664, tfa9xxx_show_value, tfa9xxx_store_value),
	__ATTR(state, 0664, tfa9xxx_show_state, tfa9xxx_store_state),
	__ATTR(duration, 0664, tfa9xxx_show_duration, tfa9xxx_store_duration),
};

static DEVICE_ATTR(f0, 0444, tfa9xxx_show_f0, NULL);
static DEVICE_ATTR(pll_off_delay, 0644, tfa9xxx_show_pll_off_delay,
	tfa9xxx_store_pll_off_delay);


int tfa9xxx_haptic_probe(struct tfa9xxx *drv)
{
	struct i2c_client *i2c = drv->i2c;
	int failed_attr_idx = 0;
	int rc, i;

	dev_info(&drv->i2c->dev, "%s\n", __func__);

	drv->timeout = TFA_HAP_MAX_TIMEOUT;
	drv->pll_off_delay = DEFAULT_PLL_OFF_DELAY;

#if defined(USE_HAPTIC_WORKQUEUE)
	/* setup work queue, will be used to haptic event */
	drv->tfa9xxx_hap_wq = create_workqueue("tfa9xxxhap");
	if (!drv->tfa9xxx_hap_wq)
		return -ENOMEM;
#endif

	INIT_WORK(&drv->tone_object.update_work, tfa9xxx_update_led_class);
#if defined(PARALLEL_OBJECTS)
	INIT_WORK(&drv->wave_object.update_work, tfa9xxx_update_led_class);
#endif
	INIT_DELAYED_WORK(&drv->pll_off_work, tfa_haptic_pll_off);
	/* cp trigger queue worker */
	INIT_WORK(&drv->vibe_pbq_work, tfa9xxx_vibe_pbq_worker);

#if defined(TFA_USE_WAITQUEUE_SEQ)
	init_waitqueue_head(&drv->tfa->waitq_seq);
#endif
	init_waitqueue_head(&drv->waitq_pbq);

	drv->tone_object.type = OBJECT_TONE;
#if defined(PARALLEL_OBJECTS)
	drv->wave_object.type = OBJECT_WAVE;
#endif
	/* add "f0" entry to i2c device */
	rc = device_create_file(&i2c->dev, &dev_attr_f0);
	if (rc < 0) {
		dev_err(&drv->i2c->dev, "%s: Error, creating sysfs f0 entry\n",
			__func__);
		goto error_sysfs_f0;
	}

	/* add "pll_off_delay" entry to i2c device */
	rc = device_create_file(&i2c->dev, &dev_attr_pll_off_delay);
	if (rc < 0) {
		dev_err(&drv->i2c->dev, "%s: Error, creating sysfs pll_off_delay entry\n",
			__func__);
		goto error_sysfs_pll;
	}

	/* active timers for led driver */
	hrtimer_init(&drv->tone_object.active_timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv->tone_object.active_timer.function = tfa_haptic_timer_func;
	
	/* active timers for index queue logic */
	hrtimer_init(&drv->pbq_timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv->pbq_timer.function = tfa_haptic_pbq_timer;	

#if defined(PARALLEL_OBJECTS)
	hrtimer_init(&drv->wave_object.active_timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv->wave_object.active_timer.function = tfa_haptic_timer_func;
#endif

	/* led driver */
	drv->led_dev.name = "vibrator";
	drv->led_dev.default_trigger = "none";

	rc = led_classdev_register(&i2c->dev, &drv->led_dev);
	if (rc < 0) {
		dev_err(&drv->i2c->dev, "%s: Error, led device\n", __func__);
		goto error_led_class;
	}

	for (i = 0; i < ARRAY_SIZE(tfa9xxx_haptic_attrs); i++) {
		rc = sysfs_create_file(&drv->led_dev.dev->kobj,
			&tfa9xxx_haptic_attrs[i].attr);
		if (rc < 0) {
			dev_err(&drv->i2c->dev,
				"%s: Error, creating sysfs entry %d rc=%d\n",
				__func__, i, rc);
			failed_attr_idx = i;
			goto error_sysfs_led;
		}
	}

	drv->to_class = class_create(THIS_MODULE, "timed_output");
	if (IS_ERR(drv->to_class))
		pr_err("[VIB]: timed_output classs create fail\n");

	drv->to_dev = device_create(drv->to_class, NULL, 0, drv, "vibrator");
	if (IS_ERR(drv->to_dev))
		return PTR_ERR(drv->to_dev);

	rc = sysfs_create_file(&drv->to_dev->kobj, &dev_attr_enable.attr);
	if (rc < 0)
		pr_err("[VIB]: Failed to register sysfs enable: %d\n", rc);

	rc = sysfs_create_file(&drv->to_dev->kobj, &dev_attr_intensity.attr);
	if (rc < 0)
		pr_err("[VIB]: Failed to register sysfs intensity: %d\n", rc);

	rc = sysfs_create_file(&drv->to_dev->kobj, &dev_attr_cp_trigger_index.attr);
	if (rc < 0)
		pr_err("[VIB]: Failed to register sysfs cp_trigger_index: %d\n", rc);

	rc = sysfs_create_file(&drv->to_dev->kobj, &dev_attr_cp_trigger_queue.attr);
	if (rc < 0)
		pr_err("[VIB]: Failed to register sysfs cp_trigger_queue: %d\n", rc);

	rc = sysfs_create_file(&drv->to_dev->kobj, &dev_attr_motor_type.attr);
	if (rc < 0)
		pr_err("[VIB]: Failed to register sysfs motor type: %d\n", rc);

	drv->intensity = MAX_INTENSITY;

	return 0;

error_sysfs_led:
	for (i = 0; i < failed_attr_idx; i++)
		sysfs_remove_file(&drv->led_dev.dev->kobj,
			&tfa9xxx_haptic_attrs[i].attr);
	led_classdev_unregister(&drv->led_dev);

error_led_class:
	device_remove_file(&i2c->dev, &dev_attr_pll_off_delay);

error_sysfs_pll:
	device_remove_file(&i2c->dev, &dev_attr_f0);

error_sysfs_f0:
	return rc;
}

int tfa9xxx_haptic_remove(struct tfa9xxx *drv)
{
	struct i2c_client *i2c = drv->i2c;
	int i;

	hrtimer_cancel(&drv->tone_object.active_timer);
#if defined(PARALLEL_OBJECTS)
	hrtimer_cancel(&drv->wave_object.active_timer);
#endif

	device_remove_file(&i2c->dev, &dev_attr_f0);
	device_remove_file(&i2c->dev, &dev_attr_pll_off_delay);

	for (i = 0; i < ARRAY_SIZE(tfa9xxx_haptic_attrs); i++) {
		sysfs_remove_file(&drv->led_dev.dev->kobj,
			&tfa9xxx_haptic_attrs[i].attr);
	}

	led_classdev_unregister(&drv->led_dev);

	cancel_work_sync(&drv->tone_object.update_work);
#if defined(PARALLEL_OBJECTS)
	cancel_work_sync(&drv->wave_object.update_work);
#endif
	cancel_delayed_work_sync(&drv->pll_off_work);

#if defined(USE_HAPTIC_WORKQUEUE)
	if (drv->tfa9xxx_hap_wq)
		destroy_workqueue(drv->tfa9xxx_hap_wq);
#endif

	return 0;
}

int __init tfa9xxx_haptic_init(void)
{
	return 0;
}

void __exit tfa9xxx_haptic_exit(void)
{
}

