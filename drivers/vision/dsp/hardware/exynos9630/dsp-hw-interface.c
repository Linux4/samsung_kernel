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

#include "dsp-log.h"
#include "hardware/dsp-ctrl.h"
#include "hardware/dsp-system.h"
#include "hardware/dsp-interface.h"
#include "hardware/dsp-dump.h"

enum dsp_to_host_int_num {
	DSP_TO_HOST_INT_BOOT,
	DSP_TO_HOST_INT_MAILBOX,
	DSP_TO_HOST_INT_RESET_DONE,
	DSP_TO_HOST_INT_RESET_REQUEST,
	DSP_TO_HOST_INT_NUM,
};

int dsp_interface_interrupt(struct dsp_interface *itf, int status)
{
	int ret;

	dsp_enter();
	if (status & BIT(DSP_TO_CC_INT_RESET)) {
		dsp_ctrl_writel(DSPC_HOST_MAILBOX_INTR_NS, 0x1 << 8);
	} else if (status & BIT(DSP_TO_CC_INT_MAILBOX)) {
		dsp_ctrl_writel(DSPC_CA5_SWI_SET_NS, 0x1 << 9);
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

static void __dsp_interface_isr_boot_done(struct dsp_interface *itf)
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

static void __dsp_interface_isr_reset_done(struct dsp_interface *itf)
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

static void __dsp_interface_isr_reset_request(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_info("reset request\n");
	dsp_leave();
}

static void __dsp_interface_isr_mailbox(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_mailbox_receive_task(&itf->sys->mailbox);
	dsp_leave();
}

static void __dsp_interface_isr(struct dsp_interface *itf)
{
	unsigned int status;

	dsp_enter();
	status = dsp_ctrl_sm_readl(DSP_SM_RESERVED(TO_HOST_INT_STATUS));
	if (status & BIT(DSP_TO_HOST_INT_BOOT)) {
		__dsp_interface_isr_boot_done(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_MAILBOX)) {
		__dsp_interface_isr_mailbox(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_RESET_DONE)) {
		__dsp_interface_isr_reset_done(itf);
	} else if (status & BIT(DSP_TO_HOST_INT_RESET_REQUEST)) {
		__dsp_interface_isr_reset_request(itf);
	} else {
		dsp_err("interrupt status is invalid\n");
		dsp_dump_ctrl();
	}
	dsp_ctrl_sm_writel(DSP_SM_RESERVED(TO_HOST_INT_STATUS), 0x0);
	dsp_leave();
}

/* Mailbox or SWI (non-secure) */
static irqreturn_t dsp_interface_isr0(int irq, void *data)
{
	struct dsp_interface *itf;
	unsigned int status;

	dsp_enter();
	itf = (struct dsp_interface *)data;

	status = dsp_ctrl_readl(DSPC_TO_HOST_MAILBOX_INTR_NS);
	if (!(status >> 8)) {
		dsp_err("interrupt occurred incorrectly\n");
		dsp_dump_ctrl();
		goto p_end;
	}

	__dsp_interface_isr(itf);

	dsp_ctrl_writel(DSPC_TO_HOST_MAILBOX_INTR_NS, 0x0);

	dsp_leave();
p_end:
	return IRQ_HANDLED;
}

/* GIC IRQ */
static irqreturn_t dsp_interface_isr1(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("GIC IRQ\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

/* Mailbox or SWI (secure) */
static irqreturn_t dsp_interface_isr2(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("Mailbox or SWI (secure)\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

/* GIC FIQ */
static irqreturn_t dsp_interface_isr3(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("GIC FIQ\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

/* WDT reset request */
static irqreturn_t dsp_interface_isr4(int irq, void *data)
{
	//struct dsp_interface *itf = (struct dsp_interface *)data;

	dsp_enter();
	dsp_info("WDT reset request\n");
	dsp_dump_ctrl();
	dsp_leave();
	return IRQ_HANDLED;
}

int dsp_interface_open(struct dsp_interface *itf)
{
	int ret;

	dsp_enter();
	ret = devm_request_irq(itf->sys->dev, itf->irq0, dsp_interface_isr0, 0,
			dev_name(itf->sys->dev), itf);
	if (ret) {
		dsp_err("Failed to request irq0(%d)\n", ret);
		goto p_err_req_irq0;
	}

	ret = devm_request_irq(itf->sys->dev, itf->irq1, dsp_interface_isr1, 0,
			dev_name(itf->sys->dev), itf);
	if (ret) {
		dsp_err("Failed to request irq1(%d)\n", ret);
		goto p_err_req_irq1;
	}

	ret = devm_request_irq(itf->sys->dev, itf->irq2, dsp_interface_isr2, 0,
			dev_name(itf->sys->dev), itf);
	if (ret) {
		dsp_err("Failed to request irq2(%d)\n", ret);
		goto p_err_req_irq2;
	}

	ret = devm_request_irq(itf->sys->dev, itf->irq3, dsp_interface_isr3, 0,
			dev_name(itf->sys->dev), itf);
	if (ret) {
		dsp_err("Failed to request irq3(%d)\n", ret);
		goto p_err_req_irq3;
	}

	ret = devm_request_irq(itf->sys->dev, itf->irq4, dsp_interface_isr4, 0,
			dev_name(itf->sys->dev), itf);
	if (ret) {
		dsp_err("Failed to request irq4(%d)\n", ret);
		goto p_err_req_irq4;
	}

	dsp_leave();
	return 0;
p_err_req_irq4:
	devm_free_irq(itf->sys->dev, itf->irq3, itf);
p_err_req_irq3:
	devm_free_irq(itf->sys->dev, itf->irq2, itf);
p_err_req_irq2:
	devm_free_irq(itf->sys->dev, itf->irq1, itf);
p_err_req_irq1:
	devm_free_irq(itf->sys->dev, itf->irq0, itf);
p_err_req_irq0:
	return ret;
}

int dsp_interface_close(struct dsp_interface *itf)
{
	dsp_enter();
	devm_free_irq(itf->sys->dev, itf->irq4, itf);
	devm_free_irq(itf->sys->dev, itf->irq3, itf);
	devm_free_irq(itf->sys->dev, itf->irq2, itf);
	devm_free_irq(itf->sys->dev, itf->irq1, itf);
	devm_free_irq(itf->sys->dev, itf->irq0, itf);
	dsp_leave();
	return 0;
}

int dsp_interface_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_interface *itf;
	struct platform_device *pdev;
	int irq;

	dsp_enter();
	itf = &sys->interface;
	itf->sys = sys;
	itf->sfr = sys->sfr;
	pdev = to_platform_device(sys->dev);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		dsp_err("Failed to get irq0(%d)", ret);
		goto p_err_get_irq0;
	}
	itf->irq0 = irq;

	irq = platform_get_irq(pdev, 1);
	if (irq < 0) {
		ret = irq;
		dsp_err("Failed to get irq1(%d)", ret);
		goto p_err_get_irq1;
	}
	itf->irq1 = irq;

	irq = platform_get_irq(pdev, 2);
	if (irq < 0) {
		ret = irq;
		dsp_err("Failed to get irq2(%d)", ret);
		goto p_err_get_irq2;
	}
	itf->irq2 = irq;

	irq = platform_get_irq(pdev, 3);
	if (irq < 0) {
		ret = irq;
		dsp_err("Failed to get irq3(%d)", ret);
		goto p_err_get_irq3;
	}
	itf->irq3 = irq;

	irq = platform_get_irq(pdev, 4);
	if (irq < 0) {
		ret = irq;
		dsp_err("Failed to get irq4(%d)", ret);
		goto p_err_get_irq4;
	}
	itf->irq4 = irq;

	dsp_leave();
	return 0;
p_err_get_irq4:
p_err_get_irq3:
p_err_get_irq2:
p_err_get_irq1:
p_err_get_irq0:
	return ret;
}

void dsp_interface_remove(struct dsp_interface *itf)
{
	dsp_enter();
	dsp_leave();
}
