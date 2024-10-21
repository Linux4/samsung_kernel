// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: test " fmt

/*
 * If you need to use iommu_test.ko, you must be
 * define "IOMMU_TEST_EN" as below and add dts node.
 */
/* #define IOMMU_TEST_EN */
/* #define SMMU_V3_TEST_EN */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/iommu.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <uapi/linux/dma-heap.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_IMPORT_NS(DMA_BUF);

#ifdef IOMMU_TEST_EN

#include "mtk_heap.h"
#include "iommu_debug.h"
#include "iommu_engine.h"

#include "../../../iommu/arm/arm-smmu-v3/mtk-smmu-v3.h"
#include "../../../iommu/arm/arm-smmu-v3/arm-smmu-v3.h"

#define SECURE_HANDLE_TEST

#define DEFINE_PROC_ATTRIBUTE(__fops, __get, __set, __fmt)		  \
static int __fops ## _open(struct inode *inode, struct file *file)	  \
{									  \
	struct inode local_inode = *inode;				  \
									  \
	local_inode.i_private = pde_data(inode);			  \
	__simple_attr_check_format(__fmt, 0ull);			  \
	return simple_attr_open(&local_inode, file, __get, __set, __fmt); \
}									  \
static const struct proc_ops __fops = {					  \
	.proc_open	 = __fops ## _open,				  \
	.proc_release    = simple_attr_release,				  \
	.proc_read	 = simple_attr_read,				  \
	.proc_write	 = simple_attr_write,				  \
	.proc_lseek	 = generic_file_llseek,				  \
}									  \

enum DMABUF_HEAP {
	TEST_REQ_SVP_REGION = 0, /* 0x0 normal region */
	TEST_REQ_SVP_REGION_ALIGN_UP, /* 0x1 by video up */
	TEST_REQ_SVP_REGION_ALIGN_VDEC, /* 0x2 by video up */
	TEST_REQ_SVP_REGION_PA, /* 0x3 PA */

	TEST_REQ_PROT_REGION = 10, /* 0xa normal region */
	TEST_REQ_PROT_REGION_ALIGN_CCU0, /* 0xb by CCU0 */
	TEST_REQ_PROT_REGION_PA, /* 0xc PA */

	TEST_REQ_SVP_PAGE = 20, /* 0x14 normal region */
	TEST_REQ_SVP_PAGE_UP, /* 0x15 special region video up */
	TEST_REQ_SVP_PAGE_VDEC, /* 0x16 PA */
	TEST_REQ_SVP_PAGE_PA, /* 0x17 PA */

	TEST_REQ_PROT_PAGE = 30, /* 0x1e normal region */
	TEST_REQ_PROT_PAGE_CCU0, /* 0x1f CCU0 */
	TEST_REQ_PROT_PAGE_PA, /* 0x20 PA */

	TEST_PROT_2D_FR_REGION = 40, /* 0x28:cancel */
	TEST_PROT_2D_FR_REGION_ALIGN, /* 0x29:cancel */

	TEST_SAPU_DATA_SHM_REGION = 50, /* 0x32 */
	TEST_SAPU_DATA_SHM_REGION_ALIGN, /* 0x33 */

	TEST_SAPU_ENGINE_SHM_REGION = 60, /* 0x3c */
	TEST_SAPU_ENGINE_SHM_REGION_ALIGN, /* 0x3d */

	TEST_WFD_REGION = 70, /* 0x46 normal region */
	TEST_WFD_REGION_ALIGN_UP, /* 0x47 by video up */
	TEST_WFD_REGION_ALIGN_VDEC, /* 0x48 by VDEC */
	TEST_WFD_REGION_PA, /* 0x49 PA */

	TEST_MTK_NORMAL = 80, /* 0x50 */

	/*
	 * error test:
	 * Use reserved module map buffer with region_heap, it will
	 * return error because of PA/SIZE not 1MB alignment
	 */
	TEST_REQ_SVP_REGION_UP_ERR = 90, /* 0x5a video uP */
	TEST_REQ_SVP_REGION_VDEC_ERR, /* 0x5b vdec */
	TEST_REQ_PROT_REGION_CCU0_ERR, /* 0x5c ccu0 */

	TEST_MTK_HEAP_NUM
};

struct heap_name {
	enum DMABUF_HEAP id;
	char *name;
};

static struct heap_name heap_obj[] = {
	[TEST_REQ_SVP_REGION] = {
		.id = TEST_REQ_SVP_REGION,
		.name = "svp_region_heap",
	},
	[TEST_REQ_SVP_REGION_ALIGN_UP] = {
		.id = TEST_REQ_SVP_REGION_ALIGN_UP,
		.name = "svp_region_heap-aligned-up",
	},
	[TEST_REQ_SVP_REGION_ALIGN_VDEC] = {
		.id = TEST_REQ_SVP_REGION_ALIGN_VDEC,
		.name = "svp_region_heap-aligned-vdec",
	},
	[TEST_REQ_SVP_REGION_PA] = {
		.id = TEST_REQ_SVP_REGION_PA,
		.name = "svp_region_heap-PA",
	},

	[TEST_REQ_PROT_REGION] = {
		.id = TEST_REQ_PROT_REGION,
		.name = "prot_region_heap",
	},
	[TEST_REQ_PROT_REGION_ALIGN_CCU0] = {
		.id = TEST_REQ_PROT_REGION_ALIGN_CCU0,
		.name = "prot_region_heap-align-ccu0",
	},
	[TEST_REQ_PROT_REGION_PA] = {
		.id = TEST_REQ_PROT_REGION_PA,
		.name = "prot_region_heap-PA",
	},

	[TEST_REQ_SVP_PAGE] = {
		.id = TEST_REQ_SVP_PAGE,
		.name = "svp_page_normal",
	},
	[TEST_REQ_SVP_PAGE_UP] = {
		.id = TEST_REQ_SVP_PAGE_UP,
		.name = "svp_page_up",
	},
	[TEST_REQ_SVP_PAGE_VDEC] = {
		.id = TEST_REQ_SVP_PAGE_VDEC,
		.name = "svp_page_vdec",
	},
	[TEST_REQ_SVP_PAGE_PA] = {
		.id = TEST_REQ_SVP_PAGE_PA,
		.name = "svp_page_pa",
	},
	[TEST_REQ_PROT_PAGE] = {
		.id = TEST_REQ_PROT_PAGE,
		.name = "prot_page_normal",
	},
	[TEST_REQ_PROT_PAGE_CCU0] = {
		.id = TEST_REQ_PROT_PAGE_CCU0,
		.name = "prot_page_ccu0",
	},
	[TEST_REQ_PROT_PAGE_PA] = {
		.id = TEST_REQ_PROT_PAGE_PA,
		.name = "prot_page_pa",
	},
	[TEST_PROT_2D_FR_REGION] = {
		.id = TEST_PROT_2D_FR_REGION,
		.name = "prot_2d_fr_region",
	},
	[TEST_PROT_2D_FR_REGION_ALIGN] = {
		.id = TEST_PROT_2D_FR_REGION_ALIGN,
		.name = "prot_2d_fr_region-aligned",
	},
	[TEST_SAPU_DATA_SHM_REGION] = {
		.id = TEST_SAPU_DATA_SHM_REGION,
		.name = "sapu_data_shm_region",
	},
	[TEST_SAPU_DATA_SHM_REGION_ALIGN] = {
		.id = TEST_SAPU_DATA_SHM_REGION_ALIGN,
		.name = "sapu_data_shm_region-aligned",
	},
	[TEST_SAPU_ENGINE_SHM_REGION] = {
		.id = TEST_SAPU_ENGINE_SHM_REGION,
		.name = "sapu_engine_shm_region",
	},
	[TEST_SAPU_ENGINE_SHM_REGION_ALIGN] = {
		.id = TEST_SAPU_ENGINE_SHM_REGION_ALIGN,
		.name = "sapu_engine_shm_region-aligned",
	},

	[TEST_WFD_REGION] = {
		.id = TEST_WFD_REGION,
		.name = "wfd_region",
	},
	[TEST_WFD_REGION_ALIGN_UP] = {
		.id = TEST_WFD_REGION_ALIGN_UP,
		.name = "wfd_region_heap-aligned-up",
	},
	[TEST_WFD_REGION_ALIGN_VDEC] = {
		.id = TEST_WFD_REGION_ALIGN_VDEC,
		.name = "wfd_region_heap-aligned-vdec",
	},
	[TEST_WFD_REGION_PA] = {
		.id = TEST_WFD_REGION_PA,
		.name = "wfd_region_heap-PA",
	},


