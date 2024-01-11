// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc QOGIRN6PRO VPU driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/compat.h>
#include "vpu_drv.h"

static int handle_vpu_dec_interrupt(struct vpu_platform_data *data, int *status)
{
	int i;
	int int_status;
	int mmu_status;
	struct device *dev = data->dev;

	int_status = readl_relaxed(data->glb_reg_base + VPU_INT_RAW_OFF);
	mmu_status = readl_relaxed(data->vpu_base + VPU_MMU_INT_RAW_OFF);
	*status |= int_status | (mmu_status << 16);

	if (((int_status & 0xffff) == 0) &&
		((mmu_status & 0xff) == 0)) {
		dev_info(dev, "%s dec IRQ_NONE int_status 0x%x 0x%x",
			__func__, int_status, mmu_status);
		return IRQ_NONE;
	}

	if (int_status & DEC_BSM_OVF_ERR)
		dev_err(dev, "dec_bsm_overflow");

	if (int_status & DEC_VLD_ERR)
		dev_err(dev, "dec_vld_err");

	if (int_status & DEC_TIMEOUT_ERR)
		dev_err(dev, "dec_timeout");

	if (int_status & DEC_MMU_INT_ERR)
		dev_err(dev, "dec_mmu_int_err");

	if (int_status & DEC_AFBCD_HERR)
		dev_err(dev, "dec_afbcd_herr");

	if (int_status & DEC_AFBCD_PERR)
		dev_err(dev, "dec_afbcd_perr");

	if (mmu_status & MMU_RD_WR_ERR) {
		/* mmu ERR */
		dev_err(dev, "dec iommu addr: 0x%x\n",
		       readl_relaxed(data->vpu_base + VPU_MMU_INT_RAW_OFF));

		for (i = 0x44; i <= 0x68; i += 4)
			dev_info(dev, "addr 0x%x is 0x%x\n", i,
				readl_relaxed(data->vpu_base + i));
		WARN_ON_ONCE(1);
	}

	/* clear VSP accelerator interrupt bit */
	clr_vpu_interrupt_mask(data);

	return IRQ_HANDLED;
}

static int handle_vpu_enc_interrupt(struct vpu_platform_data *data, int *status)
{
	int i;
	int int_status;
	int mmu_status;
	struct device *dev = data->dev;

	int_status = readl_relaxed(data->glb_reg_base + VPU_INT_RAW_OFF);
	mmu_status = readl_relaxed(data->vpu_base + VPU_MMU_INT_RAW_OFF);
	*status |= int_status | (mmu_status << 16);

	if (((int_status & 0x7f) == 0) &&
		((mmu_status & 0xff) == 0)) {
		dev_info(dev, "%s enc IRQ_NONE int_status 0x%x 0x%x",
			__func__, int_status, mmu_status);
		return IRQ_NONE;
	}

	if (int_status & ENC_BSM_OVF_ERR)
		dev_err(dev, "enc_bsm_overflow");

	if (int_status & ENC_TIMEOUT_ERR)
		dev_err(dev, "enc_timeout");

	if (int_status & ENC_AFBCD_HERR)
		dev_err(dev, "enc_afbcd_herr");

	if (int_status & ENC_AFBCD_PERR)
		dev_err(dev, "enc_afbcd_perr");

	if (int_status & ENC_MMU_INT_ERR)
		dev_err(dev, "enc_mmu_int_err");

	if (mmu_status & MMU_RD_WR_ERR) {
		/* mmu ERR */
		dev_err(dev, "enc iommu addr: 0x%x\n",
		       readl_relaxed(data->vpu_base + VPU_MMU_INT_RAW_OFF));

		for (i = 0x44; i <= 0x68; i += 4)
			dev_info(dev, "addr 0x%x is 0x%x\n", i,
				readl_relaxed(data->vpu_base + i));
		WARN_ON_ONCE(1);
	}

	/* clear VSP accelerator interrupt bit */
	clr_vpu_interrupt_mask(data);

	return IRQ_HANDLED;
}

