// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#define pr_fmt(fmt) "sysmmu: " fmt

#include <linux/kmemleak.h>
#include <linux/module.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/iova.h>
#include <trace/hooks/iommu.h>
#include "samsung-iommu-v9.h"

#define REG_MMU_NUM_CONTEXT			0x0100

#define REG_MMU_FEATURE_INFO			0x3008
#define MMU_READ_TLB_PER_VM(reg)		((reg) & 0x1)

#define MMU_NUM_CONTEXT(reg)			((reg) & 0x1F)

#define REG_MMU_ALL_INV_VM			0x8010
#define REG_MMU_RANGE_INV_START_VPN_VM		0x8020
#define REG_MMU_RANGE_INV_END_VPN_AND_TRIG_VM	0x8024

#define MMU_PMMU_INFO_VA_WIDTH(reg)		((reg) & 0x1)

#define FLPD_SHAREABLE_FLAG	BIT(6)
#define SLPD_SHAREABLE_FLAG	BIT(4)
#define FLPD_PBHA_SHIFT		4
#define SLPD_PBHA_SHIFT		2

#define CFIELD_AXCACHE		0x4
#define FLPD_CACHE_MASK		GENMASK(9, 7)
#define FLPD_CACHE_FLAG		(CFIELD_AXCACHE << 7)
#define SLPD_CACHE_MASK		GENMASK(7, 5)
#define SLPD_CACHE_FLAG		(CFIELD_AXCACHE << 5)

#define DEFAULT_QOS_VALUE	-1
#define DEFAULT_STREAM_NONE	~0U
#define UNUSED_STREAM_INDEX	~0U

#define MMU_STREAM_CFG_S2_PREFETCH_EN		0x100
#define MMU_STREAM_CFG_MASK(old_reg ,reg)	((old_reg) & MMU_STREAM_CFG_S2_PREFETCH_EN) |	\
						((reg) & (GENMASK(31, 16) | GENMASK(6, 0)))
#define MMU_STREAM_MATCH_CFG_MASK(reg)		((reg) & (GENMASK(9, 8) | 0x1))

#define MMU_CONTEXT0_CFG_ATTRIBUTE_PT_CACHEABLE_RANGE	GENMASK(19, 16)
#define MMU_CONTEXT0_CFG_ATTRIBUTE_PT_CACHEABLE		(0x2 << 16)
#define MMU_CONTEXT0_CFG_ATTRIBUTE_CACHEABLE_OVERRIDE	((0x1) << 31)

#define MMU_SWALKER_INFO_NUM_PMMU(reg)		((reg) & GENMASK(15, 0))
#define MMU_PMMU_INFO_NUM_STREAM_TABLE(reg)	(((reg) >> 16) & 0xFFFF)
#define MMU_PMMU_INFO_NUM_PTLB(reg)		(((reg) >> 1) & 0x7FFF)

static struct iommu_ops samsung_sysmmu_ops;
static struct iommu_domain_ops samsung_sysmmu_domain_ops;
static struct platform_driver samsung_sysmmu_driver_v9;

struct samsung_sysmmu_domain {
	struct iommu_domain domain;
	struct samsung_iommu_log log;
	struct iommu_group *group;
	sysmmu_pte_t *page_table;
	atomic_t *lv2entcnt;
	spinlock_t pgtablelock;
	bool cacheable_override;
};

static inline void samsung_iommu_write_event(struct samsung_iommu_log *iommu_log,
					     enum sysmmu_event_type type,
					     u64 start, u64 end)
{
	struct sysmmu_log *log;
	unsigned int index = (unsigned int)atomic_inc_return(&iommu_log->index) - 1;

	log = &iommu_log->log[index % iommu_log->len];
	log->time = sched_clock();
	log->type = type;
	log->start = start;
	log->end = end;
}

#define SYSMMU_EVENT_LOG(data, type)	samsung_iommu_write_event(&(data)->log, type, 0, 0)
#define SYSMMU_EVENT_LOG_RANGE(data, type, s, e)	\
					samsung_iommu_write_event(&(data)->log, type, s, e)

static DEFINE_MUTEX(sysmmu_global_mutex);
static struct device sync_dev;
static struct kmem_cache *slpt_cache;

static inline u32 __sysmmu_get_hw_version(struct sysmmu_drvdata *data)
{
	return MMU_VERSION_RAW(readl_relaxed(data->sfrbase + REG_MMU_VERSION));
}

static inline u32 __sysmmu_get_num_vm(struct sysmmu_drvdata *data)
{
	return MMU_NUM_CONTEXT(readl_relaxed(data->sfrbase + REG_MMU_NUM_CONTEXT));
}

static inline u32 __sysmmu_get_num_pmmu(struct sysmmu_drvdata *data)
{
	return MMU_SWALKER_INFO_NUM_PMMU(readl_relaxed(data->sfrbase + REG_MMU_SWALKER_INFO));
}

static inline u32 __sysmmu_get_num_stream_table(struct sysmmu_drvdata *data, int pmmu_idx)
{
	writel_relaxed(MMU_SET_PMMU_INDICATOR(pmmu_idx), data->sfrbase + REG_MMU_PMMU_INDICATOR);
	return MMU_PMMU_INFO_NUM_STREAM_TABLE(readl_relaxed(data->sfrbase + REG_MMU_PMMU_INFO));
}

static inline u32 __sysmmu_get_num_ptlb(struct sysmmu_drvdata *data, int pmmu_idx)
{
	writel_relaxed(MMU_SET_PMMU_INDICATOR(pmmu_idx), data->sfrbase + REG_MMU_PMMU_INDICATOR);
	return MMU_PMMU_INFO_NUM_PTLB(readl_relaxed(data->sfrbase + REG_MMU_PMMU_INFO));
}

static inline u32 __sysmmu_get_va_width(struct sysmmu_drvdata *data)
{
	writel_relaxed(MMU_SET_PMMU_INDICATOR(0), data->sfrbase + REG_MMU_PMMU_INDICATOR);
	return MMU_PMMU_INFO_VA_WIDTH(readl_relaxed(data->sfrbase + REG_MMU_PMMU_INFO));
}

static inline u32 __sysmmu_get_feature_info(struct sysmmu_drvdata *data)
{
	return MMU_READ_TLB_PER_VM(readl_relaxed(data->sfrbase + REG_MMU_FEATURE_INFO));
}

static inline void __sysmmu_write_all_vm(struct sysmmu_drvdata *data, u32 value,
					 void __iomem *addr)
{
	int i;

	for (i = 0; i < data->max_vm; i++) {
		if (data->vmid_mask & (1 << i))
			writel_relaxed(value, MMU_VM_ADDR(addr, i));
	}
}

static inline void __sysmmu_invalidate_all(struct sysmmu_drvdata *data)
{
	__sysmmu_write_all_vm(data, 0x1, data->sfrbase + REG_MMU_ALL_INV_VM);

	SYSMMU_EVENT_LOG(data, SYSMMU_EVENT_INVALIDATE_ALL);
}

static inline void __sysmmu_invalidate(struct sysmmu_drvdata *data,
					   dma_addr_t start, dma_addr_t end)
{
	__sysmmu_write_all_vm(data, ALIGN_DOWN(start, SPAGE_SIZE) >> 4,
			      data->sfrbase + REG_MMU_RANGE_INV_START_VPN_VM);
	__sysmmu_write_all_vm(data, (ALIGN_DOWN(end - 1, SPAGE_SIZE) >> 4) | 0x1,
			      data->sfrbase + REG_MMU_RANGE_INV_END_VPN_AND_TRIG_VM);

	SYSMMU_EVENT_LOG_RANGE(data, SYSMMU_EVENT_INVALIDATE_RANGE, start, end);
}

static inline void __sysmmu_disable(struct sysmmu_drvdata *data)
{
	__sysmmu_invalidate_all(data);

	SYSMMU_EVENT_LOG(data, SYSMMU_EVENT_DISABLE);
}

