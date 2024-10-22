// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Ning Li <ning.li@mediatek.com>
 */

#define pr_fmt(fmt)    "mtk-smmu-v3: " fmt

#include <linux/arm-smccc.h>
#include <linux/bitmap.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <asm/barrier.h>
#include <asm/ptrace.h>
#include <dt-bindings/memory/mtk-memory-port.h>

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_DBG)
#include "iommu_debug.h"
#endif
#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
#include "smmu_secure.h"
#endif
#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
#include "../../../misc/mediatek/smi/mtk-smi-dbg.h"
#endif

#include "arm-smmu-v3.h"
#include "mtk-smmu-v3.h"

#define LINK_WITH_APU			BIT(0)
/* For SMMU EP/bring up phase: smi not ready */
#define SMMU_EN_PRE			BIT(1)
/* For SMMU EP/bring up phase: CLK AO */
#define SMMU_CLK_AO_EN			BIT(2)
#define HAS_SMI_SUB_COMM		BIT(3)
#define SAME_SUBSYS			BIT(4)
#define SMMU_DELAY_HW_INIT		BIT(5)
#define SMMU_SEC_EN			BIT(6)
#define SMMU_SKIP_SHUTDOWN		BIT(7)
#define SMMU_HYP_EN			BIT(8)
#define SMMU_DIS_CPU_PARTID		BIT(9)
#define SMMU_HANG_DETECT		BIT(10)

#define SMMU_IRQ_COUNT_MAX		(5)
#define SMMU_IRQ_DISABLE_TIME		(10) /* 10s */
#define SMMU_POLL_MAX_ATTEMPTS		(50000)

#define SMMU_TF_IOVA_DUMP_NUM		(5)
#define SMMU_EVT_DUMP_LEN_MAX		(200)

#define SMMU_FAULT_RS_INTERVAL		DEFAULT_RATELIMIT_INTERVAL
#define SMMU_FAULT_RS_BURST		(2)

/* hyp-pmm smmu related SMC call */
#define HYP_PMM_SMMU_CONTROL		(0XBB00FFAE)

#define MTK_SMMU_ADDR(addr) ({						  \
	dma_addr_t _addr = addr;					  \
	((lower_32_bits(_addr) & GENMASK(31, 12)) | upper_32_bits(_addr));\
})

#define POWER_STA_ON			(0)
#define POWER_STA_OFF			(1)

#define STRSEC(sec)			((sec) ? "SECURE" : "NORMAL")

/* SMI debug related constants */
#define SMI_COM_REGS_SZ			(0x1000)
#define SMI_COM_REGS_NUM		(6)
#define SMI_COM_OFFSET1_START		(0x400)
#define SMI_COM_OFFSET1_END		(0x41c)
#define SMI_COM_OFFSET1_LEN		(8)
#define SMI_COM_OFFSET2_START		(0x430)
#define SMI_COM_OFFSET2_LEN		(2)
#define SMI_COM_OFFSET3_START		(0x440)
#define SMI_COM_OFFSET3_LEN		(2)

static const char *IOMMU_GROUP_PROP_NAME = "mtk,iommu-group";
static const char *SMMU_MPAM_CONFIG = "mtk,mpam-cfg";
static const char *SMMU_MPAM_CMAX = "mtk,mpam-cmax";
static const char *SMMU_GMAPM_CONFIG = "mtk,gmpam-cfg";
static const __maybe_unused char *PMU_SMMU_PROP_NAME = "mtk,smmu";

enum hyp_smmu_cmd {
	HYP_SMMU_STE_DUMP,
	HYP_SMMU_TF_DUMP,
	HYP_SMMU_REG_DUMP,
	HYP_SMMU_CMD_NUM
};

enum iova_type {
	NORMAL,
	PROTECTED,
	SECURE,
	IOVA_TYPE_NUM
};

struct mtk_smmu_iova_region {
	uint32_t		sid;
	dma_addr_t		iova_base;
	unsigned long long	size;
	enum iova_type		type;
};

struct dma_range_cb_data {
	int (*range_prop_entry_cb_fn)(const __be32 *p, int naddr,
				      int nsize, void *arg);
	void *arg;
};

struct ste_mpam_config {
	u32	sid;
	u32	partid;
	u32	pmg;
};

struct ste_mpam_cmax {
	u32	txu_id;
	u32	ris;
	u32	partid;
	u32	cmax;
};

static struct mtk_smmu_data *mtk_smmu_datas[SMMU_TYPE_NUM];
static unsigned int smmuwp_consume_intr(struct arm_smmu_device *smmu,
					unsigned int irq_bit);
static unsigned int smmuwp_process_intr(struct arm_smmu_device *smmu);
static struct mtk_smmu_fault_param *smmuwp_process_tf(
		struct arm_smmu_device *smmu,
		struct arm_smmu_master *master,
		struct mtk_iommu_fault_event *fault_evt);
static void smmuwp_clear_tf(struct arm_smmu_device *smmu);
static void smmuwp_dump_outstanding_monitor(struct arm_smmu_device *smmu);
static void smmuwp_dump_io_interface_signals(struct arm_smmu_device *smmu);
static void smmuwp_dump_dcm_en(struct arm_smmu_device *smmu);
static void mtk_smmu_glbreg_dump(struct arm_smmu_device *smmu);
static void smmu_debug_dump(struct arm_smmu_device *smmu, bool check_pm,
			    bool ratelimit);
static __le64 *arm_smmu_get_step_for_sid(struct arm_smmu_device *smmu,
					 u32 sid);
static void mtk_hyp_smmu_reg_dump(struct arm_smmu_device *smmu);

static inline unsigned int smmu_read_reg(void __iomem *base,
					 unsigned int offset)
{
	return readl_relaxed(base + offset);
}

static inline void smmu_write_reg(void __iomem *base,
				  unsigned int offset,
				  unsigned int val)
{
	writel_relaxed(val, base + offset);
}

static inline void smmu_write_field(void __iomem *base,
				    unsigned int reg,
				    unsigned int mask,
				    unsigned int val)
{
	unsigned int regval;

	regval = readl_relaxed(base + reg);
	regval = (regval & (~mask))|val;
	writel_relaxed(regval, base + reg);
}

static inline unsigned int smmu_read_field(void __iomem *base,
					   unsigned int reg,
					   unsigned int mask)
{
	return readl_relaxed(base + reg) & mask;
}

#define smmu_read_reg_poll_timeout(addr, val, cond, delay_us, timeout_us) \
	readx_poll_timeout_atomic(ioread32, addr, val, cond, delay_us, timeout_us)

static inline int smmu_write_reg_sync(void __iomem *base,
				      unsigned int val,
				      unsigned int reg_off,
				      unsigned int ack_off)
{
	unsigned int reg;

	smmu_write_reg(base, reg_off, val);
	return readl_relaxed_poll_timeout(base + ack_off, reg, reg == val,
					  1, ARM_SMMU_POLL_TIMEOUT_US);
}

static inline int smmu_read_reg_poll_value(void __iomem *base,
					   unsigned int offset,
					   unsigned int val)
{
	unsigned int observed, attempts = 0;

	while (attempts++ < SMMU_POLL_MAX_ATTEMPTS) {
		observed = smmu_read_reg(base, offset);
		if (val == observed)
			return 0;
	}

	pr_info("%s, timeout %p:0x%x, val:0x%x\n", __func__, base, offset, val);

	return -ETIMEDOUT;
}

static const struct mtk_smmu_plat_data *of_device_get_plat_data(struct device *dev);

static inline struct mtk_smmu_data *mkt_get_smmu_data(u32 smmu_type)
{
	struct mtk_smmu_data *data;

	if (smmu_type < SMMU_TYPE_NUM) {
		data = mtk_smmu_datas[smmu_type];
		if (data != NULL)
			return data;
	}

	return NULL;
}

static const char *get_smmu_name(enum mtk_smmu_type type)
{
	switch (type) {
	case MM_SMMU:
		return "MM";
	case APU_SMMU:
		return "APU";
	case SOC_SMMU:
		return "SOCSYS";
	case GPU_SMMU:
		return "GPU";
	default:
		return "Unknown";
	}
}

static const char *get_fault_reason_str(__u32 reason)
{
	switch (reason) {
	case EVENT_ID_UUT_FAULT:
		return "Unsupported Upstream Transaction";
	case EVENT_ID_BAD_STREAMID_FAULT:
		return "Bad StreamID";
	case EVENT_ID_STE_FETCH_FAULT:
		return "STE Fetch Fault";
	case EVENT_ID_BAD_STE_FAULT:
		return "Bad STE Fault";
	case EVENT_ID_BAD_ATS_TREQ_FAULT:
		return "ATS Translation Request Fault";
	case EVENT_ID_STREAM_DISABLED_FAULT:
		return "Stream Disabled";
	case EVENT_ID_TRANSL_FORBIDDEN_FAULT:
		return "ATS Translation Forbidden";
	case EVENT_ID_BAD_SUBSTREAMID_FAULT:
		return "Bad SubStreamID";
	case EVENT_ID_CD_FETCH_FAULT:
		return "CD Fetch Fault";
	case EVENT_ID_TRANSLATION_FAULT:
		return "Translation Fault";
	case EVENT_ID_ADDR_SIZE_FAULT:
		return "Address Size Fault";
	case EVENT_ID_ACCESS_FAULT:
		return "Access Flag Fault";
	case EVENT_ID_PERMISSION_FAULT:
		return "Permission Fault";
	case EVENT_ID_BAD_CD_FAULT:
		return "Bad CD Fault";
	case EVENT_ID_WALK_EABT_FAULT:
		return "EABT Fault";
	case EVENT_ID_TLB_CONFLICT_FAULT:
		return "TLB Conflict";
	case EVENT_ID_CFG_CONFLICT_FAULT:
		return "CFG Coflict";
	case EVENT_ID_PAGE_REQUEST_FAULT:
		return "Page Request Fault";
	case EVENT_ID_VMS_FETCH_FAULT:
		return "VMS Fetch Fault";
	default:
		return "Unknown Fault";
	}
}

static struct iommu_group *mtk_smmu_device_group(struct device *dev)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	const struct mtk_smmu_plat_data *plat_data;
	struct iommu_group *group = NULL;
	struct device_node *group_node;
	struct mtk_smmu_data *data;
	int i, sid;

	if (!master) {
		dev_info(dev, "get smmu master fail\n");
		return ERR_PTR(-ENODEV);
	}

	data = to_mtk_smmu_data(master->smmu);
	if (!data) {
		dev_info(dev, "get smmu data fail\n");
		return ERR_PTR(-ENODEV);
	}

	plat_data = data->plat_data;
	sid = get_smmu_stream_id(dev);
	dev_info(dev, "[%s] sid:0x%x\n", __func__, sid);
	if (sid < 0)
		return ERR_PTR(sid);

	mutex_lock(&data->group_mutexs);
	group_node = of_parse_phandle(dev->of_node, IOMMU_GROUP_PROP_NAME, 0);

	for (i = 0; i < data->iommu_group_nr; i++) {
		if (&group_node->fwnode == data->iommu_groups[i].group_id &&
		    plat_data->smmu_type == data->iommu_groups[i].smmu_type) {
			group = data->iommu_groups[i].iommu_group;
			break;
		}
	}

	if (!group) {
		pr_info("%s create group, smmu:%d,dev:%s,sid:%d\n",
			__func__,
			data->plat_data->smmu_type,
			dev_name(dev),
			sid);
		group = iommu_group_alloc();
		if (IS_ERR(group))
			dev_err(dev, "Failed to allocate IOMMU group\n");
	} else {
		pr_info("%s return exist, smmu:%d,dev:%s,sid:%d,group_name:%s\n",
			__func__,
			data->plat_data->smmu_type,
			dev_name(dev),
			sid,
			data->iommu_groups[i].label);
		iommu_group_ref_get(group);
	}
	mutex_unlock(&data->group_mutexs);

	return group;
}

static u64 get_smmu_tab_id_by_domain(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain;
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;
	u32 asid;
	u64 smmu_id;

	if (!domain)
		return 0;

	smmu_domain = to_smmu_domain(domain);
	smmu = smmu_domain->smmu;
	data = to_mtk_smmu_data(smmu);

	smmu_id = data->plat_data->smmu_type;
	asid = smmu_domain->s1_cfg.cd.asid;

	return smmu_id << 32 | asid;
}

static int mtk_smmu_map_pages(struct arm_smmu_domain *smmu_domain,
			      unsigned long iova, phys_addr_t paddr, size_t pgsize,
			      size_t pgcount, int prot, gfp_t gfp, size_t *mapped)
{
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int retry_count = 0;
	int ret;

	if (!ops)
		return -ENODEV;

	ret = ops->map_pages(ops, iova, paddr, pgsize, pgcount, prot, gfp, mapped);

	/* Retry if atomic alloc memory fail */
	while (ret == -ENOMEM && (gfp & GFP_ATOMIC) != 0 && retry_count < 8) {
		gfp_t gfp_flags = gfp;

		if (!(in_atomic() || irqs_disabled() || in_interrupt())) {
			/* If not in atomic ctx, wait memory reclaim */
			gfp_flags = (gfp & ~GFP_ATOMIC) | GFP_KERNEL;
		}

		ret = ops->map_pages(ops, iova, paddr, pgsize, pgcount,
				     prot, gfp_flags, mapped);
		dev_info_ratelimited(smmu->dev,
				     "[%s] retry map alloc memory gfp:0x%x->0x%x %d:%d\n",
				     __func__, gfp, gfp_flags, retry_count + 1, ret);

		if (ret == -ENOMEM) {
			retry_count++;
			if (in_atomic() || irqs_disabled() || in_interrupt()) {
				/* most wait 4ms at atomic */
				udelay(500);
			} else {
				usleep_range(8000, 10*1000);
			}
		}
	}

	if (ret)
		dev_info(smmu->dev,
			 "[%s] iova:0x%lx pgsize:0x%zx pgcount:0x%zx prot:%d gfp:0x%x %d:%d\n",
			 __func__, iova, pgsize, pgcount, prot, gfp, retry_count, ret);

	return ret;
}

static void mtk_iotlb_sync_map(struct iommu_domain *domain, unsigned long iova,
			       size_t size)
{
#ifdef MTK_SMMU_DEBUG
	u64 tab_id;

	tab_id = get_smmu_tab_id_by_domain(domain);
	mtk_iova_map(tab_id, iova, size);
#endif
}

static void mtk_iotlb_sync(struct iommu_domain *domain,
			   struct iommu_iotlb_gather *gather)
{
#ifdef MTK_SMMU_DEBUG
	size_t length = gather->end - gather->start + 1;
	u64 tab_id;

	if (gather->start > gather->end) {
		pr_info("%s fail, iova range: 0x%lx ~ 0x%lx\n",
			__func__, gather->start, gather->end);
		return;
	}

	tab_id = get_smmu_tab_id_by_domain(domain);
	if (gather->start > 0 && gather->start != ULONG_MAX)
		mtk_iova_unmap(tab_id, gather->start, length);
#endif
}

static void mtk_tlb_flush(struct arm_smmu_domain *smmu_domain,
			  unsigned long iova, size_t size,
			  int power_status)
{
#ifdef MTK_SMMU_DEBUG
	u64 tab_id;

	tab_id = get_smmu_tab_id_by_domain(&smmu_domain->domain);

	if (power_status != POWER_STA_ON)
		mtk_iommu_pm_trace(IOMMU_POWER_OFF, tab_id, POWER_STA_OFF, iova, NULL);

	mtk_iommu_tlb_sync_trace(iova, size, tab_id);
#endif
}

static bool mtk_delay_hw_init(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	bool delay_hw_init;

	delay_hw_init = MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_DELAY_HW_INIT);

	return delay_hw_init;
}