void clr_vpu_interrupt_mask(struct vpu_platform_data *data)
{
	int vpu_int_mask = 0;
	int mmu_int_mask = 0;

	vpu_int_mask = 0x1fff;
	mmu_int_mask = 0xff;

	/* set the interrupt mask 0 */
	writel_relaxed(0, data->glb_reg_base + VPU_INT_MASK_OFF);
	writel_relaxed(0, data->vpu_base + VPU_MMU_INT_MASK_OFF);

	/* clear vsp int */
	writel_relaxed(vpu_int_mask, data->glb_reg_base + VPU_INT_CLR_OFF);
	writel_relaxed(mmu_int_mask, data->vpu_base + VPU_MMU_INT_CLR_OFF);
}

static irqreturn_t vpu_dec_isr_handler(struct vpu_platform_data *data)
{
	int ret, status = 0;

	if (data == NULL) {
		pr_err("%s error occurred, data == NULL\n", __func__);
		return IRQ_NONE;
	}

	if (data->is_clock_enabled == false) {
		dev_err(data->dev, " vpu clk is disabled");
		return IRQ_HANDLED;
	}

	/* check which module occur interrupt and clear corresponding bit */
	ret = handle_vpu_dec_interrupt(data, &status);
	if (ret == IRQ_NONE)
		return IRQ_NONE;

	data->vpu_int_status = status;
	data->condition_work = 1;
	wake_up_interruptible(&data->wait_queue_work);

	return IRQ_HANDLED;
}

static irqreturn_t vpu_enc_isr_handler(struct vpu_platform_data *data)
{
	int ret, status = 0;

	if (data == NULL) {
		pr_err("%s error occurred, data == NULL\n", __func__);
		return IRQ_NONE;
	}

	if (data->is_clock_enabled == false) {
		dev_err(data->dev, " vpu clk is disabled");
		return IRQ_HANDLED;
	}

	/* check which module occur interrupt and clear corresponding bit */
	ret = handle_vpu_enc_interrupt(data, &status);
	if (ret == IRQ_NONE)
		return IRQ_NONE;

	data->vpu_int_status = status;
	data->condition_work = 1;
	wake_up_interruptible(&data->wait_queue_work);

	return IRQ_HANDLED;
}

irqreturn_t enc_core0_isr(int irq, void *data)
{
	struct vpu_platform_data *vpu_core = data;
	int ret = 0;
	dev_info(vpu_core->dev, "%s, isr", vpu_core->p_data->name);

	ret = vpu_enc_isr_handler(data);

	/*
		Do enc core0 specified work here, if needed.
	*/

	return ret;
}

irqreturn_t enc_core1_isr(int irq, void *data)
{
	struct vpu_platform_data *vpu_core = data;
	int ret = 0;
	dev_info(vpu_core->dev, "%s, isr", vpu_core->p_data->name);

	ret = vpu_enc_isr_handler(data);

	/*
		Do enc core1 specified work here, if needed.
	*/
	return ret;
}

irqreturn_t dec_core0_isr(int irq, void *data)
{
	struct vpu_platform_data *vpu_core = data;
	int ret = 0;
	dev_info(vpu_core->dev, "%s, isr", vpu_core->p_data->name);

	ret = vpu_dec_isr_handler(data);

	/*
		Do dec core0 specified work here, if needed.
	*/
	return ret;
}

struct clk *get_clk_src_name(struct clock_name_map_t clock_name_map[],
				unsigned int freq_level,
				unsigned int max_freq_level)
{
	if (freq_level >= max_freq_level) {
		pr_info("set freq_level to 0\n");
		freq_level = 0;
	}

	pr_debug(" freq_level %d %s\n", freq_level,
		 clock_name_map[freq_level].name);
	return clock_name_map[freq_level].clk_parent;
}

