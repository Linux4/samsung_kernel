/**
 * typec-sprd.c - Spreadtrum Type-C Glue layer
 *
 * Copyright (c) 2015 Spreadtrum Co., Ltd.
 *		http://www.spreadtrum.com
 *
 * Author: Miao Zhu <miao.zhu@spreadtrum.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <linux/mfd/typec.h>

#define REGS_SC2731_BASE ANA_REGS_GLB_BASE

#define REGS_TYPEC_BASE  ANA_TYPEC_BASE

#define TEST_ANA_REG_BASE		SCI_ADDR((SPRD_ADISLAVE_BASE + 0x550), 0x0000)
#define TEST_ANA_REG_BASE_EN		SCI_ADDR((SPRD_ADISLAVE_BASE + 0x528), 0x0000)

#define TESTANAREG_BIT14_EN	        (0x1 << 14)

/* registers definitions for controller REGS_TYPEC */

#define TYPEC_EN		        SCI_ADDR(REGS_TYPEC_BASE, 0x0000)
#define TYPEC_CLR		        SCI_ADDR(REGS_TYPEC_BASE, 0x0004)
#define TYPEC_MANUAL_DFP		SCI_ADDR(REGS_TYPEC_BASE, 0x0008)
#define TYPEC_AUTO_TOGGLE		SCI_ADDR(REGS_TYPEC_BASE, 0x000c)
#define TYPEC_CC_SWITCH 		SCI_ADDR(REGS_TYPEC_BASE, 0x0010)
#define TYPEC_INT_RAW			SCI_ADDR(REGS_TYPEC_BASE, 0x0014)
#define TYPEC_INT_MASK			SCI_ADDR(REGS_TYPEC_BASE, 0x0018)
#define TYPEC_PORT_STATUS		SCI_ADDR(REGS_TYPEC_BASE, 0x001c)
#define TYPEC_TOGGLE_CNT		SCI_ADDR(REGS_TYPEC_BASE, 0x0020)
#define TYPEC_TOGGLE_INVERVAL1		SCI_ADDR(REGS_TYPEC_BASE, 0x0024)
#define TYPEC_TOGGLE_INVERVAL2		SCI_ADDR(REGS_TYPEC_BASE, 0x0028)
#define TYPEC_ITRIM 			SCI_ADDR(REGS_TYPEC_BASE, 0x002c)
#define TYPEC_CC_RTRIM			SCI_ADDR(REGS_TYPEC_BASE, 0x0030)
#define TYPEC_CC_VREF			SCI_ADDR(REGS_TYPEC_BASE, 0x0034)
#define TYPEC_VBUS_CC_CNT		SCI_ADDR(REGS_TYPEC_BASE, 0x0038)
#define TYPEC_MODULE_RESET		SCI_ADDR(REGS_SC2731_BASE, 0x0024)
#define TYPEC_MODULE_EN 		SCI_ADDR(REGS_SC2731_BASE, 0x000C)

#define PORTTYPE_CC_MASK		0x0F
#define PORT_ATTCHED_DFP                0x5
#define PORT_UNATTCHED_DFP              0x3
#define BIT_MODULE_EN			(0x1 << 0)
#define BIT_INT_EN			(0x1 << 1)

#define BIT_INT_CLR 			(0x1 << 0)

#define BIT_PORT_DFP			(0x1 << 0)

#define BIT_PORT_DRP			(0x1 << 0)

#define BIT_CC1_SWITCH			(0x3 & 0x3)
#define BIT_CC2_SWITCH			(0x2 & 0x3)

#define BIT_ATTACH_INT			(0x1 << 0)
#define BIT_UNATTACH_INT		(0x1 << 1)
#define BIT_VBUS_CC_INT 		(0x1 << 2)

#define TYPEC_CC_CONNECT_SHIFT  	4
#define TYPEC_CC_CONNECT_MSK(x) 	((x) & 0x10)
#define TYPEC_CC1_CONNECT 		TYPEC_CC_CONNECT_MSK(1)
#define TYPEC_CC2_CONNECT 		TYPEC_CC_CONNECT_MSK(0)

