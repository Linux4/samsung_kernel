/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_RV_TAG_H__
#define __APUSYS_RV_TAG_H__

#include "apu.h"

/* Tags: Count of Tags */
#define APUSYS_RV_TAGS_CNT (3000)

/* The tag entry of APUSYS_RV */
struct apusys_rv_tag {
	int type;

	union apusys_rv_tag_data {
		struct apusys_rv_tag_ipi_send {
			unsigned int id;
			unsigned int len;
			unsigned int serial_no;
			unsigned int csum;
			unsigned int user_id;
			int usage_cnt;
			uint64_t elapse;
		} ipi_send;
		struct apusys_rv_tag_ipi_handle {
			unsigned int id;
			unsigned int len;
			unsigned int serial_no;
			unsigned int csum;
			unsigned int user_id;
			int usage_cnt;
			uint64_t top_start_time;
			uint64_t bottom_start_time;
			uint64_t latency;
			uint64_t elapse;
			uint64_t t_handler;
		} ipi_handle;
		struct apusys_rv_tag_pwr_ctrl {
			unsigned int id;
			unsigned int on;
			unsigned int off;
			uint64_t latency;
			uint64_t sub_latency_0;
			uint64_t sub_latency_1;
			uint64_t sub_latency_2;
			uint64_t sub_latency_3;
			uint64_t sub_latency_4;
			uint64_t sub_latency_5;
			uint64_t sub_latency_6;
			uint64_t sub_latency_7;
		} pwr_ctrl;
	} d;
};

#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)
int apusys_rv_init_drv_tags(struct mtk_apu *apu);
void apusys_rv_exit_drv_tags(struct mtk_apu *apu);
void apusys_rv_tags_show(struct seq_file *s);
#else
static inline int apusys_rv_init_drv_tags(struct mtk_apu *apu)
{
	return 0;
}

static inline void apusys_rv_exit_drv_tags(struct mtk_apu *apu)
{
}

static inline void apusys_rv_tags_show(struct seq_file *s)
{
}
#endif /* CONFIG_MTK_APUSYS_DEBUG */

#endif /* __APUSYS_RV_TAG_H__ */