int find_freq_level(struct clock_name_map_t clock_name_map[],
			unsigned long freq,
			unsigned int max_freq_level)
{
	int level = 0;
	int i;

	for (i = 0; i < max_freq_level; i++) {
		if (clock_name_map[i].freq == freq) {
			level = i;
			break;
		}
	}
	return level;
}

#ifdef CONFIG_COMPAT
static int compat_get_mmu_map_data(struct compat_iommu_map_data __user *data32,
				   struct iommu_map_data __user *data)
{
	compat_int_t i;
	compat_size_t s;
	compat_ulong_t ul;
	int err;

	err = get_user(i, &data32->fd);
	err |= put_user(i, &data->fd);
	err |= get_user(s, &data32->size);
	err |= put_user(s, &data->size);
	err |= get_user(ul, &data32->iova_addr);
	err |= put_user(ul, &data->iova_addr);

	return err;
}

static int compat_put_mmu_map_data(struct compat_iommu_map_data __user *
				   data32,
				   struct iommu_map_data __user *data)
{
	compat_int_t i;
	compat_size_t s;
	compat_ulong_t ul;
	int err;

	err = get_user(i, &data->fd);
	err |= put_user(i, &data32->fd);
	err |= get_user(s, &data->size);
	err |= put_user(s, &data32->size);
	err |= get_user(ul, &data->iova_addr);
	err |= put_user(ul, &data32->iova_addr);

	return err;
}

long compat_vpu_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	long ret = 0;
	int err;
	struct compat_iommu_map_data __user *data32;
	struct iommu_map_data __user *data;
	struct vpu_platform_data *platform_data = filp->private_data;
	struct device *dev = platform_data->dev;

	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_VPU_GET_IOVA:

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL) {
			dev_err(dev, "%s %d, compat_alloc_user_space failed",
				__func__, __LINE__);
			return -EFAULT;
		}

		err = compat_get_mmu_map_data(data32, data);
		if (err) {
			dev_err(dev, "%s %d, compat_get_mmu_map_data failed",
				__func__, __LINE__);
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, VPU_GET_IOVA,
						(unsigned long)data);
		err = compat_put_mmu_map_data(data32, data);
			return ret ? ret : err;

	case COMPAT_VPU_FREE_IOVA:

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL) {
			dev_err(dev, "%s %d, compat_alloc_user_space failed",
				__func__, __LINE__);
			return -EFAULT;
		}

		err = compat_get_mmu_map_data(data32, data);
		if (err) {
			dev_err(dev, "%s %d, compat_get_mmu_map_data failed",
				__func__, __LINE__);
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, VPU_FREE_IOVA,
						(unsigned long)data);
		err = compat_put_mmu_map_data(data32, data);
		return ret ? ret : err;

	default:
		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)
						  compat_ptr(arg));
	}

	return ret;
}
#endif

int get_iova(struct vpu_platform_data *data,
		 struct iommu_map_data *mapdata, void __user *arg)
{
	int ret = 0;
	struct sprd_iommu_map_data iommu_map_data;
	struct device *dev = data->dev;

	clock_enable(data);

	ret = sprd_ion_get_buffer(mapdata->fd, NULL,
					&(iommu_map_data.buf),
					&iommu_map_data.iova_size);
	if (ret) {
		dev_err(dev, "get_sg_table failed, ret %d\n", ret);
		clock_disable(data);
		return ret;
	}

	iommu_map_data.ch_type = SPRD_IOMMU_FM_CH_RW;
	ret = sprd_iommu_map(data->dev, &iommu_map_data);
	if (!ret) {
		mapdata->iova_addr = iommu_map_data.iova_addr;
		mapdata->size = iommu_map_data.iova_size;
		dev_dbg(dev, "vpu iommu map success iova addr 0x%lx size 0x%zx\n",
			mapdata->iova_addr, mapdata->size);
		ret = copy_to_user((void __user *)arg, (void *)mapdata,
					sizeof(struct iommu_map_data));
		if (ret) {
			dev_err(dev, "copy_to_user failed, ret %d\n", ret);
			clock_disable(data);
			return -EFAULT;
		}
	} else
		dev_err(dev, "vpu iommu map failed, ret %d, map size 0x%zx\n",
			ret, iommu_map_data.iova_size);
	clock_disable(data);
	return ret;
}