static inline void __sysmmu_set_stream(struct sysmmu_drvdata *data, int pmmu_id)
{
	struct stream_props *props = &data->props[pmmu_id];
	struct stream_config *cfg = props->cfg;
	int id_cnt = props->id_cnt;
	unsigned int i, index;
	u32 mmu_stream_cfg;

	writel_relaxed(MMU_SET_PMMU_INDICATOR(pmmu_id), data->sfrbase + REG_MMU_PMMU_INDICATOR);

	mmu_stream_cfg = readl_relaxed(data->sfrbase + REG_MMU_STREAM_CFG(0));

	if (props->default_cfg != DEFAULT_STREAM_NONE)
		writel_relaxed(MMU_STREAM_CFG_MASK(mmu_stream_cfg, props->default_cfg),
			       data->sfrbase + REG_MMU_STREAM_CFG(0));

	for (i = 0; i < id_cnt; i++) {
		if (cfg[i].index == UNUSED_STREAM_INDEX)
			continue;

		index = cfg[i].index;
		mmu_stream_cfg = readl_relaxed(data->sfrbase + REG_MMU_STREAM_CFG(index));

		writel_relaxed(MMU_STREAM_CFG_MASK(mmu_stream_cfg, cfg[i].cfg),
			       data->sfrbase + REG_MMU_STREAM_CFG(index));
		writel_relaxed(MMU_STREAM_MATCH_CFG_MASK(cfg[i].match_cfg),
			       data->sfrbase + REG_MMU_STREAM_MATCH_CFG(index));
		writel_relaxed(cfg[i].match_id_value,
			       data->sfrbase + REG_MMU_STREAM_MATCH_SID_VALUE(index));
		writel_relaxed(cfg[i].match_id_mask,
			       data->sfrbase + REG_MMU_STREAM_MATCH_SID_MASK(index));
	}
}

static inline void __sysmmu_init_config(struct sysmmu_drvdata *data)
{
	int i;
	u32 cfg;

	for (i = 0; i < data->max_vm; i++) {
		if (!(data->vmid_mask & (1 << i)))
			continue;

		cfg = readl_relaxed(MMU_VM_ADDR(data->sfrbase + REG_MMU_CONTEXT0_CFG_ATTRIBUTE_VM,
						i));

		cfg &= ~MMU_CONTEXT0_CFG_ATTRIBUTE_PT_CACHEABLE_RANGE;
		cfg |= MMU_CONTEXT0_CFG_ATTRIBUTE_PT_CACHEABLE;

		if (data->cacheable_override_mode)
			cfg |= MMU_CONTEXT0_CFG_ATTRIBUTE_CACHEABLE_OVERRIDE;

		if (data->qos != DEFAULT_QOS_VALUE) {
			cfg &= ~CFG_QOS(0xF);
			cfg |= CFG_QOS_OVRRIDE | CFG_QOS(data->qos);
		}
		writel_relaxed(cfg, MMU_VM_ADDR(data->sfrbase + REG_MMU_CONTEXT0_CFG_ATTRIBUTE_VM,
						i));
	}

	for (i = 0; i < data->num_pmmu; i++)
		__sysmmu_set_stream(data, i);
}

static inline void __sysmmu_enable(struct sysmmu_drvdata *data)
{
	u32 ctrl_val = CTRL_ERR_RESP_VALUE | CTRL_MMU_ENABLE | CTRL_INT_ENABLE;

	if (!data->async_fault_mode)
		ctrl_val |= CTRL_FAULT_STALL_MODE;

	__sysmmu_write_all_vm(data, data->pgtable / SPAGE_SIZE,
			      data->sfrbase + REG_MMU_CONTEXT0_CFG_FLPT_BASE_VM);
	__sysmmu_init_config(data);
	__sysmmu_invalidate_all(data);
	__sysmmu_write_all_vm(data, ctrl_val, data->sfrbase + REG_MMU_CTRL_VM);

	SYSMMU_EVENT_LOG(data, SYSMMU_EVENT_ENABLE);
}

static struct samsung_sysmmu_domain *to_sysmmu_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct samsung_sysmmu_domain, domain);
}

static inline void pgtable_flush(void *vastart, void *vaend)
{
	dma_sync_single_for_device(&sync_dev, virt_to_phys(vastart),
				   vaend - vastart, DMA_TO_DEVICE);
}

static bool samsung_sysmmu_capable(struct device *dev, enum iommu_cap cap)
{
	return cap == IOMMU_CAP_CACHE_COHERENCY;
}

#define REG_MMU_SWALKER_PM_CFG		0x4000
#define REG_MMU_SWALKER_PM_INIT		0x4004
#define REG_MMU_SWALKER_PM_EVENT(n)	0x4010 + (n) * 0x4
#define REG_MMU_SWALKER_PM_CNT(n)	0x4020 + (n) * 0x4
#define REG_MMU_PMMUn_PM_CFG(idx)	0x4800 + (idx) * 0x100
#define REG_MMU_PMMUn_PM_INIT(idx)	0x4804 + (idx) * 0x100
#define REG_MMU_PMMUn_PM_EVENT(idx, n)	0x4810 + (idx) * 0x100 + (n) * 0x4
#define REG_MMU_PMMUn_PM_CNT(idx, n)	0x4820 + (idx) * 0x100 + (n) * 0x4
void __sysmmu_set_perf_measure(struct sysmmu_drvdata *drvdata)
{
	void __iomem *sfrbase = drvdata->sfrbase;
	int i, num_pmmu = drvdata->num_pmmu;
	/* Configuration Stage */
	/* Initialize counters */
	writel(0x1, sfrbase + REG_MMU_SWALKER_PM_INIT);
	for (i = 0; i < num_pmmu; i++)
		writel(0x1, sfrbase + REG_MMU_PMMUn_PM_INIT(i));

	/*
	 * Counter0 : read request of TLB0
	 * Counter1 : TLB miss of read request of TLB0
	 * Counter2 : write request of TLB0
	 * Counter3 : TLB miss of write request of TLB0
	 */

	/* stlb */
	writel(0x12, sfrbase + REG_MMU_SWALKER_PM_EVENT(0)); /* read miss */
	writel(0x10, sfrbase + REG_MMU_SWALKER_PM_EVENT(1)); /* read total */
	writel(0x22, sfrbase + REG_MMU_SWALKER_PM_EVENT(2)); /* write miss */
	writel(0x20, sfrbase + REG_MMU_SWALKER_PM_EVENT(3)); /* write total */

	/* ptlb */
	for (i = 0; i < num_pmmu; i++)
		writel(0x12, sfrbase + REG_MMU_PMMUn_PM_EVENT(i, 0)); /* read miss */
	for (i = 0; i < num_pmmu; i++)
		writel(0x10, sfrbase + REG_MMU_PMMUn_PM_EVENT(i, 1)); /* read total */
	for (i = 0; i < num_pmmu; i++)
		writel(0x22, sfrbase + REG_MMU_PMMUn_PM_EVENT(i, 2)); /* write miss */
	for (i = 0; i < num_pmmu; i++)
		writel(0x20, sfrbase + REG_MMU_PMMUn_PM_EVENT(i, 3)); /* write total */

	/* Start measurement */
	writel(0x1, sfrbase + REG_MMU_SWALKER_PM_CFG);
	for (i = 0; i < num_pmmu; i++)
		writel(0x1, sfrbase + REG_MMU_PMMUn_PM_CFG(i));

	dev_dbg(drvdata->dev, "%s: set!\n", __func__);
}

void __sysmmu_get_perf_measure(struct sysmmu_drvdata *drvdata)
{
	void __iomem *sfrbase = drvdata->sfrbase;
	int i, j, num_pmmu = drvdata->num_pmmu;
	u32 cnt[4];

	/* Stop measurement */
	writel(0x0, sfrbase + REG_MMU_SWALKER_PM_CFG);
	for (i = 0; i < num_pmmu; i++)
		writel(0x0, sfrbase + REG_MMU_PMMUn_PM_CFG(i));

	/* stlb */
	for (i = 0; i < 4; i++)
		cnt[i] = readl_relaxed(sfrbase + REG_MMU_SWALKER_PM_CNT(i));
	dev_info(drvdata->dev, "[SWALKER][PMCNT] Read (miss/total) : %d / %d,", cnt[0], cnt[1]);
	dev_info(drvdata->dev, " Write (miss/total) : %d / %d\n", cnt[2], cnt[3]);

	/* ptlb */
	for (i = 0; i < num_pmmu; i++) {
		for (j = 0; j < 4; j++)
			cnt[j] = readl_relaxed(sfrbase + REG_MMU_PMMUn_PM_CNT(i, j));

		dev_info(drvdata->dev, "[PMMU %d][PMCNT] Read (miss/total) : %d / %d,", i,
			 cnt[0], cnt[1]);
		dev_info(drvdata->dev, " Write (miss/total) : %d / %d\n", cnt[2], cnt[3]);
	}

	dev_dbg(drvdata->dev, "%s: clear!\n", __func__);
}

