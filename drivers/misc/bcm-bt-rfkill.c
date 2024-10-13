/******************************************************************************
* Copyright (C) 2013 Broadcom Corporation
*
* @file   /kernel/drivers/misc/bcm-bt-rfkill.c
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
******************************************************************************/

/*
 * Broadcom Bluetooth rfkill power control via GPIO. This file is
 *board dependent!
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/broadcom/bcm-bt-rfkill.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include <linux/clk.h>
#include <mach/board.h>
#include <linux/of_gpio.h>

static void bt_clk_init(void)
{
	struct clk *bt_clk;
	struct clk *clk_parent;
	
	bt_clk = clk_get(NULL, "clk_aux0");
	if (IS_ERR(bt_clk)) {
		printk("clock: failed to get clk_aux0\n");
	}

	clk_parent = clk_get(NULL, "ext_32k");
	if (IS_ERR(clk_parent)) {
		printk("failed to get parent ext_32k\n");
	}

	clk_set_parent(bt_clk, clk_parent);
	
	clk_set_rate(bt_clk, 32000);
	clk_prepare_enable(bt_clk);
}

static int bcm_bt_rfkill_set_power(void *data, bool blocked)
{
	struct bcm_bt_rfkill_platform_data *pdata =
			(struct bcm_bt_rfkill_platform_data *) data;
	int vreg_gpio = pdata->bcm_bt_rfkill_vreg_gpio;
#if defined(CONFIG_BT_BCM4330)
	int n_reset_gpio = pdata->bcm_bt_rfkill_n_reset_gpio;
#endif
	//pr_info("bcm_bt_rfkill_setpower: now vreg: %s WLAN_REG:%s\n",
	//		gpio_get_value(vreg_gpio) ? "High [POWER ON]" :
	//		"Low [POWER_OFF]",gpio_get_value(186) ? "High [POWER ON]" :
	//		"Low [POWER_OFF]");


	if (blocked == false) {	/* Transmitter ON (Unblocked) */
#if defined(CONFIG_BT_BCM4330)
		if (n_reset_gpio > 0)
			gpio_set_value(n_reset_gpio,
					BCM_BT_RFKILL_N_RESET_ON);
#endif
		if (vreg_gpio > 0)
			gpio_set_value(vreg_gpio, BCM_BT_RFKILL_VREG_ON);
#if defined(CONFIG_BT_BCM4330)
		if (n_reset_gpio > 0)
			gpio_set_value(n_reset_gpio,
					BCM_BT_RFKILL_N_RESET_OFF);
#endif
#if defined(CONFIG_BT_BCM4330)
		pr_info("bcm_bt_rfkill_setpower: unblocked reset:%s\n",
			gpio_get_value(n_reset_gpio) ? "High [POWER ON]" :"Low [POWER_OFF]");
#endif
		pr_info("bcm_bt_rfkill_setpower: unblocked vreg:%s\n",
			gpio_get_value(vreg_gpio) ? "High [POWER ON]" :"Low [POWER_OFF]");

	bt_clk_init();

	} else {		/* Transmitter OFF (Blocked) */
#if defined(CONFIG_BT_BCM4330)
		if (n_reset_gpio > 0)
			gpio_set_value(n_reset_gpio,
					BCM_BT_RFKILL_N_RESET_ON);
#endif
		if (vreg_gpio > 0)
			gpio_set_value(vreg_gpio, BCM_BT_RFKILL_VREG_OFF);

#if defined(CONFIG_BT_BCM4330)
		pr_info("bcm_bt_rfkill_setpower: blocked reset:%s\n",
			gpio_get_value(n_reset_gpio) ? "High [POWER ON]" :"Low [POWER_OFF]");
#endif
		pr_info("bcm_bt_rfkill_setpower: blocked vreg:%s\n",
			gpio_get_value(vreg_gpio) ? "High [POWER ON]" :"Low [POWER_OFF]");
	}
	return 0;
}

static const struct rfkill_ops bcm_bt_rfkill_ops = {
	.set_block = bcm_bt_rfkill_set_power,
};

static const struct of_device_id bcm_bt_rfkill_of_match[] = {
	{ .compatible = "bcm,bcm-bt-rfkill", },
	{},
}
MODULE_DEVICE_TABLE(of, bcm_bt_rfkill_of_match);

