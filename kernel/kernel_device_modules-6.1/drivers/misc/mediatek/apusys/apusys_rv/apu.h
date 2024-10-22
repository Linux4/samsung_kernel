/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_H
#define APU_H
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include "apu_ipi.h"
#include "apu_config.h"
#include "apu_mbox.h"

struct mtk_apu;

struct mtk_apu_hw_ops {
	int (*init)(struct mtk_apu *apu);
	int (*exit)(struct mtk_apu *apu);
	int (*start)(struct mtk_apu *apu);
	int (*stop)(struct mtk_apu *apu);
	int (*apu_memmap_init)(struct mtk_apu *apu);
	void (*apu_memmap_remove)(struct mtk_apu *apu);
	void (*cg_gating)(struct mtk_apu *apu);
	void (*cg_ungating)(struct mtk_apu *apu);
	void (*rv_cachedump)(struct mtk_apu *apu);

	/* power related ops */
	int (*power_init)(struct mtk_apu *apu);
	int (*power_on)(struct mtk_apu *apu);
	int (*power_off)(struct mtk_apu *apu);
	/* for fast on/off, only exist if F_FAST_ON_OFF set */
	int (*power_on_off)(struct mtk_apu *apu, u32 id, u32 on, u32 off);
	void (*wake_lock)(struct mtk_apu *apu, uint32_t id);
	void (*wake_unlock)(struct mtk_apu *apu, uint32_t id);
	void (*debug_info_dump)(struct mtk_apu *apu, struct seq_file *s);
	int (*polling_rpc_status)(struct mtk_apu *apu, u32 pwr_stat, u32 timeout);

	/* ipi related ops */
	int (*ipi_send_pre)(struct mtk_apu *apu, uint32_t id, bool is_host_initiated);
	int (*ipi_send_post)(struct mtk_apu *apu);

	/* irq affinity tuning */
	int (*irq_affin_init)(struct mtk_apu *apu);
	int (*irq_affin_set)(struct mtk_apu *apu);
	int (*irq_affin_unset)(struct mtk_apu *apu);
	int (*irq_affin_clear)(struct mtk_apu *apu);

	/* check apu exception irq */
	int (*check_apu_exp_irq)(struct mtk_apu *apu, char *ce_module);

};

#define F_PRELOAD_FIRMWARE		BIT(0)
#define F_AUTO_BOOT				BIT(1)
#define F_BYPASS_IOMMU			BIT(2)
#define F_SECURE_BOOT			BIT(3)
#define F_SECURE_COREDUMP		BIT(4)
#define F_DEBUG_LOG_ON			BIT(5)
#define F_CE_EXCEPTION_ON		BIT(6)
#define F_FAST_ON_OFF			BIT(7)
#define F_APU_IPI_UT_SUPPORT	BIT(8)
#define F_APUSYS_RV_TAG_SUPPORT	BIT(9)
#define F_SMMU_SUPPORT			BIT(10)
#define F_BRINGUP				BIT(11)
#define F_FPGA_EP				BIT(12)
#define F_ACCESS_RCX_IN_ATF		BIT(13)
#define F_TCM_WA				BIT(14)
#define F_EXCEPTION_KE			BIT(15)

/* #define APUSYS_RV_FPGA_EP */

#define MAX_PWR_SUB_LATENCY (8)

struct mtk_apu_platdata {
	uint32_t flags;
	struct mtk_apu_hw_ops ops;
};

struct apusys_secure_info_t {
	unsigned int total_sz;
	unsigned int up_code_buf_ofs;
	unsigned int up_code_buf_sz;

	unsigned int up_fw_ofs;
	unsigned int up_fw_sz;
	unsigned int up_xfile_ofs;
	unsigned int up_xfile_sz;
	unsigned int mdla_fw_boot_ofs;
	unsigned int mdla_fw_boot_sz;
	unsigned int mdla_fw_main_ofs;
	unsigned int mdla_fw_main_sz;
	unsigned int mdla_xfile_ofs;
	unsigned int mdla_xfile_sz;
	unsigned int mvpu_fw_ofs;
	unsigned int mvpu_fw_sz;
	unsigned int mvpu_xfile_ofs;
	unsigned int mvpu_xfile_sz;
	unsigned int mvpu_sec_fw_ofs;
	unsigned int mvpu_sec_fw_sz;
	unsigned int mvpu_sec_xfile_ofs;
	unsigned int mvpu_sec_xfile_sz;

