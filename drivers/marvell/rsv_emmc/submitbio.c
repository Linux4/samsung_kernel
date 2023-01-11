/*
 * Copyright (C) 2013~2014 Marvell International Ltd.
 *		Jialing Fu <jlfu@marvell.com>
 *
 * Try to create 2 APIs to let module/driver can read/write
 * eMMC directly and by-pass the filesystem in kernel
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG

#include <linux/bio.h>
#include <linux/kernel.h>
#include <linux/pagemap.h>
#include<linux/blkdev.h>
#include <linux/device.h>

/* >>>>> some basic code to read/write eMMC <<<<< */

void end_bio_read(struct bio *bio, int err)
{
	struct completion *done;

	done = bio->bi_private;
	if (done)
		complete(done);

	pr_debug("bio 0x%p done\n", bio);

	bio_put(bio);
}

/*
 * submit - submit BIO request.
 * @rw:	READ or WRITE.
 * @bdev: the targed block device
 * @blk_sector: sector offset of block device, by Sector/512Byte.
 * @mem_page:	the page struct of mem buffer.
 * @mem_offset:	the Byte offset within memory
 * @len: data length, by Byte
 *
 * Note:
 * the address within block device should be sector align
 * the address within memory can be Byte align
 * the size can be Byte align, but it is better to 512Byte align
 */
static int submit(int rw, struct block_device *bdev, sector_t blk_sector,
	struct page *mem_page, unsigned int mem_offset, size_t len)
{
	struct bio *bio;
	struct completion done;

	/*
	 * submit_bio only send out the request to block device, but the data
	 * may not be handled in block driver before submit_bio returns.
	 * So if we want to make sure the data is updated, we add completion
	 * to support it
	 */
	if (rw & REQ_SYNC)
		init_completion(&done);

	bio = bio_alloc(__GFP_WAIT | __GFP_HIGH, 1);
	bio->bi_iter.bi_sector = blk_sector;
	bio->bi_bdev = bdev;
	bio->bi_end_io = end_bio_read;
	if (rw & REQ_SYNC)
		bio->bi_private = &done;
	else
		bio->bi_private = NULL;

	if (bio_add_page(bio, mem_page, len, mem_offset) < len) {
		pr_err("ERR bio_add_page:page:0x%p offset:0x%x len:0x%x\n",
			mem_page, mem_offset, (unsigned int)len);
		bio_put(bio);
		return -EFAULT;
	}

	/* bio_get here, make use end_bio_read not to release the bio */
	bio_get(bio);
	submit_bio(rw, bio);
	if (rw & REQ_SYNC)
		wait_for_completion(&done);
	bio_put(bio);

	return 0;
}

int read_bio_sectors(struct block_device *bdev, sector_t sector,
	void *kaddr, size_t size)
{
	struct page *page;
	unsigned int offset;

	/* for block device, it need to read by sectors */
	WARN_ON(size & 511);

	page = virt_to_page(kaddr);
	offset = kaddr - page_address(page);
	return submit(READ | REQ_SYNC, bdev, sector, page, offset, size);
}

int write_bio_sectors(struct block_device *bdev, sector_t sector,
	void *kaddr, size_t size)
{
	struct page *page;
	unsigned int offset;

	/*  for block device, it need to read by sectors */
	WARN_ON(size & 511);

	page = virt_to_page(kaddr);
	offset = kaddr - page_address(page);
	return submit(WRITE | REQ_SYNC, bdev, sector, page, offset, size);
}
/*<<<<< End of basic code to read/write eMMC >>>>> */
