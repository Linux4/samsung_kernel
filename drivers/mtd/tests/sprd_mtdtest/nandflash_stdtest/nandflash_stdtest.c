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

static int dev = 4;
module_param(dev, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");

static struct mtd_info *mtd = NULL;
static unsigned char *bbt = NULL;

static int ebcnt;
static int pgcnt;
static int errcnt;
static int pgsize;
static int subpgsize;
static int use_offset;
static int use_len;
static int use_len_max;
static int vary_offset;
static struct rnd_state rnd_state;
static struct timeval start, finish;
static int goodebcnt;
static int subsize;
static int subcount;
static int bufsize;
static unsigned char * writebuf;
static unsigned char *readbuf;
static int *offsets;
static unsigned char *twopages;
static unsigned char *boundary;

static inline void mtd_test_start_timing(void)
{
	do_gettimeofday(&start);
}

static inline void mtd_test_stop_timing(void)
{
	do_gettimeofday(&finish);
}

void mtd_test_dump_buf(unsigned char *buf, unsigned int num, unsigned int col_num)
{
        unsigned int i = 0, j = 0;

        if(col_num == 0)
                col_num = 32;

        for(i = 0; i < num; i ++)
        {
                if((i%col_num) == 0) {
                        if(i != 0)
                                pr_info("\n");
                        pr_info("%.8x:", j);
                        j += col_num;
                }
                if((i%4) == 0)
                        pr_info(" ");
                pr_info("%.2x", buf[i]);
        }
        pr_info("\n");
}

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
	goodebcnt = ebcnt - bad;
	return 0;
}

static int readtest_read_page(int ebnum, unsigned char *databuf, unsigned char *oobbuf)
{
        int i = 0, ret = 0;
        unsigned int read = 0;
        struct mtd_oob_ops ops = {0};
        loff_t addr = ebnum * mtd->erasesize;

        memset(databuf, 0 , mtd->erasesize);
        memset(oobbuf, 0 , mtd->erasesize);
        for (i = 0; i < pgcnt; i++) {
                ret = mtd_read(mtd, addr, pgsize, &read, databuf);
                if (ret || read != pgsize) {
                        pr_err("error: read failed at %#llx, ret: %d read: %d\n",
                               (long long)addr, ret, read);
			return ret ? ret : -EINVAL;
                }

                ops.mode      = MTD_OPS_PLACE_OOB;
                ops.len       = 0;
                ops.retlen    = 0;
                ops.ooblen    = mtd->oobsize;
                ops.oobretlen = 0;
                ops.ooboffs   = 0;
                ops.datbuf    = NULL;
                ops.oobbuf    = oobbuf;
                ret = mtd_read_oob(mtd, addr, &ops);
                if (ret || ops.oobretlen != mtd->oobsize) {
                        pr_err("error: read oob failed at "
                                          "%#llx, ret: %d oobretlen: %d\n", (long long)addr, ret, ops.oobretlen);
			return ret ? ret : -EINVAL;
                }
                databuf += mtd->writesize;
                oobbuf += mtd->oobsize;
                addr += mtd->writesize;
        }

        return ret;
}

static int mtd_test_readtest(void)
{
	int ret = 0;
	unsigned int i = 0;
	unsigned char *iobuf = NULL, *iobuf1 = NULL;

        iobuf = (unsigned char *)kmalloc(mtd->erasesize, GFP_KERNEL);
        iobuf1 = (unsigned char *)kmalloc(mtd->erasesize, GFP_KERNEL);
        if (!iobuf1 || !iobuf) {
                pr_err("error: cannot allocate enough memory, %d, 0x%.8x, 0x%.8x!\n", mtd->erasesize, (unsigned int)iobuf, (unsigned int)iobuf1);
                goto out;
        }

        pr_info("readtest begin!\n");
        for (i = 0; i < ebcnt; ++i) {

                if (bbt[i])
                        continue;
                ret = readtest_read_page(i, iobuf, iobuf1);
                if (ret) {
			pr_err("read block %d failed!\n", i);
                        mtd_test_dump_buf(iobuf, mtd->erasesize, 32);
                        mtd_test_dump_buf(iobuf1, mtd->oobsize, 32);
			goto out;
                }
                cond_resched();
        }

        pr_info("readtest finished successfully!\n");
out:
	kfree(iobuf);
	kfree(iobuf1);
	return ret;
}

static int pagetest_write_eraseblock(int ebnum)
{
	int err = 0;
	size_t written = 0;
	loff_t addr = ebnum * mtd->erasesize;

	prandom_bytes_state(&rnd_state, writebuf, mtd->erasesize);
	cond_resched();
	err = mtd_write(mtd, addr, mtd->erasesize, &written, writebuf);
	if (err || written != mtd->erasesize) {
		pr_err("%s, write failed at %#llx\n", __func__, (long long)addr);
		return err ? err : -EINVAL;
	}

	return err;
}

static int pagetest_verify_eraseblock(int ebnum)
{
	int err = 0;
	size_t read = 0;
	uint32_t j = 0, i = 0;
	loff_t addr0 = 0, addrn = 0;
	loff_t addr = ebnum * mtd->erasesize;

	for (i = 0; i < ebcnt && bbt[i]; ++i)
		addr0 += mtd->erasesize;

	addrn = mtd->size;
	for (i = 0; i < ebcnt && bbt[ebcnt - i - 1]; ++i)
		addrn -= mtd->erasesize;

	prandom_bytes_state(&rnd_state, writebuf, mtd->erasesize);
	for (j = 0; j < pgcnt - 1; ++j, addr += pgsize) {
		/* Do a read to set the internal dataRAMs to different data */
		err = mtd_read(mtd, addr0, bufsize, &read, twopages);
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != bufsize) {
			pr_err("2.1 error: read failed at %#llx\n",
			       (long long)addr0);
			return err;
		}
		err = mtd_read(mtd, addrn - bufsize, bufsize, &read, twopages);
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != bufsize) {
			pr_err("2.2 error: read failed at %#llx\n",
			       (long long)(addrn - bufsize));
			return err;
		}
		memset(twopages, 0, bufsize);
		err = mtd_read(mtd, addr, bufsize, &read, twopages);
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != bufsize) {
			pr_err("2.3 error: read failed at %#llx\n",
			       (long long)addr);
			break;
		}
		if (memcmp(twopages, writebuf + (j * pgsize), bufsize)) {
			pr_err("2.4 error: verify failed at %#llx\n",
			       (long long)addr);
		}
	}
	/* Check boundary between eraseblocks */
	if (addr <= addrn - pgsize - pgsize && !bbt[ebnum + 1]) {
		struct rnd_state old_state = rnd_state;

		/* Do a read to set the internal dataRAMs to different data */
		err = mtd_read(mtd, addr0, bufsize, &read, twopages);
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != bufsize) {
			pr_err("2.5 error: read failed at %#llx\n",
			       (long long)addr0);
			return err;
		}
		err = mtd_read(mtd, addrn - bufsize, bufsize, &read, twopages);
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != bufsize) {
			pr_err("2.6 error: read failed at %#llx\n",
			       (long long)(addrn - bufsize));
			return err;
		}
		memset(twopages, 0, bufsize);
		err = mtd_read(mtd, addr, bufsize, &read, twopages);
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != bufsize) {
			pr_err("2.7 error: read failed at %#llx\n",
			       (long long)addr);
			return err;
		}
		memcpy(boundary, writebuf + mtd->erasesize - pgsize, pgsize);
		prandom_bytes_state(&rnd_state, boundary + pgsize, pgsize);
		if (memcmp(twopages, boundary, bufsize)) {
			pr_err("2.8 error: verify failed at %#llx\n",
			       (long long)addr);
		}
		rnd_state = old_state;
	}
	return err;
}

