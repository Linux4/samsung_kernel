/*
 *  if2xx demod driver for Marvell PXA Platform
 * 
 * Copyright (C) 2012 Marvell International Ltd.
 * Author:      Lucao <lucao@marvell.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include "inno_spi_platform.h"


struct spi_device *g_spi = NULL;
struct cmmb_platform_data *gpdata;

static void inno_platform_power(unsigned char on)
{
	pr_debug("%s\n", __func__);
	if (on) {
		gpdata->power_on();
	} else {
		gpdata->power_off();
	}
}

static irqreturn_t (*irq_handler)(int irqnr, void *devid) = NULL;
static irqreturn_t plat_irq_handler(int irqnr, void *devid)
{
        if (irq_handler)
                return irq_handler(irqnr, devid);
        return IRQ_HANDLED;
}

static int inno_platform_spi_transfer(struct spi_message *msg)
{	

	if (g_spi) {
		spi_sync(g_spi, msg);
	}
	return 0;
}

int inno_platform_init(struct inno_platform *plat)
{       
	int ret = 0;

	irq_handler = plat->irq_handler;
	plat->power = inno_platform_power;
	plat->spi_transfer = inno_platform_spi_transfer;
	plat->spi = g_spi;
	plat->pdata = gpdata;
	return ret;
}
EXPORT_SYMBOL_GPL(inno_platform_init);

static int pxa_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct cmmb_platform_data *pdata;
	g_spi = spi;
	pdata = spi->dev.platform_data;
	gpdata = pdata;
	if (!pdata || !pdata->power_on)
		return -ENODEV;
	if (pdata->power_on)
		pdata->power_on();
	irq_handler = NULL;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret < 0)
		goto out;
	ret = request_irq(spi->irq, plat_irq_handler, IRQF_TRIGGER_FALLING,
			"CMMB Demodulator", NULL);
	if (ret < 0)
		pr_err("%s:request_irq failed\n", __func__);

	if (pdata->power_off)
		pdata->power_off();
out:
	if (pdata->power_off)
		pdata->power_off();

	return ret;
}

static int __devexit spi_remove(struct spi_device *spi)
{
        pr_debug("%s\n",__func__);
        return 0;
}

static struct spi_driver pxa_spi_driver = {
	.probe = pxa_spi_probe,
	.remove = __devexit_p(spi_remove),
	.driver = {
		.name = "cmmb_if",
		.owner = THIS_MODULE,
	},
};

static int __init inno_plat_init(void)
{
	pr_debug("%s\n",__func__);
	spi_register_driver(&pxa_spi_driver);
	return 0;
}

static void __exit inno_plat_exit(void)
{
	spi_unregister_driver(&pxa_spi_driver);
	pr_debug("%s\n",__func__);
}

module_init(inno_plat_init);
module_exit(inno_plat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lucao <lucao@marvell.com>");
MODULE_DESCRIPTION("innofidei cmmb platform");
