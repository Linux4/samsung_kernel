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
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/vibrator/sec_vibrator_inputff.h>

#if defined(CONFIG_SEC_KUNIT)
#include "kunit_test/sec_vibrator_inputff_test.h"
#else
#define __visible_for_testing static
#endif

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

	input_set_drvdata(ddata->input, ddata->private_data);
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

	ddata->input->ff->upload = ddata->vib_ops->upload;
	ddata->input->ff->playback = ddata->vib_ops->playback;
	ddata->input->ff->set_gain = ddata->vib_ops->set_gain;
	ddata->input->ff->erase = ddata->vib_ops->erase;

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
	} else {
		sec_vib_inputff_pdata.high_temp_ref = (int)temp;

		ret = of_property_read_u32(np, "haptic,high_temp_ratio", &temp);
		if (ret)
			pr_info("%s: high_temp_ratio isn't used\n", __func__);
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

MODULE_AUTHOR("Samsung electronics <wookwang.lee@samsung.com>");
MODULE_DESCRIPTION("sec_vibrator_inputff");
MODULE_LICENSE("GPL");

