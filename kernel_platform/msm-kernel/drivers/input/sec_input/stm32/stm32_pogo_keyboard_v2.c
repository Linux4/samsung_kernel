/******************** (C) COPYRIGHT 2019 Samsung Electronics ********************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *******************************************************************************/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_gpio.h>
#endif
#include <linux/input/pogo_i2c_notifier.h>
#include "stm32_pogo_i2c.h"
#define STM32_KEY_MAX		762

#define STM32KPD_DRV_NAME			"stm32_kpd"

#define INPUT_VENDOR_ID_SAMSUNG			0x04E8
#define INPUT_PRODUCT_ID_POGO_KEYBOARD		0xA035

#define STM32_KPC_DATA_PRESS			1
#define STM32_GAMEPAD_KEY_START		304
#define STM32_GAMEPAD_KEY_END		318

#define STM32_CHECK_LED(STM32, IS_LED_ENABLED, LED) do {		\
	if (test_bit(LED, (STM32)->input_dev->led))			\
		IS_LED_ENABLED = true;					\
	else							\
		IS_LED_ENABLED = false;			\
	} while (0)


struct stm32_keyevent_data {
	union {
		struct {
			u16 key_value:15;
			u16 press:1;
		} __packed;
		u16 data;
	};
};

struct stm32_keypad_dtdata {
	const char				*input_name;
};

struct stm32_keypad_dev {
	struct platform_device			*pdev;
	struct input_dev			*input_dev;
	struct mutex				dev_lock;

	struct stm32_keypad_dtdata		*dtdata;
	int					key_state[STM32_KEY_MAX];
	struct notifier_block			pogo_nb;
};

static void stm32_release_all_key(struct stm32_keypad_dev *stm32)
{
	int i;

	if (!stm32->input_dev)
		return;

	input_info(true, &stm32->pdev->dev, "%s\n", __func__);

	for (i = 0; i < STM32_KEY_MAX; i++) {
		if (stm32->key_state[i]) {
			input_report_key(stm32->input_dev,
					i, 0);
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &stm32->pdev->dev, "RA code(%d)\n",
					i);
#else
			input_info(true, &stm32->pdev->dev, "RA\n");
#endif
		}
	}
	input_sync(stm32->input_dev);
}

static void stm32_pogo_kpd_event(struct stm32_keypad_dev *stm32, u16 *key_values, int len)
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
		if (keydata.key_value < STM32_KEY_MAX) {
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

#if IS_ENABLED(CONFIG_OF)
static int stm32_parse_dt(struct device *dev,
		struct stm32_keypad_dev *device_data)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_string(np, "keypad,input_name", &device_data->dtdata->input_name);
	if (ret < 0) {
		input_err(true, dev, "unable to get model_name\n");
		return ret;
	}


	input_info(true, &device_data->pdev->dev, "%s \n",
			device_data->dtdata->input_name);
	return 0;
}
#else
static int stm32_parse_dt(struct device *dev,
		struct stm32_keypad_dev *device_data)
{
	return -ENODEV;
}
#endif
void stm32_toggle_led(struct stm32_keypad_dev *device_data)
{
	bool led_enabled = false;

	STM32_CHECK_LED(device_data, led_enabled, LED_CAPSL); 

	if (led_enabled) {
		stm32_caps_led_value = 0x2;
		
	} else {
		stm32_caps_led_value = 0x1;
	}
	pr_info("%s %s  led_value:%d\n", SECLOG, __func__, stm32_caps_led_value);
}

static int stm32_caps_event(struct input_dev *dev,
			unsigned int type, unsigned int code, int value)
{
	struct stm32_keypad_dev *device_data = input_get_drvdata(dev);

	switch (type) {
	case EV_LED:
		stm32_toggle_led(device_data);
		return 0;
	default:
		pr_err("%s %s(): Got unknown type %d, code %d, value %d\n",
				SECLOG, __func__, type, code, value);
	}

	return -1;
}

static int stm32_keypad_set_input_dev(struct stm32_keypad_dev *device_data, struct pogo_data_struct pogo_data)
{
	struct input_dev *input_dev;
	int ret = 0;
	int i = 0;

	if (device_data->input_dev) {
		input_err(true, &device_data->pdev->dev, "%s input dev already exist.\n", __func__);
		return ret;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &device_data->pdev->dev, "%s: Failed to allocate memory for input device\n", __func__);
		return -ENOMEM;
	}

	device_data->input_dev = input_dev;
	device_data->input_dev->dev.parent = &device_data->pdev->dev;
	if (device_data->dtdata->input_name)
		device_data->input_dev->name = device_data->dtdata->input_name;
	else
		device_data->input_dev->name = "Book Cover Keyboard";

	input_info(true, &device_data->pdev->dev, "%s: input_name: %s\n", __func__, device_data->input_dev->name);
	device_data->input_dev->id.bustype = BUS_I2C;
	device_data->input_dev->id.vendor = INPUT_VENDOR_ID_SAMSUNG;
	device_data->input_dev->id.product = INPUT_PRODUCT_ID_POGO_KEYBOARD;
	device_data->input_dev->flush = NULL;
	device_data->input_dev->event = stm32_caps_event;

	input_set_drvdata(device_data->input_dev, device_data);
	set_bit(EV_KEY, device_data->input_dev->evbit);
	__set_bit(EV_LED, device_data->input_dev->evbit);
	__set_bit(LED_CAPSL, device_data->input_dev->ledbit);

	for (i = 0; i < STM32_KEY_MAX; i++) {
		if ((i >= STM32_GAMEPAD_KEY_START && i <= STM32_GAMEPAD_KEY_END) || i == BTN_TOUCH)
			continue;
		set_bit(i, input_dev->keybit);
		device_data->key_state[i] = 0;
	}

	ret = input_register_device(device_data->input_dev);
	if (ret) {
		input_err(true, &device_data->pdev->dev, "%s: Failed to register input device\n", __func__);
		device_data->input_dev = NULL;
		return ret;
	}
	input_info(true, &device_data->pdev->dev, "%s: done\n", __func__);
	return ret;
}

