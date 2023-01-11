/*
 *  include/linux/mmc/pxa_dvfs.h
 *
 *  Copyright 2014 Jialing Fu jlfu@marvell.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#ifndef _LINUX_MMC_PXA_DVFS_H
#define _LINUX_MMC_PXA_DVFS_H
#include <linux/mmc/sdhci.h>

extern int pxa_sdh_create_dvfs(struct sdhci_host *host);
extern int pxa_sdh_get_highest_dvfs_level(struct sdhci_host *host);
extern int pxa_sdh_get_lowest_dvfs_level(struct sdhci_host *host);
extern int pxa_sdh_request_dvfs_level(struct sdhci_host *host, int level);
extern int pxa_sdh_release_dvfs(struct sdhci_host *host);
extern int pxa_sdh_enable_dvfs(struct sdhci_host *host);
extern int pxa_sdh_disable_dvfs(struct sdhci_host *host);
extern int pxa_sdh_destory_dvfs(struct sdhci_host *host);
extern int pxa_sdh_dvfs_timing_prepare(struct sdhci_host *host, unsigned char timing);

#endif /* _LINUX_MMC_PXA_DVFS_H */
