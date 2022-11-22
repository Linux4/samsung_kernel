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


#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/bitops.h>
#include <mach/board.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/syscore_ops.h>

#include "spi_gpio_ctrl.h"


#define GPIO_AP_TO_CP	239	/* cp side 71 */
#define GPIO_CP_TO_AP	238	/* cp side 70*/
#define GPIO_AP_WAKEUP_CP 150

#define DRIVER_NAME "spi_gpio"

#define DBG(f, x...) 	pr_debug(DRIVER_NAME " [%s()]: " f, __func__,## x)

static unsigned int cp_to_ap_irq;
static bool irq_need_change = true;

int cp2ap_sts(void)
{
	DBG("spi :cp2ap_sts=%d\r\n",gpio_get_value(GPIO_CP_TO_AP));
	return gpio_get_value(GPIO_CP_TO_AP);
}

int ap2cp_sts(void)
{
	DBG("spi :ap2cp_sts=%d\r\n",gpio_get_value(GPIO_AP_TO_CP));
	return gpio_get_value(GPIO_AP_TO_CP);
}

void ap2cp_enable(void)
{
	DBG("spi ap2cp_enable AP_TO_CP_sts is 1\r\n");
	gpio_set_value(GPIO_AP_TO_CP, 1);
}

void ap2cp_disable(void)
{
	DBG("spi ap2cp_disable AP_TO_CP is 0\r\n");
	gpio_set_value(GPIO_AP_TO_CP, 0);
}

void ap2cp_wakeup(void)
{
	gpio_set_value(GPIO_AP_WAKEUP_CP, 0);
}

void ap2cp_sleep(void)
{
	gpio_set_value(GPIO_AP_WAKEUP_CP, 1);
}


static irqreturn_t cp_to_ap_irq_handle(int irq, void *handle)
{
	struct ipc_spi_dev *dev = (struct ipc_spi_dev*)handle;
	if(!irq_need_change) {
		irq_set_irq_type(cp_to_ap_irq,  IRQ_TYPE_EDGE_BOTH);
		irq_need_change = true;
	}
	if(cp2ap_sts())
	{
		//irq_set_irq_type( irq,  IRQF_TRIGGER_LOW);
		dev->rx_ctl = CP2AP_RASING;
		dev->bneedrcv = true;
		/* just for debug */
		dev->cp2ap[dev->irq_num % 100].status = 1;
		dev->cp2ap[dev->irq_num % 100].time = cpu_clock(0);
	}
	else
	{
		//irq_set_irq_type( irq,  IRQF_TRIGGER_HIGH);
		dev->rx_ctl = CP2AP_FALLING;
		dev->bneedrcv = false;
		/* just for debug */
		dev->cp2ap[dev->irq_num % 100].status = 0;
		dev->cp2ap[dev->irq_num % 100].time = cpu_clock(0);
	}
	dev->irq_num++;
	wake_up(&(dev->wait));
	return IRQ_HANDLED;
}

static int spi_syscore_suspend(void)
{
	if(irq_need_change) {
		irq_set_irq_type(cp_to_ap_irq,  IRQF_TRIGGER_HIGH);
		irq_need_change = false;
	}
	return 0;
}


static struct syscore_ops spi_syscore_ops = {
	.suspend    = spi_syscore_suspend,
};


/*****************************************************************************\
 *                                                                           *
 * Driver init/exit                                                          *
 *                                                                           *
\*****************************************************************************/

int spi_hal_gpio_init(void)
{
	int ret;

	DBG("spi_hal_gpio init \n");


	//config ap_to_cp_rts
	ret = gpio_request(GPIO_AP_TO_CP, "ap_to_cp_rts");
	if (ret) {
		DBG("Cannot request GPIO %d\r\n", GPIO_AP_TO_CP);
		gpio_free(GPIO_AP_TO_CP);
		return ret;
	}
	gpio_direction_output(GPIO_AP_TO_CP, 0);
	gpio_export(GPIO_AP_TO_CP,  1);

	//config GPIO_AP_WAKEUP_CP
	ret = gpio_request(GPIO_AP_WAKEUP_CP, "ap_wakeup_cp");
	if (ret) {
		DBG("Cannot request GPIO %d\r\n", GPIO_AP_WAKEUP_CP);
		gpio_free(GPIO_AP_WAKEUP_CP);
		return ret;
	}
	gpio_direction_output(GPIO_AP_WAKEUP_CP, 1);
	gpio_export(GPIO_AP_WAKEUP_CP,  1);

	//config cp_out_rdy
	ret = gpio_request(GPIO_CP_TO_AP, "cp_out_rdy");
	if (ret) {
		DBG("Cannot request GPIO %d\r\n",GPIO_CP_TO_AP);
		gpio_free(GPIO_CP_TO_AP);
		return ret;
	}
	gpio_direction_input(GPIO_CP_TO_AP);
	gpio_export(GPIO_CP_TO_AP,  1);

	return 0;
}

int spi_hal_gpio_irq_init(struct ipc_spi_dev *dev)
{
	int ret;

	DBG("spi_hal_gpio_irq_init \n");

    cp_to_ap_irq = gpio_to_irq(GPIO_CP_TO_AP);
	if (cp_to_ap_irq < 0)
		return -1;
	ret = request_irq(cp_to_ap_irq, cp_to_ap_irq_handle,
		IRQF_NO_SUSPEND | IRQF_DISABLED|IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "cp_to_ap_irq", (void*)dev);
	if (ret) {
		DBG("lee :cannot alloc cp_to_ap_rdy_irq, err %d\r\r\n", ret);
		return ret;
	}
	enable_irq_wake(cp_to_ap_irq);
	register_syscore_ops(&spi_syscore_ops);

	return 0;

}

void spi_hal_gpio_exit(void)
{
	DBG("spi_hal_gpio_exit gpio exit\r\n");

	gpio_free(GPIO_AP_TO_CP);
	gpio_free(GPIO_CP_TO_AP);

	free_irq(cp_to_ap_irq, NULL);
}

MODULE_AUTHOR("zhongping.tan<zhongping.tan@spreadtrum.com>");
MODULE_DESCRIPTION("GPIO driver about mux spi");
MODULE_LICENSE("GPL");
