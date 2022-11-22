/*
 * Copyright (C) 2014-2019, Samsung Electronics.
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

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/bitops.h>

#include "gnss_mbox.h"
#include "gnss_utils.h"

static struct gnss_mbox_drv_data _gnss_mbox[MAX_MBOX_REGION];

static inline void gnss_mbox_writel(enum gnss_mbox_region id, u32 val, long reg)
{
	writel(val, _gnss_mbox[id].base + reg);
}

static inline u32 gnss_mbox_readl(enum gnss_mbox_region id, long reg)
{
	return readl(_gnss_mbox[id].base + reg);
}

int gnss_mbox_register_irq(enum gnss_mbox_region id, u32 int_num, irq_handler_t handler, void *data)
{
	unsigned long flags;

	if ((!handler) || (int_num > 15))
		return -EINVAL;

	spin_lock_irqsave(&_gnss_mbox[id].lock, flags);

	_gnss_mbox[id].hd[int_num].data = data;
	_gnss_mbox[id].hd[int_num].handler = handler;
	_gnss_mbox[id].registered_irq |= 1 << (int_num + 16);
	set_bit(int_num, &_gnss_mbox[id].unmasked_irq);

	spin_unlock_irqrestore(&_gnss_mbox[id].lock, flags);

	gnss_mbox_enable_irq(id, int_num);
	gif_info("id:%d num:%d intmr0:0x%08x\n", id, int_num, gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0));

	return 0;
}
EXPORT_SYMBOL(gnss_mbox_register_irq);

int gnss_mbox_unregister_irq(enum gnss_mbox_region id, u32 int_num, irq_handler_t handler)
{
	unsigned long flags;

	if (!handler || (_gnss_mbox[id].hd[int_num].handler != handler))
		return -EINVAL;

	gnss_mbox_disable_irq(id, int_num);
	gif_info("id:%d num:%d intmr0:0x%08x\n", id, int_num, gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0));

	spin_lock_irqsave(&_gnss_mbox[id].lock, flags);

	_gnss_mbox[id].hd[int_num].data = NULL;
	_gnss_mbox[id].hd[int_num].handler = NULL;
	_gnss_mbox[id].registered_irq &= ~(1 << (int_num + 16));
	clear_bit(int_num, &_gnss_mbox[id].unmasked_irq);

	spin_unlock_irqrestore(&_gnss_mbox[id].lock, flags);

	return 0;
}
EXPORT_SYMBOL(gnss_mbox_unregister_irq);

int gnss_mbox_enable_irq(enum gnss_mbox_region id, u32 int_num)
{
	unsigned long flags;
	unsigned long tmp;

	/* The irq should have been registered. */
	if (!(_gnss_mbox[id].registered_irq & BIT(int_num + 16)))
		return -EINVAL;

	spin_lock_irqsave(&_gnss_mbox[id].lock, flags);

	tmp = gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0);

	/* Clear the mask if it was set. */
	if (test_and_clear_bit(int_num + 16, &tmp))
		gnss_mbox_writel(id, tmp, EXYNOS_MBOX_INTMR0);

	/* Mark the irq as unmasked */
	set_bit(int_num, &_gnss_mbox[id].unmasked_irq);

	spin_unlock_irqrestore(&_gnss_mbox[id].lock, flags);

	return 0;
}
EXPORT_SYMBOL(gnss_mbox_enable_irq);

