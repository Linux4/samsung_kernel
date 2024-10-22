// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_spm_comm.h>
#include <lpm_dbg_common_v2.h>
#include <lpm_dbg_fs_common.h>
#include <lpm_dbg_trace_event.h>
#include <lpm_dbg_logger.h>
#include <lpm_timer.h>
#include <lpm_ccci_dump.h>

#include <mtk_ccci_common.h>

extern int ccci_register_md_state_receiver(unsigned char ch_id,
	void (*callback)(enum MD_STATE, enum MD_STATE));

static unsigned int spm_timestamp_size;

/* dump low power info to further identify MDEE is caused by LPM */
static void lpm_md_info_dump(enum MD_STATE old_state,
	enum MD_STATE new_state)
{
	#define LOG_BUF_OUT_SZ	(512)

	int i, log_size;
	unsigned int tmp;
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };

	switch (new_state) {
	case EXCEPTION:
		log_size = 0;
		for (i = 0; i < spm_timestamp_size; i++) {
			tmp = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_SPM_TIMESTAMP,
				MT_LPM_SMC_ACT_GET, i, 0);
			log_size += scnprintf(log_buf + log_size,
					      LOG_BUF_OUT_SZ - log_size,
					      "0x%x ", tmp);
		}
		pr_info("[name:spm&][LPM] spm event timestamp :%s\n", log_buf);
		break;
	default:
		break;
	}
}

int lpm_ccci_dump_init(void)
{
	int ret;

	ret = ccci_register_md_state_receiver(KERN_MD_LPM_DUMP,
					lpm_md_info_dump);
	if (ret)
		pr_debug("[name:spm&][LPM] register ccci md state receiver fail\n");

	spm_timestamp_size = lpm_smc_spm_dbg(MT_SPM_DBG_SMC_SPM_TIMESTAMP_SIZE,
		MT_LPM_SMC_ACT_GET, 0, 0);

	return 0;
}
