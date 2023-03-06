/* /linux/drivers/misc/modem_if/modem_modemctl_device_xmm6262.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <plat/devs.h>
#include <linux/platform_data/modem.h>
#include "modem_prj.h"
#include "modem_utils.h"

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

/*
 * for debugging CP_DUMP_INT transition
 */
#define CONFIG_LOGGING_CP_DUMP_INT_IRQ

static int cp_watchdog_enable = 0;
module_param(cp_watchdog_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(cp_watchdog_enable, "CP watchdog reset enable");

static inline void set_modem_state(struct modem_ctl *mc,
				enum modem_state state)
{
	mif_info("set state: %d\n", state);

	if (mc->iod && mc->iod->modem_state_changed)
		mc->iod->modem_state_changed(mc->iod, state);
	if (mc->bootd && mc->bootd->modem_state_changed)
		mc->bootd->modem_state_changed(mc->bootd, state);
}
static int m74xx_on(struct modem_ctl *mc)
{
	mif_info("\n");

	if (gpio_get_value(mc->gpio_ap_dump_int))
		gpio_set_value(mc->gpio_ap_dump_int, 0);

	gpio_set_value(mc->gpio_cp_on, 1);
	msleep(100);

	mc->phone_state = STATE_BOOTING;

	if (mc->gpio_pda_active)
		gpio_set_value(mc->gpio_pda_active, 1);

	return 0;
}

static int m74xx_off(struct modem_ctl *mc)
{
	mif_info("\n");

	if (mc->gpio_pda_active)
		gpio_set_value(mc->gpio_pda_active, 0);

	gpio_set_value(mc->gpio_cp_on, 0);
	mc->phone_state = STATE_OFFLINE;

	return 0;
}

static int m74xx_force_crash_exit(struct modem_ctl *mc)
{
	if (!gpio_get_value(mc->gpio_phone_active))
		return 0;

	if (!gpio_get_value(mc->gpio_ap_dump_int)) {
		gpio_set_value(mc->gpio_ap_dump_int, 1);
		mif_info("ap_dump_int: %d\n",
			gpio_get_value(mc->gpio_ap_dump_int));
	}

	return 0;
}

static irqreturn_t phone_active_irq_handler(int irq, void *_mc)
{
	int phone_active;
	int cp_dump_int;

	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	disable_irq_nosync(mc->irq_phone_active);

	mdelay(5);

	phone_active = gpio_get_value(mc->gpio_phone_active);
	cp_dump_int = gpio_get_value(mc->gpio_cp_dump_int);

	mif_info("PA EVENT: pa = %d cp_dump_int: %d\n",
		phone_active, cp_dump_int);

	if (!phone_active && mc->phone_state != STATE_OFFLINE)
		set_modem_state(mc, cp_dump_int ? STATE_CRASH_EXIT :
                       cp_watchdog_enable ? STATE_CRASH_WATCHDOG :
                               STATE_CRASH_RESET);

	irq_set_irq_type(mc->irq_phone_active,
		phone_active ? IRQ_TYPE_EDGE_FALLING : IRQ_TYPE_EDGE_RISING);

	enable_irq(mc->irq_phone_active);

	return IRQ_HANDLED;
}

#ifdef CONFIG_LOGGING_CP_DUMP_INT_IRQ
static irqreturn_t phone_cp_dump_irq_handler(int irq, void *_mc)
{
	int phone_active;
	int cp_dump_int;

	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	disable_irq_nosync(mc->irq_cp_dump_int);

	phone_active = gpio_get_value(mc->gpio_phone_active);
	cp_dump_int = gpio_get_value(mc->gpio_cp_dump_int);

	mif_info("CD EVENT: pa = %d cp_dump_int: %d\n",
		phone_active, cp_dump_int);

	irq_set_irq_type(mc->irq_cp_dump_int,
		cp_dump_int ? IRQ_TYPE_EDGE_FALLING : IRQ_TYPE_EDGE_RISING);

	enable_irq(mc->irq_cp_dump_int);

	return IRQ_HANDLED;
}
#endif

static void m74xx_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = m74xx_on;
	mc->ops.modem_off = m74xx_off;
	mc->ops.modem_force_crash_exit = m74xx_force_crash_exit;
}

#ifdef CONFIG_OF
static int dt_gpio_config(struct modem_ctl *mc,
		struct modem_data *pdata)
{
	int ret;
	struct device_node *np = mc->dev->of_node;