static int smmu_init_wpcfg(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	const struct mtk_smmu_plat_data *plat_data = data->plat_data;
	void __iomem *wp_base;
	u32 regval;
	int ret;

	wp_base = smmu->wp_base;
	dev_info(smmu->dev, "[%s] start, wp_base:0x%llx\n",
		 __func__, (unsigned long long)wp_base);

	/* DCM basic setting */
	smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_DCM_EN, CTL0_DCM_EN);
	smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_CFG_TAB_DCM_EN,
			 CTL0_CFG_TAB_DCM_EN);
	if (MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_DIS_CPU_PARTID))
		smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_CPU_PARTID_DIS,
				 CTL0_CPU_PARTID_DIS);

	/* Used for MM_SMMMU read command overtaking */
	if (data->plat_data->smmu_type == MM_SMMU)
		smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_STD_AXI_MODE_DIS,
				 CTL0_STD_AXI_MODE_DIS);

	if (data->plat_data->smmu_type == SOC_SMMU)
		/* Set SOC_SMMU TBU0 to use in-ordering */
		smmu_write_field(wp_base, SMMUWP_TBU0_MOGH0, MOGH_EN | MOGH_RW,
				 MOGH_EN | MOGH_RW);

	if (data->smmu_trans_type == SMMU_TRANS_WP_BYPASS) {
		dev_info(smmu->dev, "[%s] smmu:%s, smmuwp bypasss\n",
			 __func__, get_smmu_name(plat_data->smmu_type));
		smmu_write_field(wp_base, SMMUWP_GLB_CTL3, CTL3_BP_SMMU, CTL3_BP_SMMU);
	}

	/* Connect DVM */
	smmu_write_field(wp_base, SMMUWP_TCU_CTL4, TCU_DVM_EN_REQ, TCU_DVM_EN_REQ);
	ret = smmu_read_reg_poll_timeout((void *)(wp_base + SMMUWP_TCU_CTL4),
					 regval,
					 FIELD_GET(TCU_DVM_EN_ACK, regval),
					 1, SMMUWP_POLL_DVM_TIMEOUT_US);
	if (ret)
		dev_info(smmu->dev, "[%s] ret:%d, Connecting DVM is not responding\n",
			 __func__, ret);

	/* Set AXSLC */
	if (data->axslc) {
		smmu_write_field(wp_base, SMMUWP_GLB_CTL0,
				 CTL0_STD_AXI_MODE_DIS, CTL0_STD_AXI_MODE_DIS);
		smmu_write_field(wp_base, SMMU_TCU_CTL1_AXSLC, AXSLC_BIT_FIELD,
				 AXSLC_SET);
		smmu_write_field(wp_base, SMMU_TCU_CTL1_AXSLC, SLC_SB_ONLY_EN,
				 SLC_SB_ONLY_EN);
		dev_info(smmu->dev,
			 "[%s] AXSLC done SMMU_TCU_CTL1_AXSLC(0x%llx)=0x%x\n",
			 __func__,
			 (unsigned long long)wp_base + SMMU_TCU_CTL1_AXSLC,
			 smmu_read_reg(wp_base, SMMU_TCU_CTL1_AXSLC));
	}

	dev_info(smmu->dev,
		 "[%s] ret:%d, GLB_CTL0(0x%llx)=0x%x, GLB_CTL3(0x%llx)=0x%x\n",
		 __func__, ret,
		 (unsigned long long)wp_base + SMMUWP_GLB_CTL0,
		 smmu_read_reg(wp_base, SMMUWP_GLB_CTL0),
		 (unsigned long long)wp_base + SMMUWP_GLB_CTL3,
		 smmu_read_reg(wp_base, SMMUWP_GLB_CTL3));

	return ret;
}

static int smmu_deinit_wpcfg(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;
	int ret = 0;
	u32 regval;

	smmuwp_clear_tf(smmu);
	if (data->smmu_trans_type == SMMU_TRANS_WP_BYPASS)
		smmu_write_field(wp_base, SMMUWP_GLB_CTL3, CTL3_BP_SMMU, 0);

	/* Disconnect DVM */
	smmu_write_field(wp_base, SMMUWP_TCU_CTL4, TCU_DVM_EN_REQ, 0);
	ret = smmu_read_reg_poll_timeout((void *)(wp_base + SMMUWP_TCU_CTL4),
					 regval, !FIELD_GET(TCU_DVM_EN_ACK, regval),
					 1, SMMUWP_POLL_DVM_TIMEOUT_US);
	if (ret)
		dev_info(smmu->dev, "[%s] ret:%d, Disconnecting DVM not responding\n",
			 __func__, ret);

	return ret;
}

static void smmu_mpam_config_set(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	struct device_node *np;
	struct ste_mpam_config *mpam_cfgs;
	u64 sid_max = 1ULL << smmu->sid_bits;
	unsigned int mpam_cfgs_cnt;
	u32 sid, partid, pmg, regval;
	void __iomem *mpam;
	__le64 *step;
	int ret, n;

	mpam = smmu->base + ARM_SMMU_GMPAM;
	np = smmu->dev->of_node;

	/* Global mpam config */
	n = of_property_count_elems_of_size(np, SMMU_GMAPM_CONFIG, sizeof(u32));
	if (n == 2) {
		ret = of_property_read_u32_index(np, SMMU_GMAPM_CONFIG, 0, &partid);
		if (ret)
			return;
		ret = of_property_read_u32_index(np, SMMU_GMAPM_CONFIG, 1, &pmg);
		if (ret)
			return;

		regval = FIELD_PREP(SMMU_GMPAM_SO_PARTID, partid) |
			 FIELD_PREP(SMMU_GMPAM_SO_PMG, pmg);

		writel_relaxed(regval | SMMU_GMPAM_UPADTE, mpam);
		ret = smmu_read_reg_poll_timeout(mpam,
						 regval,
						 !(regval & SMMU_GMPAM_UPADTE),
						 1, ARM_SMMU_POLL_TIMEOUT_US);
		if (ret)
			dev_info(smmu->dev, "GMPAM not responding to update\n");
	}

	/* One mpam config consist of sid/partid/pmg three u32 */
	n = of_property_count_elems_of_size(np, SMMU_MPAM_CONFIG, sizeof(u32));
	if (n <= 0 || n % 3)
		return;

	mpam_cfgs_cnt = n / 3;
	mpam_cfgs = kcalloc(mpam_cfgs_cnt, sizeof(*mpam_cfgs), GFP_KERNEL);
	if (!mpam_cfgs)
		return;

	for (n = 0; n < mpam_cfgs_cnt; n++) {
		ret =  of_property_read_u32_index(np, SMMU_MPAM_CONFIG,
						  n * 3,
						  &mpam_cfgs[n].sid);
		if (ret)
			goto out_free;

		ret =  of_property_read_u32_index(np, SMMU_MPAM_CONFIG,
						  n * 3 + 1,
						  &mpam_cfgs[n].partid);
		if (ret)
			goto out_free;

		ret =  of_property_read_u32_index(np, SMMU_MPAM_CONFIG,
						  n * 3 + 2,
						  &mpam_cfgs[n].pmg);
		if (ret)
			goto out_free;

		if (mpam_cfgs[n].sid > sid_max) {
			WARN_ON(1);
			mpam_cfgs[n].sid = 0;
		}
		if (mpam_cfgs[n].partid > data->partid_max) {
			WARN_ON(1);
			mpam_cfgs[n].partid = 0;
		}
		if (mpam_cfgs[n].pmg > data->pmg_max) {
			WARN_ON(1);
			mpam_cfgs[n].pmg = 0;
		}
	}

	for (n = 0; n < mpam_cfgs_cnt; n++) {
		sid = mpam_cfgs[n].sid;
		// make sure ste is ready
		ret = arm_smmu_init_sid_strtab(smmu, sid);
		if (ret) {
			dev_err(smmu->dev, "%s init sid(0x%x) failed\n",
				__func__, sid);
			continue;
		}
		step = arm_smmu_get_step_for_sid(smmu, sid);
		step[4] = FIELD_PREP(STRTAB_STE_4_PARTID, mpam_cfgs[n].partid);
		step[5] = FIELD_PREP(STRTAB_STE_5_PMG, mpam_cfgs[n].pmg);
	}

out_free:
	kfree(mpam_cfgs);
}

static void mpam_set_txu_cmax(struct arm_smmu_device *smmu,
			      u32 txu_id, u32 partid, u32 ris, u32 cmax)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	struct txu_mpam_data mpam_data;
	u32 val;

	mpam_data = data->txu_mpam_data[txu_id];

	val = FIELD_PREP(PART_SEL_PARTID_SEL, partid);
	smmu_write_field(mpam_data.mpam_base,
			 MPAMCFG_PART_SEL,
			 PART_SEL_PARTID_SEL,
			 val);

	val = FIELD_PREP(PART_SEL_RIS, ris);
	smmu_write_field(mpam_data.mpam_base,
			 MPAMCFG_PART_SEL,
			 PART_SEL_RIS,
			 val);

	val = FIELD_PREP(MPAMCFG_CMAX_CMAX, cmax);
	smmu_write_field(mpam_data.mpam_base,
			 MPAMCFG_CMAX,
			 MPAMCFG_CMAX_CMAX,
			 val);

	if (mpam_data.has_cmax_softlim) {
		val = FIELD_PREP(MPAMCFG_CMAX_SOFTLIM,
				 1);
		smmu_write_field(mpam_data.mpam_base,
				 MPAMCFG_CMAX,
				 MPAMCFG_CMAX_SOFTLIM,
				 val);
	}
	pr_info("%s, txu_id:%d, partid:%d, ris:%d, cmx:0x%x\n",
		__func__, txu_id, partid, ris, cmax);
}

static void smmu_mpam_cmax_set(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	struct device_node *np;
	unsigned int mpam_cmax_cnt;
	struct ste_mpam_cmax *mpam_cmax;
	struct txu_mpam_data *txu_mpam_data;
	int ret, n;

	np = smmu->dev->of_node;
	/* One mpam cmax config consist of txu_id/ris/partid/cmax two u32 */
	n = of_property_count_elems_of_size(np, SMMU_MPAM_CMAX, sizeof(u32));
	if (n <= 0 || n % 4)
		return;

	mpam_cmax_cnt = n / 4;
	mpam_cmax = kcalloc(mpam_cmax_cnt, sizeof(*mpam_cmax), GFP_KERNEL);
	if (!mpam_cmax)
		return;

	for (n = 0; n < mpam_cmax_cnt; n++) {
		ret =  of_property_read_u32_index(np, SMMU_MPAM_CMAX,
						  n * 4,
						  &mpam_cmax[n].txu_id);
		if (ret)
			goto out_free;

		ret =  of_property_read_u32_index(np, SMMU_MPAM_CMAX,
						  n * 4 + 1,
						  &mpam_cmax[n].ris);
		if (ret)
			goto out_free;

		ret =  of_property_read_u32_index(np, SMMU_MPAM_CMAX,
						  n * 4 + 2,
						  &mpam_cmax[n].partid);
		if (ret)
			goto out_free;

		ret =  of_property_read_u32_index(np, SMMU_MPAM_CMAX,
						  n * 4 + 3,
						  &mpam_cmax[n].cmax);
		if (ret)
			goto out_free;

		if (mpam_cmax[n].txu_id >= data->txu_mpam_data_cnt) {
			pr_info("%s, idx:%d, txu_id:%d, cnt:%d",
				__func__, n, mpam_cmax[n].txu_id,
				data->txu_mpam_data_cnt);
			WARN_ON(1);
			mpam_cmax[n].txu_id = 0;
			continue;
		}
		txu_mpam_data = &data->txu_mpam_data[mpam_cmax[n].txu_id];

		if (mpam_cmax[n].ris > txu_mpam_data->ris_max) {
			pr_info("%s, idx:%d, ris:%d, ris_max:%d",
				__func__, n, mpam_cmax[n].ris,
				txu_mpam_data->ris_max);
			WARN_ON(1);
			mpam_cmax[n].ris = 0;
			continue;
		}

		if (mpam_cmax[n].partid > txu_mpam_data->partid_max) {
			pr_info("%s, idx:%d, partid:%d, partid_max:%d",
				__func__, n, mpam_cmax[n].partid,
				txu_mpam_data->partid_max);
			WARN_ON(1);
			mpam_cmax[n].partid = 0;
			continue;
		}

		// use tcu camx_wd to check dts's cmax value
		if (mpam_cmax[n].cmax > txu_mpam_data->cmax_max_int) {
			pr_info("%s, idx:%d, cmax:%d, cmax_max_int:%d",
				__func__, n, mpam_cmax[n].cmax,
				txu_mpam_data->cmax_max_int);
			WARN_ON(1);
			mpam_cmax[n].cmax = 0;
			continue;
		}
	}

	for (n = 0; n < mpam_cmax_cnt; n++)
		mpam_set_txu_cmax(smmu, mpam_cmax[n].txu_id,
				  mpam_cmax[n].partid,
				  mpam_cmax[n].ris,
				  mpam_cmax[n].cmax);

out_free:
	kfree(mpam_cmax);
}

static void __iomem *smmu_ioremap(struct device *dev, resource_size_t start,
				      resource_size_t size)
{
	struct resource res = DEFINE_RES_MEM(start, size);

	return devm_ioremap_resource(dev, &res);
}

static void smmu_mpam_register(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	struct txu_mpam_data *mpam_data;
	struct resource *res;
	resource_size_t ioaddr;
	int i, tbu_cnt;
	u32 reg;

	reg = readl_relaxed(smmu->base + ARM_SMMU_IDR3);
	if (!FIELD_GET(IDR3_MPAM, reg))
		goto out_free;

	smmu->features |= ARM_SMMU_FEAT_MPAM;
	tbu_cnt = SMMU_TBU_CNT(data->plat_data->smmu_type);
	/* TCU + n TBUs all need remap */
	data->txu_mpam_data = kcalloc(tbu_cnt + 1, sizeof(*data->txu_mpam_data),
				      GFP_KERNEL);
	if (!data->txu_mpam_data)
		return;

	data->txu_mpam_data_cnt = tbu_cnt + 1;
	res = platform_get_resource(to_platform_device(smmu->dev),
				    IORESOURCE_MEM, 0);
	if (!res) {
		dev_info(smmu->dev, "%s get io resource fail\n", __func__);
		goto out_free;
	}

	ioaddr = res->start;
	mpam_data = &data->txu_mpam_data[0];
	mpam_data->mpam_base = smmu_ioremap(smmu->dev,
					    ioaddr + SMMU_MPAM_TCU_OFFSET,
					    SMMU_MPAM_REG_SZ);

	if (IS_ERR(mpam_data->mpam_base)) {
		dev_info(smmu->dev, "%s mpam[0] base err\n", __func__);
		goto out_free;
	}

	for (i = 0; i < tbu_cnt; i++) {
		mpam_data = &data->txu_mpam_data[i + 1];
		mpam_data->mpam_base
			= smmu_ioremap(smmu->dev,
				       ioaddr + SMMU_MPAM_TBUx_OFFSET(i),
				       SMMU_MPAM_REG_SZ);
		if (IS_ERR(mpam_data->mpam_base)) {
			dev_info(smmu->dev,
				 "%s mpam[%d] base err\n", __func__, i + 1);
			goto out_free;
		}
	}

	reg = readl_relaxed(smmu->base + ARM_SMMU_MPAMIDR);
	data->partid_max = FIELD_GET(SMMU_MPAMIDR_PARTID_MAX, reg);
	data->pmg_max = FIELD_GET(SMMU_MPAMIDR_PMG_MAX, reg);

	for (i = 0; i < data->txu_mpam_data_cnt; i++) {
		mpam_data = &data->txu_mpam_data[i];

		reg = readl_relaxed(mpam_data->mpam_base + MPAMF_IDR_LO);
		mpam_data->partid_max = FIELD_GET(MPAMF_IDR_LO_PARTID_MAX, reg);
		mpam_data->pmg_max= FIELD_GET(MPAMF_IDR_LO_PMG_MAX, reg);
		mpam_data->has_ccap_part= FIELD_GET(MPAMF_IDR_LO_HAS_CCAP_PART, reg);
		mpam_data->has_msmon= FIELD_GET(MPAMF_IDR_LO_HAS_MSMON, reg);

		reg = readl_relaxed(mpam_data->mpam_base + MPAMF_IDR_HI);
		mpam_data->has_ris = FIELD_GET(MPAMF_IDR_HI_HAS_RIS, reg);
		mpam_data->ris_max = FIELD_GET(MPAMF_IDR_HI_RIS_MAX, reg);

		reg = readl_relaxed(mpam_data->mpam_base + MPAMF_CCAP_IDR);
		mpam_data->cmax_max_int = (2 << FIELD_GET(CCAP_IDR_CMAX_WD, reg)) - 1;
		mpam_data->has_cmax_softlim = FIELD_GET(CCAP_IDR_HAS_CMAX_SOFTLIM, reg);
	}

	smmu_mpam_config_set(smmu);

	smmu_mpam_cmax_set(smmu);

	return;

out_free:
	kfree(data->txu_mpam_data);
	data->txu_mpam_data_cnt= 0;
}

static int mtk_smmu_hw_init(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	const struct mtk_smmu_plat_data *plat_data = data->plat_data;
	int ret;

	dev_info(smmu->dev,
		 "[%s] plat_data{smmu_plat:%d, flags:0x%x, smmu:%s}\n",
		 __func__, plat_data->smmu_plat, plat_data->flags,
		 get_smmu_name(plat_data->smmu_type));

	/* Make sure ste entries installed before smmu enable */
	arm_smmu_init_sid_strtab(smmu, 0);
	data->hw_init_flag = 1;

	ret = smmu_init_wpcfg(smmu);

	smmu_mpam_register(smmu);
	if (data->plat_data->smmu_type == GPU_SMMU)
		smmu->features |= ARM_SMMU_FEAT_DIS_EVTQ;

	return ret;
}

static int mtk_smmu_hw_deinit(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	const struct mtk_smmu_plat_data *plat_data = data->plat_data;
	int ret;

	dev_info(smmu->dev,
		 "[%s] plat_data{smmu_plat:%d, flags:0x%x, smmu:%s}\n",
		 __func__, plat_data->smmu_plat, plat_data->flags,
		 get_smmu_name(plat_data->smmu_type));

	data->hw_init_flag = 0;

	ret = smmu_deinit_wpcfg(smmu);

	return ret;
}

static int mtk_smmu_hw_sec_init(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	const struct mtk_smmu_plat_data *plat_data = data->plat_data;
	int ret = 0;

	if (!MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_SEC_EN))
		return 0;

	/* SOC & GPU SMMU not support secure init */
	if (plat_data->smmu_type == SOC_SMMU || plat_data->smmu_type == GPU_SMMU)
		return 0;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	ret = mtk_smmu_sec_init(plat_data->smmu_type);
	if (ret)
		dev_info(smmu->dev, "[%s] smmu:%s fail\n", __func__,
			 get_smmu_name(plat_data->smmu_type));
