/*
 * Debugfs support for hosts and cards
 *
 * Copyright (C) 2008 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include "core.h"
#include "mmc_ops.h"

#ifdef CONFIG_FAIL_MMC_REQUEST

static DECLARE_FAULT_ATTR(fail_default_attr);
static char *fail_request;
module_param(fail_request, charp, 0);

#endif /* CONFIG_FAIL_MMC_REQUEST */

/* The debugfs functions are optimized away when CONFIG_DEBUG_FS isn't set. */
static int mmc_ios_show(struct seq_file *s, void *data)
{
	static const char *vdd_str[] = {
		[8]	= "2.0",
		[9]	= "2.1",
		[10]	= "2.2",
		[11]	= "2.3",
		[12]	= "2.4",
		[13]	= "2.5",
		[14]	= "2.6",
		[15]	= "2.7",
		[16]	= "2.8",
		[17]	= "2.9",
		[18]	= "3.0",
		[19]	= "3.1",
		[20]	= "3.2",
		[21]	= "3.3",
		[22]	= "3.4",
		[23]	= "3.5",
		[24]	= "3.6",
	};
	struct mmc_host	*host = s->private;
	struct mmc_ios	*ios = &host->ios;
	const char *str;

	seq_printf(s, "clock:\t\t%u Hz\n", ios->clock);
	if (host->actual_clock)
		seq_printf(s, "actual clock:\t%u Hz\n", host->actual_clock);
	seq_printf(s, "vdd:\t\t%u ", ios->vdd);
	if ((1 << ios->vdd) & MMC_VDD_165_195)
		seq_printf(s, "(1.65 - 1.95 V)\n");
	else if (ios->vdd < (ARRAY_SIZE(vdd_str) - 1)
			&& vdd_str[ios->vdd] && vdd_str[ios->vdd + 1])
		seq_printf(s, "(%s ~ %s V)\n", vdd_str[ios->vdd],
				vdd_str[ios->vdd + 1]);
	else
		seq_printf(s, "(invalid)\n");

	switch (ios->bus_mode) {
	case MMC_BUSMODE_OPENDRAIN:
		str = "open drain";
		break;
	case MMC_BUSMODE_PUSHPULL:
		str = "push-pull";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "bus mode:\t%u (%s)\n", ios->bus_mode, str);

	switch (ios->chip_select) {
	case MMC_CS_DONTCARE:
		str = "don't care";
		break;
	case MMC_CS_HIGH:
		str = "active high";
		break;
	case MMC_CS_LOW:
		str = "active low";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "chip select:\t%u (%s)\n", ios->chip_select, str);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		str = "off";
		break;
	case MMC_POWER_UP:
		str = "up";
		break;
	case MMC_POWER_ON:
		str = "on";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "power mode:\t%u (%s)\n", ios->power_mode, str);
	seq_printf(s, "bus width:\t%u (%u bits)\n",
			ios->bus_width, 1 << ios->bus_width);

	switch (ios->timing) {
	case MMC_TIMING_LEGACY:
		str = "legacy";
		break;
	case MMC_TIMING_MMC_HS:
		str = "mmc high-speed";
		break;
	case MMC_TIMING_SD_HS:
		str = "sd high-speed";
		break;
	case MMC_TIMING_UHS_SDR50:
		str = "sd uhs SDR50";
		break;
	case MMC_TIMING_UHS_SDR104:
		str = "sd uhs SDR104";
		break;
	case MMC_TIMING_UHS_DDR50:
		str = "sd uhs DDR50";
		break;
	case MMC_TIMING_MMC_HS200:
		str = "mmc high-speed SDR200";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "timing spec:\t%u (%s)\n", ios->timing, str);

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		str = "3.30 V";
		break;
	case MMC_SIGNAL_VOLTAGE_180:
		str = "1.80 V";
		break;
	case MMC_SIGNAL_VOLTAGE_120:
		str = "1.20 V";
		break;
	default:
		str = "invalid";
		break;
	}
	seq_printf(s, "signal voltage:\t%u (%s)\n", ios->chip_select, str);

	return 0;
}

static int mmc_ios_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_ios_show, inode->i_private);
}