	[TEST_MTK_NORMAL] = {
		.id = TEST_MTK_NORMAL,
		.name = "mm_heap",
	},

	/* error test */
	[TEST_REQ_SVP_REGION_UP_ERR] = {
		.id = TEST_REQ_SVP_REGION_UP_ERR,
		.name = "svp_region_video_up",
	},
	[TEST_REQ_SVP_REGION_VDEC_ERR] = {
		.id = TEST_REQ_SVP_REGION_VDEC_ERR,
		.name = "prot_region_vdec",
	},
	[TEST_REQ_PROT_REGION_CCU0_ERR] = {
		.id = TEST_REQ_PROT_REGION_CCU0_ERR,
		.name = "prot_region_ccu0",
	},
};

struct dmabuf_map {
	int				id;
	struct dma_buf_attachment	*attach;
	struct sg_table                 *table;
	dma_addr_t			dma_address;
	struct list_head		map_node;
};

struct dmabuf_info {
	int				id;
	size_t				size;
	struct dma_buf			*dmabuf;
	struct list_head		buf_node;
	struct list_head		map_head;
	struct mutex			map_lock;
};

struct dmabuf_data {
	struct device			*dev;
	struct dma_heap			*heap;
	struct mutex			buf_lock;
	struct list_head		buf_head;
};

struct iommu_test_debug_data {
	struct proc_dir_entry		*debug_root;
};

static struct iommu_test_debug_data *debug_data;
static struct dmabuf_data dmabuf_data_obj[TEST_MTK_HEAP_NUM];

static int dmabuf_id, attach_id;
static size_t test_size = 0x1800; //6K
static enum DMABUF_HEAP test_heap = TEST_REQ_PROT_REGION;

static inline phys_addr_t iova_to_phys(struct device *dev, dma_addr_t iova)
{
	struct iommu_domain *dom = iommu_get_domain_for_dev(dev);
	phys_addr_t pa;

	if (!dom) {
		dev_info(dev, "Not attach to iommu\n");
		return 0;
	}

	pa = iommu_iova_to_phys(dom, iova);
	return pa;
}

static inline void mtk_iommu_tlb_flush_all(struct device *dev)
{
	struct iommu_domain *domain;

	if (!dev)
		return;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain)
		return;

	iommu_flush_iotlb_all(domain);
}

static struct dmabuf_map *dmabuf_map_alloc(int id)
{
	struct dmabuf_map *map_obj = NULL;

	map_obj = kzalloc(sizeof(struct dmabuf_map), GFP_KERNEL);
	if (map_obj == NULL)
		return NULL;

	INIT_LIST_HEAD(&(map_obj->map_node));
	map_obj->id = id;

	return map_obj;
}

static struct dmabuf_info *dmabuf_info_alloc(int buf_id)
{
	struct dmabuf_info *info_obj = NULL;

	info_obj = kzalloc(sizeof(struct dmabuf_info), GFP_KERNEL);
	if (info_obj == NULL)
		return NULL;

	INIT_LIST_HEAD(&(info_obj->buf_node));
	INIT_LIST_HEAD(&(info_obj->map_head));
	mutex_init(&info_obj->map_lock);
	info_obj->id = buf_id;

	return info_obj;
}

static void add_dmabuf_info(struct dmabuf_data *data, struct dmabuf_info *plist)
{
	mutex_lock(&(data->buf_lock));
	list_add(&(plist->buf_node), &(data->buf_head));
	mutex_unlock(&(data->buf_lock));
}

static void add_dmabuf_map(struct dmabuf_info *data, struct dmabuf_map *plist)
{
	mutex_lock(&(data->map_lock));
	list_add(&(plist->map_node), &(data->map_head));
	mutex_unlock(&(data->map_lock));
}

/*
 * if @dmabuf = NULL, we need to find dmabuf_info by @id
 * if @dmabuf != NULL, we need to find dmabuf_info by @dmabuf
 */
static struct dmabuf_info *find_dmabuf_info(struct dmabuf_data *data, int id)
{
	struct list_head *plist_tmp;
	struct dmabuf_info *obj_info;

	mutex_lock(&(data->buf_lock));
	list_for_each(plist_tmp, &(data->buf_head)) {
		obj_info = container_of(plist_tmp, struct dmabuf_info, buf_node);
		if (obj_info->id == id)
			break;
	}
	if (plist_tmp == &(data->buf_head)) {
		pr_info("%s fail, heap:%s, buf_id:%d\n",
			__func__, dma_heap_get_name(data->heap), id);
		mutex_unlock(&(data->buf_lock));
		return NULL;
	}
	mutex_unlock(&(data->buf_lock));

	return obj_info;
}

static struct dmabuf_map *find_dmabuf_map(struct dmabuf_info *data, int attach_id)
{
	struct list_head *map_tmp;
	struct dmabuf_map *map_obj;

	mutex_lock(&(data->map_lock));
	list_for_each(map_tmp, &(data->map_head)) {
		map_obj = container_of(map_tmp, struct dmabuf_map, map_node);
		if (map_obj->id == attach_id)
			break;
	}

	if (map_tmp == &(data->map_head)) {
		pr_info("%s fail, attach_id:%d\n", __func__, attach_id);
		mutex_unlock(&(data->map_lock));
		return NULL;
	}
	mutex_unlock(&(data->map_lock));

	return map_obj;
}

static int check_dmabuf_id(struct dmabuf_data *data, int alloc_id)
{
	struct list_head *plist_tmp;
	struct dmabuf_info *obj_info;

	mutex_lock(&(data->buf_lock));
	list_for_each(plist_tmp, &(data->buf_head)) {
		obj_info = container_of(plist_tmp, struct dmabuf_info, buf_node);
		if (obj_info->id == alloc_id)
			break;
	}

	if (plist_tmp == &(data->buf_head)) {
		pr_info("%s success, alloc_id(%d) can be used\n",
			__func__, alloc_id);
		mutex_unlock(&(data->buf_lock));
		return 0;
	}
	mutex_unlock(&(data->buf_lock));
	pr_info("%s fail, alloc_id(%d) has aleady exist!! dmabuf:%pa, sz:0x%zx\n",
		__func__, alloc_id, obj_info->dmabuf, obj_info->size);

	return -EINVAL;
}

static int check_attach_id(struct dmabuf_info *data, int id)
{
	struct list_head *plist_tmp;
	struct dmabuf_map *map_obj;

	mutex_lock(&(data->map_lock));
	list_for_each(plist_tmp, &(data->map_head)) {
		map_obj = container_of(plist_tmp, struct dmabuf_map, map_node);
		if (map_obj->id == id)
			break;
	}

	if (plist_tmp == &(data->map_head)) {
		pr_info("%s success, attach_id(%d) can be used\n", __func__, id);
		mutex_unlock(&(data->map_lock));
		return 0;
	}
	mutex_unlock(&(data->map_lock));
	pr_info("%s fail, attach_id(%d) has aleady exist\n",
		__func__, id);

	return -EINVAL;
}

static void del_dmabuf_info(struct dmabuf_info *info_obj)
{
	list_del(&info_obj->buf_node);
	kfree(info_obj);
}

static void del_dmabuf_map(struct dmabuf_map *map_obj)
{
	list_del(&map_obj->map_node);
	kfree(map_obj);
}

static void dump_dmabuf_info_map(enum DMABUF_HEAP heap)
{
	struct list_head *info_tmp, *map_tmp;
	struct dmabuf_info *info_obj;
	struct dmabuf_map *map_obj;
	struct dmabuf_data *data = &dmabuf_data_obj[heap];

	mutex_lock(&(data->buf_lock));
	list_for_each(info_tmp, &(data->buf_head)) {
		info_obj = container_of(info_tmp, struct dmabuf_info, buf_node);
		pr_info("%s 1, info_obj:0x%lx, buf_id:%d, dmabuf:0x%lx, sz:0x%zx, heap:%s\n",
			__func__, (unsigned long)info_obj, info_obj->id,
			(unsigned long)info_obj->dmabuf, info_obj->size, heap_obj[heap].name);
		mutex_lock(&(info_obj->map_lock));
		list_for_each(map_tmp, &(info_obj->map_head)) {
			map_obj = container_of(map_tmp, struct dmabuf_map, map_node);
			pr_info("%s 2, map_obj:0x%lx, attach_id:%d, attach:0x%lx, iova:%pa\n",
				__func__, (unsigned long)map_obj, map_obj->id,
				(unsigned long)map_obj->attach, &map_obj->dma_address);
		}
		mutex_unlock(&(info_obj->map_lock));
	}
	mutex_unlock(&(data->buf_lock));
}

