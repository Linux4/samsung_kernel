//hs03s code for DEVAL5625-88 by ningkaixuan at 2022/02/15 start
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/input.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/sysrq.h>
#include <linux/jiffies.h>

/* You need to finish pressed all the keys in 5 seconds
 * after you start the process.
 */
#define TRIGGER_TIMEOUT		(msecs_to_jiffies(5000))
static u64 timeout_jiffies;

#define MAX_HIT_KEY_COUNT   6
static unsigned int all_trigger_keys[MAX_HIT_KEY_COUNT] = {
	KEY_VOLUMEUP,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_VOLUMEDOWN,
	KEY_POWER,
	KEY_POWER
};
static unsigned int next_valid_key_index;


static void reset_trigger(void)
{
	next_valid_key_index = 0;
}

static void panic_if_needed(void)
{
	if (next_valid_key_index != MAX_HIT_KEY_COUNT)
		return;

	if (time_is_before_jiffies64(timeout_jiffies)) {
		pr_info("timeout and reset\n");
		reset_trigger();
		return;
	}

	panic("crash from combine key\n");
}

static bool trigger_filter(struct input_handle *handle,
				unsigned int type, unsigned int code, int value)
{
	/* We only care EV_KEY and key up event will be ignored */
	if ((type != EV_KEY) || !value)
		return 0;

	if (next_valid_key_index >= MAX_HIT_KEY_COUNT) {
		pr_err("Something wrong, need reset\n");
		reset_trigger();
		return 0;
	}

	if (code == all_trigger_keys[next_valid_key_index]) {
		if (next_valid_key_index == 0)
			timeout_jiffies = get_jiffies_64() + TRIGGER_TIMEOUT;
		next_valid_key_index++;
		panic_if_needed();
	} else {
		reset_trigger();
	}

	return 0;
}

static int trigger_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "combine_key_trigger";

	error = input_register_handle(handle);
	if (error) {
		pr_err("Failed to register input handle, error %d\n", error);
		kfree(handle);
		return error;
	}

	error = input_open_device(handle);
	if (error) {
		pr_err("Failed to open input device, error %d\n", error);
		input_unregister_handle(handle);
		return error;
	}

	reset_trigger();

	return 0;
}

static void trigger_disconnect(struct input_handle *handle)
{
	pr_info("disconnect\n");
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id trigger_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { [BIT_WORD(EV_KEY)] = BIT_MASK(EV_KEY) },
	},
	{ },
};

static struct input_handler combine_key_trigger_handler = {
	.filter     = trigger_filter,
	.connect    = trigger_connect,
	.disconnect = trigger_disconnect,
	.name       = "combine_key_trigger",
	.id_table   = trigger_ids,
};

static int __init combine_key_trigger_init(void)
{
	int error;

	pr_info("init\n");
	error = input_register_handler(&combine_key_trigger_handler);
	if (error)
		pr_err("Failed to register input handler, error %d\n", error);

	return 0;
}

device_initcall(combine_key_trigger_init);
//hs03s code for DEVAL5625-88 by ningkaixuan at 2022/02/15 end