int gnss_mbox_disable_irq(enum gnss_mbox_region id, u32 int_num)
{
	unsigned long flags;
	unsigned long irq_mask;

	/* The interrupt must have been registered. */
	if (!(_gnss_mbox[id].registered_irq & BIT(int_num + 16)))
		return -EINVAL;

	/* Set the mask */
	spin_lock_irqsave(&_gnss_mbox[id].lock, flags);

	irq_mask = gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0);

	/* Set the mask if it was not already set */
	if (!test_and_set_bit(int_num + 16, &irq_mask)) {
		gnss_mbox_writel(id, irq_mask, EXYNOS_MBOX_INTMR0);

		udelay(5);

		/* Reset the status bit to signal interrupt needs handling */
		gnss_mbox_writel(id, BIT(int_num + 16), EXYNOS_MBOX_INTGR0);

		udelay(5);
	}

	/* Remove the irq from the umasked irqs */
	clear_bit(int_num, &_gnss_mbox[id].unmasked_irq);

	spin_unlock_irqrestore(&_gnss_mbox[id].lock, flags);

	return 0;
}
EXPORT_SYMBOL(gnss_mbox_disable_irq);

void gnss_mbox_set_interrupt(enum gnss_mbox_region id, u32 int_num)
{
	/* generate interrupt */
	if (int_num < 16)
		gnss_mbox_writel(id, 0x1 << int_num, EXYNOS_MBOX_INTGR1);
}
EXPORT_SYMBOL(gnss_mbox_set_interrupt);

void gnss_mbox_send_command(enum gnss_mbox_region id, u32 int_num, u16 cmd)
{
	/* write command */
	if (int_num < 16)
		gnss_mbox_writel(id, cmd, EXYNOS_MBOX_ISSR0 + (8 * int_num));

	/* generate interrupt */
	gnss_mbox_set_interrupt(id, int_num);
}
EXPORT_SYMBOL(gnss_mbox_send_command);

u32 gnss_mbox_get_value(enum gnss_mbox_region id, u32 mbx_num)
{
	if (mbx_num < 64)
		return gnss_mbox_readl(id, EXYNOS_MBOX_ISSR0 + (4 * mbx_num));
	else
		return 0;
}
EXPORT_SYMBOL(gnss_mbox_get_value);

void gnss_mbox_set_value(enum gnss_mbox_region id, u32 mbx_num, u32 msg)
{
	if (mbx_num < 64)
		gnss_mbox_writel(id, msg, EXYNOS_MBOX_ISSR0 + (4 * mbx_num));
}
EXPORT_SYMBOL(gnss_mbox_set_value);

u32 gnss_mbox_extract_value(enum gnss_mbox_region id, u32 mbx_num, u32 mask, u32 pos)
{
	if (mbx_num < 64)
		return (gnss_mbox_get_value(id, mbx_num) >> pos) & mask;
	else
		return 0;
}
EXPORT_SYMBOL(gnss_mbox_extract_value);

void gnss_mbox_update_value(enum gnss_mbox_region id, u32 mbx_num, u32 msg, u32 mask, u32 pos)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&_gnss_mbox[id].lock, flags);

	if (mbx_num < 64) {
		val = gnss_mbox_get_value(id, mbx_num);
		val &= ~(mask << pos);
		val |= (msg & mask) << pos;
		gnss_mbox_set_value(id, mbx_num, val);
	}

	spin_unlock_irqrestore(&_gnss_mbox[id].lock, flags);
}
EXPORT_SYMBOL(gnss_mbox_update_value);

void gnss_mbox_sw_reset(enum gnss_mbox_region id)
{
	u32 reg_val;

	gif_info("Reset GNSS mailbox\n");

	reg_val = gnss_mbox_readl(id, EXYNOS_MBOX_MCUCTLR);
	reg_val |= (0x1 << MBOX_MCUCTLR_MSWRST);

	gnss_mbox_writel(id, reg_val, EXYNOS_MBOX_MCUCTLR);

	udelay(5);

	gnss_mbox_writel(id, ~(_gnss_mbox[id].unmasked_irq) << 16, EXYNOS_MBOX_INTMR0);
	gif_info("id:%d intmr0:0x%08x\n", id, gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0));
}
EXPORT_SYMBOL(gnss_mbox_sw_reset);

