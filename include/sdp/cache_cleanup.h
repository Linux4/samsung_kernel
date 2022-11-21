/*
 * cache_cleanup.h
 *
 *  Created on: Jan 14, 2015
 *      Author: olic
 */

#ifndef PAGEMAP_H_
#define PAGEMAP_H_

#include <sdp/common.h>
#include <linux/pagemap.h>

void sdp_page_cleanup(struct page *page);

#endif /* PAGEMAP_H_ */