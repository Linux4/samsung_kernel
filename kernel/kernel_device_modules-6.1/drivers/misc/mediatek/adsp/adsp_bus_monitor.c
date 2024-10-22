// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include "adsp_helper.h"
#include "adsp_core.h"
#include "adsp_bus_monitor.h"
#include "adsp_platform_driver.h"

void adsp_bus_monitor_dump(void)
{
	struct adsp_priv *pdata = NULL;
	struct bus_monitor_debug_info debug_info;
	const struct sharedmem_info *item;
	void __iomem *dump_addr;
	u32 nums_core = get_adsp_core_total();

	if (nums_core == 0)
		return;

	/* obtain bus mon dump address */
	pdata = get_adsp_core_by_id(ADSP_A_ID);
	item = pdata->mapping_table + ADSP_SHAREDMEM_BUS_MON_DUMP;
	dump_addr = pdata->dtcm + pdata->dtcm_size - item->offset;

	adsp_enable_clock();
	memcpy_fromio(&debug_info, dump_addr, sizeof(struct bus_monitor_debug_info));
	adsp_disable_clock();

	if (debug_info.state <= STATE_RUN)
		return;

	pr_info("%s(), ADSP Bus Monitor Timeout!!, state: 0x%x, time: %d.%d",
		__func__, debug_info.state, debug_info.second, debug_info.mini_second);
	pr_info("%s(), ADSP debug info dump offset on Core 0 DTCM: 0x%x", __func__, item->offset);

}
EXPORT_SYMBOL(adsp_bus_monitor_dump);
