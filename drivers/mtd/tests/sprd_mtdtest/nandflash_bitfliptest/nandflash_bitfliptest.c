#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/delay.h>
#include <asm/div64.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/random.h>
#include <linux/vmalloc.h>

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

struct mtd_part {
        struct mtd_info mtd;
        struct mtd_info *master;
        loff_t offset;
        struct list_head list;
};
static int dev = 4;
static int threshold = 100;
module_param(dev, int, S_IRUGO);
module_param(threshold, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");
MODULE_PARM_DESC(threshold, "the threshold of bit-flip num");

static struct mtd_info *mtd = NULL;
static struct mtd_part *mtd_part = NULL;
static unsigned char *bbt = NULL;

static int ebcnt;
static int pgcnt;
static int errcnt;
static int rdcnt;
static int rdpgcnt;
static int pgsize;
static int bufsize;
static int *offsets;
static unsigned char *readbuf;
static unsigned char * writebuf;

static int mtd_test_erase_eraseblock(int ebnum, int blocks)
{
	int err = 0;
	struct erase_info ei = {0};

	if(blocks == 0)
		blocks = 1;

	ei.mtd  = mtd;
	ei.addr = ebnum * mtd->erasesize;
	ei.len  = mtd->erasesize * blocks;

	err = mtd_erase(mtd, &ei);
	if (err) {
		pr_err("error %d while erasing EB %d\n", err, ebnum);
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
		pr_err("Some erase error occurred at EB %d\n", ebnum);
		return -EIO;
	}

	return 0;
}

static int mtd_test_erase_whole_device(void)
{
	int err = 0;
	unsigned int i = 0;

	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = mtd_test_erase_eraseblock(i, 0);
		if (err)
			return err;
		cond_resched();
	}
	return 0;
}

static int mtd_test_scan_bad_block(void)
{
	unsigned int i = 0, bad = 0;

	bbt = (unsigned char *)vmalloc(ebcnt);
	if (!bbt) {
		pr_info("error: cannot allocate memory\n");
		return -ENOMEM;
	}

	pr_info("scanning for bad eraseblocks first\n");
	for (i = 0; i < ebcnt; ++i) {
		bbt[i] = mtd_block_isbad(mtd, i * mtd->erasesize) ? 1 : 0;
		if (bbt[i])
			bad += 1;
		cond_resched();
	}
	pr_info("scanned %d eraseblocks, %d are bad\n", ebcnt, bad);
	return 0;
}

static int bitfliptest_rand_eb(void)
{
	unsigned int eb = 0;

again:
	eb = prandom_u32();
	/* Read or write up 2 eraseblocks at a time - hence 'ebcnt - 1' */
	eb %= (ebcnt - 1);
	if (bbt[eb])
		goto again;
	return eb;
}

static int bitfliptest_rand_offs(void)
{
	unsigned int offs = 0;

	offs = prandom_u32();
	offs %= bufsize;
	return offs;
}

static int bitfliptest_rand_len(int offs)
{
	unsigned int len = 0;

	len = prandom_u32();
	len %= (bufsize - offs);
	return len;
}

static int bitfliptest_do_read(void)
{
	int err = 0;
	size_t read = 0;
	loff_t addr = 0;
	unsigned int corrected_num = 0;
	unsigned int eb = bitfliptest_rand_eb();
	unsigned int offs = bitfliptest_rand_offs();
	unsigned int len = bitfliptest_rand_len(offs);
	struct mtd_ecc_stats stats= {0};

	if (bbt[eb + 1]) {
		if (offs >= mtd->erasesize)
			offs -= mtd->erasesize;
		if (offs + len > mtd->erasesize)
			len = mtd->erasesize - offs;
	}
	addr = eb * mtd->erasesize + offs;

//	pr_info("read, addr: 0x%.8x len: %d\n", (unsigned int)addr, len);
	stats = mtd_part->master->ecc_stats;
	err = mtd_read(mtd, addr, len, &read, readbuf);
	if (mtd_is_bitflip(err))
		err = 0;
	if (unlikely(err || read != len)) {
		pr_err("error: read failed at 0x%llx, err:%d len: %dread:%d\n",
		       (long long)addr, err, len, read);
		if (!err)
			err = -EINVAL;
		return err;
	}
	corrected_num = mtd_part->master->ecc_stats.corrected - stats.corrected;
	if(corrected_num != 0) {
		errcnt += corrected_num;
		pr_info("When we read addr:0x%.8x:0x%.8x, we found %d(%d) bit-flip!\n", (unsigned int)addr, len, corrected_num, errcnt);
	}
	rdpgcnt += len/mtd->writesize;
	rdcnt ++;
	return 0;
}