#endif

	return 0;
}

static int mtk_smmu_power_get(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	const struct mtk_smmu_plat_data *plat_data = data->plat_data;
	int ret = 0;

	if (MTK_SMMU_HAS_FLAG(plat_data, SMMU_CLK_AO_EN))
		return 0;

	if (data->pm_ops && data->pm_ops->pm_get) {
		ret = data->pm_ops->pm_get();
		if (ret <= 0) {
			dev_info(smmu->dev, "[%s] power_status:%d, smmu:%s\n",
				 __func__, ret, get_smmu_name(plat_data->smmu_type));
			return -1;
		} else {
			return 0;
		}
	}

	ret = mtk_smmu_pm_get(plat_data->smmu_type);

	return ret;
}

static int mtk_smmu_power_put(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	const struct mtk_smmu_plat_data *plat_data = data->plat_data;
	int ret = 0;

	if (MTK_SMMU_HAS_FLAG(plat_data, SMMU_CLK_AO_EN))
		return 0;

	if (data->pm_ops && data->pm_ops->pm_put) {
		ret = data->pm_ops->pm_put();
		if (ret < 0) {
			dev_info(smmu->dev, "[%s] power_status:%d, smmu:%s\n",
				 __func__, ret, get_smmu_name(plat_data->smmu_type));
			return -1;
		} else {
			return 0;
		}
	}

	ret = mtk_smmu_pm_put(plat_data->smmu_type);

	return ret;
}

static int mtk_smmu_runtime_suspend(struct device *dev)
{
#ifdef MTK_SMMU_DEBUG
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	struct mtk_smmu_data *data;

	if (!smmu)
		return -1;

	data = to_mtk_smmu_data(smmu);
	mtk_iommu_pm_trace(IOMMU_SUSPEND, data->plat_data->smmu_type, 0, 0, dev);
#endif

	return 0;
}

static int mtk_smmu_runtime_resume(struct device *dev)
{
#ifdef MTK_SMMU_DEBUG
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	struct mtk_smmu_data *data;

	if (!smmu)
		return -1;

	data = to_mtk_smmu_data(smmu);
	mtk_iommu_pm_trace(IOMMU_RESUME, data->plat_data->smmu_type, 0, 0, dev);
#endif

	return 0;
}

struct device_node *parse_iommu_group_phandle(struct device *dev)
{
	struct device_node *np;

	if (!dev->of_node)
		return NULL;

	np = of_parse_phandle(dev->of_node, IOMMU_GROUP_PROP_NAME, 0);
	return np ? np : dev->of_node;
}

static int insert_dma_range(const __be32 *p, int naddr, int nsize,
			    struct list_head *head)
{
	struct iommu_resv_region *region, *new;
	u64 start = of_read_number(p, naddr);
	u64 end = start + of_read_number(p + naddr, nsize) - 1;
	u64 region_end;

	list_for_each_entry(region, head, list) {
		region_end = region->start + region->length - 1;
		if (end >= region->start && start <= region_end) {
			pr_info("overlapped, (0x%llx~0x%llx) to (0x%llx~0x%llx)\n",
				start, end, region->start, region_end);
			WARN_ON(1);
			return -EINVAL;
		}

		if (start < region->start)
			break;
	}

	new = iommu_alloc_resv_region(start, end - start + 1,
				      0, IOMMU_RESV_RESERVED, GFP_KERNEL);
	if (!new)
		return -ENOMEM;
	list_add_tail(&new->list, &region->list);
	return 0;
}

static int invert_dma_regions(struct device *dev,
			      struct list_head *head,
			      struct list_head *inverted)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct iommu_resv_region *prev, *curr, *new;
	phys_addr_t rsv_start, aperture_end;
	size_t rsv_size;
	int ret = 0;

	dev_info(dev, "[%s] !master:%d\n", __func__, !master);

	if (!master)
		return -EINVAL;

	if (list_empty(head)) {
		pr_info("dma range empty, dev:%s\n", dev_name(dev));
		return -EINVAL;
	}

	/*
	 * Handle case between dma ranges.
	 */
	prev = NULL;
	list_for_each_entry(curr, head, list) {
		if (!prev)
			goto next;

		rsv_start = prev->start + prev->length;
		rsv_size = curr->start - rsv_start;
		if (!rsv_size)
			goto next;

		new = iommu_alloc_resv_region(rsv_start, rsv_size,
						0, IOMMU_RESV_RESERVED, GFP_KERNEL);
		if (!new) {
			ret = -ENOMEM;
			goto out_err;
		}

		dev_info(dev, "[%s]-1 start:0x%llx, length:0x%lx, prot:0x%x, type:%d\n",
			 __func__, new->start, new->length, new->prot, new->type);

		list_add_tail(&new->list, inverted);
next:
		prev = curr;
	}

	/* Handle case smaller that dma ranges */
	curr = list_first_entry(head, struct iommu_resv_region, list);
	rsv_start = 0;
	rsv_size = curr->start;
	if (rsv_size) {
		new = iommu_alloc_resv_region(rsv_start, rsv_size,
						0, IOMMU_RESV_RESERVED, GFP_KERNEL);
		if (!new) {
			ret = -ENOMEM;
			goto out_err;
		}

		dev_info(dev, "[%s]-2 start:0x%llx, length:0x%lx, prot:0x%x, type:%d\n",
			 __func__, new->start, new->length, new->prot, new->type);

		list_add(&new->list, inverted);
	}

	/* Handle case larger that dma ranges */
	aperture_end = master->domain->domain.geometry.aperture_end;
	rsv_start = prev->start + prev->length;
	if (rsv_start > aperture_end)
		return 0;

	rsv_size = aperture_end - rsv_start + 1;
	if (rsv_size && (U64_MAX - prev->start > prev->length)) {
		new = iommu_alloc_resv_region(rsv_start, rsv_size,
					     0, IOMMU_RESV_RESERVED, GFP_KERNEL);
		if (!new) {
			ret = -ENOMEM;
			goto out_err;
		}

		dev_info(dev, "[%s]-3 start:0x%llx, length:0x%lx, prot:0x%x, type:%d\n",
			 __func__, new->start, new->length, new->prot, new->type);

		list_add_tail(&new->list, inverted);
	}

	return 0;

out_err:
	list_for_each_entry_safe(curr, prev, inverted, list)
		kfree(curr);

	dev_info(dev, "[%s] iommu_alloc_resv_region out_err\n", __func__);
	return ret;
}

int collect_smmu_dma_regions(struct device *dev, struct list_head *head)
{
	const char *propname = "mtk,iommu-dma-range";
	const __be32 *p, *property_end;
	int ret, len, naddr, nsize;
	struct device_node *np;
	int err;

	dev_info(dev, "[%s], dev:%s\n", __func__, dev_name(dev));

	/* Repalce with iommu-group np if exist */
	np = parse_iommu_group_phandle(dev);
	if (!np)
		return -EINVAL;

	p = of_get_property(np, propname, &len);
	if (!p)
		return -ENODEV;

	len /= sizeof(u32);
	err = of_property_read_u32(np, "#address-cells", &naddr);
	if (err)
		naddr = of_n_addr_cells(np);
	err = of_property_read_u32(np, "#size-cells", &nsize);
	if (err)
		nsize = of_n_size_cells(np);

	if (!naddr || !nsize || len % (naddr + nsize)) {
		pr_info("%s err:%d, p:%s, len:%d, naddr:%d, nsize:%d, dev:%s, np:%s\n",
			__func__, err, propname, len, naddr, nsize, dev_name(dev),
			np->full_name);
		return -EINVAL;
	}
	property_end = p + len;

	while (p < property_end) {
		ret = insert_dma_range(p, naddr, nsize, head);
		if (ret) {
			dev_info(dev, "%s, insert dma range fail\n", __func__);
			return ret;
		}
		p += naddr + nsize;
	}

	return 0;
}

static void mtk_get_resv_regions(struct device *dev, struct list_head *head)
{
	LIST_HEAD(resv_regions);
	LIST_HEAD(dma_regions);
	int ret;

	dev_info(dev, "[%s], dev:%s\n", __func__, dev_name(dev));

	ret = collect_smmu_dma_regions(dev, &dma_regions);
	if (ret)
		return;

	ret = invert_dma_regions(dev, &dma_regions, &resv_regions);
	iommu_put_resv_regions(dev, &dma_regions);
	if (ret)
		return;

	list_splice(&resv_regions, head);
}

static __le64 *arm_smmu_get_cd_ptr(struct arm_smmu_domain *smmu_domain,
			  u32 ssid)
{
	unsigned int idx;
	struct arm_smmu_l1_ctx_desc *l1_desc;
	struct arm_smmu_ctx_desc_cfg *cdcfg = &smmu_domain->s1_cfg.cdcfg;

	if (!cdcfg)
		return NULL;

	if (smmu_domain->s1_cfg.s1fmt == STRTAB_STE_0_S1FMT_LINEAR) {
		if (!cdcfg->cdtab)
			return NULL;

		return cdcfg->cdtab + ssid * CTXDESC_CD_DWORDS;
	}

	if (!cdcfg->l1_desc)
		return NULL;

	idx = ssid >> CTXDESC_SPLIT;
	l1_desc = &cdcfg->l1_desc[idx];
	if (!l1_desc->l2ptr)
		return NULL;

	idx = ssid & (CTXDESC_L2_ENTRIES - 1);
	return l1_desc->l2ptr + idx * CTXDESC_CD_DWORDS;
}

static __le64 *arm_smmu_get_step_for_sid(struct arm_smmu_device *smmu, u32 sid)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	__le64 *step;

	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB) {
		struct arm_smmu_strtab_l1_desc *l1_desc;
		int idx;

		/* Two-level walk */
		idx = (sid >> STRTAB_SPLIT) * STRTAB_L1_DESC_DWORDS;
		l1_desc = &cfg->l1_desc[idx];
		idx = (sid & ((1 << STRTAB_SPLIT) - 1)) * STRTAB_STE_DWORDS;
		step = &l1_desc->l2ptr[idx];
	} else {
		/* Simple linear lookup */
		step = &cfg->strtab[sid * STRTAB_STE_DWORDS];
	}

	return step;
}

static bool is_dev_bypass_smmu_s1_enabled(struct device *dev)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	int enable;

	if (!master)
		return false;

	enable = test_bit(IOMMU_DEV_FEAT_BYPASS_S1, master->features);
	return enable ? true : false;
}

static int set_dev_bypass_smmu_s1(struct device *dev, bool enable)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct arm_smmu_cmdq_ent prefetch_cmd;
	__le64 *steptr;
	bool enabled;
	u64 val;
	u32 sid;
	int i;

	if (!master)
		return -1;

	sid = master->streams[0].id;
	enabled = is_dev_bypass_smmu_s1_enabled(dev);
	prefetch_cmd.opcode = CMDQ_OP_PREFETCH_CFG;
	prefetch_cmd.prefetch.sid = sid;

	if ((enabled && enable) || (!enabled && !enable))
		return 0;

	if (enable)
		set_bit(IOMMU_DEV_FEAT_BYPASS_S1, master->features);
	else
		clear_bit(IOMMU_DEV_FEAT_BYPASS_S1, master->features);

	steptr = arm_smmu_get_step_for_sid(master->smmu, sid);
	val = le64_to_cpu(steptr[0]);
	set_mask_bits(&val, GENMASK_ULL(3, 1), 0);

	if (enable)
		val |= FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_BYPASS);
	else
		val |= FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_S1_TRANS);

	steptr[0] = cpu_to_le64(val);

	arm_smmu_sync_ste_for_sid(master->smmu, sid);
	/* See comment in arm_smmu_write_ctx_desc() */
	WRITE_ONCE(steptr[0], cpu_to_le64(val));
	arm_smmu_sync_ste_for_sid(master->smmu, sid);

	/* It's likely that we'll want to use the new STE soon */
	if (!(master->smmu->options & ARM_SMMU_OPT_SKIP_PREFETCH))
		arm_smmu_cmdq_issue_cmd(master->smmu, &prefetch_cmd);

	/* print ste */
	for (i = 0; i < STRTAB_STE_DWORDS; i++)
		pr_info("ste %d u64[%d]:0x%016llx\n", sid, i, le64_to_cpu(steptr[i]));

	return 0;
}

static int mtk_smmu_def_domain_type(struct device *dev)
{
	const char *propname = "mtk,smmu-dma-mode";
	struct device_node *np;
	const char *str;

	if (!dev)
		return false;

	/* Repalce with iommu-group np if exist */
	np = parse_iommu_group_phandle(dev);
	if (!np)
		return -EINVAL;

	if (of_property_read_string(np, propname, &str))
		str = "default";

	if (!strcmp(str, "disable")) {
		pr_info("%s, disable s1, dev=%s\n", __func__, dev_name(dev));
		return IOMMU_DOMAIN_IDENTITY;
	}

	if (!strcmp(str, "bypass")) {
		pr_info("%s, bypass s1, dev=%s\n", __func__, dev_name(dev));
		return IOMMU_DOMAIN_DMA;
	}

	return 0;
}

static bool mtk_smmu_dev_has_feature(struct device *dev,
				     enum iommu_dev_features feat)
{
	struct arm_smmu_master *master = dev_iommu_priv_get(dev);
	unsigned long mtk_iommu_feat = (unsigned long)feat;
	struct arm_smmu_device *smmu;

	if (!master)
		return false;

	smmu = master->smmu;

	switch (mtk_iommu_feat) {
	case IOMMU_DEV_FEAT_BYPASS_S1:
		pr_info("%s return true for feature:0x%lx, dev:%s\n",
			__func__, mtk_iommu_feat, dev_name(dev));
		return true;
	default:
		pr_info("%s return true for feature:0x%lx, dev:%s\n",
			__func__, mtk_iommu_feat, dev_name(dev));
		return false;
	}
}

static bool mtk_smmu_dev_feature_enabled(struct device *dev,
					 enum iommu_dev_features feat)
{
	unsigned long mtk_iommu_feat = (unsigned long)feat;

	switch (mtk_iommu_feat) {
	case IOMMU_DEV_FEAT_BYPASS_S1:
		pr_info("%s return %d for feature:0x%lx, dev:%s\n", __func__,
			is_dev_bypass_smmu_s1_enabled(dev),
			mtk_iommu_feat, dev_name(dev));
		return is_dev_bypass_smmu_s1_enabled(dev);
	default:
		pr_info("%s return false for feature:0x%lx, dev:%s\n", __func__,
			mtk_iommu_feat, dev_name(dev));
		return false;
	}
}

static bool mtk_smmu_dev_enable_feature(struct device *dev,
					enum iommu_dev_features feat)
{
	unsigned long mtk_iommu_feat = (unsigned long)feat;

	switch (mtk_iommu_feat) {
	case IOMMU_DEV_FEAT_BYPASS_S1:
		return set_dev_bypass_smmu_s1(dev, true);
	default:
		return false;
	}
}

static bool mtk_smmu_dev_disable_feature(struct device *dev,
					 enum iommu_dev_features feat)
{
	unsigned long mtk_iommu_feat = (unsigned long)feat;

	switch (mtk_iommu_feat) {
	case IOMMU_DEV_FEAT_BYPASS_S1:
		return set_dev_bypass_smmu_s1(dev, false);
	default:
		return false;
	}
}

static void mtk_smmu_setup_features(struct arm_smmu_master *master,
				    u32 sid, __le64 *dst)
{
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;
	u64 val;

	if (!master || !master->smmu || !dst)
		return;

	smmu = master->smmu;
	data = to_mtk_smmu_data(smmu);

	/* setup tcu prefetch */
	if ((smmu->features & ARM_SMMU_FEAT_TCU_PF) && !master->ats_enabled) {
		val = FIELD_PREP(STRTAB_STE_1_TCU_PF, data->tcu_prefetch);
		dst[1] |= cpu_to_le64(val);
	}
}

//=====================================================
// SMMU IRQ rate limit setup
//=====================================================
static int mtk_smmu_setup_irqs(struct arm_smmu_device *smmu, bool enable)
{
	u32 irqen_flags;
	int ret;

	if (enable) {
		irqen_flags = IRQ_CTRL_EVTQ_IRQEN | IRQ_CTRL_GERROR_IRQEN;
		if (smmu->features & ARM_SMMU_FEAT_PRI)
			irqen_flags |= IRQ_CTRL_PRIQ_IRQEN;
	} else {
		irqen_flags = 0;
	}

	dev_info(smmu->dev, "[%s] Enable:%d IRQs irqen_flags:0x%x\n",
		 __func__, enable, irqen_flags);
	smmu_write_reg(smmu->base, ARM_SMMU_IRQ_CTRL, irqen_flags);
	ret = smmu_read_reg_poll_value(smmu->base, ARM_SMMU_IRQ_CTRLACK, irqen_flags);
	if (ret) {
		dev_err(smmu->dev, "[%s] Enable:%d IRQs failed\n", __func__, enable);
		return ret;
	}

	return 0;
}

static int mtk_smmu_sec_setup_irqs(struct arm_smmu_device *smmu, bool enable)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	u32 smmu_type = data->plat_data->smmu_type;
	int ret = 0;

	if (!MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_SEC_EN))
		return 0;

	/* SOC & GPU SMMU not support secure irq */
	if (smmu_type == SOC_SMMU || smmu_type == GPU_SMMU)
		return 0;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	/* enable or disable secure interrupt */
	ret = mtk_smmu_sec_irq_setup(smmu_type, enable);
	if (ret) {
		dev_info(smmu->dev, "[%s] Enable:%d IRQs failed\n", __func__, enable);
		return ret;
	}
