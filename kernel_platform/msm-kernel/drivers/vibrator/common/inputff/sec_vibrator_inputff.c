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
#include <linux/vibrator/sec_vibrator_inputff.h>

#if defined(CONFIG_SEC_KUNIT)
#include "kunit_test/sec_vibrator_inputff_test.h"
#else
#define __visible_for_testing static
#endif

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
	int ret = 0, ff = 0;
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
		if (sec_vib_inputff_getbit(ddata, ff))
			input_set_capability(ddata->input, EV_FF, ff);
	}

	ret = input_ff_create(ddata->input, FF_MAX_EFFECTS);
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

	ret = sysfs_create_group(&ddata->input->dev.kobj,
			ddata->dev_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs group: %d\n", ret);
		goto err_sysfs;
	}
	ret = sysfs_create_group(&ddata->input->dev.kobj,
			ddata->dev_attr_cal_group);
	if (ret) {
		pr_err("Failed to create cal sysfs group: %d\n", ret);
		goto err_sysfs;
	}
	ret = sec_vib_inputff_sysfs_init(ddata);
	if (ret) {
		pr_err("Failed to create inputff sysfs group: %d\n", ret);
		goto err_sysfs;
	}
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
		sec_vib_inputff_sysfs_exit(ddata);
		sysfs_remove_group(&ddata->input->dev.kobj,
				ddata->dev_attr_group);
		sysfs_remove_group(&ddata->input->dev.kobj,
				ddata->dev_attr_cal_group);
	}
	if (ddata->input)
		input_unregister_device(ddata->input);
	ddata->vibe_init_success = false;

fail:
	return;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_unregister);

MODULE_AUTHOR("Samsung electronics <wookwang.lee@samsung.com>");
MODULE_DESCRIPTION("sec_vibrator_inputff");
MODULE_LICENSE("GPL");