static int pagetest_crosstest(void)
{
	int err = 0;
	size_t read = 0;
	unsigned int i = 0;
	loff_t addr = 0, addr0 = 0, addrn = 0;
	unsigned char *pp1 = NULL, *pp2 = NULL, *pp3 = NULL, *pp4 = NULL;

//	pr_info("crosstest\n");
	pp1 = kmalloc(pgsize * 4, GFP_KERNEL);
	if (!pp1) {
		pr_err("2.9 error: cannot allocate memory\n");
		return -ENOMEM;
	}
	pp2 = pp1 + pgsize;
	pp3 = pp2 + pgsize;
	pp4 = pp3 + pgsize;
	memset(pp1, 0, pgsize * 4);

	addr0 = 0;
	for (i = 0; i < ebcnt && bbt[i]; ++i)
		addr0 += mtd->erasesize;

	addrn = mtd->size;
	for (i = 0; i < ebcnt && bbt[ebcnt - i - 1]; ++i)
		addrn -= mtd->erasesize;

	/* Read 2nd-to-last page to pp1 */
	addr = addrn - pgsize - pgsize;
	err = mtd_read(mtd, addr, pgsize, &read, pp1);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.10 error: read failed at %#llx\n",
		       (long long)addr);
		kfree(pp1);
		return err;
	}

	/* Read 3rd-to-last page to pp1 */
	addr = addrn - pgsize - pgsize - pgsize;
	err = mtd_read(mtd, addr, pgsize, &read, pp1);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.11 error: read failed at %#llx\n",
		       (long long)addr);
		kfree(pp1);
		return err;
	}

	/* Read first page to pp2 */
	addr = addr0;
//	pr_info("reading page at %#llx\n", (long long)addr);
	err = mtd_read(mtd, addr, pgsize, &read, pp2);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.12 error: read failed at %#llx\n",
		       (long long)addr);
		kfree(pp1);
		return err;
	}

	/* Read last page to pp3 */
	addr = addrn - pgsize;
//	pr_info("reading page at %#llx\n", (long long)addr);
	err = mtd_read(mtd, addr, pgsize, &read, pp3);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.13 error: read failed at %#llx\n",
		       (long long)addr);
		kfree(pp1);
		return err;
	}

	/* Read first page again to pp4 */
	addr = addr0;
//	pr_info("reading page at %#llx\n", (long long)addr);
	err = mtd_read(mtd, addr, pgsize, &read, pp4);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.14 error: read failed at %#llx\n",
		       (long long)addr);
		kfree(pp1);
		return err;
	}

	/* pp2 and pp4 should be the same */
//	pr_info("verifying pages read at %#llx match\n", (long long)addr0);
	if (memcmp(pp2, pp4, pgsize)) {
		pr_err("2.15 verify failed!\n");
	}// else if (!err)
//		pr_info("crosstest ok\n");
	kfree(pp1);
	return err;
}

static int pagetest_erasecrosstest(void)
{
	int err = 0;
	loff_t addr0 = 0;
	char *readbuf = twopages;
	size_t read = 0, written = 0;
	unsigned int i = 0, ebnum = 0, ebnum2 = 0;

//	pr_info("erasecrosstest\n");

	ebnum = 0;
	addr0 = 0;
	for (i = 0; i < ebcnt && bbt[i]; ++i) {
		addr0 += mtd->erasesize;
		ebnum += 1;
	}

	ebnum2 = ebcnt - 1;
	while (ebnum2 && bbt[ebnum2])
		ebnum2 -= 1;

//	pr_info("erasing block %d\n", ebnum);
	err = mtd_test_erase_eraseblock(ebnum, 0);
	if (err)
		return err;

//	pr_info("writing 1st page of block %d\n", ebnum);
	prandom_bytes_state(&rnd_state, writebuf, pgsize);
	strcpy(writebuf, "There is no data like this!");
	err = mtd_write(mtd, addr0, pgsize, &written, writebuf);
	if (err || written != pgsize) {
		pr_err("2.16 error: write failed at %#llx\n",
		       (long long)addr0);
		return err ? err : -EINVAL;
	}

//	pr_info("reading 1st page of block %d\n", ebnum);
	memset(readbuf, 0, pgsize);
	err = mtd_read(mtd, addr0, pgsize, &read, readbuf);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.17 error: read failed at %#llx\n",
		       (long long)addr0);
		return err ? err : -EINVAL;
	}

//	pr_info("verifying 1st page of block %d\n", ebnum);
	if (memcmp(writebuf, readbuf, pgsize)) {
		pr_err("2.18 verify failed!\n");
		return -1;
	}

//	pr_info("erasing block %d\n", ebnum);
	err = mtd_test_erase_eraseblock(ebnum, 0);
	if (err)
		return err;

//	pr_info("writing 1st page of block %d\n", ebnum);
	prandom_bytes_state(&rnd_state, writebuf, pgsize);
	strcpy(writebuf, "There is no data like this!");
	err = mtd_write(mtd, addr0, pgsize, &written, writebuf);
	if (err || written != pgsize) {
		pr_err("2.19 error: write failed at %#llx\n",
		       (long long)addr0);
		return err ? err : -EINVAL;
	}

//	pr_info("erasing block %d\n", ebnum2);
	err = mtd_test_erase_eraseblock(ebnum2, 0);
	if (err)
		return err;

//	pr_info("reading 1st page of block %d\n", ebnum);
	memset(readbuf, 0, pgsize);
	err = mtd_read(mtd, addr0, pgsize, &read, readbuf);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.20 error: read failed at %#llx\n",
		       (long long)addr0);
		return err ? err : -EINVAL;
	}

//	pr_info("verifying 1st page of block %d\n", ebnum);
	if (memcmp(writebuf, readbuf, pgsize)) {
		pr_err("2.21 verify failed!\n");
		return -1;
	}

//	if (!err)
//		pr_info("erasecrosstest ok\n");
	return err;
}

