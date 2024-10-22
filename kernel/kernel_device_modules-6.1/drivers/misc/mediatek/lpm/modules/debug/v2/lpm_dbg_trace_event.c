// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <lpm_trace_event/lpm_trace_event.h>
#include <lpm_dbg_trace_event.h>
#include <mtk_spm_sysfs.h>

#define plat_mmio_read(offset)	__raw_readl(spm_base + offset)

void __iomem *spm_base;
struct timer_list spm_resource_req_timer;
u32 spm_resource_req_timer_is_enabled;
u32 spm_resource_req_timer_ms;
static struct subsys_req *lpm_subsys_req = NULL;
static u32 lpm_subsys_req_num;

enum subsys_req_indexx {
	SUBSYS_REQ_MD,
	SUBSYS_REQ_CONN,
	SUBSYS_REQ_SCP,
	SUBSYS_REQ_ADSP,
	SUBSYS_REQ_UFS,
	SUBSYS_REQ_MSDC,
	SUBSYS_REQ_DISP,
	SUBSYS_REQ_APU,
	SUBSYS_REQ_SPM,
	SUBSYS_REQ_MAX,
};

static void spm_resource_req_timer_fn(struct timer_list *data)
{
	unsigned int i, j;
	char *subsys_req_name[SUBSYS_REQ_MAX] = {
		"md",
		"conn",
		"scp",
		"adsp",
		"ufs",
		"msdc",
		"disp",
		"apu",
		"spm"
	};
	unsigned int subsys_req[SUBSYS_REQ_MAX] = {0};

	for (i = 0; i < lpm_subsys_req_num; i++) {
		for (j = 0; j < SUBSYS_REQ_MAX; j++) {
			if (!strcmp(subsys_req_name[j], lpm_subsys_req[i].name)) {
				if (lpm_subsys_req[i].req_addr1)
					subsys_req[j] =
						!!(plat_mmio_read(lpm_subsys_req[i].req_addr1) &
						lpm_subsys_req[i].req_mask1);
				if (lpm_subsys_req[i].req_addr2)
					subsys_req[j] |=
						!!(plat_mmio_read(lpm_subsys_req[i].req_addr2) &
						lpm_subsys_req[i].req_mask2);
			}
		}
	}

	trace_SPM__resource_req_0(
		subsys_req[SUBSYS_REQ_MD],
		subsys_req[SUBSYS_REQ_CONN],
		subsys_req[SUBSYS_REQ_SCP],
		subsys_req[SUBSYS_REQ_ADSP],
		subsys_req[SUBSYS_REQ_UFS],
		subsys_req[SUBSYS_REQ_MSDC],
		subsys_req[SUBSYS_REQ_DISP],
		subsys_req[SUBSYS_REQ_APU],
		subsys_req[SUBSYS_REQ_SPM]);

	spm_resource_req_timer.expires = jiffies +
		msecs_to_jiffies(spm_resource_req_timer_ms);
	add_timer(&spm_resource_req_timer);
}

static void spm_resource_req_timer_en(u32 enable, u32 timer_ms)
{
	if (enable) {
		if (spm_resource_req_timer_is_enabled)
			return;

		timer_setup(&spm_resource_req_timer,
			spm_resource_req_timer_fn, 0);

		spm_resource_req_timer_ms = timer_ms;
		spm_resource_req_timer.expires = jiffies +
			msecs_to_jiffies(spm_resource_req_timer_ms);
		add_timer(&spm_resource_req_timer);

		spm_resource_req_timer_is_enabled = true;
	} else if (spm_resource_req_timer_is_enabled) {
		del_timer(&spm_resource_req_timer);
		spm_resource_req_timer_is_enabled = false;
	}
}

ssize_t get_spm_resource_req_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz
				, "spm resource request timer is enabled: %d\n",
				spm_resource_req_timer_is_enabled);
	return (bLen > sz) ? sz : bLen;
}

ssize_t set_spm_resource_req_timer_enable(char *ToUserBuf
		, size_t sz, void *priv)
{
	u32 is_enable;
	u32 timer_ms;

	if (!ToUserBuf)
		return -EINVAL;

	if (sscanf(ToUserBuf, "%d %d", &is_enable, &timer_ms) == 2) {
		spm_resource_req_timer_en(is_enable, timer_ms);
		return sz;
	}

	if (kstrtouint(ToUserBuf, 10, &is_enable) == 0) {
		if (is_enable == 0) {
			spm_resource_req_timer_en(is_enable, 0);
			return sz;
		}
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op spm_resource_req_timer_enable_fops = {
	.fs_read = get_spm_resource_req_timer_enable,
	.fs_write = set_spm_resource_req_timer_enable,
};

int lpm_trace_event_init(struct subsys_req *plat_subsys_req, unsigned int plat_subsys_req_num)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node) {
		spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	lpm_subsys_req = plat_subsys_req;
	lpm_subsys_req_num = plat_subsys_req_num;
	mtk_spm_sysfs_root_entry_create();
	mtk_spm_sysfs_entry_node_add("spm_dump_res_req_enable", 0444
			, &spm_resource_req_timer_enable_fops, NULL);

	return 0;
}
EXPORT_SYMBOL(lpm_trace_event_init);

void lpm_trace_event_deinit(void)
{
}
EXPORT_SYMBOL(lpm_trace_event_deinit);