static void dmabuf_heap_alloc_test(size_t size, enum DMABUF_HEAP heap, int buf_id)
{
	int ret;
	struct dmabuf_data *data_obj;
	struct dmabuf_info *info_obj;

	pr_info("%s start, heap:%s(%d)\n", __func__, heap_obj[heap].name, heap);

	ret = check_dmabuf_id(&dmabuf_data_obj[heap], buf_id);
	if (ret) {
		pr_info("%s failed, buf_id can not be used:%d\n", __func__, buf_id);
		return;
	}

	switch (heap) {
	case TEST_MTK_NORMAL:
		data_obj = &dmabuf_data_obj[TEST_MTK_NORMAL];
		data_obj->heap = dma_heap_find("mtk_mm");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_mm failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_SVP_REGION:
	case TEST_REQ_SVP_REGION_UP_ERR:
	case TEST_REQ_SVP_REGION_VDEC_ERR:
	case TEST_REQ_SVP_REGION_PA:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_svp_region");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_svp_regio failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_SVP_REGION_ALIGN_UP:
	case TEST_REQ_SVP_REGION_ALIGN_VDEC:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_svp_region-aligned");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_svp_region-aligned failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_PROT_REGION:
	case TEST_REQ_PROT_REGION_PA:
	case TEST_REQ_PROT_REGION_CCU0_ERR:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_prot_region");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_prot_region failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_PROT_REGION_ALIGN_CCU0:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_prot_region-aligned");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_prot_region-aligned failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_SVP_PAGE:
	case TEST_REQ_SVP_PAGE_UP:
	case TEST_REQ_SVP_PAGE_VDEC:
	case TEST_REQ_SVP_PAGE_PA:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_svp_page-uncached");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_svp_page-uncached failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_PROT_PAGE:
	case TEST_REQ_PROT_PAGE_CCU0:
	case TEST_REQ_PROT_PAGE_PA:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_prot_page-uncached");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_svp_page-uncached failed!!\n", __func__);
			return;
		}
		break;
	case TEST_PROT_2D_FR_REGION:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_2d_fr_region");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_2d_fr_region failed!!\n", __func__);
			return;
		}
		break;
	case TEST_PROT_2D_FR_REGION_ALIGN:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_2d_fr_region-aligned");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_2d_fr_region-aligned failed!!\n", __func__);
			return;
		}
		break;
	case TEST_SAPU_DATA_SHM_REGION:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_sapu_data_shm_region");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_sapu_data_shm_region failed!!\n", __func__);
			return;
		}
		break;
	case TEST_SAPU_DATA_SHM_REGION_ALIGN:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_sapu_data_shm_region-aligned");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_sapu_data_shm_region-aligned failed!!\n",
				__func__);
			return;
		};
		break;
	case TEST_SAPU_ENGINE_SHM_REGION:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_sapu_engine_shm_region");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_sapu_engine_shm_region failed!!\n",
				__func__);
			return;
		}
		break;
	case TEST_SAPU_ENGINE_SHM_REGION_ALIGN:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_sapu_engine_shm_region-aligned");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_sapu_engine_shm_region-aligned failed!!\n",
				__func__);
			return;
		}
		break;
	case TEST_WFD_REGION:
	case TEST_WFD_REGION_PA:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_wfd_region");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_wfd_region failed!!\n", __func__);
			return;
		}
		break;
	case TEST_WFD_REGION_ALIGN_UP:
	case TEST_WFD_REGION_ALIGN_VDEC:
		data_obj = &dmabuf_data_obj[heap];
		data_obj->heap = dma_heap_find("mtk_wfd_region-aligned");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_wfd_region-aligned failed!!\n", __func__);
			return;
		}
		break;
	default:
		pr_info("%s failed, heap type in invalid:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}

	pr_info("%s dma_heap_find done, heap:%s\n", __func__, dma_heap_get_name(data_obj->heap));
	info_obj = dmabuf_info_alloc(buf_id);
	if (!info_obj) {
		pr_info("%s dmabuf_info_alloc fail, heap:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}
	info_obj->dmabuf = dma_heap_buffer_alloc(data_obj->heap, size,
				DMA_HEAP_VALID_FD_FLAGS, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(info_obj->dmabuf)) {
		pr_info("%s, alloc buffer fail, heap:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}

	info_obj->size = size;
	add_dmabuf_info(data_obj, info_obj);

	pr_info("%s success, heap:%s(%d), dmabuf_info:0x%lx, dmabuf:0x%lx, sz:0x%zx, buf_id:%d\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj,
		(unsigned long)info_obj->dmabuf, size, buf_id);
}

static void dmabuf_heap_free_test(enum DMABUF_HEAP heap, int buf_id)
{
	struct dmabuf_info *info_obj;

	info_obj = find_dmabuf_info(&dmabuf_data_obj[heap], buf_id);
	if (!info_obj) {
		pr_info("%s find dmabuf_info fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}
	pr_info("%s success, heap:%s(%d), dmabuf_info:0x%lx, dmabuf:0x%lx, buf_id:%d\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj,
		(unsigned long)info_obj->dmabuf, buf_id);

	dma_heap_buffer_free(info_obj->dmabuf);
	del_dmabuf_info(info_obj);
}

static void dmabuf_map_iova(enum DMABUF_HEAP heap, int buf_id, int attach_id)
{
	int ret;
	struct dmabuf_map *map_obj;
	struct dmabuf_info *info_obj;
	struct device *dev = dmabuf_data_obj[heap].dev;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
#ifdef SECURE_HANDLE_TEST
	u32 sec_handle;
#endif
	pr_info("%s start, heap:%s(%d), dev:%s\n", __func__,
		heap_obj[heap].name, heap, dev_name(dev));

	/* find dmabuf_info by attach buf_id from dmabuf_data */
	info_obj = find_dmabuf_info(&dmabuf_data_obj[heap], buf_id);
	if (!info_obj) {
		pr_info("%s find dmabuf_info fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}
#ifdef SECURE_HANDLE_TEST
	/* get secure handle */
	sec_handle = dmabuf_to_secure_handle(info_obj->dmabuf);
	if (!sec_handle)
		pr_info("%s get secure handle failed, heap:%s\n", __func__, heap_obj[heap].name);
	else
		pr_info("%s get secure handle(%u) success, heap:%s\n", __func__,
			sec_handle, heap_obj[heap].name);
#endif
	ret = check_attach_id(info_obj, attach_id);
	if (ret) {
		pr_info("%s failed, attach_id can not be used:%d\n", __func__, attach_id);
		return;
	}

	map_obj = dmabuf_map_alloc(attach_id);
	if (!map_obj) {
		pr_info("%s dmabuf_map_alloc fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}

	attach = dma_buf_attach(info_obj->dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_attach failed!!, heap:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		pr_info("%s, dma_buf_map_attachment failed!!, heap:%s\n",
			__func__, heap_obj[heap].name);
		return;
	}

	map_obj->attach = attach;
	map_obj->table = table;
	map_obj->dma_address = sg_dma_address(table->sgl);
	add_dmabuf_map(info_obj, map_obj);
	pr_info("%s success, heap:%s(%d), dmabuf:0x%lx, map_obj:0x%lx, buf_id:%d, att_id:%d, attach:0x%lx, iova:%pa\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj->dmabuf,
		(unsigned long)map_obj, buf_id, attach_id, (unsigned long)map_obj->attach,
		&map_obj->dma_address);
}

static void dmabuf_unmap_iova(enum DMABUF_HEAP heap, int buf_id, int attach_id)
{
	struct dmabuf_info *info_obj;
	struct dmabuf_map *map_obj;

	/* find dmabuf_info by buf_id from dmabuf_data */
	info_obj = find_dmabuf_info(&dmabuf_data_obj[heap], buf_id);
	if (!info_obj) {
		pr_info("%s find dmabuf_info fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}
	map_obj = find_dmabuf_map(info_obj, attach_id);
	if (!map_obj) {
		pr_info("%s find dmabuf_map fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}

	pr_info("%s success, heap:%s(%d), dmabuf:0x%lx, map_obj:0x%lx, buf_id:%d, att_id:%d, attach:0x%lx, iova:%pa\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj->dmabuf,
		(unsigned long)map_obj, buf_id, attach_id, (unsigned long)map_obj->attach,
		&map_obj->dma_address);

	dma_buf_unmap_attachment(map_obj->attach, map_obj->table, DMA_BIDIRECTIONAL);
	dma_buf_detach(info_obj->dmabuf, map_obj->attach);

	del_dmabuf_map(map_obj);
}

enum CMD_INDEX {
	SET_SIZE_CMD = 0,
	SET_HEAP_ID_CMD = 1,
	SET_DMABUF_ID_CMD = 2,
	SET_ATTACH_ID_CMD = 3,

	ALLOC_BUF_CMD = 10,
	MAP_CMD = 11,
	UNMAP_CMD = 12,
	FREE_BUF_CMD = 13,
	DUMP_INFO_CMD = 14,
};
struct cmd_name {
	enum CMD_INDEX index;
	char *name;
};

static struct cmd_name test_cmd_name[] = {
	[SET_SIZE_CMD] = {
		.index = SET_SIZE_CMD,
		.name = "Set buffer size",
	},
	[SET_HEAP_ID_CMD] = {
		.index = SET_HEAP_ID_CMD,
		.name = "Set heap type",
	},
	[SET_DMABUF_ID_CMD] = {
		.index = SET_DMABUF_ID_CMD,
		.name = "Set dmabuf id",
	},
	[SET_ATTACH_ID_CMD] = {
		.index = SET_ATTACH_ID_CMD,
		.name = "Set attach id",
	},

	[ALLOC_BUF_CMD] = {
		.index = ALLOC_BUF_CMD,
		.name = "Allocate buffer test",
	},
	[MAP_CMD] = {
		.index = MAP_CMD,
		.name = "Map buffer test",
	},
	[UNMAP_CMD] = {
		.index = UNMAP_CMD,
		.name = "Unmap buffer test",
	},
	[FREE_BUF_CMD] = {
		.index = FREE_BUF_CMD,
		.name = "Free buffer test",
	},
	[DUMP_INFO_CMD] = {
		.index = DUMP_INFO_CMD,
		.name = "Dump buffer_info_map test",
	},
};

/*
 * iommu test file: /proc/iommu_test/test
 *
 * dmabuf secure heap test:
 * UT-1: map heap_iova && buffer iova test   --> pass
 * step-1. alloc buffer by dmabuf_id = 0
 * --->
 * step-2. map dmabuf_id = 0 to iova by attach_id=0,1,2
 * step-3. check mapping result:
 *         1.heap region PA && size by alloc in step-1
 *         2.heap_region and dmabuf are all mapped in step-2
 *         3.heap_iova must be mappped by 1st level, not 2nd level
 *         4.all dmabuf only mapping one iova not more than one
 *
 * UT-2: alloc && map more than more buffer by normal region   --> pass
 * step-1. alloc buffer by dmabuf_id = 0
 * step-2. map dmabuf_id = 0 to iova by attach_id=0
 * step-3. alloc buffer by dmabuf_id = 1
 * --->	check: no need to alloc heap region PA && size(only need it in step1)
 * step-4. map dmabuf_id = 1 to iova by attach_id=1
 * --->	check: no need to run dma_map_sgtable and get iova by PA offset
 * step-5. alloc buffer by dmabuf_id = 2
 * --->	check: no need to alloc heap region PA && size(only need it in step1)
 * step-6. map dmabuf_id = 2 to iova by attach_id=2
 * --->	check: no need to run dma_map_sgtable and get iova by PA offset
 * step7. free dmabuf_id = 2,1,0
 * --->	check: only after last dmabuf free, heap_region iova will free
 *
 * UT-3: non-normal iova region test   --> pass
 * step-1. use prot_region_heap0(normal region) to alloc one buffer by dmabuf_id = 0
 * step-2. map dmabuf_id = 0 to iova by attach_id=0 and use prot_region_heap0
 * step-3. use prot_region_heap1(non-normal region) to alloc one buffer by dmabuf_id = 0
 * step-4. use prot_region_heap1 to map dmabuf_id = 0 to iova by attach_id=0
 * ---> check: special region mapping must be run dma_map_sgtable
 * step-5. use prot_region_heap1 to free dmabuf_id = 0
 * ---> check: special region unmap must be run dma_unmap_sgtable
 *
 *
 * UT-4: non-normal iova region 1MB align check   --> pass
 * step-1. set test_size not 1MB align(0x3100)
 * step-2. use prot_region_heap0(normal region) to alloc one buffer by dmabuf_id = 0
 * step-3. use prot_region_heap0 to map dmabuf_id = 0 to iova by attach_id = 0
 * ---> check: size be aligned to 0x4000 and map success
 * step-4. use prot_region_heap1(special region) to alloc one buffer by dmabuf_id = 0
 * step-5. use prot_region_heap1 to map dmabuf_id = 0 to iova by attach_id = 0
 * ---> check: map fail because of not 1MB align
 *
 *
 * UT-5: non-iommu-dev test   --> pass
 * step-1. use prot_region_heap1(non-iommu dev) alloc buffer by dmabuf_id = 0
 * step-2. map dmabuf_id = 0 by attach_id=0
 * ---> check: run non-iommu mapping pass
 * step-3. unmap dmabuf_id by attach_id=0
 * ---> check: run non-iommu unmap pass
 *
 *
 * UT-6: 1MB alignment test   --> pass
 * step-1. use prot_region_heap0(normal region) alloc 6KB by dmabuf_id = 0,1
 * ---> check: 6K align to 8k, PA may be 1MB align
 * step-2. use prot_region_heap0 map dmabuf_id = 0,1 by attach_id=0,1
 * ----> check: heap_region is mapped, and can return buffer iova
 * step-3. use prot_region_heap1(special region) alloc 6KB by dmabuf_id = 0, 1
 * ----> check: 6K align to 1MB, and check PA is 1MB alignemnt
 * step-4. use prot_region_heap1 map dmabuf_id = 0, 1 by attach_id= 0, 1
 * ----> check: special region mapping must be run dma_map_sgtable and success
 * step-5. use prot_region_heap0(normal region) alloc 6KB by dmabuf_id = 2 again
 * ---> check: 6K align to 8k
 * step-6. use prot_region_heap0 map dmabuf_id = 2 by attach_id= 2
 * ----> check: can return buffer iova, and PA is after step3
 * step-7. use prot_region_heap0 free/unmap  dmabuf_id/attach_id = 0,1,2
 * ----> check: heap_region shouldn't be unmapped
 * step-8. use prot_region_heap1 free/unmap  dmabuf_id/attach_id = 0
 * ----> check: heap_region should be unmapped
 * step-9. repeat step-1 ~ 8
 */
static int iommu_test_debug_set(void *data, u64 input)
{
	u64 index = (input & 0xff);
	u64 val = (input >> 8);

	pr_info("%s [%s], start, input:0x%llx, index=%llu, val:%llu\n",
		__func__, test_cmd_name[index].name, input, index, val);

	switch (index) {
	case 0:
		/* adb shell "echo 0xXXXXX00 > /proc/iommu_test/test" */
		test_size = val;
		break;
	case 1:
		/*
		 * mm_heap:     adb shell "echo 0xd01 > /proc/iommu_test/test"
		 * prot_region: adb shell "echo 0x101 > /proc/iommu_test/test"
		 */
		test_heap = (enum DMABUF_HEAP)val;
		break;
	case 2:
		/*
		 * dmabuf_id=1: adb shell "echo 0x102 > /proc/iommu_test/test"
		 * dmabuf_id=2: adb shell "echo 0x202 > /proc/iommu_test/test"
		 * dmabuf_id=3: adb shell "echo 0x302 > /proc/iommu_test/test"
		 */
		dmabuf_id = val;
		break;
	case 3:
		/*
		 * attach_id=1: adb shell "echo 0x103 > /proc/iommu_test/test"
		 * attach_id=2: adb shell "echo 0x203 > /proc/iommu_test/test"
		 * attach_id=3: adb shell "echo 0x303 > /proc/iommu_test/test"
		 */
		attach_id = val;
		break;
	case 10:
		/* adb shell "echo 0xa > /proc/iommu_test/test" */
		dmabuf_heap_alloc_test(test_size, test_heap, dmabuf_id);
		break;
	case 11:
		/* adb shell "echo 0xb > /proc/iommu_test/test" */
		dmabuf_map_iova(test_heap, dmabuf_id, attach_id);
		break;
	case 12:
		/* adb shell "echo 0xc > /proc/iommu_test/test" */
		dmabuf_unmap_iova(test_heap, dmabuf_id, attach_id);
		break;
	case 13:
		/* adb shell "echo 0xd > /proc/iommu_test/test" */
		dmabuf_heap_free_test(test_heap, dmabuf_id);
		break;
	case 14:
		/* adb shell "echo 0xe > /proc/iommu_test/test" */
		dump_dmabuf_info_map(test_heap);
		break;
	default:
		pr_info("%s error,index=%llu\n", __func__, index);
	}

	pr_info("%s [%s] done\n", __func__, test_cmd_name[index].name);

	return 0;
}

static int iommu_test_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}
DEFINE_PROC_ATTRIBUTE(iommu_test_debug_fops, iommu_test_debug_get, iommu_test_debug_set, "%llu\n");

static int iommu_test_debug_init(struct iommu_test_debug_data *data)
{
	struct proc_dir_entry *debug_file;

	data->debug_root = proc_mkdir("iommu_test", NULL);

	if (IS_ERR_OR_NULL(data->debug_root))
		pr_info("failed to create debug dir.\n");

	debug_file = proc_create_data("test",
		S_IFREG | 0644, data->debug_root, &iommu_test_debug_fops, NULL);

	if (IS_ERR_OR_NULL(debug_file))
		pr_info("failed to create debug files 2.\n");

	return 0;
}

/******************************************************************************/
static int dmaheap_normal_test_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_MTK_NORMAL].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_MTK_NORMAL].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_MTK_NORMAL].buf_head);

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dmaheap_sec_normal_region_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_SVP_REGION].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_REGION].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_REGION].buf_head);

	dmabuf_data_obj[TEST_REQ_PROT_REGION].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_REGION].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_REGION].buf_head);

	dmabuf_data_obj[TEST_REQ_SVP_PAGE].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_PAGE].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_PAGE].buf_head);

	dmabuf_data_obj[TEST_REQ_PROT_PAGE].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_PAGE].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_PAGE].buf_head);

	dmabuf_data_obj[TEST_WFD_REGION].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_WFD_REGION].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_WFD_REGION].buf_head);

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));

	return 0;
}

