/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * MCU IPC driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#include <mach/regs-mcu_ipc.h>

#include "mcu_ipc.h"

static irqreturn_t mcu_ipc_handler(int irq, void *data)
{
	u32 irq_stat, i;

	irq_stat = mcu_ipc_readl(EXYNOS_MCU_IPC_INTSR0) & 0xFFFF0000;

	for (i = 0; i < 16; i++) {
		if (irq_stat & (1 << (i + 16))) {
			if ((1 << (i + 16)) & mcu_dat.registered_irq)
				mcu_dat.hd[i].handler(mcu_dat.hd[i].data);
			else
				dev_err(mcu_dat.mcu_ipc_dev,
					"Unregistered INT received.\n");

			/* Interrupt Clear */
			mcu_ipc_writel(1 << (i + 16), EXYNOS_MCU_IPC_INTCR0);
			irq_stat &= ~(1 << (i + 16));
		}

		if (!irq_stat)
			break;
	}

	return IRQ_HANDLED;
}

int mbox_request_irq(u32 int_num, void (*handler)(void *),
							void *data)
{
	if ((!handler) || (int_num > 15))
		return -EINVAL;

	mcu_dat.hd[int_num].data = data;
	mcu_dat.hd[int_num].handler = handler;
	mcu_dat.registered_irq |= 1 << (int_num + 16);

	return 0;
}
EXPORT_SYMBOL(mbox_request_irq);

int mcu_ipc_unregister_handler(u32 int_num, void (*handler)(void *))
{
	if (!handler || (mcu_dat.hd[int_num].handler != handler))
		return -EINVAL;

	mcu_dat.hd[int_num].data = NULL;
	mcu_dat.hd[int_num].handler = NULL;
	mcu_dat.registered_irq &= ~(1 << (int_num + 16));

	return 0;
}
EXPORT_SYMBOL(mcu_ipc_unregister_handler);

void mbox_set_interrupt(u32 int_num)
{
	/* generate interrupt */
	if (int_num < 16)
		mcu_ipc_writel(0x1 << int_num, EXYNOS_MCU_IPC_INTGR1);
}
EXPORT_SYMBOL(mbox_set_interrupt);

void mcu_ipc_send_command(u32 int_num, u16 cmd)
{
	/* write command */
	if (int_num < 16)
		mcu_ipc_writel(cmd, EXYNOS_MCU_IPC_ISSR0 + (8 * int_num));

	/* generate interrupt */
	mbox_set_interrupt(int_num);
}
EXPORT_SYMBOL(mcu_ipc_send_command);

void mcu_ipc_clear_all_interrupt(void)
{
	mcu_ipc_writel(0xFFFF, EXYNOS_MCU_IPC_INTCR1);
}

u32 mbox_get_value(u32 mbx_num)
{
	if (mbx_num < 64)
		return mcu_ipc_readl(EXYNOS_MCU_IPC_ISSR0 + (4 * mbx_num));
	else
		return 0;
}
EXPORT_SYMBOL(mbox_get_value);

void mbox_set_value(u32 mbx_num, u32 msg)
{
	if (mbx_num < 64)
		mcu_ipc_writel(msg, EXYNOS_MCU_IPC_ISSR0 + (4 * mbx_num));
}
EXPORT_SYMBOL(mbox_set_value);

static int __devinit mcu_ipc_probe(struct platform_device *pdev)
{
	struct resource *res = NULL;
	int mcu_ipc_irq;
	int err = 0;

	mcu_dat.mcu_ipc_dev = &pdev->dev;

	/* resource for mcu_ipc SFR region */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resource defined\n");
		return -ENOENT;
	}
	res = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!res) {
		dev_err(&pdev->dev, "failded to request memory resource\n");
		return -ENOENT;
	}
	mcu_dat.ioaddr = ioremap(res->start, resource_size(res));
	if (!mcu_dat.ioaddr) {
		dev_err(&pdev->dev, "failded to request memory resource\n");
		err = -ENXIO;
		goto release_mcu_ipc;
	}

	/* Request IRQ */
	mcu_ipc_irq = platform_get_irq(pdev, 0);
	if (mcu_ipc_irq < 0) {
		dev_err(&pdev->dev, "no irq specified\n");
		err = mcu_ipc_irq;
		goto unmap_ioaddr;
	}
	err = request_irq(mcu_ipc_irq, mcu_ipc_handler, 0, pdev->name, pdev);
	if (err) {
		dev_err(&pdev->dev, "Can't request MCU_IPC IRQ\n");
		goto unmap_ioaddr;
	}

	/* set mcu_ipc_irq as the wakeup source */
	irq_set_irq_wake(mcu_ipc_irq, 1);

	mcu_ipc_clear_all_interrupt();

	return 0;

unmap_ioaddr:
	iounmap(mcu_dat.ioaddr);

release_mcu_ipc:
	release_mem_region(res->start, resource_size(res));

	return err;
}

static int __devexit mcu_ipc_remove(struct platform_device *pdev)
{
	/* TODO */
	return 0;
}

#ifdef CONFIG_PM
static int mcu_ipc_suspend(struct platform_device *dev, pm_message_t pm)
{
	/* TODO */
	return 0;
}

static int mcu_ipc_resume(struct platform_device *dev)
{
	/* TODO */
	return 0;
}
#else
#define mcu_ipc_suspend NULL
#define mcu_ipc_resume NULL
#endif

static struct platform_driver mcu_ipc_driver = {
	.probe		= mcu_ipc_probe,
	.remove		= __devexit_p(mcu_ipc_remove),
	.suspend	= mcu_ipc_suspend,
	.resume		= mcu_ipc_resume,
	.driver		= {
		.name	= "mcu_ipc",
		.owner	= THIS_MODULE,
	},
};

static int __init mcu_ipc_init(void)
{
	return platform_driver_register(&mcu_ipc_driver);
}
module_init(mcu_ipc_init);

static void __exit mcu_ipc_exit(void)
{
	platform_driver_unregister(&mcu_ipc_driver);
}
module_exit(mcu_ipc_exit);

MODULE_DESCRIPTION("MCU IPC driver");
MODULE_AUTHOR("Hyuk Lee <hyuk1.lee@samsung.com>");
MODULE_LICENSE("GPL");
