// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: debug " fmt

/*
 * For IOMMU EP/bring up phase, you must be enable "IOMMU_BRING_UP".
 * If you need to do some special config, you can also use this macro.
 */
#define IOMMU_BRING_UP	(0)

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/io-pgtable.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#include <dt-bindings/memory/mtk-memory-port.h>
#include <trace/hooks/iommu.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE) && !IOMMU_BRING_UP
#include <aee.h>
#endif
#include "mtk_iommu.h"
#include "iommu_debug.h"
#include "iommu_port.h"
#include "io-pgtable-arm.h"
#include "smmu_reg.h"

#include "../../../iommu/arm/arm-smmu-v3/arm-smmu-v3.h"

#define ERROR_LARB_PORT_ID		0xFFFF
#define F_MMU_INT_TF_MSK		GENMASK(12, 2)
#define F_MMU_INT_TF_CCU_MSK		GENMASK(12, 7)
#define F_MMU_INT_TF_LARB(id)		FIELD_GET(GENMASK(13, 7), id)
#define F_MMU_INT_TF_PORT(id)		FIELD_GET(GENMASK(6, 2), id)
#define F_APU_MMU_INT_TF_MSK(id)	FIELD_GET(GENMASK(11, 7), id)

#define F_MMU_INT_TF_SPEC_MSK(port_s_b)		GENMASK(12, port_s_b)
#define F_MMU_INT_TF_SPEC_LARB(id, larb_s_b) \
	FIELD_GET(GENMASK(12, larb_s_b), id)
#define F_MMU_INT_TF_SPEC_PORT(id, larb_s_b, port_s_b) \
	FIELD_GET(GENMASK((larb_s_b-1), port_s_b), id)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE) && !IOMMU_BRING_UP
