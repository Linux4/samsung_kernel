/*
 * Marvell Camera Debug interface
 *
 * Copyright (c) 2012 Marvell Ltd.
 * Jiaquan Su <jqsu@marvell.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/hardirq.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <media/mrvl-camera.h>
#include <media/mc_debug.h>
#include <media/soc_camera.h>
#include <linux/fs.h>
#include <media/videobuf2-dma-contig.h>

static int default_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t default_read(struct file *file,
				     char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value tmp, *value = file->private_data;
	long prod, msec, usec;

	mcd_value_dup(value, &tmp);
	msec = (tmp.ts_end.tv_sec - tmp.ts_start.tv_sec) * MSEC_PER_SEC;
	usec = tmp.ts_end.tv_usec - tmp.ts_start.tv_usec;
	if (usec < 0) {
		usec += USEC_PER_SEC;
		msec -= MSEC_PER_SEC;
	}
	msec += (usec + USEC_PER_MSEC/2) / USEC_PER_MSEC;
	if (msec == 0) {
		printk(KERN_INFO "%s: %d\n", tmp.name, tmp.cache);
		return 0;
	}
	prod = tmp.cache*MSEC_PER_SEC;
	printk(KERN_INFO "%s: %d cps=%ld.%ld\n", tmp.name, tmp.cache, \
			prod/msec, (prod-prod/msec*msec)*100/msec);
	return 0;
}

static ssize_t default_write(struct file *file,
				     const char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	char buf[STRING_LENGTH] = {0};
	int i, min_size;

	min_size = min_t(char, sizeof(buf) - 1, count);
	if (copy_from_user(buf, userbuf, min_size))
		return -EFAULT;
	if (sscanf(buf, "%d", &i) != 1)
		i = 0;

	mcd_value_set(value, i);
	printk(KERN_INFO "mcd: set value %d\n", i);
	return -1;
}

static const struct file_operations default_fop = {
	.open	= default_open,
	.read	= default_read,
	.write	= default_write,
};

static ssize_t sensor_setting_read(struct file *file,
				     char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	printk(KERN_INFO "usage:\n"
		"echo r 0x1234 > %s:\t\tread reg 0x1234\n"
		"echo w 0x1234 0xAB> %s:\twrite reg 0x1234 = 0xab\n"
		"echo r file > %s:\t\tread reg list from file\n"
		"echo w file > %s:\t\twrite reg list in file\n",
		value->name, value->name, value->name, value->name);
	return 0;
}

static int sensor_setting_fileop(struct v4l2_subdev *sd, char *name, int op)
{
	struct file *file;
	mm_segment_t old_fs;
	int ret = 0, i, size, offset;
	char *buf = NULL, tmp[STRING_LENGTH];

	if (sd == NULL || name == NULL
		|| strlen(name) == 0 || strlen(name) >= STRING_LENGTH - 4)
		return -EINVAL;
	file = filp_open(name, O_RDONLY | O_LARGEFILE, 0);

	if (IS_ERR(file)) {
		ret = PTR_ERR(file);
		printk(KERN_ERR "mcd: can't open sensor file '%s'\n", name);
		goto exit;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	size = file->f_dentry->d_inode->i_size;
	if (size == 0) {
		set_fs(old_fs);
		filp_close(file, NULL);
		printk(KERN_ERR "mcd: input file '%s' is empty\n", name);
		ret = -EINVAL;
		goto exit;
	}
	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL) {
		ret = -ENOMEM;
		goto exit_ifile;
	}
	if (vfs_read(file, buf, size, &file->f_pos) != size) {
		ret = -EIO;
		goto exit_ifile;
	}
exit_ifile:
	/* file data is already in the buffer now, release file file */
	set_fs(old_fs);
	filp_close(file, NULL);

	if (op == 0) {	/* if read reg list, then creat a file for output */
		strcpy(tmp, name);
		strcat(tmp, ".out");
		file = filp_open(tmp, O_WRONLY | O_CREAT | O_LARGEFILE, 0);
		if (IS_ERR(file)) {
			ret = PTR_ERR(file);
			printk(KERN_ERR "cam: can't open output file '%s'\n", \
				tmp);
			goto exit_buf;
		}
		printk(KERN_INFO "mcd: register values will be saved in '%s'\n"\
			, tmp);
		old_fs = get_fs();
		set_fs(KERNEL_DS);
	}

	i = 0;
	offset = 0;
	while (1) {
		int len, addr, value;
		struct v4l2_dbg_register reg;

		if (buf[offset] == 0)
			break;
		len = 0;
		while (buf[offset + len] != '\n') {
			tmp[len] = buf[offset+len];
			len++;
		}
		tmp[len++] = 0;

		if (op == 0) {
			if (sscanf(tmp, "%x", &addr) < 1)
				goto next_item;
			reg.reg = addr;
			reg.val = 0;
			ret = v4l2_subdev_call(sd, core, g_register, &reg);
			i++;
		} else {
			if (sscanf(tmp, "{%x,%x}", &addr, &value) < 2)
				goto next_item;
			reg.reg = addr;
			reg.val = value;
			ret = v4l2_subdev_call(sd, core, s_register, &reg);
			i++;
		}

		if (ret < 0) {
			printk(KERN_ERR "mcd: failed processing item%d "\
				"'%s'\n", i, tmp);
			goto exit_ofile;
		}

		printk(KERN_INFO "{0x%04X, 0x%02X}\n", \
				(u32)reg.reg, (u8)reg.val);
		if (op == 0) {
			int osize;
			sprintf(tmp, "{%04X, %02X}\n", \
				(u32)reg.reg, (u8)reg.val);
			osize = strlen(tmp);
			if (vfs_write(file, tmp, osize, &file->f_pos)
				!= osize) {
				printk(KERN_ERR "mcd: failed dump to file\n");
				ret = -EIO;
				goto exit_ofile;
			}
		}
next_item:
		offset += len;
	}
	printk(KERN_INFO "mcd: %d item processed\n", i);

