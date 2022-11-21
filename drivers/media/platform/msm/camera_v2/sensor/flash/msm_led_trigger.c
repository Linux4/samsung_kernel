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
#if defined(CONFIG_LEDS_MAX77828)
#include <linux/leds-max77828.h>
#endif
#if defined(CONFIG_LEDS_MIC2873)
#include <linux/leds-mic2873.h>
#endif
#include "msm_led_flash.h"

#define FLASH_NAME "camera-led-flash"

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_led_flash_ctrl_t fctrl;

struct device *flash_dev;

#if defined(CONFIG_LEDS_MAX77804K)
extern int led_flash_en;
extern int led_torch_en;
#define MAX77804K_DRIVER "max77804k"
#endif

#if defined(CONFIG_LEDS_MAX77828)
#define MAX77828_DRIVER "max77828"
#endif

#if defined(CONFIG_LEDS_MIC2873)
#define MIC2873_DRIVER "mic2873"
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

static int32_t msm_led_trigger_config(struct msm_led_flash_ctrl_t *fctrl,
	void *data)
{
	int rc = 0;

	struct msm_camera_led_cfg_t *cfg = (struct msm_camera_led_cfg_t *)data;
	CDBG("called led_state %d\n", cfg->cfgtype);

	if (!fctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}

#if defined(CONFIG_LEDS_MAX77804K) && defined(CONFIG_LEDS_MAX77828)
    if (!strcmp(fctrl->flash_driver, MAX77804K_DRIVER)) {
        CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        switch (cfg->cfgtype) {
        case MSM_CAMERA_LED_OFF:
            pr_info("%s : CAM Flash OFF\n", fctrl->flash_driver);
            max77804k_led_en(0, 0);
            max77804k_led_en(0, 1);
            break;

        case MSM_CAMERA_LED_LOW:
            pr_info("%s : CAM Pre Flash ON\n", fctrl->flash_driver);
            max77804k_led_en(1, 0);
            break;

        case MSM_CAMERA_LED_HIGH:
            pr_info("%s : CAM Flash ON\n", fctrl->flash_driver);
            max77804k_led_en(1, 1);
            break;

        case MSM_CAMERA_LED_INIT:
            break;

        case MSM_CAMERA_LED_RELEASE:
            pr_info("%s : CAM Flash OFF & release\n", fctrl->flash_driver);
            max77804k_led_en(0, 0);
            max77804k_led_en(0, 1);
            break;

        default:
            rc = -EFAULT;
            break;
        }
    } else if (!strcmp(fctrl->flash_driver, MAX77828_DRIVER)){ //max77828
        CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        switch (cfg->cfgtype) {
        case MSM_CAMERA_LED_OFF:
            pr_info("%s : CAM Flash OFF\n", fctrl->flash_driver);
            max77828_led_en(0, 0);
            max77828_led_en(0, 1);
            break;

        case MSM_CAMERA_LED_LOW:
            pr_info("%s : CAM Pre Flash ON\n", fctrl->flash_driver);
            max77828_led_en(1, 0);
            break;

        case MSM_CAMERA_LED_HIGH:
            pr_info("%s : CAM Flash ON\n", fctrl->flash_driver);
            max77828_led_en(1, 1);
            break;

        case MSM_CAMERA_LED_INIT:
            break;

        case MSM_CAMERA_LED_RELEASE:
            pr_info("%s : CAM Flash OFF & release\n", fctrl->flash_driver);
            max77828_led_en(0, 0);
            max77828_led_en(0, 1);
            break;

        default:
            rc = -EFAULT;
            break;
        }
    }else {
        pr_err("%s : can't find valid flash driver\n", __FUNCTION__);
        pr_err("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        rc = -EINVAL;
    }
#elif defined(CONFIG_LEDS_MIC2873) && defined(CONFIG_LEDS_MAX77828)
    if (!strcmp(fctrl->flash_driver, MIC2873_DRIVER)) {//mic2873
        CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        switch (cfg->cfgtype) {
        case MSM_CAMERA_LED_OFF:
            pr_info("%s : CAM Flash OFF\n", fctrl->flash_driver);
            mic2873_led_en(0, 0);
            mic2873_led_en(0, 1);
            break;

        case MSM_CAMERA_LED_LOW:
            pr_info("%s : CAM Pre Flash ON\n", fctrl->flash_driver);
            mic2873_led_en(1, 0);
            break;

        case MSM_CAMERA_LED_HIGH:
            pr_info("%s : CAM Flash ON\n", fctrl->flash_driver);
            mic2873_led_en(1, 1);
            break;

        case MSM_CAMERA_LED_INIT:
            break;

        case MSM_CAMERA_LED_RELEASE:
            pr_info("%s : CAM Flash OFF & release\n", fctrl->flash_driver);
            mic2873_led_en(0, 0);
            mic2873_led_en(0, 1);
            break;

        default:
            rc = -EFAULT;
            break;
        }
    } else if (!strcmp(fctrl->flash_driver, MAX77828_DRIVER)){ //max77828
        CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        switch (cfg->cfgtype) {
        case MSM_CAMERA_LED_OFF:
            pr_info("%s : CAM Flash OFF\n", fctrl->flash_driver);
            max77828_led_en(0, 0);
            max77828_led_en(0, 1);
            break;

        case MSM_CAMERA_LED_LOW:
            pr_info("%s : CAM Pre Flash ON\n", fctrl->flash_driver);
            max77828_led_en(1, 0);
            break;

        case MSM_CAMERA_LED_HIGH:
            pr_info("%s : CAM Flash ON\n", fctrl->flash_driver);
            max77828_led_en(1, 1);
            break;

        case MSM_CAMERA_LED_INIT:
            break;

        case MSM_CAMERA_LED_RELEASE:
            pr_info("%s : CAM Flash OFF & release\n", fctrl->flash_driver);
            max77828_led_en(0, 0);
            max77828_led_en(0, 1);
            break;

        default:
            rc = -EFAULT;
            break;
        }
    }else {
        pr_err("%s : can't find valid flash driver\n", __FUNCTION__);
        pr_err("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        rc = -EINVAL;
    }
#elif defined(CONFIG_LEDS_MAX77804K)
        CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        switch (cfg->cfgtype) {
        case MSM_CAMERA_LED_OFF:
            pr_info("CAM Flash OFF\n");
            max77804k_led_en(0, 0);
            max77804k_led_en(0, 1);
            break;

        case MSM_CAMERA_LED_LOW:
            pr_info("CAM Pre Flash ON\n");
            max77804k_led_en(1, 0);
            break;

        case MSM_CAMERA_LED_HIGH:
            pr_info("CAM Flash ON\n");
            max77804k_led_en(1, 1);
            break;

        case MSM_CAMERA_LED_INIT:
            break;

        case MSM_CAMERA_LED_RELEASE:
            pr_info("%s : CAM Flash OFF & release\n", fctrl->flash_driver);
            max77804k_led_en(0, 0);
            max77804k_led_en(0, 1);
            break;

        default:
            rc = -EFAULT;
            break;
        }
#elif defined(CONFIG_LEDS_MAX77828)
        CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        switch (cfg->cfgtype) {
        case MSM_CAMERA_LED_OFF:
            pr_info("CAM Flash OFF\n");
            max77828_led_en(0, 0);
            max77828_led_en(0, 1);
            break;

        case MSM_CAMERA_LED_LOW:
            pr_info("CAM Pre Flash ON\n");
            max77828_led_en(1, 0);
            break;

        case MSM_CAMERA_LED_HIGH:
            pr_info("CAM Flash ON\n");
            max77828_led_en(1, 1);
            break;

        case MSM_CAMERA_LED_INIT:
            break;

        case MSM_CAMERA_LED_RELEASE:
            pr_info("%s : CAM Flash OFF & release\n", fctrl->flash_driver);
            max77828_led_en(0, 0);
            max77828_led_en(0, 1);
            break;

        default:
            rc = -EFAULT;
            break;
        }
#elif defined(CONFIG_LEDS_MIC2873)
        CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
        switch (cfg->cfgtype) {
        case MSM_CAMERA_LED_OFF:
            pr_info("CAM Flash OFF\n");
            mic2873_led_en(0, 0);
            mic2873_led_en(0, 1);
            break;

        case MSM_CAMERA_LED_LOW:
            pr_info("CAM Pre Flash ON\n");
            mic2873_led_en(1, 0);
            break;

        case MSM_CAMERA_LED_HIGH:
            pr_info("CAM Flash ON\n");
            mic2873_led_en(1, 1);
            break;

        case MSM_CAMERA_LED_INIT:
            break;

        case MSM_CAMERA_LED_RELEASE:
            pr_info("%s : CAM Flash OFF & release\n", fctrl->flash_driver);
            mic2873_led_en(0, 0);
            mic2873_led_en(0, 1);
            break;

        default:
            rc = -EFAULT;
            break;
        }
#else
	CDBG("%s : driver is %s\n", __FUNCTION__, fctrl->flash_driver);
	switch (cfg->cfgtype) {
	case MSM_CAMERA_LED_OFF:
		led_trigger_event(fctrl->led_trigger[0], 0);
		break;

	case MSM_CAMERA_LED_LOW:
		led_trigger_event(fctrl->led_trigger[0],
			fctrl->max_current[0] / 2);
		break;

	case MSM_CAMERA_LED_HIGH:
		led_trigger_event(fctrl->led_trigger[0], fctrl->max_current[0]);
		break;

	case MSM_CAMERA_LED_INIT:
	case MSM_CAMERA_LED_RELEASE:
		led_trigger_event(fctrl->led_trigger[0], 0);
		break;

	default:
		rc = -EFAULT;
		break;
	}
#endif
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

	rc = of_property_read_u32(of_node, "cell-index", &pdev->id);
	if (rc < 0) {
		pr_err("failed\n");
		return -EINVAL;
	}
	CDBG("pdev id %d\n", pdev->id);

	rc = of_property_read_string(of_node, "flash-driver",
		&fctrl.flash_driver);
	pr_info("[%s] Flash driver is %s\n", __FUNCTION__,
		fctrl.flash_driver);
	if (rc < 0) {
		pr_err("%s failed %d\n", __FUNCTION__, __LINE__);
		return -EINVAL;
	}

	if (of_get_property(of_node, "qcom,flash-source", &count)) {
		count /= sizeof(uint32_t);
		CDBG("count %d\n", count);
		if (count > MAX_LED_TRIGGERS) {
			pr_err("failed\n");
			return -EINVAL;
		}
		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"qcom,flash-source", i);
			if (!flash_src_node) {
				pr_err("flash_src_node NULL\n");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"linux,default-trigger",
				&fctrl.led_trigger_name[i]);
			if (rc < 0) {
				pr_err("failed\n");
				of_node_put(flash_src_node);
				continue;
			}

			CDBG("default trigger %s\n", fctrl.led_trigger_name[i]);

			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current", &fctrl.max_current[i]);
			if (rc < 0) {
				pr_err("failed rc %d\n", rc);
				of_node_put(flash_src_node);
				continue;
			}

			of_node_put(flash_src_node);

			CDBG("max_current[%d] %d\n", i, fctrl.max_current[i]);

			led_trigger_register_simple(fctrl.led_trigger_name[i],
				&fctrl.led_trigger[i]);
		}
	}
	rc = msm_led_flash_create_v4lsubdev(pdev, &fctrl);

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

module_init(msm_led_trigger_add_driver);
MODULE_DESCRIPTION("LED TRIGGER FLASH");
MODULE_LICENSE("GPL v2");