#define m4u_aee_print(string, args...) do {\
		char m4u_name[150];\
		if (snprintf(m4u_name, 150, "[M4U]"string, ##args) < 0) \
			break; \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_DUMP_DISPLAY, \
		m4u_name, "[M4U] error"string, ##args); \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)

#else
#define m4u_aee_print(string, args...) do {\
		char m4u_name[150];\
		if (snprintf(m4u_name, 150, "[M4U]"string, ##args) < 0) \
			break; \
	pr_err("[M4U] error:"string, ##args);  \
	} while (0)
#endif

#define MAU_CONFIG_INIT(iommu_type, iommu_id, slave, mau, start, end,\
	port_mask, larb_mask, wr, virt, io, start_bit32, end_bit32) {\
	iommu_type, iommu_id, slave, mau, start, end, port_mask, larb_mask,\
	wr, virt, io, start_bit32, end_bit32\
}

#define mmu_translation_log_format \
	"CRDISPATCH_KEY:%s_%s\ntranslation fault:port=%s,mva=0x%llx,pa=0x%llx\n"

#define mau_assert_log_format \
	"CRDISPATCH_KEY:IOMMU\nMAU ASRT:ASRT_ID=0x%x,FALUT_ID=0x%x(%s),ADDR=0x%x(0x%x)\n"

#define FIND_IOVA_TIMEOUT_NS		(1000000 * 5) /* 5ms! */
#define MAP_IOVA_TIMEOUT_NS		(1000000 * 5) /* 5ms! */

#define IOVA_DUMP_LOG_MAX		(100)

#define IOVA_DUMP_RS_INTERVAL		DEFAULT_RATELIMIT_INTERVAL
#define IOVA_DUMP_RS_BURST		(1)

struct mtk_iommu_cb {
	int port;
	mtk_iommu_fault_callback_t fault_fn;
	void *fault_data;
};

struct mtk_m4u_data {
	struct device			*dev;
	struct proc_dir_entry	*debug_root;
	struct mtk_iommu_cb		*m4u_cb;
	const struct mtk_m4u_plat_data	*plat_data;
};

/* max value of TYPE_NUM and SMMU_TYPE_NUM */
#define MAX_IOMMU_NUM	4
struct mtk_m4u_plat_data {
	struct peri_iommu_data		*peri_data;
	const struct mtk_iommu_port	*port_list[MAX_IOMMU_NUM];
	u32				port_nr[MAX_IOMMU_NUM];
	const struct mau_config_info	*mau_config;
	u32				mau_config_nr;
	u32				mm_tf_ccu_support;
	u32 (*get_valid_tf_id)(int tf_id, u32 type, int id);
	bool (*tf_id_is_match)(int tf_id, u32 type, int id,
			       struct mtk_iommu_port port);
	int (*mm_tf_is_gce_videoup)(u32 port_tf, u32 vld_tf);
	char *(*peri_tf_analyse)(enum peri_iommu bus_id, u32 id);
	int (*smmu_common_id)(u32 type, u32 tbu_id);
	char *(*smmu_port_name)(u32 type, int id, int tf_id);
};

struct peri_iommu_data {
	enum peri_iommu id;
	u32 bus_id;
};

static struct mtk_m4u_data *m4u_data;
static bool smmu_v3_enable;
static const struct mtk_iommu_ops *iommu_ops;

/**********iommu trace**********/
#define IOMMU_EVENT_COUNT_MAX	(8000)

#define iommu_dump(file, fmt, args...) \
	do {\
		if (file)\
			seq_printf(file, fmt, ##args);\
		else\
			pr_info(fmt, ##args);\
	} while (0)

struct iommu_event_mgr_t {
	char name[11];
	unsigned int dump_trace;
	unsigned int dump_log;
};

static struct iommu_event_mgr_t event_mgr[IOMMU_EVENT_MAX];

struct iommu_event_t {
	unsigned int event_id;
	u64 time_high;
	u32 time_low;
	unsigned long data1;
	unsigned long data2;
	unsigned long data3;
	struct device *dev;
};

struct iommu_global_t {
	unsigned int enable;
	unsigned int dump_enable;
	unsigned int map_record;
	unsigned int start;
	unsigned int write_pointer;
	spinlock_t	lock;
	struct iommu_event_t *record;
};

static struct iommu_global_t iommu_globals;

/* iova statistics info for size and count */
#define IOVA_DUMP_TOP_MAX	(10)

struct iova_count_info {
	u64 tab_id;
	u32 dom_id;
	struct device *dev;
	u64 size;
	u32 count;
	struct list_head list_node;
};

struct iova_count_list {
	spinlock_t		lock;
	struct list_head	head;
};

static struct iova_count_list count_list = {};

enum mtk_iova_space {
	MTK_IOVA_SPACE0, /* 0GB ~ 4GB */
	MTK_IOVA_SPACE1, /* 4GB ~ 8GB */
	MTK_IOVA_SPACE2, /* 8GB ~ 12GB */
	MTK_IOVA_SPACE3, /* 12GB ~ 16GB */
	MTK_IOVA_SPACE_NUM
};

/* iova alloc info */
struct iova_info {
	u64 tab_id;
	u32 dom_id;
	struct device *dev;
	struct iova_domain *iovad;
	dma_addr_t iova;
	size_t size;
	u64 time_high;
	u32 time_low;
	struct list_head list_node;
};

struct iova_buf_list {
	atomic_t init_flag;
	struct list_head head;
	spinlock_t lock;
};

static struct iova_buf_list iova_list = {.init_flag = ATOMIC_INIT(0)};

/* iova map info */
struct iova_map_info {
	u64			tab_id;
	u64			iova;
	u64			time_high;
	u32			time_low;
	size_t			size;
	struct list_head	list_node;
};

struct iova_map_list {
	atomic_t		init_flag;
	spinlock_t		lock;
	struct list_head	head[MTK_IOVA_SPACE_NUM];
};

static struct iova_map_list map_list = {.init_flag = ATOMIC_INIT(0)};

static void mtk_iommu_iova_trace(int event, dma_addr_t iova, size_t size,
				 u64 tab_id, struct device *dev);
static void mtk_iommu_iova_alloc_dump_top(struct seq_file *s,
					  struct device *dev);
static void mtk_iommu_iova_alloc_dump(struct seq_file *s, struct device *dev);
static void mtk_iommu_iova_map_dump(struct seq_file *s, u64 iova, u64 tab_id);
static void mtk_iommu_iova_dump(struct seq_file *s, u64 iova, u64 tab_id);

static void mtk_iommu_system_time(u64 *high, u32 *low)
{
	u64 temp;

	temp = sched_clock();
	do_div(temp, 1000);
	*low = do_div(temp, 1000000);
	*high = temp;
}

static u64 to_system_time(u64 high, u32 low)
{
	return (high * 1000000 + low) * 1000;
}

static void mtk_iova_map_latency_check(u64 tab_id, u64 iova, size_t size,
				       u64 end_time_high, u64 end_time_low)
{
	u64 start_t, end_t, alloc_time_high, alloc_time_low, latency_time;
	struct iova_info *plist;
	struct iova_info *tmp_plist;
	struct device *dev = NULL;
	int i = 0;

	spin_lock(&iova_list.lock);
	start_t = sched_clock();
	list_for_each_entry_safe(plist, tmp_plist, &iova_list.head, list_node) {
		i++;
		if (plist->iova == iova &&
		    plist->size == size &&
			plist->tab_id == tab_id) {
			dev = plist->dev;
			alloc_time_high = plist->time_high;
			alloc_time_low = plist->time_low;
			break;
		}
	}
	end_t = sched_clock();
	spin_unlock(&iova_list.lock);

	if ((end_t - start_t) > FIND_IOVA_TIMEOUT_NS)
		pr_info("%s warnning, find iova:[0x%llx 0x%llx 0x%zx] in %d timeout:%llu\n",
			__func__, tab_id, iova, size, i, (end_t - start_t));

	if (dev == NULL) {
		pr_info("%s warnning, iova:[0x%llx 0x%llx 0x%zx] not find in %d\n",
			__func__, tab_id, iova, size, i);
	} else {
		/* iova map latency check and print warnning log */
		start_t = to_system_time(alloc_time_high, alloc_time_low);
		end_t = to_system_time(end_time_high, end_time_low);
		latency_time = end_t - start_t;
		if (latency_time > MAP_IOVA_TIMEOUT_NS) {
			pr_info("%s warnning, dev:%s, %llu, %llu.%06llu, iova:[0x%llx 0x%llx 0x%zx]\n",
				__func__, dev_name(dev), latency_time,
				(end_time_high - alloc_time_high),
				(end_time_low - alloc_time_low),
				tab_id, iova, size);
		}
	}
}

void mtk_iova_map(u64 tab_id, u64 iova, size_t size)
{
	u32 id = (iova >> 32);
	unsigned long flags;
	struct iova_map_info *iova_buf;
	u64 time_high;
	u32 time_low;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	if (iommu_globals.map_record == 0)
		goto iova_trace;

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_ATOMIC);
	if (!iova_buf)
		return;

	mtk_iommu_system_time(&(iova_buf->time_high), &(iova_buf->time_low));
	iova_buf->tab_id = tab_id;
	iova_buf->iova = iova;
	iova_buf->size = size;
	time_high = iova_buf->time_high;
	time_low = iova_buf->time_low;
	spin_lock_irqsave(&map_list.lock, flags);
	list_add(&iova_buf->list_node, &map_list.head[id]);
	spin_unlock_irqrestore(&map_list.lock, flags);

	mtk_iova_map_latency_check(tab_id, iova, size, time_high, time_low);

iova_trace:
	mtk_iommu_iova_trace(IOMMU_MAP, iova, size, tab_id, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iova_map);

void mtk_iova_unmap(u64 tab_id, u64 iova, size_t size)
{
	u32 id = (iova >> 32);
	u64 start_t, end_t;
	unsigned long flags;
	struct iova_map_info *plist;
	struct iova_map_info *tmp_plist;
	int find_iova = 0;
	int i = 0;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	if (iommu_globals.map_record == 0)
		goto iova_trace;

	spin_lock_irqsave(&map_list.lock, flags);
	start_t = sched_clock();
	list_for_each_entry_safe(plist, tmp_plist, &map_list.head[id], list_node) {
		i++;
		if (plist->iova == iova &&
		    plist->tab_id == tab_id) {
			list_del(&plist->list_node);
			kfree(plist);
			find_iova = 1;
			break;
		}
	}
	end_t = sched_clock();
	spin_unlock_irqrestore(&map_list.lock, flags);

	if ((end_t - start_t) > FIND_IOVA_TIMEOUT_NS)
		pr_info("%s warnning, find iova:[0x%llx 0x%llx 0x%zx] in %d timeout:%llu\n",
			__func__, tab_id, iova, size, i, (end_t - start_t));

	if (!find_iova)
		pr_info("%s warnning, iova:[0x%llx 0x%llx 0x%zx] not find in %d\n",
			__func__, tab_id, iova, size, i);

iova_trace:
	mtk_iommu_iova_trace(IOMMU_UNMAP, iova, size, tab_id, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iova_unmap);

/* For smmu, tab_id is smmu hardware id */
void mtk_iova_map_dump(u64 iova, u64 tab_id)
{
	mtk_iommu_iova_map_dump(NULL, iova, tab_id);
}
EXPORT_SYMBOL_GPL(mtk_iova_map_dump);

/* For smmu, tab_id is smmu hardware id */
void mtk_iova_dump(u64 iova, u64 tab_id)
{
	mtk_iommu_iova_dump(NULL, iova, tab_id);
}
EXPORT_SYMBOL_GPL(mtk_iova_dump);

static int to_smmu_hw_id(u64 tab_id)
{
	return smmu_v3_enable ? smmu_tab_id_to_smmu_id(tab_id) : tab_id;
}

static void mtk_iommu_iova_map_info_dump(struct seq_file *s,
					 struct iova_map_info *plist)
{
	if (!plist)
		return;

	if (smmu_v3_enable) {
		iommu_dump(s, "%-7u 0x%-7u 0x%-12llx 0x%-8zx %llu.%06u\n",
			   smmu_tab_id_to_smmu_id(plist->tab_id),
			   smmu_tab_id_to_asid(plist->tab_id),
			   plist->iova, plist->size,
			   plist->time_high, plist->time_low);
	} else {
		iommu_dump(s, "%-6llu 0x%-12llx 0x%-8zx %llu.%06u\n",
			   plist->tab_id, plist->iova, plist->size,
			   plist->time_high, plist->time_low);
	}
}

/* For smmu, tab_id is smmu hardware id */
static void mtk_iommu_iova_map_dump(struct seq_file *s, u64 iova, u64 tab_id)
{
	u32 i, id = (iova >> 32);
	unsigned long flags;
	struct iova_map_info *plist = NULL;
	struct iova_map_info *n = NULL;

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	if (smmu_v3_enable) {
		iommu_dump(s, "smmu iova map dump:\n");
		iommu_dump(s, "%-7s %-9s %-14s %-10s %17s\n",
			   "smmu_id", "asid", "iova", "size", "time");
	} else {
		iommu_dump(s, "iommu iova map dump:\n");
		iommu_dump(s, "%-6s %-14s %-10s %17s\n",
			   "tab_id", "iova", "size", "time");
	}

	spin_lock_irqsave(&map_list.lock, flags);
	if (!iova) {
		for (i = 0; i < MTK_IOVA_SPACE_NUM; i++) {
			list_for_each_entry_safe(plist, n, &map_list.head[i], list_node)
				if (to_smmu_hw_id(plist->tab_id) == tab_id)
					mtk_iommu_iova_map_info_dump(s, plist);
		}
		spin_unlock_irqrestore(&map_list.lock, flags);
		return;
	}

	list_for_each_entry_safe(plist, n, &map_list.head[id], list_node)
		if (to_smmu_hw_id(plist->tab_id) == tab_id &&
		    iova <= (plist->iova + plist->size) &&
		    iova >= (plist->iova)) {
			mtk_iommu_iova_map_info_dump(s, plist);
		}
	spin_unlock_irqrestore(&map_list.lock, flags);
}

static void __iommu_trace_dump(struct seq_file *s, u64 iova)
{
	int event_id;
	int i = 0;

	if (iommu_globals.dump_enable == 0)
		return;

	if (smmu_v3_enable) {
		iommu_dump(s, "smmu trace dump:\n");
		iommu_dump(s, "%-8s %-9s %-11s %-11s %-14s %-12s %-14s %17s %s\n",
			   "action", "smmu_id", "stream_id", "asid", "iova_start",
			   "size", "iova_end", "time", "dev");
	} else {
		iommu_dump(s, "iommu trace dump:\n");
		iommu_dump(s, "%-8s %-4s %-14s %-12s %-14s %17s %s\n",
			   "action", "tab_id", "iova_start", "size", "iova_end",
			   "time", "dev");
	}
	for (i = 0; i < IOMMU_EVENT_COUNT_MAX; i++) {
		unsigned long start_iova = 0;
		unsigned long end_iova = 0;

		if ((iommu_globals.record[i].time_low == 0) &&
		    (iommu_globals.record[i].time_high == 0))
			break;
		event_id = iommu_globals.record[i].event_id;
		if (event_id < 0 || event_id >= IOMMU_EVENT_MAX)
			continue;

		if (event_id <= IOMMU_UNSYNC && iommu_globals.record[i].data1 > 0) {
			end_iova = iommu_globals.record[i].data1 +
				   iommu_globals.record[i].data2 - 1;
		}

		if (iova > 0 && event_id <= IOMMU_UNSYNC) {
			start_iova = iommu_globals.record[i].data1;
			if (!(iova <= end_iova && iova >= start_iova))
				continue;
		}

		if (smmu_v3_enable) {
			iommu_dump(s,
				   "%-8s 0x%-7x 0x%-9x 0x%-9x 0x%-12lx 0x%-10zx 0x%-12lx %10llu.%06u %s\n",
				   event_mgr[event_id].name,
				   smmu_tab_id_to_smmu_id(iommu_globals.record[i].data3),
				   (iommu_globals.record[i].dev != NULL ?
				   get_smmu_stream_id(iommu_globals.record[i].dev) : -1),
				   smmu_tab_id_to_asid(iommu_globals.record[i].data3),
				   iommu_globals.record[i].data1,
				   iommu_globals.record[i].data2,
				   end_iova,
				   iommu_globals.record[i].time_high,
				   iommu_globals.record[i].time_low,
				   (iommu_globals.record[i].dev != NULL ?
				   dev_name(iommu_globals.record[i].dev) : ""));
		} else {
			iommu_dump(s, "%-8s %-6lu 0x%-12lx 0x%-10zx 0x%-12lx %10llu.%06u %s\n",
				   event_mgr[event_id].name,
				   iommu_globals.record[i].data3,
				   iommu_globals.record[i].data1,
				   iommu_globals.record[i].data2,
				   end_iova,
				   iommu_globals.record[i].time_high,
				   iommu_globals.record[i].time_low,
				   (iommu_globals.record[i].dev != NULL ?
				   dev_name(iommu_globals.record[i].dev) : ""));
		}
	}
}

static void mtk_iommu_trace_dump(struct seq_file *s)
{
	__iommu_trace_dump(s, 0);
}

void mtk_iova_trace_dump(u64 iova)
{
	__iommu_trace_dump(NULL, iova);
}
EXPORT_SYMBOL_GPL(mtk_iova_trace_dump);

void mtk_iommu_debug_reset(void)
{
	iommu_globals.enable = 1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_debug_reset);

static inline const char *get_power_status_str(int status)
{
	return (status == 0 ? "Power On" : "Power Off");
}

/**
 * Get mtk_iommu_port list index.
 * @tf_id: Hardware reported AXI id when translation fault
 * @type: mtk_iommu_type or mtk_smmu_type
 * @id: iommu_id for iommu, smmu common id for smmu
 */
static int mtk_iommu_get_tf_port_idx(int tf_id, u32 type, int id)
{
	int i;
	u32 vld_id, port_nr;
	const struct mtk_iommu_port *port_list;
	int (*mm_tf_is_gce_videoup)(u32 port_tf, u32 vld_tf);
	u32 smmu_type = type;

	/* Only support MM_SMMU and APU_SMMU */
	if (smmu_v3_enable && smmu_type > APU_SMMU) {
		pr_info("%s fail, invalid type %d\n", __func__, smmu_type);
		return m4u_data->plat_data->port_nr[MM_SMMU];
	} else if (!smmu_v3_enable && type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return m4u_data->plat_data->port_nr[MM_IOMMU];
	}

	if (m4u_data->plat_data->get_valid_tf_id) {
		vld_id = m4u_data->plat_data->get_valid_tf_id(tf_id, type, id);
	} else {
		if (type == APU_IOMMU)
			vld_id = F_APU_MMU_INT_TF_MSK(tf_id);
		else
			vld_id = tf_id & F_MMU_INT_TF_MSK;
	}

	pr_info("get vld tf_id:0x%x\n", vld_id);
	port_nr =  m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];

	/* check (larb | port) for smi_larb or apu_bus */
	for (i = 0; i < port_nr; i++) {
		if (m4u_data->plat_data->tf_id_is_match) {
			if (m4u_data->plat_data->tf_id_is_match(tf_id, type, id, port_list[i]))
				return i;
		} else {
			if (port_list[i].port_type == NORMAL &&
			    port_list[i].tf_id == vld_id &&
			    port_list[i].id == id)
				return i;
		}
	}

	/* check larb for smi_common */
	if (type == MM_IOMMU && m4u_data->plat_data->mm_tf_ccu_support) {
		for (i = 0; i < port_nr; i++) {
			if (port_list[i].port_type == CCU_FAKE &&
			    (port_list[i].tf_id & F_MMU_INT_TF_CCU_MSK) ==
			    (vld_id & F_MMU_INT_TF_CCU_MSK) &&
			    port_list[i].id == id)
				return i;
		}
	}

	/* check gce/video_uP */
	mm_tf_is_gce_videoup = m4u_data->plat_data->mm_tf_is_gce_videoup;
	if (type == MM_IOMMU && mm_tf_is_gce_videoup) {
		for (i = 0; i < port_nr; i++) {
			if (port_list[i].port_type == GCE_VIDEOUP_FAKE &&
			    mm_tf_is_gce_videoup(port_list[i].tf_id, tf_id) &&
			    port_list[i].id == id)
				return i;
		}
	}

	return port_nr;
}

static int mtk_iommu_port_idx(int id, enum mtk_iommu_type type)
{
	int i;
	u32 port_nr = m4u_data->plat_data->port_nr[type];
	const struct mtk_iommu_port *port_list;

	if (type < MM_IOMMU || type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return m4u_data->plat_data->port_nr[MM_IOMMU];
	}

	port_list = m4u_data->plat_data->port_list[type];
	for (i = 0; i < port_nr; i++) {
		if ((port_list[i].larb_id == MTK_M4U_TO_LARB(id)) &&
		     (port_list[i].port_id == MTK_M4U_TO_PORT(id)))
			return i;
	}
	return port_nr;
}

static void report_custom_fault(
	u64 fault_iova, u64 fault_pa,
	u32 fault_id, u32 type, int id)
{
	const struct mtk_iommu_port *port_list;
	bool support_tf_fn = false;
	u32 smmu_type = type;
	u32 port_nr;
	int idx;

	/* Only support MM_SMMU and APU_SMMU */
	if (smmu_v3_enable && smmu_type > APU_SMMU) {
		pr_info("%s fail, invalid type %d\n", __func__, smmu_type);
		return;
	} else if (!smmu_v3_enable && type >= TYPE_NUM) {
		pr_info("%s fail, invalid type %d\n", __func__, type);
		return;
	}

	pr_info("error, tf report start fault_id:0x%x\n", fault_id);
	port_nr = m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	idx = mtk_iommu_get_tf_port_idx(fault_id, type, id);
	if (idx >= port_nr) {
		pr_warn("fail,iova:0x%llx, port:0x%x\n",
			fault_iova, fault_id);
		return;
	}

	/* Only MM_IOMMU support fault callback */
	support_tf_fn = (smmu_v3_enable ? (smmu_type == MM_SMMU) : (type == MM_IOMMU));
	if (support_tf_fn) {
		pr_info("error, tf report larb-port:(%u--%u), idx:%d\n",
			port_list[idx].larb_id,
			port_list[idx].port_id, idx);

		if (port_list[idx].enable_tf &&
			m4u_data->m4u_cb[idx].fault_fn)
			m4u_data->m4u_cb[idx].fault_fn(m4u_data->m4u_cb[idx].port,
			fault_iova, m4u_data->m4u_cb[idx].fault_data);
	}

	m4u_aee_print(mmu_translation_log_format,
		(smmu_v3_enable ? "SMMU" : "M4U"),
		port_list[idx].name,
		port_list[idx].name, fault_iova,
		fault_pa);
}

void report_custom_iommu_fault(
	u64 fault_iova, u64 fault_pa,
	u32 fault_id, enum mtk_iommu_type type,
	int id)
{
	report_custom_fault(fault_iova, fault_pa, fault_id, type, id);
}
EXPORT_SYMBOL_GPL(report_custom_iommu_fault);

void report_iommu_mau_fault(
	u32 assert_id, u32 falut_id, char *port_name,
	u32 assert_addr, u32 assert_b32)
{
	m4u_aee_print(mau_assert_log_format,
		      assert_id, falut_id, port_name, assert_addr, assert_b32);
}
EXPORT_SYMBOL_GPL(report_iommu_mau_fault);

int mtk_iommu_register_fault_callback(int port,
	mtk_iommu_fault_callback_t fn,
	void *cb_data, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int idx = mtk_iommu_port_idx(port, type);

	if (idx >= m4u_data->plat_data->port_nr[type]) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	if (is_vpu)
		idx += m4u_data->plat_data->port_nr[type];
	m4u_data->m4u_cb[idx].port = port;
	m4u_data->m4u_cb[idx].fault_fn = fn;
	m4u_data->m4u_cb[idx].fault_data = cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_register_fault_callback);

int mtk_iommu_unregister_fault_callback(int port, bool is_vpu)
{
	enum mtk_iommu_type type = is_vpu ? APU_IOMMU : MM_IOMMU;
	int idx = mtk_iommu_port_idx(port, type);

	if (idx >= m4u_data->plat_data->port_nr[type]) {
		pr_info("%s fail, port=%d\n", __func__, port);
		return -1;
	}
	if (is_vpu)
		idx += m4u_data->plat_data->port_nr[type];
	m4u_data->m4u_cb[idx].port = -1;
	m4u_data->m4u_cb[idx].fault_fn = NULL;
	m4u_data->m4u_cb[idx].fault_data = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_unregister_fault_callback);

char *mtk_iommu_get_port_name(enum mtk_iommu_type type, int id, int tf_id)
{
	const struct mtk_iommu_port *port_list;
	u32 port_nr;
	int idx;

	if (type < MM_IOMMU || type >= TYPE_NUM) {
		pr_notice("%s fail, invalid type %d\n", __func__, type);
		return "m4u_port_unknown";
	}

	if (type == PERI_IOMMU)
		return peri_tf_analyse(id, tf_id);

	port_nr = m4u_data->plat_data->port_nr[type];
	port_list = m4u_data->plat_data->port_list[type];
	idx = mtk_iommu_get_tf_port_idx(tf_id, type, id);
	if (idx >= port_nr) {
		pr_notice("%s err, iommu(%d,%d) tf_id:0x%x\n",
			  __func__, type, id, tf_id);
		return "m4u_port_unknown";
	}

	return port_list[idx].name;
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_port_name);

const struct mau_config_info *mtk_iommu_get_mau_config(
	enum mtk_iommu_type type, int id,
	unsigned int slave, unsigned int mau)
{
#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	const struct mau_config_info *mau_config;
	int i;

	for (i = 0; i < m4u_data->plat_data->mau_config_nr; i++) {
		mau_config = &m4u_data->plat_data->mau_config[i];
		if (mau_config->iommu_type == type &&
		    mau_config->iommu_id == id &&
		    mau_config->slave == slave &&
		    mau_config->mau == mau)
			return mau_config;
	}
#endif

	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_iommu_get_mau_config);

int mtk_iommu_set_ops(const struct mtk_iommu_ops *ops)
{
	if (iommu_ops == NULL)
		iommu_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_iommu_set_ops);

int mtk_iommu_update_pm_status(u32 type, u32 id, bool pm_sta)
{
	if (iommu_ops && iommu_ops->update_pm_status)
		return iommu_ops->update_pm_status(type, id, pm_sta);

	return -1;
}
EXPORT_SYMBOL_GPL(mtk_iommu_update_pm_status);

#if IS_ENABLED(CONFIG_DEVICE_MODULES_ARM_SMMU_V3)
static const struct mtk_smmu_ops *smmu_ops;

int mtk_smmu_set_ops(const struct mtk_smmu_ops *ops)
{
	if (smmu_ops == NULL)
		smmu_ops = ops;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smmu_set_ops);

static int mtk_smmu_power_get(u32 smmu_type)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && smmu_ops->smmu_power_get)
			return smmu_ops->smmu_power_get(&data->smmu);
	}

	return -1;
}

static int mtk_smmu_power_put(u32 smmu_type)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && smmu_ops->smmu_power_put)
			return smmu_ops->smmu_power_put(&data->smmu);
	}

	return -1;
}