	/* GPIO_CP_ON */
	pdata->gpio_cp_on = of_get_named_gpio(np, "mif,gpio_cp_on", 0);
	if (!gpio_is_valid(pdata->gpio_cp_on)) {
		mif_err("cp_on: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_cp_on, "CP_ON");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "CP_ON", ret);
	gpio_direction_output(pdata->gpio_cp_on, 0);
	board_gpio_export(mc->dev, pdata->gpio_cp_on, false, "cp_on");

	/* GPIO_CP_RESET */
	pdata->gpio_cp_reset = of_get_named_gpio(np, "mif,gpio_cp_reset", 0);
	if (!gpio_is_valid(pdata->gpio_cp_reset)) {
		mif_err("cp_rst: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_cp_reset, "CP_RST");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "CP_RST", ret);
	gpio_direction_output(pdata->gpio_cp_reset, 1);

	/* GPIO_AP_DUMP_INT */
	pdata->gpio_ap_dump_int =
		of_get_named_gpio(np, "mif,gpio_ap_dump_int", 0);
	if (!gpio_is_valid(pdata->gpio_ap_dump_int)) {
		mif_err("ap_dump_int: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_ap_dump_int, "AP_DUMP_INT");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "AP_DUMP_INT", ret);
	gpio_direction_output(pdata->gpio_ap_dump_int, 0);
	board_gpio_export(mc->dev, pdata->gpio_ap_dump_int, false, "ap_dump_int");

	/* GPIO_CP_DUMP_INT */
	pdata->gpio_cp_dump_int =
		of_get_named_gpio(np, "mif,gpio_cp_dump_int", 0);
	if (!gpio_is_valid(pdata->gpio_cp_dump_int)) {
		mif_err("cp_dump_int: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_cp_dump_int, "CP_DUMP_INT");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "CP_DUMP_INT", ret);
	gpio_direction_input(pdata->gpio_cp_dump_int);
	board_gpio_export(mc->dev, pdata->gpio_cp_dump_int, false, "cp_resout");

	/* GPIO_PHONE_ACTIVE */
	pdata->gpio_phone_active =
		of_get_named_gpio(np, "mif,gpio_phone_active", 0);
	if (!gpio_is_valid(pdata->gpio_phone_active)) {
		mif_err("phone_active: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_phone_active, "PHONE_ACTIVE");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "PHONE_ACTIVE", ret);
	gpio_direction_input(pdata->gpio_phone_active);

	/* GPIO_PDA_ACTIVE */
	pdata->gpio_pda_active =
		of_get_named_gpio(np, "mif,gpio_pda_active", 0);
	if (!gpio_is_valid(pdata->gpio_pda_active)) {
		mif_err("pda_active: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_pda_active, "PDA_ACTIVE");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "PDA_ACTIVE", ret);
	gpio_direction_output(pdata->gpio_pda_active, 0);

	return 0;
}
#endif

static int cp_crash_notify_call(struct notifier_block *nfb,
		unsigned long event, void *arg)
{
	struct modem_ctl *mc = container_of(nfb, struct modem_ctl,
					cp_crash_nfb);

	mif_info("event: %ld\n", event);

	switch (event) {
	case CP_FORCE_CRASH_EVENT:
		m74xx_force_crash_exit(mc);
		break;
	}

	return 0;
}

static int verify_gpios(struct modem_ctl *mc)
{
	if (!mc->gpio_cp_on || !mc->gpio_cp_reset ||
		!mc->gpio_cp_dump_int || !mc->gpio_ap_dump_int ||
		!mc->gpio_phone_active) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	return 0;
}

int m74xx_init_modemctl_device(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret = 0;

#ifdef CONFIG_OF
	ret = dt_gpio_config(mc, pdata);
	if (ret < 0)
		return ret;
#endif
	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_ap_dump_int = pdata->gpio_ap_dump_int;
	mc->gpio_cp_dump_int = pdata->gpio_cp_dump_int;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_pda_active = pdata->gpio_pda_active;

	ret = verify_gpios(mc);
	if (ret < 0)
		return ret;

	mc->cp_crash_nfb.notifier_call = cp_crash_notify_call;
	register_cp_crash_notifier(&mc->cp_crash_nfb);

	m74xx_get_ops(mc);

	mc->irq_phone_active = gpio_to_irq(mc->gpio_phone_active);
	ret = request_irq(mc->irq_phone_active, phone_active_irq_handler,
			IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING,
			"phone_active", mc);
	if (ret) {
		mif_err("failed to request_irq:%d\n", ret);
		goto err_phone_active_request_irq;
	}

	ret = enable_irq_wake(mc->irq_phone_active);
	if (ret) {
		mif_err("failed to enable_irq_wake:%d\n", ret);
		goto err_phone_active_set_wake_irq;
	}

#ifdef CONFIG_LOGGING_CP_DUMP_INT_IRQ
	mc->irq_cp_dump_int = gpio_to_irq(mc->gpio_cp_dump_int);
	ret = request_irq(mc->irq_cp_dump_int, phone_cp_dump_irq_handler,
			IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING,
			"cp_dump_int", mc);
	if (ret) {
		mif_err("failed to enable irq_cp_dump_int:%d\n", ret);
		goto err_phone_active_set_wake_irq;
	}
#endif

	mif_info("success\n");

	return ret;

err_phone_active_set_wake_irq:
	free_irq(mc->irq_phone_active, mc);
err_phone_active_request_irq:
	return ret;
}
