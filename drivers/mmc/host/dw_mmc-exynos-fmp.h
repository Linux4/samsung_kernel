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

#define FMP_DRV_VERSION "3.1.0"

/* FMP IDs (second or third parameter to FMP/SMU Ctrls) */
#define FMP_EMBEDDED			0
#define FMP_UFSCARD			1
#define FMP_SDCARD			2

/* SMU commands (second parameter to SMC_CMD_SMU) */
#define SMU_INIT			0
#define SMU_SET				1
#define SMU_ABORT			2

/* Fourth parameter to SMC_CMD_FMP_SECURITY */
#define CFG_DESCTYPE_0			0
#define CFG_DESCTYPE_3			3

#define FMP_DATA_UNIT_SIZE 4096

enum fmp_crypto_algo_mode {
	EXYNOS_FMP_BYPASS_MODE = 0,
	EXYNOS_FMP_ALGO_MODE_AES_CBC = 1,
	EXYNOS_FMP_ALGO_MODE_AES_XTS = 2,
};

enum fmp_crypto_enc_mode {
	EXYNOS_FMP_FILE_ENC = 0,
	EXYNOS_FMP_ENC_MAX
};

struct fmp_table_setting {
        __le32 des0;            /* des0 */
        __le32 des1;            /* des1 */
        __le32 des2;            /* des2 */
#define FKL BIT(26)
#define SET_KEYLEN(d, v) ((d)->des2 |= (uint32_t)v)
#define SET_FAS(d, v) \
                        ((d)->des2 = ((d)->des2 & 0xcfffffff) | v << 28)
        __le32 des3;            /* des3 */
        /* CMDQ Operation */
#define FKL_CMDQ BIT(0)
#define SET_CMDQ_KEYLEN(d, v) ((d)->des3 |= (uint32_t)v)
#define SET_CMDQ_FAS(d, v) \
                        ((d)->des3 = ((d)->des3 & 0xfffffff3) | v << 2)
        __le32 reserved0;       /* des4 */
        __le32 reserved1;
        __le32 reserved2;
        __le32 reserved3;
        __le32 file_iv0;        /* des8 */
        __le32 file_iv1;
        __le32 file_iv2;
        __le32 file_iv3;
        __le32 file_enckey0;    /* des12 */
        __le32 file_enckey1;
        __le32 file_enckey2;
        __le32 file_enckey3;
        __le32 file_enckey4;
        __le32 file_enckey5;
        __le32 file_enckey6;
        __le32 file_enckey7;
        __le32 file_twkey0;     /* des20 */
        __le32 file_twkey1;
        __le32 file_twkey2;
        __le32 file_twkey3;
        __le32 file_twkey4;
        __le32 file_twkey5;
        __le32 file_twkey6;
        __le32 file_twkey7;
        __le32 disk_iv0;        /* des28 */
        __le32 disk_iv1;
        __le32 disk_iv2;
        __le32 disk_iv3;
};

int fmp_mmc_crypt_cfg(	struct bio *bio, void *desc,
			struct mmc_data *data,
			int page_index,
			bool cmdq_enabled);
int fmp_mmc_crypt_clear(struct bio *bio, void *desc,
			struct mmc_data *data,
			bool cmdq_enabled);
int fmp_mmc_init_crypt(struct mmc_host *mmc);
int fmp_mmc_sec_cfg(bool init);

#endif /* _MMC_EXYNOS_FMP_H_ */