static int get_smmu_common_id(u32 smmu_type, u32 tbu_id)
{
	int id = -1;

	if (smmu_type >= SMMU_TYPE_NUM) {
		pr_info("%s fail, invalid smmu_type %d\n", __func__, smmu_type);
		return -1;
	}

	if (tbu_id >= SMMU_TBU_CNT(smmu_type)) {
		pr_info("%s fail, invalid tbu_id:%u\n", __func__, tbu_id);
		return -1;
	}

	if (m4u_data->plat_data->smmu_common_id)
		id = m4u_data->plat_data->smmu_common_id(smmu_type, tbu_id);

	pr_debug("%s smmu_type:%u, tbu_id:%u, id:%d\n",
		 __func__, smmu_type, tbu_id, id);

	return id;
}

void report_custom_smmu_fault(u64 fault_iova, u64 fault_pa,
			      u32 fault_id, u32 smmu_id)
{
	u32 tbu_id = SMMUWP_TF_TBU_VAL(fault_id);
	char *port_name = NULL;
	int id;

	id = get_smmu_common_id(smmu_id, tbu_id);
	if (id < 0)
		return;

	if (smmu_id == SOC_SMMU) {
		if (m4u_data->plat_data->smmu_port_name)
			port_name = m4u_data->plat_data->smmu_port_name(SOC_SMMU, id, fault_id);

		if (port_name != NULL)
			m4u_aee_print(mmu_translation_log_format, "SMMU", port_name,
				      port_name, fault_iova, fault_pa);
		return;
	} else if (smmu_id == GPU_SMMU) {
		m4u_aee_print(mmu_translation_log_format, "SMMU", "GPUSYS",
			      "GPUSYS", fault_iova, fault_pa);
		return;
	}

	report_custom_fault(fault_iova, fault_pa, fault_id, smmu_id, id);
}
EXPORT_SYMBOL_GPL(report_custom_smmu_fault);

static void dump_wrapper_register(struct seq_file *s,
				  struct arm_smmu_device *smmu)
{
	struct mtk_smmu_data *data = to_mtk_smmu_data(smmu);
	void __iomem *wp_base = smmu->wp_base;
	unsigned int smmuwp_reg_nr, i;

	iommu_dump(s, "wp reg for smmu:%d, base:0x%llx, wp_base:0x%llx\n",
		   data->plat_data->smmu_type,
		   (unsigned long long) smmu->base,
		   (unsigned long long) smmu->wp_base);

	smmuwp_reg_nr = ARRAY_SIZE(smmuwp_regs);

	/* SOC has one less TBU than the others */
	if (data->plat_data->smmu_type == SOC_SMMU)
		smmuwp_reg_nr -= SMMU_TBU_REG_NUM;

	for (i = 0; i < smmuwp_reg_nr; i++) {
		if (i + 4 < smmuwp_reg_nr) {
			iommu_dump(s,
				   "%s:0x%x=0x%x\t%s:0x%x=0x%x\t%s:0x%x=0x%x\t%s:0x%x=0x%x\t%s:0x%x=0x%x\n",
				   smmuwp_regs[i + 0].name, smmuwp_regs[i + 0].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 0].offset),
				   smmuwp_regs[i + 1].name, smmuwp_regs[i + 1].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 1].offset),
				   smmuwp_regs[i + 2].name, smmuwp_regs[i + 2].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 2].offset),
				   smmuwp_regs[i + 3].name, smmuwp_regs[i + 3].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 3].offset),
				   smmuwp_regs[i + 4].name, smmuwp_regs[i + 4].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i + 4].offset));
			i = i + 4;
		} else {
			iommu_dump(s, "%s:0x%x=0x%x\n",
				   smmuwp_regs[i].name, smmuwp_regs[i].offset,
				   readl_relaxed(wp_base + smmuwp_regs[i].offset));
		}
	}
}

void mtk_smmu_wpreg_dump(struct seq_file *s, u32 smmu_type)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			dump_wrapper_register(s, &data->smmu);
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_wpreg_dump);

//=====================================================
// SMMU private data for Dump Page Table start
//=====================================================
#define ARM_LPAE_MAX_LEVELS		4

/* Struct accessors */
#define io_pgtable_to_data(x)						\
	container_of((x), struct arm_lpae_io_pgtable, iop)

#define io_pgtable_ops_to_data(x)					\
	io_pgtable_to_data(io_pgtable_ops_to_pgtable(x))

/*
 * Calculate the right shift amount to get to the portion describing level l
 * in a virtual address mapped by the pagetable in d.
 */
#define ARM_LPAE_LVL_SHIFT(l, d)					\
	(((ARM_LPAE_MAX_LEVELS - (l)) * (d)->bits_per_level) +		\
	ilog2(sizeof(arm_lpae_iopte)))

#define ARM_LPAE_GRANULE(d)						\
	(sizeof(arm_lpae_iopte) << (d)->bits_per_level)
#define ARM_LPAE_PGD_SIZE(d)						\
	(sizeof(arm_lpae_iopte) << (d)->pgd_bits)

/*
 * Calculate the index at level l used to map virtual address a using the
 * pagetable in d.
 */
#define ARM_LPAE_PGD_IDX(l, d)						\
	((l) == (d)->start_level ? (d)->pgd_bits - (d)->bits_per_level : 0)

#define ARM_LPAE_LVL_IDX(a, l, d)					\
	(((u64)(a) >> ARM_LPAE_LVL_SHIFT(l, d)) &			\
	 ((1 << ((d)->bits_per_level + ARM_LPAE_PGD_IDX(l, d))) - 1))

/* Calculate the block/page mapping size at level l for pagetable in d. */
#define ARM_LPAE_BLOCK_SIZE(l,d)	(1ULL << ARM_LPAE_LVL_SHIFT(l,d))

/* Page table bits */
#define ARM_LPAE_PTE_TYPE_SHIFT		0
#define ARM_LPAE_PTE_TYPE_MASK		0x3

#define ARM_LPAE_PTE_TYPE_BLOCK		1
#define ARM_LPAE_PTE_TYPE_TABLE		3
#define ARM_LPAE_PTE_TYPE_PAGE		3

#define ARM_LPAE_PTE_ADDR_MASK		GENMASK_ULL(47, 12)

/* IOPTE accessors */
#define iopte_deref(pte, d) __va(iopte_to_paddr(pte, d))

#define iopte_type(pte)					\
	(((pte) >> ARM_LPAE_PTE_TYPE_SHIFT) & ARM_LPAE_PTE_TYPE_MASK)

struct arm_lpae_io_pgtable {
	struct io_pgtable	iop;

	int			pgd_bits;
	int			start_level;
	int			bits_per_level;

	void			*pgd;
};

typedef u64 arm_lpae_iopte;

static inline bool iopte_leaf(arm_lpae_iopte pte, int lvl,
			      enum io_pgtable_fmt fmt)
{
	if (lvl == (ARM_LPAE_MAX_LEVELS - 1) && fmt != ARM_MALI_LPAE)
		return iopte_type(pte) == ARM_LPAE_PTE_TYPE_PAGE;

	return iopte_type(pte) == ARM_LPAE_PTE_TYPE_BLOCK;
}

static phys_addr_t iopte_to_paddr(arm_lpae_iopte pte,
				  struct arm_lpae_io_pgtable *data)
{
	u64 paddr = pte & ARM_LPAE_PTE_ADDR_MASK;

	if (ARM_LPAE_GRANULE(data) < SZ_64K)
		return paddr;

	/* Rotate the packed high-order bits back to the top */
	return (paddr | (paddr << (48 - 12))) & (ARM_LPAE_PTE_ADDR_MASK << 4);
}