static int pagetest_erasetest(void)
{
	int err = 0;
	loff_t addr0 = 0;
	size_t read = 0, written = 0;
	unsigned int i = 0, ebnum = 0, ok = 1;

//	pr_info("erasetest\n");

	ebnum = 0;
	addr0 = 0;
	for (i = 0; i < ebcnt && bbt[i]; ++i) {
		addr0 += mtd->erasesize;
		ebnum += 1;
	}

//	pr_info("erasing block %d\n", ebnum);
	err = mtd_test_erase_eraseblock(ebnum, 0);
	if (err)
		return err;

//	pr_info("writing 1st page of block %d\n", ebnum);
	prandom_bytes_state(&rnd_state, writebuf, pgsize);
	err = mtd_write(mtd, addr0, pgsize, &written, writebuf);
	if (err || written != pgsize) {
		pr_err("2.22 error: write failed at %#llx\n",
		       (long long)addr0);
		return err ? err : -EINVAL;
	}

//	pr_info("erasing block %d\n", ebnum);
	err = mtd_test_erase_eraseblock(ebnum, 0);
	if (err)
		return err;

//	pr_info("reading 1st page of block %d\n", ebnum);
	err = mtd_read(mtd, addr0, pgsize, &read, twopages);
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != pgsize) {
		pr_err("2.23 error: read failed at %#llx\n",
		       (long long)addr0);
		return err ? err : -EINVAL;
	}

//	pr_info("verifying 1st page of block %d is all 0xff\n", ebnum);
	for (i = 0; i < pgsize; ++i)
		if (twopages[i] != 0xff) {
			pr_err("2.24 verifying all 0xff failed at %d\n",
			       i);
			ok = 0;
			break;
		}

//	if (ok && !err)
//		pr_info("erasetest ok\n");

	return err;
}

static int mtd_test_pagetest(void)
{
	int err = 0;
	unsigned int i = 0;

	bufsize = mtd->writesize * 2;
	writebuf = kzalloc(mtd->erasesize, GFP_KERNEL);
	if (!writebuf) {
		pr_err("error: alloc writebuf  failed!\n");
		err = -ENOMEM;
		goto out;
	}
	twopages = kzalloc(bufsize, GFP_KERNEL);
	if (!twopages) {
		pr_err("error: alloc twopages failed!\n");
		err = -ENOMEM;
		goto out;
	}
	boundary = kzalloc(bufsize, GFP_KERNEL);
	if (!boundary) {
		pr_err("error: alloc boundary failed!\n");
		err = -ENOMEM;
		goto out;
	}
	pr_info("pagetest begin!\n");

	err = mtd_test_erase_whole_device();
	if(err)
		goto out;

	prandom_seed_state(&rnd_state, 1);
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = pagetest_write_eraseblock(i);
		if (err)
			goto out;
		cond_resched();
	}

	prandom_seed_state(&rnd_state, 1);
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = pagetest_verify_eraseblock(i);
		if (err)
			goto out;
		cond_resched();
	}

	err = pagetest_crosstest();
	if (err)
		goto out;

	err = pagetest_erasecrosstest();
	if (err)
		goto out;

	err = pagetest_erasetest();
	if (err)
		goto out;

	pr_info("pagetest finished successfullly!\n");

out:
	kfree(writebuf);
	kfree(boundary);
	kfree(twopages);
	return err;
}

static int subpagetest_write_eraseblock(int ebnum)
{
	int err = 0;
	size_t written = 0;
	loff_t addr = ebnum * mtd->erasesize;

	prandom_bytes_state(&rnd_state, writebuf, subpgsize);
	err = mtd_write(mtd, addr, subpgsize, &written, writebuf);
	if (unlikely(err || written != subpgsize)) {
		pr_err("error: write failed at %#llx, err: %d, written: %d\n",
		       (long long)addr, err, written);
		return err ? err : -EINVAL;
	}

	addr += subpgsize;
	prandom_bytes_state(&rnd_state, writebuf, subpgsize);
	err = mtd_write(mtd, addr, subpgsize, &written, writebuf);
	if (unlikely(err || written != subpgsize)) {
		pr_err("error: write failed at %#llx, err: %d, written: %d\n",
		       (long long)addr, err, written);
		return err ? err : -EINVAL;
	}

	return err;
}

static int subpagetest_write_eraseblock2(int ebnum)
{
	int err = 0;
	size_t written = 0;
	unsigned int k = 0;
	loff_t addr = ebnum * mtd->erasesize;

	for (k = 1; k < 33; ++k) {
		if (addr + (subpgsize * k) > (ebnum + 1) * mtd->erasesize)
			break;
		prandom_bytes_state(&rnd_state, writebuf, subpgsize * k);
		err = mtd_write(mtd, addr, subpgsize * k, &written, writebuf);
		if (unlikely(err || written != subpgsize * k)) {
			pr_err("error: write failed at %#llx, err: %d, written: %d\n",
			       (long long)addr, err, written);
			return err ? err : -EINVAL;
		}
		addr += subpgsize * k;
	}

	return err;
}

static int subpagetest_verify_eraseblock(int ebnum)
{
	int err = 0;
	size_t read = 0;
	loff_t addr = ebnum * mtd->erasesize;

	prandom_bytes_state(&rnd_state, writebuf, subpgsize);
	memset(readbuf, 0, subpgsize);
	err = mtd_read(mtd, addr, subpgsize, &read, readbuf);
	if (unlikely(err || read != subpgsize)) {
		if (mtd_is_bitflip(err) && read == subpgsize) {
			pr_info("ECC correction at %#llx\n",
			       (long long)addr);
			err = 0;
		} else {
			pr_err("error: read failed at %#llx\n",
			       (long long)addr);
			return err ? err : -EINVAL;
		}
	}
	if (unlikely(memcmp(readbuf, writebuf, subpgsize))) {
		pr_err("error: verify failed at %#llx\n",
		       (long long)addr);
		pr_info("------------- written----------------\n");
		mtd_test_dump_buf(writebuf, subpgsize, 32);
		pr_info("------------- read ------------------\n");
		mtd_test_dump_buf(readbuf, subpgsize, 32);
		pr_info("-------------------------------------\n");
	}

	addr += subpgsize;

	prandom_bytes_state(&rnd_state, writebuf, subpgsize);
	memset(readbuf, 0, subpgsize);
	err = mtd_read(mtd, addr, subpgsize, &read, readbuf);
	if (unlikely(err || read != subpgsize)) {
		if (mtd_is_bitflip(err) && read == subpgsize) {
			pr_info("ECC correction at %#llx\n",
			       (long long)addr);
			err = 0;
		} else {
			pr_err("error: read failed at %#llx\n",
			       (long long)addr);
			return err ? err : -EINVAL;
		}
	}
	if (unlikely(memcmp(readbuf, writebuf, subpgsize))) {
		pr_info("error: verify failed at %#llx\n",
		       (long long)addr);
		pr_info("------------- written----------------\n");
		mtd_test_dump_buf(writebuf, subpgsize, 32);
		pr_info("------------- read ------------------\n");
		mtd_test_dump_buf(readbuf, subpgsize, 32);
		pr_info("-------------------------------------\n");
		return -1;
	}

	return err;
}