void gnss_mbox_clear_all_interrupt(enum gnss_mbox_region id)
{
	gnss_mbox_writel(id, 0xFFFF, EXYNOS_MBOX_INTCR1);
}
EXPORT_SYMBOL(gnss_mbox_clear_all_interrupt);

/* Interrupt handler */
static irqreturn_t gnss_mbox_handler(int irq, void *data)
{
	u32 irq_stat, i;
	u32 id;

	id = ((struct gnss_mbox_drv_data *)data)->id;

	spin_lock(&_gnss_mbox[id].lock);

	/* Check raised interrupts */
	irq_stat = gnss_mbox_readl(id, EXYNOS_MBOX_INTSR0) & 0xFFFF0000;

	/* Only clear and handle unmasked interrupts */
	irq_stat &= ~(gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0)) & 0xFFFF0000;

	/* Interrupt Clear */
	gnss_mbox_writel(id, irq_stat, EXYNOS_MBOX_INTCR0);
	spin_unlock(&_gnss_mbox[id].lock);

	for (i = 0; i < 16; i++) {
		if (irq_stat & (1 << (i + 16))) {
			if ((1 << (i + 16)) & _gnss_mbox[id].registered_irq) {
				_gnss_mbox[id].hd[i].handler(i, _gnss_mbox[id].hd[i].data);
			} else {
				gif_err_limited("Unregistered interrupt:%d %d 0x%08x 0x%08lx 0x%08x\n",
							id, i, irq_stat,
							_gnss_mbox[id].unmasked_irq << 16,
							gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0));
			}

			irq_stat &= ~(1 << (i + 16));
		}

		if (!irq_stat)
			break;
	}

	return IRQ_HANDLED;
}

/* Probe */
static int gnss_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;
	int ret = 0;
	u32 id = 0;
	int irq = 0;

	gif_info("+++\n");

	if (!dev->of_node) {
		gif_err("of_node is null\n");
		return -ENODEV;
	}

	gif_dt_read_u32(dev->of_node, "mbox,id", id);
	if (id >= MAX_MBOX_REGION) {
		gif_err("mbox,id is over %d:%d\n", MAX_MBOX_REGION, id);
		return -EINVAL;
	}
	_gnss_mbox[id].id = id;
	gif_dt_read_string(dev->of_node, "mbox,name", _gnss_mbox[id].name);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	_gnss_mbox[id].base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(_gnss_mbox[id].base)) {
		gif_err("devm_ioremap_resource() error\n");
		return PTR_ERR(_gnss_mbox[id].base);
	}

	irq = platform_get_irq(pdev, 0);

	gif_init_irq(&_gnss_mbox[id].irq_gnss_mbox, irq, pdev->name, IRQF_ONESHOT);
	ret = gif_request_irq(&_gnss_mbox[id].irq_gnss_mbox, gnss_mbox_handler, &_gnss_mbox[id]);
	if (ret) {
		gif_err("gif_request_irq() error:%d\n", ret);
		return ret;
	}

	spin_lock_init(&_gnss_mbox[id].lock);
	gnss_mbox_clear_all_interrupt(id);

	gnss_mbox_writel(id, 0xFFFF0000, EXYNOS_MBOX_INTMR0);
	gif_info("id:%d intmr0:0x%08x\n", id, gnss_mbox_readl(id, EXYNOS_MBOX_INTMR0));

	gif_info("---\n");

	return 0;
}

static int __exit gnss_mbox_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id gnss_mbox_dt_match[] = {
		{ .compatible = "samsung,exynos-gnss-mailbox", },
		{},
};
MODULE_DEVICE_TABLE(of, gnss_mbox_dt_match);

static struct platform_driver gnss_mbox_driver = {
	.probe = gnss_mbox_probe,
	.remove = gnss_mbox_remove,
	.driver = {
		.name = "gnss_mailbox",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gnss_mbox_dt_match),
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(gnss_mbox_driver);

MODULE_DESCRIPTION("Exynos GNSS mailbox driver");
MODULE_LICENSE("GPL");
