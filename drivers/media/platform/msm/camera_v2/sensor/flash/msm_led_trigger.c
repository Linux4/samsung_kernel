/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#if defined(CONFIG_LEDS_MAX77804K)
#include <linux/leds-max77804k.h>
#include <linux/gpio.h>
#endif
#include "msm_led_flash.h"

#define FLASH_NAME "camera-led-flash"

/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_led_flash_ctrl_t fctrl;
unsigned int flash_widget_status = 0;

#if defined(CONFIG_LEDS_MAX77804K)
extern int led_flash_en;
extern int led_torch_en;
static int led_prev_mode = 0;
#endif

#if !defined(CONFIG_LEDS_MAX77804K)
extern struct class *camera_class; /*sys/class/camera*/
static struct device *flash_dev;

static ssize_t qpnp_led_flash(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t size)
{
	int tmp;
	uint32_t i;
	sscanf(buf, "%i", &tmp);
	CDBG("sysfs node: %d\n", tmp);
	switch (tmp) {
	case MSM_CAMERA_LED_OFF:
		fctrl.rear_flash_status=MSM_CAMERA_LED_OFF;
		flash_widget_status=0;
		for (i = 0; i < fctrl.num_sources; i++)
			if (fctrl.flash_trigger[i])
				led_trigger_event(fctrl.flash_trigger[i], 0);
		if (fctrl.torch_trigger)
			led_trigger_event(fctrl.torch_trigger, 0);
		break;

	case MSM_CAMERA_LED_LOW:
		fctrl.rear_flash_status=MSM_CAMERA_LED_LOW;
		flash_widget_status=1;
		if (fctrl.torch_trigger)
			led_trigger_event(fctrl.torch_trigger, fctrl.torch_op_current);
		break;

	case MSM_CAMERA_LED_HIGH:
		fctrl.rear_flash_status=MSM_CAMERA_LED_HIGH;
		flash_widget_status=1;
		for (i = 0; i < fctrl.num_sources; i++)
			if (fctrl.flash_trigger[i])
				led_trigger_event(fctrl.flash_trigger[i], 0);
		if (fctrl.torch_trigger)
			led_trigger_event(fctrl.torch_trigger, 0);
		break;
#if defined(CONFIG_MACH_VICTORLTE_CTC)
	case MSM_CAMERA_LED_FACTORY:
		fctrl.rear_flash_status=MSM_CAMERA_LED_LOW;
		if (fctrl.torch_trigger)
			led_trigger_event(fctrl.torch_trigger, 255);
		break;
#endif
#if defined(CONFIG_SEC_MEGA2_PROJECT)
	case MSM_CAMERA_LED_FACTORY:
		fctrl.rear_flash_status=MSM_CAMERA_LED_LOW;
		if (fctrl.torch_trigger)
			led_trigger_event(fctrl.torch_trigger, 1000);
		break;
#endif

	default:
		fctrl.rear_flash_status=MSM_CAMERA_LED_OFF;
		flash_widget_status=0;
		for (i = 0; i < fctrl.num_sources; i++)
			if (fctrl.flash_trigger[i])
				led_trigger_event(fctrl.flash_trigger[i], 0);
		if (fctrl.torch_trigger)
			led_trigger_event(fctrl.torch_trigger, 0);
		break;
	}
    return strnlen(buf, size);
}

static DEVICE_ATTR(rear_flash, S_IWUSR|S_IWGRP|S_IROTH,
	NULL, qpnp_led_flash);
#endif


static int32_t msm_led_trigger_get_subdev_id(struct msm_led_flash_ctrl_t *fctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	if (!subdev_id) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -EINVAL;
	}
	*subdev_id = fctrl->pdev->id;
	CDBG("%s:%d subdev_id %d\n", __func__, __LINE__, *subdev_id);
	return 0;
}
#if defined(CONFIG_MACH_AFYONLTE_TMO) || defined(CONFIG_MACH_VICTORLTE_CTC) \
	|| defined(CONFIG_MACH_AFYONLTE_CAN) \
	|| defined (CONFIG_MACH_AFYONLTE_MTR)