int free_iova(struct vpu_platform_data *data,
		  struct iommu_map_data *ummapdata)
{
	int ret = 0;
	struct sprd_iommu_unmap_data iommu_ummap_data;

	clock_enable(data);
	iommu_ummap_data.iova_addr = ummapdata->iova_addr;
	iommu_ummap_data.iova_size = ummapdata->size;
	iommu_ummap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
	iommu_ummap_data.buf = NULL;

	ret = sprd_iommu_unmap(data->dev,
					&iommu_ummap_data);

	if (ret)
		dev_err(data->dev, "vpu iommu-unmap failed ret %d addr&size 0x%lx 0x%zx\n",
			ret, ummapdata->iova_addr, ummapdata->size);
	else
		dev_dbg(data->dev, "vpu iommu-unmap success iova addr 0x%lx size 0x%zx\n",
			ummapdata->iova_addr, ummapdata->size);
	clock_disable(data);

	return ret;
}

int get_clk(struct vpu_platform_data *data, struct device_node *np)
{
	int ret = 0, i, j = 0;
	struct clk *clk_dev_eb;
#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	struct clk *core_clk;
#endif
	struct clk *clk_domain_eb;
	struct clk *clk_ckg_eb;
	struct clk *clk_ahb_vsp;
	struct clk *clk_parent;
	struct device *dev = data->dev;

	for (i = 0; i < ARRAY_SIZE(vpu_clk_src); i++) {
		//struct clk *clk_parent;
		unsigned long frequency;

		clk_parent = of_clk_get_by_name(np, vpu_clk_src[i]);
		if (IS_ERR_OR_NULL(clk_parent)) {
			dev_info(dev, "clk %s not found,continue to find next clock\n",
				vpu_clk_src[i]);
			continue;
		}
		frequency = clk_get_rate(clk_parent);

		data->clock_name_map[j].name = vpu_clk_src[i];
		data->clock_name_map[j].freq = frequency;
		data->clock_name_map[j].clk_parent = clk_parent;

		dev_info(dev, "vpu clk in dts file: clk[%d] = (%ld, %s)\n", j,
			frequency, data->clock_name_map[j].name);
		j++;
	}
	data->max_freq_level = j;


	clk_domain_eb = devm_clk_get(data->dev, "clk_domain_eb");

	if (IS_ERR_OR_NULL(clk_domain_eb)) {
	    dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_domain_eb", clk_domain_eb);
	    data->clk.clk_domain_eb = NULL;
	    ret = -EINVAL;
	    goto errout;
	} else
	    data->clk.clk_domain_eb = clk_domain_eb;

	clk_dev_eb =
	    devm_clk_get(data->dev, "clk_dev_eb");

	if (IS_ERR_OR_NULL(clk_dev_eb)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_dev_eb", clk_dev_eb);
		ret = -EINVAL;
		data->clk.clk_dev_eb = NULL;
		goto errout;
	} else
		data->clk.clk_dev_eb = clk_dev_eb;

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	core_clk = devm_clk_get(data->dev, "clk_vsp");

	if (IS_ERR_OR_NULL(core_clk)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n", "core_clk",
		       core_clk);
		ret = -EINVAL;
		data->clk.core_clk = NULL;
		goto errout;
	} else
		data->clk.core_clk = core_clk;
