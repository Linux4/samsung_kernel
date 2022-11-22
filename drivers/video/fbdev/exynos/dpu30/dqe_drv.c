/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung EXYNOS SoC series DQE driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/console.h>

#include "dqe.h"
#include "decon.h"
#if defined(CONFIG_SOC_EXYNOS9830)
#include "./cal_9830/regs-dqe.h"
#endif

struct dqe_device *dqe_drvdata;
struct class *dqe_class;

u32 cgc_lut[143];

#define TUNE_MODE_SIZE 4
u32 tune_mode[TUNE_MODE_SIZE][492];
char dqe_str_result[1024];

int dqe_log_level = 6;
module_param(dqe_log_level, int, 0644);

const u32 bypass_cgc_tune[143] = {
	3,0,0,3,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,512,512,512,512,512,512,512,512,512,1023,1023,1023,1023,1023,1023,1023,1023,1023,0,0,0,512,512,512,1023,1023,1023,0,0,0,512,512,512,1023,1023,1023,0,0,0,512,512,512,1023,1023,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023,0,512,1023
};

static void dqe_dump(void)
{
	int acquired = console_trylock();
	struct decon_device *decon = get_decon_drvdata(0);

	if (IS_DQE_OFF_STATE(decon)) {
		decon_info("%s: DECON is disabled, state(%d)\n",
				__func__, (decon) ? (decon->state) : -1);
		return;
	}

	decon_hiber_block_exit(decon);

	decon_info("\n=== DQE SFR DUMP ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs + DQE_BASE , 0x600, false);

	decon_info("\n=== DQE SHADOW SFR DUMP ===\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			decon->res.regs + SHADOW_DQE_OFFSET, 0x600, false);

	decon_hiber_unblock(decon);

	if (acquired)
		console_unlock();
}

static void dqe_load_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe->ctx.cgc[i].addr = DQECGCLUT_BASE + (i * 4);
		dqe->ctx.cgc[i].val = dqe_read(dqe->ctx.cgc[i].addr);
	}

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe_dbg("0x%04x %08x",
			dqe->ctx.cgc[i].addr,dqe->ctx.cgc[i].val);
	}
}

static void dqe_init_context(void)
{
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);


	dqe->ctx.cgc_on = 0;

	dqe->ctx.need_udpate = true;
}

int dqe_save_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	if (dqe->ctx.need_udpate)
		return 0;

	dqe_dbg("%s\n", __func__);

	for (i = 0; i < DQECGCLUT_MAX; i++)
		dqe->ctx.cgc[i].val =
			dqe_read(dqe->ctx.cgc[i].addr);

	dqe->ctx.cgc_on = dqe_reg_get_cgc_on();


	return 0;
}

int dqe_restore_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_dbg("%s\n", __func__);

	mutex_lock(&dqe->restore_lock);

	dqe_reg_set_dqecon_all_reset();

	for (i = 0; i < DQECGCLUT_MAX; i++)
		dqe_write(dqe->ctx.cgc[i].addr,
				dqe->ctx.cgc[i].val);

	if (dqe->ctx.cgc_on)
		dqe_reg_set_cgc_on(1);

	dqe->ctx.need_udpate = false;

	mutex_unlock(&dqe->restore_lock);

	return 0;
}

#if 0
static void decon_dqe_update_req(struct decon_device *decon)
{
	struct decon_mode_info psr;

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		return;
	}

	decon_hiber_block_exit(decon);

	decon_to_psr_info(decon, &psr);
	decon_reg_update_req_dqe(decon->id);
	decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_ENABLE);

	decon_hiber_unblock(decon);
}
#endif

static void decon_dqe_update_context(struct decon_device *decon)
{
	struct decon_mode_info psr;

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		return;
	}

	decon_hiber_block_exit(decon);

	dqe_restore_context();

	decon_to_psr_info(decon, &psr);
	decon_reg_update_req_dqe(decon->id);
	decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_ENABLE);

	decon_hiber_unblock(decon);
}

static void dqe_cgc_lut_set(void)
{
	int i, j, k;
	struct dqe_device *dqe = dqe_drvdata;

	/* DQECGC_CONTROL1*/
	dqe->ctx.cgc[0].val = (
		CGC_DITHER_CNT_DIFF(cgc_lut[1]) | CGC_DITHER_ON(cgc_lut[2]) |
		CGC_DITHER_CON(cgc_lut[3]) | CGC_GAMMA_DEC_EN(cgc_lut[4]) |
		CGC_GAMMA_ENC_EN(cgc_lut[5])
	);

	/* DQECGC_CONTROL2*/
	dqe->ctx.cgc[1].val = (
		CGC_GRAY_MODE(cgc_lut[6]) | CGC_GRAY_BYPASS(cgc_lut[7]) |
		CGC_GRAY_GAIN(cgc_lut[8])
	);

	/* DQECGC_CONTROL3*/
	dqe->ctx.cgc[2].val = (
		CGC_GRAY_WEIGHT_TH(cgc_lut[9]) | CGC_MC_GAIN(cgc_lut[10])
	);

	/* DQECGC1_RED1 --- DQECGC1_BLUE9*/
	for (i = 3, j = 11, k = 1; i < 30; i++, k++) {
		if (k % 9) {
			dqe->ctx.cgc[i].val = (
				DQECGC_GRAYLUT_R0(cgc_lut[j]) | DQECGC_GRAYLUT_R1(cgc_lut[j + 1])
			);
			j += 2;
		} else {
			dqe->ctx.cgc[i].val = (
				DQECGC_GRAYLUT_R0(cgc_lut[j])
			);
			j += 1;
		}
	}

	/* DQECGC1_LUT27_RED1 --- DQECGC1_LUT27_BLUE14*/
	for (i = 30, k = 1; i < DQECGCLUT_MAX; i++, k++) {
		if (k % 14) {
			dqe->ctx.cgc[i].val = (
				DQECGC_LUT27_000_R(cgc_lut[j]) | DQECGC_LUT27_001_R(cgc_lut[j + 1])
			);
			j += 2;
		} else {
			dqe->ctx.cgc[i].val = (
				DQECGC_LUT27_000_R(cgc_lut[j])
			);
			j += 1;
		}
	}
}