#endif

	return 0;
}

static inline void mtk_smmu_irq_setup(struct mtk_smmu_data *data, bool enable)
{
	struct arm_smmu_device *smmu = &data->smmu;

	pr_info("[%s] enable:%d smmu:%s\n", __func__, enable,
		get_smmu_name(data->plat_data->smmu_type));

	mtk_smmu_setup_irqs(smmu, enable);
	mtk_smmu_sec_setup_irqs(smmu, enable);
}

static void mtk_smmu_irq_restart(struct timer_list *t)
{
	struct mtk_smmu_data *data = from_timer(data, t, irq_pause_timer);
	struct arm_smmu_device *smmu;
	int ret;

	pr_info("[%s] smmu:%s\n", __func__,
		get_smmu_name(data->plat_data->smmu_type));

	smmu = &data->smmu;
	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return;
	}

	mtk_smmu_irq_setup(data, true);

	mtk_smmu_power_put(smmu);

#ifdef MTK_SMMU_DEBUG
	mtk_iommu_debug_reset();
#endif
}

static int mtk_smmu_irq_pause_timer_init(struct mtk_smmu_data *data)
{
	pr_info("[%s] smmu:%s\n", __func__,
		get_smmu_name(data->plat_data->smmu_type));

	timer_setup(&data->irq_pause_timer, mtk_smmu_irq_restart, 0);

	data->irq_first_jiffies = 0;
	data->irq_cnt = 0;

	return 0;
}

static int mtk_smmu_irq_pause(struct mtk_smmu_data *data, int delay)
{
	pr_info("[%s] delay:%d\n", __func__, delay);

	/* disable all intr */
	mtk_smmu_irq_setup(data, false);

	/* delay seconds */
	data->irq_pause_timer.expires = jiffies + delay * HZ;
	if (!timer_pending(&data->irq_pause_timer))
		add_timer(&data->irq_pause_timer);

	return 0;
}

static void mtk_smmu_irq_record(struct mtk_smmu_data *data)
{
	/* we allow one irq in 1s, or we will disable them after irq_cnt s. */
	if (!data->irq_cnt ||
	    time_after(jiffies, data->irq_first_jiffies + data->irq_cnt * HZ)) {
		data->irq_cnt = 1;
		data->irq_first_jiffies = jiffies;
	} else {
		data->irq_cnt++;
		if (data->irq_cnt >= SMMU_IRQ_COUNT_MAX) {
			/* irq too many! disable irq for a while, to avoid HWT timeout*/
			mtk_smmu_irq_pause(data, SMMU_IRQ_DISABLE_TIME);
			data->irq_cnt = 0;
		}
	}
}

static irqreturn_t mtk_smmu_sec_irq_process(int irq, void *dev)
{
	struct arm_smmu_device *smmu = dev;
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	u32 smmu_type = data->plat_data->smmu_type;
	unsigned long fault_iova = 0, fault_pa = 0, fault_id = 0;
	bool need_handle = false;
	int ret = 0;

	if (!MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_SEC_EN))
		return IRQ_NONE;

	/* SOC & GPU SMMU not support secure TF */
	if (smmu_type == SOC_SMMU || smmu_type == GPU_SMMU)
		return IRQ_NONE;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	/* query whether need handle secure TF */
	ret = mtk_smmu_sec_tf_handler(smmu_type, &need_handle,
			&fault_iova, &fault_pa, &fault_id);
	if (ret)
		return IRQ_NONE;
#endif

#ifdef MTK_SMMU_DEBUG
	if (need_handle) {
		pr_info("[%s] smmu_%u, fault_iova:0x%lx, fault_pa:0x%lx, fault_id:0x%lx\n",
			__func__, smmu_type, fault_iova, fault_pa, fault_id);
		report_custom_smmu_fault(fault_iova, fault_pa, fault_id, smmu_type);
		return IRQ_HANDLED;
	}
#endif

	return IRQ_NONE;
}

static int mtk_smmu_irq_handler(int irq, void *dev)
{
	struct arm_smmu_device *smmu = dev;
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	u32 gerror, gerrorn, active;
	struct smmuv3_pmu_device *pmu_device;
	unsigned long flags;

	gerror = readl_relaxed(smmu->base + ARM_SMMU_GERROR);
	gerrorn = readl_relaxed(smmu->base + ARM_SMMU_GERRORN);

	active = gerror ^ gerrorn;
	if (!(active & GERROR_ERR_MASK)) {
		/* try to secure interrupt process which maybe trigger by secure */
		if (mtk_smmu_sec_irq_process(irq, dev) == IRQ_HANDLED) {
			mtk_smmu_irq_record(data);
			return IRQ_HANDLED;
		}
	} else {
		pr_info("[%s] irq:0x%x, gerror:0x%x, gerrorn:0x%x, active:0x%x\n",
			__func__, irq, gerror, gerrorn, active);
	}

	smmuwp_process_intr(smmu);

	mtk_smmu_irq_record(data);

	spin_lock_irqsave(&data->pmu_lock, flags);
	list_for_each_entry(pmu_device, &data->pmu_devices, node) {
		if (pmu_device->impl && pmu_device->impl->pmu_irq_handler)
			pmu_device->impl->pmu_irq_handler(irq, pmu_device->dev);
	}
	spin_unlock_irqrestore(&data->pmu_lock, flags);

	return IRQ_HANDLED;
}

static void mtk_smmu_evt_dump(struct arm_smmu_device *smmu, u64 *evt)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	char evt_str[SMMU_EVT_DUMP_LEN_MAX] = {};
	bool ssid_valid = (evt[0] & EVTQ_0_SSV) > 0;
	bool stall = (evt[1] & EVTQ_1_STALL) > 0;
	u32 sid = FIELD_GET(EVTQ_0_SID, evt[0]);
	char *ptr = evt_str;

	switch (FIELD_GET(EVTQ_0_ID, evt[0])) {
	case EVT_ID_TRANSLATION_FAULT:
		ptr += sprintf(ptr, "EVT_ID_TRANSLATION_FAULT");
		break;
	case EVT_ID_ADDR_SIZE_FAULT:
		ptr += sprintf(ptr, "EVT_ID_ADDR_SIZE_FAULT");
		break;
	case EVT_ID_ACCESS_FAULT:
		ptr += sprintf(ptr, "EVT_ID_ACCESS_FAULT");
		break;
	case EVT_ID_PERMISSION_FAULT:
		ptr += sprintf(ptr, "EVT_ID_PERMISSION_FAULT");
		break;
	default:
		ptr += sprintf(ptr, "EVT OTHER FAULT");
		break;
	}

	ptr += sprintf(ptr, ", sid:%u", sid);

	if (evt[1] & EVTQ_1_S2)
		ptr += sprintf(ptr, ", S2 trans");

	if (ssid_valid)
		ptr += sprintf(ptr, ", ssid:%llu", FIELD_GET(EVTQ_0_SSID, evt[0]));

	if (evt[1] & EVTQ_1_RnW)
		ptr += sprintf(ptr, ", RnW:Read");
	else
		ptr += sprintf(ptr, ", RnW:Write");

	if (evt[1] & EVTQ_1_InD)
		ptr += sprintf(ptr, ", Ind:Instruction");
	else
		ptr += sprintf(ptr, ", Ind:Data");

	if (evt[1] & EVTQ_1_PnU)
		ptr += sprintf(ptr, ", PnU:Privileged");
	else
		ptr += sprintf(ptr, ", PnU:Unprivileged");

	ptr += sprintf(ptr, ", Stall:%d", stall);
	if (stall)
		ptr += sprintf(ptr, ", STAG:0x%llx", FIELD_GET(EVTQ_1_STAG, evt[1]));

	ptr += sprintf(ptr, ", InputAddr:0x%llx", FIELD_GET(EVTQ_2_ADDR, evt[2]));
	ptr += sprintf(ptr, ", AlignedIPA:0x%llx", evt[3] & EVTQ_3_IPA);

	dev_info(smmu->dev, "smmu:%s EVTQ_INFO: %s\n",
		 get_smmu_name(data->plat_data->smmu_type), evt_str);
}

static int mtk_smmu_evt_handler(int irq, void *dev, u64 *evt)
{
	static DEFINE_RATELIMIT_STATE(evtq_rs, SMMU_FAULT_RS_INTERVAL,
				      SMMU_FAULT_RS_BURST);
	struct arm_smmu_device *smmu = dev;
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	u32 smmu_type = data->plat_data->smmu_type;
	u8 id = FIELD_GET(EVTQ_0_ID, evt[0]);
	bool ssid_valid = evt[0] & EVTQ_0_SSV;
	u32 sid = FIELD_GET(EVTQ_0_SID, evt[0]);
	u32 ssid = 0;

	if (!__ratelimit(&evtq_rs))
		return IRQ_HANDLED;

	if (ssid_valid)
		ssid = FIELD_GET(EVTQ_0_SSID, evt[0]);

	dev_info(smmu->dev,
		 "smmu:%s EVTQ_INFO: evt{[0]:0x%llx, [1]:0x%llx, [2]:0x%llx, [3]:0x%llx\n",
		 get_smmu_name(smmu_type), evt[0], evt[1], evt[2], evt[3]);

	dev_info(smmu->dev,
		 "smmu:%s EVTQ_INFO: irq:0x%x, id:0x%x, reason:%s, ssid_valid:%d, sid:0x%x, ssid:0x%x\n",
		 get_smmu_name(smmu_type),
		 irq, id, get_fault_reason_str(id),
		 ssid_valid, sid, ssid);

	mtk_smmu_evt_dump(smmu, evt);

	mtk_smmu_glbreg_dump(smmu);
	mtk_smmu_wpreg_dump(NULL, smmu_type);
	mtk_hyp_smmu_reg_dump(smmu);

	return IRQ_HANDLED;
}

static void mtk_smmu_glbreg_dump(struct arm_smmu_device *smmu)
{
	/* SMMU ID and control registers */
	dev_info(smmu->dev,
		 "IDR0:0x%x IDR1:0x%x IDR3:0x%x IDR5:0x%x CR0:[0x%x 0x%x] CR1:0x%x CR2:0x%x GBPA:0x%x GERROR:[0x%x 0x%x]\n",
		 readl_relaxed(smmu->base + ARM_SMMU_IDR0),
		 readl_relaxed(smmu->base + ARM_SMMU_IDR1),
		 readl_relaxed(smmu->base + ARM_SMMU_IDR3),
		 readl_relaxed(smmu->base + ARM_SMMU_IDR5),
		 readl_relaxed(smmu->base + ARM_SMMU_CR0),
		 readl_relaxed(smmu->base + ARM_SMMU_CR0ACK),
		 readl_relaxed(smmu->base + ARM_SMMU_CR1),
		 readl_relaxed(smmu->base + ARM_SMMU_CR2),
		 readl_relaxed(smmu->base + ARM_SMMU_GBPA),
		 readl_relaxed(smmu->base + ARM_SMMU_GERROR),
		 readl_relaxed(smmu->base + ARM_SMMU_GERRORN));

	/* SMMU stream table, IRQ, command queue and event queue registers */
	dev_info(smmu->dev,
		 "STRTAB:[0x%llx 0x%x] IRQ:[0x%x 0x%x] CMDQ:[0x%llx 0x%x 0x%x] EVTQ:[0x%llx 0x%x 0x%x]\n",
		 readq_relaxed(smmu->base + ARM_SMMU_STRTAB_BASE),
		 readl_relaxed(smmu->base + ARM_SMMU_STRTAB_BASE_CFG),
		 readl_relaxed(smmu->base + ARM_SMMU_IRQ_CTRL),
		 readl_relaxed(smmu->base + ARM_SMMU_IRQ_CTRLACK),
		 readq_relaxed(smmu->base + ARM_SMMU_CMDQ_BASE),
		 readl_relaxed(smmu->base + ARM_SMMU_CMDQ_PROD),
		 readl_relaxed(smmu->base + ARM_SMMU_CMDQ_CONS),
		 readq_relaxed(smmu->base + ARM_SMMU_EVTQ_BASE),
		 readl_relaxed(smmu->page1 + ARM_SMMU_EVTQ_PROD),
		 readl_relaxed(smmu->page1 + ARM_SMMU_EVTQ_CONS));
}

static void __maybe_unused smmu_dump_reg(void __iomem *base,
					 phys_addr_t ioaddr,
					 unsigned int start,
					 unsigned int len)
{
	unsigned int i = 0;

	pr_info("---- dump register base:0x%llx offset:0x%x ~ 0x%x ----\n",
		ioaddr, start, (start + 4 * (len - 1)));

	for (i = 0; i < len; i += 4) {
		if (len - i == 1)
			pr_info("0x%x=0x%x\n",
				start + 4 * i,
				smmu_read_reg(base, start + 4 * i));
		else if (len - i == 2)
			pr_info("0x%x=0x%x, 0x%x=0x%x\n",
				start + 4 * i,
				smmu_read_reg(base, start + 4 * i),
				start + 4 * (i + 1),
				smmu_read_reg(base, start + 4 * (i + 1)));
		else if (len - i == 3)
			pr_info("0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x\n",
				start + 4 * i,
				smmu_read_reg(base, start + 4 * i),
				start + 4 * (i + 1),
				smmu_read_reg(base, start + 4 * (i + 1)),
				start + 4 * (i + 2),
				smmu_read_reg(base, start + 4 * (i + 2)));
		else if (len - i >= 4)
			pr_info("0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x, 0x%x=0x%x\n",
				start + 4 * i,
				smmu_read_reg(base, start + 4 * i),
				start + 4 * (i + 1),
				smmu_read_reg(base, start + 4 * (i + 1)),
				start + 4 * (i + 2),
				smmu_read_reg(base, start + 4 * (i + 2)),
				start + 4 * (i + 3),
				smmu_read_reg(base, start + 4 * (i + 3)));
	}
}

/* for s2 translation fault and permission fault, hyp smmu return ipa's page table entry */
static void dump_s2_pg_table_info(uint64_t pte)
{
	uint8_t memattr, s2ap, sh, af, contig, xn;

	if (!pte)
		pr_info("[%s] %s ipa doesn't map in the stage 2 page table\n",
			HYP_SMMU_INFO_PREFIX, __func__);

	memattr = (pte >> 2) & 0xf;
	s2ap = (pte >> 6) & 0x3;
	sh = (pte >> 8) & 0x3;
	af = (pte >> 10) & 0x1;
	contig = (pte >> 52) & 0x1;
	xn = (pte >> 53) & 0x3;

	pr_info("[%s] %s memattr=0x%x, s2ap=0x%x, sh:0x%x, af=0x%x, contig=0x%x, xn:0x%x\n",
		HYP_SMMU_INFO_PREFIX, __func__, memattr, s2ap, sh, af, contig,
		xn);
}

bool hyp_smmu_debug_value_error(uint64_t hyp_smmu_ret_val)
{
	switch (hyp_smmu_ret_val) {
	case HYP_SMMU_INVALID_SID_BIT:
		pr_info("[%s] %s, HYP_SMMU_INVALID_SID_BIT\n",
			HYP_SMMU_INFO_PREFIX, __func__);
		break;
	case HYP_SMMU_INVALID_VMID_BIT:
		pr_info("[%s] %s, HYP_SMMU_INVALID_VMID_BIT\n",
			HYP_SMMU_INFO_PREFIX, __func__);
		break;
	case HYP_SMMU_INVALID_IPA_BIT:
		pr_info("[%s] %s, HYP_SMMU_INVALID_IPA_BIT\n",
			HYP_SMMU_INFO_PREFIX, __func__);
		break;
	case HYP_SMMU_INVALID_STE_ROW_BIT:
		pr_info("[%s] %s, HYP_SMMU_INVALID_STE_ROW_BIT\n",
			HYP_SMMU_INFO_PREFIX, __func__);
		break;
	case HYP_SMMU_INVALID_ACTION_ID_BIT:
		pr_info("[%s] %s, HYP_SMMU_INVALID_ACTION_ID_BIT\n",
			HYP_SMMU_INFO_PREFIX, __func__);
		break;
	case HYP_SMMU_INVALID_SMMU_TYPE_BIT:
		pr_info("[%s] %s, HYP_SMMU_INVALID_SMMU_TYPE_BIT\n",
			HYP_SMMU_INFO_PREFIX, __func__);
		break;
	default:
		return false;
	}
	return true;
}

static int hyp_smmu_debug_smc(u32 action_id, u32 ste_row, u64 reg,
			      u32 smmu_type, u32 sid, u32 fault_ipa_32,
			      uint64_t *ret_val)
{
	u32 debug_parameter;
	struct arm_smccc_res res;

	debug_parameter = HYP_PMM_SMMU_DEBUG_PARA(action_id, ste_row, reg,
						  smmu_type, sid);
	/* smc return value is res.a0, which size could be 64 bit */
	arm_smccc_smc(HYP_PMM_SMMU_CONTROL, fault_ipa_32, debug_parameter, 0, 0,
		      0, 0, 0, &res);
	if (hyp_smmu_debug_value_error(res.a0))
		return SMC_SMMU_FAIL;

	*ret_val = res.a0;
	return SMC_SMMU_SUCCESS;
}

