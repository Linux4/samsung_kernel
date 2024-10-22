// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include "adsp_platform_driver.h"
#include "adsp_ipic.h"

#define IPIC_CHN_BASE              ipic_ctrl.ipic_chn
#define IPIC_SRC_CLR               ipic_ctrl.ipic_src_clr
#define IPIC_CHN_DST_SET           (IPIC_CHN_BASE + 0x04)
#define IPIC_CHN_MASK_SET          (IPIC_CHN_BASE + 0x14)
#define IPIC_CHN_MASK_CLR          (IPIC_CHN_BASE + 0x18)
#define IPIC_CHN_INT_SEND          (IPIC_CHN_BASE + 0x1C)
#define IPCI_CHN_MBOX_DATA         (IPIC_CHN_BASE + 0x20)

#define IPIC_SET(addr, mask)       writel(readl(addr) | (mask), addr)
#define IPIC_CLR(addr, mask)       writel(readl(addr) & ~(mask), addr)

static struct adsp_ipic_control ipic_ctrl;

static void adsp_ipic_close(void);

int adsp_ipic_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ipic_chn");
	if (!res) {
		pr_info("%s get resource IPIC_CHN fail\n", __func__);
		return -ENODEV;
	}

	ipic_ctrl.ipic_chn = devm_ioremap(dev, res->start, resource_size(res));
	if (unlikely(!ipic_ctrl.ipic_chn)) {
		pr_info("%s get ioremap IPIC_CHN fail 0x%llx\n", __func__, res->start);
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ipic_src_clr");
	if (!res) {
		pr_info("%s get resource IPIC_SRC_CLR fail\n", __func__);
		adsp_ipic_close();
		return -ENODEV;
	}

	ipic_ctrl.ipic_src_clr = devm_ioremap(dev, res->start, resource_size(res));
	if (unlikely(!ipic_ctrl.ipic_src_clr)) {
		pr_info("%s get ioremap IPIC_SRC_CLR fail 0x%llx\n", __func__, res->start);
		adsp_ipic_close();
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "ipic-ap-up-id", &ipic_ctrl.ap_up_id);
	if (ret) {
		pr_info("%s get ipic-ap-up-id fail\n", __func__);
		adsp_ipic_close();
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "ipic-adsp-up-id", &ipic_ctrl.adsp_up_id);
	if (ret) {
		pr_info("%s get ipic-adsp-up-id fail\n", __func__);
		adsp_ipic_close();
		return -ENODEV;
	}

	ret = of_property_read_u32(dev->of_node, "ipic-ap2adsp-chn", &ipic_ctrl.ap2adsp_chn);
	if (ret) {
		pr_info("%s get ipic-ap2adsp-chn fail\n", __func__);
		adsp_ipic_close();
		return -ENODEV;
	}

	spin_lock_init(&ipic_ctrl.wakelock);

	return 0;
}
EXPORT_SYMBOL(adsp_ipic_probe);

static void adsp_ipic_close(void)
{
	if (ipic_ctrl.ipic_chn != NULL) {
		iounmap(ipic_ctrl.ipic_chn);
		ipic_ctrl.ipic_chn = NULL;
	}

	if (ipic_ctrl.ipic_src_clr != NULL) {
		iounmap(ipic_ctrl.ipic_src_clr);
		ipic_ctrl.ipic_src_clr = NULL;
	}
}

bool adsp_is_ipic_support(void)
{
	return (ipic_ctrl.ipic_chn != NULL) && (ipic_ctrl.ipic_src_clr != NULL);
}

bool is_adsp_recv_ipic_irq(void)
{
	return (readl(IPIC_CHN_INT_SEND) == 2);
}

int adsp_ipic_send(struct adsp_ipic_mbox_data *mbox_data, bool need_ack)
{
	unsigned long flags;

	if (!adsp_is_ipic_support())
		return -EFAULT;

	spin_lock_irqsave(&ipic_ctrl.wakelock, flags);
	if (mbox_data != NULL)
		memcpy_toio(IPCI_CHN_MBOX_DATA, mbox_data, sizeof(struct adsp_ipic_mbox_data));

	if (!need_ack)
		IPIC_SET(IPIC_CHN_MASK_SET, 1 << ipic_ctrl.ap_up_id);

	IPIC_SET(IPIC_CHN_DST_SET, 1 << ipic_ctrl.adsp_up_id);
	spin_unlock_irqrestore(&ipic_ctrl.wakelock, flags);

	return 0;
}

void ipic_clr_chn(void)
{
	unsigned long flags;

	if (!adsp_is_ipic_support())
		return;

	spin_lock_irqsave(&ipic_ctrl.wakelock, flags);
	if (is_adsp_recv_ipic_irq())
		IPIC_SET(IPIC_SRC_CLR, 1 << ipic_ctrl.ap2adsp_chn);
	spin_unlock_irqrestore(&ipic_ctrl.wakelock, flags);
}
