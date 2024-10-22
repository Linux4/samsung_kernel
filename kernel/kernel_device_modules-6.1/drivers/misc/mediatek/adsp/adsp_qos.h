/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef ADSP_QOS_H
#define ADSP_QOS_H

#include <linux/platform_device.h>
#include <linux/interconnect.h>

struct adsp_qos_scene_info {
	uint16_t bw_mbps;
	char *name;
};

struct adsp_qos_control {
	void __iomem *cfg_hrt;
	uint32_t hrt_bits;
	struct icc_path *icc_hrt_path;
	struct mutex lock;
	uint64_t adsp_qos_scene_enable;
	uint64_t adsp_qos_scene_status;
	uint32_t cur_bw_mbps;
};

int adsp_qos_probe(struct platform_device *pdev);
void adsp_set_scene_bw(struct platform_device *pdev);
void set_adsp_icc_bw(uint32_t bw_mbps);
void clear_adsp_icc_bw(void);
int adsp_icc_bw_req(uint16_t scene, uint16_t set);

#endif /* ADSP_QOS_H */