exit_ofile:
	if (op == 0) {
		set_fs(old_fs);
		filp_close(file, NULL);
	}
exit_buf:
	kfree(buf);
exit:
	return ret;
}

static ssize_t sensor_setting_write(struct file *file,
				     const char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	struct v4l2_subdev *sd = value->priv;
	struct v4l2_dbg_register reg;
	char tag, buf[STRING_LENGTH] = {0}, name[STRING_LENGTH] = {0};
	int i = 0, min_size, ret;

	min_size = min_t(char, sizeof(buf) - 1, count);
	if (copy_from_user(buf, userbuf, min_size))
		return -EFAULT;
	if (sscanf(buf, "%s", name) != 1)
		goto fmt_err;
	switch (name[0]) {
	case 'r':
	case 'R':
		tag = 0;
		break;
	case 'w':
	case 'W':
		tag = 1;
		break;
	default:
		goto fmt_err;
	}
	min_size = strlen(name) + 1;
	if (sscanf(buf + min_size, "%X %X", &i, &ret) >= 1)
		goto parse;
	if (sscanf(buf + min_size, "%s", name) == 1) {
		tag += 2;
		goto parse;
	}
	goto fmt_err;
parse:
#ifdef CONFIG_VIDEO_ADV_DEBUG
	switch (tag) {
	case 0:
		reg.reg = i;
		ret = v4l2_subdev_call(sd, core, g_register, &reg);
		if (ret < 0) {
			printk(KERN_ERR "read failed with err = %d\n", ret);
			return -EAGAIN;
		}
		printk(KERN_INFO "read reg 0x%X = 0x%X\n", \
			(u32)reg.reg, (u8)reg.val);
		break;
	case 1:
		reg.reg = i;
		reg.val = ret;
		ret = v4l2_subdev_call(sd, core, s_register, &reg);
		if (ret < 0) {
			printk(KERN_ERR "write failed with err = %d\n", ret);
			return -EAGAIN;
		}
		v4l2_subdev_call(sd, core, g_register, &reg);
		printk(KERN_INFO "write reg 0x%X = 0x%X\n", \
			(u32)reg.reg, (u8)reg.val);
		break;
	case 2:
		sensor_setting_fileop(sd, name, 0);
		break;
	case 3:
		sensor_setting_fileop(sd, name, 1);
		break;
	}
#endif
	return -1;

fmt_err:
	/* print manual */
	printk(KERN_INFO "usage:\n"
		"echo r 0x1234 > %s:\t\tread reg 0x1234\n"
		"echo w 0x1234 0xAB> %s:\twrite reg 0x1234 = 0xab\n"
		"echo r file > %s:\t\tread reg list from file\n"
		"echo w file > %s:\t\twrite reg list in file\n",
		value->name, value->name, value->name, value->name);
	return -1;
}

static const struct file_operations sensor_setting_fop = {
	.open	= default_open,
	.read	= sensor_setting_read,
	.write	= sensor_setting_write,
};