#define TYPEC_TOGGLE_CNT_MSK(x) 	((x) & 0xffff)
#define TYPEC_TOGGLE_CNT_DEFUAUFT	TYPEC_TOGGLE_CNT_MSK(2)

#define TYPEC_TOGGLE_INVERVAL_MSK(x)	((x) & 0xffff)
#define TYPEC_TOGGLE_INVERVAL1_DEFAULT	TYPEC_TOGGLE_INVERVAL_MSK(0x7A12)
#define TYPEC_TOGGLE_INVERVAL2_DEFAULT	TYPEC_TOGGLE_INVERVAL_MSK(0x0)

#define TYPEC_ITRIM_MSK(x)		((x) & 0x3f)
#define TYPEC_ITRIM_DEFAULT		TYPEC_ITRIM_MSK(0x10)

#define TYPEC_CC1_RTRIM_MSK(x)		((x) & 0x1f)
#define TYPEC_CC2_RTRIM_MSK(x)		((x) & 0x3e0)
#define TYPEC_CC1_RTRIM_DEFAULT		TYPEC_CC1_RTRIM_MSK(0x10)
#define TYPEC_CC2_RTRIM_DEFAULT		TYPEC_CC2_RTRIM_MSK(0x10)

#define TYPEC_CC1_VREF_MSK(x)	((x) & 0x3)
#define TYPEC_CC2_VREF_MSK(x)	((x) & 0xc)
#define TYPEC_CC1_VREF_DEFAULT	TYPEC_CC1_VREF_MSK(0x1)
#define TYPEC_CC2_VREF_DEFAULT	TYPEC_CC2_VREF_MSK(0x1)

#define TYPEC_VBUS_CC_CNT_MSK(x)	((x) & 0xffff)
#define TYPEC_VBUS_CC_CNT_DEFAULT	TYPEC_VBUS_CC_CNT_MSK(0)

enum port_status {
	PORT_STATUS_UFP_UNATTACHED,
	PORT_STATUS_UFP_ATTACHED,
	PORT_STATUS_UFP_LOCK,	/* Module debug state machine */
	PORT_STATUS_DFP_UNATTACHED,
	PORT_STATUS_DFP_WAIT,	/* Module debug state machine */
	PORT_STATUS_DFP_ATTACHED,
	PORT_STATUS_POWER_CABLE,	/* Not support yet */
	PORT_STATUS_AUDIO_CABLE,	/* Not support yet */
	PORT_STATUS_DISABLED,	/* Module debug state machine */
};

enum typec_port_type {
	PORT_TYPE_UFP = 1,
	PORT_TYPE_DFP,
	PORT_TYPE_DRP,
};

struct typec_params {
	uint16_t toggle_cnt;
	uint32_t toggle_interval_cnt;
	uint16_t itrim;
	uint16_t rtrim;
	uint16_t vref;
	uint16_t vbus_cc_cnt;
};

struct typec {
	struct device *dev;
	int irq;

	enum typec_port_type port_type;
	enum typec_status status;

	enum port_status port_status;
	struct work_struct isr_work;
	struct typec_params params;
};
extern void sprd_extic_otg_power(int enable);
extern int sci_efuse_typec_cal_get(unsigned int *p_cal_data);
static void typec_set_type(uint32_t type);

static BLOCKING_NOTIFIER_HEAD(typec_chain_head);
static const char *const typec_port_type_string[] = {
	[0] = "UNKNOWN",
	[PORT_TYPE_UFP] = "UFP",
	[PORT_TYPE_DFP] = "DFP",
	[PORT_TYPE_DRP] = "DRP",
};
static const char *const typec_port_status_string[] = {
	[PORT_STATUS_UFP_UNATTACHED] = "disconnects with HOST",
	[PORT_STATUS_UFP_ATTACHED] = "connects with HOST",
	[PORT_STATUS_UFP_LOCK] = "is locking on HOST",
	[PORT_STATUS_DFP_UNATTACHED] = "disconnects with DEVICE",
	[PORT_STATUS_DFP_WAIT] = "is waiting DEVICE",
	[PORT_STATUS_DFP_ATTACHED] = "connects with DEVICE",
	[PORT_STATUS_POWER_CABLE] = "connects with POWER CABLE",
	[PORT_STATUS_AUDIO_CABLE] = "connects with AUDIO CABLE",
	[PORT_STATUS_DISABLED] = "is disabled",
};