void samsung_iommu_set_perf_measure(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct sysmmu_clientdata *client = dev_iommu_priv_get(dev);
	unsigned long flags;
	int i;

	if (!fwspec || !client) {
		dev_err(dev, "has no IOMMU\n");
		return;
	}

	for (i = 0; i < client->sysmmu_count; i++) {
		struct sysmmu_drvdata *drvdata;

		drvdata = client->sysmmus[i];
		if (!drvdata->perf_mode)
			continue;

		spin_lock_irqsave(&drvdata->lock, flags);
		if (drvdata->attached_count == 0 ||
				!pm_runtime_active(drvdata->dev)) {
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}
		__sysmmu_set_perf_measure(drvdata);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}
EXPORT_SYMBOL_GPL(samsung_iommu_set_perf_measure);

void samsung_iommu_get_perf_measure(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct sysmmu_clientdata *client = dev_iommu_priv_get(dev);
	unsigned long flags;
	int i;

	if (!fwspec || !client) {
		dev_err(dev, "has no IOMMU\n");
		return;
	}

	for (i = 0; i < client->sysmmu_count; i++) {
		struct sysmmu_drvdata *drvdata;

		drvdata = client->sysmmus[i];
		if (!drvdata->perf_mode)
			continue;

		spin_lock_irqsave(&drvdata->lock, flags);
		if (drvdata->attached_count == 0 ||
				!pm_runtime_active(drvdata->dev)) {
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}
		__sysmmu_get_perf_measure(drvdata);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}
EXPORT_SYMBOL_GPL(samsung_iommu_get_perf_measure);

static inline size_t get_log_fit_pages(struct samsung_iommu_log *log, int len)
{
	return DIV_ROUND_UP(sizeof(*(log->log)) * len, PAGE_SIZE);
}

static void samsung_iommu_deinit_log(struct samsung_iommu_log *log)
{
	struct page *page;
	int i, fit_pages = get_log_fit_pages(log, log->len);

	page = virt_to_page(log->log);
	for (i = 0; i < fit_pages; i++)
		__free_page(page + i);
}

static int samsung_iommu_init_log(struct samsung_iommu_log *log, int len)
{
	struct page *page;
	int order, order_pages, fit_pages = get_log_fit_pages(log, len);

	atomic_set(&log->index, 0);
	order = get_order(fit_pages * PAGE_SIZE);
	page = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	if (!page)
		return -ENOMEM;

	split_page(page, order);

	order_pages = 1 << order;
	if (order_pages > fit_pages) {
		int i;

		for (i = fit_pages; i < order_pages; i++)
			__free_page(page + i);
	}

	log->log = page_address(page);
	log->len = len;

	return 0;
}

static struct iommu_domain *samsung_sysmmu_domain_alloc(unsigned int type)
{
	struct samsung_sysmmu_domain *domain;

	if (type != IOMMU_DOMAIN_UNMANAGED &&
	    type != IOMMU_DOMAIN_DMA &&
	    type != IOMMU_DOMAIN_IDENTITY) {
		pr_err("invalid domain type %u\n", type);
		return NULL;
	}

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return NULL;

	domain->page_table = kcalloc(NUM_LV1ENTRIES, sizeof(*domain->page_table), GFP_KERNEL);
	if (!domain->page_table)
		goto err_pgtable;

	domain->lv2entcnt = kcalloc(NUM_LV1ENTRIES, sizeof(*domain->lv2entcnt), GFP_KERNEL);
	if (!domain->lv2entcnt)
		goto err_counter;

	if (samsung_iommu_init_log(&domain->log, SYSMMU_EVENT_MAX)) {
		pr_err("failed to init domian logging\n");
		goto err_init_log;
	}

	pgtable_flush(domain->page_table, domain->page_table + NUM_LV1ENTRIES);

	spin_lock_init(&domain->pgtablelock);

	return &domain->domain;

err_init_log:
	kfree(domain->lv2entcnt);
err_counter:
	kfree(domain->page_table);
err_pgtable:
	kfree(domain);

	return NULL;
}

static void samsung_sysmmu_domain_free(struct iommu_domain *dom)
{
	struct samsung_sysmmu_domain *domain = to_sysmmu_domain(dom);

	samsung_iommu_deinit_log(&domain->log);
	kfree(domain->page_table);
	kfree(domain->lv2entcnt);
	kfree(domain);
}

static inline void samsung_sysmmu_detach_drvdata(struct sysmmu_drvdata *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	if (--data->attached_count == 0) {
		if (pm_runtime_active(data->dev))
			__sysmmu_disable(data);

		list_del(&data->list);
		data->pgtable = 0;
		data->group = NULL;
	}
	spin_unlock_irqrestore(&data->lock, flags);
}

static void samsung_sysmmu_detach_client(struct sysmmu_clientdata *client, int count)
{
	struct sysmmu_drvdata *drvdata;
	int i;

	for (i = 0; i < count; i++) {
		drvdata = client->sysmmus[i];
		samsung_sysmmu_detach_drvdata(drvdata);
	}
}

static int samsung_of_get_dma_window(struct device_node *dn, const char *prefix, int index,
				     unsigned long *busno, dma_addr_t *addr, size_t *size)
{
	const __be32 *dma_window, *end;
	int bytes, cur_index = 0;
	char propname[NAME_MAX], addrname[NAME_MAX], sizename[NAME_MAX];

	if (!dn || !addr || !size)
		return -EINVAL;

	if (!prefix)
		prefix = "";

	snprintf(propname, sizeof(propname), "%sdma-window", prefix);
	snprintf(addrname, sizeof(addrname), "%s#dma-address-cells", prefix);
	snprintf(sizename, sizeof(sizename), "%s#dma-size-cells", prefix);

	dma_window = of_get_property(dn, propname, &bytes);
	if (!dma_window)
		return -ENODEV;
	end = dma_window + bytes / sizeof(*dma_window);

	while (dma_window < end) {
		u32 cells;
		const void *prop;

		/* busno is one cell if supported */
		if (busno)
			*busno = be32_to_cpup(dma_window++);

		prop = of_get_property(dn, addrname, NULL);
		if (!prop)
			prop = of_get_property(dn, "#address-cells", NULL);

		cells = prop ? be32_to_cpup(prop) : of_n_addr_cells(dn);
		if (!cells)
			return -EINVAL;
		*addr = of_read_number(dma_window, cells);
		dma_window += cells;

		prop = of_get_property(dn, sizename, NULL);
		cells = prop ? be32_to_cpup(prop) : of_n_size_cells(dn);
		if (!cells)
			return -EINVAL;
		*size = of_read_number(dma_window, cells);
		dma_window += cells;

		if (cur_index++ == index)
			break;
	}
	return 0;
}

static int samsung_sysmmu_set_domain_range(struct iommu_domain *dom,
					   struct device *dev)
{
	struct iommu_domain_geometry *geom = &dom->geometry;
	dma_addr_t start, end;
	size_t size;

	if (samsung_of_get_dma_window(dev->of_node, NULL, 0, NULL, &start, &size))
		return 0;

	end = start + size;

	if (end > DMA_BIT_MASK(32))
		end = DMA_BIT_MASK(32);

	if (geom->force_aperture) {
		dma_addr_t d_start, d_end;

		d_start = max(start, geom->aperture_start);
		d_end = min(end, geom->aperture_end);

		if (d_start >= d_end) {
			dev_err(dev, "current range is [%pad..%pad]\n",
				&geom->aperture_start, &geom->aperture_end);
			dev_err(dev, "requested range [%zx @ %pad] is not allowd\n",
				size, &start);
			return -ERANGE;
		}

		geom->aperture_start = d_start;
		geom->aperture_end = d_end;
	} else {
		geom->aperture_start = start;
		geom->aperture_end = end;
		/*
		 * All CPUs should observe the change of force_aperture after
		 * updating aperture_start and aperture_end because dma-iommu
		 * restricts dma virtual memory by this aperture when
		 * force_aperture is set.
		 * We allow allocating dma virtual memory during changing the
		 * aperture range because the current allocation is free from
		 * the new restricted range.
		 */
		smp_wmb();
		geom->force_aperture = true;
	}

	dev_info(dev, "changed DMA range [%pad..%pad] successfully.\n",
		 &geom->aperture_start, &geom->aperture_end);

	return 0;
}

static int samsung_sysmmu_attach_dev(struct iommu_domain *dom, struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct sysmmu_clientdata *client;
	struct samsung_sysmmu_domain *domain;
	struct list_head *group_list;
	struct sysmmu_drvdata *drvdata;
	struct iommu_group *group = dev->iommu_group;
	unsigned long flags;
	phys_addr_t page_table;
	int i, ret = -EINVAL;

	if (!fwspec || fwspec->ops != &samsung_sysmmu_ops) {
		dev_err(dev, "failed to attach, IOMMU instance data %s.\n",
			!fwspec ? "is not initialized" : "has different ops");
		return -ENXIO;
	}

	if (!dev_iommu_priv_get(dev)) {
		dev_err(dev, "has no IOMMU\n");
		return -ENODEV;
	}

	domain = to_sysmmu_domain(dom);
	domain->group = group;
	group_list = iommu_group_get_iommudata(group);
	page_table = virt_to_phys(domain->page_table);

	client = dev_iommu_priv_get(dev);
	for (i = 0; i < client->sysmmu_count; i++) {
		drvdata = client->sysmmus[i];

		spin_lock_irqsave(&drvdata->lock, flags);
		if (drvdata->attached_count++ == 0) {
			list_add(&drvdata->list, group_list);
			drvdata->group = group;
			drvdata->pgtable = page_table;

			if (drvdata->cacheable_override_mode)
				domain->cacheable_override = true;

			if (pm_runtime_active(drvdata->dev))
				__sysmmu_enable(drvdata);
		} else if (drvdata->pgtable != page_table) {
			dev_err(dev, "%s is already attached to pgtable %pa. Attach to %pa failed",
				dev_name(drvdata->dev), &drvdata->pgtable, &page_table);
			BUG();
			spin_unlock_irqrestore(&drvdata->lock, flags);
			goto err_drvdata_add;
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	ret = samsung_sysmmu_set_domain_range(dom, dev);
	if (ret)
		goto err_drvdata_add;

	dev_info(dev, "attached with pgtable %pa", &page_table);

	return 0;

err_drvdata_add:
	samsung_sysmmu_detach_client(client, i);

	return -ret;
}

static void samsung_sysmmu_detach_dev(struct iommu_domain *dom, struct device *dev)
{
	struct sysmmu_clientdata *client;
	struct samsung_sysmmu_domain *domain;
	struct list_head *group_list;
	struct iommu_group *group = dev->iommu_group;

	domain = to_sysmmu_domain(dom);
	group_list = iommu_group_get_iommudata(group);
	client = dev_iommu_priv_get(dev);
	samsung_sysmmu_detach_client(client, client->sysmmu_count);

	dev_info(dev, "detached from pgtable %pK\n", &domain->page_table);
}

static inline sysmmu_pte_t make_sysmmu_pte(phys_addr_t paddr, int pgsize, int attr)
{
	return ((sysmmu_pte_t)((paddr) >> PG_ENT_SHIFT)) | pgsize | attr;
}

static sysmmu_pte_t *alloc_lv2entry(struct samsung_sysmmu_domain *domain,
				    sysmmu_pte_t *sent, sysmmu_iova_t iova,
				    atomic_t *pgcounter)
{
	if (lv1ent_section(sent)) {
		WARN(1, "trying mapping on %#08llx mapped with 1MiB page", iova);
		return ERR_PTR(-EADDRINUSE);
	}

	if (lv1ent_unmapped(sent)) {
		unsigned long flags;
		sysmmu_pte_t *pent;

		pent = kmem_cache_zalloc(slpt_cache, GFP_KERNEL);
		if (!pent)
			return ERR_PTR(-ENOMEM);

		spin_lock_irqsave(&domain->pgtablelock, flags);
		if (lv1ent_unmapped(sent)) {
			*sent = make_sysmmu_pte(virt_to_phys(pent),
						SLPD_FLAG, 0);
			kmemleak_ignore(pent);
			atomic_set(pgcounter, 0);
			pgtable_flush(pent, pent + NUM_LV2ENTRIES);
			pgtable_flush(sent, sent + 1);
		} else {
			/* allocated entry is not used, so free it. */
			kmem_cache_free(slpt_cache, pent);
		}
		spin_unlock_irqrestore(&domain->pgtablelock, flags);
	}

	return page_entry(sent, iova);
}

static inline void clear_lv2_page_table(sysmmu_pte_t *ent, int n)
{
	memset(ent, 0, sizeof(*ent) * n);
}

#define IOMMU_PRIV_PROT_TO_PBHA(val)	(((val) >> IOMMU_PRIV_SHIFT) & 0x3)
static int lv1set_section(struct samsung_sysmmu_domain *domain,
			  sysmmu_pte_t *sent, sysmmu_iova_t iova,
			  phys_addr_t paddr, int prot, atomic_t *pgcnt)
{
	int attr = !!(prot & IOMMU_CACHE) ? FLPD_SHAREABLE_FLAG : 0;
	bool need_sync = false;
	int pbha = IOMMU_PRIV_PROT_TO_PBHA(prot);

	if (lv1ent_section(sent)) {
		WARN(1, "Trying mapping 1MB@%#08llx on valid FLPD", iova);
		return -EADDRINUSE;
	}

	if (lv1ent_page(sent)) {
		if (WARN_ON(atomic_read(pgcnt) != 0)) {
			WARN(1, "Trying mapping 1MB@%#08llx on valid SLPD", iova);
			return -EADDRINUSE;
		}

		kmem_cache_free(slpt_cache, page_entry(sent, 0));
		atomic_set(pgcnt, NUM_LV2ENTRIES);
		need_sync = true;
	}

	if (domain->cacheable_override) {
		attr &= ~FLPD_CACHE_MASK;
		attr |= FLPD_CACHE_FLAG;
	}

	attr |= pbha << FLPD_PBHA_SHIFT;
	*sent = make_sysmmu_pte(paddr, SECT_FLAG, attr);
	pgtable_flush(sent, sent + 1);

	if (need_sync) {
		struct iommu_iotlb_gather gather = {
			.start = iova,
			.end = iova + SECT_SIZE,
		};

		iommu_iotlb_sync(&domain->domain, &gather);
	}

	return 0;
}

static int lv2set_page(sysmmu_pte_t *pent, phys_addr_t paddr, size_t size, int prot,
		       atomic_t *pgcnt, bool cacheable_override)
{
	int attr = !!(prot & IOMMU_CACHE) ? SLPD_SHAREABLE_FLAG : 0;
	int pbha = IOMMU_PRIV_PROT_TO_PBHA(prot);

	if (cacheable_override) {
		attr &= ~SLPD_CACHE_MASK;
		attr |= SLPD_CACHE_FLAG;
	}

	attr |= pbha << SLPD_PBHA_SHIFT;

	if (size == SPAGE_SIZE) {
		if (WARN_ON(!lv2ent_unmapped(pent)))
			return -EADDRINUSE;

		*pent = make_sysmmu_pte(paddr, SPAGE_FLAG, attr);
		pgtable_flush(pent, pent + 1);
		atomic_inc(pgcnt);
	} else {	/* size == LPAGE_SIZE */
		unsigned long i;

		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (WARN_ON(!lv2ent_unmapped(pent))) {
				clear_lv2_page_table(pent - i, i);
				return -EADDRINUSE;
			}

			*pent = make_sysmmu_pte(paddr, LPAGE_FLAG, attr);
		}
		pgtable_flush(pent - SPAGES_PER_LPAGE, pent);
		atomic_add(SPAGES_PER_LPAGE, pgcnt);
	}

	return 0;
}

static int samsung_sysmmu_map(struct iommu_domain *dom, unsigned long l_iova, phys_addr_t paddr,
			      size_t size, int prot, gfp_t unused)
{
	struct samsung_sysmmu_domain *domain = to_sysmmu_domain(dom);
	sysmmu_iova_t iova = (sysmmu_iova_t)l_iova;
	atomic_t *lv2entcnt = &domain->lv2entcnt[lv1ent_offset(iova)];
	sysmmu_pte_t *entry;
	int ret = -ENOMEM;

	/* Do not use IO coherency if iOMMU_PRIV exists */
	if (!!(prot & IOMMU_PRIV))
		prot &= ~IOMMU_CACHE;

	entry = section_entry(domain->page_table, iova);

	if (size == SECT_SIZE) {
		ret = lv1set_section(domain, entry, iova, paddr, prot, lv2entcnt);
	} else {
		sysmmu_pte_t *pent;

		pent = alloc_lv2entry(domain, entry, iova, lv2entcnt);

		if (IS_ERR(pent))
			ret = PTR_ERR(pent);
		else
			ret = lv2set_page(pent, paddr, size, prot, lv2entcnt,
					  domain->cacheable_override);
	}

	if (ret)
		pr_err("failed to map %#zx @ %#llx, ret:%d\n", size, iova, ret);
	else
		SYSMMU_EVENT_LOG_RANGE(domain, SYSMMU_EVENT_MAP, iova, iova + size);

	return ret;
}

static size_t samsung_sysmmu_unmap(struct iommu_domain *dom, unsigned long l_iova, size_t size,
				   struct iommu_iotlb_gather *gather)
{
	struct samsung_sysmmu_domain *domain = to_sysmmu_domain(dom);
	sysmmu_iova_t iova = (sysmmu_iova_t)l_iova;
	atomic_t *lv2entcnt = &domain->lv2entcnt[lv1ent_offset(iova)];
	sysmmu_pte_t *sent, *pent;
	size_t err_pgsize;

	sent = section_entry(domain->page_table, iova);

	if (lv1ent_section(sent)) {
		if (WARN_ON(size < SECT_SIZE)) {
			err_pgsize = SECT_SIZE;
			goto err;
		}

		*sent = 0;
		pgtable_flush(sent, sent + 1);
		size = SECT_SIZE;
		goto done;
	}

	if (unlikely(lv1ent_unmapped(sent))) {
		if (size > SECT_SIZE)
			size = SECT_SIZE;
		goto done;
	}

	/* lv1ent_page(sent) == true here */

	pent = page_entry(sent, iova);

	if (unlikely(lv2ent_unmapped(pent))) {
		size = SPAGE_SIZE;
		goto done;
	}

	if (lv2ent_small(pent)) {
		*pent = 0;
		size = SPAGE_SIZE;
		pgtable_flush(pent, pent + 1);
		atomic_dec(lv2entcnt);
		goto done;
	}

	/* lv1ent_large(pent) == true here */
	if (WARN_ON(size < LPAGE_SIZE)) {
		err_pgsize = LPAGE_SIZE;
		goto err;
	}

	clear_lv2_page_table(pent, SPAGES_PER_LPAGE);
	pgtable_flush(pent, pent + SPAGES_PER_LPAGE);
	size = LPAGE_SIZE;
	atomic_sub(SPAGES_PER_LPAGE, lv2entcnt);

done:
	iommu_iotlb_gather_add_page(dom, gather, iova, size);
	SYSMMU_EVENT_LOG_RANGE(domain, SYSMMU_EVENT_UNMAP, iova, iova + size);

	return size;

err:
	pr_err("failed: size(%#zx) @ %#llx is smaller than page size %#zx\n",
	       size, iova, err_pgsize);
	return 0;
}

static void samsung_sysmmu_flush_iotlb_all(struct iommu_domain *dom)
{
	unsigned long flags;
	struct samsung_sysmmu_domain *domain = to_sysmmu_domain(dom);
	struct list_head *sysmmu_list;
	struct sysmmu_drvdata *drvdata;

	/*
	 * domain->group might be NULL if flush_iotlb_all is called
	 * before attach_dev. Just ignore it.
	 */
	if (!domain->group)
		return;

	sysmmu_list = iommu_group_get_iommudata(domain->group);

	list_for_each_entry(drvdata, sysmmu_list, list) {
		spin_lock_irqsave(&drvdata->lock, flags);
		if (drvdata->attached_count && drvdata->rpm_count > 0)
			__sysmmu_invalidate_all(drvdata);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static void samsung_sysmmu_iotlb_sync(struct iommu_domain *dom, struct iommu_iotlb_gather *gather)
{
	unsigned long flags;
	struct samsung_sysmmu_domain *domain = to_sysmmu_domain(dom);
	struct list_head *sysmmu_list;
	struct sysmmu_drvdata *drvdata;

	/*
	 * domain->group might be NULL if iotlb_sync is called
	 * before attach_dev. Just ignore it.
	 */
	if (!domain->group)
		return;

	sysmmu_list = iommu_group_get_iommudata(domain->group);

	list_for_each_entry(drvdata, sysmmu_list, list) {
		spin_lock_irqsave(&drvdata->lock, flags);
		if (drvdata->attached_count && drvdata->rpm_count > 0)
			__sysmmu_invalidate(drvdata, gather->start, gather->end);
		SYSMMU_EVENT_LOG_RANGE(drvdata, SYSMMU_EVENT_IOTLB_SYNC,
				       gather->start, gather->end);
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static phys_addr_t samsung_sysmmu_iova_to_phys(struct iommu_domain *dom, dma_addr_t d_iova)
{
	struct samsung_sysmmu_domain *domain = to_sysmmu_domain(dom);
	sysmmu_iova_t iova = (sysmmu_iova_t)d_iova;
	sysmmu_pte_t *entry;
	phys_addr_t phys = 0;

	entry = section_entry(domain->page_table, iova);

	if (lv1ent_section(entry)) {
		phys = section_phys(entry) + section_offs(iova);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, iova);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry) + lpage_offs(iova);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry) + spage_offs(iova);
	}

	return phys;
}

void samsung_sysmmu_dump_pagetable(struct device *dev, dma_addr_t iova)
{
}

static struct iommu_device *samsung_sysmmu_probe_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct sysmmu_clientdata *client;
	int i;

	if (!fwspec) {
		dev_dbg(dev, "IOMMU instance data is not initialized\n");
		return ERR_PTR(-ENODEV);
	}

	if (fwspec->ops != &samsung_sysmmu_ops) {
		dev_err(dev, "has different IOMMU ops\n");
		iommu_fwspec_free(dev);
		return ERR_PTR(-ENODEV);
	}

	client = (struct sysmmu_clientdata *)dev_iommu_priv_get(dev);
	if (!client) {
		dev_info(dev, "IOMMU private data is not initialized\n");
		return ERR_PTR(-ENODEV);
	} else if (client->dev_link) {
		dev_info(dev, "is already added. It's okay.\n");
		return &client->sysmmus[0]->iommu;
	}
	client->dev_link = kcalloc(client->sysmmu_count, sizeof(*client->dev_link), GFP_KERNEL);
	if (!client->dev_link)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < client->sysmmu_count; i++) {
		client->dev_link[i] = device_link_add(dev, client->sysmmus[i]->dev,
						      DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
		if (!client->dev_link[i]) {
			dev_err(dev, "failed to add device link of %s\n",
				dev_name(client->sysmmus[i]->dev));
			while (i-- > 0)
				device_link_del(client->dev_link[i]);
			return ERR_PTR(-EINVAL);
		}
		dev_info(dev, "device link to %s\n",
			 dev_name(client->sysmmus[i]->dev));
	}

	return &client->sysmmus[0]->iommu;
}

static void samsung_sysmmu_release_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct sysmmu_clientdata *client;
	int i;

	if (!fwspec || fwspec->ops != &samsung_sysmmu_ops)
		return;

	client = (struct sysmmu_clientdata *)dev_iommu_priv_get(dev);
	for (i = 0; i < client->sysmmu_count; i++)
		device_link_del(client->dev_link[i]);
	kfree(client->dev_link);

	iommu_fwspec_free(dev);
}

static void samsung_sysmmu_group_data_release(void *iommu_data)
{
	kfree(iommu_data);
}

static struct iommu_group *samsung_sysmmu_device_group(struct device *dev)
{
	struct iommu_group *group;
	struct device_node *np;
	struct platform_device *pdev;
	struct list_head *list;

	if (device_iommu_mapped(dev))
		return iommu_group_get(dev);

	np = of_parse_phandle(dev->of_node, "samsung,iommu-group", 0);
	if (!np) {
		dev_err(dev, "group is not registered\n");
		return ERR_PTR(-ENODEV);
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_err(dev, "no device in device_node[%s]\n", np->name);
		of_node_put(np);
		return ERR_PTR(-ENODEV);
	}

	group = platform_get_drvdata(pdev);
	if (!group) {
		dev_err(dev, "no group in device_node[%s]\n", np->name);
		of_node_put(np);
		return ERR_PTR(-EPROBE_DEFER);
	}

	of_node_put(np);

	mutex_lock(&sysmmu_global_mutex);
	if (iommu_group_get_iommudata(group)) {
		mutex_unlock(&sysmmu_global_mutex);
		return group;
	}

	list = kzalloc(sizeof(*list), GFP_KERNEL);
	if (!list) {
		mutex_unlock(&sysmmu_global_mutex);
		return ERR_PTR(-ENOMEM);
	}
	INIT_LIST_HEAD(list);
	iommu_group_set_iommudata(group, list, samsung_sysmmu_group_data_release);

	mutex_unlock(&sysmmu_global_mutex);

	return group;
}

static void samsung_sysmmu_clientdata_release(struct device *dev, void *res)
{
	struct sysmmu_clientdata *client = res;

	kfree(client->sysmmus);
}

static int samsung_sysmmu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	struct platform_device *sysmmu;
	struct sysmmu_drvdata *data;
	struct sysmmu_drvdata **new_link;
	struct sysmmu_clientdata *client;
	struct iommu_fwspec *fwspec;
	unsigned int fwid = 0;
	int ret;

	sysmmu = of_find_device_by_node(args->np);
	if (!sysmmu) {
		dev_err(dev, "failed to find sysmmu device.\n");
		return -EPROBE_DEFER;
	}

	data = platform_get_drvdata(sysmmu);
	if (!data || !data->initialized_sysmmu) {
		dev_err(dev, "has a no initialized sysmmu %s\n",
			data ? dev_name(data->dev) : sysmmu->name);

		client = dev_iommu_priv_get(dev);
		if (client) {
			kfree(client->sysmmus);
			kfree(client);
		}
		iommu_fwspec_free(dev);
		return -EPROBE_DEFER;
	}

	ret = iommu_fwspec_add_ids(dev, &fwid, 1);
	if (ret) {
		dev_err(dev, "failed to add fwspec. (err:%d)\n", ret);
		return ret;
	}

	fwspec = dev_iommu_fwspec_get(dev);
	if (!dev_iommu_priv_get(dev)) {
		client = devres_alloc(samsung_sysmmu_clientdata_release,
				      sizeof(*client), GFP_KERNEL);
		if (!client)
			return -ENOMEM;
		client->dev = dev;
		dev_iommu_priv_set(dev, client);
		devres_add(dev, client);
	}

	client = (struct sysmmu_clientdata *) dev_iommu_priv_get(dev);
	new_link = krealloc(client->sysmmus, sizeof(data) * (client->sysmmu_count + 1), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(new_link))
		return -ENOMEM;

	client->sysmmus = new_link;
	client->sysmmus[client->sysmmu_count++] = data;

	dev_info(dev, "has sysmmu %s (total count:%d)\n",
		 dev_name(data->dev), client->sysmmu_count);

	return ret;
}

static void samsung_sysmmu_put_resv_regions(struct device *dev, struct list_head *head)
{
	struct iommu_resv_region *entry, *next;

	list_for_each_entry_safe(entry, next, head, list)
		kfree(entry);
}

#define NR_IOMMU_RESERVED_TYPES 2
static int samsung_sysmmu_get_resv_regions_by_node(struct device_node *np, struct device *dev,
						   struct list_head *head)
{
	const char *propname[NR_IOMMU_RESERVED_TYPES] = {
		"samsung,iommu-identity-map",
		"samsung,iommu-reserved-map"
	};
	enum iommu_resv_type resvtype[NR_IOMMU_RESERVED_TYPES] = {
		IOMMU_RESV_DIRECT, IOMMU_RESV_RESERVED
	};
	int n_addr_cells = of_n_addr_cells(dev->of_node);
	int n_size_cells = of_n_size_cells(dev->of_node);
	int n_all_cells = n_addr_cells + n_size_cells;
	int type;

	for (type = 0; type < 2; type++) {
		const __be32 *prop;
		u64 base, size;
		int i, cnt;

		prop = of_get_property(dev->of_node, propname[type], &cnt);
		if (!prop)
			continue;

		cnt /=  sizeof(u32);
		if (cnt % n_all_cells != 0) {
			dev_err(dev, "Invalid number(%d) of values in %s\n", cnt, propname[type]);
			return -EINVAL;
		}

		for (i = 0; i < cnt; i += n_all_cells) {
			struct iommu_resv_region *region;

			base = of_read_number(prop + i, n_addr_cells);
			size = of_read_number(prop + i + n_addr_cells, n_size_cells);
			if (base & ~dma_get_mask(dev) || (base + size) & ~dma_get_mask(dev)) {
				dev_err(dev, "Unreachable DMA region in %s, [%#llx..%#llx)\n",
					propname[type], base, base + size);
				return -EINVAL;
			}

			region = iommu_alloc_resv_region(base, size, 0, resvtype[type], GFP_KERNEL);
			if (!region)
				return -ENOMEM;

			list_add_tail(&region->list, head);
			dev_info(dev, "Reserved IOMMU mapping [%#llx..%#llx)\n", base, base + size);
		}
	}

	return 0;
}

static void samsung_sysmmu_get_resv_regions(struct device *dev, struct list_head *head)
{
	struct device_node *curr_node, *target_node, *node;
	struct platform_device *pdev;
	int ret;

	target_node = of_parse_phandle(dev->of_node, "samsung,iommu-group", 0);
	if (!target_node) {
		dev_err(dev, "doesn't have iommu-group property\n");
		return;
	}

	for_each_node_with_property(node, "samsung,iommu-group") {
		curr_node = of_parse_phandle(node, "samsung,iommu-group", 0);
		if (!curr_node || curr_node != target_node) {
			of_node_put(curr_node);
			continue;
		}

		pdev = of_find_device_by_node(node);
		if (!pdev) {
			of_node_put(curr_node);
			continue;
		}

		ret = samsung_sysmmu_get_resv_regions_by_node(dev->of_node, &pdev->dev, head);

		put_device(&pdev->dev);
		of_node_put(curr_node);

		if (ret)
			goto err;
	}

	of_node_put(target_node);

	return;
err:
	of_node_put(target_node);
	samsung_sysmmu_put_resv_regions(dev, head);
	INIT_LIST_HEAD(head);
}

static int samsung_sysmmu_def_domain_type(struct device *dev)
{
	struct device_node *np;
	int ret = 0;

	np = of_parse_phandle(dev->of_node, "samsung,iommu-group", 0);
	if (!np) {
		dev_err(dev, "group is not registered\n");
		return 0;
	}

	if (of_property_read_bool(np, "samsung,unmanaged-domain"))
		ret = IOMMU_DOMAIN_UNMANAGED;

	of_node_put(np);

	return ret;
}

static struct iommu_domain_ops	samsung_sysmmu_domain_ops = {
	.attach_dev		= samsung_sysmmu_attach_dev,
	.detach_dev		= samsung_sysmmu_detach_dev,
	.map			= samsung_sysmmu_map,
	.unmap			= samsung_sysmmu_unmap,
	.flush_iotlb_all	= samsung_sysmmu_flush_iotlb_all,
	.iotlb_sync		= samsung_sysmmu_iotlb_sync,
	.iova_to_phys		= samsung_sysmmu_iova_to_phys,
	.free			= samsung_sysmmu_domain_free,
};

static struct iommu_ops 	samsung_sysmmu_ops = {
	.capable		= samsung_sysmmu_capable,
	.domain_alloc		= samsung_sysmmu_domain_alloc,
	.probe_device		= samsung_sysmmu_probe_device,
	.release_device		= samsung_sysmmu_release_device,
	.device_group		= samsung_sysmmu_device_group,
	.get_resv_regions	= samsung_sysmmu_get_resv_regions,
	.of_xlate		= samsung_sysmmu_of_xlate,
	.def_domain_type	= samsung_sysmmu_def_domain_type,
	.default_domain_ops	= &samsung_sysmmu_domain_ops,
	.pgsize_bitmap		= SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE,
	.owner			= THIS_MODULE,
};

static int sysmmu_get_hw_info(struct sysmmu_drvdata *data)
{
	int ret, pmmu_idx;

	ret = pm_runtime_get_sync(data->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(data->dev);
		return ret;
	}

	data->version = __sysmmu_get_hw_version(data);
	data->max_vm = __sysmmu_get_num_vm(data);
	data->va_width = __sysmmu_get_va_width(data);
	data->num_pmmu = __sysmmu_get_num_pmmu(data);
	if (data->num_pmmu <= 0) {
		dev_err(data->dev, "failed to get num_pmmu h/w info\n");
		pm_runtime_put(data->dev);
		return -EINVAL;
	}

	data->num_stream_table = devm_kcalloc(data->dev, data->num_pmmu,
					      sizeof(*data->num_stream_table), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(data->num_stream_table)) {
		pm_runtime_put(data->dev);
		return -ENOMEM;
	}
	data->num_ptlb = devm_kcalloc(data->dev, data->num_pmmu,
				      sizeof(*data->num_ptlb), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(data->num_ptlb)) {
		pm_runtime_put(data->dev);
		return -ENOMEM;
	}
	for (pmmu_idx = 0; pmmu_idx < data->num_pmmu; pmmu_idx++) {
		data->num_stream_table[pmmu_idx] = __sysmmu_get_num_stream_table(data, pmmu_idx);
		data->num_ptlb[pmmu_idx] = __sysmmu_get_num_ptlb(data, pmmu_idx);
	}

	data->tlb_reg_set = sysmmu_tlb_reg_set[REG_IDX_DEFAULT];
	ret = of_property_read_u32(data->dev->of_node, "sysmmu,feature-info", &data->tlb_per_vm);
	if (ret)
		data->tlb_per_vm = __sysmmu_get_feature_info(data);

	if (data->tlb_per_vm)
		data->tlb_reg_set = sysmmu_tlb_reg_set[REG_IDX_VM];

	pm_runtime_put(data->dev);

	return 0;
}

#define PROP_NAME_LEN	40
static int sysmmu_parse_stream_property(struct device *dev, struct sysmmu_drvdata *drvdata,
					int pmmu_index)
{
	char default_props_name[PROP_NAME_LEN];
	char props_name[PROP_NAME_LEN];
	char num_stream_table_name[PROP_NAME_LEN];
	char num_ptlb_name[PROP_NAME_LEN];
	struct stream_props *props = &drvdata->props[pmmu_index];
	struct stream_config *cfg;
	struct device_node *stream_np = NULL;
	int i, readsize, cnt, num_stream_table, num_ptlb;
	int ret = 0;

	stream_np = of_parse_phandle(dev->of_node, "stream-property", 0);
	if (!stream_np) {
		dev_info(dev, "stream-property is not defined in SysMMU node\n");
		dev_info(dev, "It's okay but you can't use SysMMU's tlbs\n");
		props->default_cfg = DEFAULT_STREAM_NONE;
		return ret;
	}

	snprintf(default_props_name, PROP_NAME_LEN, "pmmu%d,default_stream", pmmu_index);
	snprintf(props_name, PROP_NAME_LEN, "pmmu%d,stream_property", pmmu_index);
	snprintf(num_stream_table_name, PROP_NAME_LEN, "pmmu%d,num_stream_table", pmmu_index);
	snprintf(num_ptlb_name, PROP_NAME_LEN, "pmmu%d,num_ptlb", pmmu_index);

	/* If there is a problem reading from H/W, must read from SysMMU dts */
	if (!of_property_read_u32_index(stream_np, num_stream_table_name, 0, &num_stream_table))
		drvdata->num_stream_table[pmmu_index] = num_stream_table;
	if (!of_property_read_u32_index(stream_np, num_ptlb_name, 0, &num_ptlb))
		drvdata->num_ptlb[pmmu_index] = num_ptlb;

	if (of_property_read_u32(stream_np, default_props_name, &props->default_cfg))
		props->default_cfg = DEFAULT_STREAM_NONE;

	cnt = of_property_count_elems_of_size(stream_np, props_name, sizeof(*cfg));
	if (cnt <= 0)
		goto put_stream_node;

	cfg = devm_kcalloc(dev, cnt, sizeof(*cfg), GFP_KERNEL);
	if (!cfg) {
		ret = -ENOMEM;
		goto put_stream_node;
	}

	readsize = cnt * sizeof(*cfg) / sizeof(u32);
	ret = of_property_read_variable_u32_array(stream_np, props_name, (u32 *)cfg,
						  readsize, readsize);
	if (ret < 0) {
		dev_err(dev, "failed to get stream property, return %d\n", ret);
		goto put_stream_node;
	}

	for (i = 0; i < cnt; i++) {
		if (cfg[i].index >= drvdata->num_stream_table[pmmu_index]) {
			dev_err(dev, "invalid index %d is ignored. (max:%d)\n",
				cfg[i].index, drvdata->num_stream_table[pmmu_index]);
			cfg[i].index = UNUSED_STREAM_INDEX;
		}
	}

	props->id_cnt = cnt;
	props->cfg = cfg;
	ret = 0;

put_stream_node:
	of_node_put(stream_np);
	return ret;
}

static int __sysmmu_secure_irq_init(struct device *sysmmu, struct sysmmu_drvdata *data)
{
	struct platform_device *pdev = to_platform_device(sysmmu);
	int ret;

	ret = platform_get_irq(pdev, 1);
	if (ret <= 0) {
		dev_err(sysmmu, "unable to find secure IRQ resource\n");
		return -EINVAL;
	}
	data->secure_irq = ret;

	ret = devm_request_threaded_irq(sysmmu, data->secure_irq, samsung_sysmmu_irq,
					samsung_sysmmu_irq_thread, IRQF_ONESHOT,
					dev_name(sysmmu), data);
	if (ret) {
		dev_err(sysmmu, "failed to set secure irq handler %d, ret:%d\n",
			data->secure_irq, ret);
		return ret;
	}

	ret = of_property_read_u32(sysmmu->of_node, "sysmmu,secure_base", &data->secure_base);
	if (ret) {
		dev_err(sysmmu, "failed to get secure base\n");
		return ret;
	}
	dev_info(sysmmu, "secure base = %#x\n", data->secure_base);

	return ret;
}

static int sysmmu_parse_dt(struct device *sysmmu, struct sysmmu_drvdata *data)
{
	unsigned int qos = DEFAULT_QOS_VALUE, mask;
	int ret, i;
	struct stream_props *props;

	/* Parsing QoS */
	ret = of_property_read_u32_index(sysmmu->of_node, "qos", 0, &qos);
	if (!ret && qos > 15) {
		dev_err(sysmmu, "Invalid QoS value %d, use default.\n", qos);
		qos = DEFAULT_QOS_VALUE;
	}
	data->qos = qos;

	/* Secure IRQ */
	if (of_find_property(sysmmu->of_node, "sysmmu,secure-irq", NULL)) {
		ret = __sysmmu_secure_irq_init(sysmmu, data);
		if (ret) {
			dev_err(sysmmu, "failed to init secure irq\n");
			return ret;
		}
	}

	/* use async fault mode */
	data->async_fault_mode = of_property_read_bool(sysmmu->of_node, "sysmmu,async-fault");

	/* use cacheable override mode */
	data->cacheable_override_mode = of_property_read_bool(sysmmu->of_node,
							      "sysmmu,cacheable-override");
	/* use performance mode */
	data->perf_mode = of_property_read_bool(sysmmu->of_node, "sysmmu,performance");

	ret = of_property_read_u32_index(sysmmu->of_node, "vmid_mask", 0, &mask);
	if (!ret && (mask & ((1 << data->max_vm) - 1)))
		data->vmid_mask = mask;

	props = devm_kcalloc(sysmmu, data->num_pmmu, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;
	data->props = props;

	for (i = 0; i < data->num_pmmu; i++) {
		ret = sysmmu_parse_stream_property(sysmmu, data, i);
		if (ret)
			dev_err(sysmmu, "Failed to parse TLB property\n");
	}
	return ret;
}

static int samsung_sysmmu_device_probe(struct platform_device *pdev)
{
	struct sysmmu_drvdata *data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irq, ret, err = 0;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get resource info\n");
		return -ENOENT;
	}

	data->sfrbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->sfrbase))
		return PTR_ERR(data->sfrbase);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(dev, irq, samsung_sysmmu_irq, samsung_sysmmu_irq_thread,
					IRQF_ONESHOT, dev_name(dev), data);
	if (ret) {
		dev_err(dev, "unabled to register handler of irq %d\n", irq);
		return ret;
	}

	data->clk = devm_clk_get(dev, "gate");
	if (PTR_ERR(data->clk) == -ENOENT) {
		dev_info(dev, "no gate clock exists. it's okay.\n");
		data->clk = NULL;
	} else if (IS_ERR(data->clk)) {
		dev_err(dev, "failed to get clock!\n");
		return PTR_ERR(data->clk);
	}

	INIT_LIST_HEAD(&data->list);
	spin_lock_init(&data->lock);
	data->dev = dev;
	platform_set_drvdata(pdev, data);

	err = samsung_iommu_init_log(&data->log, SYSMMU_EVENT_MAX);
	if (err) {
		dev_err(dev, "failed to initialize log\n");
		return err;
	}

	pm_runtime_enable(dev);
	ret = sysmmu_get_hw_info(data);
	if (ret) {
		dev_err(dev, "failed to get h/w info\n");
		goto err_get_hw_info;
	}
	data->vmid_mask = SYSMMU_MASK_VMID;

	ret = sysmmu_parse_dt(data->dev, data);
	if (ret)
		goto err_get_hw_info;

	ret = iommu_device_sysfs_add(&data->iommu, data->dev, NULL, dev_name(dev));
	if (ret) {
		dev_err(dev, "failed to register iommu in sysfs\n");
		goto err_get_hw_info;
	}

	err = iommu_device_register(&data->iommu, &samsung_sysmmu_ops, dev);
	if (err) {
		dev_err(dev, "failed to register iommu\n");
		goto err_iommu_register;
	}

	data->initialized_sysmmu = true;

	dev_info(dev, "initialized IOMMU. Ver %d.%d.%d\n",
		 MMU_VERSION_MAJOR(data->version),
		 MMU_VERSION_MINOR(data->version),
		 MMU_VERSION_REVISION(data->version));
	return 0;

