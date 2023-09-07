/* drivers/video/backlight/gen_panel/dimming_debug.c
 *
 * Copyright (C) 2016 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "gen-panel.h"
#include "dynamic_aid.h"
#include "dynamic_aid_gamma.h"

#define MAX_TFILE_NAME_LEN (100)
#define MAX_BUF_SIZE (1024)
static char input_file_name[MAX_TFILE_NAME_LEN];
static char output_file_name[MAX_TFILE_NAME_LEN];
static char *tfile_paths[] = {"/sdcard/", "/data/"};
static struct smartdim_conf *sdimconf;
static u8 (*gamma_debug_table)[33];
static unsigned int *candela_table;
static unsigned int nr_candela;
const char *TAG_MTP_START = "[MTP_START]";
const char *TAG_MTP_END = "[MTP_END]";
const char *TAG_MTP_OFFSET_OUT = "[MTP_OFFSET_OUT]";

enum tag {
	TAG_DIMMING_INFO,
	TAG_MTP_OFFSET_INPUT,
	TAG_MTP_OFFSET_OUTPUT,
	MAX_INPUT_TAG,
};

char *tag_name [] = {
	"[DIMMING_INFO]",
	"[MTP_OFFSET_INPUT]",
	"[MTP_OFFSET_OUTPUT]"
};

static int dimming_debug_parse_mtp_offset(const char *buf, int (*mtp_offset)[3])
{
	int i, iv, c, pos, value[3];
	int lower, upper;
	char volt[10];

	char *s = strnstr(buf, tag_name[TAG_MTP_OFFSET_INPUT], 1024);
	if (unlikely(s == NULL)) {
		pr_warn("%s, %s not found\n", __func__, tag_name[TAG_MTP_OFFSET_INPUT]);
		return 0;
	}

	s += strlen(tag_name[TAG_MTP_OFFSET_INPUT]);
	for (iv = 0; iv < VOLT_MAX; iv++) {
		sscanf(s, "%s %i %i %i %n", volt, &value[0], &value[1], &value[2], &pos);
		for (i = 0; i < ARRAY_SIZE(volt_name); i++)
			if (!strncmp(volt_name[i], volt, strlen(volt)))
				break;

		if (i >= ARRAY_SIZE(volt_name)) {
			pr_err("%s, unknown volt name %s\n", __FUNCTION__, volt);
			break;
		}

		if (iv == VOLT_VT)
			lower = 0, upper = 0xF;
		else if (i == VOLT_V255)
			lower = -255, upper = 255;
		else
			lower = -127, upper = 127;

		for (c = 0; c < 3; c++) {
			if ((iv != VOLT_VT) && (value[c] < 0)) {
				mtp_offset[i][c] = (upper + 1 - value[c]);
			} else {
				mtp_offset[i][c] = value[c];
			}
		}

		s += pos;
	}

	if (iv != VOLT_MAX)
		return -EINVAL;

	pr_info("%s mtp_offset\n", __func__);
	for (iv = 0; iv < VOLT_MAX; iv++)
		pr_info("%-3s %4d %4d %4d\n", volt_name[iv],
				mtp_offset[iv][0], mtp_offset[iv][1], mtp_offset[iv][2]);

	return (int)(s - buf);
}

static int dimming_debug_load_file(char *filename, char **read_buf)
{
	struct file *filp;
	int ret, size;
	mm_segment_t old_fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (unlikely(IS_ERR(filp))) {
		pr_warn("%s, failed to open (%ld)\n", __func__, IS_ERR(filp));
		return -EINVAL;
	}
	size = filp->f_path.dentry->d_inode->i_size;

	pr_info("%s, open %s, size %d\n", __func__, filename, size);
	*read_buf = kmalloc(size + 10, GFP_KERNEL);
	if (unlikely(*read_buf == NULL)) {
		pr_err("%s, failed to alloc size %d\n", __func__, size + 10);
		ret = -ENOMEM;
		goto out;
	}

	ret = vfs_read(filp, (char __user *)(*read_buf), size, &filp->f_pos);
	if (unlikely(ret != size)) {
		pr_err("%s, failed to read %d\n", __func__, ret);
		ret = -EINVAL;
		kfree(*read_buf);
		*read_buf = NULL;
		goto out;
	}

out:
	set_fs(old_fs);
	filp_close(filp, NULL);
	return ret;
}

static int dimming_debug_print_dimming_info(struct smartdim_conf *conf, char *buf)
{
	int i, j, c, cnt;
	char *s = buf;
	/* TODO : base_mtp should be get from smartdim_conf */
	static int REF_MTP[][3] = {
		{0x0, 0x0, 0x0},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x80, 0x80, 0x80},
		{0x100, 0x100, 0x100},
	};

	struct coeff_t v_coeff[] = {
		{0, 860},       /* IV_VT */
		{64, 320},
		{64, 320},
		{64, 320},
		{64, 320},
		{64, 320},
		{64, 320},
		{64, 320},
		{64, 320},
		{72, 860}       /* IV_255 */
	};

	static int vt_coefficient[] = {
		0, 12, 24, 36, 48,
		60, 72, 84, 96, 108,
		138, 148, 158, 168,
		178, 186,
	};

	s += snprintf(s, 100, "%s\n", tag_name[TAG_DIMMING_INFO]);
	s += snprintf(s, 10, "%u\n", conf->vregout_voltage);
	s += snprintf(s, 10, "%u\n", BIT_SHIFT);

	for (i = 0; i < ARRAY_SIZE(REF_MTP); i++) {
		for (c = 0; c < 3; c++)
			s += snprintf(s, 10, "%-3X ", REF_MTP[i][c]);
		s += snprintf(s, 10, "\n");
	}

	s += snprintf(s, 10, "%d\n", conf->gamma_curve_table[nr_candela - 1]);
	for (i = 0; i < ARRAY_SIZE(v_coeff); i++)
		s += snprintf(s, 20, "%-3d %-3d\n",
				v_coeff[i].numerator, v_coeff[i].denominator);

	for (i = 0; i < ARRAY_SIZE(vt_coefficient); i++)
		s += snprintf(s, 10, "%d\n", vt_coefficient[i]);

	for (i = 0; i < nr_candela; i++) {
		s += snprintf(s, 50, "%-3d\t %04X %3d %3d\t ",
				conf->lux_tab[i], 0, conf->base_lux_table[i],
				conf->gamma_curve_table[i]);
		for (j = 0; j < (VOLT_MAX - VOLT_V3); j++)
			s += snprintf(s, 10, "%3d ",
					conf->candela_offset[i * (VOLT_MAX - VOLT_V3) + j]);

		s += snprintf(s, 10, "\t ");

		for (j = 0, cnt = 0; j < (VOLT_MAX - VOLT_V11); j++)
			for (c = 0; c < 3; c++, cnt++)
				s += snprintf(s, 10, "%3d ",
						conf->rgb_offset[i * 24 + cnt]);

		s += snprintf(s, 5, "\n");
	}

	return (int)s - (int)buf;
}