	unsigned int up_coredump_ofs;
	unsigned int up_coredump_sz;
	unsigned int mdla_coredump_ofs;
	unsigned int mdla_coredump_sz;
	unsigned int mvpu_coredump_ofs;
	unsigned int mvpu_coredump_sz;
	unsigned int mvpu_sec_coredump_ofs;
	unsigned int mvpu_sec_coredump_sz;

	unsigned int ce_bin_ofs;
	unsigned int ce_bin_sz;

};

struct apusys_aee_coredump_info_t {
	unsigned int up_coredump_ofs;
	unsigned int up_coredump_sz;
	unsigned int regdump_ofs;
	unsigned int regdump_sz;
	unsigned int mdla_coredump_ofs;
	unsigned int mdla_coredump_sz;
	unsigned int mvpu_coredump_ofs;
	unsigned int mvpu_coredump_sz;
	unsigned int mvpu_sec_coredump_ofs;
	unsigned int mvpu_sec_coredump_sz;

	unsigned int up_xfile_ofs;
	unsigned int up_xfile_sz;
	unsigned int mdla_xfile_ofs;
	unsigned int mdla_xfile_sz;
	unsigned int mvpu_xfile_ofs;
	unsigned int mvpu_xfile_sz;
	unsigned int mvpu_sec_xfile_ofs;
	unsigned int mvpu_sec_xfile_sz;

	unsigned int ce_bin_ofs;
	unsigned int ce_bin_sz;
	unsigned int are_sram_ofs;
	unsigned int are_sram_sz;

};

struct apu_coredump {
	char *tcmdump;
	char *ramdump;
	char *regdump;
	char *tbufdump;
	uint32_t *cachedump;
} __packed;

struct mtk_apu {
	struct rproc *rproc;
	struct platform_device *pdev;
	struct device *dev;
	void *md32_tcm;
	void *md32_cache_dump;
	void *apu_sctrl_reviser;
	void *apu_wdt;
	void *apu_ao_ctl;
	void *md32_sysctrl;
	void *md32_debug_apb;
	void *apu_mbox;
	void *apu_rpc;
	void *apu_sec_mem_base;
	void *apu_aee_coredump_mem_base;
	void *coredump_buf;
	uint64_t coredump_buf_pa;
	void *are_reg_base;
	struct apu_coredump *coredump;
	dma_addr_t coredump_da;
	int wdt_irq_number;
	int ce_exp_irq_number;
	int mbox0_irq_number;
	spinlock_t reg_lock;
	spinlock_t wakelock_spinlock;

	struct apusys_secure_info_t *apusys_sec_info;
	struct apusys_aee_coredump_info_t *apusys_aee_coredump_info;
	uint64_t apusys_sec_mem_start;
	uint64_t apusys_sec_mem_size;
	uint64_t apusys_aee_coredump_mem_start;
	uint64_t apusys_aee_coredump_mem_size;

	uint64_t smc_time_diff_ns;
	uint64_t sub_latency[MAX_PWR_SUB_LATENCY];

	/* Buffer to place execution area */
	void *code_buf;
	dma_addr_t code_da;

	/* Buffer to place config area */
	struct config_v1 *conf_buf;
	dma_addr_t conf_da;

	/* to synchronize boot status of remote processor */
	struct apu_run run;

	/* to prevent multiple ipi_send run concurrently */
	struct mutex send_lock;
	struct mutex power_lock;
	struct mutex power_profile_lock;
	spinlock_t usage_cnt_lock;
	struct apu_ipi_desc ipi_desc[APU_IPI_MAX];
	u32 ipi_id;
	bool ipi_id_ack[APU_IPI_MAX]; /* per-ipi ack */
	bool ipi_inbound_locked;
	bool bypass_pwr_off_chk;
	bool bypass_aee;
	bool pwr_on_polling_dbg_mode;
	bool ce_dbg_polling_dump_mode;
	bool apusys_rv_trace_on;
	wait_queue_head_t ack_wq; /* for waiting for ipi ack */
	struct timespec64 intr_ts_begin;
	struct timespec64 intr_ts_end;
	struct apu_mbox_hdr hdr;