static inline int reg_read(unsigned long reg)
{
	return sci_adi_read(reg);
}

static inline int reg_write(unsigned long reg, uint16_t val)
{
	return sci_adi_write(reg, val, 0);
}

static inline int reg_clear(unsigned long reg, uint16_t val)
{
	return sci_adi_write(reg, 0, val);
}

int register_typec_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&typec_chain_head, nb);
}

EXPORT_SYMBOL_GPL(register_typec_notifier);

int unregister_typec_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&typec_chain_head, nb);
}

EXPORT_SYMBOL_GPL(unregister_typec_notifier);

static int typec_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&typec_chain_head, val, NULL)
	        == NOTIFY_BAD) ? -EINVAL : 0;
}

static ssize_t port_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct typec *tc = dev_get_drvdata(dev);

	if (!tc)
		return -EINVAL;

	return sprintf(buf, "%s\n",
		typec_port_status_string[tc->port_status]);
}
static DEVICE_ATTR(port_status, S_IRUGO,
	port_status_show, NULL);

static ssize_t port_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct typec *tc = dev_get_drvdata(dev);

	if (!tc)
		return -EINVAL;

	return sprintf(buf, "Type now works on %s\n",
		typec_port_type_string[tc->port_type]);
}

static ssize_t port_type_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct typec *tc = dev_get_drvdata(dev);
	uint32_t port_type;

	if (!tc)
		return -EINVAL;

	sscanf(buf, "%d", &port_type);
	if (port_type > PORT_TYPE_DRP || port_type < PORT_TYPE_UFP)
		return -EINVAL;

	typec_set_type(port_type);
	tc->port_type = port_type;

	return size;
}
static DEVICE_ATTR(port_type, S_IRUGO | S_IWUSR,
	port_type_show, port_type_store);

static void typec_debug_info(void)
{
	pr_debug("TYPEC_EN:%x TYPEC_CLR:%x\n", reg_read(TYPEC_EN),
		reg_read(TYPEC_CLR));
	pr_debug("TYPEC_MANUAL_DFP:%x TYPEC_AUTO_TOGGLE:%x\n",
		reg_read(TYPEC_MANUAL_DFP), reg_read(TYPEC_AUTO_TOGGLE));
	pr_debug("TYPEC_CC_SWITCH:%x TYPEC_INT_RAW:%x\n",
		reg_read(TYPEC_CC_SWITCH), reg_read(TYPEC_INT_RAW));
	pr_debug("TYPEC_INT_MASK:%x TYPEC_PORT_STATUS:%x\n",
		reg_read(TYPEC_INT_MASK), reg_read(TYPEC_PORT_STATUS));
	pr_debug("TYPEC_TOGGLE_CNT:%x TYPEC_TOGGLE_INVERVAL1:%x\n",
		reg_read(TYPEC_TOGGLE_CNT), reg_read(TYPEC_TOGGLE_INVERVAL1));
	pr_debug("TYPEC_TOGGLE_INVERVAL2:%x\n",
		reg_read(TYPEC_TOGGLE_INVERVAL2));
	pr_debug("TYPEC_ITRIM:%x TYPEC_CC_RTRIM:%x\n", reg_read(TYPEC_ITRIM),
		reg_read(TYPEC_CC_RTRIM));
	pr_debug("TYPEC_CC_VREF:%x TYPEC_VBUS_CC_CNT:%x\n",
		reg_read(TYPEC_CC_VREF), reg_read(TYPEC_VBUS_CC_CNT));
	pr_debug("TYPEC_MODULE_RESET:%x TYPEC_MODULE_EN:%x\n",
		reg_read(TYPEC_MODULE_RESET), reg_read(TYPEC_MODULE_EN));
	pr_debug("typec:vbus 0k reg=0x%x vbus ok en reg=0x %x\n",
		reg_read(TEST_ANA_REG_BASE), reg_read(TEST_ANA_REG_BASE_EN));
}