static int subpagetest_verify_eraseblock2(int ebnum)
{
	int err = 0;
	size_t read = 0;
	unsigned int k = 0;
	loff_t addr = ebnum * mtd->erasesize;

	for (k = 1; k < 33; ++k) {
		if (addr + (subpgsize * k) > (ebnum + 1) * mtd->erasesize)
			break;
		prandom_bytes_state(&rnd_state, writebuf, subpgsize * k);
		memset(readbuf, 0, subpgsize * k);
		err = mtd_read(mtd, addr, subpgsize * k, &read, readbuf);
		if (unlikely(err || read != subpgsize * k)) {
			if (mtd_is_bitflip(err) && read == subpgsize * k) {
				pr_info("ECC correction at %#llx\n",
				       (long long)addr);
				err = 0;
			} else {
				pr_err("error: read failed at "
				       "%#llx\n", (long long)addr);
				return err ? err : -EINVAL;
			}
		}
		if (unlikely(memcmp(readbuf, writebuf, subpgsize * k))) {
			pr_err("error: verify failed at %#llx\n",
			       (long long)addr);
			return -1;
		}
		addr += subpgsize * k;
	}

	return err;
}

static int subpagetest_verify_eraseblock_ff(int ebnum)
{
	int err = 0;
	uint32_t j = 0;
	size_t read = 0;
	loff_t addr = ebnum * mtd->erasesize;

	memset(writebuf, 0xff, subpgsize);
	for (j = 0; j < mtd->erasesize / subpgsize; ++j) {
		memset(readbuf, 0, subpgsize);
		err = mtd_read(mtd, addr, subpgsize, &read, readbuf);
		if (unlikely(err || read != subpgsize)) {
			if (mtd_is_bitflip(err) && read == subpgsize) {
				pr_info("ECC correction at %#llx\n",
				       (long long)addr);
				err = 0;
			} else {
				pr_err("error: read failed at "
				       "%#llx\n", (long long)addr);
				return err ? err : -EINVAL;
			}
		}
		if (unlikely(memcmp(readbuf, writebuf, subpgsize))) {
			pr_err("error: verify 0xff failed at "
			       "%#llx\n", (long long)addr);
			return -1;
		}
		addr += subpgsize;
	}

	return err;
}

static int subpagetest_verify_all_eraseblocks_ff(void)
{
	int err = 0;
	unsigned int i = 0;

//	pr_info("verifying all eraseblocks for 0xff\n");
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = subpagetest_verify_eraseblock_ff(i);
		if (err)
			return err;
		cond_resched();
	}
//	pr_info("verified %u eraseblocks\n", i);
	return 0;
}

static int mtd_test_subpagetest(void)
{
	int err = 0;
	unsigned int i = 0;

	writebuf = kmalloc(32 * subpgsize, GFP_KERNEL);
	if (!writebuf) {
		pr_err("Error: cannot allocate enough memory!\n");
		err = -ENOMEM;
		goto out;
	}
	readbuf = kmalloc(32 * subpgsize, GFP_KERNEL);
	if (!readbuf) {
		pr_err("Error: cannot allocate enough memory!\n");
		err = -ENOMEM;
		goto out;
	}
	pr_info("subpagetest begin!\n");
	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

//	pr_info("writing whole device\n");
	prandom_seed_state(&rnd_state, 1);
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = subpagetest_write_eraseblock(i);
		if (unlikely(err))
			goto out;
		cond_resched();
	}
//	pr_info("written %u eraseblocks\n", i);

	prandom_seed_state(&rnd_state, 1);
//	pr_info("verifying all eraseblocks\n");
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = subpagetest_verify_eraseblock(i);
		if (unlikely(err))
			goto out;
		cond_resched();
	}
//	pr_info("verified %u eraseblocks\n", i);

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	err = subpagetest_verify_all_eraseblocks_ff();
	if (err)
		goto out;

	/* Write all eraseblocks */
	prandom_seed_state(&rnd_state, 3);
//	pr_info("writing whole device\n");
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = subpagetest_write_eraseblock2(i);
		if (unlikely(err))
			goto out;
		cond_resched();
	}
//	pr_info("written %u eraseblocks\n", i);

	/* Check all eraseblocks */
	prandom_seed_state(&rnd_state, 3);
//	pr_info("verifying all eraseblocks\n");
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = subpagetest_verify_eraseblock2(i);
		if (unlikely(err))
			goto out;
		cond_resched();
	}
//	pr_info("verified %u eraseblocks\n", i);

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	err = subpagetest_verify_all_eraseblocks_ff();
	if (err)
		goto out;

	pr_info("subpagetest finished successfully!\n");

out:
	kfree(readbuf);
	kfree(writebuf);
	return err;
}

static void oobtest_do_vary_offset(void)
{
	use_len -= 1;
	if (use_len < 1) {
		use_offset += 1;
		if (use_offset >= use_len_max)
			use_offset = 0;
		use_len = use_len_max - use_offset;
	}
}

static int oobtest_write_eraseblock(int ebnum)
{
	int err = 0;
	unsigned int i = 0;
	struct mtd_oob_ops ops;
	loff_t addr = ebnum * mtd->erasesize;

	for (i = 0; i < pgcnt; ++i, addr += mtd->writesize) {
		prandom_bytes_state(&rnd_state, writebuf, use_len);
		ops.mode      = MTD_OPS_AUTO_OOB;
		ops.len       = 0;
		ops.retlen    = 0;
		ops.ooblen    = use_len;
		ops.oobretlen = 0;
		ops.ooboffs   = use_offset;
		ops.datbuf    = NULL;
		ops.oobbuf    = writebuf;
		err = mtd_write_oob(mtd, addr, &ops);
		if (err || ops.oobretlen != use_len) {
			pr_err("error: writeoob failed at %#llx\n",
			       (long long)addr);
			pr_err("error: use_len %d, use_offset %d\n",
			       use_len, use_offset);
			errcnt += 1;
			return err ? err : -1;
		}
		if (vary_offset)
			oobtest_do_vary_offset();
	}

	return err;
}

static int oobtest_write_whole_device(void)
{
	int err = 0;
	unsigned int i = 1;

//	pr_info("writing OOBs of whole device\n");
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = oobtest_write_eraseblock(i);
		if (err)
			return err;
		cond_resched();
	}
//	pr_info("written %u eraseblocks\n", i);
	return 0;
}