static ssize_t dphy_obs_read(struct file *file,
				     char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	struct mcd_dphy_hw *hw = value->priv;
	struct csi_dphy_desc *obs = &hw->obs;

	printk(KERN_INFO
		"cfc (MIPI clock lane frequency): %dMHz\n"
		"dhp (MIPI data lane HS-PREPARE duration): %dns\n"
		"dhz (MIPI data lane HS-ZERO duration): %dns\n"
		"lane (number of MIPI data lane): %d\n",
		obs->clk_freq, obs->hs_prepare, obs->hs_zero, obs->nr_lane);
	return 0;
}

static ssize_t dphy_obs_write(struct file *file,
				     const char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	struct mcd_dphy_hw *hw = value->priv;
	struct csi_dphy_desc *obs = &hw->obs;
	char buf[STRING_LENGTH] = {0}, name[STRING_LENGTH] = {0};
	int size = 0, ret;

	size = min_t(char, sizeof(buf) - 1, count);
	if (copy_from_user(buf, userbuf, size))
		return -EFAULT;
	if (sscanf(buf, "%s %d", name, &ret) != 2)
		goto fmt_err;

	if (!strcmp(name, "cfc")) {
		if (ret <= 0)
			goto fmt_err;
		obs->clk_freq = ret;
		goto done;
	}
	if (!strcmp(name, "dhp")) {
		if (ret <= 0)
			goto fmt_err;
		obs->hs_prepare = ret;
		goto done;
	}
	if (!strcmp(name, "dhz")) {
		if (ret <= 0)
			goto fmt_err;
		obs->hs_zero = ret;
		goto done;
	}
	if (!strcmp(name, "lane")) {
		if (ret <= 0)
			goto fmt_err;
		obs->nr_lane = ret;
		goto done;
	}
done:
	return -1;
fmt_err:
	return -1;
}

static const struct file_operations dphy_obs_fop = {
	.open	= default_open,
	.read	= dphy_obs_read,
	.write	= dphy_obs_write,
};

int dphy_calc_reg(struct csi_dphy_desc *pdesc, \
		struct csi_dphy_calc *algo, struct csi_dphy_reg *preg)
{
	u32 ps_period, ps_ui, ps_termen_max, ps_prep_max, ps_prep_min;
	u32 ps_sot_min, ps_termen, ps_settle;
#if 0	/* since these value is not filled by driver, the following code can't
	 * be activated yet */
	if (pdesc->clk_freq == 0)
		pdesc->clk_freq = mclk * pdesc->clk_mul / pdesc->clk_div;
#endif
	ps_period = 1000000 / pdesc->clk_freq;
	ps_ui = ps_period / 2;
	ps_termen_max	= 35000 + 4 * ps_ui;
	ps_prep_min	= 40000 + 4 * ps_ui;
	ps_prep_max	= 85000 + 6 * ps_ui;
	ps_sot_min	= 145000 + 10 * ps_ui;
	ps_termen	= ps_termen_max + algo->hs_termen_pos * ps_period;
	ps_settle	= 1000 * (pdesc->hs_prepare + pdesc->hs_zero * \
						algo->hs_settle_pos / 100);

	preg->cl_termen	= 0x00;
	preg->cl_settle	= 0x04;
	preg->cl_miss	= 0x00;
	/* term_en = round_up(ps_termen / ps_period) - 1 */
	preg->hs_termen	= (ps_termen + ps_period - 1) / ps_period - 1;
	/* For Marvell DPHY, Ths-settle started from HS-0, not VILmax */
	ps_settle -= (preg->hs_termen + 1) * ps_period;
	/* round_up(ps_settle / ps_period) - 1 */
	preg->hs_settle = (ps_settle + ps_period - 1) / ps_period - 1;
	preg->hs_rx_to	= 0xFFFF;
	preg->lane	= pdesc->nr_lane;
	return 0;
}

enum {
	CALC_SAFE,
	CALC_OPT,
	CALC_ENUM,
	CALC_SCAN,
};

struct csi_dphy_calc dphy_calc_profiles[] = {
	[CALC_SAFE] = {
		.name		= "safe",
		.hs_termen_pos	= 0,
		.hs_settle_pos	= 50,
	},
	[CALC_OPT] = {
		.name		= "opt",
		.hs_termen_pos	= -1,
		.hs_settle_pos	= 50,
	},
	[CALC_ENUM] = {
		.name		= "enum",
	},
	[CALC_SCAN] = {
		.name		= "scan",
	},
};

int enum_dphy(struct mcd_dphy_hw *hw, struct csi_dphy_reg *preg,
		struct mcd_value *eof,
		int te_min, int te_max, int st_min, int st_max)
{
	int i, j;
	int cnt;

	for (i = te_min; i <= te_max; i++) {
		for (j = st_min; j <= st_max; j++) {
			preg->hs_termen = i;
			preg->hs_settle = j;
			mcd_value_set(eof, 0);
			(*hw->reg_write)(hw->hw_ctx, preg);
			msleep(1000);
			cnt = mcd_value_read(eof);
			printk(KERN_INFO "termen = 0x%02X, settle = 0x%02X, "\
				"fps = %d\n", i, j, cnt);
		}
	}
	return 0;
}