static u64 arm_lpae_iova_to_iopte(struct io_pgtable_ops *ops, unsigned long iova)
{
	struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
	arm_lpae_iopte pte, *ptep = data->pgd;
	int lvl = data->start_level;

	do {
		/* Valid IOPTE pointer? */
		if (!ptep)
			return 0;

		/* Grab the IOPTE we're interested in */
		ptep += ARM_LPAE_LVL_IDX(iova, lvl, data);
		pte = READ_ONCE(*ptep);

		/* Valid entry? */
		if (!pte)
			return 0;

		/* Leaf entry? */
		if (iopte_leaf(pte, lvl, data->iop.fmt))
			goto found_translation;

		/* Take it to the next level */
		ptep = iopte_deref(pte, data);
	} while (++lvl < ARM_LPAE_MAX_LEVELS);

	/* Ran out of page tables to walk */
	return 0;

found_translation:
	pr_info("%s, iova:0x%lx, pte:0x%llx, lvl:%d, iopte_type:%llu, fmt:%u\n",
		__func__, iova, pte, lvl, iopte_type(pte), data->iop.fmt);
	return pte;
}

static void arm_lpae_ops_dump(struct seq_file *s, struct io_pgtable_ops *ops)
{
	struct arm_lpae_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable_cfg *cfg = &data->iop.cfg;

	iommu_dump(s, "SMMU OPS values:\n");
	iommu_dump(s,
		   "ops cfg: quirks 0x%lx, pgsize_bitmap 0x%lx, ias %u-bit, oas %u-bit, coherent_walk:%d\n",
		   cfg->quirks, cfg->pgsize_bitmap, cfg->ias, cfg->oas, cfg->coherent_walk);
	iommu_dump(s,
		   "ops data: %d levels, 0x%zx pgd_size, %u pg_shift, %u bits_per_level, pgd @ %p\n",
		   ARM_LPAE_MAX_LEVELS - data->start_level, ARM_LPAE_PGD_SIZE(data),
		   ilog2(ARM_LPAE_GRANULE(data)), data->bits_per_level, data->pgd);
}

static void dump_pgtable_ops(struct seq_file *s, struct arm_smmu_master *master)
{
	if (!master || !master->domain || !master->domain->pgtbl_ops) {
		iommu_dump(s, "Not do arm_smmu_domain_finalise\n");
		return;
	}

	arm_lpae_ops_dump(s, master->domain->pgtbl_ops);
}

static inline bool is_valid_ste(__le64 *ste)
{
	return (ste && (le64_to_cpu(ste[0]) & STRTAB_STE_0_V));
}

static inline bool is_valid_cd(__le64 *cd)
{
	return (cd && (le64_to_cpu(cd[0]) & CTXDESC_CD_0_V));
}

static void ste_dump(struct seq_file *s, u32 sid, __le64 *ste)
{
	int i;

	if (ste == NULL)
		return;

	if (!is_valid_ste(ste)) {
		iommu_dump(s, "Failed to valid ste(sid:%u)\n", sid);
		return;
	}

	iommu_dump(s, "SMMU STE values:\n");
	for (i = 0; i < STRTAB_STE_DWORDS; i++) {
		if (i + 3 < STRTAB_STE_DWORDS) {
			iommu_dump(s, "u64[%d~%d]:0x%016llx|0x%016llx|0x%016llx|0x%016llx\n",
				   i, i + 3, ste[i + 0], ste[i + 1], ste[i + 2], ste[i + 3]);
			i = i + 3;
		} else {
			iommu_dump(s, "u64[%d]:0x%016llx\n", i, ste[i]);
		}
	}
}

static void cd_dump(struct seq_file *s, u32 ssid, __le64 *cd)
{
	int i;

	if (cd == NULL)
		return;

	if (!is_valid_cd(cd)) {
		iommu_dump(s, "Failed to valid cd(ssid:%u)\n", ssid);
		return;
	}

	iommu_dump(s, "SMMU CD values:\n");
	for (i = 0; i < CTXDESC_CD_DWORDS; i++) {
		if (i + 3 < CTXDESC_CD_DWORDS) {
			iommu_dump(s, "u64[%d~%d]:0x%016llx|0x%016llx|0x%016llx|0x%016llx\n",
				   i, i + 3, cd[i + 0], cd[i + 1], cd[i + 2], cd[i + 3]);
			i = i + 3;
		} else {
			iommu_dump(s, "u64[%d]:0x%016llx\n", i, cd[i]);
		}
	}
}

static void dump_ste_cd_info(struct seq_file *s,
			     struct arm_smmu_master *master)
{
	struct arm_smmu_domain *domain;
	struct arm_smmu_device *smmu;
	__le64 *steptr = NULL, *cdptr = NULL;
	u64 asid = 0, ttbr = 0;
	u32 sid, ssid;

	if (!master || !master->streams) {
		pr_info("%s, ERROR", __func__);
		return;
	}

	/* currently only support one master one sid */
	sid = master->streams[0].id;
	ssid = 0;
	smmu = master->smmu;
	domain = master->domain;

	if (smmu_ops && smmu_ops->get_step_ptr)
		steptr = smmu_ops->get_step_ptr(smmu, sid);

	if (smmu_ops && smmu_ops->get_cd_ptr)
		cdptr = smmu_ops->get_cd_ptr(domain, ssid);

	if (is_valid_cd(cdptr)) {
		asid = FIELD_GET(CTXDESC_CD_0_ASID, le64_to_cpu(cdptr[0]));
		ttbr = FIELD_GET(CTXDESC_CD_1_TTB0_MASK,  le64_to_cpu(cdptr[1]));
	}

	iommu_dump(s, "%s strtab base:0x%llx, cfg:0x%x\n", __func__,
		   smmu->strtab_cfg.strtab_base, smmu->strtab_cfg.strtab_base_cfg);
	iommu_dump(s, "%s sid=0x%x asid=0x%llx ttbr=0x%llx dev(%s) [cd:%d ste:%d]\n",
		   __func__, sid, asid, ttbr, dev_name(master->dev),
		   (cdptr == NULL), (steptr == NULL));

	ste_dump(s, sid, steptr);
	cd_dump(s, ssid, cdptr);
}

static inline void pt_info_dump(struct seq_file *s,
				struct arm_lpae_io_pgtable *data,
				arm_lpae_iopte *ptep,
				arm_lpae_iopte pte_s,
				arm_lpae_iopte pte_e,
				u64 iova_s,
				u64 iova_e,
				u64 pgsize,
				u64 pgcount)
{
	iommu_dump(s,
		   "ptep:%pa pte:0x%llx ~ 0x%llx iova:0x%llx ~ 0x%llx -> pa:0x%llx ~ 0x%llx count:%llu\n",
		   &ptep, pte_s, pte_e, iova_s, (iova_e + pgsize -1),
		   iopte_to_paddr(pte_s, data),
		   (iopte_to_paddr(pte_e, data) + pgsize -1),
		   pgcount);
}

static void __ptdump(struct seq_file *s, arm_lpae_iopte *ptep, int lvl, u64 va,
		     struct arm_lpae_io_pgtable *data)
{
	arm_lpae_iopte pte, *ptep_next;
	u64 i, entry_num, pgsize, pgcount = 0, tmp_va = 0;
	arm_lpae_iopte pte_pre = 0;
	arm_lpae_iopte pte_s = 0;
	arm_lpae_iopte pte_e = 0;
	bool need_ptdump = false;
	bool need_continue = false;
	u64 iova_s = 0;
	u64 iova_e = 0;

	entry_num = 1 << (data->bits_per_level + ARM_LPAE_PGD_IDX(lvl, data));
	pgsize = ARM_LPAE_BLOCK_SIZE(lvl, data);

	iommu_dump(s, "ptep:%pa lvl:%d va:0x%llx pgsize:0x%llx entry_num:%llu\n",
		   &ptep, lvl, va, pgsize, entry_num);

	for (i = 0; i < entry_num; i++) {
		pte = READ_ONCE(*(ptep + i));
		if (!pte)
			goto ptdump_reset_continue;

		tmp_va = va | (i << ARM_LPAE_LVL_SHIFT(lvl, data));

		if (iopte_leaf(pte, lvl, data->iop.fmt)) {
#ifdef SMMU_PTDUMP_RAW
			iommu_dump(s, "ptep:%pa pte_raw:0x%llx iova:0x%llx -> pa:0x%llx\n",
				   &ptep, pte, tmp_va, iopte_to_paddr(pte, data));
#endif
			if (pte_s == 0) {
				pte_s = pte;
				pte_e = pte;
				iova_s = tmp_va;
				iova_e = tmp_va;
			}

			if (pte_pre == 0 || pte - pte_pre == pgsize) {
				need_ptdump = true;
				pte_pre = pte;
				pte_e = pte;
				iova_e = tmp_va;
				pgcount++;
			} else {
				pt_info_dump(s, data, ptep, pte_s, pte_e, iova_s, iova_e,
					     pgsize, pgcount);
				need_ptdump = true;
				pte_pre = pte;
				pte_s = pte;
				pte_e = pte;
				iova_s = tmp_va;
				iova_e = tmp_va;
				pgcount = 1;
			}

			if ((i + 1) == entry_num)
				goto ptdump_reset_continue;

			continue;
		}

		goto ptdump_reset;

ptdump_reset_continue:
		need_continue = true;

ptdump_reset:
		if (need_ptdump) {
			pt_info_dump(s, data, ptep, pte_s, pte_e, iova_s, iova_e,
				     pgsize, pgcount);
			need_ptdump = false;
			pte_pre = 0;
			pte_s = 0;
			pte_e = 0;
			iova_s = 0;
			iova_e = 0;
			pgcount = 0;
		}

		if(need_continue) {
			need_continue = false;
			continue;
		}

		ptep_next = iopte_deref(pte, data);
		__ptdump(s, ptep_next, lvl + 1, tmp_va, data);
	}
}

static void ptdump(struct seq_file *s,
		   struct arm_smmu_domain *domain,
		   void *pgd, int stage, u32 ssid)
{
	struct arm_lpae_io_pgtable *data, data_sva;
	int levels, va_bits, bits_per_level;
	struct io_pgtable_ops *ops;
	arm_lpae_iopte *ptep = pgd;

	iommu_dump(s, "SMMU dump page table for stage %d, ssid 0x%x:\n",
		   stage, ssid);

	if (stage == 1 && !ssid) {
		ops = domain->pgtbl_ops;
		data = io_pgtable_ops_to_data(ops);
	} else {
		va_bits = VA_BITS - PAGE_SHIFT;
		bits_per_level = PAGE_SHIFT - ilog2(sizeof(arm_lpae_iopte));
		levels = DIV_ROUND_UP(va_bits, bits_per_level);

		memset(&data_sva, 0, sizeof(data_sva));
		data_sva.start_level = ARM_LPAE_MAX_LEVELS - levels;
		data_sva.pgd_bits = va_bits - (bits_per_level * (levels - 1));
		data_sva.bits_per_level = bits_per_level;
		data_sva.pgd = pgd;

		data = &data_sva;
	}

	__ptdump(s, ptep, data->start_level, 0, data);
}

static void dump_io_pgtable_s1(struct seq_file *s,
			       struct arm_smmu_domain *domain,
			       u32 sid, u32 ssid, __le64 *cd)
{
	void *pgd;
	u64 ttbr;

	if (!is_valid_cd(cd)) {
		iommu_dump(s, "Failed to find valid cd(sid:%u, ssid:%u):%d\n",
			   sid, ssid, (cd == NULL));
		return;
	}

	/* CD0 and other CDx are all using ttbr0 */
	ttbr = le64_to_cpu(cd[1]) & CTXDESC_CD_1_TTB0_MASK;

	if (!ttbr) {
		iommu_dump(s, "Stage 1 TTBR is not valid (sid: %u, ssid: %u)\n",
			   sid, ssid);
		return;
	}

	pgd = phys_to_virt(ttbr);
	ptdump(s, domain, pgd, 1, ssid);
}

static void dump_io_pgtable_s2(struct seq_file *s,
			       struct arm_smmu_domain *domain,
			       u32 sid, u32 ssid, __le64 *ste)
{
	void *pgd;
	u64 vttbr;

	if (!is_valid_ste(ste)) {
		iommu_dump(s, "Failed to valid ste(sid:%u):%d\n", sid, (ste == NULL));
		return;
	}

	if (!(le64_to_cpu(ste[0]) & (1UL << 2))) {
		iommu_dump(s, "Stage 2 translation is not valid (sid: %u, ssid: %u)\n",
			   sid, ssid);
		return;
	}

	vttbr = le64_to_cpu(ste[3]) & STRTAB_STE_3_S2TTB_MASK;

	if (!vttbr) {
		iommu_dump(s, "Stage 2 TTBR is not valid (sid: %u, ssid: %u)\n",
			   sid, ssid);
		return;
	}

	pgd = phys_to_virt(vttbr);
	ptdump(s, domain, pgd, 2, ssid);
}