static int oobtest_verify_eraseblock(int ebnum)
{
	int err = 0;
	unsigned int i = 0;
	struct mtd_oob_ops ops = {0};
	loff_t addr = ebnum * mtd->erasesize;

	for (i = 0; i < pgcnt; ++i, addr += mtd->writesize) {
		prandom_bytes_state(&rnd_state, writebuf, use_len);
		ops.mode      = MTD_OPS_AUTO_OOB;
		ops.len       = 0;
		ops.retlen    = 0;
		ops.ooblen    = use_len;
		ops.oobretlen = 0;
		ops.ooboffs   = use_offset;
		ops.datbuf    = NULL;
		ops.oobbuf    = readbuf;
		err = mtd_read_oob(mtd, addr, &ops);
		if (err || ops.oobretlen != use_len) {
			pr_err("error: readoob failed at %#llx\n",
			       (long long)addr);
			errcnt += 1;
			return err ? err : -1;
		}
		if (memcmp(readbuf, writebuf, use_len)) {
			pr_err("error: verify failed at %#llx\n",
			       (long long)addr);
			errcnt += 1;
			if (errcnt > 1000) {
				pr_err("error: too many errors\n");
				return -1;
			}
		}
		if (use_offset != 0 || use_len < mtd->ecclayout->oobavail) {
			int k;

			ops.mode      = MTD_OPS_AUTO_OOB;
			ops.len       = 0;
			ops.retlen    = 0;
			ops.ooblen    = mtd->ecclayout->oobavail;
			ops.oobretlen = 0;
			ops.ooboffs   = 0;
			ops.datbuf    = NULL;
			ops.oobbuf    = readbuf;
			err = mtd_read_oob(mtd, addr, &ops);
			if (err || ops.oobretlen != mtd->ecclayout->oobavail) {
				pr_err("error: readoob failed at %#llx\n",
						(long long)addr);
				errcnt += 1;
				return err ? err : -1;
			}
			if (memcmp(readbuf + use_offset, writebuf, use_len)) {
				pr_err("error: verify failed at %#llx\n",
						(long long)addr);
				errcnt += 1;
				if (errcnt > 1000) {
					pr_err("error: too many errors\n");
					return -1;
				}
			}
			for (k = 0; k < use_offset; ++k)
				if (readbuf[k] != 0xff) {
					pr_err("error: verify 0xff "
					       "failed at %#llx\n",
					       (long long)addr);
					errcnt += 1;
					if (errcnt > 1000) {
						pr_err("error: too "
						       "many errors\n");
						return -1;
					}
				}
			for (k = use_offset + use_len;
			     k < mtd->ecclayout->oobavail; ++k)
				if (readbuf[k] != 0xff) {
					pr_err("error: verify 0xff "
					       "failed at %#llx\n",
					       (long long)addr);
					errcnt += 1;
					if (errcnt > 1000) {
						pr_err("error: too "
						       "many errors\n");
						return -1;
					}
				}
		}
		if (vary_offset)
			oobtest_do_vary_offset();
	}
	return err;
}

/*
add by ming.tang
adjust the str when mtd->ecclayout->oobavail is not 4bytes alignment
@str:the string need to be adjusted
@str_len:the string's length
@sub_len:mtd->ecclayout->oobavail
*/
static void oobtest_adjust(char *str, unsigned int str_len, unsigned int sub_len)
{
	int i = 0;
	char *tmp_str = NULL;
	unsigned int tmp_len = 0;

	tmp_len = ((sub_len >> 2) + 1) << 2;
	tmp_str = (char *)vmalloc(str_len);
	if(!tmp_str) {
		pr_err("%s, can't get enough memory!\n", __func__);
		return;
	}

	memcpy(tmp_str, str, str_len);
	memset(str, 0, str_len);

	for(i = 0; i < str_len/tmp_len; i++)
	{
		memcpy(str + (sub_len * i), tmp_str + (tmp_len * i), sub_len);
	}
	vfree(tmp_str);
	return;
}

static int oobtest_verify_eraseblock_in_one_go(int ebnum)
{
	int err = 0;
	struct mtd_oob_ops ops = {0};
	loff_t addr = ebnum * mtd->erasesize;
	size_t len = mtd->ecclayout->oobavail * pgcnt;

	prandom_bytes_state(&rnd_state, writebuf, (((mtd->ecclayout->oobavail >> 2) + 1) << 2) * pgcnt);
	if(mtd->ecclayout->oobavail%4 != 0)
		oobtest_adjust(writebuf, (((mtd->ecclayout->oobavail >> 2) + 1) << 2) * pgcnt, mtd->ecclayout->oobavail);
	ops.mode      = MTD_OPS_AUTO_OOB;
	ops.len       = 0;
	ops.retlen    = 0;
	ops.ooblen    = len;
	ops.oobretlen = 0;
	ops.ooboffs   = 0;
	ops.datbuf    = NULL;
	ops.oobbuf    = readbuf;
	err = mtd_read_oob(mtd, addr, &ops);
	if (err || ops.oobretlen != len) {
		pr_err("error: readoob failed at %#llx\n",
		       (long long)addr);
		errcnt += 1;
		return err ? err : -1;
	}
	if (memcmp(readbuf, writebuf, len)) {
		pr_err("error: verify failed at %#llx\n",
		       (long long)addr);
		errcnt += 1;
		if (errcnt > 1000) {
			pr_err("error: too many errors\n");
			return -1;
		}
	}

	return err;
}

static int oobtest_verify_all_eraseblocks(void)
{
	int err = 0;
	unsigned int i = 0;

//	pr_info("verifying all eraseblocks\n");
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = oobtest_verify_eraseblock(i);
		if (err)
			return err;
		cond_resched();
	}
//	pr_info("verified %u eraseblocks\n", i);
	return 0;
}

static int mtd_test_oobtest(void)
{
	int err = 0;
	unsigned int i = 0;
	loff_t addr = 0, addr0 = 0;
	struct mtd_oob_ops ops = {0};

	err = -ENOMEM;
	readbuf = vmalloc(mtd->erasesize);
	if (!readbuf) {
		pr_err("error: alloc readbuf failed!\n");
		goto out;
	}
	writebuf = vmalloc(mtd->erasesize);
	if (!writebuf) {
		pr_err("error: alloc writebuf failed!\n");
		goto out;
	}

	use_offset = 0;
	use_len = mtd->ecclayout->oobavail;
	use_len_max = mtd->ecclayout->oobavail;
	vary_offset = 0;
	pr_info("oobtest begin!\n");

	/* First test: write all OOB, read it back and verify */
	pr_info("test 1 of 5\n");

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	prandom_seed_state(&rnd_state, 1);
	err = oobtest_write_whole_device();
	if (err)
		goto out;

	prandom_seed_state(&rnd_state, 1);
	err = oobtest_verify_all_eraseblocks();
	if (err)
		goto out;

	/*
	 * Second test: write all OOB, a block at a time, read it back and
	 * verify.
	 */
	pr_info("test 2 of 5\n");

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	prandom_seed_state(&rnd_state, 3);
	err = oobtest_write_whole_device();
	if (err)
		goto out;

	/* Check all eraseblocks */
	prandom_seed_state(&rnd_state, 3);
//	pr_info("verifying all eraseblocks\n");
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = oobtest_verify_eraseblock_in_one_go(i);
		if (err)
			goto out;
		cond_resched();
	}
