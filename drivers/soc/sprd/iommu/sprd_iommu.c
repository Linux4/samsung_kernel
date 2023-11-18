/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dma-buf.h>

#include "ion.h"
#include "sprd_iommu_sysfs.h"
#include "drv/com/sprd_com.h"

BLOCKING_NOTIFIER_HEAD(sprd_iommu_notif_chain);

static int sprd_iommu_probe(struct platform_device *pdev);
static int sprd_iommu_remove(struct platform_device *pdev);


int sprd_iommu_notifier_call_chain(void *data)
{
	return blocking_notifier_call_chain(&sprd_iommu_notif_chain, 0, data);
}

static struct sprd_iommu_list_data sprd_iommu_list[SPRD_IOMMU_MAX] = {
	{ .iommu_id = SPRD_IOMMU_VSP,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_VSP1,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_VSP2,
	  .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_DCAM,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_DCAM1,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_CPP,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_GSP,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_JPG,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_DISP,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_ISP,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_ISP1,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_FD,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_NPU,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_EPP,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_EDP,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_IDMA,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_VDMA,
	   .iommu_dev = NULL},
};

static const struct of_device_id sprd_iommu_ids[] = {
	{ .compatible = "sprd,iommuex-gsp",
	   .data = (void *)(IOMMU_EX_GSP)},

	{ .compatible = "sprd,iommuex-dispc",
	   .data = (void *)(IOMMU_EX_DISP)},

	{ .compatible = "sprd,iommuex-vsp",
	   .data = (void *)(IOMMU_EX_VSP)},

	{ .compatible = "sprd,iommuex-dcam",
	   .data = (void *)(IOMMU_EX_DCAM)},

	{ .compatible = "sprd,iommuex-isp",
	   .data = (void *)(IOMMU_EX_ISP)},

	{ .compatible = "sprd,iommuex-newisp",
	   .data = (void *)(IOMMU_EX_NEWISP)},

	{ .compatible = "sprd,iommuex-cpp",
	   .data = (void *)(IOMMU_EX_CPP)},

	{ .compatible = "sprd,iommuex-jpg",
	   .data = (void *)(IOMMU_EX_JPG)},

	{ .compatible = "sprd,iommuex-fd",
	   .data = (void *)(IOMMU_EX_FD)},

	{ .compatible = "sprd,iommuex-ai",
	   .data = (void *)(IOMMU_EX_NPU)},

	{ .compatible = "sprd,iommuex-epp",
	   .data = (void *)(IOMMU_EX_EPP)},

	{ .compatible = "sprd,iommuex-edp",
	   .data = (void *)(IOMMU_EX_EDP)},

	{ .compatible = "sprd,iommuex-idma",
	   .data = (void *)(IOMMU_EX_IDMA)},

	{ .compatible = "sprd,iommuex-vdma",
	   .data = (void *)(IOMMU_EX_VDMA)},

	{ .compatible = "sprd,iommuvau-gsp",
	   .data = (void *)(IOMMU_VAU_GSP)},

	{ .compatible = "sprd,iommuvau-gsp1",
	   .data = (void *)(IOMMU_VAU_GSP1)},

	{ .compatible = "sprd,iommuvau-dispc",
	   .data = (void *)(IOMMU_VAU_DISP)},

	{ .compatible = "sprd,iommuvau-dispc1",
	   .data = (void *)(IOMMU_VAU_DISP1)},

	{ .compatible = "sprd,iommuvau-vsp",
	   .data = (void *)(IOMMU_VAU_VSP)},

	{ .compatible = "sprd,iommuvau-vsp1",
	  .data = (void *)(IOMMU_VAU_VSP1)},

	{ .compatible = "sprd,iommuvau-vsp2",
	  .data = (void *)(IOMMU_VAU_VSP2)},

	{ .compatible = "sprd,iommuvau-dcam",
	   .data = (void *)(IOMMU_VAU_DCAM)},

	{ .compatible = "sprd,iommuvau-dcam1",
	  .data = (void *)(IOMMU_VAU_DCAM1)},

	{ .compatible = "sprd,iommuvau-isp",
	   .data = (void *)(IOMMU_VAU_ISP)},

	{ .compatible = "sprd,iommuvau-cpp",
	   .data = (void *)(IOMMU_VAU_CPP)},

	{ .compatible = "sprd,iommuvau-jpg",
	   .data = (void *)(IOMMU_VAU_JPG)},

	{ .compatible = "sprd,iommuvau-fd",
	  .data = (void *)(IOMMU_VAU_FD)},

	{ .compatible = "sprd,iommuvau-ai",
	  .data = (void *)(IOMMU_VAU_NPU)},

	{ .compatible = "sprd,iommuvau-epp",
	  .data = (void *)(IOMMU_VAU_EPP)},

	{ .compatible = "sprd,iommuvau-edp",
	  .data = (void *)(IOMMU_VAU_EDP)},

	{ .compatible = "sprd,iommuvau-idma",
	  .data = (void *)(IOMMU_VAU_IDMA)},

	{ .compatible = "sprd,iommuvau-vdma",
	  .data = (void *)(IOMMU_VAU_VDMA)},
};

