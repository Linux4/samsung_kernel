/*
 *  RSV PAGE private h file
 *
 *  Copyright (C) 2014 Marvell International Ltd.
 *  Jialing Fu <jlfu@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#ifndef _RSV_PAGE_H_
#define _RSV_PAGE_H_

extern int read_bio_sectors(struct block_device *bdev, sector_t sector,
	void *kaddr, size_t size);
extern int write_bio_sectors(struct block_device *bdev, sector_t sector,
	void *kaddr, size_t size);

#define RSV_PAGE_SIZE_SDH	128
#define RSV_PAGE_SIZE 4096
struct mrvl_rsv_page {
	int magic1;
	char sdh_emmc[RSV_PAGE_SIZE_SDH];
	char sdh_sd[RSV_PAGE_SIZE_SDH];
	char sdh_sdio[RSV_PAGE_SIZE_SDH];
	int magic2;
} __aligned(RSV_PAGE_SIZE);

#endif /* _RSV_PAGE_H_ */