//	pr_info("verified %u eraseblocks\n", i);

	/*
	 * Third test: write OOB at varying offsets and lengths, read it back
	 * and verify.
	 */
	pr_info("test 3 of 5\n");

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	/* Write all eraseblocks */
	use_offset = 0;
	use_len = mtd->ecclayout->oobavail;
	use_len_max = mtd->ecclayout->oobavail;
	vary_offset = 1;
	prandom_seed_state(&rnd_state, 5);

	err = oobtest_write_whole_device();
	if (err)
		goto out;

	/* Check all eraseblocks */
	use_offset = 0;
	use_len = mtd->ecclayout->oobavail;
	use_len_max = mtd->ecclayout->oobavail;
	vary_offset = 1;
	prandom_seed_state(&rnd_state, 5);
	err = oobtest_verify_all_eraseblocks();
	if (err)
		goto out;

	use_offset = 0;
	use_len = mtd->ecclayout->oobavail;
	use_len_max = mtd->ecclayout->oobavail;
	vary_offset = 0;

	/* Fourth test: try to write off end of device */
	pr_info("test 4 of 5\n");

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	addr0 = 0;
	for (i = 0; i < ebcnt && bbt[i]; ++i)
		addr0 += mtd->erasesize;

	/* Attempt to write off end of OOB */
	ops.mode      = MTD_OPS_AUTO_OOB;
	ops.len       = 0;
	ops.retlen    = 0;
	ops.ooblen    = 1;
	ops.oobretlen = 0;
	ops.ooboffs   = mtd->ecclayout->oobavail;
	ops.datbuf    = NULL;
	ops.oobbuf    = writebuf;
	pr_info("attempting to start write past end of OOB\n");
	pr_info("an error is expected...\n");
	err = mtd_write_oob(mtd, addr0, &ops);
	if (err) {
		pr_info("error occurred as expected\n");
		err = 0;
	} else {
		pr_info("error: can write past end of OOB\n");
		errcnt += 1;
	}

	/* Attempt to read off end of OOB */
	ops.mode      = MTD_OPS_AUTO_OOB;
	ops.len       = 0;
	ops.retlen    = 0;
	ops.ooblen    = 1;
	ops.oobretlen = 0;
	ops.ooboffs   = mtd->ecclayout->oobavail;
	ops.datbuf    = NULL;
	ops.oobbuf    = readbuf;
	pr_info("attempting to start read past end of OOB\n");
	pr_info("an error is expected...\n");
	err = mtd_read_oob(mtd, addr0, &ops);
	if (err) {
		pr_info("error occurred as expected\n");
		err = 0;
	} else {
		pr_err("error: can read past end of OOB\n");
		errcnt += 1;
	}

	if (bbt[ebcnt - 1])
		pr_info("skipping end of device tests because last "
		       "block is bad\n");
	else {
		/* Attempt to write off end of device */
		ops.mode      = MTD_OPS_AUTO_OOB;
		ops.len       = 0;
		ops.retlen    = 0;
		ops.ooblen    = mtd->ecclayout->oobavail + 1;
		ops.oobretlen = 0;
		ops.ooboffs   = 0;
		ops.datbuf    = NULL;
		ops.oobbuf    = writebuf;
		pr_info("attempting to write past end of device\n");
		pr_info("an error is expected...\n");
		err = mtd_write_oob(mtd, mtd->size - mtd->writesize, &ops);
		if (err) {
			pr_info("error occurred as expected\n");
			err = 0;
		} else {
			pr_err("error: wrote past end of device\n");
			errcnt += 1;
		}

		/* Attempt to read off end of device */
		ops.mode      = MTD_OPS_AUTO_OOB;
		ops.len       = 0;
		ops.retlen    = 0;
		ops.ooblen    = mtd->ecclayout->oobavail + 1;
		ops.oobretlen = 0;
		ops.ooboffs   = 0;
		ops.datbuf    = NULL;
		ops.oobbuf    = readbuf;
		pr_info("attempting to read past end of device\n");
		pr_info("an error is expected...\n");
		err = mtd_read_oob(mtd, mtd->size - mtd->writesize, &ops);
		if (err) {
			pr_info("error occurred as expected\n");
			err = 0;
		} else {
			pr_err("error: read past end of device\n");
			errcnt += 1;
		}

		err = mtd_test_erase_eraseblock(ebcnt - 1, 0);
		if (err)
			goto out;

		/* Attempt to write off end of device */
		ops.mode      = MTD_OPS_AUTO_OOB;
		ops.len       = 0;
		ops.retlen    = 0;
		ops.ooblen    = mtd->ecclayout->oobavail;
		ops.oobretlen = 0;
		ops.ooboffs   = 1;
		ops.datbuf    = NULL;
		ops.oobbuf    = writebuf;
		pr_info("attempting to write past end of device\n");
		pr_info("an error is expected...\n");
		err = mtd_write_oob(mtd, mtd->size - mtd->writesize, &ops);
		if (err) {
			pr_info("error occurred as expected\n");
			err = 0;
		} else {
			pr_err("error: wrote past end of device\n");
			errcnt += 1;
		}

		/* Attempt to read off end of device */
		ops.mode      = MTD_OPS_AUTO_OOB;
		ops.len       = 0;
		ops.retlen    = 0;
		ops.ooblen    = mtd->ecclayout->oobavail;
		ops.oobretlen = 0;
		ops.ooboffs   = 1;
		ops.datbuf    = NULL;
		ops.oobbuf    = readbuf;
		pr_info("attempting to read past end of device\n");
		pr_info("an error is expected...\n");
		err = mtd_read_oob(mtd, mtd->size - mtd->writesize, &ops);
		if (err) {
			pr_info("error occurred as expected\n");
			err = 0;
		} else {
			pr_err("error: read past end of device\n");
			errcnt += 1;
		}
	}

	/* Fifth test: write / read across block boundaries */
	pr_info("test 5 of 5\n");

	/* Erase all eraseblocks */
	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	/* Write all eraseblocks */
	prandom_seed_state(&rnd_state, 11);
//	pr_info("writing OOBs of whole device\n");
	for (i = 0; i < ebcnt - 1; ++i) {
		int cnt = 2;
		int pg;
		size_t sz = mtd->ecclayout->oobavail;
		if (bbt[i] || bbt[i + 1])
			continue;
		addr = (i + 1) * mtd->erasesize - mtd->writesize;
		for (pg = 0; pg < cnt; ++pg) {
			prandom_bytes_state(&rnd_state, writebuf, sz);
			ops.mode      = MTD_OPS_AUTO_OOB;
			ops.len       = 0;
			ops.retlen    = 0;
			ops.ooblen    = sz;
			ops.oobretlen = 0;
			ops.ooboffs   = 0;
			ops.datbuf    = NULL;
			ops.oobbuf    = writebuf;
			err = mtd_write_oob(mtd, addr, &ops);
			if (err)
				goto out;
			cond_resched();
			addr += mtd->writesize;
		}
	}
//	pr_info("written %u eraseblocks\n", i);

	/* Check all eraseblocks */
	prandom_seed_state(&rnd_state, 11);