static int dmaheap_sec_ccu0_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_PROT_REGION_ALIGN_CCU0].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_REGION_ALIGN_CCU0].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_REGION_ALIGN_CCU0].buf_head);

	dmabuf_data_obj[TEST_REQ_PROT_PAGE_CCU0].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_PAGE_CCU0].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_PAGE_CCU0].buf_head);

	dmabuf_data_obj[TEST_REQ_PROT_REGION_CCU0_ERR].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_REGION_CCU0_ERR].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_REGION_CCU0_ERR].buf_head);

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dmaheap_sec_video_up_probe(struct platform_device *pdev)
{
	/* include region && page dev */
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_SVP_REGION_ALIGN_UP].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_REGION_ALIGN_UP].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_REGION_ALIGN_UP].buf_head);

	dmabuf_data_obj[TEST_WFD_REGION_ALIGN_UP].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_WFD_REGION_ALIGN_UP].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_WFD_REGION_ALIGN_UP].buf_head);

	dmabuf_data_obj[TEST_REQ_SVP_PAGE_UP].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_PAGE_UP].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_PAGE_UP].buf_head);

	dmabuf_data_obj[TEST_REQ_SVP_REGION_UP_ERR].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_REGION_UP_ERR].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_REGION_UP_ERR].buf_head);

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dmaheap_sec_vdec_probe(struct platform_device *pdev)
{
	/* include region && page dev */
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_SVP_REGION_ALIGN_VDEC].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_REGION_ALIGN_VDEC].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_REGION_ALIGN_VDEC].buf_head);

	dmabuf_data_obj[TEST_REQ_SVP_PAGE_VDEC].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_PAGE_VDEC].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_PAGE_VDEC].buf_head);

	dmabuf_data_obj[TEST_REQ_SVP_REGION_VDEC_ERR].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_REGION_VDEC_ERR].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_REGION_VDEC_ERR].buf_head);

	dmabuf_data_obj[TEST_WFD_REGION_ALIGN_VDEC].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_WFD_REGION_ALIGN_VDEC].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_WFD_REGION_ALIGN_VDEC].buf_head);
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dmaheap_sec_pa_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_SVP_REGION_PA].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_REGION_PA].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_REGION_PA].buf_head);

	dmabuf_data_obj[TEST_REQ_PROT_REGION_PA].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_REGION_PA].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_REGION_PA].buf_head);

	dmabuf_data_obj[TEST_REQ_SVP_PAGE_PA].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_SVP_PAGE_PA].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_SVP_PAGE_PA].buf_head);

	dmabuf_data_obj[TEST_REQ_PROT_PAGE_PA].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_PAGE_PA].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_PAGE_PA].buf_head);

	dmabuf_data_obj[TEST_WFD_REGION_PA].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_WFD_REGION_PA].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_WFD_REGION_PA].buf_head);

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