static int mtk_hyp_smmu_debug_dump(struct arm_smmu_device *smmu, u64 fault_ipa,
				   u64 reg, u32 sid, u32 event_id, bool trans_s2)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	u32 smmu_type = data->plat_data->smmu_type;
	u32 fault_ipa_32, ste_row;
	uint64_t debug_info, sid_max = 1ULL << smmu->sid_bits;

	if (!MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_HYP_EN))
		return SMC_SMMU_SUCCESS;

	if (smmu_type >= SMMU_TYPE_NUM) {
		pr_info("[%s] %s, SMMU HW type is invalid, type:%u\n",
			HYP_SMMU_INFO_PREFIX, __func__, smmu_type);
		return SMC_SMMU_FAIL;
	}

	if (sid >= sid_max) {
		pr_info("[%s] %s, sid:%u is invalid, because max sid is %llu\n",
			HYP_SMMU_INFO_PREFIX, __func__, sid, sid_max);
		return SMC_SMMU_FAIL;
	}

	switch (event_id) {
	case EVENT_ID_BAD_STE_FAULT:
		/* dump global ste row 0 from hypvisor */
		ste_row = 0;
		if (hyp_smmu_debug_smc(HYP_SMMU_STE_DUMP, ste_row, 0, smmu_type,
				       sid, 0, &debug_info))
			return SMC_SMMU_FAIL;
		pr_info("[%s] %s ste_config=0x%llx, ste_valid=0x%llx\n",
			HYP_SMMU_INFO_PREFIX, __func__, (debug_info >> 1) & 0x7,
			debug_info & 0x1);
		break;
	case EVENT_ID_TRANSLATION_FAULT:
	case EVENT_ID_PERMISSION_FAULT:
		/* dump page table info */
		if (!trans_s2)
			break;
		fault_ipa_32 = MTK_SMMU_ADDR(fault_ipa);
		if (hyp_smmu_debug_smc(HYP_SMMU_TF_DUMP, 0, 0, smmu_type, sid,
				       fault_ipa_32, &debug_info))
			return SMC_SMMU_FAIL;
		dump_s2_pg_table_info(debug_info);
		break;
	case HYP_SMMU_REG_DUMP_EVT:
		/* dump smmu hw reg */
		if (hyp_smmu_debug_smc(HYP_SMMU_REG_DUMP, 0, reg, smmu_type,
				       sid, 0, &debug_info))
			return SMC_SMMU_FAIL;
		pr_info("[%s] %s smmu%u reg[%#llx]:%#llx\n",
			HYP_SMMU_INFO_PREFIX, __func__, smmu_type, reg,
			debug_info);
		break;
	case HYP_SMMU_GLOBAL_STE_DUMP_EVT:
		/* dump global ste row from hypvisor */
		for (ste_row = 0; ste_row < 8; ste_row++) {
			if (hyp_smmu_debug_smc(HYP_SMMU_STE_DUMP, ste_row, 0,
					       smmu_type, sid, 0, &debug_info))
				return SMC_SMMU_FAIL;
			pr_info("[%s] %s sid %u's STE[%u]:%#llx\n",
				HYP_SMMU_INFO_PREFIX, __func__, sid, ste_row,
				debug_info);
		}
		break;
	default:
		break;
	}
	return SMC_SMMU_SUCCESS;
}

/* dump smmu regs, which are controlled by host */
static void mtk_hyp_smmu_reg_dump(struct arm_smmu_device *smmu)
{
	mtk_hyp_smmu_debug_dump(smmu, 0, ARM_SMMU_CMDQ_PROD, 0,
				HYP_SMMU_REG_DUMP_EVT, 0);
	mtk_hyp_smmu_debug_dump(smmu, 0, ARM_SMMU_CMDQ_CONS, 0,
				HYP_SMMU_REG_DUMP_EVT, 0);
	mtk_hyp_smmu_debug_dump(smmu, 0, ARM_SMMU_CR0, 0,
				HYP_SMMU_REG_DUMP_EVT, 0);
	mtk_hyp_smmu_debug_dump(smmu, 0, ARM_SMMU_CR0ACK, 0,
				HYP_SMMU_REG_DUMP_EVT, 0);
	mtk_hyp_smmu_debug_dump(smmu, 0, ARM_SMMU_IDR1, 0,
				HYP_SMMU_REG_DUMP_EVT, 0);
	mtk_hyp_smmu_debug_dump(smmu, 0, ARM_SMMU_STRTAB_BASE, 0,
				HYP_SMMU_REG_DUMP_EVT, 0);
	mtk_hyp_smmu_debug_dump(smmu, 0, ARM_SMMU_CMDQ_BASE, 0,
				HYP_SMMU_REG_DUMP_EVT, 0);
}

static u64 smmu_iova_to_iopte(struct arm_smmu_master *master, u64 iova)
{
	if (!master || !master->domain || !master->domain->pgtbl_ops)
		return 0;

	return mtk_smmu_iova_to_iopte(master->domain->pgtbl_ops, iova);
}

static phys_addr_t smmu_iova_to_phys(struct arm_smmu_master *master, u64 iova)
{
	if (!master || !master->domain || !master->domain->pgtbl_ops)
		return 0;

	return master->domain->pgtbl_ops->iova_to_phys(master->domain->pgtbl_ops, iova);
}

static void mtk_smmu_fault_iova_dump(struct arm_smmu_master *master,
				     u64 fault_iova, u32 sid, u32 ssid)
{
	phys_addr_t fault_pa;
	u64 tf_iova_tmp;
	u64 tab_id = 0;
	u64 pte;
	int i;

	if (master == NULL)
		return;

	tab_id = get_smmu_tab_id_by_domain(&master->domain->domain);

	for (i = 0, tf_iova_tmp = fault_iova; i < SMMU_TF_IOVA_DUMP_NUM; i++) {
		if (i > 0)
			tf_iova_tmp -= SZ_4K;

		pte = smmu_iova_to_iopte(master, tf_iova_tmp);
		fault_pa = smmu_iova_to_phys(master, tf_iova_tmp);

		pr_info("error, tab_id:0x%llx, index:%d, pte:0x%llx, fault_iova:0x%llx, fault_pa:0x%llx\n",
			tab_id, i, pte, tf_iova_tmp, fault_pa);

		if (tf_iova_tmp == 0 || (!fault_pa && i > 0))
			break;
	}

#ifdef MTK_SMMU_DEBUG
	/* skip dump when fault iova = 0 */
	if (fault_iova) {
		/* For smmu iova dump, tab_id use smmu hardware id */
		mtk_iova_dump(fault_iova, smmu_tab_id_to_smmu_id(tab_id));
	}
#endif
}

static void mtk_smmu_fault_device_dump(struct arm_smmu_master *master,
				       u64 *evt, struct iommu_fault *flt)
{
	/* when master is NULL, mean invalid evt or unknown fault, no need dump */
	if (master == NULL)
		return;

	if (flt->type == IOMMU_FAULT_PAGE_REQ) {
		pr_info("[%s][page request fault] evt{[0]:0x%llx, [1]:0x%llx, [2]:0x%llx}, type:%d, prm{flags:0x%x, grpid:0x%x, perm:0x%x, addr:0x%llx, pasid:0x%x}\n",
			dev_name(master->dev), evt[0], evt[1], evt[2],
			flt->type, flt->prm.flags,
			flt->prm.grpid, flt->prm.perm,
			flt->prm.addr, flt->prm.pasid);
	} else if (flt->type == IOMMU_FAULT_DMA_UNRECOV) {
		pr_info("[%s][unrecoverable fault] evt{[0]:0x%llx, [1]:0x%llx, [2]:0x%llx}, type:%d, event{reason:0x%x, flags:0x%x, perm:0x%x, addr:0x%llx, pasid:0x%x}\n",
			dev_name(master->dev), evt[0], evt[1], evt[2],
			flt->type, flt->event.reason,
			flt->event.flags, flt->event.perm,
			flt->event.addr, flt->event.pasid);
	}
}

static void mtk_smmu_sid_dump(struct arm_smmu_device *smmu, u32 sid)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	u32 smmu_type = data->plat_data->smmu_type;
	int ret = 0;

	if (!MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_SEC_EN))
		return;

#if IS_ENABLED(CONFIG_MTK_IOMMU_MISC_SECURE)
	ret = mtk_smmu_dump_sid(smmu_type, sid);
	if (ret)
		pr_info("[%s] smmu_%u fail\n", __func__, smmu_type);
#endif
}

static int mtk_report_device_fault(struct arm_smmu_device *smmu,
				   struct arm_smmu_master *master,
				   u64 *evt,
				   struct iommu_fault_event *fault_evt)
{
	static DEFINE_RATELIMIT_STATE(fault_rs, SMMU_FAULT_RS_INTERVAL,
				      SMMU_FAULT_RS_BURST);
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	struct mtk_iommu_fault_event mtk_fault_evt = { };
	struct mtk_smmu_fault_param *fault_param = NULL;
	u32 smmu_type = data->plat_data->smmu_type;
	struct dev_iommu *param;
	struct iommu_fault *flt;
	u64 fault_iova, fault_ipa;
	bool unknown_evt = false;
	int id, ret = 0;
	bool ssid_valid;
	u32 sid, ssid;
	u64 s2_trans;

	/* limit TF handle dump rate */
	if (!__ratelimit(&fault_rs)) {
		smmuwp_clear_tf(smmu);
		return 0;
	}

	mtk_fault_evt.fault_evt.fault = fault_evt->fault;
	mtk_fault_evt.fault_evt.list = fault_evt->list;
	flt = &mtk_fault_evt.fault_evt.fault;

	fault_iova = FIELD_GET(EVTQ_2_ADDR, evt[2]);
	fault_ipa = evt[3] & EVTQ_3_IPA;
	ssid_valid = evt[0] & EVTQ_0_SSV;
	sid = FIELD_GET(EVTQ_0_SID, evt[0]);
	ssid = FIELD_GET(EVTQ_0_SSID, evt[0]);
	id = FIELD_GET(EVTQ_0_ID, evt[0]);
	s2_trans = evt[1] & EVTQ_1_S2;

	fault_param = smmuwp_process_tf(smmu, master, &mtk_fault_evt);

	/**
	 * when master is NULL, mean invalid evt content or unknown fault id,
	 * still need to report fault by wrapper register
	 */
	if (master == NULL) {
		/* No TRANSLATION FAULT detected in wrapper register */
		if (fault_param == NULL) {
			smmuwp_clear_tf(smmu);
			return 0;
		}

		unknown_evt = true;
		fault_iova = fault_param->fault_iova;
		sid = fault_param->fault_sid;
		ssid = fault_param->fault_ssid;
		ssid_valid = fault_param->fault_ssidv;

		/* can't get: fault_ipa, id, s2_trans */

		mutex_lock(&smmu->streams_mutex);
		master = arm_smmu_find_master(smmu, sid);
		if (master != NULL)
			fault_param->fault_pa = smmu_iova_to_phys(master, fault_iova);
	}

	mtk_smmu_ste_cd_info_dump(NULL, smmu_type, sid);
	mtk_hyp_smmu_debug_dump(smmu, 0, 0, sid, HYP_SMMU_GLOBAL_STE_DUMP_EVT, 0);

	dev_info(smmu->dev,
		 "[%s] smmu:%s, master:%s, fault_iova=0x%llx, fault_ipa=0x%llx, sid=0x%x, ssid=0x%x, ssid_valid:0x%x, id:0x%x, reason:%s, s2_trans:0x%llx\n",
		 __func__, get_smmu_name(smmu_type),
		 (master != NULL ? dev_name(master->dev) : "NULL"),
		 fault_iova, fault_ipa, sid, ssid, ssid_valid, id,
		 get_fault_reason_str(id), s2_trans);

	mtk_smmu_fault_device_dump(master, evt, flt);

	mtk_smmu_fault_iova_dump(master, fault_iova, sid, ssid);

	mtk_smmu_sid_dump(smmu, sid);

	mtk_hyp_smmu_debug_dump(smmu, fault_ipa, 0, sid, id, s2_trans);

	/* Report fault event to master device driver */
	if (master != NULL) {
		ret = iommu_report_device_fault(master->dev, &mtk_fault_evt.fault_evt);
		if (ret) {
			param = master->dev->iommu;
			if (!param || !param->fault_param || !param->fault_param->handler) {
				dev_dbg(smmu->dev,
					"[%s] ret:%d, dev:%s not register device fault handler",
					__func__, ret, dev_name(master->dev));
			}
		}
	}

#ifdef MTK_SMMU_DEBUG
	if (fault_param != NULL) {
		report_custom_smmu_fault(fault_param->fault_iova,
					 fault_param->fault_pa,
					 fault_param->fault_id,
					 smmu_type);
	}
#endif

	if (unknown_evt)
		mutex_unlock(&smmu->streams_mutex);

	smmuwp_clear_tf(smmu);

	return 0;
}

static void smmu_debug_dump(struct arm_smmu_device *smmu, bool check_pm,
			    bool ratelimit)
{
#ifdef MTK_SMMU_DEBUG
	static DEFINE_RATELIMIT_STATE(debug_rs, SMMU_FAULT_RS_INTERVAL,
				      SMMU_FAULT_RS_BURST);
	struct mtk_smmu_data *data;
	u32 type;
	int ret;

	if (!smmu)
		return;

	if (ratelimit && !__ratelimit(&debug_rs))
		return;

	data = to_mtk_smmu_data(smmu);
	type = data->plat_data->smmu_type;
	if (data->hw_init_flag != 1) {
		pr_info("[%s] smmu:%s hw not init\n", __func__, get_smmu_name(type));
		return;
	}

	dev_info(smmu->dev, "[%s] smmu:%s, check_pm:%d\n",
		 __func__, get_smmu_name(type), check_pm);

	if (check_pm) {
		ret = mtk_smmu_power_get(smmu);
		if (ret) {
			dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
			return;
		}
	}

	mtk_smmu_glbreg_dump(smmu);
	mtk_smmu_wpreg_dump(NULL, type);

	smmuwp_dump_outstanding_monitor(smmu);
	smmuwp_dump_io_interface_signals(smmu);
	mtk_hyp_smmu_reg_dump(smmu);

	if (check_pm)
		mtk_smmu_power_put(smmu);
#endif
}

static void smi_debug_dump(struct arm_smmu_device *smmu)
{
#ifdef MTK_SMMU_DEBUG
	struct mtk_smmu_data *data;
	void __iomem *base;
	phys_addr_t ioaddr;
	int i;

	if (!smmu)
		return;

	data = to_mtk_smmu_data(smmu);
	if (data->smi_com_base_cnt == 0 || data->smi_com_base == NULL)
		return;

	for (i = 0; i < data->smi_com_base_cnt; i++) {
		ioaddr = data->smi_com_base[i];
		if (ioaddr == 0)
			continue;

		base = ioremap(ioaddr, SMI_COM_REGS_SZ);
		if (IS_ERR_OR_NULL(base)) {
			dev_info(smmu->dev, "%s, failed to map smi_com_base:0x%llx\n",
				 __func__, ioaddr);
			continue;
		}

		smmu_dump_reg(base, ioaddr, SMI_COM_OFFSET1_START, SMI_COM_OFFSET1_LEN);
		smmu_dump_reg(base, ioaddr, SMI_COM_OFFSET2_START, SMI_COM_OFFSET2_LEN);
		smmu_dump_reg(base, ioaddr, SMI_COM_OFFSET3_START, SMI_COM_OFFSET3_LEN);

		iounmap(base);
	}
#endif
}

static void mtk_smmu_fault_dump(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);

	smmu_debug_dump(smmu, false, false);

	if (data->plat_data->smmu_type == MM_SMMU) {
		mtk_smi_dbg_hang_detect("iommu");
		smi_debug_dump(smmu);
	}
}

static bool mtk_smmu_skip_shutdown(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	bool skip_shutdown;

	skip_shutdown = MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_SKIP_SHUTDOWN);

	return skip_shutdown;
}

static bool mtk_smmu_skip_sync_timeout(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	bool ret;

	ret = data->plat_data->smmu_type == MM_SMMU;

	return ret;
}

static const struct arm_smmu_impl mtk_smmu_impl = {
	.device_group = mtk_smmu_device_group,
	.delay_hw_init = mtk_delay_hw_init,
	.smmu_hw_init = mtk_smmu_hw_init,
	.smmu_hw_deinit = mtk_smmu_hw_deinit,
	.smmu_hw_sec_init = mtk_smmu_hw_sec_init,
	.smmu_power_get = mtk_smmu_power_get,
	.smmu_power_put = mtk_smmu_power_put,
	.smmu_runtime_suspend = mtk_smmu_runtime_suspend,
	.smmu_runtime_resume = mtk_smmu_runtime_resume,
	.smmu_setup_features = mtk_smmu_setup_features,
	.get_resv_regions = mtk_get_resv_regions,
	.smmu_irq_handler = mtk_smmu_irq_handler,
	.smmu_evt_handler = mtk_smmu_evt_handler,
	.report_device_fault = mtk_report_device_fault,
	.map_pages = mtk_smmu_map_pages,
	.iotlb_sync_map = mtk_iotlb_sync_map,
	.iotlb_sync = mtk_iotlb_sync,
	.tlb_flush = mtk_tlb_flush,
	.def_domain_type = mtk_smmu_def_domain_type,
	.dev_has_feature = mtk_smmu_dev_has_feature,
	.dev_feature_enabled = mtk_smmu_dev_feature_enabled,
	.dev_enable_feature = mtk_smmu_dev_enable_feature,
	.dev_disable_feature = mtk_smmu_dev_disable_feature,
	.fault_dump = mtk_smmu_fault_dump,
	.skip_shutdown = mtk_smmu_skip_shutdown,
	.skip_sync_timeout = mtk_smmu_skip_sync_timeout,
};

