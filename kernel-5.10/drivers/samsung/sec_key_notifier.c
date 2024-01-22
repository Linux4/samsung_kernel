/*
 * drivers/debug/sec_key_notifier.c
 *
 * COPYRIGHT(C) 2016 Samsung Electronics Co., Ltd. All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmg(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/input.h>
#include <linux/notifier.h>
#include <linux/slab.h>

#include <linux/sec_debug.h>

#include "sec_key_notifier.h"

static DEFINE_SPINLOCK(sec_kn_event_lock);

static ATOMIC_NOTIFIER_HEAD(sec_kn_notifier_list);

int sec_kn_register_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&sec_kn_notifier_list, nb);
}
EXPORT_SYMBOL(sec_kn_register_notifier);

int sec_kn_unregister_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&sec_kn_notifier_list, nb);
}
EXPORT_SYMBOL(sec_kn_unregister_notifier);

static void sec_kn_event(struct input_handle *handle, unsigned int event_type,
		unsigned int event_code, int value)
{
	struct sec_key_notifier_param param = {
		.keycode = event_code,
		.down = value,
	};
	int rc;

	spin_lock(&sec_kn_event_lock);

	/* BTN : 0x100 ~ 0x151, 0x2c0 ~ 0x2e7 */
	if (event_type != EV_KEY || (event_code >= 0x100 && event_code <= 0x151) || (event_code >= 0x2c0 && event_code <= 0x2e7))
		goto out;

	rc = atomic_notifier_call_chain(&sec_kn_notifier_list, 0, &param);

out:
	spin_unlock(&sec_kn_event_lock);
}

static int sec_kn_connect(struct input_handler *handler, struct input_dev *dev,
		const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "sec_key_notifier";

	pr_info("%s\n", __func__);

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

err_unregister_handle:
	pr_err("%s: err_unregister_handle!!\n", __func__);
	input_unregister_handle(handle);
err_free_handle:
	pr_err("%s: err_free_handle!!\n", __func__);
	kfree(handle);
	return error;
}

static void sec_kn_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id sec_kn_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{},
};

static struct input_handler sec_kn_handler = {
	.event		= sec_kn_event,
	.connect	= sec_kn_connect,
	.disconnect	= sec_kn_disconnect,
	.name		= "sec_key_notifier",
	.id_table	= sec_kn_ids,
};

int sec_kn_init(void)
{
	int err;

	pr_info("%s\n", __func__);

	err = input_register_handler(&sec_kn_handler);

	return err;
}
EXPORT_SYMBOL(sec_kn_init);