/******************************************************************************/
static const size_t IOVA_SIZES[] = {
	(SZ_4K),
	(SZ_4K),
	(SZ_4K),
	(SZ_4K),
	(SZ_1K * 10),
	(SZ_1K * 40),
	(SZ_64K),
	(SZ_512K),
	(PAGE_SIZE),
	(SZ_1M),
	(SZ_1M + PAGE_SIZE),
	(SZ_2M + PAGE_SIZE * 2),
	(SZ_1M * 6 + PAGE_SIZE * 3),
	(SZ_16M),
	(SZ_16M + SZ_4K),
	(SZ_16M + SZ_1M),
	(SZ_16M + SZ_1M + PAGE_SIZE),
	(SZ_32M + SZ_1M),
	(SZ_64M + SZ_4K),
	(SZ_128M + SZ_1M * 3),
	(SZ_256M + SZ_4K),
	(SZ_512M + SZ_4K)
};
static const int IOVA_SIZES_NUM = ARRAY_SIZE(IOVA_SIZES);

static int dma_alloc(struct device *dev, int address_num, int iova_index,
		     struct dma_device_res *res)
{
	dma_addr_t dma_addr;
	phys_addr_t phys_addr;
	void *vaddr;
	int i;

	if (address_num > IOVA_SIZES_NUM || iova_index >= IOVA_SIZES_NUM) {
		pr_info("%s, error: address_num:%d, index:%d, iova_sizes:%d, dev:%s\n",
			__func__, address_num, iova_index, IOVA_SIZES_NUM, dev_name(dev));
			return -EINVAL;
	}

	pr_info("%s, address_num:%d, iova_index:%d, dev:%s\n",
		__func__, address_num, iova_index, dev_name(dev));

	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	for (i = iova_index; i < address_num; i++) {
		res[i].size = IOVA_SIZES[i];
		res[i].attrs = DMA_ATTR_WRITE_COMBINE;

		vaddr = dma_alloc_attrs(dev, res[i].size, &dma_addr,
					      GFP_KERNEL, res[i].attrs);
		if (!vaddr || !dma_addr) {
			pr_info("dev:%s:%d, alloc iova fail, size:0x%zx\n",
				dev_name(dev), i, res[i].size);
			return -EINVAL;
		}
		phys_addr = iova_to_phys(dev, dma_addr);

		res[i].vaddr = vaddr;
		res[i].dma_addr = dma_addr;
		res[i].phys_addr = phys_addr;

		pr_info("%s, dev:%s:%d iova:%pa, pa:%pa, va:%pa, size:0x%zx\n",
			__func__, dev_name(dev), i, &res[i].dma_addr,
			&res[i].phys_addr, &res[i].vaddr, res[i].size);
	}

	return 0;
}

static int dma_free(struct device *dev, int address_num, int iova_index,
		    struct dma_device_res *res)
{
	int i;

	if (address_num > IOVA_SIZES_NUM || iova_index >= IOVA_SIZES_NUM) {
		pr_info("%s, error: address_num:%d, index:%d, iova_sizes:%d, dev:%s\n",
			__func__, address_num, iova_index, IOVA_SIZES_NUM, dev_name(dev));
		return -EINVAL;
	}