static const struct file_operations mmc_ios_fops = {
	.open		= mmc_ios_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_clock_opt_get(void *data, u64 *val)
{
	struct mmc_host *host = data;

	*val = host->ios.clock;

	return 0;
}

static int mmc_clock_opt_set(void *data, u64 val)
{
	struct mmc_host *host = data;

	/* We need this check due to input value is u64 */
	if (val > host->f_max)
		return -EINVAL;

	mmc_claim_host(host);
	mmc_set_clock(host, (unsigned int) val);
	mmc_release_host(host);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mmc_clock_fops, mmc_clock_opt_get, mmc_clock_opt_set,
	"%llu\n");

void mmc_add_host_debugfs(struct mmc_host *host)
{
	struct dentry *root;

	root = debugfs_create_dir(mmc_hostname(host), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	if (!debugfs_create_file("ios", S_IRUSR, root, host, &mmc_ios_fops))
		goto err_node;

	if (!debugfs_create_file("clock", S_IRUSR | S_IWUSR, root, host,
			&mmc_clock_fops))
		goto err_node;

#ifdef CONFIG_MMC_CLKGATE
	if (!debugfs_create_u32("clk_delay", (S_IRUSR | S_IWUSR),
				root, &host->clk_delay))
		goto err_node;
#endif
#ifdef CONFIG_FAIL_MMC_REQUEST
	if (fail_request)
		setup_fault_attr(&fail_default_attr, fail_request);
	host->fail_mmc_request = fail_default_attr;
	if (IS_ERR(fault_create_debugfs_attr("fail_mmc_request",
					     root,
					     &host->fail_mmc_request)))
		goto err_node;
#endif
	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	dev_err(&host->class_dev, "failed to initialize debugfs\n");
}

void mmc_remove_host_debugfs(struct mmc_host *host)
{
	debugfs_remove_recursive(host->debugfs_root);
}

static int mmc_dbg_card_status_get(void *data, u64 *val)
{
	struct mmc_card	*card = data;
	u32		status;
	int		ret;

	mmc_claim_host(card->host);

	ret = mmc_send_status(data, &status);
	if (!ret)
		*val = status;

	mmc_release_host(card->host);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(mmc_dbg_card_status_fops, mmc_dbg_card_status_get,
		NULL, "%08llx\n");

#define EXT_CSD_STR_LEN 1025



static int mmc_ext_csd_open(struct inode *inode, struct file *filp)
{
	struct mmc_card *card = inode->i_private;
	char *buf;
	ssize_t n = 0;
	u8 *ext_csd;
	int err, i;

	buf = kmalloc(EXT_CSD_STR_LEN + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ext_csd = kmalloc(512, GFP_KERNEL);
	if (!ext_csd) {
		err = -ENOMEM;
		goto out_free;
	}

	mmc_claim_host(card->host);
	err = mmc_send_ext_csd(card, ext_csd);
	mmc_release_host(card->host);
	if (err)
		goto out_free;

	for (i = 0; i < 512; i++)
		n += sprintf(buf + n, "%02x", ext_csd[i]);
	n += sprintf(buf + n, "\n");
	BUG_ON(n != EXT_CSD_STR_LEN);

	filp->private_data = buf;
	kfree(ext_csd);
	return 0;

out_free:
	kfree(buf);
	kfree(ext_csd);
	return err;
}

static ssize_t mmc_ext_csd_read(struct file *filp, char __user *ubuf,
				size_t cnt, loff_t *ppos)
{
	char *buf = filp->private_data;

	return simple_read_from_buffer(ubuf, cnt, ppos,
				       buf, EXT_CSD_STR_LEN);
}

static int mmc_ext_csd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations mmc_dbg_ext_csd_fops = {
	.open		= mmc_ext_csd_open,
	.read		= mmc_ext_csd_read,
	.release	= mmc_ext_csd_release,
	.llseek		= default_llseek,
};


static int mmc_extcsd_show(struct seq_file *s, void *data)
{
	struct mmc_card		*card = s->private;
	struct mmc_ext_csd	ext_csd = card->ext_csd;	/* mmc v4 extended card specific */

	seq_printf(s,"\nExtended Card specific Info\n\n");
	seq_printf(s, "rev :\t\t%x \n", ext_csd.rev);
	seq_printf(s, "erase_group_def :\t\t%x \n", ext_csd.erase_group_def);
	seq_printf(s, "sec_feature_support :\t\t%d \n", ext_csd.sec_feature_support);
	seq_printf(s, "rel_sectors :\t\t%d \n", ext_csd.rel_sectors);
	seq_printf(s, "rel_param :\t\t%d \n", ext_csd.rel_param);
	seq_printf(s, "part_config :\t\t%d \n", ext_csd.part_config);
	seq_printf(s, "cache_ctrl :\t\t%d \n", ext_csd.cache_ctrl);
	seq_printf(s, "rst_n_function :\t\t%d \n", ext_csd.rst_n_function);
	seq_printf(s, "max_packed_writes :\t\t%d \n", ext_csd.max_packed_writes);
	seq_printf(s, "max_packed_reads :\t\t%d \n", ext_csd.max_packed_reads);
	seq_printf(s, "packed_event_en :\t\t%d \n", ext_csd.packed_event_en);
	seq_printf(s, "part_time :\t\t%d \n", ext_csd.part_time);
	seq_printf(s, "sa_timeout :\t\t%d \n", ext_csd.sa_timeout);
	seq_printf(s, "generic_cmd6_time :\t\t%d \n", ext_csd.generic_cmd6_time);
	seq_printf(s, "power_off_longtime :\t\t%d \n", ext_csd.power_off_longtime);
	seq_printf(s, "power_off_notification :\t\t%d \n", ext_csd.power_off_notification);
	seq_printf(s, "hs_max_dtr :\t\t%d \n", ext_csd.hs_max_dtr);
	seq_printf(s, "sectors :\t\t%d \n", ext_csd.sectors);
	seq_printf(s, "card_type :\t\t%d \n", ext_csd.card_type);
	seq_printf(s, "hc_erase_size :\t\t%d \n", ext_csd.hc_erase_size);
	seq_printf(s, "hc_erase_timeout :\t\t%d \n", ext_csd.hc_erase_timeout);
	seq_printf(s, "sec_trim_mult :\t\t%d \n", ext_csd.sec_trim_mult);
	seq_printf(s, "sec_erase_mult :\t\t%d \n", ext_csd.sec_erase_mult);
	seq_printf(s, "trim_timeout :\t\t%d \n", ext_csd.trim_timeout);
	seq_printf(s, "enhanced_area_en :\t\t%d \n", ext_csd.enhanced_area_en);
	seq_printf(s, "enhanced_area_offset :\t\t%lld \n", ext_csd.enhanced_area_offset);
	seq_printf(s, "enhanced_area_size :\t\t%d \n", ext_csd.enhanced_area_size);
	seq_printf(s, "cache_size :\t\t%d \n", ext_csd.cache_size);
	seq_printf(s, "hpi_en :\t\t%d \n", ext_csd.hpi_en);
	seq_printf(s, "hpi :\t\t%d \n", ext_csd.hpi);
	seq_printf(s, "hpi_cmd :\t\t%d \n", ext_csd.hpi_cmd);
	seq_printf(s, "bkops :\t\t%d \n", ext_csd.bkops);
	seq_printf(s, "bkops_en :\t\t%d \n", ext_csd.bkops_en);
	seq_printf(s, "data_sector_size :\t\t%d \n", ext_csd.data_sector_size);
	seq_printf(s, "data_tag_unit_size :\t\t%d \n", ext_csd.data_tag_unit_size);
	seq_printf(s, "boot_ro_lock :\t\t%d \n", ext_csd.boot_ro_lock);
	seq_printf(s, "boot_ro_lockable :\t\t%d \n", ext_csd.boot_ro_lockable);
	seq_printf(s, "raw_exception_status :\t\t%d \n", ext_csd.raw_exception_status);
	seq_printf(s, "raw_partition_support :\t\t%d \n", ext_csd.raw_partition_support);
	seq_printf(s, "raw_rpmb_size_mult :\t\t%d \n", ext_csd.raw_rpmb_size_mult);
	seq_printf(s, "raw_erased_mem_count :\t\t%d \n", ext_csd.raw_erased_mem_count);
	seq_printf(s, "raw_ext_csd_structure :\t\t%d \n", ext_csd.raw_ext_csd_structure);
	seq_printf(s, "raw_card_type :\t\t%d \n", ext_csd.raw_card_type);
	seq_printf(s, "out_of_int_time :\t\t%d \n", ext_csd.out_of_int_time);
	seq_printf(s, "raw_s_a_timeout :\t\t%d \n", ext_csd.raw_s_a_timeout);
	seq_printf(s, "raw_erase_timeout_mult :\t\t%d \n", ext_csd.raw_erase_timeout_mult);
	seq_printf(s, "raw_hc_erase_grp_size :\t\t%d \n", ext_csd.raw_hc_erase_grp_size);
	seq_printf(s, "raw_sec_trim_mult :\t\t%d \n", ext_csd.raw_sec_trim_mult);
	seq_printf(s, "raw_sec_erase_mult :\t\t%d \n", ext_csd.raw_sec_erase_mult);
	seq_printf(s, "raw_sec_feature_support :\t\t%d \n", ext_csd.raw_sec_feature_support);
	seq_printf(s, "raw_trim_mult :\t\t%d \n", ext_csd.raw_trim_mult);
	seq_printf(s, "raw_bkops_status :\t\t%d \n", ext_csd.raw_bkops_status);
	seq_printf(s, "raw_sectors[0] :\t\t%d \n", ext_csd.raw_sectors[0]);
	seq_printf(s, "raw_sectors[1] :\t\t%d \n", ext_csd.raw_sectors[1]);
	seq_printf(s, "raw_sectors[2] :\t\t%d \n", ext_csd.raw_sectors[2]);
	seq_printf(s, "raw_sectors[3] :\t\t%d \n", ext_csd.raw_sectors[3]);
	seq_printf(s, "feature_support:\t\t%d \n", ext_csd.feature_support);

	return 0;
}



static int mmc_cid_show(struct seq_file *s, void *data)
{
	struct mmc_card		*card = s->private;
	struct mmc_cid		cid = card->cid;		/* card identification */

	seq_printf(s, "\nCard identification  Info:\n\n");
	seq_printf(s,"manfid :\t%d \n", cid.manfid);
	seq_printf(s,"prod_name :\t%s \n", cid.prod_name);
	seq_printf(s,"prv :\t\t0x%x \n", cid.prv);
	seq_printf(s,"serial :\t%d \n", cid.serial);
	seq_printf(s,"oemid :\t\t%d \n", cid.oemid);
	seq_printf(s,"year :\t\t%d \n", cid.year);
	seq_printf(s,"hwrev :\t\t0x%x \n", cid.hwrev);
	seq_printf(s,"fwrev :\t\t0x%x \n", cid.fwrev); 
	seq_printf(s,"month :\t\t0x%x \n", cid.month);


	return 0;
}



static int mmc_csd_show(struct seq_file *s, void *data)
{
	struct mmc_card		*card = s->private;
	struct mmc_csd		csd = card->csd;		/* card specific */


    seq_printf(s, "\nCard specific Info\n\n");

	seq_printf(s, "structure :\t\t%x \n", csd.structure);
	seq_printf(s, "mmca_vsn :\t\t%x \n", csd.mmca_vsn);
	seq_printf(s, "cmdclass :\t\t%d \n", csd.cmdclass);
	seq_printf(s, "tacc_clks :\t\t%d \n", csd.tacc_clks);
	seq_printf(s, "tacc_ns :\t\t%d \n", csd.tacc_ns);
	seq_printf(s, "c_size :\t\t%d \n", csd.c_size);
	seq_printf(s, "r2w_factor :\t\t%d \n", csd.r2w_factor);
	seq_printf(s, "max_dtr :\t\t%d\n", csd.max_dtr); 
	seq_printf(s, "erase_size :\t\t%d \n", csd.erase_size);
	seq_printf(s, "read_blkbits :\t\t%d \n", csd.read_blkbits);
	seq_printf(s, "write_blkbits :\t\t%d \n", csd.write_blkbits);
	seq_printf(s, "capacity :\t\t%d \n", csd.capacity);
	seq_printf(s, "read_partial :\t\t%x \n", csd.read_partial);
	seq_printf(s, "read_misalign :\t\t%x \n", csd.read_misalign);
	seq_printf(s, "write_partial :\t\t%x \n", csd.write_partial);
	seq_printf(s, "write_misalign :\t%x \n", csd.write_misalign);


	return 0;
}




static int mmc_cid_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_cid_show, inode->i_private);
}

static const struct file_operations mmc_dbg_cid_fops = {
	.open		= mmc_cid_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int mmc_csd_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_csd_show, inode->i_private);
}

static const struct file_operations mmc_dbg_csd_fops = {
	.open		= mmc_csd_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mmc_extcsd_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmc_extcsd_show, inode->i_private);
}

