/* /linux/drivers/misc/modem_v2/modem_modemctl_device_xmm6262.c
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

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <plat/devs.h>
#include <linux/platform_data/modem.h>
#include "modem_prj.h"

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

static void xmm6262_gpio_revers_bias_clear(struct modem_ctl *mc)
{
	gpio_direction_output(mc->gpio_pda_active, 0);
	gpio_set_value(mc->gpio_phone_active, 0);
	gpio_direction_output(mc->gpio_cp_dump_int, 0);

	msleep(20);
}

static void xmm6262_gpio_revers_bias_restore(struct modem_ctl *mc)
{
	gpio_direction_input(mc->gpio_cp_dump_int);
}

static int xmm6262_force_crash_exit(struct modem_ctl *mc)
{
	unsigned gpio_ap_dump_int = mc->gpio_ap_dump_int;

	if (!gpio_ap_dump_int)
		return -ENXIO;

	if (gpio_get_value(gpio_ap_dump_int)) {
		gpio_set_value(gpio_ap_dump_int, 0);
		msleep(20);
	}
	gpio_set_value(gpio_ap_dump_int, 1);
	msleep(20);
	mif_info("set ap_dump_int(%d) to high=%d\n",
		gpio_ap_dump_int, gpio_get_value(gpio_ap_dump_int));
	gpio_set_value(gpio_ap_dump_int, 0);

	return 0;
}

static int xmm6262_on(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_cp_reset || !mc->gpio_cp_on || !mc->gpio_reset_req_n) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	if (mc->gpio_revers_bias_clear)
		mc->gpio_revers_bias_clear();
	else
		xmm6262_gpio_revers_bias_clear(mc);

	/* TODO */
	gpio_set_value(mc->gpio_reset_req_n, 0);
	gpio_set_value(mc->gpio_cp_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(100);
	gpio_set_value(mc->gpio_cp_reset, 1);
	/* If XMM6262 was connected with C2C, AP wait 50ms to BB Reset*/
	msleep(50);
	gpio_set_value(mc->gpio_reset_req_n, 1);

	gpio_set_value(mc->gpio_cp_on, 1);
	udelay(60);
	gpio_set_value(mc->gpio_cp_on, 0);
	msleep(20);

	if (mc->gpio_revers_bias_restore)
		mc->gpio_revers_bias_restore();
	else
		xmm6262_gpio_revers_bias_restore(mc);

	gpio_set_value(mc->gpio_pda_active, 1);

	mc->phone_state = STATE_BOOTING;

	return 0;
}

static int xmm6262_off(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_cp_reset || !mc->gpio_cp_on) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	gpio_set_value(mc->gpio_cp_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);

	if (mc->phone_state == STATE_ONLINE)
		mc->phone_state = STATE_OFFLINE;

	return 0;
}

static int xmm6262_reset(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_reset_req_n)
		return -ENXIO;

	if (mc->gpio_revers_bias_clear)
		mc->gpio_revers_bias_clear();
	else
		xmm6262_gpio_revers_bias_clear(mc);

	gpio_set_value(mc->gpio_reset_req_n, 0);
	msleep(10);
	gpio_set_value(mc->gpio_reset_req_n, 1);
	gpio_set_value(mc->gpio_pda_active, 1);

	if (mc->gpio_revers_bias_restore)
		mc->gpio_revers_bias_restore();
	else
		xmm6262_gpio_revers_bias_restore(mc);

	mc->phone_state = STATE_BOOTING;

	return 0;
}

static irqreturn_t phone_active_irq_handler(int irq, void *_mc)
{
	int phone_reset = 0;
	int phone_active_value = 0;
	int cp_dump_value = 0;
	int phone_state = 0;
	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	disable_irq_nosync(mc->irq_phone_active);

	if (!mc->gpio_cp_reset || !mc->gpio_phone_active ||
			!mc->gpio_cp_dump_int) {
		mif_err("no gpio data\n");
		return IRQ_HANDLED;
	}

	phone_reset = gpio_get_value(mc->gpio_cp_reset);
	phone_active_value = gpio_get_value(mc->gpio_phone_active);
	cp_dump_value = gpio_get_value(mc->gpio_cp_dump_int);

	mif_info("PA EVENT : reset =%d, pa=%d, cp_dump=%d\n",
				phone_reset, phone_active_value, cp_dump_value);

	if (phone_reset && phone_active_value)
		phone_state = STATE_BOOTING;
	else if (phone_reset && !phone_active_value) {
		if (cp_dump_value)
			phone_state = STATE_CRASH_EXIT;
		else
			phone_state = STATE_CRASH_RESET;
	} else if (mc->phone_state == STATE_CRASH_EXIT
					|| mc->phone_state == STATE_CRASH_RESET)
		phone_state = mc->phone_state;

	if (mc->iod && mc->iod->modem_state_changed)
		mc->iod->modem_state_changed(mc->iod, phone_state);

	if (mc->bootd && mc->bootd->modem_state_changed)
		mc->bootd->modem_state_changed(mc->bootd, phone_state);

	if (phone_active_value)
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_LOW);
	else
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_HIGH);
	enable_irq(mc->irq_phone_active);

	return IRQ_HANDLED;
}