	pr_info("%s, address_num:%d, iova_index:%d, dev:%s\n",
		__func__, address_num, iova_index, dev_name(dev));

	for (i = iova_index; i < address_num; i++) {
		dma_free_attrs(dev, res[i].size, res[i].vaddr, res[i].dma_addr,
				res[i].attrs);
		pr_info("%s, dev:%s:%d iova:%pa, pa:%pa, va:%pa, size:0x%zx\n",
			__func__, dev_name(dev), i, &res[i].dma_addr,
			&res[i].phys_addr, &res[i].vaddr, res[i].size);
	}

	return 0;
}

static int __maybe_unused iommu_test_translation(int dma_engine,
						 struct platform_device *pdev)
{
	#define TEST_DMA_NUM	4
	struct dma_device_res res[TEST_DMA_NUM];
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	dma_alloc(dev, TEST_DMA_NUM, 0, res);

	engine_access(dma_engine, dev, res);

	dma_free(dev, TEST_DMA_NUM, 0, res);
	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return 0;
}

static int iommu_test(struct platform_device *pdev)
{
	#define TEST_NUM	20
	int i;
	void *cpu_addr[TEST_NUM];
	dma_addr_t dma_addr[TEST_NUM];

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	for (i = 0; i < TEST_NUM; i++) {
		cpu_addr[i] = dma_alloc_attrs(&pdev->dev, IOVA_SIZES[i], &dma_addr[i],
					      GFP_KERNEL, DMA_ATTR_WRITE_COMBINE);
		if (!cpu_addr[i] || !dma_addr[i]) {
			pr_info("dev:%s:%d, alloc iova fail, size:0x%zx\n",
				dev_name(&pdev->dev), i, IOVA_SIZES[i]);
			return -EINVAL;
		}
		pr_info("dev:%s:%d, alloc iova success, iova:%pa, size:0x%zx\n",
			dev_name(&pdev->dev), i, &dma_addr[i], IOVA_SIZES[i]);
	}
	for (i = 0; i < TEST_NUM; i++) {
		dma_free_attrs(&pdev->dev, IOVA_SIZES[i], cpu_addr[i], dma_addr[i],
				DMA_ATTR_WRITE_COMBINE);
		pr_info("dev:%s:%d, free iova success, iova:%pa, size:0x%zx\n",
			dev_name(&pdev->dev), i, &dma_addr[i], IOVA_SIZES[i]);
	}
	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));

	return 0;
}

static int dmabuf_test(struct platform_device *pdev)
{
	size_t size = SZ_1M;
	struct dma_heap	*heap;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));

	heap = dma_heap_find("mtk_mm");
	if (!heap) {
		pr_info("%s, find mtk_mm failed!!\n", __func__);
		return -EINVAL;
	}

	dmabuf = dma_heap_buffer_alloc(heap, size, DMA_HEAP_VALID_FD_FLAGS,
				       DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(dmabuf)) {
		pr_info("%s, alloc buffer fail, heap:%s\n",
			__func__, dma_heap_get_name(heap));
		return -EINVAL;
	}
	pr_info("%s alloc dma-buf success, size:0x%zx, heap:%s\n",
		__func__, size, dma_heap_get_name(heap));

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_attach failed!!, heap:%s\n",
			__func__, dma_heap_get_name(heap));
		return -EINVAL;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_map_attachment failed!!, heap:%s\n",
			__func__, dma_heap_get_name(heap));
		return -EINVAL;
	}
	pr_info("%s map dma-buf success, size:0x%zx, heap:%s, iova:0x%lx\n",
		__func__, size, dma_heap_get_name(heap),
		(unsigned long)sg_dma_address(table->sgl));

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);

	dma_heap_buffer_free(dmabuf);

	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));
	return 0;
}

static int iommu_test_dom_probe(struct platform_device *pdev)
{
	int ret;

	ret = iommu_test(pdev);
	if (ret)
		pr_info("%s failed, iommu_test fail, dev:%s\n",
			__func__, dev_name(&pdev->dev));

	ret = dmabuf_test(pdev);
	if (ret)
		pr_info("%s failed, dmabuf_test fail, dev:%s\n",
			__func__, dev_name(&pdev->dev));

	return 0;
}

#ifdef SMMU_V3_TEST_EN
#define TEST_SMMU_ADDRESS_NUM	4

static inline struct arm_smmu_master *arm_smmu_get_master(struct device *dev)
{
	struct arm_smmu_master *master;

	if (!dev->iommu) {
		pr_info("master device driver may not be loaded!\n");
		return NULL;
	}

	master = dev_iommu_priv_get(dev);
	if (!master) {
		pr_info("Failed to find master dev\n");
		return NULL;
	}

	return master;
}

static inline struct arm_smmu_device *arm_smmu_get_device(struct device *dev)
{
	struct arm_smmu_master *master;

	master = arm_smmu_get_master(dev);
	if (!master)
		return NULL;

	return master->smmu;
}

static int smmu_test_translation(int dma_engine, struct platform_device *pdev)
{
	struct dma_device_res res[TEST_SMMU_ADDRESS_NUM];
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	dma_alloc(dev, TEST_SMMU_ADDRESS_NUM, 0, res);

	engine_access(dma_engine, dev, res);

	dma_free(dev, TEST_SMMU_ADDRESS_NUM, 0, res);
	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return 0;
}

static int smmu_test_performance(int dma_engine, struct platform_device *pdev)
{
	struct dma_device_res res[TEST_SMMU_ADDRESS_NUM];
	unsigned long tcu_tot = 0, tbu_tot = 0;
	struct device *dev = &pdev->dev;
	int ret;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	dma_alloc(dev, TEST_SMMU_ADDRESS_NUM, 0, res);

	pr_info("--- Dump transaction commmand counter for performance debug ---\n");

	/* Start TCU and TBU counter then translation */
	ret = mtk_smmu_start_transaction_counter(dev);
	if (ret)
		goto out;

	engine_access(dma_engine, dev, res);

	/* TCU counters are expected to increase after 1st translation */
	mtk_smmu_end_transaction_counter(dev, &tcu_tot, &tbu_tot);
	ret = tcu_tot ? 0 : -1;
	pr_info("--- TCU counters increase as expected:%d ---\n", ret);
	if (ret)
		goto out;

	ret = mtk_smmu_start_transaction_counter(dev);
	if (ret)
		goto out;

	pr_info("%s dma access start again, dev:%s\n", __func__, dev_name(dev));
	engine_access(dma_engine, dev, res);
	pr_info("%s dma access done again, dev:%s\n", __func__, dev_name(dev));

	/* TBU counters are expected to increase after 2nd translation
	 * since access data via TLB
	 */
	mtk_smmu_end_transaction_counter(dev, &tcu_tot, &tbu_tot);
	ret = tbu_tot ? 0 : -1;
	pr_info("--- TBU counters increase as expected:%d ---\n", ret);
	if (ret)
		goto out;

	ret = mtk_smmu_start_transaction_counter(dev);
	if (ret)
		goto out;

	mtk_iommu_tlb_flush_all(dev);

	/* TCU counters are expected to increase after 3rd translation
	 * since invalid all
	 */
	mtk_smmu_end_transaction_counter(dev, &tcu_tot, &tbu_tot);
	ret = tcu_tot ? 0 : -1;
	pr_info("--- TCU counters increase in TLB INV ALL case as expected:%d ---\n",
		ret);

out:
	dma_free(dev, TEST_SMMU_ADDRESS_NUM, 0, res);
	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return ret;
}

