// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-n1-system.h"
#include "dsp-hw-n1-memory.h"
#include "dsp-hw-n1-ctrl.h"
#include "dsp-hw-n1-dump.h"
#include "hardware/dummy/dsp-hw-dummy-interface.h"
#include "dsp-hw-n1-interface.h"

#define DSP_N1_IRQ_MAX_NUM	(5)

struct dsp_hw_n1_interface {
	int irq[DSP_N1_IRQ_MAX_NUM];
};

static void __dsp_hw_n1_interface_isr(struct dsp_interface *itf);

static int dsp_hw_n1_interface_send_irq(struct dsp_interface *itf, int status)
{
	int ret;

	dsp_enter();
	if (status & BIT(DSP_TO_CC_INT_RESET)) {
		dsp_ctrl_writel(DSP_N1_DSPC_HOST_MAILBOX_INTR_NS, 0x1 << 8);
	} else if (status & BIT(DSP_TO_CC_INT_MAILBOX)) {
		dsp_ctrl_writel(DSP_N1_DSPC_CA5_SWI_SET_NS, 0x1 << 9);
	} else {
		dsp_err("wrong interrupt requested(%x)\n", status);
		ret = -EINVAL;
		goto p_err;
	}
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_n1_interface_check_irq(struct dsp_interface *itf)
{
	unsigned int status;
	unsigned long flags;

	dsp_enter();
	spin_lock_irqsave(&itf->irq_lock, flags);
	status = dsp_ctrl_readl(DSP_N1_DSPC_TO_HOST_MAILBOX_INTR_NS);
	if (!(status >> 8))
		goto p_end;

	__dsp_hw_n1_interface_isr(itf);

	dsp_ctrl_writel(DSP_N1_DSPC_TO_HOST_MAILBOX_INTR_NS, 0x0);
	dsp_leave();
p_end:
	spin_unlock_irqrestore(&itf->irq_lock, flags);
	return 0;
}

static void __dsp_hw_n1_interface_isr_boot_done(struct dsp_interface *itf)
{
	dsp_enter();
	if (!(itf->sys->system_flag & BIT(DSP_SYSTEM_BOOT))) {
		itf->sys->system_flag = BIT(DSP_SYSTEM_BOOT);
		wake_up(&itf->sys->system_wq);
	} else {
		dsp_warn("boot_done interrupt occurred incorrectly\n");
	}
	dsp_leave();
}

static void __dsp_hw_n1_interface_isr_reset_done(struct dsp_interface *itf)
{
	dsp_enter();
	if (!(itf->sys->system_flag & BIT(DSP_SYSTEM_RESET))) {
		itf->sys->system_flag = BIT(DSP_SYSTEM_RESET);
		wake_up(&itf->sys->system_wq);
	} else {
		dsp_warn("reset_done interrupt occurred incorrectly\n");
	}
	dsp_leave();
}

static void __dsp_hw_n1_interface_isr_reset_request(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_info("reset request\n");
	dsp_leave();
}

static void __dsp_hw_n1_interface_isr_mailbox(struct dsp_interface *itf)
{
	dsp_enter();
	itf->sys->mailbox.ops->receive_task(&itf->sys->mailbox);
	dsp_leave();
}

static void __dsp_hw_n1_interface_isr(struct dsp_interface *itf)
{
	unsigned int status;

	dsp_enter();
	status = dsp_ctrl_sm_readl(
			DSP_N1_SM_RESERVED(DSP_N1_SM_TO_HOST_INT_STATUS));
	if (status & BIT(DSP_TO_HOST_INT_BOOT)) {
		__dsp_hw_n1_interface_isr_boot_done(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_MAILBOX)) {
		__dsp_hw_n1_interface_isr_mailbox(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_RESET_DONE)) {
		__dsp_hw_n1_interface_isr_reset_done(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_RESET_REQUEST)) {
		__dsp_hw_n1_interface_isr_reset_request(itf);
	} else {
		dsp_err("interrupt status is invalid (%u)\n", status);
		dsp_dump_ctrl();
	}
	dsp_ctrl_sm_writel(DSP_N1_SM_RESERVED(DSP_N1_SM_TO_HOST_INT_STATUS), 0);
	dsp_leave();
}

/* Mailbox or SWI (non-secure) */
static irqreturn_t dsp_hw_n1_interface_isr0(int irq, void *data)
{
	struct dsp_interface *itf;
	unsigned int status;

	dsp_enter();
	itf = (struct dsp_interface *)data;

	status = dsp_ctrl_readl(DSP_N1_DSPC_TO_HOST_MAILBOX_INTR_NS);
	if (!(status >> 8)) {
		dsp_warn("interrupt occurred incorrectly\n");
		dsp_dump_ctrl();
		goto p_end;
	}

	__dsp_hw_n1_interface_isr(itf);

	dsp_ctrl_writel(DSP_N1_DSPC_TO_HOST_MAILBOX_INTR_NS, 0x0);

	dsp_leave();
p_end:
	return IRQ_HANDLED;
}

/* GIC IRQ */
static irqreturn_t dsp_hw_n1_interface_isr1(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("GIC IRQ\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

/* Mailbox or SWI (secure) */
static irqreturn_t dsp_hw_n1_interface_isr2(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("Mailbox or SWI (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

/* GIC FIQ */
static irqreturn_t dsp_hw_n1_interface_isr3(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("GIC FIQ\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

/* WDT reset request */
static irqreturn_t dsp_hw_n1_interface_isr4(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("WDT reset request\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

static irqreturn_t (*isr_list[DSP_N1_IRQ_MAX_NUM])(int, void *) = {
	dsp_hw_n1_interface_isr0,
	dsp_hw_n1_interface_isr1,
	dsp_hw_n1_interface_isr2,
	dsp_hw_n1_interface_isr3,
	dsp_hw_n1_interface_isr4,
};

static int dsp_hw_n1_interface_open(struct dsp_interface *itf)
{
	int ret, idx;
	struct dsp_hw_n1_interface *itf_sub;

	dsp_enter();
	itf_sub = itf->sub_data;

	for (idx = 0; idx < DSP_N1_IRQ_MAX_NUM; ++idx) {
		ret = devm_request_irq(itf->sys->dev, itf_sub->irq[idx],
				isr_list[idx], 0, dev_name(itf->sys->dev), itf);
		if (ret) {
			dsp_err("Failed to request irq%d(%d)\n", idx, ret);
			goto p_err_req_irq;
		}
	}

	dsp_leave();
	return 0;
p_err_req_irq:
	for (idx -= 1; idx >= 0; --idx)
		devm_free_irq(itf->sys->dev, itf_sub->irq[idx], itf);
	return ret;
}

static int dsp_hw_n1_interface_close(struct dsp_interface *itf)
{
	int idx;
	struct dsp_hw_n1_interface *itf_sub;

	dsp_enter();
	itf_sub = itf->sub_data;

	for (idx = 0; idx < DSP_N1_IRQ_MAX_NUM; ++idx)
		devm_free_irq(itf->sys->dev, itf_sub->irq[idx], itf);
	dsp_leave();
	return 0;
}

static int dsp_hw_n1_interface_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_interface *itf;
	struct dsp_hw_n1_interface *itf_sub;
	struct platform_device *pdev;
	int idx;

	dsp_enter();
	itf = &sys->interface;
	itf->sys = sys;
	itf->sfr = sys->sfr;
	pdev = to_platform_device(sys->dev);

	itf_sub = kzalloc(sizeof(*itf_sub), GFP_KERNEL);
	if (!itf_sub) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc itf_sub\n");
		goto p_err_itf_sub;
	}
	itf->sub_data = itf_sub;

	for (idx = 0; idx < DSP_N1_IRQ_MAX_NUM; ++idx) {
		ret = platform_get_irq(pdev, idx);
		if (ret < 0) {
			dsp_err("Failed to get irq(%d/%d)\n", idx, ret);
			goto p_err_get_irq;
		}
		itf_sub->irq[idx] = ret;
	}

	spin_lock_init(&itf->irq_lock);
	dsp_leave();
	return 0;
p_err_get_irq:
	kfree(itf->sub_data);
p_err_itf_sub:
	return ret;
}

static void dsp_hw_n1_interface_remove(struct dsp_interface *itf)
{
	dsp_enter();
	kfree(itf->sub_data);
	dsp_leave();
}

static const struct dsp_interface_ops n1_interface_ops = {
	.send_irq	= dsp_hw_n1_interface_send_irq,
	.check_irq	= dsp_hw_n1_interface_check_irq,

	.start		= dsp_hw_dummy_interface_start,
	.stop		= dsp_hw_dummy_interface_stop,
	.open		= dsp_hw_n1_interface_open,
	.close		= dsp_hw_n1_interface_close,
	.probe		= dsp_hw_n1_interface_probe,
	.remove		= dsp_hw_n1_interface_remove,
};

int dsp_hw_n1_interface_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_interface_register_ops(DSP_DEVICE_ID_N1, &n1_interface_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
