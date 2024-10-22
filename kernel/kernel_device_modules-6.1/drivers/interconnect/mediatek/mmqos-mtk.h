/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */
#ifndef MMQOS_MTK_H
#define MMQOS_MTK_H
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect-provider.h"
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <soc/mediatek/mmqos.h>
#include <linux/kernel.h>

#define MMQOS_NO_LINK			(0xffffffff)
#define MMQOS_MAX_COMM_NUM		(3)
#define MMQOS_MAX_COMM_PORT_NUM		(10)
#define MMQOS_COMM_CHANNEL_NUM		(2)
#define MMQOS_MAX_DUAL_PIPE_LARB_NUM	(2)
#define MMQOS_MAX_REPORT_LARB_NUM	(13)
#define MMQOS_MAX_DISP_VIRT_LARB_NUM	(3)
#define MMQOS_MAX_DVFS_STEP_NUM		(11)
#define MMQOS_DVFS_VALUE_NUM		(2)

#define RECORD_NUM		(10)

enum {
	MD_SCEN_NONE,
	MD_SCEN_SUB6_EXT,
};

struct hrt_record {
	u8 idx;
	u64 time[RECORD_NUM];
	u32 avail_hrt[RECORD_NUM];
	u32 cam_max_hrt[RECORD_NUM];
	u32 cam_hrt[RECORD_NUM];
	struct mutex lock;
};

struct cam_hrt_record {
	u8 idx;
	u64 time[RECORD_NUM];
	u32 cam_max_hrt[RECORD_NUM];
};

struct mmqos_hrt {
	u32 hrt_bw[HRT_TYPE_NUM];
	u32 hrt_ratio[HRT_TYPE_NUM];
	u32 hrt_total_bw;
	u32 cam_max_bw;
	u32 cam_occu_bw;
	u32 md_speech_bw[4];
	u32 emi_ratio;
	bool blocking;
	bool cam_bw_inc;
	struct delayed_work work;
	struct blocking_notifier_head hrt_bw_throttle_notifier;
	atomic_t lock_count;
	wait_queue_head_t hrt_wait;
	struct mutex blocking_lock;
	bool in_speech;
	u8 md_type;
	u8 md_scen;
	struct hrt_record hrt_rec;
	struct cam_hrt_record cam_hrt_rec;
};

struct mmqos_base_node {
	struct icc_node *icc_node;
	u32	mix_bw;
};

struct common_node {
	struct mmqos_base_node *base;
	struct device *comm_dev;
	struct regulator *comm_reg;
	const char *clk_name;
	struct clk *clk;
	u64 freq;
	u64 smi_clk;
	u32 volt;
	struct list_head list;
	struct icc_path *icc_path;
	struct icc_path *icc_hrt_path;
	struct work_struct work;
	struct list_head comm_port_list;
};

struct common_port_node {
	struct mmqos_base_node *base;
	struct common_node *common;
	struct device *larb_dev;
	struct mutex bw_lock;
	u32 latest_mix_bw;
	u64 latest_peak_bw;
	u32 latest_avg_bw;
	u32 old_avg_w_bw;
	u32 old_avg_r_bw;
	u32 old_peak_w_bw;
	u32 old_peak_r_bw;
	struct list_head list;
	u8 channel;
	u8 channel_v2;
	u8 hrt_type;
	u32 write_peak_bw;
	u32 write_avg_bw;
};

struct larb_node {
	struct mmqos_base_node *base;
	struct device *larb_dev;
	struct work_struct work;
	struct icc_path *icc_path;
	u32 old_avg_bw;
	u32 old_peak_bw;
	u8 channel;
	u8 channel_v2;
	u8 dual_pipe_id;
	u16 bw_ratio;
	bool is_write;
	bool is_report_bw_larbs;
};

struct mtk_node_desc {
	const char *name;
	u32 id;
	u32 link;
	u16 bw_ratio;
	u8 channel;
	bool is_write;
};

