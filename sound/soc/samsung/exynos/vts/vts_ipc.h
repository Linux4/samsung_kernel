/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_ipc.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_IPCSS_H
#define __SND_SOC_VTS_IPCSS_H

//#include <linux/miscdevice.h>
//#include <linux/device.h>
#include "vts.h"

/**
 * Register irq
 * @param[in]	dev		pointer to vts device
 * @return	error code if any
 */
extern int vts_irq_register(
	struct device *dev_vts);

/**
 * ipc ack
 * @param[in]	dev		pointer to vts device
 * @param[out]	ret		result of ipc
 * @return	error code if any
 */
extern int vts_ipc_ack(struct device *dev_vts, u32 ret);

/**
 * write ipc
 * @param[in]	pdev		pointer to platform device
 * @param[in]	values		data
 * @param[in]	start		start position
 * @param[in]	end		end position
 */
extern void vts_mailbox_write_shared_register(
	const struct platform_device *pdev,
	const u32 *values,
	int start,
	int count);

/**
 * write ipc
 * @param[in]	pdev		pointer to platform device
 * @param[in]	values		data
 * @param[in]	start		start position
 * @param[in]	end		end position
 */
extern void vts_mailbox_read_shared_register(
	const struct platform_device *pdev,
	u32 *values,
	int start,
	int count);

/**
 * write ipc
 * @param[in]	pdev		pointer to platform device
 * @param[in]	hw irq		number of irq
 * @return	error code if any
 */
extern int vts_mailbox_generate_interrupt(
	const struct platform_device *pdev,
	int hw_irq);

/**
 * update mailbox state
 * @param[in]	on		update state
 */
extern void vts_mailbox_update_vts_is_on(bool on);

#endif /* __SND_SOC_VTS_IPC_H */