err_iommu_register:
	iommu_device_sysfs_remove(&data->iommu);
err_get_hw_info:
	pm_runtime_disable(dev);
	samsung_iommu_deinit_log(&data->log);
	return err;
}

static void samsung_sysmmu_device_shutdown(struct platform_device *pdev)
{
}

static int __maybe_unused samsung_sysmmu_runtime_suspend(struct device *sysmmu)
{
	unsigned long flags;
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);

	spin_lock_irqsave(&drvdata->lock, flags);
	drvdata->rpm_count--;
	if (drvdata->attached_count > 0)
		__sysmmu_disable(drvdata);
	spin_unlock_irqrestore(&drvdata->lock, flags);

	SYSMMU_EVENT_LOG_RANGE(drvdata, SYSMMU_EVENT_POWEROFF,
			       drvdata->rpm_count, drvdata->attached_count);
	return 0;
}

static int __maybe_unused samsung_sysmmu_runtime_resume(struct device *sysmmu)
{
	unsigned long flags;
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);

	spin_lock_irqsave(&drvdata->lock, flags);
	drvdata->rpm_count++;
	if (drvdata->attached_count > 0)
		__sysmmu_enable(drvdata);
	spin_unlock_irqrestore(&drvdata->lock, flags);

	SYSMMU_EVENT_LOG_RANGE(drvdata, SYSMMU_EVENT_POWERON,
			       drvdata->rpm_count, drvdata->attached_count);
	return 0;
}

