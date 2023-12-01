/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Stage 2 Protection Unit(S2MPU)
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_S2MPU_H__
#define __EXYNOS_S2MPU_H__

#define EXYNOS_MAX_S2MPU_INSTANCE		(64)
#define EXYNOS_MAX_SUBSYSTEM			(32)
#define EXYNOS_MAX_SUBSYSTEM_NAME_LEN		(16)

#define SUBSYSTEM_FW_NUM_SHIFT			(32)
#define SUBSYSTEM_INDEX_SHIFT			(0)

/* S2MPU version */
#define S2MPU_V1_2				(0x12)
#define S2MPU_V2_0				(0x20)
#define S2MPU_V9_0				(0x90)

/* Error */
#define ERROR_INVALID_SUBSYSTEM_NAME		(0x1)
#define ERROR_DO_NOT_SUPPORT_SUBSYSTEM		(0x2)
#define ERROR_INVALID_FW_BIN_INFO		(0x3)
#define ERROR_INVALID_NOTIFIER_BLOCK		(0x4)
#define ERROR_INVALID_PARAMETER			(0x5)
#define ERROR_S2MPU_NOT_INITIALIZED		(0xE12E0001)

/* Backup and restore S2MPU */
#define EXYNOS_PD_S2MPU_BACKUP			(0)
#define EXYNOS_PD_S2MPU_RESTORE			(1)

/* S2MPU fault */
#define MAX_VID_OF_S2MPU_FAULT_INFO		(8)

#define FAULT_TYPE_MPTW_ACCESS_FAULT		(0x1)
#define FAULT_TYPE_AP_FAULT			(0x2)
#define FAULT_TYPE_CONTEXT_FAULT		(0x4)
#define FAULT_THIS_ADDR_IS_NOT_BLOCKLIST	(0xFFFF)

#define S2MPU_CONTEXT_VALID_SHIFT		(3)
#define S2MPU_CONTEXT_VID_SHIFT			(0)
#define S2MPU_CONTEXT_VALID_MASK		(0x1)
#define S2MPU_CONTEXT_VID_MASK			(0x7)

#define OWNER_IS_KERNEL_RO			(99)
#define OWNER_IS_TEST				(98)

#define ERROR_NOTHING_S2MPU_FAULT		(0x2)

/* S2MPU Notifier */
#define S2MPU_NOTIFIER_SET			(1)
#define S2MPU_NOTIFIER_UNSET			(0)

#define S2MPU_NOTIFIER_PANIC			(0xFF00000000)
#define S2MPU_NOTIFY_OK				(0)
#define S2MPU_NOTIFY_BAD			(1)

/* S2MPU enable check */
#define S2_BUF_SIZE 				(30)

/* S2MPU TLB(PTLB/STLB/MPTC) */
/* v1.2/v2.0 */
#define S2MPU_NUM_OF_MPTC_WAY			(4)

#define S2MPU_MAX_NUM_OF_FAULT_MPTC		(2)

/* v9.0 */
#define S2MPU_MAX_NUM_TLB_WAY			(24)
#define S2MPU_NUM_OF_STLB_SUBLINE		(4)

#define S2MPU_TLB_TYPE_NUM			(3)
#define S2MPU_FAULT_PTLB_INDEX			(0)
#define S2MPU_FAULT_STLB_INDEX			(1)
#define S2MPU_FAULT_MPTC_INDEX			(2)

#define S2MPU_PTLB_STAGE1_ENABLED_ENTRY		(0x1)
#define S2MPU_STLB_STAGE1_ENABLED_ENTRY		(0x1)
#define S2MPU_STLB_S2VALID_S2_DESC_UPDATED	(0x1)

/* For backward compatibility */
#define exynos_verify_subsystem_fw		exynos_s2mpu_verify_subsystem_fw
#define exynos_request_fw_stage2_ap		exynos_s2mpu_request_fw_stage2_ap

/* PM QoS for SSS */
#define PM_QOS_SSS_UPDATE			(true)
#define PM_QOS_SSS_RELEASE			(false)

#define PM_QOS_SSS_FREQ_DOMAIN_LEN		(8)

#ifndef __ASSEMBLY__

#include <linux/mutex.h>

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/exynos/debug-snapshot.h>
#endif

#include <soc/samsung/exynos_pm_qos.h>

/* S2MPU access permission */
enum stage2_ap {
	ATTR_NO_ACCESS = 0x0,
	ATTR_RO = 0x1,
	ATTR_WO = 0x2,
	ATTR_RW = 0x3
};

/* S2MPU fault information */

