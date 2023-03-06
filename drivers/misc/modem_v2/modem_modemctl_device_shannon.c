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

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <plat/devs.h>
#include <linux/platform_data/modem.h>
#include "modem_prj.h"

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

static inline void set_modem_state(struct modem_ctl *mc, enum modem_state state)
{
	mif_info("set state: %d\n", state);

	if (mc->iod && mc->iod->modem_state_changed)
		mc->iod->modem_state_changed(mc->iod,
				state);
	if (mc->bootd && mc->bootd->modem_state_changed)
		mc->bootd->modem_state_changed(mc->bootd,
				state);
}

static int shannon_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);

	mif_info("\n");

	ld->mode = LINK_MODE_OFFLINE;

	gpio_direction_input(mc->gpio_cp_off);

	gpio_set_value(mc->gpio_cp_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(10);

	gpio_set_value(mc->gpio_cp_on, 1);
	msleep(10);
	gpio_set_value(mc->gpio_cp_reset, 1);
	msleep(10);

	mif_info("cp_on: %d cp_reset: %d\n",
		gpio_get_value(mc->gpio_cp_on),
		gpio_get_value(mc->gpio_cp_reset));

	return 0;
}

static int shannon_off(struct modem_ctl *mc)
{
	mif_info("\n");

	gpio_set_value(mc->gpio_cp_reset, 0);
	gpio_direction_output(mc->gpio_cp_off, 1);
	gpio_set_value(mc->gpio_cp_on, 0);

	mc->phone_state = STATE_OFFLINE;

	return 0;
}

static int shannon_reset(struct modem_ctl *mc)
{
	shannon_off(mc);
	msleep(100);
	shannon_on(mc);

	return 0;
}

static int shannon_boot_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);

	mif_info("\n");

	disable_irq_nosync(mc->irq_phone_active);

	gpio_set_value(mc->gpio_ap_dump_int, 0);

	ld->mode = LINK_MODE_BOOT;
	set_modem_state(mc, STATE_BOOTING);

	return 0;
}

static int shannon_boot_done(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);

	mif_info("\n");

	ld->mode = LINK_MODE_IPC;
	mc->phone_state = STATE_ONLINE;

	set_modem_state(mc, STATE_ONLINE);

	enable_irq(mc->irq_phone_active);

	return 0;
}

static int shannon_force_crash_exit(struct modem_ctl *mc)
{
	mif_info("\n");

	if (mc->phone_state == STATE_OFFLINE)
		return -ENODEV;

	if (!gpio_get_value(mc->gpio_ap_dump_int)) {
		gpio_set_value(mc->gpio_ap_dump_int, 1);
		mif_info("gpio_ap_dump_int set high\n");
		queue_delayed_work(mc->cp_crash_dump_wq, &mc->cp_crash_dump_dwork,
				msecs_to_jiffies(5000));
	}

	return 0;
}

static int shannon_dump_reset(struct modem_ctl *mc)
{
	mif_info("\n");

	gpio_set_value(mc->gpio_cp_reset, 0);
	usleep_range(200, 250);
	gpio_set_value(mc->gpio_cp_reset, 1);
	msleep(300);

	gpio_set_value(mc->gpio_ap_dump_int, 1);

	return 0;
}

static irqreturn_t phone_active_irq_handler(int irq, void *data)
{
	struct modem_ctl *mc = data;

	mif_info("\n");

	if (mc->phone_state == STATE_OFFLINE)
		return IRQ_HANDLED;

	disable_irq_nosync(mc->irq_phone_active);

	if (gpio_get_value(mc->gpio_cp_dump_int)) {
		if (!gpio_get_value(mc->gpio_ap_dump_int))
			gpio_set_value(mc->gpio_ap_dump_int, 1);

		if (mc->phone_state != STATE_CRASH_EXIT)
			set_modem_state(mc, STATE_CRASH_EXIT);
	}

	enable_irq(mc->irq_phone_active);

	return IRQ_HANDLED;
}

