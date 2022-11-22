#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/delay.h>
#include <asm/div64.h>
#include <linux/err.h>
#include <linux/random.h>
#include <linux/vmalloc.h>

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int dev = 4;
module_param(dev, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");

static int ebcnt = 0;
static int pgcnt = 0;
static unsigned char *bbt = NULL;
static struct mtd_info *mtd = NULL;
static struct rnd_state rnd_state = {0};

static int agetest_erase_eraseblock(unsigned int ebnum)
{
	int err = 0;
	struct erase_info ei = {0};

	ei.mtd  = mtd;
	ei.addr = ebnum * mtd->erasesize;
	ei.len  = mtd->erasesize;

	err = mtd_erase(mtd, &ei);
	if (err) {
		pr_err("err %d while erasing EB %d\n", err, ebnum);
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
		pr_err("some erase error occurred at EB %d\n", ebnum);
		return -EIO;
	}

	return 0;
}

static int agetest_scan_bad_block(void)
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

static int nandflash_agetest(void)
{
	int err = 0;
	loff_t addr = 0;
	unsigned int test_blk = 10;
	unsigned int op = 0;
	unsigned char *readbuf = NULL;
	unsigned char *writebuf = NULL;
	unsigned int written = 0, read = 0;
	unsigned int continue_err = 0;
	unsigned int erase_time = 0;

	readbuf = vmalloc(mtd->writesize);
	if(!readbuf) {
		pr_err("%s, alloc readbuf failed!\n", __func__);
		err = -ENOMEM;
		goto out;
	}

	writebuf = vmalloc(mtd->writesize);
	if(!writebuf) {
		pr_err("%s, alloc writebuf failed!\n", __func__);
		err = -ENOMEM;
		goto out;
	}

	while((test_blk < ebcnt) && bbt[test_blk]) {
		test_blk ++;
	}

	if(test_blk >= ebcnt) {
		pr_err("The whole mtd_partion are bad!\n");
		err = -EINVAL;
		goto out;
	}

	pr_info("Begin to make block %d as bad!\n", test_blk);
	addr = test_blk * mtd->erasesize;
	prandom_seed_state(&rnd_state, 1);
	prandom_bytes_state(&rnd_state, writebuf, mtd->writesize);
	err = agetest_erase_eraseblock(test_blk);
	if(err) {
		pr_err("the first erase failed at addr: 0x%.8x, err: %d\n", (unsigned int)addr, err);
		goto out;
	}

	while(1) {
		if(signal_pending(current))
			goto out;
		err = mtd_write(mtd, addr, mtd->writesize, &written, writebuf);
		if(err || written != mtd->writesize) {
			pr_err("write failed at addr: 0x%.8x, err: %d, written: %d, time: %d\n", (unsigned int)addr, err, written, op);
			goto erase;
		}

		err = mtd_read(mtd, addr, mtd->writesize, &read, readbuf);
		if(err || read != mtd->writesize) {
			pr_err("read failed at addr: 0x%.8x, err: %d, read: %d, time: %d\n", (unsigned int)addr, err, read, op);
			goto erase;
		}

		if(memcmp(readbuf, writebuf, mtd->writesize) != 0) {
			pr_err("verify failed at addr: 0x%.8x!", (unsigned int)addr);
			goto erase;
		}

erase:
		err = agetest_erase_eraseblock(test_blk);
		if(err) {
			pr_err("erase failed at addr: 0x%.8x, err: %d, time: %d\n", (unsigned int)addr, err, op);
			break;
		}
		op += 1;
		if(op % 1024 == 0)
			pr_info("we have erased %d times!\n", op);
	}

/*make sure the block has been erased as bad block*/
	while(1) {
		if(signal_pending(current))
			break;
		err = mtd_write(mtd, addr, mtd->writesize, &written, writebuf);
		err = agetest_erase_eraseblock(test_blk);
		if(err) {
			continue_err ++;
			if(continue_err >= 0x1000) {
				pr_info("after %d times continue erase, the block has been erased as bad block!", erase_time);
				break;
			}
			if( continue_err % 100 == 0)
				pr_info("the continue_err is %d!", continue_err);
		} else {
			if(continue_err)
				pr_info("the continue_err may be clean, old value is %d\n", continue_err);
			continue_err = 0;
		}
		erase_time ++;
		if( erase_time % 1024 == 0)
				pr_info("the erase_time is %d!", erase_time);
	}
	pr_info("mark block %d as bad!\n", test_blk);
	mtd->_block_markbad(mtd, addr);
	err = 0;

out:
	vfree(readbuf);
	vfree(writebuf);
	pr_info("agetest finished, %d(%d) operations done!\n", op, erase_time);
	return err;
}

static int __init nandflash_agetest_init(void)
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
	if (mtd->type != MTD_NANDFLASH) {
		pr_info("this test requires NAND flash\n");
		goto out;
	}

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / mtd->writesize;

	pr_info("MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n",
	       (unsigned long long)mtd->size, mtd->erasesize,
	       mtd->writesize, ebcnt, pgcnt, mtd->oobsize);

	err = agetest_scan_bad_block();
	if (err)
		goto out;

	err = nandflash_agetest();
	if(err) {
		pr_err("stresstest failed!\n");
		goto out;
	}

out:
	pr_info(KERN_INFO "=================================================\n");
	if(bbt != NULL)
		vfree(bbt);
	put_mtd_device(mtd);

	return -1;
}

static void __exit nandflash_agetest_exit(void)
{
	return;
}

module_init(nandflash_agetest_init);

module_exit(nandflash_agetest_exit);

MODULE_DESCRIPTION("mtd_age_test");
MODULE_AUTHOR("ming.tang@spreadtrum.com");
MODULE_LICENSE("GPL");
