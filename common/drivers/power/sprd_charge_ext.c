/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/gpio.h>
#include <linux/device.h>

#include <linux/slab.h>
#include <linux/jiffies.h>
#include "sprd_battery.h"
#include <mach/usb.h>

#ifdef SPRDBAT_TWO_CHARGE_CHANNEL

struct sprdchg_drivier_data_ext {
    uint32_t gpio_vchg_ovi;
    uint32_t irq_vchg_ovi;
    uint32_t gpio_charge_en;
    uint32_t gpio_charge_done;
    struct work_struct ovi_irq_work;
};
static struct sprdchg_drivier_data_ext sprdchg_ext_ic;

int sprdchg_is_chg_done_ext(void)
{
	return gpio_get_value(sprdchg_ext_ic.gpio_charge_done);
}

void sprdchg_start_charge_ext(void)
{
	gpio_set_value(sprdchg_ext_ic.gpio_charge_en, 0);
}

void sprdchg_stop_charge_ext(void)
{
	gpio_set_value(sprdchg_ext_ic.gpio_charge_en, 1);
}

void sprdchg_open_ovi_fun_ext(void)
{
	irq_set_irq_type(sprdchg_ext_ic.irq_vchg_ovi, IRQ_TYPE_LEVEL_HIGH);
	enable_irq(sprdchg_ext_ic.irq_vchg_ovi);
}

void sprdchg_close_ovi_fun_ext(void)
{
	disable_irq_nosync(sprdchg_ext_ic.irq_vchg_ovi);
}

static void sprdchg_ovi_irq_works_ext(struct work_struct *work)
{
	int value;
	value = gpio_get_value(sprdchg_ext_ic.gpio_vchg_ovi);
	if (value) {
		sprdbat_charge_event_ext(SPRDBAT_CHG_EVENT_EXT_OVI);
	} else {
		sprdbat_charge_event_ext(SPRDBAT_CHG_EVENT_EXT_OVI_RESTART);
	}
}

static __used irqreturn_t sprdchg_vchg_ovi_irq_ext(int irq, void *dev_id)
{
	int value;
	value = gpio_get_value(sprdchg_ext_ic.gpio_vchg_ovi);
	if (value) {
		printk("sprdchg_vchg_ovi_irq_ext ovi high\n");
		irq_set_irq_type(sprdchg_ext_ic.irq_vchg_ovi, IRQ_TYPE_LEVEL_LOW);
	} else {
		printk("sprdchg_vchg_ovi_irq_ext ovi low\n");
		irq_set_irq_type(sprdchg_ext_ic.irq_vchg_ovi, IRQ_TYPE_LEVEL_HIGH);
	}
	schedule_work(&sprdchg_ext_ic.ovi_irq_work);
	return IRQ_HANDLED;
}

int sprdchg_charge_init_ext(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct resource *res = NULL;

	res = platform_get_resource(pdev, IORESOURCE_IO, 3);
	sprdchg_ext_ic.gpio_charge_done = res->start;
	res = platform_get_resource(pdev, IORESOURCE_IO, 4);
	sprdchg_ext_ic.gpio_charge_en = res->start;
	res = platform_get_resource(pdev, IORESOURCE_IO, 5);
	sprdchg_ext_ic.gpio_vchg_ovi = res->start;
	gpio_request(sprdchg_ext_ic.gpio_vchg_ovi, "vchg_ovi_ext");
	gpio_direction_input(sprdchg_ext_ic.gpio_vchg_ovi);
	sprdchg_ext_ic.irq_vchg_ovi = gpio_to_irq(sprdchg_ext_ic.gpio_vchg_ovi);
	set_irq_flags(sprdchg_ext_ic.irq_vchg_ovi, IRQF_VALID | IRQF_NOAUTOEN);
	ret = request_irq(sprdchg_ext_ic.irq_vchg_ovi, sprdchg_vchg_ovi_irq_ext,
		    IRQF_NO_SUSPEND, "sprdbat_vchg_ovi_ext", NULL);
	INIT_WORK(&sprdchg_ext_ic.ovi_irq_work, sprdchg_ovi_irq_works_ext);
	gpio_request(sprdchg_ext_ic.gpio_charge_done, "chg_done_ext");
	gpio_direction_input(sprdchg_ext_ic.gpio_charge_done);
	gpio_request(sprdchg_ext_ic.gpio_charge_en, "charge_en_ext");
	gpio_direction_output(sprdchg_ext_ic.gpio_charge_en, 1);
	sprdchg_stop_charge_ext();
	return ret;
}

void sprdchg_chg_monitor_cb_ext(void *data)
{
	return;
}

#endif /*  */