int scan_dphy(struct mcd_dphy_hw *hw, struct csi_dphy_reg *preg,
		struct mcd_value *eof,
		int te_min, int te_max, int st_min, int st_max)
{
	int i, j, lst = 0, ever_succ = 0;
	int cnt, max_cnt = 15;	/* target FPS */

	for (i = te_min; i <= te_max; i++) {
		preg->hs_termen = i;
		j = st_min;
		lst = -1;	/* last result unknown */
		while (1) {
			preg->hs_settle = j;
			mcd_value_set(eof, 0);
			(*hw->reg_write)(hw->hw_ctx, preg);
			msleep(1000);
			cnt = mcd_value_read(eof);
			printk(KERN_INFO "termen = 0x%02X, settle = 0x%02X, "\
				"fps = %d\n", i, j, cnt);
			if (cnt > max_cnt)
				max_cnt = cnt;
			if (cnt >= max_cnt/2) {
				/* if succ this loop */
				st_min = j;
				j--;
				if (j < 0 || lst == 0)
					break;
				lst = 1;	/* mark succ */
			} else {
				/* if failed this loop */
				j++;
				if (j >= 0xFF) {
					st_min = st_max + 1;
					break;
				}
				lst = 0;	/* mark fail */
			}
		}

		j = st_max;
		lst = -1;	/* last result unknown */
		while (1) {
			preg->hs_settle = j;
			mcd_value_set(eof, 0);
			(*hw->reg_write)(hw->hw_ctx, preg);
			msleep(1000);
			cnt = mcd_value_read(eof);
			printk(KERN_INFO "termen = 0x%02X, settle = 0x%02X, "\
				"fps = %d\n", i, j, cnt);
			if (cnt > max_cnt)
				max_cnt = cnt;
			if (cnt >= max_cnt/2) {
				/* if succ this loop */
				st_max = j;
				j++;
				if (j > 0xFF || lst == 0)
					break;
				lst = 1;	/* mark succ */
			} else {
				/* if failed this loop */
				j--;
				if (j < 0) {
					st_max = st_min - 1;
					break;
				}
				lst = 0;	/* mark fail */
			}
		}
		if (st_max < st_min) {
			if (ever_succ)
				break;
			else {
				/* no available value found, so can't abort */
				/* reset search border */
				st_max = st_min = (st_max + st_min) / 2;
			}
		} else
			ever_succ = 1;
		printk(KERN_INFO "available value: when termen = 0x%02X, "\
			"settle = [0x%02X:0x%02X]\n", i, st_min, st_max);
	}
	return 0;
}

int dphy_calc_reg_auto(struct mcd *pmcd, int profile)
{
	struct mcd_dma *pdma = container_of(pmcd->pentity[MCD_DMA], \
						struct mcd_dma, entity);
	struct mcd_dphy *pdphy = container_of(pmcd->pentity[MCD_DPHY], \
						struct mcd_dphy, entity);
	struct mcd_value *mcd_dphy_reg = pdphy->entity.value + MCD_DPHY_REG;
	struct mcd_dphy_hw *hw = pdphy->entity.value[MCD_DPHY_CAL].priv;
	struct csi_dphy_desc *pdesc = &hw->obs;
	struct csi_dphy_reg *preg = &hw->reg;
	int te_min, te_max, st_min, st_max;

	if (pdesc->nr_lane == 0) {
		printk(KERN_ERR "mcd: dphy: please specify number of MIPI data");
		return -EPERM;
	}
	preg->lane = pdesc->nr_lane;
#if 0	/* since these value is not filled by driver, the following code can't
	 * be activated yet */
	if (pdesc->clk_freq == 0)
		pdesc->clk_freq = mclk * pdesc->clk_mul / pdesc->clk_div;
#endif
	if (pdesc->clk_freq == 0) {
		te_min = 0;
		te_max = 0xFF;
		st_min = 0;
		st_max = 0xFF;
	} else {
		u32 ps_period, ps_ui, ps_termen_max;
		ps_period = 1000000 / pdesc->clk_freq;
		ps_ui = ps_period / 2;
		ps_termen_max = 35000 + 4 * ps_ui;
		te_min = (ps_termen_max + ps_period - 1) / ps_period - 1;
		te_max = te_min + 1;
		te_min = te_min - 1;
		st_min = 0;
		st_max = 0xFF;
	}
	if (preg->hs_termen != 0)
		te_min = te_max = preg->hs_termen;

	if (preg->hs_settle != 0) {
		st_min = 0;
		st_max = preg->hs_settle;
	}

	mcd_dphy_reg->cache = 1;
	switch (profile) {	/* pdphy->entity.value[MCD_DPHY_CAL].cache */
	case CALC_ENUM:
		enum_dphy(hw, preg, pdma->values + MCD_DMA_EOF, \
				te_min, te_max, st_min, st_max);
		break;
	case CALC_SCAN:
		scan_dphy(hw, preg, pdma->values + MCD_DMA_EOF, \
				te_min, te_max, st_min, 0x40);
		/* FIXME: 0x40 is a hard coding to save time,
		 * st_max is the reasonable value */
		break;
	default:
		return -EINVAL;
	}
	mcd_dphy_reg->cache = 0;

	return 0;
}

