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
#include <mach/mcu_ipc.h>

static struct mcu_ipc_drv_data mcu_dat;

static inline void mcu_ipc_writel(u32 val, int reg)
{
	writel(val, mcu_dat.ioaddr + reg);
}

static inline u32 mcu_ipc_readl(int reg)
{
	return readl(mcu_dat.ioaddr + reg);
}

static irqreturn_t mcu_ipc_handler(int irq, void *data)
{
	int i;
	u32 irq_stat;

	irq_stat = mcu_ipc_readl(EXYNOS_MCU_IPC_INTSR0) & 0xFFFF0000;
	if (!irq_stat) {
		pr_err("mif: %s: ERR! FALSE interrupt\n", __func__);
		goto exit;
	}

	for (i = 0; i < 16; i++) {
		u32 mask = 1 << (i + 16);

		if (irq_stat & mask) {
			/* Clear interrupt */
			mcu_ipc_writel(mask, EXYNOS_MCU_IPC_INTCR0);
			irq_stat &= ~mask;

			if (mask & mcu_dat.registered_irq) {
				struct mcu_ipc_irq_handler *hd = &mcu_dat.hd[i];
				hd->handler(hd->data);
			} else {
				pr_err("mif: %s: ERR! no ISR for IRQ%d\n",
					__func__, i);
			}
		}
	}

exit:
	return IRQ_HANDLED;
}

int mbox_request_irq(u32 int_num, void (*handler)(void *), void *data)
{
	if (int_num > 15 || !handler || !data)
		return -EINVAL;

	mcu_dat.hd[int_num].data = data;
	mcu_dat.hd[int_num].handler = handler;
	mcu_dat.registered_irq |= 1 << (int_num + 16);

	return 0;
}
EXPORT_SYMBOL(mbox_request_irq);

static int mcu_ipc_unregister_handler(u32 int_num, void (*handler)(void *))
{
	if (!handler || handler != mcu_dat.hd[int_num].handler)
		return -EINVAL;

	mcu_dat.hd[int_num].data = NULL;
	mcu_dat.hd[int_num].handler = NULL;
	mcu_dat.registered_irq &= ~(1 << (int_num + 16));

	return 0;
}

int mbox_free_irq(u32 int_num, void *data)
{
	if (int_num > 15 || !data || data != mcu_dat.hd[int_num].data)
		return -EINVAL;

	mcu_ipc_unregister_handler(int_num, mcu_dat.hd[int_num].handler);
	return 0;
}
EXPORT_SYMBOL(mbox_free_irq);

void mbox_set_interrupt(u32 int_num)
{
	/* generate interrupt */
	if (int_num < 16)
		mcu_ipc_writel(0x1 << int_num, EXYNOS_MCU_IPC_INTGR1);
}
EXPORT_SYMBOL(mbox_set_interrupt);

void mbox_send_command(u32 int_num, u16 cmd)
{
	/* write command */
	if (int_num < 16)
		mcu_ipc_writel(cmd, EXYNOS_MCU_IPC_ISSR0 + (8 * int_num));

	/* generate interrupt */
	mbox_set_interrupt(int_num);
}
EXPORT_SYMBOL(mbox_send_command);

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

static void mcu_ipc_clear_all_interrupt(void)
{
	mcu_ipc_writel(0xFFFF, EXYNOS_MCU_IPC_INTCR1);
}

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
	mcu_dat.ioaddr = ioremap_nocache(res->start, resource_size(res));
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
MODULE_AUTHOR("Hankook Jang <hankook.jang@samsung.com>");
MODULE_LICENSE("GPL");