static const struct file_operations mmc_dbg_extcsd_fops = {
	.open		= mmc_extcsd_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};



void mmc_add_card_debugfs(struct mmc_card *card)
{
	struct mmc_host	*host = card->host;
	struct dentry	*root;

	if (!host->debugfs_root)
		return;

	root = debugfs_create_dir(mmc_card_id(card), host->debugfs_root);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err;

	card->debugfs_root = root;

	if (!debugfs_create_x32("state", S_IRUSR, root, &card->state))
		goto err;

	if (mmc_card_mmc(card) || mmc_card_sd(card))
		if (!debugfs_create_file("status", S_IRUSR, root, card,
					&mmc_dbg_card_status_fops))
			goto err;

	if (mmc_card_mmc(card))
		if (!debugfs_create_file("ext_csd", S_IRUSR, root, card,
					&mmc_dbg_ext_csd_fops))
			goto err;

	if (mmc_card_mmc(card)|| mmc_card_sd(card))
	if (!debugfs_create_file("cid", S_IRUSR, root, card,
				&mmc_dbg_cid_fops))
		goto err;

	if (mmc_card_mmc(card)|| mmc_card_sd(card))
	if (!debugfs_create_file("csd", S_IRUSR, root, card,
				&mmc_dbg_csd_fops))
		goto err;
	if (mmc_card_mmc(card)|| mmc_card_sd(card))
	if (!debugfs_create_file("extcsd", S_IRUSR, root, card,
				&mmc_dbg_extcsd_fops))
		goto err;

	return;

err:
	debugfs_remove_recursive(root);
	card->debugfs_root = NULL;
	dev_err(&card->dev, "failed to initialize debugfs\n");
}

void mmc_remove_card_debugfs(struct mmc_card *card)
{
	debugfs_remove_recursive(card->debugfs_root);
}