static int bcm_bt_rfkill_probe(struct platform_device *pdev)
{
	int rc = -EINVAL;
	struct bcm_bt_rfkill_platform_data *pdata = NULL;
	struct bcm_bt_rfkill_platform_data *pdata1 = NULL;
       printk("bcm_bt_rfkill_probe in\n");
	const struct of_device_id *match = NULL;
	match = of_match_device(bcm_bt_rfkill_of_match, &pdev->dev);
	if (!match) {
		pr_err("%s: **INFO** No matcing device found\n", __func__);
	}

	pdata = devm_kzalloc(&pdev->dev,
		sizeof(struct bcm_bt_rfkill_platform_data), GFP_KERNEL);

	if (pdata == NULL)
		return -ENOMEM;

	if (!match) {
		if (pdev->dev.platform_data) {
			pdata1 = pdev->dev.platform_data;
			pdata->bcm_bt_rfkill_vreg_gpio =
				pdata1->bcm_bt_rfkill_vreg_gpio;
#if defined(CONFIG_BT_BCM4330)
			pdata->bcm_bt_rfkill_n_reset_gpio =
				pdata1->bcm_bt_rfkill_n_reset_gpio;
#endif
			printk("%s: %d \n", __func__, pdata->bcm_bt_rfkill_vreg_gpio);
#if defined(CONFIG_BT_BCM4330)
			printk("%s: %d \n", __func__, pdata->bcm_bt_rfkill_n_reset_gpio);
#endif
		} else {
			pr_err("%s: **ERROR** NO platform data available\n", __func__);
			rc = -ENODEV;
			goto out;
		}
	} else if (pdev->dev.of_node) {
		pdata->bcm_bt_rfkill_vreg_gpio = of_get_gpio( pdev->dev.of_node, 0);
		if (!gpio_is_valid(pdata->bcm_bt_rfkill_vreg_gpio)) {
			pr_err("%s: ERROR Invalid bcm-bt-rfkill-vreg-gpio.\n",
				__func__);
			rc = pdata->bcm_bt_rfkill_vreg_gpio;
			goto out;
		}
		//pdata->bcm_bt_rfkill_vreg_gpio =231; //moonys
#if defined(CONFIG_BT_BCM4330)
		pdata->bcm_bt_rfkill_n_reset_gpio =
					of_get_named_gpio(pdev->dev.of_node,
					"bcm-bt-rfkill-n-reset-gpio", 0);
		if (!gpio_is_valid(pdata->bcm_bt_rfkill_n_reset_gpio)) {
			pr_err("%s:ERR Invalid bcm-bt-rfkill-n-reset-gpio\n",
				__func__);
			pdata->bcm_bt_rfkill_n_reset_gpio = -1;
		}
#endif
		pdev->dev.platform_data = pdata;
	} else {
		pr_err("%s: **ERROR** No platformdata/devicetree\n", __func__);
		rc = -ENODEV;
		goto out;
	}

	rc = devm_gpio_request_one(&pdev->dev,
				pdata->bcm_bt_rfkill_vreg_gpio,
				GPIOF_OUT_INIT_LOW,
					"bcm_bt_rfkill_vreg_gpio");
	if (rc) {
		pr_err("%s: ERROR bcm_bt_rfkill_vreg_gpio request failed\n",
								__func__);
		goto out;
	}
	pr_info("bcm_bt_rfkill_probe: Set bcm_bt_rfkill_vreg_gpio:%d,level:%s\n",
	       pdata->bcm_bt_rfkill_vreg_gpio,
	       gpio_get_value(pdata->bcm_bt_rfkill_vreg_gpio) ? "High" : "Low");
	gpio_export(pdata->bcm_bt_rfkill_vreg_gpio, false);
	gpio_direction_output(pdata->bcm_bt_rfkill_vreg_gpio,
						BCM_BT_RFKILL_VREG_OFF);

#if defined(CONFIG_BT_BCM4330)
	/* JIRA case --> HW4334-336*/
	if (pdata->bcm_bt_rfkill_n_reset_gpio > 0) {
		rc = devm_gpio_request_one(&pdev->dev,
					pdata->bcm_bt_rfkill_n_reset_gpio,
					GPIOF_OUT_INIT_LOW,
						"bcm_bt_rfkill_n_reset_gpio");
		if (rc) {
			pr_err(
			"%s:ERROR bcm_bt_rfkill_n_reset_gpio request failed\n",
					__func__);
		}
		gpio_export(pdata->bcm_bt_rfkill_n_reset_gpio, false);
		gpio_direction_output(pdata->bcm_bt_rfkill_n_reset_gpio,
						BCM_BT_RFKILL_N_RESET_ON);
		printk("bcm_bt_rfkill_probe: bcm_bt_rfkill_n_reset_gpio: %s\n",
		       gpio_get_value(pdata->bcm_bt_rfkill_n_reset_gpio) ?
			"High [chip out of reset]"
		       : "Low [put into reset]");
	}
#endif
	pdata->rfkill =
	    rfkill_alloc("bcm-bt-rfkill", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			 &bcm_bt_rfkill_ops, pdata);

	if (unlikely(!pdata->rfkill)) {
		rc = -ENOMEM;
		goto out;
	}

	/* Keep BT Blocked by default as per above init */
	rfkill_init_sw_state(pdata->rfkill, false);

	rc = rfkill_register(pdata->rfkill);

	if (unlikely(rc)) {
		rfkill_destroy(pdata->rfkill);
		goto out;
	}

	return 0;
out:
	if (pdev->dev.of_node)
		pdev->dev.platform_data = NULL;
	return rc;
}

static int bcm_bt_rfkill_remove(struct platform_device *pdev)
{
	struct bcm_bt_rfkill_platform_data *pdata =
					pdev->dev.platform_data;

	if (pdata != NULL) {
		rfkill_unregister(pdata->rfkill);
		rfkill_destroy(pdata->rfkill);

		if (pdev->dev.of_node)
			pdev->dev.platform_data = NULL;
	}
	return 0;
}

static struct platform_driver bcm_bt_rfkill_platform_driver = {
	.probe = bcm_bt_rfkill_probe,
	.remove = bcm_bt_rfkill_remove,
	.driver = {
		   .name = "bcm-bt-rfkill",
		   .owner = THIS_MODULE,
		   .of_match_table = bcm_bt_rfkill_of_match,
		   },
};

static int __init bcm_bt_rfkill_init(void)
{
       printk("bcm_bt_rfkill_init\n");
	return platform_driver_register(&bcm_bt_rfkill_platform_driver);
}

static void __exit bcm_bt_rfkill_exit(void)
{
	platform_driver_unregister(&bcm_bt_rfkill_platform_driver);
}

late_initcall(bcm_bt_rfkill_init);
module_exit(bcm_bt_rfkill_exit);

MODULE_DESCRIPTION("bcm-bt-rfkill");
MODULE_AUTHOR("Broadcom");
MODULE_LICENSE("GPL");

