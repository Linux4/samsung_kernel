/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DVFSRC_MB_H__
#define __DVFSRC_MB_H__

#define MAX_DATA_SIZE 6

struct mtk_dvfsrc_header{
	uint8_t module_id;
	uint8_t version;
	uint16_t data_offset;
	uint32_t data_length;
	uint32_t data[MAX_DATA_SIZE];
};

struct mtk_dvfsrc_config {
	void (*get_data)(struct mtk_dvfsrc_header *header);
};

struct mtk_dvfsrc_data {
	uint8_t module_id;
	uint8_t version;
	uint16_t data_offset;
	uint32_t data_length;
	const struct mtk_dvfsrc_config *config;
};

struct mtk_dvfsrc_mb {
	struct device *dev;
	void __iomem *regs;
	const struct mtk_dvfsrc_data *dvd;
};

#define DVFSRC_RSV_0        (0x788)
#define DVFSRC_RSV_1        (0x78C)
#define DVFSRC_RSV_2        (0x790)
#define DVFSRC_RSV_3        (0x794)
#define DVFSRC_RSV_4        (0x798)
#define DVFSRC_RSV_5        (0x79C)
#define DVFSRC_RSV_6        (0x7A0)

void dvfsrc_get_data(struct mtk_dvfsrc_header *header);

#endif /*__DVFSRC_MB_H__*/