//	pr_info("verifying all eraseblocks\n");
	for (i = 0; i < ebcnt - 1; ++i) {
		if (bbt[i] || bbt[i + 1])
			continue;
		prandom_bytes_state(&rnd_state, writebuf,
					(((mtd->ecclayout->oobavail >> 2) + 1) << 2) * 2);
		if(mtd->ecclayout->oobavail%4 != 0)
			oobtest_adjust(writebuf, (((mtd->ecclayout->oobavail >> 2) + 1) << 2) * 2, mtd->ecclayout->oobavail);
		addr = (i + 1) * mtd->erasesize - mtd->writesize;
		ops.mode      = MTD_OPS_AUTO_OOB;
		ops.len       = 0;
		ops.retlen    = 0;
		ops.ooblen    = mtd->ecclayout->oobavail * 2;
		ops.oobretlen = 0;
		ops.ooboffs   = 0;
		ops.datbuf    = NULL;
		ops.oobbuf    = readbuf;
		err = mtd_read_oob(mtd, addr, &ops);
		if (err)
			goto out;
		if (memcmp(readbuf, writebuf, mtd->ecclayout->oobavail * 2)) {
			pr_err("error: verify failed at %#llx\n",
			       (long long)addr);
			errcnt += 1;
			if (errcnt > 1000) {
				pr_err("error: too many errors\n");
				goto out;
			}
		}
		cond_resched();
	}
//	pr_info("verified %u eraseblocks\n", i);

	pr_info("oobtest finished with %d errors!\n", errcnt);
out:
	vfree(writebuf);
	vfree(readbuf);

	return err;
}

static long speedtest_calc_speed(void)
{
	long ms = 0;
	uint64_t k = 0;

	ms = (finish.tv_sec - start.tv_sec) * 1000 +
	     (finish.tv_usec - start.tv_usec) / 1000;
	if (ms == 0)
		return 0;
	k = goodebcnt * (mtd->erasesize / 1024) * 1000;
	do_div(k, ms);
	return k;
}

static int speedtest_write_eraseblock(int ebnum)
{
	int err = 0;
	size_t written = 0;
	loff_t addr = ebnum * mtd->erasesize;

	err = mtd_write(mtd, addr, mtd->erasesize, &written, writebuf);
	if (err || written != mtd->erasesize) {
		pr_err("error: write failed at %#llx\n", addr);
		if (!err)
			err = -EINVAL;
	}

	return err;
}

static int speedtest_write_eraseblock_by_page(int ebnum)
{
	int err = 0;
	size_t written = 0;
	unsigned int i = 0;
	void *buf = writebuf;
	loff_t addr = ebnum * mtd->erasesize;

	for (i = 0; i < pgcnt; i++) {
		err = mtd_write(mtd, addr, pgsize, &written, buf);
		if (err || written != pgsize) {
			pr_err("error: write failed at %#llx\n",
			       addr);
			if (!err)
				err = -EINVAL;
			break;
		}
		addr += pgsize;
		buf += pgsize;
	}

	return err;
}

static int speedtest_write_eraseblock_by_2pages(int ebnum)
{
	int err = 0;
	void *buf = writebuf;
	unsigned int i = 0, n = pgcnt / 2;
	size_t written = 0, sz = pgsize * 2;
	loff_t addr = ebnum * mtd->erasesize;

	for (i = 0; i < n; i++) {
		err = mtd_write(mtd, addr, sz, &written, buf);
		if (err || written != sz) {
			pr_err("error: write failed at %#llx\n",
			       addr);
			if (!err)
				err = -EINVAL;
			return err;
		}
		addr += sz;
		buf += sz;
	}
	if (pgcnt % 2) {
		err = mtd_write(mtd, addr, pgsize, &written, buf);
		if (err || written != pgsize) {
			pr_err("error: write failed at %#llx\n",
			       addr);
			if (!err)
				err = -EINVAL;
		}
	}

	return err;
}

static int speedtest_read_eraseblock(int ebnum)
{
	int err = 0;
	size_t read = 0;
	loff_t addr = ebnum * mtd->erasesize;

	err = mtd_read(mtd, addr, mtd->erasesize, &read, writebuf);
	/* Ignore corrected ECC errors */
	if (mtd_is_bitflip(err))
		err = 0;
	if (err || read != mtd->erasesize) {
		pr_err("error: read failed at %#llx\n", addr);
		if (!err)
			err = -EINVAL;
	}

	return err;
}

static int speedtest_read_eraseblock_by_page(int ebnum)
{
	int err = 0;
	size_t read = 0;
	unsigned int i = 0;
	void *buf = writebuf;
	loff_t addr = ebnum * mtd->erasesize;

	for (i = 0; i < pgcnt; i++) {
		err = mtd_read(mtd, addr, pgsize, &read, buf);
		/* Ignore corrected ECC errors */
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != pgsize) {
			pr_err("error: read failed at %#llx\n",
			       addr);
			if (!err)
				err = -EINVAL;
			break;
		}
		addr += pgsize;
		buf += pgsize;
	}

	return err;
}

static int speedtest_read_eraseblock_by_2pages(int ebnum)
{
	int err = 0;
	void *buf = writebuf;
	size_t read = 0, sz = pgsize * 2;
	unsigned int i = 0, n = pgcnt / 2;
	loff_t addr = ebnum * mtd->erasesize;

	for (i = 0; i < n; i++) {
		err = mtd_read(mtd, addr, sz, &read, buf);
		/* Ignore corrected ECC errors */
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != sz) {
			pr_err("error: read failed at %#llx\n",
			       addr);
			if (!err)
				err = -EINVAL;
			return err;
		}
		addr += sz;
		buf += sz;
	}
	if (pgcnt % 2) {
		err = mtd_read(mtd, addr, pgsize, &read, buf);
		/* Ignore corrected ECC errors */
		if (mtd_is_bitflip(err))
			err = 0;
		if (err || read != pgsize) {
			pr_err("error: read failed at %#llx\n",
			       addr);
			if (!err)
				err = -EINVAL;
		}
	}

	return err;
}

static int mtd_test_speedtest(void )
{
	int err = 0;
	long speed = 0;
	unsigned int i = 0, blocks = 0, j = 0, k = 0;

	writebuf = vmalloc(mtd->erasesize);
	if(!writebuf) {
		pr_err("%s, can't get enough memory!\n", __func__);
		return -ENOMEM;
	}

	memset(writebuf, 0, mtd->erasesize);
	prandom_bytes(writebuf, mtd->erasesize);

	pr_info("speedtest begin!\n");
	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	/* Write all eraseblocks, 1 eraseblock at a time */
//	pr_info("testing eraseblock write speed\n");
	mtd_test_start_timing();
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = speedtest_write_eraseblock(i);
		if (err)
			goto out;
		cond_resched();
	}
	mtd_test_stop_timing();
	speed = speedtest_calc_speed();
	pr_info("eraseblock write speed is %ld KiB/s\n", speed);

	/* Read all eraseblocks, 1 eraseblock at a time */
//	pr_info("testing eraseblock read speed\n");
	mtd_test_start_timing();
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = speedtest_read_eraseblock(i);
		if (err)
			goto out;
		cond_resched();
	}
	mtd_test_stop_timing();
	speed = speedtest_calc_speed();
	pr_info("eraseblock read speed is %ld KiB/s\n", speed);

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	/* Write all eraseblocks, 1 page at a time */
//	pr_info("testing page write speed\n");
	mtd_test_start_timing();
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = speedtest_write_eraseblock_by_page(i);
		if (err)
			goto out;
		cond_resched();
	}
	mtd_test_stop_timing();
	speed = speedtest_calc_speed();
	pr_info("page write speed is %ld KiB/s\n", speed);

	/* Read all eraseblocks, 1 page at a time */