static ssize_t dphy_cal_read(struct file *file,
				     char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;

	if (value->cache < 0 || value->cache >= CALC_ENUM) {
		printk("calculation profile undefined\n");
		return 0;
	}
	printk(KERN_INFO "used profile '%s'\n", \
		dphy_calc_profiles[value->cache].name);
	return 0;
}

static ssize_t dphy_cal_write(struct file *file,
				     const char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	struct mcd_dphy_hw *hw = value->priv;
	struct csi_dphy_desc *pobs = &hw->obs;
	struct csi_dphy_reg *preg = &hw->reg;
	char buf[STRING_LENGTH] = {0}, name[STRING_LENGTH] = {0};
	int size = 0, i;

	size = min_t(char, sizeof(buf) - 1, count);
	if (copy_from_user(buf, userbuf, size))
		return -EFAULT;
	size = 0;
	if (sscanf(buf + size, "%s", name) < 1)
		goto fmt_err;

	for (i = 0; i < ARRAY_SIZE(dphy_calc_profiles); i++) {
		if (!strcmp(name, dphy_calc_profiles[i].name)) {
			value->cache = i;
			goto calc;
		}
	}
	value->cache = -1;
	goto fmt_err;
calc:
	if (i == CALC_ENUM || i == CALC_SCAN) {
		struct mcd_entity *dphy = container_of(value, \
					struct mcd_entity, value[MCD_DPHY_CAL]);
		struct mcd *pmcd = dphy->pmcd;
		dphy_calc_reg_auto(pmcd, i);
	} else {
		printk(KERN_INFO "start calculation with termen_pos: %d, "\
			"settle: %d\n",	dphy_calc_profiles[i].hs_termen_pos,
					dphy_calc_profiles[i].hs_settle_pos);
		dphy_calc_reg(pobs, dphy_calc_profiles + i, preg);
	}
	return -1;
fmt_err:
	return -1;
}

static const struct file_operations dphy_cal_fop = {
	.open	= default_open,
	.read	= dphy_cal_read,
	.write	= dphy_cal_write,
};

static ssize_t dphy_reg_read(struct file *file,
				     char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	struct mcd_dphy_hw *hw = value->priv;
	struct csi_dphy_reg *preg = &hw->reg;

	printk(KERN_INFO "MCD: overwrite MIPI DPHY register: %s\n"
		"cl_termen: 0x%04X hs_termen: 0x%04X\n"
		"cl_settle: 0x%04X hs_settle: 0x%04X\n"
		"cl_miss:   0x%04X hs_rx_to:  0x%04X\n"
		"lane: %d\n\n",
		(value->cache) ? "enabled" : "disabled",
		preg->cl_termen, preg->hs_termen,
		preg->cl_settle, preg->hs_settle,
		preg->cl_miss, preg->hs_rx_to, preg->lane);
	return 0;
}

