/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos CAMERA-PP VOTF driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_VOTF_H
#define CAMERAPP_VOTF_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/smc.h>
#include <media/v4l2-ioctl.h>
#include <linux/iommu.h>
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#include <kunit/test.h>
#endif

#include "pablo-hw-api-votf.h"
#include "pablo-votf-common-enum.h"

#define votf_err(fmt, args...) \
	pr_err("[VOTF]" fmt "\n", ##args)
#define votf_warn(fmt, args...) \
	pr_warn("[VOTF]" fmt "\n", ##args)
#define votf_info(fmt, args...) \
	pr_info("[VOTF]" fmt "\n", ##args)
#define votf_dbg(level, fmt, args...) \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_HW)) >= (level), "[VOTF]", fmt, ##args)

#define votfd_err(fmt, dev, args...) \
	votf_err("[D%d]%s: " fmt, dev->id, __func__, ##args)
#define votfd_warn(fmt, dev, args...) \
	votf_warn("[D%d]%s: " fmt, dev->id, __func__, ##args)
#define votfd_info(fmt, dev, args...) \
	votf_info("[D%d]%s: " fmt, dev->id, __func__, ##args)

#define votfi_err(fmt, vinfo, args...) \
	votf_err("[IP0x%x]%s: " fmt, vinfo->ip, __func__, ##args)
#define votfi_warn(fmt, vinfo, args...) \
	votf_warn("[IP0x%x]%s: " fmt, vinfo->ip, __func__, ##args)
#define votfi_info(fmt, vinfo, args...) \
	votf_info("[IP0x%x]%s: " fmt, vinfo->ip, __func__, ##args)

#define votfdi_err(fmt, dev, vinfo, args...) \
	votf_err("[D%d][IP0x%x]%s: " fmt, dev->id, vinfo->ip, __func__, ##args)
#define votfdi_warn(fmt, dev, vinfo, args...) \
	votf_warn("[D%d][IP0x%x]%s: " fmt, dev->id, vinfo->ip, __func__, ##args)
#define votfdi_info(fmt, dev, vinfo, args...) \
	votf_info("[D%d][IP0x%x]%s: " fmt, dev->id, vinfo->ip, __func__, ##args)

#define VOTF_OPTION_BIT_CHANGE	(0)
#define VOTF_OPTION_BIT_COUNT	(1)
#define VOTF_OPTION_MSK_CHANGE	(0x1 << VOTF_OPTION_BIT_CHANGE)
#define VOTF_OPTION_MSK_COUNT	(0x1 << VOTF_OPTION_BIT_COUNT)

#define VOTF_DEV_NUM	3

struct votf_info {
	int service;
	u32 ip;
	int id;
	u32 mode;
};

struct votf_selreg_cfg {
	u32 set;			/* setA, setB */
	u32 mode;			/* 0 : Shadow Register, 1 : immediately settings */
};

struct votf_lost_cfg {
	bool recover_enable;
	bool flush_enable;
};

struct votf_module_type_addr {
	u32 tws_addr;
	u32 tws_gap;
	u32 trs_addr;
	u32 trs_gap;
};

struct votf_service_cfg {
	bool enable;
	u32 limit;			/* (line unit) */
	u32 token_size;			/* token size(line unit) */
	u32 width;			/* frame width(pixel unit) */
	u32 height;			/* frame height(pixel unit) */
	u32 bitwidth;			/* 1pixel memory bitwidth */
	u32 connected_ip;		/* other side ip, for dest reg */
	u32 connected_id;		/* other side id, for dest reg*/
	u32 option;			/* 1st bit: IP/ID change */
	u32 reserved[9];
};

struct votf_debug_info {
	u32 debug[2];			/* [0]:C2COM_DEBUG, [1]:C2COM_DEBUG_DOUT */
	unsigned long long time;
};

struct votf_table_info {
	u32 use;
	u32 addr;
	u32 ip;
	int id;
	int service;			/* 0:Mater(TWS), 1:Slave(TRS) */
	int module;			/* C2SERV, C2AGENT */
	int module_type;		/* M16S16, M1S3, M2S3.. */
	struct votf_debug_info debug_info;
};

struct votf_dev {
	u32				id;
	bool				use_axi;
	int				ring_request;
	atomic_t			ip_enable_cnt[IP_MAX];
	atomic_t			id_enable_cnt[IP_MAX][ID_MAX];
	int				ring_pair[SERVICE_CNT][IP_MAX][ID_MAX];		/* to check 1:1 mapping */
	void __iomem			*votf_addr[IP_MAX];
	struct device			*dev;
	spinlock_t			votf_slock;
	struct votf_module_type_addr	votf_module_addr[MODULE_TYPE_CNT];
	struct votf_service_cfg		votf_cfg[SERVICE_CNT][IP_MAX][ID_MAX];
	struct votf_table_info		votf_table[SERVICE_CNT][IP_MAX][ID_MAX];
};

/* TODO: deprecate CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF */
#if IS_ENABLED(CONFIG_VIDEO_EXYNOS_PABLO_LIB_VOTF) || IS_ENABLED(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF)
/* common function */
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct votf_dev *get_votf_dev(u32 dev_id);
void set_votf_dev(u32 dev_id, struct votf_dev *dev);
#endif
void votfitf_init(void);
int votfitf_create_ring(void);
int votfitf_destroy_ring(void);
int votfitf_set_service_cfg(struct votf_info *vinfo, struct votf_service_cfg *cfg);
int votfitf_reset(struct votf_info *vinfo, int type);	/* type(0) : sw_core_reset, type(1) : sw_reset */
void votfitf_disable_service(void);
int votfitf_create_link(int src_ip, int dst_ip);
int votfitf_destroy_link(int src_ip, int dst_ip);

int votfitf_set_flush(struct votf_info *vinfo);
int votfitf_set_crop_start(struct votf_info *vinfo, bool start);
int votfitf_get_crop_start(struct votf_info *vinfo);
int votfitf_set_crop_enable(struct votf_info *vinfo, bool enable);
int votfitf_get_crop_enable(struct votf_info *vinfo);
bool votfitf_check_votf_ring(void __iomem *base_addr, int module);
void votfitf_votf_create_ring(void __iomem *base_addr, int ip, int module);
void votfitf_votf_set_sel_reg(void __iomem *base_addr, u32 set, u32 mode);
u32 votfitf_wrapper_reset(void __iomem *base_addr);
int votfitf_check_wait_con(struct votf_info *s_vinfo, struct votf_info *d_vinfo);
int votfitf_check_invalid_state(struct votf_info *s_vinfo, struct votf_info *d_vinfo);

/* C2SERV function */
int votfitf_set_trs_lost_cfg(struct votf_info *vinfo, struct votf_lost_cfg *cfg);
int votfitf_set_first_token_size(struct votf_info *vinfo, u32 size);
int votfitf_set_token_size(struct votf_info *vinfo, u32 size);
int votfitf_set_frame_size(struct votf_info *vinfo, u32 size);
u32 votfitf_get_busy(struct votf_info *vinfo);

/* local function */
void votf_sfr_dump(void);
int votf_set_mcsc_hwfc(struct votf_info *vinfo, u32 output_id);
struct votf_dev *get_votf_dev_by_ip_kunit_wrapper(struct votf_info *vinfo);
inline int search_ip_idx_kunit_wrapper(struct votf_info *vinfo);

#else
/* common function */
static inline void votfitf_init(void) {return;}
static inline int votfitf_create_ring(void) {return 0;}
static inline int votfitf_destroy_ring(void) {return 0;}
static inline int votfitf_set_service_cfg(struct votf_info *vinfo, struct votf_service_cfg *cfg) {return 0;}
static inline int votfitf_reset(struct votf_info *vinfo, int type) {return 0;}
static inline void votfitf_disable_service(void) {return;};
static inline int votfitf_create_link(int src_ip, int dst_ip) {return 0;};
static inline int votfitf_destroy_link(int src_ip, int dst_ip) {return 0;};

static inline int votfitf_set_flush(struct votf_info *vinfo) {return 0;}
static inline int votfitf_set_crop_start(struct votf_info *vinfo, bool start) {return 0;}
static inline int votfitf_get_crop_start(struct votf_info *vinfo) {return 0;}
static inline int votfitf_set_crop_enable(struct votf_info *vinfo, bool enable) {return 0;}
static inline int votfitf_get_crop_enable(struct votf_info *vinfo) {return 0;}
static inline bool votfitf_check_votf_ring(void __iomem *base_addr, int module) {return 0;}
static inline void votfitf_votf_create_ring(void __iomem *base_addr, int ip, int module) {return;}
static inline void votfitf_votf_set_sel_reg(void __iomem *base_addr, u32 set, u32 mode) {return;}
static inline u32 votfitf_wrapper_reset(void __iomem *base_addr) {return 0;}
static inline int votfitf_check_wait_con(struct votf_info *s_vinfo, struct votf_info *d_vinfo) {return 0;}
static inline int votfitf_check_invalid_state(struct votf_info *s_vinfo, struct votf_info *d_vinfo) {return 0;}

/* C2SERV function */
static inline int votfitf_set_trs_lost_cfg(struct votf_info *vinfo, struct votf_lost_cfg *cfg) {return 0;}
static inline int votfitf_set_first_token_size(struct votf_info *vinfo, u32 size) {return 0;}
static inline int votfitf_set_token_size(struct votf_info *vinfo, u32 size) {return 0;}
static inline int votfitf_set_frame_size(struct votf_info *vinfo, u32 size) {return 0;}
static inline u32 votfitf_get_busy(struct votf_info *vinfo) {return 0;}

/* local function */
static inline void votf_sfr_dump(void) {return;}
#endif

#endif /* CAMERAPP_VOTF__H */