static void mtk_smmu_dbg_hang_detect(enum mtk_smmu_type type)
{
	struct mtk_smmu_data *data = mkt_get_smmu_data(type);

	if (!data)
		return;

	smmu_debug_dump(&data->smmu, true, true);
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
static int mtk_smmu_dbg_hang_cb(struct notifier_block *nb,
				unsigned long action, void *data)
{
	if (!IS_ERR_OR_NULL(data) && strcmp((char *) data, "iommu") == 0)
		return NOTIFY_DONE;

	mtk_smmu_dbg_hang_detect(MM_SMMU);
	return NOTIFY_OK;
}

static int register_dbg_notifier;
static struct notifier_block mtk_smmu_dbg_hang_nb = {
		.notifier_call = mtk_smmu_dbg_hang_cb,
};
#endif

static const struct mtk_smmu_ops mtk_smmu_dbg_ops = {
	.get_smmu_data		= mkt_get_smmu_data,
	.get_cd_ptr		= arm_smmu_get_cd_ptr,
	.get_step_ptr		= arm_smmu_get_step_for_sid,
	.smmu_power_get		= mtk_smmu_power_get,
	.smmu_power_put		= mtk_smmu_power_put,
};

static void mtk_smmu_parse_driver_properties(struct mtk_smmu_data *data)
{
	struct arm_smmu_device *smmu = &data->smmu;
	u32 prefetch;
	int ret;

	/* parse tcu prefetch config */
	ret = of_property_read_u32(smmu->dev->of_node, "tcu-prefetch", &prefetch);
	if (!ret) {
		smmu->features |= ARM_SMMU_FEAT_TCU_PF;
		data->tcu_prefetch = prefetch;
		dev_info(smmu->dev, "parse tcu-prefetch:%d\n", prefetch);
	}

	if (of_property_read_bool(smmu->dev->of_node, "mediatek,axslc"))
		data->axslc = true;

	data->smi_com_base = kcalloc(SMI_COM_REGS_NUM, sizeof(u32), GFP_KERNEL);
	ret = of_property_read_u32_array(smmu->dev->of_node, "smi-common-base",
					 data->smi_com_base, SMI_COM_REGS_NUM);
	if (!ret)
		data->smi_com_base_cnt = SMI_COM_REGS_NUM;
}

static int mtk_smmu_config_translation(struct mtk_smmu_data *data)
{
	struct device *dev = data->smmu.dev;
	unsigned int trans;
	int ret;

	ret = of_property_read_u32(dev->of_node, "mtk,smmu-trans-type", &trans);
	if (ret)
		trans = SMMU_TRANS_MIX;

	switch (trans) {
	case SMMU_TRANS_S1:
	case SMMU_TRANS_S2:
	case SMMU_TRANS_MIX:
	case SMMU_TRANS_BYPASS:
	case SMMU_TRANS_WP_BYPASS:
		data->smmu_trans_type = trans;
		ret = 0;
		break;
	default:
		ret = -1;
		break;
	}

	pr_info("[%s] smmu:%s, trans:0x%x, ret:%d\n",
		__func__, get_smmu_name(data->plat_data->smmu_type), trans, ret);

	return ret;
}

static void populate_iommu_groups(struct mtk_smmu_data *data,
				  struct device_node *smmu_node)
{
	struct device_node *groups_node, *child_part;
	int group_idx = 0;
	int nr_groups;

	if (WARN_ON(data->iommu_groups))
		return;
	groups_node = of_get_child_by_name(smmu_node, "iommu-groups");
	if (!groups_node)
		return;

	nr_groups = of_get_child_count(groups_node);

	dev_info(data->smmu.dev, "[%s] groups_node full_name:%s, nr_groups:%d\n",
		 __func__, groups_node->name, nr_groups);

	if (!nr_groups)
		goto out_put_node;

	data->iommu_groups = kcalloc(nr_groups, sizeof(*data->iommu_groups),
				     GFP_KERNEL);
	data->iommu_group_nr = nr_groups;
	if (WARN_ON(!data->iommu_groups))
		goto out_put_node;

	for_each_child_of_node(groups_node, child_part) {
		struct iommu_group_data *group_data;
		const char *str;

		group_data = &data->iommu_groups[group_idx];
		group_data->group_id = &child_part->fwnode;
		group_data->domain_type = IOMMU_DOMAIN_DMA;
		group_data->smmu_type = data->plat_data->smmu_type;
		group_data->iommu_group = iommu_group_alloc();

		if (of_property_read_string(child_part, "mtk,smmu-mode", &str))
			str = "default";
		if (!strcmp(str, "dma"))
			group_data->domain_type = IOMMU_DOMAIN_DMA;
		if (!strcmp(str, "bypass"))
			group_data->domain_type = IOMMU_DOMAIN_IDENTITY;

		if (of_property_read_string(child_part, "label", &str))
			str = "default";
		group_data->label = str;
		group_idx++;

		pr_info("%s, group label=%s, id=%pa\n",
			__func__, group_data->label, group_data->group_id);

		dev_info(data->smmu.dev,
			 "[%s], group label=%s, id=%pa, domain_type=0x%x, smmu_type=%d\n",
			 __func__, group_data->label, group_data->group_id,
			 group_data->domain_type, group_data->smmu_type);
	}

out_put_node:
	of_node_put(groups_node);
}

static int mtk_smmu_data_init(struct mtk_smmu_data *data)
{
	const struct mtk_smmu_plat_data *plat_data;
	struct device *dev = data->smmu.dev;

	mutex_init(&data->group_mutexs);
	spin_lock_init(&data->pmu_lock);
	data->plat_data = of_device_get_plat_data(data->smmu.dev);
	plat_data = data->plat_data;

	if (data->plat_data == NULL) {
		dev_info(dev, "%s, plat_data NULL\n", __func__);
		return -ENODEV;
	}

	dev_info(dev, "[%s] plat_data{smmu_plat:%d, flags:0x%x, smmu_type:%d}\n",
		 __func__, data->plat_data->smmu_plat, data->plat_data->flags,
		 data->plat_data->smmu_type);

	mtk_smmu_parse_driver_properties(data);

	mtk_smmu_config_translation(data);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MTK_SMI)
	if (MTK_SMMU_HAS_FLAG(data->plat_data, SMMU_HANG_DETECT)) {
		if (register_dbg_notifier != 1) {
			mtk_smi_dbg_register_notifier(&mtk_smmu_dbg_hang_nb);
			register_dbg_notifier = 1;
		}
	}
#endif

	populate_iommu_groups(data, dev->of_node);

	mtk_smmu_irq_pause_timer_init(data);

	data->hw_init_flag = 0;

	return 0;
}

static struct arm_smmu_device *mtk_smmu_create(struct arm_smmu_device *smmu,
					       const struct arm_smmu_impl *impl)
{
	struct mtk_smmu_data *data;

	if (!smmu)
		return ERR_PTR(-ENOMEM);

	data = devm_kzalloc(smmu->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	data->smmu.dev = smmu->dev;
	data->smmu.base = smmu->base;
	data->smmu.page1 = smmu->page1;
	data->smmu.wp_base = smmu->wp_base;
	data->smmu.features = smmu->features;
	data->smmu.impl = impl;
	INIT_LIST_HEAD(&data->pmu_devices);

	mtk_smmu_data_init(data);

	devm_kfree(smmu->dev, smmu);

	mtk_smmu_datas[data->plat_data->smmu_type] = data;

#ifdef MTK_SMMU_DEBUG
	mtk_smmu_set_ops(&mtk_smmu_dbg_ops);
#endif

	return &data->smmu;
}

struct arm_smmu_device *mtk_smmu_v3_impl_init(struct arm_smmu_device *smmu)
{
	return mtk_smmu_create(smmu, &mtk_smmu_impl);
}

static const struct mtk_smmu_plat_data mt6989_data_mm = {
	.smmu_plat		= SMMU_MT6989,
	.smmu_type		= MM_SMMU,
	.flags			= SMMU_DELAY_HW_INIT | SMMU_SEC_EN | SMMU_HYP_EN |
				  SMMU_HANG_DETECT,
};

static const struct mtk_smmu_plat_data mt6989_data_apu = {
	.smmu_plat		= SMMU_MT6989,
	.smmu_type		= APU_SMMU,
	.flags			= SMMU_DELAY_HW_INIT | SMMU_SEC_EN | SMMU_HYP_EN |
				  SMMU_SKIP_SHUTDOWN,
};

static const struct mtk_smmu_plat_data mt6989_data_soc = {
	.smmu_plat		= SMMU_MT6989,
	.smmu_type		= SOC_SMMU,
	.flags			= SMMU_SEC_EN | SMMU_CLK_AO_EN | SMMU_HYP_EN,
};

static const struct mtk_smmu_plat_data mt6989_data_gpu = {
	.smmu_plat		= SMMU_MT6989,
	.smmu_type		= GPU_SMMU,
	.flags			= SMMU_DELAY_HW_INIT | SMMU_HYP_EN | SMMU_DIS_CPU_PARTID,
};

static const struct mtk_smmu_plat_data *of_device_get_plat_data(struct device *dev)
{
	const struct device_node *np = dev->of_node;

	if (of_device_is_compatible(np, "mediatek,mt6989-mm-smmu"))
		return &mt6989_data_mm;
	else if (of_device_is_compatible(np, "mediatek,mt6989-apu-smmu"))
		return &mt6989_data_apu;
	else if (of_device_is_compatible(np, "mediatek,mt6989-soc-smmu"))
		return &mt6989_data_soc;
	else if (of_device_is_compatible(np, "mediatek,mt6989-gpu-smmu"))
		return &mt6989_data_gpu;

	return NULL;
}

//=====================================================
//SMMU wrapper register process and dump
//=====================================================
static unsigned int smmuwp_consume_intr(struct arm_smmu_device *smmu,
					unsigned int irq_bit)
{
	void __iomem *wp_base = smmu->wp_base;
	unsigned int pend_cnt = 0;

	pend_cnt = smmu_read_reg(wp_base, SMMUWP_IRQ_NS_CNTx(__ffs(irq_bit)));
	smmu_write_field(wp_base, SMMUWP_IRQ_NS_ACK_CNT, IRQ_NS_ACK_CNT_MSK,
			 pend_cnt);
	smmu_write_reg(wp_base, SMMUWP_IRQ_NS_ACK, irq_bit);

	return pend_cnt;
}

/* clear translation fault mark */
static void smmuwp_clear_tf(struct arm_smmu_device *smmu)
{
	void __iomem *wp_base = smmu->wp_base;

	smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_ABT_CNT_CLR,
			 CTL0_ABT_CNT_CLR);
	smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_ABT_CNT_CLR, 0);
}

static unsigned int smmuwp_process_intr(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	unsigned int irq_sta = 0, pend_cnt = 0, i = 0;
	void __iomem *wp_base = smmu->wp_base;

	irq_sta = smmu_read_reg(wp_base, SMMUWP_IRQ_NS_STA);

	if (irq_sta > 0) {
		pr_info("%s smmu:%s irq_sta(0x%x)=0x%x\n",
			__func__, get_smmu_name(data->plat_data->smmu_type),
			SMMUWP_IRQ_NS_STA, irq_sta);
	}

	if (irq_sta & STA_NS_TCU_GLB_INTR) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_NS_TCU_GLB_INTR);
		pr_info("Non-secure TCU global interrupt detected %d\n", pend_cnt);
	}

	if (irq_sta & STA_NS_TCU_CMD_SYNC_INTR) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_NS_TCU_CMD_SYNC_INTR);
		pr_info("Non-secure TCU CMD_SYNC interrupt detected %d\n", pend_cnt);
	}

	if (irq_sta & STA_NS_TCU_EVTQ_INTR) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_NS_TCU_EVTQ_INTR);
		pr_info("Non-secure TCU event queue interrupt detected %d\n",
			pend_cnt);
	}

	if (irq_sta & STA_TCU_PRI_INTR) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_TCU_PRI_INTR);
		pr_info("TCU PRI interrupt detected %d\n", pend_cnt);
	}

	if (irq_sta & STA_TCU_PMU_INTR) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_TCU_PMU_INTR);
		pr_info("TCU PMU interrupt detected %d\n", pend_cnt);
	}

	if (irq_sta & STA_TCU_RAS_CRI) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_TCU_RAS_CRI);
		pr_info("TCU RAS CRI detected %d\n", pend_cnt);
	}

	if (irq_sta & STA_TCU_RAS_ERI) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_TCU_RAS_ERI);
		pr_info("TCU RAS ERI detected %d\n", pend_cnt);
	}

	if (irq_sta & STA_TCU_RAS_FHI) {
		pend_cnt = smmuwp_consume_intr(smmu, STA_TCU_RAS_FHI);
		pr_info("TCU RAS FHI detected %d\n", pend_cnt);
	}

	for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
		if (irq_sta & STA_TBUx_RAS_CRI(i)) {
			pend_cnt = smmuwp_consume_intr(smmu, STA_TBUx_RAS_CRI(i));
			pr_info("TBU%d RAS CRI detected %d\n", i, pend_cnt);
		}

		if (irq_sta & STA_TBUx_RAS_ERI(i)) {
			pend_cnt = smmuwp_consume_intr(smmu, STA_TBUx_RAS_ERI(i));
			pr_info("TBU%d RAS ERI detected %d\n", i, pend_cnt);
		}

		if (irq_sta & STA_TBUx_RAS_FHI(i)) {
			pend_cnt = smmuwp_consume_intr(smmu, STA_TBUx_RAS_FHI(i));
			pr_info("TBU%d RAS FHI detected %d\n", i, pend_cnt);
		}
	}

	return irq_sta;
}

static u32 smmuwp_fault_id(u32 axi_id, u32 tbu_id)
{
	u32 fault_id = (axi_id & ~SMMUWP_TF_TBU_MSK) | (SMMUWP_TF_TBU(tbu_id));

	return fault_id;
}

/* Process TBU translation fault Monitor */
static struct mtk_smmu_fault_param *smmuwp_process_tf(
		struct arm_smmu_device *smmu,
		struct arm_smmu_master *master,
		struct mtk_iommu_fault_event *fault_evt)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	struct mtk_smmu_fault_param *first_fault_param = NULL;
	void __iomem *wp_base = smmu->wp_base;
	unsigned int sid, ssid, secsidv, ssidv;
	u32 i, regval, va35_32, axiid, fault_id;
	u64 fault_iova, fault_pa;
	bool tf_det = false;

	for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_RTFM0(i));
		if (!(regval & RTFM0_FAULT_DET))
			goto write;

		tf_det = true;
		axiid = FIELD_GET(RTFM0_FAULT_AXI_ID, regval);
		fault_id = smmuwp_fault_id(axiid, i);

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_RTFM1(i));
		va35_32 = FIELD_GET(RTFM1_FAULT_VA_35_32, regval);
		fault_iova = (u64)(regval & RTFM1_FAULT_VA_31_12);
		fault_iova |= (u64)va35_32 << 32;
		fault_pa = smmu_iova_to_phys(master, fault_iova);

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_RTFM2(i));
		sid = FIELD_GET(RTFM2_FAULT_SID, regval);
		ssid = FIELD_GET(RTFM2_FAULT_SSID, regval);
		ssidv = FIELD_GET(RTFM2_FAULT_SSIDV, regval);
		secsidv = FIELD_GET(RTFM2_FAULT_SECSID, regval);
		pr_info("TBU%d %s TF read, iova:0x%llx, fault_id:0x%x, sid:%d, ssid:%d, ssidv:%d, secsidv:%d\n",
			i, STRSEC(secsidv), fault_iova, axiid, sid, ssid, ssidv, secsidv);

		fault_evt->mtk_fault_param[SMMU_TFM_READ][i] = (struct mtk_smmu_fault_param) {
			.tfm_type = SMMU_TFM_READ,
			.fault_id = fault_id,
			.fault_det = tf_det,
			.fault_iova = fault_iova,
			.fault_pa = fault_pa,
			.fault_sid = sid,
			.fault_ssid = ssid,
			.fault_ssidv = ssidv,
			.fault_secsid = secsidv,
			.tbu_id = i,
		};

		if (first_fault_param == NULL)
			first_fault_param = &fault_evt->mtk_fault_param[SMMU_TFM_READ][i];

