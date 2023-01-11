/*
 * linux/include/video/mmp_esd.h
 * Header file for Marvell MMP Display Controller
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors: Yu Xu <yuxu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MMP_ESD_H_
#define _MMP_ESD_H_

#include <video/mipi_display.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/irq.h>

enum {
	ESD_STATUS_OK = 0,
	ESD_STATUS_NG = 1,
};

enum {
	ESD_POLLING = IRQF_TRIGGER_NONE,
	ESD_TRIGGER_RISING = IRQF_TRIGGER_RISING,
	ESD_TRIGGER_FALLING = IRQF_TRIGGER_FALLING,
	ESD_TRIGGER_HIGH = IRQF_TRIGGER_HIGH,
	ESD_TRIGGER_LOW = IRQF_TRIGGER_LOW,
};

static inline void esd_start(struct dsi_esd *esd)
{
	if (esd->type == ESD_POLLING)
		queue_delayed_work(esd->wq, &esd->work, HZ);
	else {
		enable_irq(esd->irq);
		irq_set_irq_type(esd->irq, esd->type);
	}
}

static inline void esd_stop(struct dsi_esd *esd)
{
	if (esd->type == ESD_POLLING)
		cancel_delayed_work_sync(&esd->work);
	else {
		irq_set_irq_type(esd->irq, IRQF_TRIGGER_NONE);
		disable_irq(esd->irq);
	}
}

static inline void esd_work(struct work_struct *work)
{
	int status = 0, retry = 5;
	struct dsi_esd *esd =
		(struct dsi_esd *)container_of(work,
					struct dsi_esd, work.work);
	struct mmp_panel *panel =
		(struct mmp_panel *)container_of(esd,
					struct mmp_panel, esd);

	if (esd->type == ESD_POLLING) {
		if (esd->esd_check)
			status = esd->esd_check(panel);
		if (status && esd->esd_recover)
			esd->esd_recover(panel);
	} else {
		do {
			if (esd->esd_recover)
				esd->esd_recover(panel);
			if (esd->esd_check)
				status = esd->esd_check(panel);
		} while (status && --retry);
	}
	esd_start(esd);
}

static inline void esd_panel_recover(struct mmp_path *path)
{
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
	if (dsi && dsi->set_status)
		dsi->set_status(dsi, MMP_RESET);
}

static inline irqreturn_t esd_handler(int irq, void *dev_id)
{
	struct dsi_esd *esd = (struct dsi_esd *)dev_id;

	irq_set_irq_type(irq, IRQF_TRIGGER_NONE);
	disable_irq_nosync(irq);
	queue_delayed_work(esd->wq, &esd->work, 0);

	return IRQ_HANDLED;
}

static inline int esd_check_vgh_on(struct dsi_esd *esd)
{
	int gpio = esd->gpio, type = esd->type, status;

	if (unlikely(gpio < 0)) {
		pr_warn("%s: vgh pin unused\n", __func__);
		return -ENODEV;
	}

	if (type == ESD_TRIGGER_RISING || type == ESD_TRIGGER_HIGH)
		status = gpio_get_value(gpio) ?
			ESD_STATUS_NG : ESD_STATUS_OK;
	else if (type == ESD_TRIGGER_FALLING || type == ESD_TRIGGER_LOW)
		status = !gpio_get_value(gpio) ?
			ESD_STATUS_NG : ESD_STATUS_OK;
	else
		status = ESD_STATUS_NG;

	return (status == ESD_STATUS_OK);
}


static inline int esd_init_irq(struct dsi_esd *esd)
{
	int gpio, irq, type, ret, status;

	gpio = esd->gpio;
	type = esd->type;
	irq = gpio_to_irq(gpio);

	ret = gpio_request(gpio, "dsi-esd");
	if (unlikely(ret)) {
		pr_err("%s: gpio_request (%d) failed: %d\n",
				__func__, gpio, ret);
		return ret;
	}
	gpio_direction_input(gpio);
	irq_set_status_flags(irq, IRQ_NOAUTOEN);
	irq_set_irq_type(irq, IRQF_TRIGGER_NONE);
	ret = request_irq(irq, esd_handler, 0, "dsi-esd-irq", esd);
	if (unlikely(ret)) {
		pr_err("%s, request_irq (%d) failed\n",
				__func__, irq);
		return ret;
	}

	if (type == ESD_TRIGGER_RISING || type == ESD_TRIGGER_HIGH)
		status = gpio_get_value(gpio) ?
			ESD_STATUS_NG : ESD_STATUS_OK;
	else if (type == ESD_TRIGGER_FALLING || type == ESD_TRIGGER_LOW)
		status = !gpio_get_value(gpio) ?
			ESD_STATUS_NG : ESD_STATUS_OK;
	else
		status = ESD_STATUS_NG;

	if (unlikely(status == ESD_STATUS_NG))
		pr_warn("%s: gpio(%d), value(%s) - ESD_STATUS_NG\n",
				__func__, gpio,
				gpio_get_value(gpio) ? "HIGH" : "LOW");
	pr_info("%s: type(%d), gpio(%d), irq(%d)\n",
			__func__, type, gpio, irq);
	return 0;
}

static inline void esd_init(struct mmp_panel *panel)
{
	struct dsi_esd *esd = &panel->esd;

	INIT_DELAYED_WORK(&esd->work, esd_work);
	esd->wq = alloc_workqueue("dsi-esd", WQ_HIGHPRI
				| WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	esd->type = panel->esd_type;
	esd->esd_check = panel->get_status;
	esd->esd_recover = panel->panel_esd_recover;

	if (esd->type != ESD_POLLING) {
		esd->gpio = panel->esd_gpio;
		esd->irq = gpio_to_irq(esd->gpio);
		esd_init_irq(esd);
	}
	pr_info("%s: type:%s\n", __func__,
			esd->type ? "intr" : "poll");
}

#endif  /* _MMP_ESD_H_ */