static void typec_isr_work(struct work_struct *w)
{
	struct typec *tc = container_of(w, struct typec, isr_work);

	tc->port_status &= PORTTYPE_CC_MASK;
	pr_info("Type-C port type is %s, status %d, Port %s\n",
		typec_port_type_string[tc->port_type], tc->status,
		typec_port_status_string[tc->port_status]);

	typec_notifier_call_chain(tc->status);
}

static irqreturn_t typec_interrupt(int irq, void *dev_id)
{
	struct typec *tc = (struct typec *)dev_id;
	uint32_t int_msk, port_status;
	bool attach;


	if (!dev_id) {
		tc->status = TYPEC_STATUS_UNKNOWN;
		pr_err("typec: can't find the device\n");
		return -IRQ_HANDLED;
	}

	int_msk = reg_read(TYPEC_INT_MASK);
	if (int_msk & BIT_VBUS_CC_INT) {
		dev_err(tc->dev, "goes wrong!\n");
		tc->status = TYPEC_STATUS_UNKNOWN;
		goto end;
	} else if (int_msk & BIT_UNATTACH_INT)
		attach = 0;
	else
		attach = 1;

	port_status = reg_read(TYPEC_PORT_STATUS);
	port_status &= PORTTYPE_CC_MASK;
	switch (tc->port_type) {
	case PORT_TYPE_UFP:
		if (port_status == PORT_STATUS_UFP_ATTACHED)
			tc->status = TYPEC_STATUS_DEV_CONN;
		else
			tc->status = TYPEC_STATUS_DEV_DISCONN;
		break;
	case PORT_TYPE_DFP:
		if (port_status == PORT_STATUS_DFP_ATTACHED) {
			sprd_extic_otg_power(1);
			tc->status = TYPEC_STATUS_HOST_CONN;
		} else {
			sprd_extic_otg_power(0);
			tc->status = TYPEC_STATUS_HOST_DISCONN;
		}
		break;
	case PORT_TYPE_DRP:
		switch (port_status) {
		case PORT_STATUS_UFP_ATTACHED:
			tc->status = TYPEC_STATUS_DEV_CONN;
			break;
		case PORT_STATUS_DFP_ATTACHED:
			sprd_extic_otg_power(1);
			tc->status = TYPEC_STATUS_HOST_CONN;
			break;
		default:
			if (tc->status == TYPEC_STATUS_HOST_CONN && !attach) {
				sprd_extic_otg_power(0);
				tc->status = TYPEC_STATUS_HOST_DISCONN;
			} else if (tc->status == TYPEC_STATUS_DEV_CONN &&
				!attach)
				tc->status = TYPEC_STATUS_DEV_DISCONN;
			else
				tc->status = TYPEC_STATUS_UNKNOWN;
			break;
		}
		break;
	}

	tc->port_status = port_status;

	schedule_work(&tc->isr_work);
end:
	sci_adi_write(TYPEC_CLR, 1, BIT_INT_CLR);	/*clear interrupt */

	return IRQ_HANDLED;
}