static int dimming_debug_print_gamma_table(char *buf)
{
	char *s = buf;
	int i, cnt, ref_table[] = {
		27, 28, 29, 24, 25, 26,
		21, 22, 23, 18, 19, 20,
		15, 16, 17, 12, 13, 14,
		9, 10, 11, 6, 7, 8
	};

	if (unlikely(!buf)) {
		pr_err("%s, error output buffer is null\n", __func__);
		return -EINVAL;
	}

	s += snprintf(s, 100, "%s\n", tag_name[TAG_MTP_OFFSET_OUTPUT]);
	for (i = 0; i < nr_candela; i++) {
		s += snprintf(s, 5, "%-3d\t", candela_table[i]);
		for (cnt = 0; cnt < ARRAY_SIZE(ref_table); cnt++)
			s += snprintf(s, 5, " %3d", gamma_debug_table[i][ref_table[cnt]]);

		s += snprintf(s, 5, " %3d",
				gamma_debug_table[i][0] * 256 + gamma_debug_table[i][1]);
		s += snprintf(s, 5, " %3d",
				gamma_debug_table[i][2] * 256 + gamma_debug_table[i][3]);
		s += snprintf(s, 5, " %3d",
				gamma_debug_table[i][4] * 256 + gamma_debug_table[i][5]);

		for (cnt = 30; cnt < 33; cnt++)
			s += snprintf(s, 5, " %3d", gamma_debug_table[i][cnt]);

		s += snprintf(s, 5, "\n");
	}

	return (int)(s - buf);
}

static inline void mtp_itoc(int val, u8 *msb, u8 *lsb)
{
	*msb = (val & 0xFF00) >> 8;
	*lsb = (val & 0x00FF);
}

static void mtp_offset_32to8(int *mtp_offset_32, u8 *mtp_offset_8)
{
	int i, j, c, count;

	mtp_itoc(mtp_offset_32[27], &mtp_offset_8[0], &mtp_offset_8[1]);
	mtp_itoc(mtp_offset_32[28], &mtp_offset_8[2], &mtp_offset_8[3]);
	mtp_itoc(mtp_offset_32[29], &mtp_offset_8[4], &mtp_offset_8[5]);

	for (i = 6, j = 24, count = VOLT_VT; count < VOLT_V255; count++) {
		for (c = 0; c < 3; c++)
			mtp_offset_8[i + c] = mtp_offset_32[j + c];
		i += 3;
		j -= 3;
	}
}