//	pr_info("testing page read speed\n");
	mtd_test_start_timing();
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = speedtest_read_eraseblock_by_page(i);
		if (err)
			goto out;
		cond_resched();
	}
	mtd_test_stop_timing();
	speed = speedtest_calc_speed();
	pr_info("page read speed is %ld KiB/s\n", speed);

	err = mtd_test_erase_whole_device();
	if (err)
		goto out;

	/* Write all eraseblocks, 2 pages at a time */
//	pr_info("testing 2 page write speed\n");
	mtd_test_start_timing();
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = speedtest_write_eraseblock_by_2pages(i);
		if (err)
			goto out;
		cond_resched();
	}
	mtd_test_stop_timing();
	speed = speedtest_calc_speed();
	pr_info("2 page write speed is %ld KiB/s\n", speed);

	/* Read all eraseblocks, 2 pages at a time */
//	pr_info("testing 2 page read speed\n");
	mtd_test_start_timing();
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = speedtest_read_eraseblock_by_2pages(i);
		if (err)
			goto out;
		cond_resched();
	}
	mtd_test_stop_timing();
	speed = speedtest_calc_speed();
	pr_info("2 page read speed is %ld KiB/s\n", speed);

	/* Erase all eraseblocks */
//	pr_info("Testing erase speed\n");
	mtd_test_start_timing();
	for (i = 0; i < ebcnt; ++i) {
		if (bbt[i])
			continue;
		err = mtd_test_erase_eraseblock(i, 0);
		if (err)
			goto out;
		cond_resched();
	}
	mtd_test_stop_timing();
	speed = speedtest_calc_speed();
	pr_info("erase speed is %ld KiB/s\n", speed);

	/* Multi-block erase all eraseblocks */
	for (k = 1; k < 7; k++) {
		blocks = 1 << k;
//		pr_info("Testing %dx multi-block erase speed\n", blocks);
		mtd_test_start_timing();
		for (i = 0; i < ebcnt; ) {
			for (j = 0; j < blocks && (i + j) < ebcnt; j++)
				if (bbt[i + j])
					break;
			if (j < 1) {
				i++;
				continue;
			}
			err = mtd_test_erase_eraseblock(i, j);
			if (err)
				goto out;
			cond_resched();
			i += j;
		}
		mtd_test_stop_timing();
		speed = speedtest_calc_speed();
		pr_info("%dx multi-block erase speed is %ld KiB/s\n",
		       blocks, speed);
	}
	pr_info("speedtest finished successfully!\n");

out:
	vfree(writebuf);
	return err;
}

static int stresstest_rand_eb(void)
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

static int stresstest_rand_offs(void)
{
	unsigned int offs = 0;

	offs = prandom_u32();
	offs %= bufsize;
	return offs;
}

static int stresstest_rand_len(int offs)
{
	unsigned int len = 0;

	len = prandom_u32();
	len %= (bufsize - offs);
	return len;
}

static int stresstest_do_read(void)
{
	int err = 0;
	size_t read = 0;
	loff_t addr = 0;
	int eb = stresstest_rand_eb();
	int offs = stresstest_rand_offs();
	int len = stresstest_rand_len(offs);

	if (bbt[eb + 1]) {
		if (offs >= mtd->erasesize)
			offs -= mtd->erasesize;
		if (offs + len > mtd->erasesize)
			len = mtd->erasesize - offs;
	}
	addr = eb * mtd->erasesize + offs;
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
	return 0;
}

static int stresstest_do_write(void)
{
	int err = 0;
	loff_t addr = 0;
	size_t written = 0;
	int eb = stresstest_rand_eb();
	unsigned int offs = 0, len = 0;

	offs = offsets[eb];
	if (offs >= mtd->erasesize) {
		err = mtd_test_erase_eraseblock(eb, 0);
		if (err)
			return err;
		offs = offsets[eb] = 0;
	}
	len = stresstest_rand_len(offs);
	len = ((len + pgsize - 1) / pgsize) * pgsize;
	if (offs + len > mtd->erasesize) {
		if (bbt[eb + 1])
			len = mtd->erasesize - offs;
		else {
			err = mtd_test_erase_eraseblock(eb + 1, 0);
			if (err)
				return err;
			offsets[eb + 1] = 0;
		}
	}
	addr = eb * mtd->erasesize + offs;
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

static int stresstest_do_operation(void)
{
	if (prandom_u32() & 1) {
		return stresstest_do_read();
	} else {
		return stresstest_do_write();
	}
}

static int mtd_test_stresstest(void)
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
	pr_info("stresstest begin!\n");

	for (i = 0; i < ebcnt; i++)
		offsets[i] = mtd->erasesize;
	prandom_bytes(writebuf, bufsize);

	err = mtd_test_erase_whole_device();
	if(err)
		goto out;

	for (op = 0; op < 10000; op++) {
		if ((op & 1023) == 0)
			pr_info("%d operations done\n", op);
		err = stresstest_do_operation();
		if (err)
			goto out;
		cond_resched();
	}

out:
	vfree(offsets);
	vfree(readbuf);
	vfree(writebuf);
	pr_info("stresstest finished, %d operations done!\n", op);
	return err;
}

static int __init nandflash_stdtest_init(void)
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
	pgsize = mtd->writesize;
	subsize  = mtd->writesize >> mtd->subpage_sft;
	subcount = mtd->writesize / subsize;
	subpgsize = mtd->writesize >> mtd->subpage_sft;

	pr_info("MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n",
	       (unsigned long long)mtd->size, mtd->erasesize,
	       mtd->writesize, ebcnt, pgcnt, mtd->oobsize);

	err = mtd_test_scan_bad_block();
	if (err)
		goto out;

	err = mtd_test_readtest();
	if(err) {
		pr_err("readtest failed!\n");
		goto out;
	}
	mdelay(100);
	err = mtd_test_pagetest();
	if(err) {
		pr_err("pagetest failed!\n");
		goto out;
	}
	mdelay(100);
	err = mtd_test_subpagetest();
	if(err) {
		pr_err("subpagetest failed!\n");
		goto out;
	}
	mdelay(100);
	err = mtd_test_oobtest();
	if(err) {
		pr_err("oobtest failed!\n");
		goto out;
	}
	mdelay(100);
	err = mtd_test_speedtest();
	if(err) {
		pr_err("speedtest failed!\n");
		goto out;
	}
	mdelay(100);
	err = mtd_test_stresstest();
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

static void __exit nandflash_stdtest_exit(void)
{
	return;
}

module_init(nandflash_stdtest_init);
module_exit(nandflash_stdtest_exit);

MODULE_DESCRIPTION("mtd_stdand_test");
MODULE_AUTHOR("ming.tang@spreadtrum.com");
MODULE_LICENSE("GPL");