write:
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_WTFM0(i));
		if (!(regval & WTFM0_FAULT_DET))
			continue;

		tf_det = true;
		axiid = FIELD_GET(WTFM0_FAULT_AXI_ID, regval);
		fault_id = smmuwp_fault_id(axiid, i);

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_WTFM1(i));
		va35_32 = FIELD_GET(WTFM1_FAULT_VA_35_32, regval);
		fault_iova = (u64)(regval & RTFM1_FAULT_VA_31_12);
		fault_iova |= (u64)va35_32 << 32;
		fault_pa = smmu_iova_to_phys(master, fault_iova);

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_WTFM2(i));
		sid = FIELD_GET(WTFM2_FAULT_SID, regval);
		ssid = FIELD_GET(WTFM2_FAULT_SSID, regval);
		ssidv = FIELD_GET(WTFM2_FAULT_SSIDV, regval);
		secsidv = FIELD_GET(WTFM2_FAULT_SECSID, regval);
		pr_info("TBU%d %s TF write, iova:0x%llx, fault_id:0x%x, sid:%d, ssid:%d, ssidv:%d, secsidv:%d\n",
			i, STRSEC(secsidv), fault_iova, axiid, sid, ssid, ssidv, secsidv);

		fault_evt->mtk_fault_param[SMMU_TFM_WRITE][i] = (struct mtk_smmu_fault_param) {
			.tfm_type = SMMU_TFM_WRITE,
			.fault_id = fault_id,
			.fault_det = tf_det,
			.fault_iova = fault_iova,
			.fault_pa = fault_pa,
			.fault_sid = sid,
			.fault_ssid = ssid,
			.fault_ssidv = ssidv,
			.fault_secsid = secsidv,
			.tbu_id = i,
		};

		if (first_fault_param == NULL)
			first_fault_param = &fault_evt->mtk_fault_param[SMMU_TFM_WRITE][i];
	}

	if (!tf_det)
		dev_info(smmu->dev, "[%s] No TF detected or has been cleaned\n", __func__);

	return first_fault_param;
}

static int __maybe_unused smmuwp_tf_detect(struct arm_smmu_device *smmu,
					   u32 sid, u32 tbu,
					   u32 *axids, u32 num_axids,
					   struct mtk_smmu_fault_param *param)
{
	struct mtk_iommu_fault_event fault_evt = { };
	struct mtk_smmu_fault_param *fault_param;
	enum mtk_smmu_tfm_type tfm_type;
	u32 axid, i;

	fault_param = smmuwp_process_tf(smmu, NULL, &fault_evt);
	if (fault_param == NULL)
		return 0;

	if (tbu >= SMMU_TBU_CNT_MAX)
		return 0;

	for (tfm_type = SMMU_TFM_READ; tfm_type < SMMU_TFM_TYPE_NUM; tfm_type++) {
		fault_param = &fault_evt.mtk_fault_param[tfm_type][tbu];
		if (fault_param &&
		    fault_param->fault_det &&
		    fault_param->fault_sid == sid) {
			axid = fault_param->fault_id & ~SMMUWP_TF_TBU_MSK;
			for (i = 0; i < num_axids; i++) {
				if (axid == axids[i]) {
					/* param return match data */
					param->tfm_type = fault_param->tfm_type;
					param->fault_det = fault_param->fault_det;
					param->fault_iova = fault_param->fault_iova;
					param->fault_pa = fault_param->fault_pa;
					param->fault_id = fault_param->fault_id;
					param->fault_sid = fault_param->fault_sid;
					param->fault_ssid = fault_param->fault_ssid;
					param->fault_ssidv = fault_param->fault_ssidv;
					param->fault_secsid = fault_param->fault_secsid;
					param->tbu_id = fault_param->tbu_id;
					return 1;
				}
			}
		}
	}

	return 0;
}

static void smmuwp_check_transaction_counter(struct arm_smmu_device *smmu,
					     unsigned long *tcu_tot,
					     unsigned long *tbu_tot)
{
	unsigned int i, tcu_send_rcmd, tcu_send_wcmd, tcu_recv_dvmcmd;
	unsigned int tbu_rcmd_trans[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wcmd_trans[SMMU_TBU_CNT_MAX];
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;

	*tcu_tot = 0;
	tcu_send_rcmd = smmu_read_reg(wp_base, SMMUWP_TCU_MON5);
	tcu_send_wcmd = smmu_read_reg(wp_base, SMMUWP_TCU_MON6);
	tcu_recv_dvmcmd = smmu_read_reg(wp_base, SMMUWP_TCU_MON7);
	*tcu_tot += tcu_send_rcmd + tcu_send_wcmd + tcu_recv_dvmcmd;

	*tbu_tot = 0;
	for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
		tbu_rcmd_trans[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON7(i));
		tbu_wcmd_trans[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON8(i));
		*tbu_tot += tbu_rcmd_trans[i] + tbu_wcmd_trans[i];
	}

	if (*tcu_tot > 0 || *tbu_tot > 0) {
		pr_info("---- %s start ----\n", __func__);
		pr_info("\tTCU send read cmd total(0x%x):%u, write cmd total(0x%x):%u, receive DVM cmd total(0x%x):%u\n",
			SMMUWP_TCU_MON5, tcu_send_rcmd, SMMUWP_TCU_MON6, tcu_send_wcmd,
			SMMUWP_TCU_MON7, tcu_recv_dvmcmd);
		for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++)
			pr_info("\tTBU%u read cmd trans total(0x%x):%u, write cmd trans total(0x%x):%u\n",
				i, SMMUWP_TBUx_MON7(i), tbu_rcmd_trans[i], SMMUWP_TBUx_MON8(i),
				tbu_wcmd_trans[i]);
		pr_info("---- %s end, tcu_tot:%lu, tbu_tot:%lu ----\n",
			__func__, *tcu_tot, *tbu_tot);
	}
}

/* TCU only support measure read command, donot support write command */
static void smmuwp_check_latency_counter(struct arm_smmu_device *smmu,
					 unsigned int *maxlat_axiid,
					 unsigned long *tcu_rlat_tots,
					 unsigned long *tbu_lat_tots,
					 unsigned long *oos_trans_tot)
{
	unsigned int i, regval, tcu_lat_max, tcu_pend_max, tbu_maxlat = 0;
	unsigned int tcu_lat_tot, tcu_trans_tot, tcu_oos_trans_tot, tcu_avg_lat;
	unsigned int tbu_rlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_id_rlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_id_wlat_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_rpend_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wpend_max[SMMU_TBU_CNT_MAX];
	unsigned int tbu_rlat_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wlat_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_rtrans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_wtrans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_roos_trans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_woos_trans_tot[SMMU_TBU_CNT_MAX];
	unsigned int tbu_avg_rlat[SMMU_TBU_CNT_MAX];
	unsigned int tbu_avg_wlat[SMMU_TBU_CNT_MAX];
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;

	regval = smmu_read_reg(wp_base, SMMUWP_TCU_MON1);
	tcu_lat_max = FIELD_GET(TCU_LAT_MAX, regval);
	tcu_pend_max = FIELD_GET(TCU_PEND_MAX, regval);
	tcu_lat_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON2);
	tcu_trans_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON3);
	tcu_oos_trans_tot = smmu_read_reg(wp_base, SMMUWP_TCU_MON4);
	tcu_avg_lat = tcu_trans_tot > 0 ? tcu_lat_tot/tcu_trans_tot : 0;
	*tcu_rlat_tots = tcu_lat_tot;
	*oos_trans_tot = tcu_oos_trans_tot;

	*tbu_lat_tots = 0;
	*maxlat_axiid = 0;
	for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON1(i));
		tbu_rlat_max[i] = FIELD_GET(MON1_RLAT_MAX, regval);
		tbu_wlat_max[i] = FIELD_GET(MON1_WLAT_MAX, regval);
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON2(i));
		tbu_id_rlat_max[i] = FIELD_GET(MON2_ID_RLAT_MAX, regval);
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON3(i));
		tbu_id_wlat_max[i] = FIELD_GET(MON3_ID_WLAT_MAX, regval);
		if (tbu_rlat_max[i] > tbu_maxlat) {
			tbu_maxlat = tbu_rlat_max[i];
			*maxlat_axiid = tbu_id_rlat_max[i];
		}
		if (tbu_wlat_max[i] > tbu_maxlat) {
			tbu_maxlat = tbu_wlat_max[i];
			*maxlat_axiid = tbu_id_wlat_max[i];
		}
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_MON4(i));
		tbu_rpend_max[i] = FIELD_GET(MON4_RPEND_MAX, regval);
		tbu_wpend_max[i] = FIELD_GET(MON4_WPEND_MAX, regval);
		tbu_rlat_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON5(i));
		tbu_wlat_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON6(i));
		tbu_rtrans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON7(i));
		tbu_wtrans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON8(i));
		tbu_roos_trans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON9(i));
		tbu_woos_trans_tot[i] = smmu_read_reg(wp_base, SMMUWP_TBUx_MON10(i));
		*oos_trans_tot += tbu_roos_trans_tot[i] + tbu_woos_trans_tot[i];
		tbu_avg_rlat[i] = tbu_rtrans_tot[i] > 0 ?
				  tbu_rlat_tot[i]/tbu_rtrans_tot[i] : 0;
		tbu_avg_wlat[i] = tbu_wtrans_tot[i] > 0 ?
				  tbu_wlat_tot[i]/tbu_wtrans_tot[i] : 0;
		*tbu_lat_tots += tbu_rlat_tot[i] + tbu_wlat_tot[i];
	}

	if (*tcu_rlat_tots > 0 || *tbu_lat_tots > 0 || *oos_trans_tot > 0) {
		pr_info("---- %s start ----\n", __func__);
		pr_info("\tTCU read cmd max latency(0x%x[15:0]):%uT\n",
			SMMUWP_TCU_MON1, tcu_lat_max);
		pr_info("\tTCU read cmd max pend latency(0x%x[31:16]):%uT\n",
			SMMUWP_TCU_MON1, tcu_pend_max);
		pr_info("\tSUM of total TCU read cmds latency(0x%x):%uT\n",
			SMMUWP_TCU_MON2, tcu_lat_tot);
		pr_info("\tAverage TCU read cmd latency:%uT\n", tcu_avg_lat);
		pr_info("\tTotal TCU read cmd count(0x%x):%u\n",
			SMMUWP_TCU_MON3, tcu_trans_tot);
		pr_info("\tTotal TCU read cmd count over latency water mark(0x%x):%u\n",
			SMMUWP_TCU_MON4, tcu_oos_trans_tot);

		for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
			pr_info("\tTBU%u read cmd max latency(0x%x[15:0]):%uT\t|\tTBU%u write cmd max latency(0x%x[31:16]):%uT\n",
				i, SMMUWP_TBUx_MON1(i), tbu_rlat_max[i],
				i, SMMUWP_TBUx_MON1(i), tbu_wlat_max[i]);
			pr_info("\tAXI id of TBU%u read cmd max latency(0x%x):%u\t|\tAXI id of TBU%u write cmd max latency(0x%x):%u\n",
				i, SMMUWP_TBUx_MON2(i), tbu_id_rlat_max[i],
				i, SMMUWP_TBUx_MON3(i), tbu_id_wlat_max[i]);
			pr_info("\tTBU%u read cmd max pend latency(0x%x[15:0]):%uT\t|\tTBU%u write cmd max pend latency(0x%x[31:16]):%uT\n",
				i, SMMUWP_TBUx_MON4(i), tbu_rpend_max[i],
				i, SMMUWP_TBUx_MON4(i), tbu_wpend_max[i]);
			pr_info("\tSUM of total TBU%u read cmds latency(0x%x):%uT\t|\tSUM of total TBU%u write cmds latency(0x%x):%uT\n",
				i, SMMUWP_TBUx_MON5(i), tbu_rlat_tot[i],
				i, SMMUWP_TBUx_MON6(i), tbu_wlat_tot[i]);
			pr_info("\tAverage TBU%u read cmd latency:%uT\t|\tAverage TBU%u write cmd latency:%uT\n",
				i, tbu_avg_rlat[i], i, tbu_avg_wlat[i]);
			pr_info("\tTotal TBU%u read cmd count(0x%x):%u\t|\tTotal TBU%u write cmd count(0x%x):%u\n",
				i, SMMUWP_TBUx_MON7(i), tbu_rtrans_tot[i],
				i, SMMUWP_TBUx_MON8(i), tbu_wtrans_tot[i]);
			pr_info("\tTBU%u read cmd trans total over latency water mark(0x%x):%u\t|\tTBU%u write cmd trans total over latency water mark(0x%x):%u\n",
				i, SMMUWP_TBUx_MON9(i), tbu_roos_trans_tot[i],
				i, SMMUWP_TBUx_MON10(i), tbu_woos_trans_tot[i]);
		}

		pr_info("---- %s end, maxlat_axiid:%u, tcu_rlat_tots:%lu, tbu_lat_tots:%lu, oos_trans_tot:%lu ----\n",
			__func__, *maxlat_axiid, *tcu_rlat_tots,
			*tbu_lat_tots, *oos_trans_tot);
	}
}

/**
 * Used to performance debug:
 * Start TCU and TBU counter before translation.
 */
static int __maybe_unused smmuwp_start_transaction_counter(struct arm_smmu_device *smmu)
{
	void __iomem *wp_base = smmu->wp_base;
	unsigned int lmu_ctl0, glb_ctl0;
	unsigned long tcu_tot, tbu_tot;

	lmu_ctl0 = smmu_read_reg(wp_base, SMMUWP_LMU_CTL0);
	glb_ctl0 = smmu_read_reg(wp_base, SMMUWP_GLB_CTL0);

	/* Clear TCU MON5,6,7 counter which default enabled */
	smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_MON_DIS, CTL0_MON_DIS);
	smmu_write_field(wp_base, SMMUWP_GLB_CTL0, CTL0_MON_DIS, 0);

	/* Clear TBUx MON7,8 counter */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START,
			 CTL0_LAT_MON_START);
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START, 0);

	/* Both TCU MON5,6,7 and TBU MON7,8 count are expected to be 0 now */
	smmuwp_check_transaction_counter(smmu, &tcu_tot, &tbu_tot);
	if (tcu_tot || tbu_tot) {
		dev_err(smmu->dev, "%s clear counter failed\n", __func__);
		return -1;
	}

	/* Start TBUx MON7,8 counter */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START,
			 CTL0_LAT_MON_START);
	dev_info(smmu->dev,
		 "%s, LMU_CTL0(0x%x):0x%x->0x%x, GLB_CTL0(0x%x)=0x%x->0x%x\n",
		 __func__, SMMUWP_LMU_CTL0, lmu_ctl0,
		 smmu_read_reg(wp_base, SMMUWP_LMU_CTL0),
		 SMMUWP_GLB_CTL0, glb_ctl0,
		 smmu_read_reg(wp_base, SMMUWP_GLB_CTL0));

	return 0;
}

/**
 * Used to performance debug:
 * TBU or TCU counters are expected to increase after translation
 * when stop TCU and TBU counter.
 */
static void __maybe_unused smmuwp_end_transaction_counter(struct arm_smmu_device *smmu,
							  unsigned long *tcu_tot,
							  unsigned long *tbu_tot)
{
	void __iomem *wp_base = smmu->wp_base;

	/* End TBUx MON7,8 counter */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START, 0);

	smmuwp_check_transaction_counter(smmu, tcu_tot, tbu_tot);
}

/*
 * In order to get the information of how many cycles spent for masters
 * or a particular master in the SMMU in issue analysis.
 * HW provide the function(for both read and write) below:
 * Average latency(Unit is T, 1T = a few ns(diff. on each subsys,
 * the range is about 0.6ns ~ 38ns)) Max. latency and its AxID ID mask.
 *
 * @mon_axiid: the monitoring AXI ID, -1 mean monitor all
 * @lat_spec: latency water mark, used with SMMUWP_TCU_MON4,
 * SMMUWP_TBUx_MON9, SMMUWP_TBUx_MON10
 */
static int __maybe_unused smmuwp_start_latency_monitor(struct arm_smmu_device *smmu,
						       int mon_axiid,
						       int lat_spec)
{
	unsigned long tcu_rlat_tots, tbu_lat_tots, oos_trans_tot;
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;
	unsigned int i, maxlat_axiid;

	dev_info(smmu->dev, "%s, mon_axiid:%d, lat_spec:%d\n",
		 __func__, mon_axiid, lat_spec);

	if (lat_spec >= 0)
		smmu_write_field(wp_base, SMMUWP_GLB_CTL4, CTL4_LAT_SPEC,
				 FIELD_PREP(CTL4_LAT_SPEC, lat_spec));
	else
		smmu_write_field(wp_base, SMMUWP_GLB_CTL4, CTL4_LAT_SPEC,
				 FIELD_PREP(CTL4_LAT_SPEC, LAT_SPEC_DEF_VAL));

	if (mon_axiid >= 0) {
		smmu_write_field(wp_base, SMMUWP_TCU_CTL8, TCU_MON_ID,
				 FIELD_PREP(TCU_MON_ID, mon_axiid));
		smmu_write_field(wp_base, SMMUWP_TCU_CTL8, TCU_MON_ID_MASK,
				 TCU_MON_ID_MASK);
	} else {
		smmu_write_field(wp_base, SMMUWP_TCU_CTL8, TCU_MON_ID, 0);
		smmu_write_field(wp_base, SMMUWP_TCU_CTL8, TCU_MON_ID_MASK, 0);
	}
	pr_info("\tTCU_CTL8(0x%x):0x%x, GLB_CTL4(0x%x):0x%x\n",
		SMMUWP_TCU_CTL8, smmu_read_reg(wp_base, SMMUWP_TCU_CTL8),
		SMMUWP_GLB_CTL4, smmu_read_reg(wp_base, SMMUWP_GLB_CTL4));

	for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
		if (mon_axiid >= 0) {
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL4(i),
					 CTL4_RLAT_MON_ID,
					 FIELD_PREP(CTL4_RLAT_MON_ID, mon_axiid));
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL6(i),
					 CTL6_RLAT_MON_ID_MASK,
					 CTL6_RLAT_MON_ID_MASK);
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL5(i),
					 CTL5_WLAT_MON_ID,
					 FIELD_PREP(CTL5_WLAT_MON_ID, mon_axiid));
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL7(i),
					 CTL7_WLAT_MON_ID_MASK,
					 CTL7_WLAT_MON_ID_MASK);
		} else {
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL4(i),
					CTL4_RLAT_MON_ID, 0);
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL6(i),
					 CTL6_RLAT_MON_ID_MASK, 0);
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL5(i),
					 CTL5_WLAT_MON_ID, 0);
			smmu_write_field(wp_base, SMMUWP_TBUx_CTL7(i),
					 CTL7_WLAT_MON_ID_MASK, 0);
		}
		pr_info("\tTBU%u_CTL4(0x%x):0x%x, TBU%u_CTL5(0x%x):0x%x, TBU%u_CTL6(0x%x):0x%x, TBU%u_CTL7(0x%x):0x%x\n",
			i, SMMUWP_TBUx_CTL4(i), smmu_read_reg(wp_base, SMMUWP_TBUx_CTL4(i)),
			i, SMMUWP_TBUx_CTL5(i), smmu_read_reg(wp_base, SMMUWP_TBUx_CTL5(i)),
			i, SMMUWP_TBUx_CTL6(i), smmu_read_reg(wp_base, SMMUWP_TBUx_CTL6(i)),
			i, SMMUWP_TBUx_CTL7(i), smmu_read_reg(wp_base, SMMUWP_TBUx_CTL7(i)));
	}

	/* Clear latency counter */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START,
			 CTL0_LAT_MON_START);
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START, 0);

	/* Both TCU and TBU latency count are expected to be 0 now */
	smmuwp_check_latency_counter(smmu, &maxlat_axiid, &tcu_rlat_tots,
				     &tbu_lat_tots, &oos_trans_tot);
	if (tcu_rlat_tots || tbu_lat_tots || oos_trans_tot) {
		pr_info("%s clear counter failed\n", __func__);
		return -1;
	}

	/* Start latency counter */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START,
			 CTL0_LAT_MON_START);

	return 0;
}

