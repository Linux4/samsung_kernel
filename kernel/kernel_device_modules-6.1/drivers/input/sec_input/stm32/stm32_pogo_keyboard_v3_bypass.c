#include "stm32_pogo_keyboard_v3.h"

static void release_all_key_bypass(struct stm32_keypad_dev *stm32);
static int keypad_set_input_dev_bypass(struct stm32_keypad_dev *stm32, struct pogo_data_struct pogo_data);
static void pogo_kpd_event_bypass(struct stm32_keypad_dev *stm32, u16 *key_values, int len);

static struct stm32_keypad_dev_function bypass = {
	release_all_key_bypass,
	keypad_set_input_dev_bypass,
	pogo_kpd_event_bypass,
};

struct stm32_keypad_dev_function *setup_stm32_keypad_dev_bypass(void)
{
	return &bypass;
}

static void release_all_key_bypass(struct stm32_keypad_dev *stm32)
{
	int i;

	if (!stm32->input_dev)
		return;
	input_info(true, &stm32->pdev->dev, "%s\n", __func__);
	for (i = 0; i < KEY_MAX; i++) {
		if (stm32->key_state[i]) {
			input_report_key(stm32->input_dev, i, 0);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &stm32->pdev->dev, "RA code(%d)\n", i);
#else
			input_info(true, &stm32->pdev->dev, "RA\n");
#endif
		}
	}
	input_sync(stm32->input_dev);
}

static int keypad_set_input_dev_bypass(struct stm32_keypad_dev *stm32, struct pogo_data_struct pogo_data)
{
	struct input_dev *input_dev;
	static char name[128];
	int ret = 0;
	int i = 0;
	bool need_to_set_ai_key = false;

	if (stm32->input_dev) {
		input_err(true, &stm32->pdev->dev, "%s input dev already exist.\n", __func__);
		return ret;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &stm32->pdev->dev, "%s: Failed to allocate memory for input device\n", __func__);
		return -ENOMEM;
	}

	stm32->input_dev = input_dev;
	stm32->input_dev->dev.parent = &stm32->pdev->dev;

	if (pogo_data.model_info) {
		if (pogo_data.model_info->is_oem)
			snprintf(name, sizeof(name), "%s", pogo_data.model_info->name);
		else
			snprintf(name, sizeof(name), "Book Cover Keyboard %s(%s)",
					pogo_data.model_info->has_touchpad ? "" : "Slim ",
					pogo_data.model_info->name);
		if (pogo_data.model_info->has_ai_key)
			need_to_set_ai_key = true;
	} else {
		snprintf(name, sizeof(name), INPUT_NAME_DEFAULT);
	}
	stm32->input_dev->name = name;

	input_info(true, &stm32->pdev->dev, "%s: input_name: %s\n", __func__, stm32->input_dev->name);
	stm32->input_dev->id.bustype = BUS_I2C;
	stm32->input_dev->id.vendor = INPUT_VENDOR_ID_SAMSUNG;
	stm32->input_dev->id.product = INPUT_PRODUCT_ID_POGO_KEYBOARD;
	stm32->input_dev->flush = NULL;
	stm32->input_dev->event = stm32_caps_event;

	input_set_drvdata(stm32->input_dev, stm32);
	set_bit(EV_KEY, stm32->input_dev->evbit);
	set_bit(EV_LED, stm32->input_dev->evbit);
	set_bit(LED_CAPSL, stm32->input_dev->ledbit);

	for (i = 0; i < KEY_MAX; i++) {
		if (i >= BTN_MISC && i < KEY_OK)
			continue;
		if (i == KEY_AI_HOT && !need_to_set_ai_key)
			continue;
		set_bit(i, input_dev->keybit);
		stm32->key_state[i] = 0;
	}

	ret = input_register_device(stm32->input_dev);
	if (ret) {
		input_err(true, &stm32->pdev->dev, "%s: Failed to register input device\n", __func__);
		input_free_device(input_dev);
		stm32->input_dev = NULL;
		return -ENODEV;
	}

	input_info(true, &stm32->pdev->dev, "%s: done\n", __func__);
	return ret;
}

static void pogo_kpd_event_bypass(struct stm32_keypad_dev *stm32, u16 *key_values, int len)
{
	int i, event_count;

	if (!stm32->input_dev) {
		input_err(true, &stm32->pdev->dev, "%s: input dev is null\n", __func__);
		return;
	}

	if (!key_values || !len) {
		input_err(true, &stm32->pdev->dev, "%s: no event data\n", __func__);
		return;
	}

	event_count = len / sizeof(struct stm32_keyevent_data);
	for (i = 0; i < event_count; i++) {
		struct stm32_keyevent_data keydata;

		keydata.data = key_values[i];

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &stm32->pdev->dev, "%s '%s' (%d)\n", __func__,
				keydata.press != STM32_KPC_DATA_PRESS ? "R" : "P", keydata.key_value);
#else
		input_info(true, &stm32->pdev->dev, "%s '%s'\n", __func__,
				keydata.press != STM32_KPC_DATA_PRESS ? "R" : "P");
#endif
		if (keydata.key_value < KEY_MAX) {
			if (keydata.press != STM32_KPC_DATA_PRESS)
				stm32->key_state[keydata.key_value] = 0;
			else
				stm32->key_state[keydata.key_value] = 1;
		}
		input_report_key(stm32->input_dev, keydata.key_value, keydata.press);
		input_sync(stm32->input_dev);
	}

	input_dbg(false, &stm32->pdev->dev, "%s--\n", __func__);
}