static ssize_t dphy_reg_write(struct file *file,
				     const char __user *userbuf,
				     size_t count, loff_t *ppos)
{
	struct mcd_value *value = file->private_data;
	struct mcd_dphy_hw *hw = value->priv;
	struct csi_dphy_reg *preg = &hw->reg;
	char buf[STRING_LENGTH] = {0}, name[STRING_LENGTH] = {0};
	int size = 0, ret;

	size = min_t(char, sizeof(buf) - 1, count);
	if (copy_from_user(buf, userbuf, size))
		return -EFAULT;
	size = 0;
	if (sscanf(buf + size, "%s %x", name, &ret) < 1)
		goto fmt_err;

	if (!strcmp(name, "enable")) {
		value->cache = 1;
		goto done;
	}

	if (!strcmp(name, "disable")) {
		value->cache = 0;
		goto done;
	}

	if (!strcmp(name, "write")) {
		if (hw->reg_write)
			(*hw->reg_write)(hw->hw_ctx, preg);
		else
			printk(KERN_ERR "MCD: PHY: setting write function "\
				"not implemented\n");
		goto done;
	}

	if (!strcmp(name, "read")) {
		if (hw->reg_read) {
			(*hw->reg_read)(hw->hw_ctx, preg);
		printk(KERN_INFO "mcd: dphy: current setting in register:\n" \
			"------------------------------------------\n" \
			"\tCL_TERMEN = 0x%04X\n" \
			"\tCL_SETTLE = 0x%04X\n" \
			"\tCL_MISS   = 0x%04X\n" \
			"\tHS_TERMEN = 0x%04X\n" \
			"\tHS_SETTLE = 0x%04X\n" \
			"\tHS_RX_TO  = 0x%04X\n" \
			"\tNR_LANE   = %d\n" \
			"------------------------------------------\n", \
			preg->cl_termen, preg->cl_settle, preg->cl_miss, \
			preg->hs_termen, preg->hs_settle, preg->hs_rx_to, \
			preg->lane);
		} else
			printk(KERN_ERR "MCD: PHY: setting read function "\
				"not implemented\n");
		goto done;
	}

	if (!strcmp(name, "cl_termen")) {
		if (ret < 0)
			goto fmt_err;
		preg->cl_termen = ret;
		goto done;
	}

	if (!strcmp(name, "cl_settle")) {
		if (ret < 0)
			goto fmt_err;
		preg->cl_settle = ret;
		goto done;
	}

	if (!strcmp(name, "cl_miss")) {
		if (ret < 0)
			goto fmt_err;
		preg->cl_miss = ret;
		goto done;
	}

	if (!strcmp(name, "hs_termen")) {
		if (ret < 0)
			goto fmt_err;
		preg->hs_termen = ret;
		goto done;
	}

	if (!strcmp(name, "hs_settle")) {
		if (ret < 0)
			goto fmt_err;
		preg->hs_settle = ret;
		goto done;
	}

	if (!strcmp(name, "hs_rx_to")) {
		if (ret < 0)
			goto fmt_err;
		preg->hs_rx_to = ret;
		goto done;
	}

	if (!strcmp(name, "lane")) {
		if (ret < 0)
			goto fmt_err;
		preg->lane = ret;
		goto done;
	}

done:
	return -1;
fmt_err:
	return -1;
}

static const struct file_operations dphy_reg_fop = {
	.open	= default_open,
	.read	= dphy_reg_read,
	.write	= dphy_reg_write,
};

struct mcd_sensor default_mcd_sensor = {
	.entity = {
		.name		= "mcd_sensor",
		.type		= MCD_SENSOR,
		.nr_value	= MCD_SENSOR_END,
	},
	.values = {
		[MCD_SENSOR_SET] = {
			.name = "setting",
			.fops = &sensor_setting_fop,
		},
	},
};

struct mcd_dphy default_mcd_dphy = {
	.entity = {
		.name		= "mcd_dphy",
		.type		= MCD_DPHY,
		.nr_value	= MCD_DPHY_END,
	},
	.values = {
		[MCD_DPHY_OBS] = {
			.name	= "observed",
			.fops	= &dphy_obs_fop,
		},
		[MCD_DPHY_CAL] = {
			.name	= "calculator",
			.cache	= -1,
			.fops	= &dphy_cal_fop,
		},
		[MCD_DPHY_REG] = {
			.name	= "register",
			.fops	= &dphy_reg_fop,
		},
	},
};

struct mcd_dma default_mcd_dma = {
	.entity = {
		.name		= "mcd_dma_engine",
		.type		= MCD_DMA,
		.nr_value	= MCD_DMA_END,
	},
	.values = {
		[MCD_DMA_SOF]		= {.name = "sof"},
		[MCD_DMA_EOF]		= {.name = "eof"},
		[MCD_DMA_OVERFLOW]	= {.name = "overflow"},
		[MCD_DMA_DROP_FRAME]	= {.name = "framedrop"},
	},
};

struct mcd_vdev default_mcd_vdev = {
	.entity = {
		.name		= "mcd_vdev",
		.type		= MCD_VDEV,
		.nr_value	= MCD_VDEV_END,
	},
	.values = {
		[MCD_VDEV_REG]		= {.name = "registered"},
		[MCD_VDEV_ACT]		= {.name = "opened"},
		[MCD_VDEV_FMT]		= {.name = "format"},
		[MCD_VDEV_STREAM]	= {.name = "streaming"},
		[MCD_VDEV_QBUF]		= {.name = "qbuf"},
		[MCD_VDEV_DQBUF]	= {.name = "dqbuf"},
		[MCD_VDEV_DUMP]		= {.name = "dump"},
	},
};