	/* ipi share buffer */
	dma_addr_t recv_buf_da;
	struct mtk_share_obj *recv_buf;
	dma_addr_t send_buf_da;
	struct mtk_share_obj *send_buf;

	struct work_struct timesync_work;

	struct work_struct deepidle_work;

	struct rproc_subdev *rpmsg_subdev;

	struct mtk_apu_platdata	*platdata;
	struct device *power_dev;
	struct device *apu_iommu0, *apu_iommu1;

	uint32_t md32_tcm_sz;
	uint32_t up_code_buf_sz;
	dma_addr_t apummu_hwlog_buf_da;

	uint32_t local_pwr_ref_cnt;
	uint32_t wake_lock_ref_cnt;
	uint32_t ipi_pwr_ref_cnt[APU_IPI_MAX];
	uint32_t ipi_wake_lock_ref_cnt[APU_IPI_MAX];

	bool disable_ke;
};

#define CONFIG_SIZE (round_up(sizeof(struct config_v1), PAGE_SIZE))
#define REG_SIZE (4UL * 151UL)
#define TBUF_SIZE (4UL * 32UL)
#define CACHE_DUMP_SIZE (37UL * 1024UL)
#define DRAM_OFFSET (0x00000UL)
#define TCM_OFFSET (0x1d000000UL)
#define CODE_BUF_DA (DRAM_OFFSET)
#define APU_SEC_FW_IOVA (0x200000UL)

int apu_mem_init(struct mtk_apu *apu);
void apu_mem_remove(struct mtk_apu *apu);

int apu_config_setup(struct mtk_apu *apu);
void apu_config_remove(struct mtk_apu *apu);

void apu_ipi_remove(struct mtk_apu *apu);
int apu_ipi_init(struct platform_device *pdev, struct mtk_apu *apu);
int apu_ipi_register(struct mtk_apu *apu, u32 id,
		ipi_top_handler_t top_handler, ipi_handler_t handler, void *priv);
void apu_ipi_unregister(struct mtk_apu *apu, u32 id);
int apu_ipi_send(struct mtk_apu *apu, u32 id, void *data, u32 len,
		 u32 wait_ms);

int apu_procfs_init(struct platform_device *pdev);
void apu_procfs_remove(struct platform_device *pdev);
int apu_coredump_init(struct mtk_apu *apu);
void apu_coredump_remove(struct mtk_apu *apu);
void apu_setup_dump(struct mtk_apu *apu, dma_addr_t da);
int apu_timesync_init(struct mtk_apu *apu);
void apu_timesync_remove(struct mtk_apu *apu);
int apu_debug_init(struct mtk_apu *apu);
void apu_debug_remove(struct mtk_apu *apu);

extern const struct mtk_apu_platdata mt6878_platdata;
extern const struct mtk_apu_platdata mt6879_platdata;
extern const struct mtk_apu_platdata mt6886_platdata;
extern const struct mtk_apu_platdata mt6893_platdata;
extern const struct mtk_apu_platdata mt6895_platdata;
extern const struct mtk_apu_platdata mt6897_platdata;
extern const struct mtk_apu_platdata mt6983_platdata;
extern const struct mtk_apu_platdata mt6985_platdata;
extern const struct mtk_apu_platdata mt6989_platdata;
extern const struct mtk_apu_platdata mt8188_platdata;

extern int reviser_set_init_info(struct mtk_apu *apu);
extern int vpu_set_init_info(struct mtk_apu *apu);
extern int power_set_chip_info(struct mtk_apu *apu);

extern int apu_get_power_dev(struct mtk_apu *apu);
extern int apu_deepidle_init(struct mtk_apu *apu);
extern void apu_deepidle_exit(struct mtk_apu *apu);

#define apu_info_ratelimited(dev, fmt, ...)  \
{                                                \
	static DEFINE_RATELIMIT_STATE(_rs,           \
				      HZ * 5,                    \
				      50);                       \
	if (__ratelimit(&_rs))                       \
		dev_info(dev, fmt, ##__VA_ARGS__);       \
}

#endif /* APU_H */