static void dump_io_pgtable(struct seq_file *s, struct arm_smmu_master *master)
{
	struct arm_smmu_domain *domain;
	struct arm_smmu_device *smmu;
	__le64 *steptr = NULL, *cdptr = NULL;
	u32 sid, ssid;

	if (!master || !master->streams) {
		pr_info("%s, ERROR", __func__);
		return;
	}

	/* currently only support one master one sid */
	sid = master->streams[0].id;
	ssid = 0;
	smmu = master->smmu;
	domain = master->domain;

	iommu_dump(s, "SMMU dump page table for sid:0x%x, ssid:0x%x:\n",
		   sid, ssid);

	if (smmu_ops && smmu_ops->get_cd_ptr)
		cdptr = smmu_ops->get_cd_ptr(domain, ssid);

	dump_io_pgtable_s1(s, domain, sid, ssid, cdptr);

	if (smmu_ops && smmu_ops->get_step_ptr)
		steptr = smmu_ops->get_step_ptr(smmu, sid);

	dump_io_pgtable_s2(s, domain, sid, ssid, steptr);
}

static void dump_ste_info_list(struct seq_file *s,
			       struct arm_smmu_device *smmu)
{
	struct rb_node *n;
	struct arm_smmu_stream *stream;

	if (!smmu) {
		pr_info("%s, ERROR\n", __func__);
		return;
	}

	for (n = rb_first(&smmu->streams); n; n = rb_next(n)) {
		stream = rb_entry(n, struct arm_smmu_stream, node);
		if (stream != NULL && stream->master != NULL)
			dump_ste_cd_info(s, stream->master);
	}
}

void mtk_smmu_ste_cd_dump(struct seq_file *s, u32 smmu_type)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			dump_ste_info_list(s, &data->smmu);
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_ste_cd_dump);

void mtk_smmu_ste_cd_info_dump(struct seq_file *s, u32 smmu_type, u32 sid)
{
	struct arm_smmu_device *smmu = NULL;
	struct arm_smmu_stream *stream;
	struct mtk_smmu_data *data;
	struct rb_node *n;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			smmu = &data->smmu;
	}

	if (smmu == NULL) {
		pr_info("%s, ERROR\n", __func__);
		return;
	}

	for (n = rb_first(&smmu->streams); n; n = rb_next(n)) {
		stream = rb_entry(n, struct arm_smmu_stream, node);
		if (stream != NULL && stream->master != NULL &&
		    stream->master->streams != NULL)
			/* currently only support one master one sid */
			if (sid == stream->master->streams[0].id) {
				dump_ste_cd_info(s, stream->master);
				return;
			}
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_ste_cd_info_dump);

static void smmu_pgtable_dump(struct seq_file *s, struct arm_smmu_device *smmu, bool dump_rawdata)
{
	struct rb_node *n;
	struct arm_smmu_stream *stream;
	struct mtk_smmu_data *data;

	if (!smmu) {
		pr_info("%s, ERROR\n", __func__);
		return;
	}

	data = to_mtk_smmu_data(smmu);
	iommu_dump(s, "pgtable dump for smmu_%d:\n", data->plat_data->smmu_type);

	for (n = rb_first(&smmu->streams); n; n = rb_next(n)) {
		stream = rb_entry(n, struct arm_smmu_stream, node);
		if (stream != NULL && stream->master != NULL) {
			dump_pgtable_ops(s, stream->master);
			dump_ste_cd_info(s, stream->master);

			if (dump_rawdata)
				dump_io_pgtable(s, stream->master);
		}
	}
}

void mtk_smmu_pgtable_dump(struct seq_file *s, u32 smmu_type, bool dump_rawdata)
{
	struct mtk_smmu_data *data;

	if (smmu_ops && smmu_ops->get_smmu_data) {
		data = smmu_ops->get_smmu_data(smmu_type);
		if (data != NULL && data->hw_init_flag == 1)
			smmu_pgtable_dump(s, &data->smmu, dump_rawdata);
	}
}
EXPORT_SYMBOL_GPL(mtk_smmu_pgtable_dump);

void mtk_smmu_pgtable_ops_dump(struct seq_file *s, struct io_pgtable_ops *ops)
{
	arm_lpae_ops_dump(s, ops);
}
EXPORT_SYMBOL_GPL(mtk_smmu_pgtable_ops_dump);

u64 mtk_smmu_iova_to_iopte(struct io_pgtable_ops *ops, u64 iova)
{
	return arm_lpae_iova_to_iopte(ops, iova);
}
EXPORT_SYMBOL_GPL(mtk_smmu_iova_to_iopte);
#else /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */
static inline int mtk_smmu_power_get(u32 smmu_type)
{
	return -1;
}

static inline int mtk_smmu_power_put(u32 smmu_type)
{
	return -1;
}
#endif /* CONFIG_DEVICE_MODULES_ARM_SMMU_V3 */

/* peri_iommu */
static struct peri_iommu_data mt6983_peri_iommu_data[PERI_IOMMU_NUM] = {
	[PERI_IOMMU_M4] = {
		.id = PERI_IOMMU_M4,
		.bus_id = 4,
	},
	[PERI_IOMMU_M6] = {
		.id = PERI_IOMMU_M6,
		.bus_id = 6,
	},
	[PERI_IOMMU_M7] = {
		.id = PERI_IOMMU_M7,
		.bus_id = 7,
	},
};

static char *mt6983_peri_m7_id(u32 id)
{
	u32 id1_0 = id & GENMASK(1, 0);
	u32 id4_2 = FIELD_GET(GENMASK(4, 2), id);

	if (id1_0 == 0)
		return "MCU_AP_M";
	else if (id1_0 == 1)
		return "DEBUG_TRACE_LOG";
	else if (id1_0 == 2)
		return "PERI2INFRA1_M";

	switch (id4_2) {
	case 0:
		return "CQ_DMA";
	case 1:
		return "DEBUGTOP";
	case 2:
		return "GPU_EB";
	case 3:
		return "CPUM_M";
	case 4:
		return "DXCC_M";
	default:
		return "UNKNOWN";
	}
}

static char *mt6983_peri_m6_id(u32 id)
{
	return "PERI2INFRA0_M";
}

static char *mt6983_peri_m4_id(u32 id)
{
	u32 id0 = id & 0x1;
	u32 id1_0 = id & GENMASK(1, 0);
	u32 id3_2 = FIELD_GET(GENMASK(3, 2), id);

	if (id0 == 0)
		return "DFD_M";
	else if (id1_0 == 1)
		return "DPMAIF_M";

	switch (id3_2) {
	case 0:
		return "ADSPSYS_M0_M";
	case 1:
		return "VLPSYS_M";
	case 2:
		return "CONN_M";
	default:
		return "UNKNOWN";
	}
}

static char *mt6983_peri_tf(enum peri_iommu id, u32 fault_id)
{
	switch (id) {
	case PERI_IOMMU_M4:
		return mt6983_peri_m4_id(fault_id);
	case PERI_IOMMU_M6:
		return mt6983_peri_m6_id(fault_id);
	case PERI_IOMMU_M7:
		return mt6983_peri_m7_id(fault_id);
	default:
		return "UNKNOWN";
	}
}

enum peri_iommu get_peri_iommu_id(u32 bus_id)
{
	int i;

	for (i = PERI_IOMMU_M4; i < PERI_IOMMU_NUM; i++) {
		if (bus_id == m4u_data->plat_data->peri_data[i].bus_id)
			return i;
	}

	return PERI_IOMMU_NUM;
};
EXPORT_SYMBOL_GPL(get_peri_iommu_id);

char *peri_tf_analyse(enum peri_iommu iommu_id, u32 fault_id)
{
	if (m4u_data->plat_data->peri_tf_analyse)
		return m4u_data->plat_data->peri_tf_analyse(iommu_id, fault_id);

	pr_info("%s is not support\n", __func__);
	return NULL;
}
EXPORT_SYMBOL_GPL(peri_tf_analyse);

static int mtk_iommu_debug_help(struct seq_file *s)
{
	iommu_dump(s, "iommu debug file:\n");
	iommu_dump(s, "help: description debug file and command\n");
	iommu_dump(s, "debug: iommu main debug file, receive debug command\n");
	iommu_dump(s, "iommu_dump: iova trace dump file\n");
	iommu_dump(s, "iova_alloc: iova alloc list dump file\n");
	iommu_dump(s, "iova_map: iova map list dump file\n\n");

	iommu_dump(s, "iommu debug command:\n");
	iommu_dump(s, "echo 1 > /proc/iommu_debug/debug: iommu debug help\n");
	iommu_dump(s, "echo 2 > /proc/iommu_debug/debug: mm translation fault test\n");
	iommu_dump(s, "echo 3 > /proc/iommu_debug/debug: apu translation fault test\n");
	iommu_dump(s, "echo 4 > /proc/iommu_debug/debug: peri translation fault test\n");
	iommu_dump(s, "echo 5 > /proc/iommu_debug/debug: enable trace log\n");
	iommu_dump(s, "echo 6 > /proc/iommu_debug/debug: disable trace log\n");
	iommu_dump(s, "echo 7 > /proc/iommu_debug/debug: enable trace dump\n");
	iommu_dump(s, "echo 8 > /proc/iommu_debug/debug: disable trace dump\n");
	iommu_dump(s, "echo 9 > /proc/iommu_debug/debug: reset to default trace log & dump\n");
	iommu_dump(s, "echo 10 > /proc/iommu_debug/debug: dump iova trace\n");
	iommu_dump(s, "echo 11 > /proc/iommu_debug/debug: dump iova alloc list\n");
	iommu_dump(s, "echo 12 > /proc/iommu_debug/debug: dump iova map list\n");

	return 0;
}

/* Notice: Please also update help info if debug command changes */
static int m4u_debug_set(void *data, u64 val)
{
	int ret = 0;

	pr_info("%s:val=%llu\n", __func__, val);

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	switch (val) {
	case 1:	/* show help info */
		ret = mtk_iommu_debug_help(NULL);
		break;
	case 2: /* mm translation fault test */
		report_custom_iommu_fault(0, 0, 0x500000f, MM_IOMMU, 0);
		break;
	case 3: /* apu translation fault test */
		report_custom_iommu_fault(0, 0, 0x102, APU_IOMMU, 0);
		break;
	case 4: /* peri translation fault test */
		report_custom_iommu_fault(0, 0, 0x102, PERI_IOMMU, 0);
		break;
	case 5:	/* enable trace log */
		event_mgr[IOMMU_ALLOC].dump_log = 1;
		event_mgr[IOMMU_FREE].dump_log = 1;
		event_mgr[IOMMU_MAP].dump_log = 1;
		event_mgr[IOMMU_UNMAP].dump_log = 1;
		break;
	case 6:	/* disable trace log */
		event_mgr[IOMMU_ALLOC].dump_log = 0;
		event_mgr[IOMMU_FREE].dump_log = 0;
		event_mgr[IOMMU_MAP].dump_log = 0;
		event_mgr[IOMMU_UNMAP].dump_log = 0;
		break;
	case 7:	/* enable trace dump */
		event_mgr[IOMMU_ALLOC].dump_trace = 1;
		event_mgr[IOMMU_FREE].dump_trace = 1;
		event_mgr[IOMMU_MAP].dump_trace = 1;
		event_mgr[IOMMU_UNMAP].dump_trace = 1;
		event_mgr[IOMMU_SYNC].dump_trace = 1;
		event_mgr[IOMMU_UNSYNC].dump_trace = 1;
		break;
	case 8:	/* disable trace dump */
		event_mgr[IOMMU_ALLOC].dump_trace = 0;
		event_mgr[IOMMU_FREE].dump_trace = 0;
		event_mgr[IOMMU_MAP].dump_trace = 0;
		event_mgr[IOMMU_UNMAP].dump_trace = 0;
		event_mgr[IOMMU_SYNC].dump_trace = 0;
		event_mgr[IOMMU_UNSYNC].dump_trace = 0;
		break;
	case 9:	/* reset to default trace log & dump */
		event_mgr[IOMMU_ALLOC].dump_trace = 1;
		event_mgr[IOMMU_FREE].dump_trace = 1;
		event_mgr[IOMMU_SYNC].dump_trace = 1;
		event_mgr[IOMMU_UNSYNC].dump_trace = 1;
		event_mgr[IOMMU_MAP].dump_trace = 0;
		event_mgr[IOMMU_UNMAP].dump_trace = 0;
		event_mgr[IOMMU_ALLOC].dump_log = 0;
		event_mgr[IOMMU_FREE].dump_log = 0;
		event_mgr[IOMMU_MAP].dump_log = 0;
		event_mgr[IOMMU_UNMAP].dump_log = 0;
		break;
	case 10:	/* dump iova trace */
		mtk_iommu_trace_dump(NULL);
		break;
	case 11:	/* dump iova alloc list */
		mtk_iommu_iova_alloc_dump_top(NULL, NULL);
		mtk_iommu_iova_alloc_dump(NULL, NULL);
		break;
	case 12:	/* dump iova map list */
		if (smmu_v3_enable) {
			mtk_iommu_iova_map_dump(NULL, 0, MM_SMMU);
			mtk_iommu_iova_map_dump(NULL, 0, APU_SMMU);
		} else {
			mtk_iommu_iova_map_dump(NULL, 0, MM_TABLE);
			mtk_iommu_iova_map_dump(NULL, 0, APU_TABLE);
		}
		break;
	default:
		pr_err("%s error,val=%llu\n", __func__, val);
		break;
	}
#endif

	if (ret)
		pr_info("%s failed:val=%llu, ret=%d\n", __func__, val, ret);

	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_PROC_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

/* Define proc_ops: *_proc_show function will be called when file is opened */
#define DEFINE_PROC_FOPS_RO(name)				\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			pde_data(inode));			\
	}							\
	static const struct proc_ops name = {			\
		.proc_open		= name ## _proc_open,	\
		.proc_read		= seq_read,		\
		.proc_lseek		= seq_lseek,		\
		.proc_release	= single_release,		\
	}

