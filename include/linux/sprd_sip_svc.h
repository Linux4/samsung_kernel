/*
 * Copyright (C) 2012-2015 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SPRD_SIP_SVC_H__
#define __SPRD_SIP_SVC_H__

#include <linux/init.h>
#include <linux/types.h>

/*
 * Provides interfaces to SoC implementation-specific services on this
 * platform, for example secure platform initialization, configuration,
 * and some power control services.
 */

/* structure definitions */

/**
 * struct sprd_sip_svc_rev_info - version information structure
 *
 * @major_ver: Major ABI version. Change here implies risk of backward
 *	compatibility break.
 * @minor_ver: Minor ABI version. Change here implies new feature addition,
 *	or compatible change in ABI.
 */
struct sprd_sip_svc_rev_info {
	u32 major_ver;
	u32 minor_ver;
};

/**
 * struct sprd_sip_svc_perf_ops - represents the various operations provided
 *	by SPRD SIP PERF
 *
 * @set_freq: sets the frequency of a given device
 * @get_freq: gets the frequency of a given device
 *
 * @set_div: sets the clock divisor of a given device
 * @get_div: gets the clock divisor of a given device
 *
 * @set_parent: sets the parent of a given device
 * @get_parent: gets the parent of a given device
 */
struct sprd_sip_svc_perf_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*set_freq)(u32 id, u32 parent_id, u32 freq);
	int (*get_freq)(u32 id, u32 parent_id, u32 *p_freq);

	int (*set_div)(u32 id, u32 div);
	int (*get_div)(u32 id, u32 *p_div);

	int (*set_parent)(u32 id, u32 parent_id);
	int (*get_parent)(u32 id, u32 *p_parent_id);
};

/**
 * struct sprd_sip_svc_dbg_ops - represents the various operations provided
 *	by SPRD SIP DBG
 *
 * @set_hang_handle: set hang handle
 */
struct sprd_sip_svc_dbg_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*set_hang_hdl)(uintptr_t hdl, uintptr_t pgd);
	int (*get_hang_ctx)(uintptr_t id, uintptr_t *val);
};

/**
 * struct sprd_sip_svc_storage_ops - represents the various operations
 * 	provided by SPRD SIP STORAGE
 *
 * @ufs_crypto_enable: make crypto cfg field configurable to normal world
 * @ufs_crypto_disable: make crypto cfg field non-configurable to normal world
 */
struct sprd_sip_svc_storage_ops {
	struct sprd_sip_svc_rev_info rev;

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	int (*ufs_crypto_enable)(void);
	int (*ufs_crypto_disable)(void);
#endif
};

/**
 * struct sprd_sip_svc_pwr_ops - represents the various operations
 *      provided by SPRD SIP POWER
 *
 * @get_wakeup_source: gets the wakeup source
 *
 */
struct sprd_sip_svc_pwr_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*get_wakeup_source)(u32 *major, u32 *second, u32 *thrid);
};

/**
 * struct sprd_sip_svc_dvfs_ops - represents the various operations
 *      provided by SPRD SIP DVFS
 *
 * @chip_config: set the chip version
 *
 * @phy_enable: enable cluster dvfs
 * @table_update: update dvfs table for cluster
 * @cluster_config: set the cluster information
 *
 * @freq_set: set cluster frequency
 * @freq_get: get cluster current frequency
 * @index_info: get freq and volt of index
 *
 */
struct sprd_sip_svc_dvfs_ops {
	struct sprd_sip_svc_rev_info rev;

	int (*chip_config)(u32 ver);

	int (*phy_enable)(u32 cluster);
	int (*table_update)(u32 cluster, u32 *num, u32 temp);
	int (*cluster_config)(u32 cluster, u32 bin, u32 margin, u32 step);

	int (*freq_set)(u32 cluster, u32 index);
	int (*freq_get)(u32 cluster, u64 *freq);
	int (*index_info)(u32 cluster, u32 index, u64 *freq, u64 *vol);
};

/**
 * struct sprd_sip_svc_handle - Handle returned to SPRD SIP clients for usage
 *
 * @perf_ops: pointer to set of performance operations
 * @dbg_ops: pointer to set of dbg operations
 * @storage_ops: pointer to set of storage operations
 */
struct sprd_sip_svc_handle {
	struct sprd_sip_svc_perf_ops perf_ops;
	struct sprd_sip_svc_dbg_ops dbg_ops;
	struct sprd_sip_svc_storage_ops storage_ops;
	struct sprd_sip_svc_pwr_ops pwr_ops;
	struct sprd_sip_svc_dvfs_ops dvfs_ops;
};

/**
 * sprd_sip_svc_get_handle() - returns a pointer to SPRD SIP handle
 *
 * Return: a pointer to SPRD_SIP handle
 */
struct sprd_sip_svc_handle *sprd_sip_svc_get_handle(void);

#endif /* __SPRD_SIP_SVC_H__ */