static inline int mcd_value_init(struct mcd_value *value, struct dentry *parent)
{
	if (value == NULL)
		return -EINVAL;
	if (strlen(value->name) == 0)
		return -EPERM;

	if (value->fops == NULL)
		value->fops = &default_fop;

	value->dbgfs = debugfs_create_file(value->name, 0600, parent, NULL, \
						value->fops);
	if (IS_ERR(value->dbgfs)) {
		int err = PTR_ERR(value->dbgfs);
		value->dbgfs = NULL;
		return err;
	}
	value->dbgfs->d_inode->i_private = value;

	/* Invalidate start and end TS */
	memset(&value->ts_start, -1, sizeof(struct timeval));
	memset(&value->ts_end, -1, sizeof(struct timeval));
	value->cache = 0;
	spin_lock_init(&value->lock);
	return 0;
}

void mcd_value_remove(struct mcd_value *value)
{
	debugfs_remove(value->dbgfs);
	memset(value, 0, sizeof(struct mcd_value));
}

void mcd_value_peg(struct mcd_value *value, int inc)
{
	unsigned long flag;
	struct timeval ts;

	do_gettimeofday(&ts);

	spin_lock_irqsave(&value->lock, flag);
	value->cache += inc;
	memcpy(&value->ts_end, &ts, sizeof(struct timeval));
	/* if start TS is invalid, update it */
	if (value->ts_start.tv_sec < 0)
		memcpy(&value->ts_start, &ts, sizeof(struct timeval));
	spin_unlock_irqrestore(&value->lock, flag);
}

/* initialize value */
void mcd_value_set(struct mcd_value *value, int init)
{
	unsigned long flag;

	spin_lock_irqsave(&value->lock, flag);
	value->cache = init;
	/* Invalidate start and end TS */
	memset(&value->ts_start, -1, sizeof(struct timeval));
	memset(&value->ts_end, -1, sizeof(struct timeval));
	spin_unlock_irqrestore(&value->lock, flag);
}
/* initialize value and update TS */
void mcd_value_write(struct mcd_value *value, int init)
{
	unsigned long flag;
	struct timeval ts;

	do_gettimeofday(&ts);

	spin_lock_irqsave(&value->lock, flag);
	value->cache = init;
	memcpy(&value->ts_end, &ts, sizeof(struct timeval));
	/* if start TS is invalid, update it */
	if (value->ts_start.tv_sec < 0)
		memcpy(&value->ts_start, &ts, sizeof(struct timeval));
	spin_unlock_irqrestore(&value->lock, flag);
}

int mcd_value_read(struct mcd_value *value)
{
	unsigned long flag;
	int tmp;

	spin_lock_irqsave(&value->lock, flag);
	tmp = value->cache;
	spin_unlock_irqrestore(&value->lock, flag);

	return tmp;
}

int mcd_value_dup(struct mcd_value *value, struct mcd_value *dst)
{
	unsigned long flag;

	if (unlikely(value == NULL || dst == NULL))
		return -EINVAL;

	spin_lock_irqsave(&value->lock, flag);
	memcpy(dst, value, sizeof(struct mcd_value));
	spin_unlock_irqrestore(&value->lock, flag);
	return 0;
}

int mcd_entity_init(struct mcd_entity *entity, struct mcd *pmcd)
{
	int i;

	if (entity == NULL)
		return -EINVAL;
	if (strlen(entity->name) == 0)
		return -EPERM;

	if (entity->dbgfs != NULL)
		return -EBUSY;

	entity->dbgfs = debugfs_create_dir(entity->name, pmcd->dbgfs);
	if (IS_ERR(entity->dbgfs)) {
		int err = PTR_ERR(entity->dbgfs);
		entity->dbgfs = NULL;
		return err;
	}
	entity->pmcd = pmcd;

	for (i = 0; i < entity->nr_value; i++) {
		int ret;
		if (entity->value[i].priv == NULL)
			entity->value[i].priv = entity->priv;
		ret = mcd_value_init(entity->value + i, entity->dbgfs);
		if (ret < 0)
			return ret;
	}
	return 0;
}

void mcd_entity_remove(struct mcd_entity *entity)
{
	int i;

	for (i = 0; i < entity->nr_value; i++)
		mcd_value_remove(entity->value + i);
	if (entity->dbgfs != NULL)
		debugfs_remove(entity->dbgfs);
	memset(entity, 0, sizeof(struct mcd_entity));
}