static int mtk_iommu_help_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_debug_help(s);
	return 0;
}

static int mtk_iommu_dump_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_trace_dump(s);
	mtk_iommu_iova_alloc_dump(s, NULL);
	mtk_iommu_iova_alloc_dump_top(s, NULL);

	if (smmu_v3_enable) {
		int i, ret;

		/* dump all smmu if exist */
		for (i = 0; i < SMMU_TYPE_NUM; i++) {
			/* skip GPU SMMU */
			if (i == GPU_SMMU)
				continue;

			ret = mtk_smmu_power_get(i);
			iommu_dump(s, "smmu_%d: %s\n", i, get_power_status_str(ret));
			if (ret)
				continue;

			mtk_smmu_wpreg_dump(s, i);
			mtk_smmu_power_put(i);

			/* no need dump all page table raw data */
			mtk_smmu_pgtable_dump(s, i, false);
		}
	}
	return 0;
}

static int mtk_iommu_iova_alloc_fops_proc_show(struct seq_file *s, void *unused)
{
	mtk_iommu_iova_alloc_dump_top(s, NULL);
	mtk_iommu_iova_alloc_dump(s, NULL);
	return 0;
}

static int mtk_iommu_iova_map_fops_proc_show(struct seq_file *s, void *unused)
{
	if (smmu_v3_enable) {
		mtk_iommu_iova_map_dump(s, 0, MM_SMMU);
		mtk_iommu_iova_map_dump(s, 0, APU_SMMU);
	} else {
		mtk_iommu_iova_map_dump(s, 0, MM_TABLE);
		mtk_iommu_iova_map_dump(s, 0, APU_TABLE);
	}
	return 0;
}

static int mtk_smmu_wp_fops_proc_show(struct seq_file *s, void *unused)
{
	if (smmu_v3_enable) {
		int i, ret;

		/* dump all smmu if exist */
		for (i = 0; i < SMMU_TYPE_NUM; i++) {
			ret = mtk_smmu_power_get(i);
			iommu_dump(s, "smmu_%d: %s\n", i, get_power_status_str(ret));
			if (ret)
				continue;

			mtk_smmu_wpreg_dump(s, i);
			mtk_smmu_power_put(i);
		}
	}
	return 0;
}

static int mtk_smmu_pgtable_fops_proc_show(struct seq_file *s, void *unused)
{
	if (smmu_v3_enable) {
		int i;

		/* dump all smmu if exist */
		for (i = 0; i < SMMU_TYPE_NUM; i++)
			mtk_smmu_pgtable_dump(s, i, true);
	}
	return 0;
}

/* adb shell cat /proc/iommu_debug/xxx */
DEFINE_PROC_FOPS_RO(mtk_iommu_help_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_dump_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_iova_alloc_fops);
DEFINE_PROC_FOPS_RO(mtk_iommu_iova_map_fops);
DEFINE_PROC_FOPS_RO(mtk_smmu_wp_fops);
DEFINE_PROC_FOPS_RO(mtk_smmu_pgtable_fops);

static void mtk_iommu_trace_init(struct mtk_m4u_data *data)
{
	int total_size = IOMMU_EVENT_COUNT_MAX * sizeof(struct iommu_event_t);

	strncpy(event_mgr[IOMMU_ALLOC].name, "alloc", 10);
	strncpy(event_mgr[IOMMU_FREE].name, "free", 10);
	strncpy(event_mgr[IOMMU_MAP].name, "map", 10);
	strncpy(event_mgr[IOMMU_UNMAP].name, "unmap", 10);
	strncpy(event_mgr[IOMMU_SYNC].name, "sync", 10);
	strncpy(event_mgr[IOMMU_UNSYNC].name, "unsync", 10);
	strncpy(event_mgr[IOMMU_SUSPEND].name, "suspend", 10);
	strncpy(event_mgr[IOMMU_RESUME].name, "resume", 10);
	strncpy(event_mgr[IOMMU_POWER_ON].name, "pwr_on", 10);
	strncpy(event_mgr[IOMMU_POWER_OFF].name, "pwr_off", 10);
	event_mgr[IOMMU_ALLOC].dump_trace = 1;
	event_mgr[IOMMU_FREE].dump_trace = 1;
	event_mgr[IOMMU_SYNC].dump_trace = 1;
	event_mgr[IOMMU_UNSYNC].dump_trace = 1;
	event_mgr[IOMMU_SUSPEND].dump_trace = 1;
	event_mgr[IOMMU_RESUME].dump_trace = 1;
	event_mgr[IOMMU_POWER_ON].dump_trace = 1;
	event_mgr[IOMMU_POWER_OFF].dump_trace = 1;

	event_mgr[IOMMU_SUSPEND].dump_log = 1;
	event_mgr[IOMMU_RESUME].dump_log = 1;
	event_mgr[IOMMU_POWER_ON].dump_log = 0;
	event_mgr[IOMMU_POWER_OFF].dump_log = 0;

	iommu_globals.record = vmalloc(total_size);
	if (!iommu_globals.record) {
		iommu_globals.enable = 0;
		return;
	}

	memset(iommu_globals.record, 0, total_size);
	iommu_globals.enable = 1;
	iommu_globals.dump_enable = 1;
	iommu_globals.write_pointer = 0;

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	iommu_globals.map_record = (smmu_v3_enable ? 0 : 1);
#else
	iommu_globals.map_record = 0;
#endif

	spin_lock_init(&iommu_globals.lock);
}

static void mtk_iommu_trace_rec_write(int event,
	unsigned long data1, unsigned long data2,
	unsigned long data3, struct device *dev)
{
	unsigned int index;
	struct iommu_event_t *p_event = NULL;
	unsigned long flags;

	if (iommu_globals.enable == 0)
		return;
	if ((event >= IOMMU_EVENT_MAX) ||
	    (event < 0))
		return;

	if (event_mgr[event].dump_log) {
		if (smmu_v3_enable)
			pr_info("[trace] %5s |0x%-9lx |%9zx |0x%x |0x%-4x |%s\n",
				event_mgr[event].name,
				data1,
				data2,
				smmu_tab_id_to_smmu_id(data3),
				smmu_tab_id_to_asid(data3),
				(dev != NULL ? dev_name(dev) : ""));
		else
			pr_info("[trace] %5s |0x%-9lx |%9zx |0x%lx |%s\n",
				event_mgr[event].name,
				data1, data2, data3,
				(dev != NULL ? dev_name(dev) : ""));
	}

	if (event_mgr[event].dump_trace == 0)
		return;

	index = (atomic_inc_return((atomic_t *)
			&(iommu_globals.write_pointer)) - 1)
	    % IOMMU_EVENT_COUNT_MAX;

	spin_lock_irqsave(&iommu_globals.lock, flags);

	p_event = (struct iommu_event_t *)
		&(iommu_globals.record[index]);
	mtk_iommu_system_time(&(p_event->time_high), &(p_event->time_low));
	p_event->event_id = event;
	p_event->data1 = data1;
	p_event->data2 = data2;
	p_event->data3 = data3;
	p_event->dev = dev;

	spin_unlock_irqrestore(&iommu_globals.lock, flags);
}

static void mtk_iommu_iova_trace(int event,
	dma_addr_t iova, size_t size,
	u64 tab_id, struct device *dev)
{
	u32 id = (iova >> 32);

	if (id >= MTK_IOVA_SPACE_NUM) {
		pr_err("out of iova space: 0x%llx\n", iova);
		return;
	}

	mtk_iommu_trace_rec_write(event, (unsigned long) iova, size, tab_id, dev);
}

void mtk_iommu_tlb_sync_trace(u64 iova, size_t size, int iommu_ids)
{
	mtk_iommu_trace_rec_write(IOMMU_SYNC, (unsigned long) iova, size,
				  (unsigned long) iommu_ids, NULL);
}
EXPORT_SYMBOL_GPL(mtk_iommu_tlb_sync_trace);

void mtk_iommu_pm_trace(int event, int iommu_id, int pd_sta,
	unsigned long flags, struct device *dev)
{
	mtk_iommu_trace_rec_write(event, (unsigned long) pd_sta, flags,
				  (unsigned long) iommu_id, dev);
}
EXPORT_SYMBOL_GPL(mtk_iommu_pm_trace);

static int m4u_debug_init(struct mtk_m4u_data *data)
{
	struct proc_dir_entry *debug_file;

	data->debug_root = proc_mkdir("iommu_debug", NULL);

	if (IS_ERR_OR_NULL(data->debug_root))
		pr_err("failed to create debug dir\n");

	debug_file = proc_create_data("debug",
		S_IFREG | 0640, data->debug_root, &m4u_debug_fops, NULL);

	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to create debug file\n");

	debug_file = proc_create_data("help",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_help_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create help file\n");

	debug_file = proc_create_data("iommu_dump",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_dump_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iommu_dump file\n");

	debug_file = proc_create_data("iova_alloc",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_iova_alloc_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iova_alloc file\n");

	debug_file = proc_create_data("iova_map",
		S_IFREG | 0640, data->debug_root, &mtk_iommu_iova_map_fops, NULL);
	if (IS_ERR_OR_NULL(debug_file))
		pr_err("failed to proc_create iova_map file\n");

	if (smmu_v3_enable) {
		debug_file = proc_create_data("smmu_wp",
			S_IFREG | 0640, data->debug_root, &mtk_smmu_wp_fops, NULL);
		if (IS_ERR_OR_NULL(debug_file))
			pr_err("failed to proc_create smmu_wp file\n");

		debug_file = proc_create_data("smmu_pgtable",
			S_IFREG | 0640, data->debug_root, &mtk_smmu_pgtable_fops, NULL);
		if (IS_ERR_OR_NULL(debug_file))
			pr_err("failed to proc_create smmu_pgtable file\n");
	}

	mtk_iommu_trace_init(data);

	spin_lock_init(&iova_list.lock);
	INIT_LIST_HEAD(&iova_list.head);

	spin_lock_init(&map_list.lock);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE0]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE1]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE2]);
	INIT_LIST_HEAD(&map_list.head[MTK_IOVA_SPACE3]);

	spin_lock_init(&count_list.lock);
	INIT_LIST_HEAD(&count_list.head);

	return 0;
}