static struct platform_driver iommu_driver = {
	.probe = sprd_iommu_probe,
	.remove = sprd_iommu_remove,
	.driver = {
		.name = "sprd_iommu_drv",
		.of_match_table = sprd_iommu_ids,
	},
};

static void sprd_iommu_set_list(struct sprd_iommu_dev *iommu_dev)
{
	if (!iommu_dev) {
		pr_err("%s, iommu_dev == NULL!\n", __func__);
		return;
	}

	switch (iommu_dev->init_data->id) {
	case IOMMU_EX_VSP:
	case IOMMU_VAU_VSP:
		sprd_iommu_list[SPRD_IOMMU_VSP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VSP;
		break;
	case IOMMU_VAU_VSP1:
		sprd_iommu_list[SPRD_IOMMU_VSP1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VSP1;
		break;
	case IOMMU_VAU_VSP2:
		sprd_iommu_list[SPRD_IOMMU_VSP2].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VSP2;
		break;
	case IOMMU_EX_DCAM:
	case IOMMU_VAU_DCAM:
		sprd_iommu_list[SPRD_IOMMU_DCAM].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DCAM;
		break;
	case IOMMU_VAU_DCAM1:
		sprd_iommu_list[SPRD_IOMMU_DCAM1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DCAM1;
		break;
	case IOMMU_EX_CPP:
	case IOMMU_VAU_CPP:
		sprd_iommu_list[SPRD_IOMMU_CPP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_CPP;
		break;
	case IOMMU_EX_GSP:
	case IOMMU_VAU_GSP:
		sprd_iommu_list[SPRD_IOMMU_GSP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_GSP;
		break;
	case IOMMU_VAU_GSP1:
		sprd_iommu_list[SPRD_IOMMU_GSP1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_GSP1;
		break;
	case IOMMU_EX_JPG:
	case IOMMU_VAU_JPG:
		sprd_iommu_list[SPRD_IOMMU_JPG].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_JPG;
		break;
	case IOMMU_EX_DISP:
	case IOMMU_VAU_DISP:
		sprd_iommu_list[SPRD_IOMMU_DISP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DISP;
		break;
	case IOMMU_VAU_DISP1:
		sprd_iommu_list[SPRD_IOMMU_DISP1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DISP1;
		break;
	case IOMMU_EX_ISP:
	case IOMMU_EX_NEWISP:
	case IOMMU_VAU_ISP:
		sprd_iommu_list[SPRD_IOMMU_ISP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_ISP;
		break;
	case IOMMU_EX_EPP:
	case IOMMU_VAU_EPP:
		sprd_iommu_list[SPRD_IOMMU_EPP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_EPP;
		break;
	case IOMMU_EX_EDP:
	case IOMMU_VAU_EDP:
		sprd_iommu_list[SPRD_IOMMU_EDP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_EDP;
		break;
	case IOMMU_EX_FD:
	case IOMMU_VAU_FD:
		sprd_iommu_list[SPRD_IOMMU_FD].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_FD;
		break;
	case IOMMU_EX_IDMA:
	case IOMMU_VAU_IDMA:
		sprd_iommu_list[SPRD_IOMMU_IDMA].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_IDMA;
		break;
	case IOMMU_EX_VDMA:
	case IOMMU_VAU_VDMA:
		sprd_iommu_list[SPRD_IOMMU_VDMA].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VDMA;
		break;

	case IOMMU_EX_NPU:
	case IOMMU_VAU_NPU:
		sprd_iommu_list[SPRD_IOMMU_NPU].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_NPU;
		break;
	default:
		pr_err("%s, no iommu id: %d\n", __func__,
			iommu_dev->init_data->id);
		iommu_dev->id = -1;
		break;
	}
}

static struct sprd_iommu_dev *sprd_iommu_get_subnode(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	struct of_phandle_args args;

	ret = of_parse_phandle_with_args(np, "iommus", "#iommu-cells", 0,
					 &args);
	if (ret) {
		IOMMU_ERR("of_parse_phandle_with_args(%s) => %d\n",
			np->full_name, ret);
		return NULL;
	}

	if (args.args_count != 0) {
		IOMMU_ERR("err param for %s (found %d, expected 0)\n",
			args.np->full_name, args.args_count);
		return NULL;
	}

	return args.np->data;
}

static struct sprd_iommu_dev *sprd_iommu_get_subnode_with_idx(
		struct device *dev, int idx)
{
	struct device_node *np = dev->of_node;
	int ret;
	struct of_phandle_args args;

	ret = of_parse_phandle_with_args(np, "iommus", "#iommu-cells", idx,
					 &args);
	if (ret) {
		IOMMU_ERR("of_parse_phandle_with_args(%s) => %d\n",
			np->full_name, ret);
		return NULL;
	}

	if (args.args_count != 0) {
		IOMMU_ERR("err param for %s (found %d, expected 0)\n",
			args.np->full_name, args.args_count);
		return NULL;
	}

	return args.np->data;
}

static bool sprd_iommu_target_buf(struct sprd_iommu_dev *iommu_dev,
						void *buf_addr,
						unsigned long *iova_addr)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (iommu_dev->sg_pool.slot[index].status == SG_SLOT_USED &&
		   iommu_dev->sg_pool.slot[index].buf_addr == buf_addr) {
			*iova_addr = iommu_dev->sg_pool.slot[index].iova_addr;
			iommu_dev->sg_pool.slot[index].map_usrs++;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_target_iova_find_buf(struct sprd_iommu_dev *iommu_dev,
						unsigned long iova_addr,
						size_t iova_size,
						void **buf)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (iommu_dev->sg_pool.slot[index].status == SG_SLOT_USED &&
		   iommu_dev->sg_pool.slot[index].iova_addr == iova_addr &&
		   iommu_dev->sg_pool.slot[index].iova_size == iova_size) {
			*buf = iommu_dev->sg_pool.slot[index].buf_addr;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_target_buf_find_iova(struct sprd_iommu_dev *iommu_dev,
						void *buf_addr,
						size_t iova_size,
						unsigned long *iova_addr)
{
	int index;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (iommu_dev->sg_pool.slot[index].status == SG_SLOT_USED &&
		   iommu_dev->sg_pool.slot[index].buf_addr == buf_addr &&
		   iommu_dev->sg_pool.slot[index].iova_size == iova_size) {
			*iova_addr = iommu_dev->sg_pool.slot[index].iova_addr;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_insert_slot(struct sprd_iommu_dev *iommu_dev,
						unsigned long sg_table_addr,
						void *buf_addr,
						unsigned long iova_addr,
						unsigned long iova_size)
{
	int index = 0;
	struct sprd_iommu_sg_rec *rec;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		rec = &(iommu_dev->sg_pool.slot[index]);
		if (rec->status == SG_SLOT_FREE) {
			rec->sg_table_addr = sg_table_addr;
			rec->buf_addr = buf_addr;
			rec->iova_addr = iova_addr;
			rec->iova_size = iova_size;
			rec->status = SG_SLOT_USED;
			rec->map_usrs++;
			iommu_dev->sg_pool.pool_cnt++;
			break;
		}
	}

	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

/**
* remove timeout sg table addr from sg pool
*/
static bool sprd_iommu_remove_sg_iova(struct sprd_iommu_dev *iommu_dev,
							unsigned long iova_addr,
							bool *be_free)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (SG_SLOT_USED == iommu_dev->sg_pool.slot[index].status) {
			if (iommu_dev->sg_pool.slot[index].iova_addr == iova_addr) {
				iommu_dev->sg_pool.slot[index].map_usrs--;

				if (0 == iommu_dev->sg_pool.slot[index].map_usrs) {
					iommu_dev->sg_pool.slot[index].sg_table_addr = 0;
					iommu_dev->sg_pool.slot[index].iova_addr = 0;
					iommu_dev->sg_pool.slot[index].iova_size = 0;
					iommu_dev->sg_pool.slot[index].status = SG_SLOT_FREE;
					iommu_dev->sg_pool.pool_cnt--;
					*be_free = true;
				} else
					*be_free = false;
				break;
			}
		}
	}

	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_clear_sg_iova(struct sprd_iommu_dev *iommu_dev,
			void *buf_addr,
			unsigned long sg_addr, unsigned long size,
			unsigned long *iova)
{
	int index;
	struct sprd_iommu_sg_rec *rec;
	bool ret = false;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		rec = &(iommu_dev->sg_pool.slot[index]);
		if (rec->status == SG_SLOT_USED &&
		    rec->buf_addr == buf_addr &&
		    rec->sg_table_addr == sg_addr &&
		    rec->iova_size == size) {
			rec->map_usrs = 0;
			rec->status = SG_SLOT_FREE;
			iommu_dev->sg_pool.pool_cnt--;
			*iova = rec->iova_addr;
			ret = true;
			break;
		}
	}

	return ret;
}

void sprd_iommu_pool_show(struct device *dev)
{
	int index;
	struct sprd_iommu_sg_rec *rec;
	struct sprd_iommu_dev *iommu_dev = NULL;

	if (!dev) {
		IOMMU_ERR("null parameter err!\n");
		return;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev\n");
		return;
	}

	IOMMU_INFO("%s restore, map_count %u\n",
		iommu_dev->init_data->name,
		iommu_dev->map_count);

	if (iommu_dev->map_count > 0)
		for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
			rec = &(iommu_dev->sg_pool.slot[index]);
			if (rec->status == SG_SLOT_USED) {
				IOMMU_ERR("buffer iova 0x%lx size 0x%lx sg 0x%lx buf %p map_usrs %d\n",
					rec->iova_addr, rec->iova_size,
					rec->sg_table_addr, rec->buf_addr,
					rec->map_usrs);
			}
		}
}
EXPORT_SYMBOL(sprd_iommu_pool_show);

void sprd_iommu_reg_dump(struct device *dev)
{
	int i;
	u32 *dump_regs;
	unsigned long ctrl_reg;
	struct sprd_iommu_dev *iommu_dev = NULL;

	if (!dev) {
		IOMMU_ERR("null parameter err!\n");
		return;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev\n");
		return;
	}

	dump_regs = iommu_dev->init_data->dump_regs;
	ctrl_reg = iommu_dev->init_data->ctrl_reg;

	for (i = 0; i < iommu_dev->init_data->dump_size; i++)
		dump_regs[i] = reg_read_dword(ctrl_reg + 4 * i);
}
EXPORT_SYMBOL(sprd_iommu_reg_dump);

void sprd_iommu_reg_show(struct device *dev)
{
	int i;
	u32 *dump_regs;
	struct sprd_iommu_dev *iommu_dev = NULL;

	if (!dev) {
		IOMMU_ERR("null parameter err!\n");
		return;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev\n");
		return;
	}

	dump_regs = (u32 *)(iommu_dev->init_data->dump_regs);

	for (i = 0; i < iommu_dev->init_data->dump_size; i += 4)
		IOMMU_INFO("%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", i * 4,
			dump_regs[i], dump_regs[i + 1],
			dump_regs[i + 2], dump_regs[i + 3]);
}
EXPORT_SYMBOL(sprd_iommu_reg_show);

int sprd_iommu_attach_device(struct device *dev)
{
	struct device_node *np = NULL;
	struct of_phandle_args args;
	const char *status;
	int ret;
	int err = -1;

	if (!dev) {
		IOMMU_ERR("null parameter err!\n");
		return -EINVAL;
	}

	np = dev->of_node;
	/*
	 * An iommu master has an iommus property containing a list of phandles
	 * to iommu nodes, each with an #iommu-cells property with value 0.
	 */
	ret = of_parse_phandle_with_args(np, "iommus", "#iommu-cells", 0,
					 &args);
	if (!ret) {
		ret = of_property_read_string(args.np, "status", &status);
		if (!ret) {
			if (!strcmp("disabled", status))
				err = -EPERM;
			else
				err = 0;
		} else
			err = -EPERM;
	} else
		err = -EPERM;

	return err;
}
EXPORT_SYMBOL(sprd_iommu_attach_device);

int sprd_iommu_map(struct device *dev, struct sprd_iommu_map_data *data)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	unsigned long iova = 0;
	unsigned long flag = 0;
	bool buf_cached = false;
	bool buf_insert = true;
	struct sg_table *table = NULL;

	if (!dev || !data) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (!(data->buf)) {
		IOMMU_ERR("null buf pointer!\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	ret = sprd_ion_get_sg(data->buf, &table);
	if (ret || !table) {
		IOMMU_ERR("%s get sg error, buf %p size 0x%zx ret %d table %p\n",
			  iommu_dev->init_data->name,
			  data->buf, data->iova_size, ret, table);
		data->iova_addr = 0;
		ret = -EINVAL;
		goto out;
	}

	/**search the sg_cache_pool to identify if buf already mapped;
	* if yes, return cached iova directly, otherwise, alloc new iova for it;
	*/
	buf_cached = sprd_iommu_target_buf(iommu_dev,
				data->buf,
				(unsigned long *)&iova);
	if (buf_cached) {
		data->iova_addr = iova;
		ret = 0;
		IOMMU_DEBUG("%s cached iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		goto out;
	}

	/*new sg, alloc for it*/
	iova = iommu_dev->ops->iova_alloc(iommu_dev,
			data->iova_size, data);
	if (iova == 0) {
		IOMMU_ERR("%s alloc error iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}

	ret = iommu_dev->ops->iova_map(iommu_dev,
			iova, data->iova_size, table, data);

	if (ret) {
		IOMMU_ERR("%s error, iova 0x%lx size 0x%zx ret %d buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, ret, data->buf);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}
	iommu_dev->map_count++;
	data->iova_addr = iova;
	buf_insert = sprd_iommu_insert_slot(iommu_dev,
				(unsigned long)table,
				data->buf,
				data->iova_addr,
				data->iova_size);
	if (!buf_insert) {
		IOMMU_ERR("%s error pool full iova 0x%lx size 0x%zx buf %p\n",
				iommu_dev->init_data->name, iova,
				data->iova_size, data->buf);
		iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		ret = -ENOMEM;
		goto out;
	}

	IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
		  iommu_dev->init_data->name, iova,
		  data->iova_size, data->buf);

	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;
}
EXPORT_SYMBOL(sprd_iommu_map);

int sprd_iommu_map_single_page(struct device *dev, struct sprd_iommu_map_data *data)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	unsigned long iova = 0;
	unsigned long flag = 0;
	bool buf_cached = false;
	bool buf_insert = true;
	struct sg_table *table = NULL;

	if (dev == NULL || data == NULL) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (data->buf == NULL) {
		IOMMU_ERR("null buf pointer!\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	ret = sprd_ion_get_sg(data->buf, &table);
	if (ret || table == NULL) {
		IOMMU_ERR("%s get sg error, buf %p size 0x%zx ret %d table %p\n",
			  iommu_dev->init_data->name,
			  data->buf, data->iova_size, ret, table);
		data->iova_addr = 0;
		ret = -EINVAL;
		goto out;
	}

	/*
	 * search the sg_cache_pool to identify if buf already mapped;
	 * if yes, return cached iova directly, otherwise, alloc new iova for it;
	 */
	buf_cached = sprd_iommu_target_buf(iommu_dev, data->buf,
								      (unsigned long *)&iova);
	if (buf_cached) {
		data->iova_addr = iova;
		ret = 0;
		IOMMU_DEBUG("%s cached iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		goto out;
	}

	/*new sg, alloc for it*/
	iova = iommu_dev->ops->iova_alloc(iommu_dev,
			data->iova_size, data);
	if (iova == 0) {
		IOMMU_ERR("%s alloc error iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}

	ret = iommu_dev->ops->iova_map(iommu_dev,
			iova, data->iova_size, NULL, data);
	if (ret) {
		IOMMU_ERR("%s error, iova 0x%lx size 0x%zx ret %d buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, ret, data->buf);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}
	iommu_dev->map_count++;
	data->iova_addr = iova;
	buf_insert = sprd_iommu_insert_slot(iommu_dev,
				(unsigned long)table,
				data->buf,
				data->iova_addr,
				data->iova_size);
	if (!buf_insert) {
		IOMMU_ERR("%s error pool full iova 0x%lx size 0x%zx buf %p\n",
				iommu_dev->init_data->name, iova,
				data->iova_size, data->buf);
		iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		ret = -ENOMEM;
		goto out;
	}

	IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
		  iommu_dev->init_data->name, iova,
		  data->iova_size, data->buf);

	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;
}
EXPORT_SYMBOL_GPL(sprd_iommu_map_single_page);

int sprd_iommu_map_with_idx(
		struct device *dev,
		struct sprd_iommu_map_data *data, int idx)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	unsigned long iova = 0;
	unsigned long flag = 0;
	bool buf_cached = false;
	bool buf_insert = true;
	struct sg_table *table = NULL;

	if (!dev || !data) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (!(data->buf)) {
		IOMMU_ERR("null buf pointer!\n");
		return -EINVAL;
	}


	iommu_dev = sprd_iommu_get_subnode_with_idx(dev, idx);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev idx %d\n", idx);
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	ret = sprd_ion_get_sg(data->buf, &table);
	if (ret || !table) {
		IOMMU_ERR("%s sg error,buf %p size 0x%zx ret %d table %p\n",
			  iommu_dev->init_data->name,
			  data->buf, data->iova_size, ret, table);
		data->iova_addr = 0;
		ret = -EINVAL;
		goto out;
	}

	/**search the sg_cache_pool to identify if buf already mapped;*/
	/* if yes, return cached iova directly, otherwise, */
	/* alloc new iova for it;*/
	buf_cached = sprd_iommu_target_buf(iommu_dev,
				data->buf,
				(unsigned long *)&iova);
	if (buf_cached) {
		data->iova_addr = iova;
		ret = 0;
		IOMMU_DEBUG("%s cached iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		goto out;
	}

	/*new sg, alloc for it*/
	iova = iommu_dev->ops->iova_alloc(iommu_dev,
			data->iova_size, data);
	if (iova == 0) {
		IOMMU_ERR("%s alloc error iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}

	ret = iommu_dev->ops->iova_map(iommu_dev,
			iova, data->iova_size, table, data);
	if (ret) {
		IOMMU_ERR("%s error, iova 0x%lx size 0x%zx ret %d buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, ret, data->buf);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}
	iommu_dev->map_count++;
	data->iova_addr = iova;
	buf_insert = sprd_iommu_insert_slot(iommu_dev,
				(unsigned long)table,
				data->buf,
				data->iova_addr,
				data->iova_size);
	if (!buf_insert) {
		IOMMU_ERR("%s error pool full iova 0x%lx size 0x%zx buf %p\n",
				iommu_dev->init_data->name, iova,
				data->iova_size, data->buf);
		iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		ret = -ENOMEM;
		goto out;
	}

	IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
		  iommu_dev->init_data->name, iova,
		  data->iova_size, data->buf);

	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);

	return ret;
}
EXPORT_SYMBOL(sprd_iommu_map_with_idx);

int sprd_iommu_unmap(struct device *dev, struct sprd_iommu_unmap_data *data)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	bool be_free = false;
	unsigned long flag = 0;
	bool valid_iova = false;
	bool valid_buf = false;
	void *buf;
	unsigned long iova;

	if (!dev || !data) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	/*check iova legal*/
	valid_iova = sprd_iommu_target_iova_find_buf(iommu_dev,
				data->iova_addr, data->iova_size, &buf);
	if (valid_iova) {
		iova = data->iova_addr;
	} else {
		valid_buf = sprd_iommu_target_buf_find_iova(iommu_dev,
					data->buf,
					data->iova_size, &iova);
		if (valid_buf) {
			buf = data->buf;
			data->iova_addr = iova;
		} else {
			IOMMU_ERR("%s illegal error iova 0x%lx buf %p size 0x%zx\n",
				iommu_dev->init_data->name,
				data->iova_addr, data->buf,
				data->iova_size);
			ret = -EINVAL;
			goto out;
		}
	}

	sprd_iommu_remove_sg_iova(iommu_dev, iova, &be_free);
	if (be_free) {
		ret = iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		if (ret)
			IOMMU_ERR("%s error iova 0x%lx 0x%zx buf %p\n",
				iommu_dev->init_data->name,
				iova, data->iova_size, buf);
		iommu_dev->map_count--;
		iommu_dev->ops->iova_free(iommu_dev,
			iova, data->iova_size);
		IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
	} else {
		IOMMU_DEBUG("%s cached iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
		ret = 0;
	}

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;
}
EXPORT_SYMBOL(sprd_iommu_unmap);

int sprd_iommu_unmap_with_idx(
		struct device *dev,
		struct sprd_iommu_unmap_data *data, int idx)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	bool be_free = false;
	unsigned long flag = 0;
	bool valid_iova = false;
	bool valid_buf = false;
	void *buf;
	unsigned long iova;

	if (!dev || !data) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode_with_idx(dev, idx);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev idx %d\n", idx);
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	/*check iova legal*/
	valid_iova = sprd_iommu_target_iova_find_buf(iommu_dev,
				data->iova_addr, data->iova_size, &buf);
	if (valid_iova) {
		iova = data->iova_addr;
	} else {
		valid_buf = sprd_iommu_target_buf_find_iova(iommu_dev,
					data->buf,
					data->iova_size, &iova);
		if (valid_buf) {
			buf = data->buf;
			data->iova_addr = iova;
		} else {
			IOMMU_ERR("%s illegal error iova 0x%lx buf %p size 0x%zx\n",
				iommu_dev->init_data->name,
				data->iova_addr, data->buf,
				data->iova_size);
			ret = -EINVAL;
			goto out;
		}
	}

	sprd_iommu_remove_sg_iova(iommu_dev, iova, &be_free);
	if (be_free) {
		ret = iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		if (ret)
			IOMMU_ERR("%s error iova 0x%lx 0x%zx buf %p\n",
				iommu_dev->init_data->name,
				iova, data->iova_size, buf);
		iommu_dev->map_count--;
		iommu_dev->ops->iova_free(iommu_dev,
			iova, data->iova_size);
		IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
	} else {
		IOMMU_DEBUG("%s cached iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
		ret = 0;
	}

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);

	return ret;
}
EXPORT_SYMBOL(sprd_iommu_unmap_with_idx);

int sprd_iommu_unmap_orphaned(struct sprd_iommu_dev *iommu_dev, struct sprd_iommu_unmap_data *data)
{
	int ret;
	unsigned long iova;
	unsigned long flag = 0;

	if (!data) {
		IOMMU_ERR("null parameter error! data %p\n", data);
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	ret = sprd_iommu_clear_sg_iova(iommu_dev, data->buf,
					(unsigned long)(data->table),
					data->iova_size, &iova);
	if (ret) {
		ret = iommu_dev->ops->iova_unmap_orphaned(iommu_dev,
						 iova, data->iova_size);
		iommu_dev->map_count--;
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		IOMMU_ERR("%s iova leak error, buf %p iova 0x%lx size 0x%zx\n",
			iommu_dev->init_data->name, data->buf,
			iova, data->iova_size);
	}

	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);

	return ret;
}

static int sprd_iommu_notify_callback(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct sprd_iommu_dev *iommu_dev = container_of(nb, struct sprd_iommu_dev, sprd_iommu_nb);
	struct sprd_iommu_unmap_data unmap_data = {0};

	if (iommu_dev) {
		unmap_data.buf = data;
		unmap_data.table = ((struct ion_buffer *)data)->sg_table;
		unmap_data.iova_size = ((struct ion_buffer *)data)->size;
		unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		sprd_iommu_unmap_orphaned(iommu_dev, &unmap_data);
	}

	return NOTIFY_OK;
}

int sprd_iommu_restore(struct device *dev)
{
	struct sprd_iommu_dev *iommu_dev = NULL;
	int ret = 0;

	if (!dev) {
		IOMMU_ERR("null parameter err!\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (!iommu_dev) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	if (iommu_dev->ops->restore)
		iommu_dev->ops->restore(iommu_dev);
	else
		ret = -1;

	if (iommu_dev->id != SPRD_IOMMU_VSP &&
	    iommu_dev->id != SPRD_IOMMU_VSP1 &&
	    iommu_dev->id != SPRD_IOMMU_VSP2 &&
	    iommu_dev->id != SPRD_IOMMU_DISP &&
	    iommu_dev->id != SPRD_IOMMU_DISP1)
		sprd_iommu_pool_show(dev);

	return ret;
}
EXPORT_SYMBOL(sprd_iommu_restore);

int sprd_iommu_set_cam_bypass(bool vaor_bp_en)
{
	struct sprd_iommu_dev *iommu_dev;
	int ret = 0;

	iommu_dev = sprd_iommu_list[SPRD_IOMMU_DCAM].iommu_dev;
	if (iommu_dev->ops->set_bypass)
		iommu_dev->ops->set_bypass(iommu_dev, vaor_bp_en);
	else
		ret = -1;

	iommu_dev = sprd_iommu_list[SPRD_IOMMU_ISP].iommu_dev;
	if (iommu_dev->ops->set_bypass)
		iommu_dev->ops->set_bypass(iommu_dev, vaor_bp_en);
	else
		ret = -1;

	return ret;
}
EXPORT_SYMBOL(sprd_iommu_set_cam_bypass);

static int sprd_iommu_get_resource(struct device_node *np,
				struct sprd_iommu_init_data *pdata)
{
	int err = 0;
	uint32_t val;
	struct resource res;
	struct device_node *np_memory;
	uint32_t out_values[4];

	err = of_address_to_resource(np, 0, &res);
	if (err < 0)
		return err;

	/*sharkl2 ctrl_reg is iommu base reg addr*/
	IOMMU_INFO("ctrl_reg phy:0x%lx\n", (unsigned long)(res.start));
	pdata->ctrl_reg = (unsigned long)ioremap_nocache(res.start,
		resource_size(&res));
	if (!(pdata->ctrl_reg)) {
		IOMMU_ERR("fail to remap ctrl_reg\n");
		return -ENOMEM;
	}
	IOMMU_INFO("ctrl_reg:0x%lx\n", pdata->ctrl_reg);
	pdata->dump_size = resource_size(&res) / 4;

	err = of_property_read_u32(np, "sprd,frc-reg", &val);
	if (!err) {
		IOMMU_INFO("frc_reg_addr:0x%x\n", val);
		pdata->frc_reg_addr = (unsigned long)ioremap_nocache(val,
				       PAGE_SIZE);
		if (!(pdata->frc_reg_addr)) {
			IOMMU_ERR("fail to remap frc_reg\n");
			return -ENOMEM;
		}
	}

	err = of_property_read_u32(np, "sprd,iova-base", &val);
	if (err < 0)
		return err;
	pdata->iova_base = val;
	err = of_property_read_u32(np, "sprd,iova-size", &val);
	if (err < 0)
		return err;
	pdata->iova_size = val;

	IOMMU_INFO("iova_base:0x%lx,iova_size:%zx\n",
		pdata->iova_base,
		pdata->iova_size);

	err = of_property_read_u32(np, "sprd,reserved-fault-page", &val);
	if (err) {
		unsigned long page =  0;

		page = __get_free_page(GFP_KERNEL);
		if (page)
			pdata->fault_page = virt_to_phys((void *)page);
		else
			pdata->fault_page = 0;

		IOMMU_INFO("fault_page: 0x%lx\n", pdata->fault_page);
	} else {
		IOMMU_INFO("reserved fault page phy:0x%x\n", val);
		pdata->fault_page = val;
	}

	/*get mmu page table reserved memory*/
	np_memory = of_parse_phandle(np, "memory-region", 0);
	if (!np_memory) {
		pdata->pagt_base_ddr = 0;
		pdata->pagt_ddr_size = 0;
	} else {
#ifdef CONFIG_64BIT
		err = of_property_read_u32_array(np_memory, "reg",
				out_values, 4);
		if (!err) {
			pdata->pagt_base_ddr = out_values[0];
			pdata->pagt_base_ddr =
					pdata->pagt_base_ddr << 32;
			pdata->pagt_base_ddr |= out_values[1];

			pdata->pagt_ddr_size = out_values[2];
			pdata->pagt_ddr_size =
					pdata->pagt_ddr_size << 32;
			pdata->pagt_ddr_size |= out_values[3];
		} else {
			pdata->pagt_base_ddr = 0;
			pdata->pagt_ddr_size = 0;
		}
#else
		err = of_property_read_u32_array(np_memory, "reg",
				out_values, 2);
		if (!err) {
			pdata->pagt_base_ddr = out_values[0];
			pdata->pagt_ddr_size = out_values[1];
		} else {
			pdata->pagt_base_ddr = 0;
			pdata->pagt_ddr_size = 0;
		}
#endif
	}

	return 0;
}

static int sprd_iommu_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device_node *np = pdev->dev.of_node;
	struct sprd_iommu_dev *iommu_dev = NULL;
	struct sprd_iommu_init_data *pdata = NULL;

	IOMMU_INFO("start\n");

	iommu_dev = kzalloc(sizeof(struct sprd_iommu_dev), GFP_KERNEL);
	if (!iommu_dev) {
		IOMMU_ERR("fail to kzalloc\n");
		return -ENOMEM;
	}

	pdata = kzalloc(sizeof(struct sprd_iommu_init_data), GFP_KERNEL);
	if (!pdata) {
		IOMMU_ERR("fail to kzalloc\n");
		kfree(iommu_dev);
		return -ENOMEM;
	}

	pdata->id = (int)((enum IOMMU_ID)
		((of_match_node(sprd_iommu_ids, np))->data));

	switch (pdata->id) {
	/*for sharkle iommu*/
	case IOMMU_EX_VSP:
	case IOMMU_EX_DCAM:
	case IOMMU_EX_CPP:
	case IOMMU_EX_GSP:
	case IOMMU_EX_JPG:
	case IOMMU_EX_DISP:
	case IOMMU_EX_ISP:
	case IOMMU_EX_NEWISP:
	case IOMMU_EX_FD:
	{
		iommu_dev->ops = &sprd_iommuex_hw_ops;
		break;
	}
	/*for roc1 iommu*/
	case IOMMU_VAU_VSP:
	case IOMMU_VAU_VSP1:
	case IOMMU_VAU_VSP2:
	case IOMMU_VAU_DCAM:
	case IOMMU_VAU_DCAM1:
	case IOMMU_VAU_CPP:
	case IOMMU_VAU_GSP:
	case IOMMU_VAU_GSP1:
	case IOMMU_VAU_JPG:
	case IOMMU_VAU_DISP:
	case IOMMU_VAU_DISP1:
	case IOMMU_VAU_ISP:
	case IOMMU_VAU_FD:
	case IOMMU_VAU_NPU:
	case IOMMU_VAU_EPP:
	case IOMMU_VAU_EDP:
	case IOMMU_VAU_IDMA:
	case IOMMU_VAU_VDMA:
	{
		iommu_dev->ops = &sprd_iommuvau_hw_ops;
		break;
	}

	default:
	{
		IOMMU_ERR("unknown iommu id %d\n", pdata->id);
		err = -ENOMEM;
		goto errout;
	}
	}

	err = sprd_iommu_get_resource(np, pdata);
	if (err) {
		IOMMU_ERR("no reg of property specified\n");
		goto errout;
	}

	/*If ddr frequency >= 500Mz, iommu need enable div2 frequency,
	 * because of limit of iommu iram frq.*/
	iommu_dev->init_data = pdata;
	iommu_dev->map_count = 0;
	iommu_dev->init_data->name = (char *)(
		(of_match_node(sprd_iommu_ids, np))->compatible);
	iommu_dev->init_data->dump_regs = kcalloc(iommu_dev->init_data->dump_size,
				       sizeof(u32), GFP_KERNEL);
	if (!iommu_dev->init_data->dump_regs)
		return -ENOMEM;

	err = iommu_dev->ops->init(iommu_dev, pdata);
	if (err) {
		IOMMU_ERR("iommu %s : failed init %d.\n", pdata->name, err);
		goto errout;
	}
	spin_lock_init(&iommu_dev->pgt_lock);
	memset(&iommu_dev->sg_pool, 0, sizeof(struct sprd_iommu_sg_pool));
#ifdef SPRD_DEBUG
	sprd_iommu_sysfs_create(iommu_dev, iommu_dev->init_data->name);
#endif
	platform_set_drvdata(pdev, iommu_dev);

	np->data  = iommu_dev;
	sprd_iommu_set_list(iommu_dev);
	iommu_dev->sprd_iommu_nb.notifier_call = sprd_iommu_notify_callback;
	err = blocking_notifier_chain_register(&sprd_iommu_notif_chain,
					       &iommu_dev->sprd_iommu_nb);
	IOMMU_INFO("%s end\n", iommu_dev->init_data->name);
	return 0;

errout:
	kfree(iommu_dev);
	kfree(pdata);
	return err;
}

static int sprd_iommu_remove(struct platform_device *pdev)
{
	struct sprd_iommu_dev *iommu_dev = platform_get_drvdata(pdev);

#ifdef SPRD_DEBUG
	sprd_iommu_sysfs_destroy(iommu_dev, iommu_dev->init_data->name);
#endif
	iommu_dev->ops->exit(iommu_dev);
	gen_pool_destroy(iommu_dev->pool);
	kfree(iommu_dev);
	return 0;
}

static int __init sprd_iommu_init(void)
{
	int err = -1;

	IOMMU_INFO("begin\n");
	err = platform_driver_register(&iommu_driver);
	if (err < 0)
		IOMMU_ERR("sprd_iommu register err: %d\n", err);
	IOMMU_INFO("end\n");
	return err;
}

static void __exit sprd_iommu_exit(void)
{
	platform_driver_unregister(&iommu_driver);
}

module_init(sprd_iommu_init);
module_exit(sprd_iommu_exit);

MODULE_DESCRIPTION("SPRD IOMMU Driver");
MODULE_LICENSE("GPL v2");
