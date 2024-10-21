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

#include "stm32_pogo_keyboard_v3.h"

static int stm32_parse_dt(struct device *dev, struct stm32_keypad_dev *stm32)
{
	struct device_node *np = dev->of_node;

	if (of_find_property(np, "keypad,num-rows", NULL))
		return stm32_keypad_parse_dt_row(dev, stm32);

	return 0;
}

void stm32_toggle_led(struct stm32_keypad_dev *stm32)
{
	bool led_enabled = false;

	STM32_CHECK_LED(stm32, led_enabled, LED_CAPSL);

	if (led_enabled) {
		stm32_caps_led_value = 0x2;

	} else {
		stm32_caps_led_value = 0x1;
	}
	pr_info("%s %s  led_value:%d\n", SECLOG, __func__, stm32_caps_led_value);
}

int stm32_caps_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	struct stm32_keypad_dev *stm32 = input_get_drvdata(dev);

	switch (type) {
	case EV_LED:
		stm32_toggle_led(stm32);
		return 0;
	default:
		pr_err("%s %s(): Got unknown type %d, code %d, value %d\n",
				SECLOG, __func__, type, code, value);
	}

	return -1;
}

static int stm32_kpd_pogo_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct stm32_keypad_dev *stm32 = container_of(nb, struct stm32_keypad_dev, pogo_nb);
	struct pogo_data_struct pogo_data = *(struct pogo_data_struct *)data;
	enum pogo_notifier_id_t notifier_action = (enum pogo_notifier_id_t)action;
	int ret = 0;

	mutex_lock(&stm32->dev_lock);
	ret = stm32_setup_attach_keypad(stm32, pogo_data);
	if (!ret)
		goto out;

	switch (notifier_action) {
	case POGO_NOTIFIER_ID_ATTACHED:
		stm32->keypad_set_input_dev(stm32, pogo_data);
		break;
	case POGO_NOTIFIER_ID_DETACHED:
		stm32->release_all_key(stm32);
		if (stm32->input_dev) {
			input_unregister_device(stm32->input_dev);
			stm32->input_dev = NULL;
			input_info(true, &stm32->pdev->dev, "%s: input dev unregistered\n", __func__);
		}
		break;
	case POGO_NOTIFIER_ID_RESET:
		stm32->release_all_key(stm32);
		break;
	case POGO_NOTIFIER_EVENTID_KEYPAD:
		stm32->pogo_kpd_event(stm32, (u16 *)pogo_data.data, pogo_data.size);
		break;
	case POGO_NOTIFIER_EVENTID_TOUCHPAD:
	case POGO_NOTIFIER_EVENTID_HALL:
		break;
	default:
		input_info(true, &stm32->pdev->dev, "%s: notifier_action type:%d\n", __func__, notifier_action);
	};
out:
	mutex_unlock(&stm32->dev_lock);

	return NOTIFY_DONE;
}

static int stm32_keypad_probe(struct platform_device *pdev)
{
	struct stm32_keypad_dev *stm32;
	int ret = 0;

	input_info(true, &pdev->dev, "%s\n", __func__);

	stm32 = devm_kzalloc(&pdev->dev, sizeof(struct stm32_keypad_dev), GFP_KERNEL);
	if (!stm32) {
		input_err(true, &pdev->dev, "%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	stm32->pdev = pdev;

	platform_set_drvdata(pdev, stm32);

	mutex_init(&stm32->dev_lock);

	if (pdev->dev.of_node) {
		ret = stm32_parse_dt(&pdev->dev, stm32);
		if (ret) {
			input_err(true, &pdev->dev, "%s: Failed to use device tree\n", __func__);
			ret = -EIO;
			goto err_config;
		}

	} else {
		input_err(true, &stm32->pdev->dev, "Not use device tree\n");
	}

	stm32_setup_probe_keypad(stm32);

	pogo_notifier_register(&stm32->pogo_nb, stm32_kpd_pogo_notifier, POGO_NOTIFY_DEV_KEYPAD);

	input_info(true, &stm32->pdev->dev, "%s: done\n", __func__);

	return ret;
err_config:
	mutex_destroy(&stm32->dev_lock);
	input_err(true, &pdev->dev, "Error at %s\n", __func__);
	return ret;
}

static int stm32_keypad_remove(struct platform_device *pdev)
{
	struct stm32_keypad_dev *stm32 = platform_get_drvdata(pdev);

	pogo_notifier_unregister(&stm32->pogo_nb);

	input_free_device(stm32->input_dev);

	mutex_destroy(&stm32->dev_lock);

	kfree(stm32);
	return 0;
}

static struct of_device_id stm32_match_table[] = {
	{ .compatible = "stm,keypad",},
	{ },
};

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
