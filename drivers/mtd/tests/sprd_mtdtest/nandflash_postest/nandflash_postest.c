#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/random.h>
#include <mach/hardware.h>
#include <linux/vmalloc.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/moduleparam.h>

static struct mtd_info *mtd = NULL;
static int dev = 4;
static int bad_pos = 0;
static int bad_len = 2;
module_param(dev, int, S_IRUGO);
module_param(bad_pos, int, S_IRUGO);
module_param(bad_len, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");
MODULE_PARM_DESC(bad_pos, "indicate the bad_flag's position");
MODULE_PARM_DESC(bad_len, "indicate the bad_flag's length");

static int erase_eraseblock(struct mtd_info *mtd, long offset)
{
	int err = 0;
	struct erase_info ei = {0};

	ei.mtd  = mtd;
	ei.addr = (offset/mtd->erasesize)*mtd->erasesize;
	ei.len  = mtd->erasesize;

	err = mtd_erase(mtd, &ei);
	if (err) {
		pr_err("err %d while erasing addr:0x%.8x\n", err, (unsigned int)offset);
		return err;
	}

	if (ei.state == MTD_ERASE_FAILED) {
		pr_err("some erase error occurred at addr: 0x%.8x\n", (unsigned int)offset);
		return -EIO;
	}

	return 0;
}

static int write_oob(loff_t offset, unsigned char *databuf, unsigned int len, unsigned char *oobbuf, unsigned int ooblen, unsigned int mode)
{
	struct mtd_oob_ops ops = {0};
	int err = 0;

	ops.mode      = mode;
	ops.len       = len;
	ops.retlen    = 0;
	ops.ooblen    = ooblen;
	ops.oobretlen = 0;
	ops.ooboffs   = 0;
	ops.datbuf    = databuf;
	ops.oobbuf    = oobbuf;
	err = mtd->_write_oob(mtd, offset, &ops);
	if (err || ops.oobretlen != ops.ooblen || ops.retlen != ops.len) {
		pr_err("write address: 0x%.8x's failed, err:%d!", (unsigned int)offset, err);
		if(err == 0) {
			pr_err("expect:%d , %d real:%d, %d\n", ops.len, ops.ooblen, ops.retlen, ops.oobretlen);
			err = -1;
		}
	}

	return err;
}

static int read_oob(loff_t offset, unsigned char *databuf, unsigned int len, unsigned char *oobbuf, unsigned int ooblen, unsigned int mode)
{
	struct mtd_oob_ops ops = {0};
	int err = 0;

	ops.mode      = mode;
	ops.len       = len;
	ops.retlen    = 0;
	ops.ooblen    = ooblen;
	ops.oobretlen = 0;
	ops.ooboffs   = 0;
	ops.datbuf    = databuf;
	ops.oobbuf    = oobbuf;
	err = mtd->_read_oob(mtd, offset, &ops);
	if (err || ops.oobretlen != ops.ooblen || ops.retlen != ops.len) {
		pr_err("read address: 0x%.8x's failed, err:%d!", (unsigned int)offset, err);
		if(err == 0) {
			pr_err("expect:%d , %d real:%d, %d\n", ops.len, ops.ooblen, ops.retlen, ops.oobretlen);
			err = -1;
		}
	}

	return err;
}

static int __init nandflash_postest_init(void)
{
	int err = 0;
	loff_t offset = 0;
	unsigned char *pagebuf_r = NULL;
	unsigned char *oobbuf_r = NULL;
	unsigned char *pagebuf_w = NULL;
	unsigned char *oobbuf_w = NULL;
	unsigned int j = 0, i = 0;
	unsigned int ret_len = 0;
	struct rnd_state rnd_state;
	unsigned int tmp_len = 0, tmp_pos = 0;

	printk("\n");
	pr_info("----------------------------------------------------------------------\n");

	/*prepare work*/
	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		pr_err("Cannot get MTD device\n");
		return err;
	}

	if(!mtd->_block_isbad || !mtd->_block_markbad || !mtd->_read_oob || !mtd->_write_oob ||
	   !mtd->_read || !mtd->_write) {
		pr_err("Some mtd's func may be NULL!");
		goto out;
	}

	oobbuf_r = vmalloc(mtd->oobsize);
	oobbuf_w = vmalloc(mtd->oobsize);
	pagebuf_r = vmalloc(mtd->writesize);
	pagebuf_w = vmalloc(mtd->writesize);
	if(!pagebuf_r || !oobbuf_r || !pagebuf_w || !oobbuf_w ) {
		pr_err("Alloc buf failed!");
		goto out;
	}

	//find a vailid block
	while(offset < mtd->size) {
		if(!(mtd->_block_isbad(mtd, offset)))
			break;
		offset += mtd->erasesize;
	}

	if(offset >= mtd->size) {
		pr_err("The whole mtd_partion are bad!\n");
		goto out;
	}

	/*step 1:bad flag test*/
	pr_info("Step 1 begin:\n");
	err = erase_eraseblock(mtd, (long)offset);
	if(err) {
		pr_err("1.1 erase failed at EB: %d\n", ((unsigned int)offset)/mtd->erasesize);
		goto out;
	}
	mtd->_block_markbad(mtd, offset);
	pr_info("Mark block 0x%.8x as bad!\n", (unsigned int)offset);

	err = read_oob(offset, NULL, 0, oobbuf_r, mtd->oobsize, MTD_OPS_RAW);
	if(err != 0) {
		pr_err("1.2 read failed!\n");
		goto erase;
	}

	for(i = bad_pos; i < (bad_pos + bad_len); i ++) {
		if(oobbuf_r[i] != 0) {
			pr_err("1.2 bad_flag pos failed, pos:%d flag:0x%.2x\n", i, oobbuf_r[i]);
			goto erase;
		}
	}

	if(mtd->_block_isbad(mtd, offset) != 1) {
		pr_err("1.3 bad_flag func failed!\n");
		goto erase;
	}

	err = erase_eraseblock(mtd, (long)offset);
	if(err) {
		pr_err("1.4 erase failed at EB: %d\n", ((unsigned int)offset)/mtd->erasesize);
		goto out;
	}
	pr_info("Bad block test successful: pos and func are all ok!\n");
	pr_info("Step 1 end successfully!\n");

	/* step 2: oob and main area position test */
	pr_info("Step 2 begin:\n");
	//prepare value
	prandom_seed_state(&rnd_state, 1);
	prandom_bytes_state(&rnd_state, oobbuf_w, mtd->oobsize);
	prandom_bytes_state(&rnd_state, pagebuf_w, mtd->writesize);

	offset += mtd->erasesize;
	err = erase_eraseblock(mtd, (long)offset);
	if(err) {
		pr_err("2.1 erase failed at EB: %d\n", ((unsigned int)offset)/mtd->erasesize);
		goto out;
	}
	pr_info("Erase block %d successfully!\n", ((unsigned int)offset)/mtd->erasesize);

	err = write_oob(offset, pagebuf_w, mtd->writesize, oobbuf_w, mtd->oobsize, MTD_OPS_RAW);
	if(err != 0) {
		pr_err("2.1 write failed!\n");
		goto erase;
	}

	err = read_oob(offset, pagebuf_r, mtd->writesize, oobbuf_r, mtd->oobsize, MTD_OPS_RAW);
	if(err != 0) {
		pr_err("2.1 read failed!\n");
		goto erase;
	}

	if(memcmp(oobbuf_w, oobbuf_r, mtd->oobsize) != 0) {
		pr_err("2.1 compare oob failed!\n");
		goto erase;
	}
	if(memcmp(pagebuf_w, pagebuf_r, mtd->writesize) != 0) {
		pr_err("2.1 compare page failed!\n");
		goto erase;
	}

	offset += mtd->writesize;
	err = write_oob(offset, pagebuf_w, mtd->writesize, oobbuf_w, mtd->oobsize, MTD_OPS_PLACE_OOB);
	if(err != 0) {
		pr_err("2.2 write failed!\n");
		goto erase;
	}

	err = read_oob(offset, pagebuf_r, mtd->writesize, oobbuf_r, mtd->oobsize, MTD_OPS_RAW);
	if(err != 0) {
		pr_err("2.2 read failed!\n");
		goto erase;
	}

	for(i = 0, j = 0; j < mtd->ecclayout->oobavail; i ++)//test spare_info pos
	{
		tmp_pos = mtd->ecclayout->oobfree[i].offset;
		tmp_len = mtd->ecclayout->oobfree[i].length;
		if(tmp_pos == 0 && tmp_len == 0) {
			pr_err("2.2 ecclayout may wrong!\n");
			goto erase;
		}
		if(memcmp(oobbuf_w + tmp_pos, oobbuf_r + tmp_pos, tmp_len) != 0) {
			pr_err("2.2 spare_info pos test failed!\n");
			goto erase;
		}
		j += tmp_len;
	}

	if(memcmp(pagebuf_r, pagebuf_w, mtd->writesize) != 0) {
		pr_err("2.2 compare page failed!\n");
		goto erase;
	}

	err = read_oob(offset, pagebuf_r, mtd->writesize, oobbuf_r, mtd->oobsize, MTD_OPS_PLACE_OOB);
	if(err != 0) {
		pr_err("2.3 read failed!\n");
		goto erase;
	}

	if(memcmp(pagebuf_w, pagebuf_r, mtd->writesize) != 0) {
		pr_err("2.3 compare page failed!\n");
		goto erase;
	}
	memcpy(oobbuf_w, oobbuf_r, mtd->oobsize);

	pr_info("Step 2 end successfully!\n");

	/*step 3: oob and main area position test with ecc*/
	pr_info("Step 3 begin:\n");
	offset += mtd->writesize;
	//write main with ecc
	err = mtd->_write(mtd, offset, mtd->writesize, &ret_len, pagebuf_w);
	if((err != 0 ) || (ret_len != mtd->writesize)) {
		pr_err("3.1 write page failed at %#llx\n", (long long)offset);
		goto erase;
	}

	//read main with ecc
	err = mtd->_read(mtd, offset, mtd->writesize, &ret_len, pagebuf_r);
	if((err != 0 ) || (ret_len != mtd->writesize)) {
		pr_err("3.1 read page failed at %#llx\n", (long long)offset);
		goto erase;
	}
	if(memcmp(pagebuf_w, pagebuf_r, mtd->writesize) != 0) {
		pr_err("3.1 compare page failed!\n");
	}
	pr_info("Step 3 end successfully!\n");
	pr_info("The position test end successfully!\n");

erase:
	err = erase_eraseblock(mtd, (long)offset);
	if(err) {
		pr_err("4.1 erase failed at EB: %d\n", ((unsigned int)offset)/mtd->erasesize);
		goto out;
	}
out:
	vfree(pagebuf_r);
	vfree(oobbuf_r);
	vfree(pagebuf_w);
	vfree(oobbuf_w);
	pr_info("----------------------------------------------------------------------\n");
	return -1;//inorder to return failed when insmod this module
}

static void __exit nandflash_postest_exit(void)
{
	put_mtd_device(mtd);
	return;
}

module_init(nandflash_postest_init);
module_exit(nandflash_postest_exit);

MODULE_DESCRIPTION("Test basic nandflash driver");
MODULE_AUTHOR("ming.tang");
MODULE_LICENSE("GPL");
