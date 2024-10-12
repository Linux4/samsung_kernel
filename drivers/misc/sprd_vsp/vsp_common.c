/*
 * Copyright (C) 2012--2016 Spreadtrum Communications Inc.
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
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include "vsp_common.h"
#if IS_ENABLED(CONFIG_SPRD_VSP_PW_DOMAIN)
#include <linux/mfd/syscon.h>
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vsp: " fmt

struct vsp_iommu_map_entry {
	struct list_head list;

	int fd;
	unsigned long iova_addr;
	size_t iova_size;

	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
};

struct system_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;

	bool uncached;
};

struct register_gpr regs[ARRAY_SIZE(tb_name)];

struct clk *vsp_get_clk_src_name(struct clock_name_map_t clock_name_map[],
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

int find_vsp_freq_level(struct clock_name_map_t clock_name_map[],
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

int vsp_get_dmabuf(int fd, struct dma_buf **dmabuf, void **buf, size_t *size)
{
	struct system_heap_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd, dmabuf);
		return -EINVAL;
	}

	if (fd >= 0) {
		*dmabuf = dma_buf_get(fd);
		if (IS_ERR_OR_NULL(*dmabuf)) {
			pr_err("%s, dmabuf error: %p !\n", __func__, *dmabuf);
			return PTR_ERR(*dmabuf);
		}
	}
	buffer = (*dmabuf)->priv;

	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->len;

	return 0;
}


#ifdef CONFIG_COMPAT
static int compat_get_mmu_map_data(struct compat_vsp_iommu_map_data __user *
				   data32,
				   struct vsp_iommu_map_data __user *data)
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
};

static int compat_put_mmu_map_data(struct compat_vsp_iommu_map_data __user *
				   data32,
				   struct vsp_iommu_map_data __user *data)
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
};

long compat_vsp_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	long ret = 0;
	int err;
	struct compat_vsp_iommu_map_data __user *data32;
	struct vsp_iommu_map_data __user *data;

	struct vsp_fh *vsp_fp = filp->private_data;

	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	if (vsp_fp == NULL) {
		pr_err("%s, vsp_ioctl error occurred, vsp_fp == NULL\n",
		       __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case COMPAT_VSP_GET_IOVA:

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL) {
			pr_err("%s %d, compat_alloc_user_space failed",
				__func__, __LINE__);
			return -EFAULT;
		}

		err = compat_get_mmu_map_data(data32, data);
		if (err) {
			pr_err("%s %d, compat_get_mmu_map_data failed",
				__func__, __LINE__);
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, VSP_GET_IOVA,
						(unsigned long)data);
		err = compat_put_mmu_map_data(data32, data);
			return ret ? ret : err;

	case COMPAT_VSP_FREE_IOVA:

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL) {
			pr_err("%s %d, compat_alloc_user_space failed",
				__func__, __LINE__);
			return -EFAULT;
		}

		err = compat_get_mmu_map_data(data32, data);
		if (err) {
			pr_err("%s %d, compat_get_mmu_map_data failed",
				__func__, __LINE__);
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, VSP_FREE_IOVA,
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

void vsp_check_pw_status(struct vsp_dev_t *vsp_hw_dev)
{
#if IS_ENABLED(CONFIG_SPRD_VSP_PW_DOMAIN)
	int ret = 0;
	u32 power_state1 = 0, vsp_eb_reg = 0;
	u32 mm_vsp_ahb_reg = 0;
	u32 ap_ahb_regs = 0;

	if (vsp_hw_dev->version == SHARKL3) {
		regmap_read(regs[PMU_PWR_STATUS].gpr, regs[PMU_PWR_STATUS].reg,
			&power_state1);
		regmap_read(regs[VSP_DOMAIN_EB].gpr, regs[VSP_DOMAIN_EB].reg, &vsp_eb_reg);
		if ((vsp_eb_reg & 0x4000) != 0x4000) {
			ret = regmap_update_bits(regs[VSP_DOMAIN_EB].gpr, regs[VSP_DOMAIN_EB].reg,
				BIT(14), BIT(14));
			if (ret) {
				pr_err("iova regmap_update_bits failed %s, %d\n",
					__func__, __LINE__);
			}
			regmap_read(regs[VSP_DOMAIN_EB].gpr, regs[VSP_DOMAIN_EB].reg, &vsp_eb_reg);
		}

		if (((power_state1 & 0x1f) == 0x0) &&
			((vsp_eb_reg & 0x4000) == 0x4000)) {
			regmap_read(regs[RESET].gpr, 0x0, &mm_vsp_ahb_reg);
			if ((mm_vsp_ahb_reg & 0x3) != 0x3) {
				pr_info("mm vsp ahb 0x%x\n", mm_vsp_ahb_reg);
				ret = regmap_update_bits(regs[RESET].gpr, 0x0, BIT(1) | BIT(0),
					BIT(1) | BIT(0));
				if (ret) {
					pr_err("vsp_iova regmap_update_bits failed %s, %d\n",
						__func__, __LINE__);
				}
			}
		} else {
			pr_info("offset 0x%x,power 0x%x,offset 0x%x,vsp eb 0x%x\n",
				regs[PMU_PWR_STATUS].reg, power_state1,
				regs[VSP_DOMAIN_EB].reg, vsp_eb_reg);
		}
	}

	if (vsp_hw_dev->version == SHARKL5Pro) {
		regmap_read(regs[PMU_PWR_STATUS].gpr, regs[PMU_PWR_STATUS].reg,
			&power_state1);

		if ((power_state1 & regs[PMU_PWR_STATUS].mask) == 0x0) {
			regmap_read(regs[RESET].gpr, 0x0, &ap_ahb_regs);
			if ((ap_ahb_regs & (BIT(7) | BIT(2))) != (BIT(7) | BIT(2))) {
				pr_info("ap ahb 0x%x\n", ap_ahb_regs);
				ret = regmap_update_bits(regs[RESET].gpr, 0x0, BIT(7) | BIT(2),
					BIT(7) | BIT(2));
				if (ret) {
					pr_err("vsp_iova regmap_update_bits failed %s, %d\n",
							__func__, __LINE__);
				}
			}
		} else {
			pr_info("offset 0x%x, power 0x%x\n", regs[PMU_PWR_STATUS].reg, power_state1);
		}
	}
#endif
}

int vsp_get_iova(struct vsp_dev_t *vsp_hw_dev,
		 struct vsp_iommu_map_data *mapdata, void __user *arg)
{
	int ret = 0;
	u32 power_state1 = 0, vsp_eb_reg = 0;
	struct sprd_iommu_map_data iommu_map_data = {0};
	struct sprd_iommu_unmap_data iommu_ummap_data = {0};
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attachment = NULL;
	struct sg_table *table = NULL;
	struct vsp_iommu_map_entry *entry = NULL;

	vsp_clk_enable(vsp_hw_dev);
	vsp_check_pw_status(vsp_hw_dev);
	ret = vsp_get_dmabuf(mapdata->fd, &dmabuf,
				&(iommu_map_data.buf),
				&iommu_map_data.iova_size);
	if (ret) {
		pr_err("vsp_get_dmabuf failed: ret=%d\n", ret);
		goto err_get_dmabuf;
	}

	attachment = dma_buf_attach(dmabuf, vsp_hw_dev->vsp_dev);
	if (IS_ERR_OR_NULL(attachment)) {
		pr_err("Failed to attach dmabuf=%p\n", dmabuf);
		ret = PTR_ERR(attachment);
		goto err_attach;
	}


	table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(table)) {
		pr_err("Failed to map attachment=%p\n", attachment);
		ret = PTR_ERR(table);
		goto err_map_attachment;
	}

	iommu_map_data.ch_type = SPRD_IOMMU_FM_CH_RW;
	ret = sprd_iommu_map(vsp_hw_dev->vsp_dev, &iommu_map_data);
	if (!ret) {
		mutex_lock(&vsp_hw_dev->map_lock);
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			mutex_unlock(&vsp_hw_dev->map_lock);
			iommu_ummap_data.iova_addr = iommu_map_data.iova_addr;
			iommu_ummap_data.iova_size = iommu_map_data.iova_size;
			iommu_ummap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_ummap_data.buf = NULL;
			ret = -ENOMEM;
			pr_err("fatal error! kzalloc fail!, ret=%d\n", ret);
			goto err_kzalloc;
		}
		entry->fd = mapdata->fd;
		entry->iova_addr = iommu_map_data.iova_addr;
		entry->iova_size = iommu_map_data.iova_size;
		entry->dmabuf = dmabuf;
		entry->attachment = attachment;
		entry->table = table;
		list_add(&entry->list, &vsp_hw_dev->map_list);
		mutex_unlock(&vsp_hw_dev->map_lock);

		mapdata->iova_addr = iommu_map_data.iova_addr;
		mapdata->size = iommu_map_data.iova_size;
		pr_info("vsp iommu map success iova addr=%#llx size=%llu\n",
			mapdata->iova_addr, mapdata->size);
		ret = copy_to_user((void __user *)arg, (void *)mapdata,
					sizeof(struct vsp_iommu_map_data));
		if (ret) {
			pr_err("fatal error! copy_to_user failed, ret=%d\n", ret);
			goto err_copy_to_user;
		}
		pr_info("suceess to add map_node(iova_addr=%#llx, size=%llu)\n",
			mapdata->iova_addr, mapdata->size);
	} else {
		pr_err("vsp iommu map failed, ret=%d, map_size=%zu\n",
			ret, iommu_map_data.iova_size);
		goto err_iommu_map;
	}

	if (vsp_hw_dev->version == SHARKL3) {
		regmap_read(regs[PMU_PWR_STATUS].gpr, regs[PMU_PWR_STATUS].reg,
			&power_state1);
		regmap_read(regs[VSP_DOMAIN_EB].gpr, regs[VSP_DOMAIN_EB].reg,
			&vsp_eb_reg);
		pr_debug("reg 0x%x, vsp_power 0x%x, reg 0x%x, vsp_eb 0x%x\n",
			regs[PMU_PWR_STATUS].reg, power_state1, regs[VSP_DOMAIN_EB].reg,
			vsp_eb_reg);
	}

	vsp_clk_disable(vsp_hw_dev);
	return ret;

err_copy_to_user:
	mutex_lock(&vsp_hw_dev->map_lock);
	list_del(&entry->list);
	kfree(entry);
	mutex_unlock(&vsp_hw_dev->map_lock);
err_kzalloc:
	ret = sprd_iommu_unmap(vsp_hw_dev->vsp_dev, &iommu_ummap_data);
	if (ret) {
		pr_err("sprd_iommu_unmap failed, ret=%d, addr&size: 0x%lx 0x%zx\n",
			ret, iommu_ummap_data.iova_addr, iommu_ummap_data.iova_size);
	}
err_iommu_map:
	dma_buf_unmap_attachment(attachment, table, DMA_BIDIRECTIONAL);
err_map_attachment:
	dma_buf_detach(dmabuf, attachment);
err_attach:
	dma_buf_put(dmabuf);
err_get_dmabuf:
	vsp_clk_disable(vsp_hw_dev);

	return ret;

}

int vsp_free_iova(struct vsp_dev_t *vsp_hw_dev,
		  struct vsp_iommu_map_data *ummapdata)
{
	int ret = 0;
	int b_find = 0;
	struct vsp_iommu_map_entry *entry = NULL;
	struct sprd_iommu_unmap_data iommu_ummap_data = {0};

	vsp_clk_enable(vsp_hw_dev);
	list_for_each_entry(entry, &vsp_hw_dev->map_list, list) {
		if (entry->iova_addr == ummapdata->iova_addr &&
			entry->iova_size == ummapdata->size) {
			b_find = 1;
			break;
		}
	}
	if (b_find) {
		iommu_ummap_data.iova_addr = entry->iova_addr;
		iommu_ummap_data.iova_size = entry->iova_size;
		iommu_ummap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		iommu_ummap_data.buf = NULL;
		list_del(&entry->list);
		pr_info("success to find node(inst %p, iova_addr=%#llx, size=%llu)\n",
			ummapdata->iova_addr, ummapdata->size);
	} else {
		pr_err("fatal error! not find node(inst %p, iova_addr=%#llx, size=%llu)\n",
			ummapdata->iova_addr, ummapdata->size);
		mutex_unlock(&vsp_hw_dev->map_lock);
		vsp_clk_disable(vsp_hw_dev);
		return -EFAULT;
	}
	mutex_unlock(&vsp_hw_dev->map_lock);

	ret = sprd_iommu_unmap(vsp_hw_dev->vsp_dev,
					&iommu_ummap_data);

	if (ret) {
		pr_err("vsp iommu-unmap failed ret %d addr&size 0x%lx 0x%zx\n",
			ret, ummapdata->iova_addr, ummapdata->size);
		vsp_clk_disable(vsp_hw_dev);
		return ret;
	}
	pr_debug("vsp iommu-unmap success iova addr 0x%lx size 0x%zx\n",
			ummapdata->iova_addr, ummapdata->size);
	dma_buf_unmap_attachment(entry->attachment, entry->table, DMA_BIDIRECTIONAL);
	dma_buf_detach(entry->dmabuf, entry->attachment);
	dma_buf_put(entry->dmabuf);
	kfree(entry);

	vsp_clk_disable(vsp_hw_dev);
	return ret;
}

int vsp_get_mm_clk(struct vsp_dev_t *vsp_hw_dev)
{
	int ret = 0;
	struct clk *clk_mm_eb;
	struct clk *clk_vsp_mq_ahb_eb;
	struct clk *clk_vsp_ahb_mmu_eb;
	struct clk *clk_axi_gate_vsp;
	struct clk *clk_ahb_gate_vsp_eb;
#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	struct clk *clk_vsp;
#endif
	struct clk *clk_ahb_vsp;
	struct clk *clk_emc_vsp;
	struct clk *clk_parent;

	if (vsp_hw_dev->version == SHARKL3  || vsp_hw_dev->version == PIKE2) {
		clk_mm_eb = devm_clk_get(vsp_hw_dev->vsp_dev, "clk_mm_eb");

		if (IS_ERR_OR_NULL(clk_mm_eb)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
				"clk_mm_eb", clk_mm_eb);
			vsp_hw_dev->clk_mm_eb = NULL;
			ret = -EINVAL;
			goto errout;
		} else
			vsp_hw_dev->clk_mm_eb = clk_mm_eb;

		clk_axi_gate_vsp = devm_clk_get(vsp_hw_dev->vsp_dev,
			"clk_axi_gate_vsp");
		if (IS_ERR_OR_NULL(clk_axi_gate_vsp)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
				"clk_axi_gate_vsp",
			clk_axi_gate_vsp);
			vsp_hw_dev->clk_axi_gate_vsp = NULL;
			vsp_hw_dev->light_sleep_en = false;
			goto errout;
		} else {
			vsp_hw_dev->clk_axi_gate_vsp = clk_axi_gate_vsp;
			vsp_hw_dev->light_sleep_en = false;
		}
	}

	clk_ahb_gate_vsp_eb =
	    devm_clk_get(vsp_hw_dev->vsp_dev, "clk_ahb_gate_vsp_eb");

	if (IS_ERR_OR_NULL(clk_ahb_gate_vsp_eb)) {
		pr_err("Failed: Can't get clock [%s]! %p\n",
		       "clk_ahb_gate_vsp_eb", clk_ahb_gate_vsp_eb);
		ret = -EINVAL;
		vsp_hw_dev->clk_ahb_gate_vsp_eb = NULL;
		goto errout;
	} else
		vsp_hw_dev->clk_ahb_gate_vsp_eb = clk_ahb_gate_vsp_eb;

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	clk_vsp = devm_clk_get(vsp_hw_dev->vsp_dev, "clk_vsp");

	if (IS_ERR_OR_NULL(clk_vsp)) {
		pr_err("Failed: Can't get clock [%s]! %p\n", "clk_vsp",
		       clk_vsp);
		ret = -EINVAL;
		vsp_hw_dev->vsp_clk = NULL;
		goto errout;
	} else
		vsp_hw_dev->vsp_clk = clk_vsp;
#endif

	if (vsp_hw_dev->version == SHARKL3) {
		clk_ahb_vsp =
			devm_clk_get(vsp_hw_dev->vsp_dev, "clk_ahb_vsp");

		if (IS_ERR_OR_NULL(clk_ahb_vsp)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
				   "clk_ahb_vsp", clk_ahb_vsp);
			ret = -EINVAL;
			goto errout;
		} else
			vsp_hw_dev->clk_ahb_vsp = clk_ahb_vsp;

		clk_parent = devm_clk_get(vsp_hw_dev->vsp_dev,
				"clk_ahb_vsp_parent");
		if (IS_ERR_OR_NULL(clk_parent)) {
			pr_err("clock[%s]: failed to get parent in probe!\n",
				 "clk_ahb_vsp_parent");
			ret = -EINVAL;
			goto errout;
		} else
			vsp_hw_dev->ahb_parent_clk = clk_parent;

		clk_emc_vsp =
			devm_clk_get(vsp_hw_dev->vsp_dev, "clk_emc_vsp");

		if (IS_ERR_OR_NULL(clk_emc_vsp)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
				   "clk_emc_vsp", clk_emc_vsp);
			ret = -EINVAL;
			goto errout;
		} else
			vsp_hw_dev->clk_emc_vsp = clk_emc_vsp;

		clk_parent = devm_clk_get(vsp_hw_dev->vsp_dev,
				"clk_emc_vsp_parent");
		if (IS_ERR_OR_NULL(clk_parent)) {
			pr_err("clock[%s]: failed to get parent in probe!\n",
				 "clk_emc_vsp_parent");
			ret = -EINVAL;
			goto errout;
		} else
			vsp_hw_dev->emc_parent_clk = clk_parent;

		clk_vsp_ahb_mmu_eb =
			devm_clk_get(vsp_hw_dev->vsp_dev,
				"clk_vsp_ahb_mmu_eb");

		if (IS_ERR_OR_NULL(clk_vsp_ahb_mmu_eb)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
				   "clk_vsp_ahb_mmu_eb", clk_vsp_ahb_mmu_eb);
			ret = -EINVAL;
			vsp_hw_dev->clk_vsp_ahb_mmu_eb = NULL;
			goto errout;
		} else
			vsp_hw_dev->clk_vsp_ahb_mmu_eb = clk_vsp_ahb_mmu_eb;
	}

	if (vsp_hw_dev->version == PIKE2) {
		clk_vsp_mq_ahb_eb =
		    devm_clk_get(vsp_hw_dev->vsp_dev, "clk_vsp_mq_ahb_eb");

		if (IS_ERR(clk_vsp_mq_ahb_eb)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
			       "clk_vsp_mq_ahb_eb", clk_vsp_mq_ahb_eb);
			ret = PTR_ERR(clk_vsp_mq_ahb_eb);
			vsp_hw_dev->clk_vsp_mq_ahb_eb = NULL;
			goto errout;
		} else {
			vsp_hw_dev->clk_vsp_mq_ahb_eb = clk_vsp_mq_ahb_eb;
		}
	}

errout:
	return ret;
}

int vsp_clk_enable(struct vsp_dev_t *vsp_hw_dev)
{
	int ret = 0;

	if (vsp_hw_dev->version == SHARKL3  || vsp_hw_dev->version == PIKE2) {
		if (vsp_hw_dev->clk_mm_eb) {
			ret = clk_prepare_enable(vsp_hw_dev->clk_mm_eb);
			if (ret) {
				pr_err("vsp clk_mm_eb: clk_enable failed!\n");
				return ret;
			}
			pr_debug("vsp clk_mm_eb: clk_prepare_enable ok.\n");
		}
	}

	if (vsp_hw_dev->clk_ahb_gate_vsp_eb != NULL) {
		ret = clk_prepare_enable(vsp_hw_dev->clk_ahb_gate_vsp_eb);
		if (ret) {
			pr_err("clk_ahb_gate_vsp_eb: clk_prepare_enable failed!\n");
			return ret;
		}
		pr_debug("clk_ahb_gate_vsp_eb: clk_prepare_enable ok.\n");
	}

	if (vsp_hw_dev->version == PIKE2) {
		if (vsp_hw_dev->clk_axi_gate_vsp) {
			ret = clk_prepare_enable(vsp_hw_dev->clk_axi_gate_vsp);
			if (ret) {
				pr_err("clk_axi_gate_vsp: clk_prepare_enable fail!\n");
				return ret;
			}
			pr_debug("clk_axi_gate_vsp: clk_prepare_enable ok.\n");
		}

		if (vsp_hw_dev->clk_vsp_mq_ahb_eb) {
			ret = clk_prepare_enable(vsp_hw_dev->clk_vsp_mq_ahb_eb);
			if (ret) {
				pr_err("clk_vsp_mq_ahb_eb: clk_enable fail!\n");
				return ret;
			}
			pr_debug("clk_vsp_mq_ahb_eb: clk_prepare_enable ok.\n");
		}
	}

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	ret = clk_set_parent(vsp_hw_dev->vsp_clk, vsp_hw_dev->vsp_parent_clk);
	if (ret) {
		pr_err("clock[%s]: clk_set_parent() failed!", "clk_vsp");
		return -EINVAL;
	}

	ret = clk_prepare_enable(vsp_hw_dev->vsp_clk);
	if (ret) {
		pr_err("vsp_clk: clk_prepare_enable failed!\n");
		return ret;
	}
	pr_debug("vsp_clk: clk_prepare_enable ok.\n");
#endif

	if (vsp_hw_dev->version == SHARKL3) {
		ret = clk_prepare_enable(vsp_hw_dev->clk_vsp_ahb_mmu_eb);
		if (ret) {
			pr_err("clk_vsp_ahb_mmu_eb: clk_prepare_enable failed!\n");
			return ret;
		}
		pr_debug("clk_vsp_ahb_mmu_eb: clk_prepare_enable ok.\n");

		if (vsp_hw_dev->clk_axi_gate_vsp) {
			ret = clk_prepare_enable(vsp_hw_dev->clk_axi_gate_vsp);
			if (ret) {
				pr_err("clk_axi_gate_vsp: clk_prepare_enable failed!\n");
				return ret;
			}
			pr_debug("clk_axi_gate_vsp: clk_prepare_enable ok.\n");
		}

		ret = clk_set_parent(vsp_hw_dev->clk_ahb_vsp,
				   vsp_hw_dev->ahb_parent_clk);
		if (ret) {
			pr_err("clock[%s]: clk_set_parent() failed!",
				   "ahb_parent_clk");
			return -EINVAL;
		}

		ret = clk_prepare_enable(vsp_hw_dev->clk_ahb_vsp);
		if (ret) {
			pr_err("clk_ahb_vsp: clk_prepare_enable failed!\n");
			return ret;
		}
		pr_debug("clk_ahb_vsp: clk_prepare_enable ok.\n");

		ret = clk_set_parent(vsp_hw_dev->clk_emc_vsp,
				   vsp_hw_dev->emc_parent_clk);
		if (ret) {
			pr_err("clock[%s]: clk_set_parent() failed!",
				   "emc_parent_clk");
			return -EINVAL;
		}

		ret = clk_prepare_enable(vsp_hw_dev->clk_emc_vsp);
		if (ret) {
			pr_err("clk_emc_vsp: clk_prepare_enable failed!\n");
			return ret;
		}
		pr_debug("clk_emc_vsp: clk_prepare_enable ok.\n");
	}

	return ret;
}

void vsp_clk_disable(struct vsp_dev_t *vsp_hw_dev)
{
	if (vsp_hw_dev->version == SHARKL3) {
		clk_disable_unprepare(vsp_hw_dev->clk_emc_vsp);
		clk_disable_unprepare(vsp_hw_dev->clk_ahb_vsp);
		clk_disable_unprepare(vsp_hw_dev->clk_axi_gate_vsp);
		clk_disable_unprepare(vsp_hw_dev->clk_vsp_ahb_mmu_eb);
	}
#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	clk_disable_unprepare(vsp_hw_dev->vsp_clk);
#endif
	if (vsp_hw_dev->version == PIKE2) {
		clk_disable_unprepare(vsp_hw_dev->clk_axi_gate_vsp);
		clk_disable_unprepare(vsp_hw_dev->clk_vsp_mq_ahb_eb);
	}
	clk_disable_unprepare(vsp_hw_dev->clk_ahb_gate_vsp_eb);
	if (vsp_hw_dev->version == SHARKL3  || vsp_hw_dev->version == PIKE2)
		clk_disable_unprepare(vsp_hw_dev->clk_mm_eb);
}