static int bitfliptest_do_write(void)
{
	int err = 0;
	loff_t addr = 0;
	size_t written = 0;
	int eb = bitfliptest_rand_eb();
	unsigned int offs = 0, len = 0;

	offs = offsets[eb];
	if (offs >= mtd->erasesize) {
		err = mtd_test_erase_eraseblock(eb, 0);
		if (err)
			return err;
		offs = offsets[eb] = 0;
	}
	len = bitfliptest_rand_len(offs);
	len = ((len + pgsize - 1) / pgsize) * pgsize;
	if (offs + len > mtd->erasesize) {
		if (bbt[eb + 1])
			len = mtd->erasesize - offs;
		else {
//			pr_info("erase, block: %d\n", eb + 1);
			err = mtd_test_erase_eraseblock(eb + 1, 0);
			if (err)
				return err;
			offsets[eb + 1] = 0;
		}
	}
	addr = eb * mtd->erasesize + offs;
//	pr_info("write, addr: 0x%.8x len: %d\n", (unsigned int)addr, len);
	err = mtd_write(mtd, addr, len, &written, writebuf);
	if (unlikely(err || written != len)) {
		pr_err("error: write failed at 0x%llx\n",
		       (long long)addr);
		if (!err)
			err = -EINVAL;
		return err;
	}
	offs += len;
	while (offs > mtd->erasesize) {
		offsets[eb++] = mtd->erasesize;
		offs -= mtd->erasesize;
	}
	offsets[eb] = offs;
	return 0;
}

static int bitfliptest_do_operation(void)
{
	if (prandom_u32() & 1) {
		return bitfliptest_do_read();
	} else {
		return bitfliptest_do_write();
	}
}

static int mtd_test_bitfliptest(void)
{
	int err = 0;
	unsigned int i = 0, op = 0;

	bufsize = mtd->erasesize * 2;
	readbuf = vmalloc(bufsize);
	if(!readbuf) {
		pr_err("%s, alloc readbuf failed!\n", __func__);
		return -ENOMEM;
	}
	writebuf = vmalloc(bufsize);
	if(!writebuf) {
		pr_err("%s, alloc writebuf failed!\n", __func__);
		vfree(readbuf);
		return -ENOMEM;
	}
	offsets = vmalloc(ebcnt * sizeof(int));
	if(!offsets) {
		pr_err("%s, alloc offsets failed!\n", __func__);
		vfree(readbuf);
		vfree(writebuf);
		return -ENOMEM;
	}
	pr_info("bitfliptest begin!\n");

	for (i = 0; i < ebcnt; i++)
		offsets[i] = mtd->erasesize;
	prandom_bytes(writebuf, bufsize);

	err = mtd_test_erase_whole_device();
	if(err)
		goto out;

	while(1) {
		if ((op & 1023) == 0)
			pr_info("%d operations done\n", op);
		err = bitfliptest_do_operation();
		if (err)
			goto out;
		if(errcnt >= threshold) {
			pr_info("Test times: %d, read times: %d, read pages: %d, bit-flip: %d.\n", op, rdcnt, rdpgcnt, errcnt);
			break;
		}
		cond_resched();
		op += 1;
	}

out:
	vfree(offsets);
	vfree(readbuf);
	vfree(writebuf);
	pr_info("bitfliptest finished, %d operations done!\n", op);
	return err;
}

static int __init nandflash_bitfliptest_init(void)
{
	int err = 0;
	uint64_t tmp = 0;

	pr_info(KERN_INFO "\n");
	pr_info(KERN_INFO "=================================================\n");

	if (dev < 0) {
		pr_info("Please specify a valid mtd-device via module parameter\n");
		return -EINVAL;
	}

	pr_info("MTD device: %d\n", dev);

	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		pr_info("error: cannot get MTD device\n");
		return err;
	}
	mtd_part = (struct mtd_part *)mtd;

	if (mtd->type != MTD_NANDFLASH) {
		pr_info("this test requires NAND flash\n");
		goto out;
	}

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / mtd->writesize;
	pgsize = mtd->writesize;

	pr_info("MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n",
	       (unsigned long long)mtd->size, mtd->erasesize,
	       mtd->writesize, ebcnt, pgcnt, mtd->oobsize);

	err = mtd_test_scan_bad_block();
	if (err)
		goto out;

	err = mtd_test_bitfliptest();
	if(err) {
		pr_err("bitfliptest failed!\n");
		goto out;
	}

out:
	pr_info(KERN_INFO "=================================================\n");
	if(bbt != NULL)
		vfree(bbt);
	put_mtd_device(mtd);

	return -1;
}

static void __exit nandflash_bitfliptest_exit(void)
{
	return;
}

module_init(nandflash_bitfliptest_init);
module_exit(nandflash_bitfliptest_exit);

MODULE_DESCRIPTION("mtd_bitflip_test");
MODULE_AUTHOR("ming.tang@spreadtrum.com");
MODULE_LICENSE("GPL");