/* Used to performance debug */
static void __maybe_unused smmuwp_end_latency_monitor(struct arm_smmu_device *smmu,
						      unsigned int *maxlat_axiid,
						      unsigned long *tcu_rlat_tots,
						      unsigned long *tbu_lat_tots,
						      unsigned long *oos_trans_tot)
{
	void __iomem *wp_base = smmu->wp_base;

	/* End latency counter */
	smmu_write_field(wp_base, SMMUWP_LMU_CTL0, CTL0_LAT_MON_START, 0);

	smmuwp_check_latency_counter(smmu, maxlat_axiid, tcu_rlat_tots,
				     tbu_lat_tots, oos_trans_tot);
}

/* Used to debug hang/stability issue */
static void smmuwp_dump_outstanding_monitor(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;
	unsigned int i, regval;

	pr_info("---- %s start ----\n", __func__);

	regval = smmu_read_reg(wp_base, SMMUWP_TCU_DBG1);
	pr_info("\tTCU_DBG1(0x%x):0x%x, AROSTD_M:0x%lx, AWOSTD_M:0x%lx\n",
		SMMUWP_TCU_DBG1, regval, FIELD_GET(TCU_AROSTD_M, regval),
		FIELD_GET(TCU_AWOSTD_M, regval));

	regval = smmu_read_reg(wp_base, SMMUWP_TCU_DBG2);
	pr_info("\tTCU_DBG2(0x%x):0x%x, WOSTD_M:0x%lx, WDAT_M:0x%lx\n",
		SMMUWP_TCU_DBG2, regval, FIELD_GET(TCU_WOSTD_M, regval),
		FIELD_GET(TCU_WDAT_M, regval));

	regval = smmu_read_reg(wp_base, SMMUWP_TCU_DBG3);
	pr_info("\tTCU_DBG3(0x%x):0x%x, RDAT_M:0x%lx, ACOSTD_M:0x%lx\n",
		SMMUWP_TCU_DBG3, regval, FIELD_GET(TCU_RDAT_M, regval),
		FIELD_GET(TCU_ACOSTD_M, regval));

	regval = smmu_read_reg(wp_base, SMMUWP_TCU_DBG4);
	pr_info("\tTCU_DBG4(0x%x):0x%x, DVM_AC_OSTD:0x%lx, DVM_SYNC_OSTD:0x%lx, DVM_COMP_OSTD:0x%lx\n",
		SMMUWP_TCU_DBG4, regval, FIELD_GET(TCU_DVM_AC_OSTD, regval),
		FIELD_GET(TCU_DVM_SYNC_OSTD, regval),
		FIELD_GET(TCU_DVM_COMP_OSTD, regval));

	for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG1(i));
		pr_info("\tTBU%u_DBG1(0x%x):0x%x, AWOSTD_S:0x%lx, AWOSTD_M:0x%lx\n",
			i, SMMUWP_TBUx_DBG1(i), regval, FIELD_GET(DBG1_AWOSTD_S, regval),
			FIELD_GET(DBG1_AWOSTD_M, regval));

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG2(i));
		pr_info("\tTBU%u_DBG2(0x%x):0x%x, AROSTD_S:0x%lx, AROSTD_M:0x%lx\n",
			i, SMMUWP_TBUx_DBG2(i), regval, FIELD_GET(DBG2_AROSTD_S, regval),
			FIELD_GET(DBG2_AROSTD_M, regval));

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG3(i));
		pr_info("\tTBU%u_DBG3(0x%x):0x%x, WOSTD_S:0x%lx, WOSTD_M:0x%lx\n",
			i, SMMUWP_TBUx_DBG3(i), regval, FIELD_GET(DBG3_WOSTD_S, regval),
			FIELD_GET(DBG3_WOSTD_M, regval));

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG4(i));
		pr_info("\tTBU%u_DBG4(0x%x):0x%x, WDAT_S:0x%lx, WDAT_M:0x%lx\n",
			i, SMMUWP_TBUx_DBG4(i), regval, FIELD_GET(DBG4_WDAT_S, regval),
			FIELD_GET(DBG4_WDAT_M, regval));

		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG5(i));
		pr_info("\tTBU%u_DBG5(0x%x):0x%x, RDAT_S:0x%lx, RDAT_M:0x%lx\n",
			i, SMMUWP_TBUx_DBG5(i), regval, FIELD_GET(DBG5_RDAT_S, regval),
			FIELD_GET(DBG5_RDAT_M, regval));
	}

	pr_info("---- %s end ----\n", __func__);
}

/* Used to debug hang/stability issue */
static void smmuwp_dump_io_interface_signals(struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;
	unsigned int i, regval;

	pr_info("---- %s start ----\n", __func__);

	regval = smmu_read_reg(wp_base, SMMUWP_TCU_DBG0);
	pr_info("\tTCU_DBG0(0x%x):0x%x, AXI_DBG_M:0x%lx\n",
		SMMUWP_TCU_DBG0, regval, FIELD_GET(TCU_AXI_DBG_M, regval));

	for (i = 0; i < SMMU_TBU_CNT(data->plat_data->smmu_type); i++) {
		regval = smmu_read_reg(wp_base, SMMUWP_TBUx_DBG0(i));
		pr_info("\tTBU%u_DBG0(0x%x):0x%x, AXI_DBG_S:0x%lx, AXI_DBG_M:0x%lx\n",
			i, SMMUWP_TBUx_DBG0(i), regval, FIELD_GET(DBG0_AXI_DBG_S, regval),
			FIELD_GET(DBG0_AXI_DBG_M, regval));
	}

	pr_info("---- %s end ----\n", __func__);
}

/*
 * Used to debug power consumption issue.
 * DCM is enabled during SMMU init flow to save power.
 * If the power consumption issue still exists after DCM is enabled,
 * you can consider turning off SMMU to check power consumption.
 */
static void __maybe_unused smmuwp_dump_dcm_en(struct arm_smmu_device *smmu)
{
	void __iomem *wp_base = smmu->wp_base;
	unsigned int regval;

	regval = smmu_read_reg(wp_base, SMMUWP_GLB_CTL0);
	dev_info(smmu->dev,
		 "[%s] GLB_CTL0(0x%x):0x%x, DCM_EN:0x%x, CFG_TAB_DCM_EN:0x%x\n",
		 __func__, SMMUWP_GLB_CTL0, regval, FIELD_GET(CTL0_DCM_EN, regval),
		 FIELD_GET(CTL0_CFG_TAB_DCM_EN, regval));
}

#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3) && IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
void mtk_smmu_reg_dump(enum mtk_smmu_type type,
		       struct device *master_dev,
		       int sid)
{
	static DEFINE_RATELIMIT_STATE(dbg_rs, SMMU_FAULT_RS_INTERVAL,
				      SMMU_FAULT_RS_BURST);
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;
	bool sid_valid;
	u32 sid_max;
	int ret;

	if (!master_dev || type >= SMMU_TYPE_NUM)
		return;

	data = mkt_get_smmu_data(type);
	if (!data)
		return;

	if (!__ratelimit(&dbg_rs))
		return;

	smmu = &data->smmu;
	if (data->hw_init_flag != 1) {
		dev_info(smmu->dev, "[%s] smmu:%s hw not init\n",
			 __func__, get_smmu_name(type));
		return;
	}

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return;
	}

	sid_max = 1ULL << smmu->sid_bits;
	sid_valid = sid >= 0 && sid < sid_max;

	dev_info(smmu->dev,
		 "[%s] smmu:%s dev:%s sid:[0x%x, 0x%x]\n",
		 __func__, get_smmu_name(type), dev_name(master_dev),
		 sid, sid_valid);

	mtk_smmu_glbreg_dump(smmu);
	mtk_smmu_wpreg_dump(NULL, type);
	mtk_hyp_smmu_reg_dump(smmu);

	if (sid_valid) {
		mtk_smmu_ste_cd_info_dump(NULL, type, sid);
		mtk_hyp_smmu_debug_dump(smmu, 0, 0, sid, HYP_SMMU_GLOBAL_STE_DUMP_EVT, 0);
		mtk_smmu_sid_dump(smmu, sid);
	}

	mtk_smmu_power_put(smmu);
}
EXPORT_SYMBOL_GPL(mtk_smmu_reg_dump);

int mtk_smmu_tf_detect(enum mtk_smmu_type type,
		       struct device *master_dev,
		       u32 sid, u32 tbu,
		       u32 *axids, u32 num_axids,
		       struct mtk_smmu_fault_param *param)
{
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;
	unsigned int irq_sta = 0;
	bool sid_valid;
	u64 sid_max;
	int ret = 0;

	if (!master_dev || type >= SMMU_TYPE_NUM || !param)
		return -EINVAL;

	data = mkt_get_smmu_data(type);
	if (!data)
		return -EINVAL;

	if (data->hw_init_flag != 1)
		return -1;

	if (tbu >= SMMU_TBU_CNT(type))
		return -EINVAL;

	if (num_axids == 0)
		return -EINVAL;

	smmu = &data->smmu;
	sid_max = 1ULL << smmu->sid_bits;
	sid_valid = sid > 0 && sid < sid_max;
	if (!sid_valid)
		return -EINVAL;

	irq_sta = smmu_read_reg(smmu->wp_base, SMMUWP_IRQ_NS_STA);
	if (irq_sta > 0) {
		ret = smmuwp_tf_detect(smmu, sid, tbu, axids, num_axids, param);
		dev_info(smmu->dev,
			 "[%s] smmu:%s dev:%s sid:0x%x tbu:%d irq_sta:0x%x ret:%d\n",
			 __func__, get_smmu_name(type), dev_name(master_dev),
			 sid, tbu, irq_sta, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smmu_tf_detect);

int mtk_smmu_start_transaction_counter(struct device *dev)
{
	struct arm_smmu_device *smmu = get_smmu_device(dev);
	int ret;

	if (!smmu)
		return -1;

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return -1;
	}

	ret = smmuwp_start_transaction_counter(smmu);

	mtk_smmu_power_put(smmu);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smmu_start_transaction_counter);

void mtk_smmu_end_transaction_counter(struct device *dev,
				      unsigned long *tcu_tot,
				      unsigned long *tbu_tot)
{
	struct arm_smmu_device *smmu = get_smmu_device(dev);
	int ret;

	if (!smmu)
		return;

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return;
	}

	smmuwp_end_transaction_counter(smmu, tcu_tot, tbu_tot);

	mtk_smmu_power_put(smmu);
}
EXPORT_SYMBOL_GPL(mtk_smmu_end_transaction_counter);

int mtk_smmu_start_latency_monitor(struct device *dev,
				   int mon_axiid,
				   int lat_spec)
{
	struct arm_smmu_device *smmu = get_smmu_device(dev);
	int ret;

	if (!smmu)
		return -1;

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return -1;
	}

	ret = smmuwp_start_latency_monitor(smmu, mon_axiid, lat_spec);

	mtk_smmu_power_put(smmu);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smmu_start_latency_monitor);

void mtk_smmu_end_latency_monitor(struct device *dev,
				  unsigned int *maxlat_axiid,
				  unsigned long *tcu_rlat_tots,
				  unsigned long *tbu_lat_tots,
				  unsigned long *oos_trans_tot)
{
	struct arm_smmu_device *smmu = get_smmu_device(dev);
	int ret;

	if (!smmu)
		return;

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return;
	}

	smmuwp_end_latency_monitor(smmu, maxlat_axiid,
			tcu_rlat_tots, tbu_lat_tots, oos_trans_tot);

	mtk_smmu_power_put(smmu);
}
EXPORT_SYMBOL_GPL(mtk_smmu_end_latency_monitor);

void mtk_smmu_dump_outstanding_monitor(struct device *dev)
{
	struct arm_smmu_device *smmu = get_smmu_device(dev);
	int ret;

	if (!smmu)
		return;

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return;
	}

	smmuwp_dump_outstanding_monitor(smmu);

	mtk_smmu_power_put(smmu);
}
EXPORT_SYMBOL_GPL(mtk_smmu_dump_outstanding_monitor);

void mtk_smmu_dump_io_interface_signals(struct device *dev)
{
	struct arm_smmu_device *smmu = get_smmu_device(dev);
	int ret;

	if (!smmu)
		return;

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return;
	}

	smmuwp_dump_io_interface_signals(smmu);

	mtk_smmu_power_put(smmu);
}
EXPORT_SYMBOL_GPL(mtk_smmu_dump_io_interface_signals);

void mtk_smmu_dump_dcm_en(struct device *dev)
{
	struct arm_smmu_device *smmu = get_smmu_device(dev);
	int ret;

	if (!smmu)
		return;

	ret = mtk_smmu_power_get(smmu);
	if (ret) {
		dev_info(smmu->dev, "[%s] power_status:%d\n", __func__, ret);
		return;
	}

	smmuwp_dump_dcm_en(smmu);

	mtk_smmu_power_put(smmu);
}
EXPORT_SYMBOL_GPL(mtk_smmu_dump_dcm_en);

static struct arm_smmu_device *get_smmu_dev_for_pmu(struct smmuv3_pmu_device *pmu_device)
{
	struct platform_device *smmudev;
	struct device_node *smmu_node;
	struct device *dev;

	if (!pmu_device || !pmu_device->dev)
		return ERR_PTR(-ENOENT);

	dev = pmu_device->dev;
	smmu_node = of_parse_phandle(dev->of_node, PMU_SMMU_PROP_NAME, 0);
	if (!smmu_node) {
		dev_info(dev, "%s, can't find smmu node\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	smmudev = of_find_device_by_node(smmu_node);
	if (!smmudev) {
		dev_info(dev, "%s, can't find smmu dev\n", __func__);
		of_node_put(smmu_node);
		return ERR_PTR(-EINVAL);
	}

	of_node_put(smmu_node);
	return platform_get_drvdata(smmudev);
}

int mtk_smmu_register_pmu_device(struct smmuv3_pmu_device *pmu_device)
{
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;
	unsigned long flags;
	struct device *dev;

	smmu = get_smmu_dev_for_pmu(pmu_device);
	if (IS_ERR(smmu))
		return -ENOENT;

	data = to_mtk_smmu_data(smmu);
	dev = pmu_device->dev;

	spin_lock_irqsave(&data->pmu_lock, flags);
	list_add(&pmu_device->node, &data->pmu_devices);
	spin_unlock_irqrestore(&data->pmu_lock, flags);

	if (pmu_device->impl && pmu_device->impl->late_init)
		pmu_device->impl->late_init(smmu->wp_base, dev);

	if (pmu_device->impl && pmu_device->impl->irq_set_up)
		pmu_device->impl->irq_set_up(smmu->combined_irq, dev);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_register_pmu_device);

void mtk_smmu_unregister_pmu_device(struct smmuv3_pmu_device *pmu_device)
{
	struct arm_smmu_device *smmu;
	struct mtk_smmu_data *data;
	unsigned long flags;

	smmu = get_smmu_dev_for_pmu(pmu_device);
	if (IS_ERR(smmu))
		return;

	data = to_mtk_smmu_data(smmu);

	if (pmu_device) {
		spin_lock_irqsave(&data->pmu_lock, flags);
		list_del(&pmu_device->node);
		spin_unlock_irqrestore(&data->pmu_lock, flags);
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_unregister_pmu_device);
#endif

MODULE_DESCRIPTION("MediaTek SMMUv3 Customization");
MODULE_LICENSE("GPL");