static void typec_set_type(uint32_t type)
{
	/* Reset module */
	reg_write(TYPEC_MODULE_RESET, 1 << 3);
	udelay(10);
	reg_clear(TYPEC_MODULE_RESET, 1 << 3);

	switch (type) {
	case PORT_TYPE_DRP:
		reg_clear(TYPEC_MANUAL_DFP, BIT_PORT_DFP);
		reg_write(TYPEC_AUTO_TOGGLE, BIT_PORT_DRP);
		reg_clear(TYPEC_TOGGLE_INVERVAL1, 0xffff);
		reg_clear(TYPEC_TOGGLE_INVERVAL1, 0xffff);
		reg_write(TYPEC_TOGGLE_CNT, 0xffff);
		break;
	case PORT_TYPE_DFP:
		reg_write(TYPEC_MANUAL_DFP, BIT_PORT_DFP);
		reg_clear(TYPEC_AUTO_TOGGLE, BIT_PORT_DRP);
		break;
	case PORT_TYPE_UFP:
		reg_clear(TYPEC_MANUAL_DFP, BIT_PORT_DFP);
		reg_clear(TYPEC_AUTO_TOGGLE, BIT_PORT_DRP);
		break;
	default:
		break;
	}
	pr_info("port type is %s\n", typec_port_type_string[type]);
}
static int typec_set_params(struct typec *tc)
{
	unsigned int ccrtim[2], var;
	int ret;

	/*read params from EFUSE */
	ret = sci_efuse_typec_cal_get(ccrtim);
	if (!ret) {
		dev_err(tc->dev, "read efuse data error!!\n");
		return -1;
	}

	var = ccrtim[0] | (ccrtim[1] << 5);
	if (var)
		reg_clear(TYPEC_CC_RTRIM, 0xffff);
	reg_write(TYPEC_CC_RTRIM, var);

	dev_dbg(tc->dev, "TYPEC_CC_RTRIM = 0x%x\n", reg_read(TYPEC_CC_RTRIM));
	return 0;
}

static struct typec *__typec;

static int typec_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct typec *tc;

	int soft_rst_msk, port_type;
	int ret = 0;

	tc = devm_kzalloc(dev, sizeof(*tc), GFP_KERNEL);
	if (!tc) {
		dev_err(dev, "%s not enough memory\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, tc);
	tc->dev = dev;

	INIT_WORK(&tc->isr_work, typec_isr_work);

	ret = of_property_read_u32(node, "typec-soft-rst", &soft_rst_msk);
	if (ret) {
		dev_err(dev, "fail to get soft reset bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "typec-port-type", &port_type);
	if (ret) {
		dev_err(dev, "fail to get port type\n");
		return ret;
	}
	tc->port_type = port_type;
	if (tc->port_type < 1 || tc->port_type > 3)
		tc->port_type = PORT_TYPE_UFP;

	tc->irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(tc->dev, tc->irq, typec_interrupt,
		IRQF_NO_SUSPEND, "typec-sprd", tc);
	if (ret) {
		dev_err(dev, "fail to irq #%d --> %d\n", tc->irq, ret);
		return ret;
	}

	typec_set_type(tc->port_type);

	/* Init module */
	ret = typec_set_params(tc);
	if (ret) {
		dev_err(dev,
			"typec fail to init module's params, err code %d\n", ret);
		return ret;
	}

	reg_clear(TYPEC_VBUS_CC_CNT, 0xFFFF);
	reg_write(TYPEC_EN, BIT_INT_EN);

	__typec = tc;
	device_create_file(dev, &dev_attr_port_status);
	device_create_file(dev, &dev_attr_port_type);

	typec_debug_info();

	return 0;
}

static int typec_remove(struct platform_device *pdev)
{
	struct typec *tc = platform_get_drvdata(pdev);

	disable_irq(tc->irq);
	platform_device_unregister(pdev);

	return 0;
}

static const struct of_device_id typec_sprd_match[] = {
	{.compatible = "sprd,typec"},
	{},
};

MODULE_DEVICE_TABLE(of, typec_sprd_match);

static struct platform_driver typec_sprd_driver = {
	.probe = typec_probe,
	.remove = typec_remove,
	.driver = {
		.name = "sprd-typec",
		.of_match_table = typec_sprd_match,
	},
};

static int __init typec_sprd_init(void)
{
	return platform_driver_register(&typec_sprd_driver);
}

static void __exit typec_sprd_exit(void)
{
	platform_driver_unregister(&typec_sprd_driver);
}

device_initcall_sync(typec_sprd_init);
module_exit(typec_sprd_exit);

MODULE_ALIAS("platform: Type-C");
MODULE_AUTHOR("Miao Zhu <miao.zhu@spreadtrum.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ADI Type-C SPRD Glue Layer");