static int smmu_test_latency(int dma_engine, struct platform_device *pdev)
{
	struct dma_device_res res[TEST_SMMU_ADDRESS_NUM];
	struct device *dev = &pdev->dev;
	int ret, mon_axiid, lat_spec;
	unsigned int maxlat_axiid = 0;
	unsigned long tcu_rlat_tots = 0, tbu_lat_tots = 0, oos_trans_tot = 0;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	dma_alloc(dev, TEST_SMMU_ADDRESS_NUM, 0, res);

	pr_info("--- Dump latency monitor for performance debug ---\n");

	/* Start TCU and TBU latency monitor then translation */
	ret = mtk_smmu_start_latency_monitor(dev, -1, -1);
	if (ret)
		goto out;

	engine_access(dma_engine, dev, res);

	/* TCU latency meters are expected to increase after 1st translation */
	mtk_smmu_end_latency_monitor(dev, &maxlat_axiid, &tcu_rlat_tots,
				     &tbu_lat_tots, &oos_trans_tot);
	ret = tcu_rlat_tots ? 0 : -1;
	pr_info("--- TCU latency meters increase as expected:%d ---\n", ret);
	if (ret)
		goto out;

	ret = mtk_smmu_start_latency_monitor(dev, -1, -1);
	if (ret)
		goto out;

	engine_access(dma_engine, dev, res);

	/*
	 * TBU latency meters are expected to increase after 2nd translation
	 * since access data via TLB
	 */
	mtk_smmu_end_latency_monitor(dev, &maxlat_axiid, &tcu_rlat_tots,
				     &tbu_lat_tots, &oos_trans_tot);
	ret = tbu_lat_tots ? 0 : -1;
	pr_info("--- TBU latency meters increase as expected:%d ---\n", ret);

	/* Monitor a specified AXI id */
	mon_axiid = maxlat_axiid;
	ret = mtk_smmu_start_latency_monitor(dev, mon_axiid, -1);
	if (ret)
		goto out;

	engine_access(dma_engine, dev, res);

	/* TBU latency meters are expected to increase after 3rd translation */
	mtk_smmu_end_latency_monitor(dev, &maxlat_axiid, &tcu_rlat_tots,
				     &tbu_lat_tots, &oos_trans_tot);
	ret = (tbu_lat_tots && (mon_axiid == maxlat_axiid)) ? 0 : -1;
	pr_info("--- TBU latency meters increase in monitor AXI ID case as expected:%d ---\n",
		ret);
	if (ret)
		goto out;

	/* Set a minimum latency water level and monitor how many commands exceed */
	lat_spec = 0;
	ret = mtk_smmu_start_latency_monitor(dev, -1, lat_spec);
	if (ret)
		goto out;

	mtk_iommu_tlb_flush_all(dev);

	engine_access(dma_engine, dev, res);

	/*
	 * 1. TCU latency meters are expected to increase after 4th translation
	 * since invalid all
	 * 2. Total command count over latency water mark is not expected
	 * to be 0 since a min walter mark is set
	 */
	mtk_smmu_end_latency_monitor(dev, &maxlat_axiid, &tcu_rlat_tots,
				     &tbu_lat_tots, &oos_trans_tot);
	ret = (tcu_rlat_tots && oos_trans_tot) ? 0 : -1;
	pr_info("--- TCU latency meters increase & Latency water mark meters increase in TLB INV ALL case as expected:%d ---\n",
		ret);

out:
	dma_free(dev, TEST_SMMU_ADDRESS_NUM, 0, res);
	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return ret;
}

static int smmu_test_stability(int dma_engine, struct platform_device *pdev)
{
	struct dma_device_res res[TEST_SMMU_ADDRESS_NUM];
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	dma_alloc(dev, TEST_SMMU_ADDRESS_NUM, 0, res);

	mtk_smmu_dump_outstanding_monitor(dev);
	mtk_smmu_dump_io_interface_signals(dev);

	engine_access(dma_engine, dev, res);

	mtk_smmu_dump_outstanding_monitor(dev);
	mtk_smmu_dump_io_interface_signals(dev);

	dma_free(dev, TEST_SMMU_ADDRESS_NUM, 0, res);
	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return ret;
}

static int smmu_test_stress(int dma_engine, struct platform_device *pdev)
{
	int ret, test_times = 1000;

	while (test_times--) {
		pr_info("--- SMMU stress test: %d/1000 ---\n",
			(1000 - test_times));
		ret = smmu_test_translation(dma_engine, pdev);
		if (ret)
			break;
	}

	return ret;
}

static int smmu_test_dcm_state(int dma_engine, struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("--- Dump DCM enable state for power consumption debug ---\n");

	mtk_smmu_dump_dcm_en(dev);

	return ret;
}

static int smmu_test_cqdma_tf(int dma_engine, struct platform_device *pdev)
{
	struct dma_device_res res[TEST_SMMU_ADDRESS_NUM];
	dma_addr_t	dma_addr0, dma_addr1;
	struct device *dev = &pdev->dev;
	struct arm_smmu_device *smmu;
	int i;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));
	smmu = arm_smmu_get_device(dev);
	dma_alloc(dev, TEST_SMMU_ADDRESS_NUM, 0, res);

	dma_free(dev, TEST_SMMU_ADDRESS_NUM, 2, res);

	pr_info("%s before TF, dev:%s\n", __func__, dev_name(dev));

	/* dump all smmu if exist */
	for (i = 0; i < SMMU_TYPE_NUM; i++)
		mtk_smmu_wpreg_dump(NULL, i);

	for (i = 0; i < SMMU_TYPE_NUM; i++)
		mtk_smmu_pgtable_dump(NULL, i, true);

	/* backup dma_addr */
	dma_addr0 = res[0].dma_addr;
	dma_addr1 = res[1].dma_addr;

	/* use fault iova, to trigger TF */
	res[0].dma_addr = res[2].dma_addr + SZ_4K;
	res[1].dma_addr = res[3].dma_addr + SZ_4K;

	pr_info("%s use fault iova(0x%llx, 0x%llx), to trigger TF start\n",
		__func__, (res[0].dma_addr), (res[1].dma_addr));
	engine_access(dma_engine, dev, res);
	pr_info("%s use fault iova trigger TF done\n", __func__);

	/* restore dma_addr */
	res[0].dma_addr = dma_addr0;
	res[1].dma_addr = dma_addr1;

	pr_info("%s after TF, dev:%s\n", __func__, dev_name(dev));

	dma_free(dev, TEST_SMMU_ADDRESS_NUM - 2, 0, res);
	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return 0;
}

static int smmu_test_cqdma(int dma_engine, struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));

	ret = smmu_test_dcm_state(dma_engine, pdev);
	if (ret)
		pr_info("ret:%d, smmu_test_dcm_state fail\n", ret);

	ret = smmu_test_translation(dma_engine, pdev);
	if (ret)
		pr_info("ret:%d, smmu_test_trans fail\n", ret);

	ret = smmu_test_cqdma_tf(dma_engine, pdev);
	if (ret)
		pr_info("ret:%d, smmu_test_cqdma_tf fail\n", ret);

	ret = smmu_test_performance(dma_engine, pdev);
	if (ret)
		pr_info("ret:%d, smmu_test_performance fail\n", ret);

	ret = smmu_test_latency(dma_engine, pdev);
	if (ret)
		pr_info("ret:%d, smmu_test_latency fail\n", ret);

	ret = smmu_test_stability(dma_engine, pdev);
	if (ret)
		pr_info("ret:%d, smmu_test_stability fail\n", ret);

	ret = smmu_test_stress(dma_engine, pdev);
	if (ret)
		pr_info("ret:%d, smmu_test_performance fail\n", ret);

	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));

	return ret;
}