static int dimming_debug_mtp_test(char *of_name, const char *read_buf, int size)
{
	const char *s = read_buf;
	char *out_buf;
	int i, mtp_offset[VOLT_MAX][3];
	int ret = 0, input_size, output_size;
	struct file *filp;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(get_ds());

	pr_info("%s, open %s\n", __func__, of_name);
	filp = filp_open(of_name, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	if (unlikely(IS_ERR(filp))) {
		pr_warn("%s, failed to open (%ld)\n", __func__, IS_ERR(filp));
		set_fs(old_fs);
		return -EINVAL;
	}

	out_buf = kmalloc(nr_candela * 1024, GFP_KERNEL);
	if (unlikely(!out_buf)) {
		pr_err("%s, failed to alloc out buffer\n", __func__);
		ret = -ENOMEM;
		goto err_mtp_test_alloc;
	}

	output_size = dimming_debug_print_dimming_info(sdimconf, out_buf);
	vfs_write(filp, out_buf, output_size, &filp->f_pos);
	ret += output_size;

	while ((input_size = dimming_debug_parse_mtp_offset(s, mtp_offset)) > 0) {
		vfs_write(filp, s, input_size, &filp->f_pos);
		mtp_offset_32to8((int *)mtp_offset, (u8 *)sdimconf->mtp_buffer);
		sdimconf->init();
		for (i = 0; i < nr_candela; i++)
			sdimconf->generate_gamma(candela_table[i], gamma_debug_table[i]);
		output_size = dimming_debug_print_gamma_table(out_buf);
		vfs_write(filp, out_buf, output_size, &filp->f_pos);
		ret += output_size;
		s += input_size;
	}

	if (unlikely(output_size < 0)) {
		pr_err("%s failed to parse mtp offset %d\n", __func__, output_size);
		ret = -EINVAL;
	}

	kfree(out_buf);

err_mtp_test_alloc:
	set_fs(old_fs);
	filp_close(filp, NULL);

	return ret;
}

static ssize_t mtp_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "filename : %s\n", input_file_name);
}

static ssize_t mtp_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	char fname[256], of_name[256], *name, *read_buf = NULL;
	unsigned int len, i;
	int fsize, ret;

	for (i = 0; i < ARRAY_SIZE(tfile_paths); i++) {
		len = strlen(tfile_paths[i]) + strlen(buf) + 1;
		sprintf(fname, "%s%s", tfile_paths[i], buf);
		name = fname;
		while (*name) {
			if (isspace(*name)) {
				*name = '\0';
				break;
			}
			name++;
		}
		sprintf(of_name, "%s_out", fname);

		pr_info("%s, start loading %s\n", __func__, fname);
		fsize = dimming_debug_load_file(fname, &read_buf);
		if (fsize > 0) {
			strncpy(input_file_name, fname, len);
			strncpy(output_file_name, of_name, len + strlen("_out"));
			break;
		}
	}

	if (unlikely(fsize <= 0 || read_buf == NULL)) {
		pr_err("%s, failed to load %s\n", __func__, buf);
		return -EINVAL;
	}

	mutex_lock(&lcd->access_ok);
	ret = dimming_debug_mtp_test(of_name, read_buf, fsize);
	mutex_unlock(&lcd->access_ok);

	pr_err("%s, mtp test %s, %d\n",
			__func__, ((ret < 0) ? "failed" : "done"), ret);

	kfree(read_buf);
	return size;
}

static DEVICE_ATTR(mtp_debug, 0664, mtp_debug_show, mtp_debug_store);

int mtp_debug_attach(struct lcd *lcd, struct smartdim_conf *dim_conf)
{
	int ret;

	if (IS_ERR(lcd->class)) {
		ret = PTR_ERR(lcd->class);
		pr_err("no class(lcd)");
		return ret;
	}

	if (IS_ERR(lcd->dev)) {
		ret = PTR_ERR(lcd->dev);
		pr_err("no device(panel)!\n");
		return ret;
	}

	ret = device_create_file(lcd->dev, &dev_attr_mtp_debug);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_mtp_debug.attr.name);
		return ret;
	}

	if (unlikely(dim_conf == NULL)) {
		pr_err("no dimming conf\n");
		return -EINVAL;
	}

	sdimconf = dim_conf;
	candela_table = dim_conf->lux_tab;
	nr_candela = dim_conf->lux_tabsize;
	pr_info("%s, candela_table %p, nr_candela %d\n", __func__, candela_table, nr_candela);
	if (!gamma_debug_table)
		gamma_debug_table = (u8 (*)[33])kzalloc(sizeof(u8) * nr_candela * 33, GFP_KERNEL);

	return 0;
}
EXPORT_SYMBOL(mtp_debug_attach);

void mtp_debug_detach(struct lcd *lcd)
{
	device_remove_file(lcd->dev, &dev_attr_mtp_debug);
	return;
}
EXPORT_SYMBOL(mtp_debug_detach);

MODULE_DESCRIPTION("DYNAMIC AID DEBUG Driver");
MODULE_LICENSE("GPL");