/*******************************
 * S2MPU v1.2: MPTC
 * S2MPU v2.0: PTLB and STLB
 *******************************
 */
struct s2mpu_mptc {
	unsigned int valid;
	unsigned int ppn;
	unsigned int vid;
	unsigned int granule;
	unsigned int ap;
};

struct s2mpu_fault_mptc {
	unsigned int set;
	struct s2mpu_mptc mptc[S2MPU_NUM_OF_MPTC_WAY];
};

struct __s2mpu_fault_info {
	unsigned int fault_addr_low;
	unsigned int fault_addr_high;
	unsigned int vid;
	unsigned int type;
	unsigned int rw;
	unsigned int len;
	unsigned int axid;
	unsigned int context;
	unsigned int subsystem;
	unsigned int blocklist_owner;
	struct s2mpu_fault_mptc fault_mptc[S2MPU_MAX_NUM_OF_FAULT_MPTC];
};

struct s2mpu_fault_info {
	unsigned int fault_cnt;
	struct __s2mpu_fault_info info[MAX_VID_OF_S2MPU_FAULT_INFO];
};

/*******************************
 * S2MPU v9.0
 *
 * 3 TLBs: PTLB, STLB and MPTC
 *******************************
 */
/*
 * S2MPU instance type
 *
 * @S2MPU_A_TYPE : S1 + S2
 * @S2MPU_B_TYPE : S2 only
 */
enum s2mpu_type {
	S2MPU_A_TYPE,
	S2MPU_B_TYPE,
	S2MPU_TYPE_MAX
};

struct s2mpu_fault_map_info {
	unsigned long sfr_l2table_addr;
	unsigned long map_l2table_addr;
	unsigned int l2table_offset;
	unsigned int sfr_ap_val;
	unsigned int map_ap_val;
};

struct s2mpu_tlb {
	unsigned int valid;
	unsigned int ppn;
	unsigned int vid;
	unsigned int page_size;
	unsigned int ap;
	unsigned int tpn;
	unsigned int base_page_size;
	unsigned int stage1;
	unsigned int s2valid;
};

struct s2mpu_fault_tlb {
	unsigned int set;
	unsigned int way_num;
	struct s2mpu_tlb tlb[S2MPU_MAX_NUM_TLB_WAY];
};

struct s2mpu_fault_regs {
	unsigned int fault_pa_low;
	unsigned int fault_pa_high;
	unsigned int fault_info0;
	unsigned int fault_info1;
	unsigned int fault_info2;
};

struct __s2mpu_fault_info_v9 {
	unsigned int fault_addr_low;
	unsigned int fault_addr_high;
	unsigned int vid;
	unsigned int type;
	unsigned int rw;
	unsigned int len;
	unsigned int axid;
	unsigned int pmmu_id;
	unsigned int stream_id;
	unsigned int context;
	unsigned int subsystem;
	unsigned int blocklist_owner;
	struct s2mpu_fault_regs fault_regs;
	struct s2mpu_fault_tlb fault_tlb[S2MPU_TLB_TYPE_NUM];
	struct s2mpu_fault_map_info fault_map_info;
};

struct s2mpu_fault_info_v9 {
	unsigned int fault_cnt;
	enum s2mpu_type s2mpu_type;
	struct __s2mpu_fault_info_v9 info[MAX_VID_OF_S2MPU_FAULT_INFO];
};

/*******************************
 * S2MPU common
 *******************************
 */
struct s2mpu_fault_info_common {
	unsigned int s2mpu_type;
	unsigned int addr_low;
	unsigned long addr_high;
	unsigned int vid;
	unsigned int type;
	unsigned int rw;
	unsigned int len;
	unsigned int axid;
	unsigned int pmmu_id;
	unsigned int stream_id;
	unsigned int context;
	unsigned int subsystem_index;
	unsigned int blocklist_owner;
	void *fault_tlb;
	unsigned long sfr_l2table_addr;
	unsigned long map_l2table_addr;
	unsigned int l2table_offset;
	unsigned int sfr_ap_val;
	unsigned int map_ap_val;
};

/* Structure of notifier info */
struct s2mpu_notifier_info {
	const char *subsystem;
	unsigned long fault_addr;
	unsigned int fault_rw;
	unsigned int fault_len;
	unsigned int fault_type;
};

/* Structure of notifier block */
struct s2mpu_notifier_block {
	const char *subsystem;
	int (*notifier_call)(struct s2mpu_notifier_block *,
			     struct s2mpu_notifier_info *);
};

