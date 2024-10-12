// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts_ipc.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include "vts_ipc.h"
#include "vts_dbg.h"
#include "vts_util.h"
#include "mailbox_api.h"

static struct device *vts_ipc_vts_dev;
static struct vts_data *p_vts_ipc_vts_data;

void  vts_mailbox_update_vts_is_on(bool on)
{
	mailbox_update_vts_is_on(on);
}

/* vts mailbox interface functions */
int vts_mailbox_generate_interrupt(
	const struct platform_device *pdev,
	int hw_irq)
{
	struct vts_data *data = p_vts_ipc_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;
	int result = 0;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		result = -EINVAL;
		goto out;
	}
	result = mailbox_generate_interrupt(pdev, hw_irq);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
	return result;
}

void vts_mailbox_write_shared_register(
	const struct platform_device *pdev,
	const u32 *values,
	int start,
	int count)
{
	struct vts_data *data = p_vts_ipc_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		goto out;
	}
	mailbox_write_shared_register(pdev, values, start, count);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
}

void vts_mailbox_read_shared_register(
	const struct platform_device *pdev,
	u32 *values,
	int start,
	int count)
{
	struct vts_data *data = p_vts_ipc_vts_data;
	struct device *dev = data ? (data->pdev ?
				&data->pdev->dev : NULL) : NULL;
	unsigned long flag;

	if (!data || !dev) {
		vts_dev_warn(dev, "%s: VTS not Initialized\n", __func__);
		return;
	}

	spin_lock_irqsave(&data->state_spinlock, flag);
	/* Check VTS state before accessing mailbox */
	if (data && (data->vts_state == VTS_STATE_RUNTIME_SUSPENDED ||
		data->vts_state == VTS_STATE_VOICECALL ||
		data->vts_state == VTS_STATE_NONE)) {
		vts_dev_warn(dev, "%s: VTS wrong state [%d]\n", __func__,
			data->vts_state);
		goto out;
	}

	mailbox_read_shared_register(pdev, values, start, count);

out:
	spin_unlock_irqrestore(&data->state_spinlock, flag);
}

int vts_ipc_ack(struct device *dev_vts, u32 result)
{
	struct vts_data *data = dev_get_drvdata(dev_vts);

	if (!data->vts_ready)
		return 0;

	pr_debug("%s(%p, %u)\n", __func__, data, result);
	vts_mailbox_write_shared_register(data->pdev_mailbox,
						&result, 0, 1);
	vts_mailbox_generate_interrupt(data->pdev_mailbox,
					VTS_IRQ_AP_IPC_RECEIVED);
	return 0;
}


int vts_irq_register(struct device *dev_vts)
{
	struct device *dev = dev_vts;
	struct vts_data *data = dev_get_drvdata(dev);

	vts_ipc_vts_dev = dev;
	p_vts_ipc_vts_data = data;

	return 0;
}
