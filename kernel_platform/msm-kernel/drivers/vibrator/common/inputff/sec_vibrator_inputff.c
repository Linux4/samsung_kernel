// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* sec vibrator inputff */
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/vibrator/sec_vibrator_inputff.h>

#if defined(CONFIG_SEC_KUNIT)
#include <kunit/mock.h>
#else
#define __visible_for_testing static
#endif

enum compose_thread_state {
	COMPOSE_STOP = 0,
	COMPOSE_RUN = 1,
	COMPOSE_START = 2,
	COMPOSE_EXIT = 3,
};

struct common_inputff_effect {
	int type;
	int effect_id;
	int scale;
	int duration;
	int frequency;
};

struct common_inputff_effects {
	struct common_inputff_effect effects[MAX_COMPOSE_EFFECT];
	int num_of_effects;
	int repeat;
};

static struct sec_vib_inputff_pdata  sec_vib_inputff_pdata;

static struct blocking_notifier_head sec_vib_inputff_nb_head =
	BLOCKING_NOTIFIER_INIT(sec_vib_inputff_nb_head);

int sec_vib_inputff_notifier_register(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = blocking_notifier_chain_register(&sec_vib_inputff_nb_head, nb);
	if (ret < 0)
		pr_err("%s: failed(%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_notifier_register);

int sec_vib_inputff_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = blocking_notifier_chain_unregister(&sec_vib_inputff_nb_head, nb);
	if (ret < 0)
		pr_err("%s: failed(%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_notifier_unregister);

int sec_vib_inputff_vib_notifier_notify(void)
{
	int ret = 0;

	ret = blocking_notifier_call_chain(&sec_vib_inputff_nb_head,
			VIB_NOTIFIER_ON, NULL);

	switch (ret) {
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s failed(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_vib_notifier_notify);

__visible_for_testing bool sec_vib_inputff_getbit(struct sec_vib_inputff_drvdata *ddata, int val)
{
	return (ddata->ff_val >> (val - FF_EFFECT_MIN) & 1);
}

int sec_vib_inputff_setbit(struct sec_vib_inputff_drvdata *ddata, int val)
{
	return ddata->ff_val |= (1 << (val - FF_EFFECT_MIN));
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_setbit);

static int parsing_compose_effects(struct sec_vib_inputff_drvdata *ddata,
		struct common_inputff_effects *input_effects)
{
	struct common_inputff_effect *inputff_effect;
	int i = 0, ret = 0;

	ddata->compose.num_of_compose_effects = input_effects->num_of_effects;
	ddata->compose.compose_repeat = input_effects->repeat;

	pr_info("%s num_of_effects:%d repeat:%d\n", __func__,
		ddata->compose.num_of_compose_effects, ddata->compose.compose_repeat);

	if (ddata->compose.num_of_compose_effects > MAX_COMPOSE_EFFECT) {
		pr_err("%s error. num_of_compose_effects=%d\n", __func__,
			ddata->compose.num_of_compose_effects);
		ret = -EFBIG;
		goto err;
	}

	for (i = 0; i < ddata->compose.num_of_compose_effects; i++) {
		inputff_effect = &input_effects->effects[i];

		switch (inputff_effect->type) {
		case FF_CONSTANT:
			ddata->effects[i].gain = (ddata->effect_gain * inputff_effect->scale) / 100;
			ddata->effects[i].compose_effects.type = inputff_effect->type;
			ddata->effects[i].compose_effects.id = ddata->compose.compose_effect_id;
			ddata->effects[i].compose_effects.replay.length = inputff_effect->duration;
			ddata->effects[i].compose_effects.u.constant.level = inputff_effect->frequency;
			pr_info("%s <%d> gain:%d type:%d duration:%d\n", __func__, i,
				ddata->effects[i].gain, ddata->effects[i].compose_effects.type,
				ddata->effects[i].compose_effects.replay.length);
			break;
		case FF_PERIODIC:
			ddata->effects[i].gain = (ddata->effect_gain * inputff_effect->scale) / 100;
			ddata->effects[i].compose_effects.type = inputff_effect->type;
			ddata->effects[i].compose_effects.id = ddata->compose.compose_effect_id;
			ddata->effects[i].compose_effects.replay.length = inputff_effect->duration;
			ddata->effects[i].compose_effects.u.periodic.offset = inputff_effect->effect_id;
			ddata->effects[i].compose_effects.u.periodic.waveform = 0;
			pr_info("%s <%d> gain:%d type:%d sep index:%d duration:%d\n", __func__, i,
				ddata->effects[i].gain, ddata->effects[i].compose_effects.type,
				ddata->effects[i].compose_effects.u.periodic.offset,
				ddata->effects[i].compose_effects.replay.length);
			break;
		case 0:
			ddata->effects[i].compose_effects.type = 0;
			ddata->effects[i].compose_effects.replay.length = inputff_effect->duration;
			pr_info("%s <%d> type:%d duration:%d\n", __func__, i,
				ddata->effects[i].compose_effects.type,
				ddata->effects[i].compose_effects.replay.length);
			break;
		default:
			pr_err("%s invalid effect type=%d\n", __func__, inputff_effect->type);
			ret = -EINVAL;
			goto err;
		}
	}
err:
	return ret;
}

static void compose_effects_play_work(struct kthread_work *work)
{
	struct sec_vib_inputff_compose *compose =
	    container_of(work, struct sec_vib_inputff_compose, kwork);
	struct sec_vib_inputff_drvdata *ddata =
	    container_of(compose, struct sec_vib_inputff_drvdata, compose);
	struct input_dev *dev = ddata->input;
	struct ff_effect *effect = NULL;
	u16 gain = 0;
	int ret = 0, i = 0, type = 0, duration = 0;

	ddata->compose.thread_state = COMPOSE_START;

	if (ddata->compose.num_of_compose_effects < 1) {
		pr_err("%s error. ddata->compose.num_of_compose_effects=%d\n", __func__,
			ddata->compose.num_of_compose_effects);
		goto exit;
	}

	while (!ddata->compose.thread_exit) {
		gain = ddata->effects[i].gain;
		effect = &ddata->effects[i].compose_effects;
		type = ddata->effects[i].compose_effects.type;
		duration = effect->replay.length;

		switch (type) {
		case FF_CONSTANT:
		case FF_PERIODIC:
			pr_info("%s type %s duration %dms\n", __func__,
				(type == FF_CONSTANT) ? "FF_CONSTANT" : "FF_PERIODIC", duration);
			if (ddata->vib_ops->set_gain)
				ddata->vib_ops->set_gain(dev, gain);

			if (ddata->compose.thread_exit) {
				ddata->compose.compose_effect_id = -1;
				break;
			}

			ret = ddata->vib_ops->upload(dev, effect, NULL);
			if (ret) {
				pr_err("%s error. upload ret=%d\n", __func__, ret);
				ddata->compose.compose_effect_id = -1;
				goto exit;
			}

			if (ddata->compose.thread_exit)
				break;

			if (ddata->vib_ops->playback)
				ddata->vib_ops->playback(dev, effect->id, 1);
			break;
		case 0:
			pr_info("%s delay duration %dms\n", __func__, duration);
			break;
		default:
			pr_err("%s error. type=%d\n", __func__, type);
			break;
		}
		wait_event_interruptible_timeout(ddata->compose.delay_wait,
			ddata->compose.thread_exit, msecs_to_jiffies(duration));

		if (type == FF_CONSTANT || type == FF_PERIODIC) {
			if (ddata->vib_ops->playback)
				ddata->vib_ops->playback(dev, effect->id, 0);
		}

		if (ddata->compose.thread_exit)
			break;

		i++;

		if (i >= ddata->compose.num_of_compose_effects) {
			if (ddata->compose.compose_repeat)
				i = 0;
			else
				break;
		}

		if (type == FF_CONSTANT || type == FF_PERIODIC) {
			if (ddata->vib_ops->erase)
				ret = ddata->vib_ops->erase(dev, effect->id);
		}
	}

exit:
	ddata->compose.thread_state = COMPOSE_EXIT;
	pr_info("%s exit\n", __func__);
	ddata->compose.thread_exit = 1;
}

static void stop_compose_effects_thread(struct sec_vib_inputff_drvdata *ddata)
{
	pr_info("%s\n", __func__);
	if (ddata->compose.compose_thread) {
		if (ddata->compose.thread_state != COMPOSE_STOP) {
			ddata->compose.thread_exit = 1;
			wake_up_interruptible(&ddata->compose.delay_wait);
		}
	}
}

static int sec_vib_inputff_upload_effect(struct input_dev *dev,
					 struct ff_effect *effect,
					 struct ff_effect *old)
{
	struct sec_vib_inputff_drvdata *ddata = input_get_drvdata(dev);
	struct common_inputff_effects input_compose_effects;
	int ret = 0;

	if (ddata->use_common_inputff) {
		if (effect->u.periodic.custom_len > sizeof(int)) {
			if (copy_from_user(&input_compose_effects,
					effect->u.periodic.custom_data,
					sizeof(struct common_inputff_effects))) {
				ret = -ENODATA;
				goto err;
			}
			ddata->compose.compose_effect_id = effect->id;
			ret = parsing_compose_effects(ddata, &input_compose_effects);
			if (ret)
				goto err;
			ddata->compose.upload_compose_effect = 1;
		} else {
			if (ddata->vib_ops->upload)
				ret = ddata->vib_ops->upload(dev, effect, old);
		}
	} else {
		if (ddata->vib_ops->upload)
			ret = ddata->vib_ops->upload(dev, effect, old);
	}

err:
	return ret;
}

static int sec_vib_inputff_erase(struct input_dev *dev, int effect_id)
{
	struct sec_vib_inputff_drvdata *ddata = input_get_drvdata(dev);
	int ret = 0;

	if (ddata->use_common_inputff) {
		if (ddata->compose.upload_compose_effect) {
			stop_compose_effects_thread(ddata);
			kthread_flush_work(&ddata->compose.kwork);
			ddata->compose.thread_state = COMPOSE_STOP;
			ddata->compose.upload_compose_effect = 0;
			ddata->compose.num_of_compose_effects = 0;
			ddata->compose.compose_repeat = 0;
			if (ddata->compose.compose_effect_id == -1)
				goto exit;
			ddata->compose.compose_effect_id = -1;
		}
		if (ddata->vib_ops->erase)
			ret = ddata->vib_ops->erase(dev, effect_id);
	} else {
		if (ddata->vib_ops->erase)
			ret = ddata->vib_ops->erase(dev, effect_id);
	}
exit:
	return ret;
}

static int sec_vib_inputff_playback(struct input_dev *dev, int effect_id,
				    int val)
{
	struct sec_vib_inputff_drvdata *ddata = input_get_drvdata(dev);
	int ret = 0;

	if (ddata->use_common_inputff) {
		if (ddata->compose.upload_compose_effect) {
			if (val) {
				ddata->compose.thread_exit = 0;
				kthread_queue_work(&ddata->compose.kworker, &ddata->compose.kwork);
				ddata->compose.thread_state = COMPOSE_RUN;
			} else
				stop_compose_effects_thread(ddata);
		} else {
			if (ddata->vib_ops->playback)
				ret = ddata->vib_ops->playback(dev, effect_id, val);
		}
	} else {
		if (ddata->vib_ops->playback)
			ret = ddata->vib_ops->playback(dev, effect_id, val);
	}

	return ret;
}

static void sec_vib_inputff_set_gain(struct input_dev *dev, u16 gain)
{
	struct sec_vib_inputff_drvdata *ddata = input_get_drvdata(dev);

	ddata->effect_gain = gain;
	if (ddata->vib_ops->set_gain)
		ddata->vib_ops->set_gain(dev, gain);
}

int sec_vib_inputff_register(struct sec_vib_inputff_drvdata *ddata)
{
	int i, ret = 0, ff = 0, ff_cnt = 0;
	struct device *dev = NULL;

	if (!ddata) {
		ret = -ENODEV;
		pr_err("%s no ddata\n", __func__);
		goto err_nodev;
	}
	dev = ddata->dev;

	pr_info("%s +++\n", __func__);

	ddata->input = devm_input_allocate_device(dev);
	if (!ddata->input) {
		ret = -ENOMEM;
		dev_err(dev, "%s no memory\n", __func__);
		goto err_nomem;
	}

	ddata->input->name = "sec_vibrator_inputff";
	ddata->input->id.product = ddata->devid;
	ddata->input->id.version = ddata->revid;

	input_set_drvdata(ddata->input, ddata);

	init_waitqueue_head(&ddata->compose.delay_wait);
	kthread_init_worker(&ddata->compose.kworker);
	kthread_init_work(&ddata->compose.kwork, compose_effects_play_work);

	ddata->compose.compose_thread = kthread_run(kthread_worker_fn,
			&ddata->compose.kworker, "compose-effects");
	if (IS_ERR(ddata->compose.compose_thread)) {
		pr_err("%s Unable to start compose effects thread\n", __func__);
		ret = PTR_ERR(ddata->compose.compose_thread);
		goto err_nomem;
	}

	for (ff = FF_EFFECT_MIN; ff <= FF_MAX_EFFECTS; ff++) {
		if (sec_vib_inputff_getbit(ddata, ff)) {
			input_set_capability(ddata->input, EV_FF, ff);
			ff_cnt++;
		}
	}

	ret = input_ff_create(ddata->input, ff_cnt);
	if (ret) {
		pr_err("Failed to create FF device: %d\n", ret);
		goto err_input_ff;
	}

	/*
	 * input_ff_create() automatically sets FF_RUMBLE capabilities;
	 * we want to restrtict this to only FF_PERIODIC
	 */
	__clear_bit(FF_RUMBLE, ddata->input->ffbit);

	ddata->input->ff->upload = sec_vib_inputff_upload_effect;
	ddata->input->ff->erase = sec_vib_inputff_erase;
	ddata->input->ff->playback = sec_vib_inputff_playback;
	ddata->input->ff->set_gain = sec_vib_inputff_set_gain;

	ret = input_register_device(ddata->input);
	if (ret) {
		pr_err("Cannot register input device: %d\n", ret);
		goto err_input_reg;
	}

	for (i = 0; ddata->vendor_dev_attr_groups[i]; i++) {
		ret = sysfs_create_group(&ddata->input->dev.kobj,
			ddata->vendor_dev_attr_groups[i]);
		if (ret) {
			pr_err("Failed to create sysfs groups[%d] : ret - %d\n", i, ret);
			goto err_sysfs;
		}
	}
	ret = sec_vib_inputff_sysfs_init(ddata);
	if (ret) {
		pr_err("Failed to create inputff sysfs group: %d\n", ret);
		goto err_sysfs;
	}

	ddata->pdata = &sec_vib_inputff_pdata;

#if defined(CONFIG_SEC_VIB_FOLD_MODEL)
	sec_vib_inputff_event_cmd(ddata);
#endif

	ddata->vibe_init_success = true;

	pr_info("%s ---\n", __func__);
	return ret;
err_sysfs:
	if (ddata->input)
		input_unregister_device(ddata->input);
err_input_reg:
err_input_ff:
	kthread_stop(ddata->compose.compose_thread);
err_nomem:
err_nodev:
	return ret;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_register);

void sec_vib_inputff_unregister(struct sec_vib_inputff_drvdata *ddata)
{
	struct device *dev = NULL;

	if (!ddata) {
		pr_err("%s no ddata\n", __func__);
		goto fail;
	}
	dev = ddata->dev;

	pr_info("%s +++\n", __func__);

	if (ddata->vibe_init_success) {
		int i;
		sec_vib_inputff_sysfs_exit(ddata);
		for (i = 0; ddata->vendor_dev_attr_groups[i]; i++) {
			sysfs_remove_group(&ddata->input->dev.kobj,
				ddata->vendor_dev_attr_groups[i]);
		}
	}
	if (ddata->input)
		input_unregister_device(ddata->input);

	if (ddata->compose.compose_thread)
		kthread_stop(ddata->compose.compose_thread);

	ddata->vibe_init_success = false;

fail:
	return;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_unregister);

/*
 * sec_vib_inputff_recheck_gain() - facilitate drvier IC to finds
 *		the appropriate gain ratio by checking temperature
 *		& fold position.
 * @ddata - driver data passed from the driver ic.
 */
int sec_vib_inputff_recheck_gain(struct sec_vib_inputff_drvdata *ddata)
{
	int temp = sec_vib_inputff_get_current_temp(ddata);

	if (temp >= ddata->pdata->high_temp_ref)
		return ddata->pdata->high_temp_ratio;
#if defined(CONFIG_SEC_VIB_FOLD_MODEL)
	else
		return set_fold_model_ratio(ddata);
#endif

	return ddata->pdata->normal_ratio;
}

/*
 * sec_vib_inputff_tune_gain() - facilitate drvier IC to finds
 *		the appropriate gain ratio by checking temperature
 *		& fold position.
 * @ddata - driver data passed from the driver ic.
 * @gain - Currently selected gain value
 *
 * return : Tuned gain value based on temperature, fold state, etc.,
 */
int sec_vib_inputff_tune_gain(struct sec_vib_inputff_drvdata *ddata, int gain)
{
	int ratio = sec_vib_inputff_recheck_gain(ddata);
	int tuned_gain = (gain * ratio) / 100;

	pr_info("%s: gain: %d, ratio: %d, tunned gain: %d\n",
			__func__, gain, ratio, tuned_gain);

	return tuned_gain;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_tune_gain);

/*
 * sec_vib_inputff_parse_dt - parses hw details from dts
 */
static int sec_vib_inputff_parse_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	int temp;

	pr_info("%s +++\n", __func__);

	if (unlikely(!np)) {
		pr_err("err: could not parse dt node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "haptic,normal_ratio", &temp);
	if (ret) {
		pr_err("%s: WARNING! normal_ratio not found\n", __func__);
		sec_vib_inputff_pdata.normal_ratio = 100;  // 100%
	} else
		sec_vib_inputff_pdata.normal_ratio = (int)temp;

	ret = of_property_read_u32(np, "haptic,overdrive_ratio", &temp);
	if (ret) {
		pr_info("%s: overdrive_ratio isn't used\n", __func__);
		sec_vib_inputff_pdata.overdrive_ratio = sec_vib_inputff_pdata.normal_ratio;
	} else
		sec_vib_inputff_pdata.overdrive_ratio = (int)temp;

	ret = of_property_read_u32(np, "haptic,high_temp_ref", &temp);
	if (ret) {
		pr_info("%s: high temperature concept isn't used\n", __func__);
		sec_vib_inputff_pdata.high_temp_ref = SEC_VIBRATOR_INPUTFF_DEFAULT_HIGH_TEMP_REF;
	} else {
		sec_vib_inputff_pdata.high_temp_ref = (int)temp;

		ret = of_property_read_u32(np, "haptic,high_temp_ratio", &temp);
		if (ret) {
			pr_info("%s: high_temp_ratio isn't used\n", __func__);
			sec_vib_inputff_pdata.high_temp_ratio = SEC_VIBRATOR_INPUTFF_DEFAULT_HIGH_TEMP_RATIO;
		}
		else
			sec_vib_inputff_pdata.high_temp_ratio = (int)temp;
	}

#if defined(CONFIG_SEC_VIB_FOLD_MODEL)
	ret = of_property_read_string(np, "haptic,fold_string",
		(const char **)&sec_vib_inputff_pdata.fold_cmd);
	if (ret < 0)
		pr_err("%s: WARNING! fold_string not found\n", __func__);

	ret = of_property_read_u32(np,
			"haptic,tent_open_ratio", &temp);
	if (ret) {
		pr_err("%s: WARNING! tent_open_ratio not found\n", __func__);
		sec_vib_inputff_pdata.tent_open_ratio = sec_vib_inputff_pdata.normal_ratio;
	} else
		sec_vib_inputff_pdata.tent_open_ratio = (int)temp;

	ret = of_property_read_u32(np,
			"haptic,tent_close_ratio", &temp);
	if (ret) {
		pr_err("%s: WARNING! tent_close_ratio not found\n", __func__);
		sec_vib_inputff_pdata.tent_close_ratio = sec_vib_inputff_pdata.normal_ratio;
	} else
		sec_vib_inputff_pdata.tent_close_ratio = (int)temp;

	ret = of_property_read_u32(np, "haptic,fold_open_ratio", &temp);
	if (ret) {
		pr_err("%s: WARNING! fold_open_ratio not found\n", __func__);
		sec_vib_inputff_pdata.fold_open_ratio = sec_vib_inputff_pdata.normal_ratio;
	} else
		sec_vib_inputff_pdata.fold_open_ratio = (int)temp;

	ret = of_property_read_u32(np,
			"haptic,fold_close_ratio", &temp);
	if (ret) {
		pr_err("%s: WARNING! fold_close_ratio not found\n", __func__);
		sec_vib_inputff_pdata.fold_close_ratio = sec_vib_inputff_pdata.normal_ratio;
	} else
		sec_vib_inputff_pdata.fold_close_ratio = (int)temp;
#endif

	ret = of_property_read_string(np, "haptic,f0_cal_way",
		(const char **)&sec_vib_inputff_pdata.f0_cal_way);
	if (ret < 0) {
		sec_vib_inputff_pdata.f0_cal_way = "NONE";
		pr_err("%s: WARNING! F0 Calibration Method not found\n", __func__);
	}

	pr_info("%s : done! ---\n", __func__);
	return 0;
}

static int sec_vib_inputff_probe(struct platform_device *pdev)
{
	pr_info("%s +++\n", __func__);

	if (unlikely(sec_vib_inputff_parse_dt(pdev)))
		pr_err("%s: WARNING!>..parse dt failed\n", __func__);

	sec_vib_inputff_pdata.probe_done = true;
	pr_info("%s : done! ---\n", __func__);

	return 0;
}


static const struct of_device_id sec_vib_inputff_id[] = {
	{ .compatible = "sec_vib_inputff" },
	{ }
};

static struct platform_driver sec_vib_inputff_driver = {
	.probe		= sec_vib_inputff_probe,
	.driver		= {
		.name	=  "sec_vib_inputff",
		.owner	= THIS_MODULE,
		.of_match_table = sec_vib_inputff_id,
	},
};

module_platform_driver(sec_vib_inputff_driver);

MODULE_AUTHOR("Samsung electronics <wookwang.lee@samsung.com>, Samsung vibrator team members");
MODULE_DESCRIPTION("sec_vibrator_inputff");
MODULE_LICENSE("GPL");
