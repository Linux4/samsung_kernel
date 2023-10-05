/*
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _EXYNOS_FMP_FIPS_H_
#define _EXYNOS_FMP_FIPS_H_

#include <linux/bio.h>

struct fmp_table_setting {
	__le32 des0;			/* des0 */
	__le32 des1;			/* des1 */
	__le32 des2;			/* des2 */
	__le32 des3;			/* des3 */
	__le32 file_iv0;		/* des4 */
	__le32 file_iv1;		/* des5 */
	__le32 file_iv2;		/* des6 */
	__le32 file_iv3;		/* des7 */
	__le32 file_enckey0;	/* des8 */
	__le32 file_enckey1;	/* des9 */
	__le32 file_enckey2;	/* des10 */
	__le32 file_enckey3;	/* des11 */
	__le32 file_enckey4;	/* des12 */
	__le32 file_enckey5;	/* des13 */
	__le32 file_enckey6;	/* des14 */
	__le32 file_enckey7;	/* des15 */
	__le32 file_twkey0;	/* des16 */
	__le32 file_twkey1;	/* des17 */
	__le32 file_twkey2;	/* des18 */
	__le32 file_twkey3;	/* des19 */
	__le32 file_twkey4;	/* des20 */
	__le32 file_twkey5;	/* des21 */
	__le32 file_twkey6;	/* des22 */
	__le32 file_twkey7;	/* des23 */
	__le32 disk_iv0;		/* des24 */
	__le32 disk_iv1;		/* des25 */
	__le32 disk_iv2;		/* des26 */
	__le32 disk_iv3;		/* des27 */
	__le32 reserved0;		/* des28 */
	__le32 reserved1;		/* des29 */
	__le32 reserved2;		/* des30 */
	__le32 reserved3;		/* des31 */
};

struct exynos_fmp_crypt_info {
	bool fips;
	u64 dun_hi;
	u64 dun_lo;
	const u8 *enckey;
	const u8 *twkey;
};

bool is_fmp_fips_op(struct bio *bio);
bool is_fmp_fips_clean(struct bio *bio);
int exynos_fmp_crypt(struct exynos_fmp_crypt_info *fmp_ci, struct fmp_table_setting *table);
int exynos_fmp_clear(struct fmp_table_setting *table);
#endif /* _EXYNOS_FMP_FIPS_H_ */