/* Data structure for S2MPU Fault Information */
struct s2mpu_info_data {
	struct device *dev;
	void *fault_info;
	dma_addr_t fault_info_pa;
	size_t fault_info_size;
	unsigned int instance_num;
	unsigned int irq[EXYNOS_MAX_S2MPU_INSTANCE];
	unsigned int irqcnt;
	struct s2mpu_notifier_info *noti_info;
	unsigned int *notifier_flag;
	unsigned int hw_version;
};

/* PM QoS for SSS */
struct s2mpu_pm_qos_sss {
	struct exynos_pm_qos_request qos_sss;
	unsigned int sss_freq_domain;
	unsigned int qos_sss_freq;
	unsigned int qos_count;
	struct mutex qos_count_lock;
	bool need_qos_sss;
};


unsigned long exynos_s2mpu_set_stage2_ap(const char *subsystem,
					 unsigned long base,
					 unsigned long size,
					 unsigned int ap);
unsigned long exynos_s2mpu_set_blocklist(const char *subsystem,
					 unsigned long base,
					 unsigned long size);
unsigned long exynos_s2mpu_set_all_blocklist(unsigned long base,
					     unsigned long size);

/**
 * exynos_s2mpu_verify_subsystem_fw - Verify the signature of sub-system FW.
 *
 * @subsystem:	 Sub-system name.
 *		 Please refer to subsystem-names property of s2mpu node
 *		 in exynosXXXX-security.dtsi.
 * @fw_id:	 FW ID of this subsystem.
 * @fw_base:	 FW base address.
 *		 It's physical address(PA) and should be aligned with 64KB
 *		 because of S2MPU granule.
 * @fw_bin_size: FW binary size.
 *		 It should be aligned with 4bytes because of the limit of
 *		 signature verification.
 * @fw_mem_size: The size to be used by FW.
 *		 This memory region needs to be protected from other
 *		 sub-systems. It should be aligned with 64KB like fw_base
 *		 because of S2MPU granlue.
 */
unsigned long exynos_s2mpu_verify_subsystem_fw(const char *subsystem,
					       unsigned int fw_id,
					       unsigned long fw_base,
					       size_t fw_bin_size,
					       size_t fw_mem_size);

/**
 * exynos_s2mpu_request_fw_stage2_ap - Request Stage 2 access permission
 *				       of FW to allow access memory.
 *
 * @subsystem: Sub-system name.
 *	       Please refer to subsystem-names property of s2mpu node
 *	       in exynosXXXX-security.dtsi.
 *
 * This function must be called in case that only sub-system FW is
 * verified.
 */
unsigned long exynos_s2mpu_request_fw_stage2_ap(const char *subsystem);

/**
 * exynos_s2mpu_release_fw_stage2_ap - Release Stage 2 access permission
 *				       for sub-system FW region and block
 *				       all Stage 2 access permission of
 *				       the sub-system.
 *
 * @subsystem:	 Sub-system name
 *		 Please refer to subsystem-names property of
 *		 s2mpu node in exynosXXXX-security.dtsi.
 * @fw_id:	 FW ID of this subsystem
 *
 * This function must be called in case that only sub-system FW is verified.
 */
unsigned long exynos_s2mpu_release_fw_stage2_ap(const char *subsystem,
						uint32_t fw_id);

/**
 * exynos_s2mpu_notifier_call_register - Register subsystem's S2MPU notifier call.
 *
 * @s2mpu_notifier_block:	S2MPU Notifier structure
 *				It should have two elements.
 *				One is Sub-system's name, 'subsystem'.
 *				The other is notifier call function pointer,
 *				s2mpu_notifier_fn_t 'notifier_call'.
 * @'subsystem':		Please refer to subsystem-names property of s2mpu
 *				node in exynosXXXX-security.dtsi.
 * @'notifier_call'		It's type is s2mpu_notifier_fn_t.
 *				And it should return S2MPU_NOTIFY
 */
unsigned long exynos_s2mpu_notifier_call_register(struct s2mpu_notifier_block *snb);

#ifdef CONFIG_EXYNOS_S2MPU_PD
unsigned long exynos_pd_backup_s2mpu(unsigned int pd);
unsigned long exynos_pd_restore_s2mpu(unsigned int pd);
#else
static inline unsigned long exynos_pd_backup_s2mpu(unsigned int pd)
{
	return 0;
}

static inline unsigned long exynos_pd_restore_s2mpu(unsigned int pd)
{
	return 0;
}
#endif

static inline void exynos_s2mpu_halt(void)
{
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	if (dbg_snapshot_expire_watchdog())
		pr_err("WDT reset fails\n");
#endif
	BUG();
}

#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_S2MPU_H__ */