static int stm32_kpd_pogo_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct stm32_keypad_dev *stm32 = container_of(nb, struct stm32_keypad_dev, pogo_nb);
	struct pogo_data_struct pogo_data =  *(struct pogo_data_struct *)data;

	mutex_lock(&stm32->dev_lock);

	switch (action) {
	case POGO_NOTIFIER_ID_ATTACHED:
		stm32_keypad_set_input_dev(stm32, pogo_data);
		break;
	case POGO_NOTIFIER_ID_DETACHED:
		stm32_release_all_key(stm32);
		if (stm32->input_dev) {
			input_unregister_device(stm32->input_dev);
			stm32->input_dev = NULL;
			input_info(true, &stm32->pdev->dev, "%s: input dev unregistered\n", __func__);
		}
		break;
	case POGO_NOTIFIER_ID_RESET:
		stm32_release_all_key(stm32);
		break;
	case POGO_NOTIFIER_EVENTID_KEYPAD:
		stm32_pogo_kpd_event(stm32, (u16 *)pogo_data.data, pogo_data.size);
		break;
	};

	mutex_unlock(&stm32->dev_lock);

	return NOTIFY_DONE;
}

static int stm32_keypad_probe(struct platform_device *pdev)
{
	struct stm32_keypad_dev *device_data;
	int ret = 0;

	input_info(true, &pdev->dev, "%s\n", __func__);

	device_data = kzalloc(sizeof(struct stm32_keypad_dev), GFP_KERNEL);
	if (!device_data) {
		input_err(true, &pdev->dev, "%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	device_data->pdev = pdev;

	platform_set_drvdata(pdev, device_data);

	mutex_init(&device_data->dev_lock);

	if (pdev->dev.of_node) {
		device_data->dtdata = kzalloc(sizeof(struct stm32_keypad_dtdata), GFP_KERNEL);
		if (!device_data->dtdata) {
			input_err(true, &pdev->dev, "%s: Failed to allocate memory\n", __func__);
			ret = -ENOMEM;
			goto err_config;
		}
		ret = stm32_parse_dt(&pdev->dev, device_data);
		if (ret) {
			input_err(true, &pdev->dev, "%s: Failed to use device tree\n", __func__);
			ret = -EIO;
			goto err_config;
		}

	} else {
		input_err(true, &device_data->pdev->dev, "Not use device tree\n");
		device_data->dtdata = pdev->dev.platform_data;
		if (!device_data->dtdata) {
			input_err(true, &pdev->dev, "%s: failed to get platform data\n", __func__);
			ret = -ENOENT;
			goto err_config;
		}
	}

	pogo_notifier_register(&device_data->pogo_nb,
			stm32_kpd_pogo_notifier, POGO_NOTIFY_DEV_KEYPAD);

	input_info(true, &device_data->pdev->dev, "%s: done\n", __func__);

	return ret;

err_config:
	mutex_destroy(&device_data->dev_lock);

	kfree(device_data);

	input_err(true, &pdev->dev, "Error at %s\n", __func__);

	return ret;
}

static int stm32_keypad_remove(struct platform_device *pdev)
{
	struct stm32_keypad_dev *device_data = platform_get_drvdata(pdev);

	pogo_notifier_unregister(&device_data->pogo_nb);

	input_free_device(device_data->input_dev);

	mutex_destroy(&device_data->dev_lock);

	kfree(device_data);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id stm32_match_table[] = {
	{ .compatible = "stm,keypad",},
	{ },
};
#else
#define stm32_match_table NULL
#endif

static struct platform_driver stm32_pogo_kpd_driver = {
	.driver = {
		.name = STM32KPD_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = stm32_match_table,
	},
	.probe = stm32_keypad_probe,
	.remove = stm32_keypad_remove,
};

int stm32_pogo_kpd_init(void)
{
	pr_info("%s %s\n", SECLOG, __func__);

	return platform_driver_probe(&stm32_pogo_kpd_driver, stm32_keypad_probe);
}
EXPORT_SYMBOL(stm32_pogo_kpd_init);

void stm32_pogo_kpd_exit(void)
{
	pr_info("%s %s\n", SECLOG, __func__);
	platform_driver_unregister(&stm32_pogo_kpd_driver);
}
EXPORT_SYMBOL(stm32_pogo_kpd_exit);

MODULE_DESCRIPTION("STM32 Keypad Driver for POGO Keyboard V2");
MODULE_AUTHOR("Samsung");
