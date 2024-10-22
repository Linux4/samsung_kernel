/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/rbtree.h>

void fbt_ux_frame_start(struct render_info *thr, unsigned long long ts);
void fbt_ux_frame_end(struct render_info *thr,
		unsigned long long start_ts, unsigned long long end_ts);
void fbt_ux_frame_err(struct render_info *thr, unsigned long long ts);
void fpsgo_ux_reset(struct render_info *thr);

struct ux_frame_info {
	unsigned long long frameID;
	unsigned long long start_ts;
	struct rb_node entry;
};

void fpsgo_ux_delete_frame_info(struct render_info *thr, struct ux_frame_info *info);
struct ux_frame_info *fpsgo_ux_search_and_add_frame_info(struct render_info *thr,
		unsigned long long frameID, unsigned long long start_ts, int action);
struct ux_frame_info *fpsgo_ux_get_next_frame_info(struct render_info *thr);
int fpsgo_ux_count_frame_info(struct render_info *thr, int target);

void fpsgo_sbe_rescue(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id);
void fpsgo_sbe_rescue_legacy(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id);

int fpsgo_ctrl2ux_get_perf(void);
void fbt_ux_set_perf(int cur_pid, int cur_blc);

void __exit fbt_cpu_ux_exit(void);
int __init fbt_cpu_ux_init(void);