int mcd_init(struct mcd *mcd)
{
	if (mcd == NULL)
		return -EINVAL;
	if (strlen(mcd->name) == 0)
		return -EPERM;

	mcd->dbgfs = debugfs_create_dir(mcd->name, NULL);
	if (IS_ERR(mcd->dbgfs)) {
		int err = PTR_ERR(mcd->dbgfs);
		mcd->dbgfs = NULL;
		return err;
	}

	mcd->log_next = 0;
	mcd->parse_next = -1;

	return 0;
}

#if 0 /* don't use now, save for future */
int mcd_parse_log(struct mcd *mcd, int user_trigger)
{
	int pstart, pend, i;

	switch (mcd->parse_flag & MCD_PARSE_CTX_MASK) {
	case MCD_PARSE_USERACT:
		if (!user_trigger)
			return 0;
	case MCD_PARSE_PROCESS:
		if (in_interrupt())
			return 0;
	case MCD_PARSE_REALTIME:
		break;
	default:
		return -EPERM;
	}
	/* If parse pointer is out-of-date, we should parse the entire log */
	if (mcd->parse_next < 0)
		mcd->parse_next = mcd->log_next;
	pstart = mcd->parse_next;
	pend = LOOP_ADD(mcd->parse_next, -1);
	/* Keep in mind that these code can be interrupt by IRQ anytime
	 * so log_* is volitile, parse_* is stable */
	for (i = pstart; i < pend; i = LOOP_ADD(i, 1)) {
		struct mcd_log log, *plog = mcd->log_pool + i;
		struct mcd_entity *pent;
		/* acquire lock and copy here */
		memcpy(&log, plog, sizeof(struct mcd_log));
		/* release lock here */
		if (log.entity < mcd->nr_entity)
			pent = mcd->entity + log.entity;
		else
			return -EINVAL;

		/* call peg function of each value here, peg(&log, value) */
	}
	mcd->parse_next = pend;
	return LOOP_ADD(pend, -pstart);
}
#endif

int vb_dump_block(struct vb2_buffer *vb2, char *name)
{
	struct file *dumpfile;
	mm_segment_t old_fs;
	int ret = 0, i;

	if (vb2 == NULL || name == NULL || strlen(name) == 0)
		return -EINVAL;
	dumpfile = filp_open(name, O_RDWR | O_LARGEFILE|O_CREAT, 0);
	if (IS_ERR(dumpfile)) {
		ret = PTR_ERR(dumpfile);
		printk(KERN_ERR "cam: can't open dumpfile '%s'\n", name);
		return ret;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	for (i = 0; i < vb2->num_planes; i++) {
		int plane_size = vb2->v4l2_planes[i].bytesused;
		void *plane_addr = vb2_plane_vaddr(vb2, i);

		if (plane_addr == NULL)
			plane_addr = phys_to_virt(
					vb2_dma_contig_plane_dma_addr(vb2, i));
		ret = dumpfile->f_op->write(dumpfile, plane_addr, plane_size,
						&dumpfile->f_pos);
		if (ret != plane_size) {
			printk(KERN_ERR "mcd: dump error: failed to write");
			break;
		}
	}
	set_fs(old_fs);
	filp_close(dumpfile, NULL);

	if (ret < 0) {
		printk(KERN_ERR "cam: error in dump file: %d\n", ret);
		return ret;
	} else
		printk(KERN_ERR "cam: data dumped in '%s'\n", name);
	return 0;
}
EXPORT_SYMBOL(vb_dump_block);

struct cam_dump_wq_t {
	struct work_struct wq;
	struct vb2_buffer *vb2;
	char	*name;
} cam_dump;

static void dump_handler(struct work_struct *work)
{
	struct cam_dump_wq_t *dump = container_of(work,
					struct cam_dump_wq_t, wq);

	vb_dump_block(dump->vb2, dump->name);
	dump->vb2 = NULL;
}

int vb_dump_nonblock(struct vb2_buffer *vb2, char *name)
{
	if (vb2 == NULL || name == NULL || strlen(name) == 0)
		return -EINVAL;

	if (cam_dump.vb2 != NULL) {
		printk(KERN_ERR "mcd: dump rejected: last dump in progress\n");
		return -EAGAIN;
	}

	INIT_WORK(&cam_dump.wq, dump_handler);
	cam_dump.vb2	= vb2;
	cam_dump.name	= name;
	schedule_work(&cam_dump.wq);
	return 0;
}
EXPORT_SYMBOL(vb_dump_nonblock);