struct mtk_mmqos_desc {
	const struct mtk_node_desc *nodes;
	const size_t num_nodes;
	const char * const *comm_muxes;
	const char * const *comm_icc_path_names;
	const char * const *comm_icc_hrt_path_names;
	const char * const *larb_icc_path_names;
	const u32 max_ratio;
	const u32 max_disp_ostdl;
	const struct mmqos_hrt hrt;
	const struct mmqos_hrt hrt_LPDDR4;
	const u32 dual_pipe_larbs[MMQOS_MAX_DUAL_PIPE_LARB_NUM];
	const u8 comm_port_channels[MMQOS_MAX_COMM_NUM][MMQOS_MAX_COMM_PORT_NUM];
	const u8 comm_port_hrt_types[MMQOS_MAX_COMM_NUM][MMQOS_MAX_COMM_PORT_NUM];
	const u8 md_scen;
	const u32 mmqos_state;
	const u32 report_bw_larbs[MMQOS_MAX_REPORT_LARB_NUM];
	const u32 report_bw_real_larbs[MMQOS_MAX_REPORT_LARB_NUM];
	const u32 disp_virt_larbs[MMQOS_MAX_DISP_VIRT_LARB_NUM];
	const u32 freq_mode;
};

#define DEFINE_MNODE(_name, _id, _bw_ratio, _is_write, _channel, _link) {	\
	.name = #_name,	\
	.id = _id,	\
	.bw_ratio = _bw_ratio,	\
	.is_write = _is_write,	\
	.channel = _channel,	\
	.link = _link,	\
	}
int mtk_mmqos_probe(struct platform_device *pdev);
int mtk_mmqos_v2_probe(struct platform_device *pdev);
int mtk_mmqos_remove(struct platform_device *pdev);
/* For HRT */
void mtk_mmqos_init_hrt(struct mmqos_hrt *hrt);
int mtk_mmqos_register_hrt_sysfs(struct device *dev);
void mtk_mmqos_unregister_hrt_sysfs(struct device *dev);

enum MMQOS_PROFILE_LEVEL {
	MMQOS_PROFILE_MET = 0,
	MMQOS_PROFILE_SYSTRACE = 1,
	MMQOS_PROFILE_MAX /* Always keep at the end */
};

enum mminfra_freq_mode {
	BY_REGULATOR,
	BY_MMDVFS,
	BY_VMMRC,
};

/* For Channel BW */
void update_channel_bw(const u32 comm_id, const u32 chnn_id,
	struct icc_node *src);
void clear_chnn_bw(void);
void check_disp_chnn_bw(int i, int j, const int *ans);
void check_chnn_bw(int i, int j, int srt_r, int srt_w, int hrt_r, int hrt_w);
struct common_port_node *create_fake_comm_port_node(int hrt_type,
	int srt_r, int srt_w, int hrt_r, int hrt_w);
void set_mmqos_state(const u32 new_state);
int get_mmqos_state(void);

/* For MET */
bool mmqos_met_enabled(void);

/* For systrace */
bool mmqos_systrace_enabled(void);
int tracing_mark_write(char *fmt, ...);

#define TRACE_MSG_LEN	1024

#define MMQOS_TRACE_FORCE_BEGIN_TID(tid, fmt, args...) \
	tracing_mark_write("B|%d|" fmt "\n", tid, ##args)

#define MMQOS_TRACE_FORCE_BEGIN(fmt, args...) \
	MMQOS_TRACE_FORCE_BEGIN_TID(current->tgid, fmt, ##args)

#define MMQOS_TRACE_FORCE_END() \
	tracing_mark_write("E\n")

#define MMQOS_SYSTRACE_BEGIN(fmt, args...) do { \
	if (mmqos_systrace_enabled()) { \
		MMQOS_TRACE_FORCE_BEGIN(fmt, ##args); \
	} \
} while (0)

#define MMQOS_SYSTRACE_END() do { \
	if (mmqos_systrace_enabled()) { \
		MMQOS_TRACE_FORCE_END(); \
	} \
} while (0)
#endif /* MMQOS_MTK_H */