static irqreturn_t sim_detect_irq_handler(int irq, void *_mc)
{
	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	if (mc->iod && mc->iod->sim_state_changed)
		mc->iod->sim_state_changed(mc->iod,
			gpio_get_value(mc->gpio_sim_detect) == mc->sim_polarity
			);

	return IRQ_HANDLED;
}

static void xmm6262_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = xmm6262_on;
	mc->ops.modem_off = xmm6262_off;
	mc->ops.modem_reset = xmm6262_reset;
	mc->ops.modem_force_crash_exit = xmm6262_force_crash_exit;
}

void xmm6262_start_loopback(struct io_device *iod, struct modem_shared *msd)
{
	struct link_device *ld = get_current_link(iod);
	struct sk_buff *skb = alloc_skb(16, GFP_ATOMIC);
	int ret;

	if (unlikely(!skb))
		return;
	memcpy(skb_put(skb, 1), (msd->loopback_ipaddr) ? "s" : "x", 1);
	skbpriv(skb)->iod = iod;
	skbpriv(skb)->ld = ld;

	ret = ld->send(ld, iod, skb);
	if (ret < 0) {
		mif_err("usb_tx_urb fail\n");
		dev_kfree_skb_any(skb);
	}
	mif_info("Send loopback key '%s'\n",
					(msd->loopback_ipaddr) ? "s" : "x");

}

#ifdef CONFIG_OF
static int dt_gpio_config(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret = 0;

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

	/* GPIO_RESET_REQ_N */
	pdata->gpio_reset_req_n =
		of_get_named_gpio(np, "mif,gpio_reset_req_n", 0);
	if (!gpio_is_valid(pdata->gpio_reset_req_n)) {
		mif_err("reset_req_n: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->gpio_reset_req_n, "RESET_REQ_N");
	if (ret)
		mif_err("fail to request gpio %s:%d\n", "RESET_REQ_N", ret);
	gpio_direction_output(pdata->gpio_reset_req_n, 0);

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
	/* s3c_gpio_setpull(gpio_cp_reset, S3C_GPIO_PULL_NONE); */

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

	return ret;
}
#endif

int xmm6262_init_modemctl_device(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret = 0;
	struct platform_device *pdev;
#ifdef CONFIG_OF
	ret = dt_gpio_config(mc, pdata);
	if (ret < 0)
		return ret;
#endif
	mc->gpio_reset_req_n = pdata->gpio_reset_req_n;
	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_cp_dump_int = pdata->gpio_cp_dump_int;
	mc->gpio_ap_dump_int = pdata->gpio_ap_dump_int;
	mc->gpio_flm_uart_sel = pdata->gpio_flm_uart_sel;
	mc->gpio_cp_warm_reset = pdata->gpio_cp_warm_reset;
	mc->gpio_sim_detect = pdata->gpio_sim_detect;
	mc->sim_polarity = pdata->sim_polarity;

	mc->gpio_revers_bias_clear = pdata->gpio_revers_bias_clear;
	mc->gpio_revers_bias_restore = pdata->gpio_revers_bias_restore;

	pdev = to_platform_device(mc->dev);
	mc->irq_phone_active = gpio_to_irq(mc->gpio_phone_active);

	if (mc->gpio_sim_detect)
		mc->irq_sim_detect = gpio_to_irq(mc->gpio_sim_detect);

	xmm6262_get_ops(mc);
	mc->msd->loopback_start = xmm6262_start_loopback;

	ret = request_irq(mc->irq_phone_active, phone_active_irq_handler,
				IRQF_NO_SUSPEND | IRQF_TRIGGER_HIGH,
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

	/* initialize sim_state if gpio_sim_detect exists */
	mc->sim_state.online = false;
	mc->sim_state.changed = false;
	if (mc->gpio_sim_detect) {
		ret = request_irq(mc->irq_sim_detect, sim_detect_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"sim_detect", mc);
		if (ret) {
			mif_err("failed to request_irq: %d\n", ret);
			goto err_sim_detect_request_irq;
		}

		ret = enable_irq_wake(mc->irq_sim_detect);
		if (ret) {
			mif_err("failed to enable_irq_wake: %d\n", ret);
			goto err_sim_detect_set_wake_irq;
		}

		/* initialize sim_state => insert: gpio=0, remove: gpio=1 */
		mc->sim_state.online =
			gpio_get_value(mc->gpio_sim_detect) == mc->sim_polarity;
	}

	/* CP Slient logging channel is ACM:1 */
	mc->fixed_log_ch = 1;

	return ret;

err_sim_detect_set_wake_irq:
	free_irq(mc->irq_sim_detect, mc);
err_sim_detect_request_irq:
	mc->sim_state.online = false;
	mc->sim_state.changed = false;
err_phone_active_set_wake_irq:
	free_irq(mc->irq_phone_active, mc);
err_phone_active_request_irq:
	return ret;
}
