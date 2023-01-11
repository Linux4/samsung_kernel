/*
 *  EMMC RESERVE PAGE APIs
 *
 *  Copyright (C) 2014 Marvell International Ltd.
 *  Jialing Fu <jlfu@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */
#ifndef _EMMC_RSV_H_
#define _EMMC_RSV_H_

/*
 * How to use these APIs
 * 1. Put attention that the memory size is limit
 * 2. When you call rsv_page_update, all the rsv_page memory will be flushed
 * to eMMC block, so you need to avoid that some data is still being modified.
 */
enum rsv_page_index {
	RSV_PAGE_SDH_EMMC,
	RSV_PAGE_SDH_SD,
	RSV_PAGE_SDH_SDIO,
	RSV_PAGE_INDEX_MAX,
};

extern void *rsv_page_get_kaddr(enum rsv_page_index index, size_t want);
extern void rsv_page_update(void);

#endif /* _EMMC_RSV_H_ */