static ssize_t decon_dqe_cgc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	int i, str_len = 0;

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_str_result[0] = '\0';
	for (i = 0; i < 143; i++) {
		char num[5] = {0,};

		sprintf(num, "%d,", cgc_lut[i]);
		strcat(dqe_str_result, num);
	}
	str_len = strlen(dqe_str_result);
	dqe_str_result[str_len - 1] = '\0';

	mutex_unlock(&dqe->lock);

	count = sprintf(buf, "%s\n", dqe_str_result);

	return count;
}

static ssize_t decon_dqe_cgc_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);
	s32 cgc_lut_signed[143] = {0,};

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("cgc write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 1; i++) {
			k = 142;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtos32(head, 0, &cgc_lut_signed[j]);
				if (ret) {
					dqe_err("strtos32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtos32(head, 0, &cgc_lut_signed[j]);
		if (ret) {
			dqe_err("strtos32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (j = 0; j < 143; j++)
		dqe_dbg("%d ", cgc_lut_signed[j]);

	/* signed -> unsigned */
	for (j = 0; j < 143; j++) {
		cgc_lut[j] = (u32)cgc_lut_signed[j];
		dqe_dbg("%04x %d", cgc_lut[j], cgc_lut_signed[j]);
	}

	dqe_cgc_lut_set();

	dqe->ctx.cgc_on = DQE_CGC_ON_MASK;
	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(cgc, 0660,
	decon_dqe_cgc_show,
	decon_dqe_cgc_store);

static ssize_t decon_dqe_off_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int ret = 0;
	unsigned int value = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("dqe off write count error\n");
		ret = -1;
		goto err;
	}

	if (IS_DQE_OFF_STATE(decon)) {
		dqe_err("decon is not enabled!(%d)\n", (decon) ? (decon->state) : -1);
		ret = -1;
		goto err;
	}

	ret = kstrtouint(buffer, 0, &value);
	if (ret < 0)
		goto err;

	dqe_info("%s: %d\n", __func__, value);

	switch (value) {
	case 0:
		dqe->ctx.cgc_on = 0;
		break;
	default:
		break;
	}

	dqe->ctx.need_udpate = true;

	decon_dqe_update_context(decon);

	mutex_unlock(&dqe->lock);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(dqe_off, 0660,
	NULL,
	decon_dqe_off_store);

static ssize_t decon_dqe_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t count = 0;

	dqe_info("%s\n", __func__);

	dqe_dump();

	count = snprintf(buf, PAGE_SIZE, "dump = %d\n", val);

	return count;
}

static DEVICE_ATTR(dump, 0440,
	decon_dqe_dump_show,
	NULL);

static struct attribute *dqe_attrs[] = {
	&dev_attr_cgc.attr,
	&dev_attr_dqe_off.attr,
	&dev_attr_dump.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dqe);

void decon_dqe_sw_reset(struct decon_device *decon)
{
	if (decon->id)
		return;

	dqe_reg_hsc_sw_reset(decon);
}

void decon_dqe_enable(struct decon_device *decon)
{
	u32 val;

	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	dqe_restore_context();
	dqe_reg_start(decon->id, decon->lcd_info);

	val = dqe_read(DQECON);
	dqe_info("dqe cgc:%d\n", DQE_CGC_ON_GET(val));
}

void decon_dqe_disable(struct decon_device *decon)
{
	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	dqe_save_context();
	dqe_reg_stop(decon->id);
}

int decon_dqe_create_interface(struct decon_device *decon)
{
	int ret = 0;
	struct dqe_device *dqe;

	if (decon->id || (decon->dt.out_type != DECON_OUT_DSI)) {
		dqe_info("decon%d doesn't need dqe interface\n", decon->id);
		return 0;
	}

	dqe = kzalloc(sizeof(struct dqe_device), GFP_KERNEL);
	if (!dqe) {
		ret = -ENOMEM;
		goto exit0;
	}

	dqe_drvdata = dqe;
	dqe->decon = decon;

	if (IS_ERR_OR_NULL(dqe_class)) {
		dqe_class = class_create(THIS_MODULE, "dqe");
		if (IS_ERR_OR_NULL(dqe_class)) {
			pr_err("failed to create dqe class\n");
			ret = -EINVAL;
			goto exit1;
		}

		dqe_class->dev_groups = dqe_groups;
	}

	dqe->dev = device_create(dqe_class, decon->dev, 0,
			&dqe, "dqe%d", 0);
	if (IS_ERR_OR_NULL(dqe->dev)) {
		pr_err("failed to create dqe device\n");
		ret = -EINVAL;
		goto exit2;
	}

	mutex_init(&dqe->lock);
	mutex_init(&dqe->restore_lock);
	dev_set_drvdata(dqe->dev, dqe);

	dqe_load_context();

	dqe_init_context();

	dqe_info("decon_dqe_create_interface done.\n");

	return ret;
exit2:
	class_destroy(dqe_class);
exit1:
	kfree(dqe);
exit0:
	return ret;
}