static void shannon_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = shannon_on;
	mc->ops.modem_off = shannon_off;
	mc->ops.modem_reset = shannon_reset;
	mc->ops.modem_boot_on = shannon_boot_on;
	mc->ops.modem_boot_done = shannon_boot_done;
	mc->ops.modem_force_crash_exit = shannon_force_crash_exit;
	mc->ops.modem_dump_reset = shannon_dump_reset;
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

	/* GPIO_CP_OFF */
	pdata->gpio_cp_off = of_get_named_gpio(np, "mif,gpio_cp_off", 0);
	if (!gpio_is_valid(pdata->gpio_cp_off)) {
		mif_err("cp_off: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_cp_off, "CP_OFF");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "CP_OFF", ret);
	gpio_direction_output(pdata->gpio_cp_off, 0);

	/* GPIO_CP_RESET */
	pdata->gpio_cp_reset = of_get_named_gpio(np, "mif,gpio_cp_reset", 0);
	if (!gpio_is_valid(pdata->gpio_cp_reset)) {
		mif_err("cp_rst: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_cp_reset, "CP_RST");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "CP_RST", ret);
	gpio_direction_output(pdata->gpio_cp_reset, 0);

	/* GPIO_PDA_ACTIVE */
	pdata->gpio_pda_active =
		of_get_named_gpio(np, "mif,gpio_pda_active", 0);
	if (!gpio_is_valid(pdata->gpio_pda_active)) {
		mif_err("gpio_pda_active: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_pda_active, "PDA_ACTIVE");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "PDA_ACTIVE", ret);
	gpio_direction_output(pdata->gpio_pda_active, 1);

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

	return 0;
}
#endif

static void cp_crash_dump_work_func(struct work_struct *work)
{
	struct modem_ctl *mc = container_of(work, struct modem_ctl,
				cp_crash_dump_dwork.work);

	if (mc->phone_state != STATE_CRASH_EXIT)
		set_modem_state(mc, STATE_CRASH_EXIT);
}

static int cp_crash_notify_call(struct notifier_block *nfb,
		unsigned long event, void *arg)
{
	struct modem_ctl *mc = container_of(nfb, struct modem_ctl,
					cp_crash_nfb);

	mif_info("event: %ld\n", event);

	switch (event) {
	case CP_FORCE_CRASH_EVENT:
		shannon_force_crash_exit(mc);
		break;
	}

	return 0;
}

static int verify_gpios(struct modem_ctl *mc)
{
	if (!mc->gpio_cp_on || !mc->gpio_cp_off ||
		!mc->gpio_cp_reset || !mc->gpio_cp_dump_int ||
		!mc->gpio_ap_dump_int || !mc->gpio_pda_active ) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	return 0;
}

int shannon_init_modemctl_device(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret = 0;

#ifdef CONFIG_OF
	ret = dt_gpio_config(mc, pdata);
	if (ret < 0)
		return ret;
#endif
	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_cp_off = pdata->gpio_cp_off;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;

	mc->gpio_cp_dump_int = pdata->gpio_cp_dump_int;
	mc->gpio_ap_dump_int = pdata->gpio_ap_dump_int;

	mc->gpio_pda_active = pdata->gpio_pda_active;

	ret = verify_gpios(mc);
	if (ret < 0)
		return ret;

	mc->phone_state = STATE_OFFLINE;

	mc->cp_crash_dump_wq = create_workqueue("cp_crash_dump_wq");
	if (!mc->cp_crash_dump_wq) {
		mif_err("cp_crash_dump_wq create fail\n");
		return -ENODEV;
	}
	INIT_DELAYED_WORK(&mc->cp_crash_dump_dwork, cp_crash_dump_work_func);

	mc->cp_crash_nfb.notifier_call = cp_crash_notify_call;
	register_cp_crash_notifier(&mc->cp_crash_nfb);

	shannon_get_ops(mc);

	mc->irq_phone_active = gpio_to_irq(mc->gpio_cp_dump_int);

	ret = request_irq(mc->irq_phone_active, phone_active_irq_handler,
			IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING,
			"phone_active", mc);
	if (ret) {
		mif_err("request_irq fail(%d)\n", ret);
		goto err_request_irq;
	}

	ret = enable_irq_wake(mc->irq_phone_active);
	if (ret) {
		mif_err("enable_irq_wake fail(%d)\n", ret);
		goto err_enable_irq_wake;
	}

	mif_info("success\n");

	return ret;

err_enable_irq_wake:
	free_irq(mc->irq_phone_active, mc);
err_request_irq:
	return ret;
}