static int iova_size_cmp(void *priv, const struct list_head *a,
	const struct list_head *b)
{
	struct iova_count_info *ia, *ib;

	ia = list_entry(a, struct iova_count_info, list_node);
	ib = list_entry(b, struct iova_count_info, list_node);

	if (ia->size < ib->size)
		return 1;
	if (ia->size > ib->size)
		return -1;

	return 0;
}

static void mtk_iommu_clear_iova_size(void)
{
	struct iova_count_info *plist;
	struct iova_count_info *tmp_plist;

	list_for_each_entry_safe(plist, tmp_plist, &count_list.head, list_node) {
		list_del(&plist->list_node);
		kfree(plist);
	}
}

static void mtk_iommu_count_iova_size(
	struct device *dev, dma_addr_t iova, size_t size)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_count_info *plist = NULL;
	struct iova_count_info *n = NULL;
	struct iova_count_info *new_info;
	u64 tab_id;
	u32 dom_id;

	fwspec = dev_iommu_fwspec_get(dev);
	if (fwspec == NULL) {
		pr_notice("%s fail! dev:%s, fwspec is NULL\n",
			  __func__, dev_name(dev));
		return;
	}

	/* Add to iova_count_info if exist */
	spin_lock(&count_list.lock);
	list_for_each_entry_safe(plist, n, &count_list.head, list_node) {
		if (plist->dev == dev) {
			plist->count++;
			plist->size += (unsigned long) (size / 1024);
			spin_unlock(&count_list.lock);
			return;
		}
	}

	/* Create new iova_count_info if no exist */
	new_info = kzalloc(sizeof(*new_info), GFP_ATOMIC);
	if (!new_info) {
		spin_unlock(&count_list.lock);
		pr_notice("%s, alloc iova_count_info fail! dev:%s\n",
			  __func__, dev_name(dev));
		return;
	}

	if (smmu_v3_enable) {
		tab_id = get_smmu_tab_id(dev);
		dom_id = 0;
	} else {
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	}

	new_info->tab_id = tab_id;
	new_info->dom_id = dom_id;
	new_info->dev = dev;
	new_info->size = (unsigned long) (size / 1024);
	new_info->count = 1;
	list_add_tail(&new_info->list_node, &count_list.head);
	spin_unlock(&count_list.lock);
}

static void mtk_iommu_iova_alloc_dump_top(
	struct seq_file *s, struct device *dev)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	struct iova_count_info *p_count_list = NULL;
	struct iova_count_info *n_count = NULL;
	int total_cnt = 0, dom_count = 0, i = 0;
	u64 size = 0, total_size = 0, dom_size = 0;
	int smmu_id = -1, stream_id = -1;
	u64 tab_id = 0;
	u32 dom_id = 0;

	/* check fwspec by device */
	if (dev != NULL) {
		fwspec = dev_iommu_fwspec_get(dev);
		if (fwspec == NULL) {
			pr_notice("%s fail! dev:%s, fwspec is NULL\n",
				  __func__, dev_name(dev));
			return;
		}
		if (smmu_v3_enable) {
			smmu_id = get_smmu_id(dev);
			stream_id = get_smmu_stream_id(dev);
			tab_id = get_smmu_tab_id(dev);
			dom_id = 0;
		} else {
			tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
			dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		}
	}

	/* count iova size by device */
	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node) {
		size = (unsigned long) (plist->size / 1024);
		if (dev == NULL ||
		    (plist->dom_id == dom_id && plist->tab_id == tab_id)) {
			mtk_iommu_count_iova_size(plist->dev, plist->iova, plist->size);
			dom_size += size;
			dom_count++;
		}
		total_size += size;
		total_cnt++;
	}
	spin_unlock(&iova_list.lock);

	spin_lock(&count_list.lock);
	/* sort count iova size by device */
	list_sort(NULL, &count_list.head, iova_size_cmp);

	/* dump top max user */
	if (smmu_v3_enable) {
		iommu_dump(s,
			   "smmu iova alloc total:(%d/%lluKB), dom:(%d/%lluKB,%d,%d,%d) top %d user:\n",
			   total_cnt, total_size, dom_count, dom_size,
			   smmu_id, stream_id, dom_id, IOVA_DUMP_TOP_MAX);
		iommu_dump(s, "%-8s %-10s %-10s %-7s %-10s %-16s %s\n",
			   "smmu_id", "stream_id", "asid", "dom_id", "count",
			   "size(KB)", "dev");
	} else {
		iommu_dump(s,
			   "iommu iova alloc total:(%d/%lluKB), dom:(%d/%lluKB,%llu,%d) top %d user:\n",
			   total_cnt, total_size, dom_count, dom_size,
			   tab_id, dom_id, IOVA_DUMP_TOP_MAX);
		iommu_dump(s, "%11s %6s %8s %10s %3s\n",
			   "tab_id", "dom_id", "count", "size", "dev");
	}

	list_for_each_entry_safe(p_count_list, n_count, &count_list.head, list_node) {
		if (smmu_v3_enable) {
			iommu_dump(s, "%-8u 0x%-8x 0x%-8x %-7u %-10u %-16llu %s\n",
				   smmu_tab_id_to_smmu_id(p_count_list->tab_id),
				   get_smmu_stream_id(p_count_list->dev),
				   smmu_tab_id_to_asid(p_count_list->tab_id),
				   p_count_list->dom_id,
				   p_count_list->count,
				   p_count_list->size,
				   dev_name(p_count_list->dev));
		} else {
			iommu_dump(s, "%6llu %6u %8u %8lluKB %s\n",
				   p_count_list->tab_id,
				   p_count_list->dom_id,
				   p_count_list->count,
				   p_count_list->size,
				   dev_name(p_count_list->dev));
		}
		i++;
		if (i >= IOVA_DUMP_TOP_MAX)
			break;
	}

	/* clear count iova size */
	mtk_iommu_clear_iova_size();

	spin_unlock(&count_list.lock);
}

static void mtk_iommu_iova_alloc_dump(struct seq_file *s, struct device *dev)
{
	struct iommu_fwspec *fwspec = NULL;
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	int dump_count = 0;
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (dev != NULL) {
		fwspec = dev_iommu_fwspec_get(dev);
		if (fwspec == NULL) {
			pr_info("%s fail! dev:%s, fwspec is NULL\n",
				__func__, dev_name(dev));
			return;
		}
		if (smmu_v3_enable) {
			tab_id = get_smmu_tab_id(dev);
			dom_id = 0;
		} else {
			tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
			dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
		}
	}

	if (smmu_v3_enable) {
		iommu_dump(s, "smmu iova alloc dump:\n");
		iommu_dump(s, "%-9s %-10s %-10s %-18s %-14s %17s, %s\n",
			   "smmu_id", "stream_id", "asid", "iova", "size", "time", "dev");
	} else {
		iommu_dump(s, "iommu iova alloc dump:\n");
		iommu_dump(s, "%6s %6s %-18s %-10s %17s %s\n",
			   "tab_id", "dom_id", "iova", "size", "time", "dev");
	}

	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node)
		if (dev == NULL ||
		    (plist->dom_id == dom_id && plist->tab_id == tab_id)) {
			if (smmu_v3_enable)
				iommu_dump(s, "%-9u 0x%-8x 0x%-8x %-18pa 0x%-12zx %10llu.%06u %s\n",
					   smmu_tab_id_to_smmu_id(plist->tab_id),
					   get_smmu_stream_id(plist->dev),
					   smmu_tab_id_to_asid(plist->tab_id),
					   &plist->iova,
					   plist->size,
					   plist->time_high,
					   plist->time_low,
					   dev_name(plist->dev));
			else
				iommu_dump(s, "%6llu %6u %-18pa 0x%-8zx %10llu.%06u %s\n",
					   plist->tab_id,
					   plist->dom_id,
					   &plist->iova,
					   plist->size,
					   plist->time_high,
					   plist->time_low,
					   dev_name(plist->dev));

			if (s == NULL && dev != NULL && ++dump_count > IOVA_DUMP_LOG_MAX)
				break;
		}
	spin_unlock(&iova_list.lock);
}

/* For smmu, tab_id is smmu hardware id */
static void mtk_iommu_iova_dump(struct seq_file *s, u64 iova, u64 tab_id)
{
	struct iova_info *plist = NULL;
	struct iova_info *n = NULL;
	int dump_count = 0;

	if (!iova || tab_id > MTK_M4U_TAB_NR_MAX) {
		pr_info("%s fail, invalid iova:0x%llx tab_id:%llu\n",
			__func__, iova, tab_id);
		return;
	}

	if (smmu_v3_enable) {
		iommu_dump(s, "smmu iova dump:\n");
		iommu_dump(s, "%-9s %-10s %-10s %-18s %-14s %17s, %s\n",
			   "smmu_id", "stream_id", "asid", "iova", "size", "time", "dev");
	} else {
		iommu_dump(s, "iommu iova dump:\n");
		iommu_dump(s, "%6s %6s %-18s %-10s %17s %s\n",
			   "tab_id", "dom_id", "iova", "size", "time", "dev");
	}

	spin_lock(&iova_list.lock);
	list_for_each_entry_safe(plist, n, &iova_list.head, list_node)
		if (to_smmu_hw_id(plist->tab_id) == tab_id &&
			    iova <= (plist->iova + plist->size) &&
			    iova >= (plist->iova)) {
			if (smmu_v3_enable)
				iommu_dump(s, "%-9u 0x%-8x 0x%-8x %-18pa 0x%-12zx %10llu.%06u %s\n",
					   smmu_tab_id_to_smmu_id(plist->tab_id),
					   get_smmu_stream_id(plist->dev),
					   smmu_tab_id_to_asid(plist->tab_id),
					   &plist->iova,
					   plist->size,
					   plist->time_high,
					   plist->time_low,
					   dev_name(plist->dev));
			else
				iommu_dump(s, "%6llu %6u %-18pa 0x%-8zx %10llu.%06u %s\n",
					   plist->tab_id,
					   plist->dom_id,
					   &plist->iova,
					   plist->size,
					   plist->time_high,
					   plist->time_low,
					   dev_name(plist->dev));

			if (s == NULL && ++dump_count > IOVA_DUMP_LOG_MAX)
				break;
		}
	spin_unlock(&iova_list.lock);
}

static void mtk_iova_dbg_alloc(struct device *dev,
	struct iova_domain *iovad, dma_addr_t iova, size_t size)
{
	static DEFINE_RATELIMIT_STATE(dump_rs, IOVA_DUMP_RS_INTERVAL,
				      IOVA_DUMP_RS_BURST);
	struct iova_info *iova_buf;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	u64 tab_id = 0;
	u32 dom_id = 0;

	if (!fwspec) {
		pr_info("%s fail, dev(%s) is not iommu-dev\n",
			__func__, dev_name(dev));
		return;
	}

	if (smmu_v3_enable) {
		tab_id = get_smmu_tab_id(dev);
		dom_id = 0;
	} else {
		tab_id = MTK_M4U_TO_TAB(fwspec->ids[0]);
		dom_id = MTK_M4U_TO_DOM(fwspec->ids[0]);
	}

	if (!iova) {
		pr_info("%s fail! dev:%s, size:0x%zx\n",
			__func__, dev_name(dev), size);

		if (!__ratelimit(&dump_rs))
			return;

		if (dom_id > 0 || smmu_v3_enable)
			mtk_iommu_iova_alloc_dump(NULL, dev);

		return mtk_iommu_iova_alloc_dump_top(NULL, dev);
	}

	iova_buf = kzalloc(sizeof(*iova_buf), GFP_ATOMIC);
	if (!iova_buf)
		return;

	mtk_iommu_system_time(&(iova_buf->time_high), &(iova_buf->time_low));
	iova_buf->tab_id = tab_id;
	iova_buf->dom_id = dom_id;
	iova_buf->dev = dev;
	iova_buf->iovad = iovad;
	iova_buf->iova = iova;
	iova_buf->size = size;
	spin_lock(&iova_list.lock);
	list_add(&iova_buf->list_node, &iova_list.head);
	spin_unlock(&iova_list.lock);

	mtk_iommu_iova_trace(IOMMU_ALLOC, iova, size, tab_id, dev);
}

static void mtk_iova_dbg_free(
	struct iova_domain *iovad, dma_addr_t iova, size_t size)
{
	u64 start_t, end_t;
	struct iova_info *plist;
	struct iova_info *tmp_plist;
	struct device *dev = NULL;
	u64 tab_id = 0;
	int i = 0;