static int __maybe_unused samsung_sysmmu_suspend(struct device *dev)
{
	dev->power.must_resume = true;

	if (pm_runtime_status_suspended(dev))
		return 0;

	return samsung_sysmmu_runtime_suspend(dev);
}

static int __maybe_unused samsung_sysmmu_resume(struct device *dev)
{
	if (pm_runtime_status_suspended(dev))
		return 0;

	return samsung_sysmmu_runtime_resume(dev);
}

static const struct dev_pm_ops samsung_sysmmu_pm_ops = {
	SET_RUNTIME_PM_OPS(samsung_sysmmu_runtime_suspend, samsung_sysmmu_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(samsung_sysmmu_suspend, samsung_sysmmu_resume)
};

static const struct of_device_id sysmmu_of_match[] = {
	{ .compatible = "samsung,sysmmu-v9" },
	{ }
};

static struct platform_driver samsung_sysmmu_driver_v9 = {
	.driver	= {
		.name			= "samsung-sysmmu-v9",
		.of_match_table		= of_match_ptr(sysmmu_of_match),
		.pm			= &samsung_sysmmu_pm_ops,
		.suppress_bind_attrs	= true,
	},
	.probe	= samsung_sysmmu_device_probe,
	.shutdown = samsung_sysmmu_device_shutdown,
};

static struct iova *to_iova(struct rb_node *node)
{
	return rb_entry(node, struct iova, node);
}

/* Insert the iova into domain rbtree by holding writer lock */
static void iova_insert_rbtree(struct rb_root *root, struct iova *iova, struct rb_node *start)
{
	struct rb_node **new, *parent = NULL;

	new = (start) ? &start : &(root->rb_node);
	/* Figure out where to put new node */
	while (*new) {
		struct iova *this = to_iova(*new);

		parent = *new;

		if (iova->pfn_lo < this->pfn_lo)
			new = &((*new)->rb_left);
		else if (iova->pfn_lo > this->pfn_lo)
			new = &((*new)->rb_right);
		else {
			WARN_ON(1); /* this should not happen */
			return;
		}
	}
	/* Add new node and rebalance tree. */
	rb_link_node(&iova->node, parent, new);
	rb_insert_color(&iova->node, root);
}

static void alloc_insert_iova_best_fit(void *data, struct iova_domain *iovad,
					  unsigned long size, unsigned long limit_pfn,
					  struct iova *new_iova, bool size_aligned, int *ret)
{
	struct rb_node *curr, *prev;
	struct iova *curr_iova, *prev_iova;
	unsigned long flags;
	unsigned long align_mask = ~0UL;
	struct rb_node *candidate_rb_parent;
	unsigned long new_pfn, candidate_pfn = ~0UL;
	unsigned long gap, candidate_gap = ~0UL;

	if (!iovad->android_vendor_data1) {
		*ret = -EINVAL;
		return;
	}

	if (size_aligned) {
		unsigned long shift = fls_long(size - 1);
		trace_android_rvh_iommu_limit_align_shift(iovad, size, &shift);
		align_mask <<= shift;
	}

	/* Walk the tree backwards */
	spin_lock_irqsave(&iovad->iova_rbtree_lock, flags);
	curr = &iovad->anchor.node;
	prev = rb_prev(curr);
	for (; prev; curr = prev, prev = rb_prev(curr)) {
		curr_iova = rb_entry(curr, struct iova, node);
		prev_iova = rb_entry(prev, struct iova, node);

		limit_pfn = min(limit_pfn, curr_iova->pfn_lo);
		new_pfn = (limit_pfn - size) & align_mask;
		gap = curr_iova->pfn_lo - prev_iova->pfn_hi - 1;
		if ((limit_pfn >= size) && (new_pfn > prev_iova->pfn_hi)
				&& (gap < candidate_gap)) {
			candidate_gap = gap;
			candidate_pfn = new_pfn;
			candidate_rb_parent = curr;
			if (gap == size)
				goto insert;
		}
	}

	curr_iova = rb_entry(curr, struct iova, node);
	limit_pfn = min(limit_pfn, curr_iova->pfn_lo);
	new_pfn = (limit_pfn - size) & align_mask;
	gap = curr_iova->pfn_lo - iovad->start_pfn;
	if (limit_pfn >= size && new_pfn >= iovad->start_pfn &&
			gap < candidate_gap) {
		candidate_gap = gap;
		candidate_pfn = new_pfn;
		candidate_rb_parent = curr;
	}

insert:
	if (candidate_pfn == ~0UL) {
		spin_unlock_irqrestore(&iovad->iova_rbtree_lock, flags);
		*ret = -ENOMEM;
		return;
	}

	/* pfn_lo will point to size aligned address if size_aligned is set */
	new_iova->pfn_lo = candidate_pfn;
	new_iova->pfn_hi = new_iova->pfn_lo + size - 1;

	/* If we have 'prev', it's a valid place to start the insertion. */
	iova_insert_rbtree(&iovad->rbroot, new_iova, candidate_rb_parent);
	spin_unlock_irqrestore(&iovad->iova_rbtree_lock, flags);
	*ret = 0;
}

static void iovad_init_best_fit(void *data, struct device *dev, struct iova_domain *iovad)
{
	if(of_property_read_bool(dev->of_node, "sysmmu,best-fit"))
		iovad->android_vendor_data1 = true;
}

static int __init samsung_sysmmu_init(void)
{
	int err;

	register_trace_android_rvh_iommu_iovad_init_alloc_algo(iovad_init_best_fit, NULL);
	register_trace_android_rvh_iommu_alloc_insert_iova(alloc_insert_iova_best_fit, NULL);

	slpt_cache = kmem_cache_create("samsung-iommu-lv2table", LV2TABLE_SIZE, LV2TABLE_SIZE,
				       0, NULL);
	if (!slpt_cache)
		return -ENOMEM;

	device_initialize(&sync_dev);

	err = platform_driver_register(&samsung_sysmmu_driver_v9);
	if (err < 0) {
		pr_err("Failed to register samsung-iommu-v9 driver\n");
		kmem_cache_destroy(slpt_cache);
		return err;
	}

	return 0;
}

static void __exit samsung_sysmmu_exit(void)
{
	kmem_cache_destroy(slpt_cache);
	platform_driver_unregister(&samsung_sysmmu_driver_v9);
}

module_init(samsung_sysmmu_init);
module_exit(samsung_sysmmu_exit);
MODULE_SOFTDEP("pre: samsung-iommu-group-v9");
MODULE_LICENSE("GPL v2");
