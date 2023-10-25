/*
 * Exynos FMP MMC crypto interface
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _MMC_EXYNOS_FMP_H_
#define _MMC_EXYNOS_FMP_H_

#ifdef CONFIG_MMC_DW_EXYNOS_FMP
int fmp_mmc_crypt_cfg(struct bio *bio,
			void *desc,
			struct mmc_data *data,
			int page_index,
			bool cmdq_enabled);
int fmp_mmc_crypt_clear(struct bio *bio,
			void *desc,
			struct mmc_data *data,
			bool cmdq_enabled);
int fmp_mmc_init_crypt(struct mmc_host *mmc);
#endif /* CONFIG_MMC_DW_EXYNOS_FMP */
#endif /* _MMC_EXYNOS_FMP_H_ */
