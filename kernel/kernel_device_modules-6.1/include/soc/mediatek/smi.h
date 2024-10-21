/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 */
#ifndef MTK_IOMMU_SMI_H
#define MTK_IOMMU_SMI_H

#include <linux/bitops.h>
#include <linux/device.h>

struct mtk_smi_lock {
	spinlock_t lock;
	unsigned long flags;
};

enum smi_user {
	SMI_MMINFRA = 0,
	SMI_VENC,
	SMI_VDEC,
	SMI_DISP,
	SMI_MML,
	SMI_DIP1,
	SMI_TRAW,
	SMI_USER_NR,
};

extern struct mtk_smi_lock smi_lock;

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)

#define MTK_SMI_MMU_EN(port)	BIT(port)
#define TRIGGER_SMI_HANG_DETECT	(0xff)

struct mtk_smi_larb_iommu {
	struct device *dev;
	unsigned int   mmu;
	unsigned char  bank[64];
};
int mtk_smi_driver_register_notifier(struct notifier_block *nb);
int mtk_smi_driver_unregister_notifier(struct notifier_block *nb);
void mtk_smi_common_bw_set(struct device *dev, const u32 port, const u32 val);
void mtk_smi_common_ostdl_set(struct device *dev, const u32 port, bool is_write, const u32 val);
void mtk_smi_larb_bw_set(struct device *dev, const u32 port, const u32 val);
s32 mtk_smi_dbg_hang_detect(char *user);
void mtk_smi_dbg_hang_detect_force_dump(char *user, u64 larb_skip_id, u64 comm_skip_id);
void mtk_smi_dbg_dump_for_isp_fast(u32 isp_id);
void mtk_smi_dbg_dump_for_disp(void);
void mtk_smi_dbg_dump_for_mml(void);
void mtk_smi_dbg_dump_for_venc(void);
void mtk_smi_dbg_dump_for_vdec(void);
void mtk_smi_dbg_dump_for_mminfra(void);
void mtk_smi_init_power_off(void);
void mtk_smi_dump_last_pd(const char *user);
void mtk_smi_larb_clamp_and_lock(struct device *larbdev, bool on);
s32 smi_sysram_enable(struct device *larbdev, const u32 master_id,
			const bool enable, const char *user);
s32 mtk_smi_sysram_set(struct device *larbdev, const u32 master_id,
			u32 set_val, const char *user);
s32 mtk_smi_dbg_cg_status(void);
void mtk_smi_check_comm_ref_cnt(struct device *dev);
void mtk_smi_check_larb_ref_cnt(struct device *dev);
int mtk_smi_larb_ultra_dis(struct device *larbdev, bool is_dis);
s32 mtk_smi_golden_set(bool enable, bool is_larb, u32 id, u32 port);
#else


static inline int mtk_smi_driver_register_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int mtk_smi_driver_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline void
mtk_smi_common_bw_set(struct device *dev, const u32 port, const u32 val) { }
static inline void
mtk_smi_common_ostdl_set(struct device *dev, const u32 port, bool is_write, const u32 val) { }
static inline void
mtk_smi_larb_bw_set(struct device *dev, const u32 port, const u32 val) { }
static inline s32 mtk_smi_dbg_hang_detect(char *user)
{
	return 0;
}
static inline void
mtk_smi_dbg_hang_detect_force_dump(char *user, u64 larb_skip_id, u64 comm_skip_id) { }
static inline void mtk_smi_dbg_dump_for_isp_fast(u32 isp_id) { }

static inline void mtk_smi_dbg_dump_for_disp(void) { }

static inline void mtk_smi_dbg_dump_for_mml(void) { }

static inline s32 mtk_smi_dbg_cg_status(void)
{
	return 0;
}
static inline  void mtk_smi_check_comm_ref_cnt(struct device *dev) {}
static inline  void mtk_smi_check_larb_ref_cnt(struct device *dev) {}

static inline void mtk_smi_init_power_off(void) { }

static inline void mtk_smi_dump_last_pd(const char *user) { }

static inline void mtk_smi_larb_clamp_and_lock(struct device *larbdev, bool on) { }

static inline
s32 smi_sysram_enable(struct device *larbdev, const u32 master_id,
			const bool enable, const char *user)
{
	return 0;
}

static inline
s32 mtk_smi_sysram_set(struct device *larbdev, const u32 master_id,
			u32 set_val, const char *user)
{
	return 0;
}

static inline
int mtk_smi_larb_ultra_dis(struct device *larbdev, bool is_dis)
{
	return 0;
}

s32 mtk_smi_golden_set(bool enable, bool is_larb, u32 id, u32 port)
{
	return 0;
}
#endif

#endif
