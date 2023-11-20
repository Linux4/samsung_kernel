#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/random.h>
#include <linux/mtd/mtd.h>
#include <linux/moduleparam.h>

struct mtd_part {
        struct mtd_info mtd;
        struct mtd_info *master;
        loff_t offset;
        struct list_head list;
};

#define CBIT(v, n) ((v) & (1 << (n)))
#define BCLR(v, n) ((v) = (v) & ~(1 << (n)))

static struct mtd_info *mtd = NULL;
static int dev = 4;
module_param(dev, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");

static int insert_biterror(unsigned char *buf)
{
	unsigned int bit = 0;
	unsigned int byte = 0;

	while (byte < mtd->writesize) {
		for (bit = 0; bit <= 7; bit++) {
			if (CBIT(buf[byte], bit)) {
				BCLR(buf[byte], bit);
				pr_info("Inserted biterror @ %u/%u\n", byte, bit);
				return 0;
			}
		}
		byte++;
	}
	pr_err("biterror: Failed to find a '1' bit\n");
	return -EIO;
}

static int erase_eraseblock(unsigned int ebnum)
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
                pr_err("read address: 0x%.8x failed, err:%d!\n", (unsigned int)offset, err);
                if(err == 0) {
                        pr_err("expect:%d , %d real:%d, %d\n", ops.len, ops.ooblen, ops.retlen, ops.oobretlen);
                        err = -1;
                } else if(err == -EUCLEAN) {
			err = 0;
		}
        }

        return err;
}

static int __init nandflash_ecctest_init(void)
{
	unsigned int read_len = 0;
	int err = 0;
	loff_t offset = 0;
	unsigned int j = 0;
	struct rnd_state rnd_state;
	unsigned int corrected_num = 0;
	unsigned char *oobbuf_w = NULL;
	unsigned char *oobbuf_r = NULL;
	unsigned char *pagebuf_r = NULL;
	unsigned char *pagebuf_w = NULL;
	unsigned char *pagebuf_v = NULL;
	struct mtd_ecc_stats stats = {0};
	struct mtd_part *mtd_part = NULL;

	printk("\n");
	pr_info("--------------------------------------------------------------------------\n");
	/*prepare work*/
	mtd = get_mtd_device(NULL, 4);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		pr_err("Cannot get MTD device\n");
		return err;
	}

	mtd_part = (struct mtd_part *)mtd;

	if(!mtd->_block_isbad || !mtd->_block_markbad || !mtd->_read_oob || !mtd->_write_oob ||
	   !mtd->_read || !mtd->_write) {
		pr_err("Some mtd's method may be NULL!");
		goto out;
	}

	pagebuf_r = vmalloc(mtd->writesize);
	oobbuf_r = vmalloc(mtd->oobsize);
	pagebuf_w = vmalloc(mtd->writesize);
	pagebuf_v = vmalloc(mtd->writesize);
	oobbuf_w = vmalloc(mtd->oobsize);
	if(!pagebuf_r || !oobbuf_r || !pagebuf_w || !pagebuf_v || !oobbuf_w) {
		pr_err("Alloc buf failed!");
		goto out;
	}

	//find a vailid block
	while(offset < mtd->size) {
		if(mtd->_block_isbad(mtd, offset)) {
			offset += mtd->erasesize;
		} else {
			break;
		}
	}

	if(offset >= mtd->size) {
		pr_err("The whole mtd_partion are bad!\n");
		goto out;
	}

	err = erase_eraseblock(((long)offset)/mtd->erasesize);
	if(err) {
		pr_err("Erase failed at EB: %d\n", ((unsigned int)offset)/mtd->erasesize);
		goto out;
	}
	pr_info("Erase block %d successfully!\n", ((unsigned int)offset)/mtd->erasesize);

	//prepare value
	prandom_seed_state(&rnd_state, 1);
	prandom_bytes_state(&rnd_state, oobbuf_w, mtd->oobsize);
	prandom_bytes_state(&rnd_state, pagebuf_w, mtd->writesize);
	memcpy(pagebuf_v, pagebuf_w, mtd->writesize);

	err = write_oob(offset, pagebuf_w, mtd->writesize, oobbuf_w, mtd->oobsize, MTD_OPS_PLACE_OOB);
	if(err != 0) {
		pr_err("1.1 write failed!\n");
		goto erase;
	}

	err = read_oob(offset, pagebuf_r, mtd->writesize, oobbuf_r, mtd->oobsize, MTD_OPS_PLACE_OOB);
	if(err != 0) {
		pr_err("1.1 read failed!\n");
		goto erase;
	}

	if(memcmp(pagebuf_v, pagebuf_r, mtd->writesize) != 0) {
		pr_err("1.1 compare page failed!\n");
		goto erase;
	}
	memcpy(oobbuf_w, oobbuf_r, mtd->oobsize);

	for(j = 0; j < mtd->writesize * 8; j ++)
	{
		offset += mtd->writesize;
		err = insert_biterror(pagebuf_w);
		if(err) {
			pr_err("Insert biterror failed!\n");
			goto erase;
		}
		pr_info("Insert %d bit-flip sucessfully!\n", j + 1);

		err = write_oob(offset, pagebuf_w, mtd->writesize, oobbuf_w, mtd->oobsize, MTD_OPS_RAW);
		if(err != 0) {
			pr_err("2.1 write failed!\n");
			goto erase;
		}

		stats = mtd_part->master->ecc_stats;
		err = read_oob(offset, pagebuf_r, mtd->writesize, oobbuf_r, mtd->oobsize, MTD_OPS_PLACE_OOB);
		if(err != 0) {
			pr_err("2.1 read failed!\n");
			break;
		}

		if(memcmp(pagebuf_v, pagebuf_r, mtd->writesize) != 0) {
			pr_err("2.2 compare page failed!\n");
			goto erase;
		}

		corrected_num = mtd_part->master->ecc_stats.corrected - stats.corrected;
		if(corrected_num != (j + 1)) {
			pr_err("Bit-flip num is not match, expected:%d, really:%d\n", j + 1, corrected_num);
			goto erase;
		}
	}

	pr_info("Test successfully, this system can corrected %d bit-flip at most!\n", j);
erase:
	err = erase_eraseblock(((unsigned int)offset)/mtd->erasesize);
	if(err) {
		pr_err("1.3 erase failed at EB: %d\n", ((unsigned int)offset)/mtd->erasesize);
		goto out;
	}
out:
	vfree(pagebuf_r);
	vfree(oobbuf_r);
	vfree(pagebuf_w);
	vfree(oobbuf_w);
	pr_info("--------------------------------------------------------------------------\n");
	return -1;
}

static void __exit nandflash_ecctest_exit(void)
{
	put_mtd_device(mtd);
	return;
}

module_init(nandflash_ecctest_init);
module_exit(nandflash_ecctest_exit);

MODULE_DESCRIPTION("Test ecc strength");
MODULE_AUTHOR("ming.tang");
MODULE_LICENSE("GPL");