int32_t s5k4ecgx_set_flash(int mode)
{
	uint32_t i;
	if( flash_widget_status == 1 )
	{
		printk(" %s Dont handle flash..rear_flash is set\n",__func__);
		return 0;
	}
	switch (mode) {
	case MSM_CAMERA_LED_OFF:
	fctrl.rear_flash_status=MSM_CAMERA_LED_OFF;
	for (i = 0; i < fctrl.num_sources; i++)
	if (fctrl.flash_trigger[i])
		led_trigger_event(fctrl.flash_trigger[i], 0);
	if (fctrl.torch_trigger)
		led_trigger_event(fctrl.torch_trigger, 0);
	break;

	case MSM_CAMERA_LED_LOW:
	fctrl.rear_flash_status=MSM_CAMERA_LED_LOW;
	if (fctrl.torch_trigger)
		led_trigger_event(fctrl.torch_trigger, fctrl.torch_op_current);
	break;
	case MSM_CAMERA_LED_HIGH:
	fctrl.rear_flash_status=MSM_CAMERA_LED_HIGH;
	if (fctrl.torch_trigger)
		led_trigger_event(fctrl.torch_trigger, 0);
	for (i = 0; i < fctrl.num_sources; i++)
	if (fctrl.flash_trigger[i])
		led_trigger_event(fctrl.flash_trigger[i], fctrl.flash_op_current[i]);
	break;
	default:
	fctrl.rear_flash_status=MSM_CAMERA_LED_OFF;
	for (i = 0; i < fctrl.num_sources; i++)
	if (fctrl.flash_trigger[i])
		led_trigger_event(fctrl.flash_trigger[i], 0);
	if (fctrl.torch_trigger)
		led_trigger_event(fctrl.torch_trigger, 0);
	break;
	}
	return 0;
}
EXPORT_SYMBOL(s5k4ecgx_set_flash);
#endif
static int32_t msm_led_trigger_config(struct msm_led_flash_ctrl_t *fctrl,
	void *data)
{
	int rc = 0;
	struct msm_camera_led_cfg_t *cfg = (struct msm_camera_led_cfg_t *)data;
#if !defined(CONFIG_LEDS_MAX77804K)
	uint32_t i;
#endif
	CDBG("called led_state %d\n", cfg->cfgtype);

	if (!fctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
#if defined(CONFIG_MACH_AFYONLTE_TMO) || defined(CONFIG_MACH_AFYONLTE_CAN) \
	|| defined (CONFIG_MACH_AFYONLTE_MTR)
	if( flash_widget_status == 1 )
	{
		printk(" %s Dont handle flash..rear_flash is set\n",__func__);
		return 0;
	}
#else
	if(fctrl->rear_flash_status == MSM_CAMERA_LED_LOW || fctrl->rear_flash_status == MSM_CAMERA_LED_HIGH)
	{
		printk("Dont handle flash..rear_flash is set\n");
		return rc;
	}
#endif
	switch (cfg->cfgtype) {
#if defined(CONFIG_LEDS_MAX77804K)
	case MSM_CAMERA_LED_OFF:
		pr_err("CAM Flash OFF\n");
		max77804k_led_en(0, 0);
		max77804k_led_en(0, 1);
		break;

	case MSM_CAMERA_LED_LOW:
		pr_err("CAM Pre Flash ON\n");
		max77804k_led_en(1, 0);
		break;

	case MSM_CAMERA_LED_HIGH:
		pr_err("CAM Flash ON\n");
		max77804k_led_en(1, 1);
		break;

	case MSM_CAMERA_LED_INIT:
		break;
	case MSM_CAMERA_LED_RELEASE:
        {
            int ret = 0;
            pr_err("CAM Flash OFF & release\n");
            ret = gpio_request(led_flash_en, "max77804k_flash_en");
            if (ret)
                pr_err("can't get max77804k_flash_en\n");
            else {
                gpio_direction_output(led_flash_en, 0);
                gpio_free(led_flash_en);
            }
            ret = gpio_request(led_torch_en, "max77804k_torch_en");
            if (ret)
                pr_err("can't get max77804k_torch_en\n");
            else {
                gpio_direction_output(led_torch_en, 0);
                gpio_free(led_torch_en);
            }
        }
		break;

	default:
		rc = -EFAULT;
		break;
#else
	case MSM_CAMERA_LED_OFF:
		for (i = 0; i < fctrl->num_sources; i++)
			if (fctrl->flash_trigger[i])
				led_trigger_event(fctrl->flash_trigger[i], 0);
		if (fctrl->torch_trigger)
			led_trigger_event(fctrl->torch_trigger, 0);
		break;

	case MSM_CAMERA_LED_LOW:
		if (fctrl->torch_trigger)
			led_trigger_event(fctrl->torch_trigger,
				fctrl->torch_op_current);
		break;

	case MSM_CAMERA_LED_HIGH:
		if (fctrl->torch_trigger)
			led_trigger_event(fctrl->torch_trigger, 0);
		for (i = 0; i < fctrl->num_sources; i++)
			if (fctrl->flash_trigger[i])
				led_trigger_event(fctrl->flash_trigger[i],
					fctrl->flash_op_current[i]);
		break;

	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
		for (i = 0; i < fctrl->num_sources; i++)
			if (fctrl->flash_trigger[i])
				led_trigger_event(fctrl->flash_trigger[i], 0);
		if (fctrl->torch_trigger)
			led_trigger_event(fctrl->torch_trigger, 0);
		break;

	default:
		rc = -EFAULT;
		break;
#endif
	}
	CDBG("flash_set_led_state: return %d\n", rc);
	return rc;
}

static const struct of_device_id msm_led_trigger_dt_match[] = {
	{.compatible = "qcom,camera-led-flash"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_led_trigger_dt_match);

static struct platform_driver msm_led_trigger_driver = {
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_led_trigger_dt_match,
	},
};

static int32_t msm_led_trigger_probe(struct platform_device *pdev)
{
	int32_t rc = 0, i = 0;
	struct device_node *of_node = pdev->dev.of_node;
	struct device_node *flash_src_node = NULL;
	uint32_t count = 0;

	CDBG("called\n");

	if (!of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	fctrl.pdev = pdev;
	fctrl.num_sources = 0;

	rc = of_property_read_u32(of_node, "cell-index", &pdev->id);
	if (rc < 0) {
		pr_err("failed\n");
		return -EINVAL;
	}
	CDBG("pdev id %d\n", pdev->id);

	if (of_get_property(of_node, "qcom,flash-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			pr_err("failed\n");
			return -EINVAL;
		}
		fctrl.num_sources = count;
		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"qcom,flash-source", i);
			if (!flash_src_node) {
				pr_err("flash_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"linux,default-trigger",
				&fctrl.flash_trigger_name[i]);
			if (rc < 0) {
				pr_err("failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			CDBG("default trigger %s\n",
				fctrl.flash_trigger_name[i]);

			rc = of_property_read_u32(flash_src_node,
				"qcom,current", &fctrl.flash_op_current[i]);
			if (rc < 0) {
				pr_err("failed rc %d\n", rc);
				of_node_put(flash_src_node);
				continue;
			}

			of_node_put(flash_src_node);

			CDBG("max_current[%d] %d\n",
				i, fctrl.flash_op_current[i]);

			led_trigger_register_simple(fctrl.flash_trigger_name[i],
				&fctrl.flash_trigger[i]);
		}

		/* Torch source */
		flash_src_node = of_parse_phandle(of_node, "qcom,torch-source",
			0);
		if (flash_src_node) {
			rc = of_property_read_string(flash_src_node,
				"linux,default-trigger",
				&fctrl.torch_trigger_name);
			if (rc < 0) {
				pr_err("failed\n");
			} else {
				CDBG("default trigger %s\n",
					fctrl.torch_trigger_name);

				rc = of_property_read_u32(flash_src_node,
					"qcom,current",
					&fctrl.torch_op_current);
				if (rc < 0) {
					pr_err("failed rc %d\n", rc);
				} else {
					CDBG("torch max_current %d\n",
						fctrl.torch_op_current);

					led_trigger_register_simple(
						fctrl.torch_trigger_name,
						&fctrl.torch_trigger);
				}
			}
			of_node_put(flash_src_node);
		}
	}
	rc = msm_led_flash_create_v4lsubdev(pdev, &fctrl);

#if !defined(CONFIG_LEDS_MAX77804K)
	if (!IS_ERR(camera_class)) {
		flash_dev = device_create(camera_class, NULL, 0, NULL, "flash");
		if (IS_ERR(flash_dev))
			pr_err("Failed to create device(flash)!\n");

		if (device_create_file(flash_dev, &dev_attr_rear_flash) < 0) {
			pr_err("failed to create device file, %s\n",
			   dev_attr_rear_flash.attr.name);
		}

	} else
		pr_err("Failed to create device(flash) because of nothing camera class!\n");
#endif
	return rc;
}

static int __init msm_led_trigger_add_driver(void)
{
	CDBG("called\n");
	return platform_driver_probe(&msm_led_trigger_driver,
		msm_led_trigger_probe);
}

static struct msm_flash_fn_t msm_led_trigger_func_tbl = {
	.flash_get_subdev_id = msm_led_trigger_get_subdev_id,
	.flash_led_config = msm_led_trigger_config,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.func_tbl = &msm_led_trigger_func_tbl,
};


#if defined(CONFIG_LEDS_MAX77804K)
int set_led_flash(int mode)
{
    struct msm_camera_led_cfg_t cfg;
    int rc = 0;
    cfg.cfgtype = mode;

    if (led_prev_mode && mode)
        return -1;

    rc = msm_led_trigger_config(&fctrl, &mode);

    if (rc == 0)
        led_prev_mode = mode;

    return rc;
}
EXPORT_SYMBOL(set_led_flash);
#endif

module_init(msm_led_trigger_add_driver);
MODULE_DESCRIPTION("LED TRIGGER FLASH");
MODULE_LICENSE("GPL v2");
