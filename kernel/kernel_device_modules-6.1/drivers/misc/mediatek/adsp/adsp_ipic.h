/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef ADSP_IPIC_H
#define ADSP_IPIC_H

#include <linux/platform_device.h>

struct adsp_ipic_mbox_data {
	uint32_t header;
	uint32_t payload[7];
};

struct adsp_ipic_control {
	void __iomem *ipic_chn;
	void __iomem *ipic_src_clr;
	uint32_t ap_up_id;
	uint32_t adsp_up_id;
	uint32_t ap2adsp_chn;
	spinlock_t wakelock;
};

int adsp_ipic_probe(struct platform_device *pdev);
bool adsp_is_ipic_support(void);
int adsp_ipic_send(struct adsp_ipic_mbox_data *mbox_data, bool need_ack);
bool is_adsp_recv_ipic_irq(void);
void ipic_clr_chn(void);

#endif /* ADSP_QOS_H */