	spin_lock(&iova_list.lock);
	start_t = sched_clock();
	list_for_each_entry_safe(plist, tmp_plist, &iova_list.head, list_node) {
		i++;
		if (plist->iova == iova &&
		    plist->size == size &&
		    plist->iovad == iovad) {
			tab_id = plist->tab_id;
			dev = plist->dev;
			list_del(&plist->list_node);
			kfree(plist);
			break;
		}
	}
	end_t = sched_clock();
	spin_unlock(&iova_list.lock);

	if ((end_t - start_t) > FIND_IOVA_TIMEOUT_NS)
		pr_info("%s warnning, dev:%s, find iova:[0x%llx 0x%llx 0x%zx] in %d timeout:%llu\n",
			__func__, (dev ? dev_name(dev) : "NULL"),
			tab_id, iova, size, i, (end_t - start_t));

	if (dev == NULL)
		pr_info("%s warnning, iova:[0x%llx 0x%zx] not find in %d\n",
			__func__, iova, size, i);

	mtk_iommu_iova_trace(IOMMU_FREE, iova, size, tab_id, dev);
}

/* all code inside alloc_iova_hook can't be scheduled! */
static void alloc_iova_hook(void *data,
	struct device *dev, struct iova_domain *iovad,
	dma_addr_t iova, size_t size)
{
	return mtk_iova_dbg_alloc(dev, iovad, iova, size);
}

/* all code inside free_iova_hook can't be scheduled! */
static void free_iova_hook(void *data,
	struct iova_domain *iovad,
	dma_addr_t iova, size_t size)
{
	return mtk_iova_dbg_free(iovad, iova, size);
}

static int mtk_m4u_dbg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 total_port;
	int ret = 0;

	smmu_v3_enable = smmu_v3_enabled();
	pr_info("%s start, smmu_v3_enable:%d\n", __func__, smmu_v3_enable);

	m4u_data = devm_kzalloc(dev, sizeof(struct mtk_m4u_data), GFP_KERNEL);
	if (!m4u_data)
		return -ENOMEM;

	m4u_data->dev = dev;
	m4u_data->plat_data = of_device_get_match_data(dev);
	total_port = m4u_data->plat_data->port_nr[MM_IOMMU] +
		     m4u_data->plat_data->port_nr[APU_IOMMU] +
		     m4u_data->plat_data->port_nr[PERI_IOMMU];
	m4u_data->m4u_cb = devm_kzalloc(dev, total_port *
		sizeof(struct mtk_iommu_cb), GFP_KERNEL);
	if (!m4u_data->m4u_cb)
		return -ENOMEM;

	m4u_debug_init(m4u_data);

	ret = register_trace_android_vh_iommu_iovad_alloc_iova(alloc_iova_hook,
							       "mtk_m4u_dbg_probe");
	pr_debug("add alloc iova hook %s\n", (ret ? "fail" : "pass"));
	ret = register_trace_android_vh_iommu_iovad_free_iova(free_iova_hook,
							      "mtk_m4u_dbg_probe");
	pr_debug("add free iova hook %s\n", (ret ? "fail" : "pass"));

	return 0;
}

static const struct mau_config_info mau_config_default[] = {
	/* Monitor each IOMMU input IOVA<4K and output PA=0 */
	MAU_CONFIG_INIT(0, 0, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 0, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(0, 1, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(0, 1, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(1, 0, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 0, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),

	MAU_CONFIG_INIT(1, 1, 0, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 0, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 0, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x0, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 1, 0x0, (SZ_4K - 1),
			0xffffffff, 0xffffffff, 0x1, 0x1, 0x0, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 2, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x0, 0x0, 0x1, 0x0, 0x0),
	MAU_CONFIG_INIT(1, 1, 1, 3, 0x0, 0x1,
			0xffffffff, 0xffffffff, 0x1, 0x0, 0x1, 0x0, 0x0),
};

static int mt6855_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6878_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static u32 mt6878_get_valid_tf_id(int tf_id, u32 type, int id)
{
	u32 vld_id = 0;

	if (type == APU_IOMMU)
		vld_id = FIELD_GET(GENMASK(11, 8), tf_id);
	else
		vld_id = tf_id & F_MMU_INT_TF_MSK;

	return vld_id;
}

static int mt6879_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 9), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6886_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6897_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static u32 mt6897_get_valid_tf_id(int tf_id, u32 type, int id)
{
	u32 vld_id = 0;

	if (type == APU_IOMMU)
		vld_id = FIELD_GET(GENMASK(11, 8), tf_id);
	else
		vld_id = tf_id & F_MMU_INT_TF_MSK;

	return vld_id;
}

static int mt6983_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 10), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);

}

static int mt6985_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static int mt6989_tf_is_gce_videoup(u32 port_tf, u32 vld_tf)
{
	return F_MMU_INT_TF_LARB(port_tf) ==
	       FIELD_GET(GENMASK(12, 8), vld_tf) &&
	       F_MMU_INT_TF_PORT(port_tf) ==
	       FIELD_GET(GENMASK(1, 0), vld_tf);
}

static u32 mt6989_get_valid_tf_id(int tf_id, u32 type, int id)
{
	u32 vld_id = 0;

	if (type == APU_SMMU)
		vld_id = FIELD_GET(GENMASK(12, 8), tf_id);
	else
		vld_id = tf_id & F_MMU_INT_TF_MSK;

	return vld_id;
}

static bool mt6989_tf_id_is_match(int tf_id, u32 type, int id,
				  struct mtk_iommu_port port)
{
	int vld_id = -1;

	if (port.id != id)
		return false;

	if (port.port_type == SPECIAL)
		vld_id = tf_id & F_MMU_INT_TF_SPEC_MSK(port.port_start);
	else if (port.port_type == NORMAL)
		vld_id = mt6989_get_valid_tf_id(tf_id, type, id);

	if (port.tf_id == vld_id)
		return true;

	return false;
}

static const u32 mt6989_smmu_common_ids[SMMU_TYPE_NUM][SMMU_TBU_CNT_MAX] = {
	[MM_SMMU] = {
		MM_SMMU_MDP,
		MM_SMMU_MDP,
		MM_SMMU_DISP,
		MM_SMMU_DISP,
	},
	[APU_SMMU] = {
		APU_SMMU_M0,
		APU_SMMU_M0,
		APU_SMMU_M0,
		APU_SMMU_M0,
	},
	[SOC_SMMU] = {
		SOC_SMMU_M4,
		SOC_SMMU_M6,
		SOC_SMMU_M7,
	},
};

static int mt6989_smmu_common_id(u32 smmu_type, u32 tbu_id)
{
	if (smmu_type >= SMMU_TYPE_NUM || tbu_id >= SMMU_TBU_CNT(smmu_type))
		return -1;

	return mt6989_smmu_common_ids[smmu_type][tbu_id];
}

static char *mt6989_smmu_soc_port_name(u32 type, int id, int tf_id)
{
	if (type != SOC_SMMU) {
		pr_info("%s is not support type:%u\n", __func__, type);
		return NULL;
	}

	switch (id) {
	case SOC_SMMU_M4:
		return mt6989_soc_m4_port_name(tf_id);
	case SOC_SMMU_M6:
		return mt6989_soc_m6_port_name(tf_id);
	case SOC_SMMU_M7:
		return mt6989_soc_m7_port_name(tf_id);
	default:
		return "SOC_UNKNOWN";
	}
}

static const struct mtk_m4u_plat_data mt6855_data = {
	.port_list[MM_IOMMU] = mm_port_mt6855,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6855),
	.mm_tf_is_gce_videoup = mt6855_tf_is_gce_videoup,
	.mm_tf_ccu_support = 0,
};

static const struct mtk_m4u_plat_data mt6983_data = {
	.port_list[MM_IOMMU] = mm_port_mt6983,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6983),
	.port_list[APU_IOMMU] = apu_port_mt6983,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6983),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6983_tf_is_gce_videoup,
	.peri_data	= mt6983_peri_iommu_data,
	.peri_tf_analyse = mt6983_peri_tf,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6878_data = {
	.port_list[MM_IOMMU] = mm_port_mt6878,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6878),
	.port_list[APU_IOMMU] = apu_port_mt6878,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6878),
	.mm_tf_is_gce_videoup = mt6878_tf_is_gce_videoup,
	.get_valid_tf_id = mt6878_get_valid_tf_id,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6879_data = {
	.port_list[MM_IOMMU] = mm_port_mt6879,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6879),
	.port_list[APU_IOMMU] = apu_port_mt6879,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6879),
	.port_list[PERI_IOMMU] = peri_port_mt6879,
	.port_nr[PERI_IOMMU]   = ARRAY_SIZE(peri_port_mt6879),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6879_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6886_data = {
	.port_list[MM_IOMMU] = mm_port_mt6886,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6886),
	.port_list[APU_IOMMU] = apu_port_mt6886,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6886),
	.mm_tf_is_gce_videoup = mt6886_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6895_data = {
	.port_list[MM_IOMMU] = mm_port_mt6895,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6895),
	.port_list[APU_IOMMU] = apu_port_mt6895,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6895),
	.mm_tf_ccu_support = 1,
	.mm_tf_is_gce_videoup = mt6983_tf_is_gce_videoup,
	.peri_data	= mt6983_peri_iommu_data,
	.peri_tf_analyse = mt6983_peri_tf,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6897_data = {
	.port_list[MM_IOMMU] = mm_port_mt6897,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6897),
	.port_list[APU_IOMMU] = apu_port_mt6897,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6897),
	.mm_tf_is_gce_videoup = mt6897_tf_is_gce_videoup,
	.get_valid_tf_id = mt6897_get_valid_tf_id,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6985_data = {
	.port_list[MM_IOMMU] = mm_port_mt6985,
	.port_nr[MM_IOMMU]   = ARRAY_SIZE(mm_port_mt6985),
	.port_list[APU_IOMMU] = apu_port_mt6985,
	.port_nr[APU_IOMMU]   = ARRAY_SIZE(apu_port_mt6985),
	.mm_tf_is_gce_videoup = mt6985_tf_is_gce_videoup,
	.mau_config	= mau_config_default,
	.mau_config_nr = ARRAY_SIZE(mau_config_default),
};

static const struct mtk_m4u_plat_data mt6989_smmu_data = {
	.port_list[MM_SMMU] = mm_port_mt6989,
	.port_nr[MM_SMMU]   = ARRAY_SIZE(mm_port_mt6989),
	.port_list[APU_SMMU] = apu_port_mt6989,
	.port_nr[APU_SMMU]   = ARRAY_SIZE(apu_port_mt6989),
	.get_valid_tf_id = mt6989_get_valid_tf_id,
	.tf_id_is_match = mt6989_tf_id_is_match,
	.mm_tf_is_gce_videoup = mt6989_tf_is_gce_videoup,
	.smmu_common_id = mt6989_smmu_common_id,
	.smmu_port_name = mt6989_smmu_soc_port_name,
};

static const struct of_device_id mtk_m4u_dbg_of_ids[] = {
	{ .compatible = "mediatek,mt6855-iommu-debug", .data = &mt6855_data},
	{ .compatible = "mediatek,mt6878-iommu-debug", .data = &mt6878_data},
	{ .compatible = "mediatek,mt6879-iommu-debug", .data = &mt6879_data},
	{ .compatible = "mediatek,mt6886-iommu-debug", .data = &mt6886_data},
	{ .compatible = "mediatek,mt6895-iommu-debug", .data = &mt6895_data},
	{ .compatible = "mediatek,mt6897-iommu-debug", .data = &mt6897_data},
	{ .compatible = "mediatek,mt6983-iommu-debug", .data = &mt6983_data},
	{ .compatible = "mediatek,mt6985-iommu-debug", .data = &mt6985_data},
	{ .compatible = "mediatek,mt6989-smmu-debug", .data = &mt6989_smmu_data},
	{},
};

static struct platform_driver mtk_m4u_dbg_drv = {
	.probe	= mtk_m4u_dbg_probe,
	.driver	= {
		.name = "mtk-m4u-debug",
		.of_match_table = of_match_ptr(mtk_m4u_dbg_of_ids),
	}
};

module_platform_driver(mtk_m4u_dbg_drv);
MODULE_LICENSE("GPL v2");