static int iommu_test_cqdma_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("%s start dev:%s\n", __func__, dev_name(&pdev->dev));

	engine_init(DMA_ENGINE_CQDMA, pdev);

	iommu_test_translation(DMA_ENGINE_CQDMA, pdev);

	ret = smmu_test_cqdma(DMA_ENGINE_CQDMA, pdev);

	pr_info("%s done dev:%s, ret:%d\n", __func__, dev_name(&pdev->dev), ret);

	return 0;
}
#else /* SMMU_V3_TEST_EN */
static int iommu_test_cqdma_probe(struct platform_device *pdev)
{
	pr_info("%s dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}
#endif /* SMMU_V3_TEST_EN */

/*************************************************************************/
/*
 * iommu test DTS sample node:
 * iommu_mm_test0 {
 *	compatible = "mediatek,iommu-test-dom0";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&disp_iommu M4U_PORT_L0_DISP_FAKE0>;
 *};
 *
 * iommu_mm_test1 {
 *	compatible = "mediatek,iommu-test-dom1";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L4_HW_VDEC_VLD_EXT>;
 * };
 *
 * iommu_mm_test2 {
 *	compatible = "mediatek,iommu-test-dom0";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&disp_iommu M4U_PORT_L35_RESERVED_0>;
 * };
 *
 * iommu_mm_test3 {
 *	compatible = "mediatek,iommu-test-dom3";
 *	dma-ranges = <0x1 0x0 0x1 0x0 0x1 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L38_CCU0>;
 * };
 *
 * iommu_mm_test4 {
 *	compatible = "mediatek,iommu-test-dom4";
 *	dma-ranges = <0x1 0x0 0x1 0x0 0x1 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L38_CCU1>;
 * };
 *
 * iommu_mm_test5 {
 *	compatible = "mediatek,iommu-test-dom5";
 *	dma-ranges = <0x1 0x0 0x1 0x0 0x1 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L40_VIDEO_UP_512MB1>;
 * };
 *
 * iommu_mm_test6 {
 *	compatible = "mediatek,iommu-test-dom6";
 *	dma-ranges = <0x1 0x0 0x1 0x0 0x1 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L40_VIDEO_UP_512MB2>;
 * };
 *
 * iommu_mm_test7 {
 *	compatible = "mediatek,iommu-test-dom7";
 *	dma-ranges = <0x1 0x0 0x1 0x0 0x1 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L40_VIDEO_UP_256MB1>;
 * };
 *
 * iommu_mm_test8 {
 *	compatible = "mediatek,iommu-test-dom8";
 *	dma-ranges = <0x1 0x0 0x1 0x0 0x1 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L40_VIDEO_UP_256MB2>;
 * };
 *
 * iommu_apu_test0 {
 *	compatible = "mediatek,iommu-test-dom0";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&apu_iommu0 M4U_PORT_L41_APU_DATA>;
 * };
 *
 * iommu_apu_test1 {
 *	compatible = "mediatek,iommu-test-dom0";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&apu_iommu1 M4U_PORT_L41_APU_DATA>;
 * };
 *
 * iommu_apu_test2 {
 *	compatible = "mediatek,iommu-test-dom1";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x1 0x0>;
 *	iommus = <&apu_iommu0 M4U_PORT_L41_APU_SECURE>;
 * };
 *
 * iommu_apu_test3 {
 *	compatible = "mediatek,iommu-test-dom2";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x1 0x0>;
 *	iommus = <&apu_iommu0 M4U_PORT_L41_APU_CODE>;
 * };
 *
 * iommu_test_dmaheap_normal {
 *	compatible = "mediatek,dmaheap-normal";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&disp_iommu M4U_PORT_L0_DISP_FAKE0>;
 * };
 *
 * iommu_test_dmaheap_normal_region {
 *	compatible = "mediatek,dmaheap-sec-normal-region";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&mdp_iommu M4U_PORT_L0_DISP_FAKE0>;
 * };
 *
 * iommu_test_dmaheap_ccu0 {
 *	compatible = "mediatek,dmaheap-sec-ccu0";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&disp_iommu M4U_PORT_L38_CCU0>;
 * };
 *
 * iommu_test_dmaheap_up {
 *	compatible = "mediatek,dmaheap-sec-up";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&disp_iommu M4U_PORT_L40_VIDEO_UP_512MB1>;
 * };
 *
 * iommu_test_dmaheap_vdec {
 *	compatible = "mediatek,dmaheap-sec-vdec";
 *	dma-ranges = <0x0 0x0 0x0 0x0 0x4 0x0>;
 *	iommus = <&disp_iommu M4U_PORT_L4_HW_VDEC_VLD_EXT>;
 * };
 *
 * iommu_test_dmaheap_pa {
 *	compatible = "mediatek,dmaheap-sec-pa";
 * };
 */
/*************************************************************************/

static const struct of_device_id iommu_test_dom_match_table[] = {
	{.compatible = "mediatek,iommu-test-dom0"},
	{.compatible = "mediatek,iommu-test-dom1"},
	{.compatible = "mediatek,iommu-test-dom2"},
	{.compatible = "mediatek,iommu-test-dom3"},
	{.compatible = "mediatek,iommu-test-dom4"},
	{.compatible = "mediatek,iommu-test-dom5"},
	{.compatible = "mediatek,iommu-test-dom6"},
	{.compatible = "mediatek,iommu-test-dom7"},
	{.compatible = "mediatek,iommu-test-dom8"},
	{.compatible = "mediatek,iommu-test-dom9"},
	{},
};

/*************************************************************************/

static const struct of_device_id iommu_test_cqdma_match_table[] = {
	{.compatible = "mediatek,iommu-test-cqdma"},
	{},
};

/*************************************************************************/

static const struct of_device_id dmaheap_normal_match_table[] = {
	{.compatible = "mediatek,dmaheap-normal"},
	{},
};

static const struct of_device_id dmaheap_sec_normal_region_match_table[] = {
	{.compatible = "mediatek,dmaheap-sec-normal-region"},
	{},
};

static const struct of_device_id dmaheap_sec_ccu0_match_table[] = {
	{.compatible = "mediatek,dmaheap-sec-ccu0"},
	{},
};

static const struct of_device_id dmaheap_sec_up_match_table[] = {
	{.compatible = "mediatek,dmaheap-sec-up"},
	{},
};

static const struct of_device_id dmaheap_sec_vdec_match_table[] = {
	{.compatible = "mediatek,dmaheap-sec-vdec"},
	{},
};

static const struct of_device_id dmaheap_sec_pa_match_table[] = {
	{.compatible = "mediatek,dmaheap-sec-pa"},
	{},
};

static struct platform_driver iommu_test_dmaheap_normal = {
	.probe = dmaheap_normal_test_probe,
	.driver = {
		.name = "dmaheap-normal",
		.of_match_table = dmaheap_normal_match_table,
	},
};

static struct platform_driver iommu_test_dmaheap_sec_normal = {
	.probe = dmaheap_sec_normal_region_probe,
	.driver = {
		.name = "dmaheap-secure-normal-region",
		.of_match_table = dmaheap_sec_normal_region_match_table,
	},
};

static struct platform_driver iommu_test_dmaheap_sec_ccu0 = {
	.probe = dmaheap_sec_ccu0_probe,
	.driver = {
		.name = "dmaheap-secure-ccu0",
		.of_match_table = dmaheap_sec_ccu0_match_table,
	},
};

static struct platform_driver iommu_test_dmaheap_sec_up = {
	.probe = dmaheap_sec_video_up_probe,
	.driver = {
		.name = "dmaheap-secure-up",
		.of_match_table = dmaheap_sec_up_match_table,
	},
};

static struct platform_driver iommu_test_dmaheap_sec_vdec = {
	.probe = dmaheap_sec_vdec_probe,
	.driver = {
		.name = "dmaheap-secure-vdec",
		.of_match_table = dmaheap_sec_vdec_match_table,
	},
};

static struct platform_driver iommu_test_dmaheap_sec_pa = {
	.probe = dmaheap_sec_pa_probe,
	.driver = {
		.name = "dmaheap-secure-pa",
		.of_match_table = dmaheap_sec_pa_match_table,
	},
};
/*************************************************************************/

/* iova region test */
static struct platform_driver __maybe_unused iommu_test_driver_dom = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver __maybe_unused iommu_test_driver_cqdma = {
	.probe = iommu_test_cqdma_probe,
	.driver = {
		.name = "iommu-test-cqdma",
		.of_match_table = iommu_test_cqdma_match_table,
	},
};

static struct platform_driver *const iommu_test_drivers[] = {
	&iommu_test_driver_dom,
	&iommu_test_driver_cqdma,
	&iommu_test_dmaheap_normal,
	&iommu_test_dmaheap_sec_normal,
	&iommu_test_dmaheap_sec_ccu0,
	&iommu_test_dmaheap_sec_up,
	&iommu_test_dmaheap_sec_vdec,
	&iommu_test_dmaheap_sec_pa,
};

static int __init iommu_test_init(void)
{
	int ret;
	int i;

	pr_info("%s+\n", __func__);

	debug_data = kmalloc(sizeof(struct iommu_test_debug_data), GFP_KERNEL);
	if (!debug_data)
		return -ENOMEM;

	iommu_test_debug_init(debug_data);

	for (i = 0; i < ARRAY_SIZE(iommu_test_drivers); i++) {
		pr_info("%s, register %d\n", __func__, i);
		ret = platform_driver_register(iommu_test_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
				iommu_test_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	pr_info("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(iommu_test_drivers[i]);

	return ret;
}

static void __exit iommu_test_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(iommu_test_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(iommu_test_drivers[i]);
}
#else /* IOMMU_TEST_EN */
static int __init iommu_test_init(void)
{
	return 0;
}

static void __exit iommu_test_exit(void)
{
}
#endif /* IOMMU_TEST_EN */

module_init(iommu_test_init);
module_exit(iommu_test_exit);
MODULE_LICENSE("GPL v2");
