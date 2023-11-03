/*
 * mm.h
 *
 *  Created on: Aug 20, 2014
 *      Author: olic
 */

#ifndef ECRYPTFS_MM_H_
#define ECRYPTFS_MM_H_

void ecryptfs_mm_drop_cache(int userid, int engineid);
void ecryptfs_mm_do_sdp_cleanup(struct inode *inode);

void page_dump (struct page *p);

#endif /* ECRYPTFS_MM_H_ */