#endif

	clk_ahb_vsp =
		devm_clk_get(data->dev, "clk_ahb_vsp");

	if (IS_ERR_OR_NULL(clk_ahb_vsp)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_ahb_vsp", clk_ahb_vsp);
		ret = -EINVAL;
		goto errout;
	} else
		data->clk.clk_ahb_vsp = clk_ahb_vsp;

	clk_ckg_eb =
		devm_clk_get(data->dev, "clk_ckg_eb");

	if (IS_ERR_OR_NULL(clk_ckg_eb)) {
		dev_err(dev, "Failed: Can't get clock [%s]! %p\n",
		       "clk_ckg_eb", clk_ckg_eb);
		ret = -EINVAL;
		goto errout;
	} else
		data->clk.clk_ckg_eb = clk_ckg_eb;

	clk_parent = devm_clk_get(data->dev,
		       "clk_ahb_vsp_parent");

	if (IS_ERR_OR_NULL(clk_parent)) {
		dev_err(dev, "clock[%s]: failed to get parent in probe!\n",
		       "clk_ahb_vsp_parent");
		ret = -EINVAL;
		goto errout;
	} else
		data->clk.ahb_parent_clk = clk_parent;

errout:
	return ret;
}

int clock_enable(struct vpu_platform_data *data)
{
	int ret = 0;
	struct vpu_clk *clk = &data->clk;
	struct device *dev = data->dev;

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	ret = clk_set_parent(clk->core_clk, clk->core_parent_clk);
	if (ret) {
		dev_err(dev, "clock[%s]: clk_set_parent() failed!", "clk_core");
		goto error1;
	}

	ret = clk_prepare_enable(clk->core_clk);
	if (ret) {
		dev_err(dev, "core_clk: clk_prepare_enable failed!\n");
		goto error1;
	}
	dev_dbg(dev, "vsp_clk: clk_prepare_enable ok.\n");
#endif

	if (clk->clk_domain_eb) {
		ret = clk_prepare_enable(clk->clk_domain_eb);
		if (ret) {
			dev_err(dev, "vsp clk_domain_eb: clk_enable failed!\n");
			goto error2;
		}
		dev_dbg(dev, "vsp clk_domain_eb: clk_prepare_enable ok.\n");
	} else {
		dev_err(dev, "clk_domain_eb is NULL!\n");
		goto error2;
	}


	if (clk->clk_dev_eb) {
		ret = clk_prepare_enable(clk->clk_dev_eb);
		if (ret) {
			dev_err(dev, "clk_dev_eb: clk_prepare_enable failed!\n");
			goto error3;
		}
		dev_dbg(dev, "clk_dev_eb: clk_prepare_enable ok.\n");
	} else {
		dev_err(dev, "clk_dev_eb is NULL!\n");
		goto error3;
	}

	if (clk->clk_ahb_vsp) {
		ret = clk_set_parent(clk->clk_ahb_vsp, clk->ahb_parent_clk);
		if (ret) {
			dev_err(dev, "clock[%s]: clk_set_parent() failed!",
				"ahb_parent_clk");
			goto error4;
		}
		ret = clk_prepare_enable(clk->clk_ahb_vsp);
		if (ret) {
			dev_err(dev, "clk_ahb_vsp: clk_prepare_enable failed!\n");
			goto error4;
		}
		dev_dbg(dev, "clk_ahb_vsp: clk_prepare_enable ok.\n");
	} else {
		dev_err(dev, "clk_ahb_vsp is NULL!\n");
		goto error4;
	}

	dev_dbg(data->dev, "%s %d,OK\n", __func__, __LINE__);

	return ret;

error4:
	clk_disable_unprepare(clk->clk_dev_eb);
error3:
	clk_disable_unprepare(clk->clk_domain_eb);
error2:
	clk_disable_unprepare(clk->core_clk);
error1:
	return ret;
}

void clock_disable(struct vpu_platform_data *data)
{
	struct vpu_clk *clk = &data->clk;
	clk_disable_unprepare(clk->clk_ahb_vsp);
	clk_disable_unprepare(clk->clk_dev_eb);
	clk_disable_unprepare(clk->clk_domain_eb);
#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	clk_disable_unprepare(clk->core_clk);
#endif
	dev_dbg(data->dev, "%s %d,OK\n", __func__, __LINE__);
}
