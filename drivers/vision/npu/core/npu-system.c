/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/dma-direct.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <linux/smc.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-heap.h>
#include <linux/notifier.h>

#include "common/npu-log.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "npu-stm-soc.h"
#include "npu-llc.h"
#include "npu-memory.h"
#include "mailbox_ipc.h"
#include "dsp-dhcp.h"
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
#include "dsp-common-type.h"
#endif

#if IS_ENABLED(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS)
#include "npu-util-memdump.h"
#endif

#if IS_ENABLED(QUADRA_BRINGUP_NOTDONE)
#define IOMMU_PRIV_SHIFT		10
#define DMA_ATTR_PRIV_SHIFT		16
#define DMA_ATTR_HAS_PRIV_DATA		(1UL << 15)
#define DMA_ATTR_SET_PRIV_DATA(val)	(DMA_ATTR_HAS_PRIV_DATA |	\
					 ((val) & 0xf) << DMA_ATTR_PRIV_SHIFT)
#define DMA_ATTR_TO_PRIV_PROT(val)	(((val) >> DMA_ATTR_PRIV_SHIFT) & 0x3)
#endif

#define BUF_SIZE 4096
#define BUF2_SIZE 240
static char buf[BUF_SIZE]; // print 4K characters when log2dram is triggered
static char buf2[BUF2_SIZE]; // used to print log2dram line by line(max 240 characters)

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
static void npu_imgloader_desc_release(struct npu_system *system)
{
	imgloader_desc_release(&system->binary.imgloader);
}

void npu_imgloader_shutdown(struct npu_system *system)
{
	imgloader_shutdown(&system->binary.imgloader);
}
#else
static void npu_imgloader_desc_release(__attribute__((unused))struct npu_system *system)
{
	return;
}

static void npu_imgloader_shutdown(__attribute__((unused))struct npu_system *system)
{
	return;
}
#endif

static struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned int flag)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_heap *dma_heap;

	dma_heap = dma_heap_find("system-uncached");
	if (dma_heap) {
		dma_buf = dma_heap_buffer_alloc(dma_heap, size, 0, flag);
		dma_heap_put(dma_heap);
	} else {
		npu_info("dma_heap is not exist\n");
	}

	return dma_buf;
}

static dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer)
{
	return sg_dma_address(buffer->sgt->sgl);
}

static void npu_memory_dma_buf_dva_unmap(
		__attribute__((unused))struct npu_memory_buffer *buffer)
{
	struct iommu_domain *domain = NULL;
	size_t unmapped = 0;

	domain = iommu_get_domain_for_dev(buffer->attachment->dev);
	if (!domain) {
		probe_warn("iommu_unmap failed\n");
		return;
	}

	unmapped = iommu_unmap(domain, buffer->daddr, buffer->size);
	if (!unmapped)
		probe_warn("iommu_unmap failed\n");
	if (unmapped != buffer->size)
		probe_warn("iomuu_unmap unmapped size(%lu) is not buffer size(%lu)\n",
				unmapped, buffer->size);
}

static int npu_iommu_dma_enable_best_fit_algo(struct device *dev)
{
#if IS_ENABLED(QUADRA_BRINGUP_NOTDONE)
	return 0;
#else
	return iommu_dma_enable_best_fit_algo(dev);
#endif
}

static unsigned long npu_memory_set_prot(int prot,
		struct dma_buf_attachment *attachment)
{
	probe_info("prot : %d", prot);
	attachment->dma_map_attrs |= DMA_ATTR_SET_PRIV_DATA(prot);
	return ((prot) << IOMMU_PRIV_SHIFT);
}

static int npu_reserve_iova(struct platform_device *pdev,
				dma_addr_t daddr, size_t size)
{
	return 0;
}

static int of_get_irq_count(struct device_node *dev)
{
	struct of_phandle_args irq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &irq) == 0)
		nr++;

	return nr;
}

static int npu_platform_get_irq(struct npu_system *system)
{
	int i, irq;

	system->irq_num = of_get_irq_count(system->pdev->dev.of_node);

	for (i = 0; i < system->irq_num; i++) {
		irq = platform_get_irq(system->pdev, i);
		if (irq < 0) {
			probe_err("fail(%d) in platform_get_irq(%d)\n", irq, i);
			irq = -EINVAL;
			goto p_err;
		}
		system->irq[i] = irq;
	}
	system->irq_num -= NPU_AFM_IRQ_CNT;
	system->afm_irq_num = NPU_AFM_IRQ_CNT;
	probe_info("NPU AFM irq registered, mailbox_irq : %u, afm_irq : %u\n",
		system->irq_num, system->afm_irq_num);

p_err:
	return irq;
}

struct system_pwr sysPwr;

#define OFFSET_END	0xFFFFFFFF

/* Initialzation steps for system_resume */
enum npu_system_resume_steps {
	NPU_SYS_RESUME_SETUP_WAKELOCK,
	NPU_SYS_RESUME_INIT_FWBUF,
	NPU_SYS_RESUME_FW_LOAD,
	NPU_SYS_RESUME_CLK_PREPARE,
	NPU_SYS_RESUME_FW_VERIFY,
	NPU_SYS_RESUME_SOC,
	NPU_SYS_RESUME_OPEN_INTERFACE,
	NPU_SYS_RESUME_COMPLETED
};

/* Initialzation steps for system_soc_resume */
enum npu_system_resume_soc_steps {
	NPU_SYS_RESUME_SOC_CPU_ON,
	NPU_SYS_RESUME_SOC_COMPLETED
};

static int npu_firmware_load(struct npu_system *system, int mode);

int npu_memory_alloc_from_heap(struct platform_device *pdev,
		struct npu_memory_buffer *buffer, dma_addr_t daddr,
		struct npu_memory *memory, const char *heapname, int prot)
{
	int ret = 0;
	bool complete_suc = false;
	unsigned long flags;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	phys_addr_t phys_addr;
	void *vaddr;
	int flag;

	size_t size;
	size_t map_size = 0;
	unsigned long iommu_attributes = 0;

	BUG_ON(!buffer);

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;
	strncpy(buffer->name, heapname, strlen(heapname));
	INIT_LIST_HEAD(&buffer->list);

	size = buffer->size;
	flag = 0;

	dma_buf = npu_memory_ion_alloc(size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		probe_err("npu_memory_ion_alloc is fail(%p)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma_buf = dma_buf;

	attachment = dma_buf_attach(dma_buf, &pdev->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		probe_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->attachment = attachment;

	if (prot)
		iommu_attributes = npu_memory_set_prot(prot, attachment);

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		probe_err("dma_buf_map_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->sgt = sgt;

	/* But, first phys. Not all */
	phys_addr = sg_phys(sgt->sgl);
	buffer->paddr = phys_addr;

	if (!daddr) {
		daddr = npu_memory_dma_buf_dva_map(buffer);
		if (IS_ERR_VALUE(daddr)) {
			probe_err("fail(err %pad) to allocate iova\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}
	} else {
		struct iommu_domain *domain = iommu_get_domain_for_dev(&pdev->dev);

		if (likely(domain)) {
			map_size = iommu_map_sg(domain, daddr, sgt->sgl, sgt->orig_nents, iommu_attributes);
		} else {
			probe_err("fail(err %pad) in iommu_map\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}

		if (prot) {
			ret = npu_reserve_iova(pdev, daddr, size);
			if (ret) {
				probe_err("failed(err %pad) in iommu_dma_reserve_iova(%d)\n", &daddr, ret);
				goto p_err;
			}
		}

		if (map_size != size) {
			probe_info("iommu_mag_sg return %lu, we want to map %lu",
				map_size, size);
		}
	}

	buffer->daddr = daddr;
	vaddr = npu_dma_buf_vmap(dma_buf);
	if (IS_ERR(vaddr) || !vaddr) {
		if (vaddr)
			probe_err("fail(err %p) in dma_buf_vmap\n", vaddr);
		else /* !vaddr */
			probe_err("fail(err NULL) in dma_buf_vmap\n");

		ret = -EFAULT;
		goto p_err;
	}
	buffer->vaddr = vaddr;

	complete_suc = true;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	list_add_tail(&buffer->list, &memory->alloc_list);
	memory->alloc_count++;
	spin_unlock_irqrestore(&memory->alloc_lock, flags);

p_err:
	if (complete_suc != true) {
		npu_memory_free_from_heap(&pdev->dev, memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

void npu_memory_free_from_heap(struct device *dev, struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	unsigned long flags;

	if (buffer->vaddr)
		npu_dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	if (buffer->daddr && !IS_ERR_VALUE(buffer->daddr))
		npu_memory_dma_buf_dva_unmap(buffer);
	if (buffer->sgt)
		dma_buf_unmap_attachment(buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
	if (buffer->attachment)
		dma_buf_detach(buffer->dma_buf, buffer->attachment);
	if (buffer->dma_buf)
		dma_buf_put(buffer->dma_buf);

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		memory->alloc_count--;
	} else
		npu_info("buffer[%pK] is not linked to alloc_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&memory->alloc_lock, flags);
}

void npu_soc_status_report(struct npu_system *system)
{
#if !IS_ENABLED(CONFIG_SOC_S5E8835)
	int ret = 0;

	BUG_ON(!system);

	npu_info("Print CPU PC value\n");
	ret = npu_cmd_map(system, "cpupc");
	if (ret)
		npu_err("fail(%d) in npu_cmd_map for cpupc\n", ret);
#endif
}

#if IS_ENABLED(CONFIG_EXYNOS_NPU_DRAM_FW_LOG_BUF)
#define DRAM_FW_REPORT_BUF_SIZE		(1024*1024)
#define DRAM_FW_PROFILE_BUF_SIZE	(1024*1024)

static struct npu_memory_v_buf fw_report_buf = {
	.size = DRAM_FW_REPORT_BUF_SIZE,
};
static struct npu_memory_v_buf fw_profile_buf = {
	.size = DRAM_FW_PROFILE_BUF_SIZE,
};

int npu_system_alloc_fw_dram_log_buf(struct npu_system *system)
{
	int ret = 0;
	BUG_ON(!system);

	npu_info("start: initialization.\n");

	if (!fw_report_buf.v_buf) {
		strcpy(fw_report_buf.name, "FW_REPORT");
		ret = npu_memory_v_alloc(&system->memory, &fw_report_buf);
		if (ret) {
			npu_err("fail(%d) in FW report buffer memory allocation\n", ret);
			return ret;
		}
		npu_fw_report_init(fw_report_buf.v_buf, fw_report_buf.size);
	} else {//Case of fw_report is already allocated by ion memory
		npu_dbg("fw_report is already initialized - %pK.\n", fw_report_buf.v_buf);
	}

	if (!fw_profile_buf.v_buf) {
		strcpy(fw_profile_buf.name, "FW_PROFILE");
		ret = npu_memory_v_alloc(&system->memory, &fw_profile_buf);
		if (ret) {
			npu_err("fail(%d) in FW profile memory allocation\n", ret);
			return ret;
		}
		npu_fw_profile_init(fw_profile_buf.v_buf, fw_profile_buf.size);
	} else {//Case of fw_profile is already allocated by ion memory
		npu_dbg("fw_profile is already initialized - %pK.\n", fw_profile_buf.v_buf);
	}

	/* Initialize firmware utc handler with dram log buf */
	ret = npu_fw_test_initialize(system);
	if (ret) {
		npu_err("npu_fw_test_probe() failed : ret = %d\n", ret);
		return ret;
	}

	npu_info("complete : initialization.\n");
	return ret;
}

static int npu_system_free_fw_dram_log_buf(void)
{
	/* TODO */
	return 0;
}

#else
#define npu_system_alloc_fw_dram_log_buf(t)	(0)
#define npu_system_free_fw_dram_log_buf(t)	(0)
#endif	/* CONFIG_EXYNOS_NPU_DRAM_FW_LOG_BUF */

static struct reg_cmd_list npu_cmd_list[] = {
	{	.name = "fwpwm",	.list = NULL,	.count = 0	},
	{	.name = "cpupc",	.list = NULL,	.count = 0	},
//	{	.name = "poweron",	.list = NULL,	.count = 0	},
//	{	.name = "poweroff",	.list = NULL,	.count = 0	},
//	{	.name = "gnpufreq",	.list = NULL,	.count = 0	},
	{	.name = "cpuon",	.list = NULL,	.count = 0	},
	{	.name = "cpuon64",	.list = NULL,	.count = 0	},
	{	.name = "cpuoff",	.list = NULL,	.count = 0	},

	{	.name = "hwacgen",	.list = NULL,	.count = 0	},
	{	.name = "hwacgendnc",	.list = NULL,	.count = 0	},
	{	.name = "hwacgennpu",	.list = NULL,	.count = 0	},
	{	.name = "hwacgendsp",	.list = NULL,	.count = 0	},
	{	.name = "hwacgdis",	.list = NULL,	.count = 0	},
	{	.name = "hwacgdisdnc",	.list = NULL,	.count = 0	},
	{	.name = "hwacgdisnpu",	.list = NULL,	.count = 0	},
	{	.name = "hwacgdisdsp",	.list = NULL,	.count = 0	},

	{	.name = "npufreq",	.list = NULL,	.count = 0	},
#if IS_ENABLED(CONFIG_NPU_AFM)
	{	.name = "afmdncen",	.list = NULL,	.count = 0	},
	{	.name = "afmdncdis",	.list = NULL,	.count = 0	},
	{	.name = "afmgnpu1en",	.list = NULL,	.count = 0	},
	{	.name = "afmgnpu1dis",	.list = NULL,	.count = 0	},
	{	.name = "chkdncitr",	.list = NULL,	.count = 0	},
	{	.name = "clrdncitr",	.list = NULL,	.count = 0	},
	{	.name = "clrdnctdc",	.list = NULL,	.count = 0	},
	{	.name = "chkgnpu1itr",	.list = NULL,	.count = 0	},
	{	.name = "clrgnpu1itr",	.list = NULL,	.count = 0	},
	{	.name = "clrgnpu1tdc",	.list = NULL,	.count = 0	},
	{	.name = "clrdncdiv",	.list = NULL,	.count = 0	},
	{	.name = "clrgnpu0div",	.list = NULL,	.count = 0	},
	{	.name = "clrgnpu1div",	.list = NULL,	.count = 0	},
	{	.name = "clrdspdiv",	.list = NULL,	.count = 0	},
	{	.name = "printafmst",	.list = NULL,	.count = 0	},
#endif
#if IS_ENABLED(CONFIG_NPU_STM)
	{	.name = "buspdiv1to1",	.list = NULL,	.count = 0	},
	{	.name = "buspdiv1to4",	.list = NULL,	.count = 0	},
	{	.name = "enablestm",	.list = NULL,	.count = 0	},
	{	.name = "disablestm",	.list = NULL,	.count = 0	},
	{	.name = "enstmdnc",	.list = NULL,	.count = 0	},
	{	.name = "disstmdnc",	.list = NULL,	.count = 0	},
	{	.name = "enstmnpu",	.list = NULL,	.count = 0	},
	{	.name = "disstmnpu",	.list = NULL,	.count = 0	},
	{	.name = "enstmdsp",	.list = NULL,	.count = 0	},
	{	.name = "disstmdsp",	.list = NULL,	.count = 0	},
	{	.name = "allow64stm",	.list = NULL,	.count = 0	},
	{	.name = "syncevent",	.list = NULL,	.count = 0	},
#endif
	{	.name = NULL,	.list = NULL,	.count = 0	}
};

static int npu_init_cmd_list(struct npu_system *system)
{
	int ret = 0;
	int i;

	BUG_ON(!system);

	system->cmd_list = (struct reg_cmd_list *)npu_cmd_list;

	for (i = 0; ((system->cmd_list) + i)->name; i++) {
		if (npu_get_reg_cmd_map(system, (system->cmd_list) + i) == -ENODEV)
			probe_info("No cmd for %s\n", ((system->cmd_list) + i)->name);
		else
			probe_info("register cmd for %s\n", ((system->cmd_list) + i)->name);
	}

	return ret;
}

static inline void set_max_npu_core(struct npu_system *system, s32 num)
{
	BUG_ON(num < 0);
	probe_info("Max number of npu core : %d\n", num);
	system->max_npu_core = num;
}

static int npu_rsvd_map(struct npu_system *system, struct npu_rmem_data *rmt)
{
	int ret = 0;
	unsigned int num_pages;
	struct page **pages, **page;
	phys_addr_t phys;

	/* try to map kvmap */
	num_pages = (unsigned int)DIV_ROUND_UP(rmt->area_info->size, PAGE_SIZE);
	pages = kcalloc(num_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		probe_err("fail to alloc pages for rmem(%s)\n", rmt->name);
		iommu_unmap(system->domain, rmt->area_info->daddr,
				(size_t) rmt->rmem->size);
		goto p_err;
	}

	phys = rmt->rmem->base;
	for (page = pages; (page - pages < num_pages); page++) {
		*page = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	rmt->area_info->paddr = rmt->rmem->base;

	rmt->area_info->vaddr = vmap(pages, num_pages,
			VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	kfree(pages);
	if (!rmt->area_info->vaddr) {
		ret = -ENOMEM;
		probe_err("fail to vmap %u pages for rmem(%s)\n",
				num_pages, rmt->name);
		iommu_unmap(system->domain, rmt->area_info->daddr,
				(size_t) rmt->area_info->size);
		goto p_err;
	}

p_err:
	return ret;
}

static int npu_init_iomem_area(struct npu_system *system)
{
	int ret = 0;
	int i, k, si, mi;
	void __iomem *iomem;
	struct device *dev;
	int iomem_count, init_count, phdl_cnt, rmem_cnt;
	struct iomem_reg_t *iomem_data;
	struct iomem_reg_t *iomem_init_data = NULL;
	struct iommu_domain	*domain;
	const char *iomem_name;
	const char *heap_name;
	struct npu_iomem_area *id;
	struct npu_memory_buffer *bd;
	struct npu_io_data *it;
	struct npu_mem_data *mt;
	struct npu_rmem_data *rmt;
	struct device_node *mems_node, *mem_node, *phdl_node;
	struct reserved_mem *rsvd_mem;
	char tmpname[32];
	u32 core_num;
	u32 size;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &(system->pdev->dev);
	iomem_count = of_property_count_strings(
			dev->of_node, "samsung,npumem-names") / 2;
	if (IS_ERR_VALUE((unsigned long)iomem_count)) {
		probe_err("invalid iomem list in %s node", dev->of_node->name);
		ret = -EINVAL;
		goto err_exit;
	}
	probe_info("%d iomem items\n", iomem_count);

	iomem_data = (struct iomem_reg_t *)devm_kzalloc(dev,
			iomem_count * sizeof(struct iomem_reg_t), GFP_KERNEL);
	if (!iomem_data) {
		probe_err("failed to alloc for iomem data");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = of_property_read_u32_array(dev->of_node, "samsung,npumem-address", (u32 *)iomem_data,
			iomem_count * sizeof(struct iomem_reg_t) / sizeof(u32));
	if (ret) {
		probe_err("failed to get iomem data (%d)\n", ret);
		ret = -EINVAL;
		goto err_data;
	}

	si = 0; mi = 0;
	for (i = 0; i < iomem_count; i++) {
		ret = of_property_read_string_index(dev->of_node,
				"samsung,npumem-names", i * 2, &iomem_name);
		if (ret) {
			probe_err("failed to read iomem name %d from %s node(%d)\n",
					i, dev->of_node->name, ret);
			ret = -EINVAL;
			goto err_data;
		}
		ret = of_property_read_string_index(dev->of_node,
				"samsung,npumem-names", i * 2 + 1, &heap_name);
		if (ret) {
			probe_err("failed to read iomem type for %s (%d)\n",
					iomem_name, ret);
			ret = -EINVAL;
			goto err_data;
		}
		if (strcmp(heap_name, "SFR") // !SFR
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		    && strcmp(heap_name, "IMB") // !IMB
#endif
		    ){
			mt = &((system->mem_area)[mi]);
			mt->heapname = heap_name;
			mt->name = iomem_name;
			probe_info("MEM %s(%d) uses %s\n", mt->name, mi,
				strcmp("", mt->heapname) ? mt->heapname : "System Heap");

			mt->area_info = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);
			if (mt->area_info == NULL) {
				probe_err("error allocating npu buffer\n");
				goto err_data;
			}
			mt->area_info->size = (iomem_data + i)->size;
			probe_info("Flags[%08x], DVA[%08x], SIZE[%08x]",
				(iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size);
			ret = npu_memory_alloc_from_heap(system->pdev, mt->area_info,
					(iomem_data + i)->paddr, &system->memory, mt->heapname, (iomem_data + i)->vaddr);
			if (ret) {
				for (k = 0; k < mi; k++) {
					bd = (system->mem_area)[k].area_info;
					if (bd) {
						if (bd->vaddr)
							npu_memory_free_from_heap(&system->pdev->dev, &system->memory, bd);
						devm_kfree(dev, bd);
					}
				}
				probe_err("buffer allocation for %s heap failed w/ err: %d\n",
						mt->name, ret);
				ret = -EFAULT;
				goto err_data;
			}
			probe_info("%s : daddr[%08x], [%08x] alloc memory\n",
					iomem_name, (iomem_data + i)->paddr, (iomem_data + i)->size);

			mi++;
		} else { // heap_name is SFR or IMB
			it = &((system->io_area)[si]);
			it->heapname = heap_name;
			it->name = iomem_name;

			probe_info("SFR %s(%d)\n", it->name, si);

			it->area_info = (struct npu_iomem_area *)devm_kzalloc(dev, sizeof(struct npu_iomem_area), GFP_KERNEL);
			if (it->area_info == NULL) {
				probe_err("error allocating npu iomem\n");
				goto err_data;
			}
			id = (struct npu_iomem_area *)it->area_info;
			id->paddr = (iomem_data + i)->paddr;
			id->size = (iomem_data + i)->size;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
			if (!strcmp(heap_name, "IMB"))
				goto common;
#endif

			iomem = devm_ioremap(&(system->pdev->dev),
				(iomem_data + i)->paddr, (iomem_data + i)->size);
			if (IS_ERR_OR_NULL(iomem)) {
				probe_err("fail(%pK) in devm_ioremap(0x%08x, %u)\n",
					iomem, id->paddr, (u32)id->size);
				ret = -EFAULT;
				goto err_data;
			}
			id->vaddr = iomem;

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
common:
#endif
			probe_info("%s : Paddr[%08x], [%08x] => Mapped @[%pK], Length = %llu\n",
					iomem_name, (iomem_data + i)->paddr, (iomem_data + i)->size,
					id->vaddr, id->size);

			/* initial settings for this area
			 * use dt "samsung,npumem-xxx"
			 */
			tmpname[0] = '\0';
			strcpy(tmpname, "samsung,npumem-");
			strcat(tmpname, iomem_name);
			init_count = of_property_read_variable_u32_array(dev->of_node,
					tmpname, (u32 *)iomem_init_data,
					sizeof(struct reg_set_map) / sizeof(u32), 0);
			if (init_count > 0) {
				probe_trace("start in init settings for %s\n", iomem_name);
				ret = npu_set_hw_reg(id,
						(struct reg_set_map *)iomem_init_data,
						init_count / sizeof(struct reg_set_map), 0);
				if (ret) {
					probe_err("fail(%d) in npu_set_hw_reg\n", ret);
					goto err_data;
				}
				probe_info("complete in %d init settings for %s\n",
						(int)(init_count / sizeof(struct reg_set_map)),
						iomem_name);
			}
			si++;
		}
	}

	/* reserved memory */
	rmem_cnt = 0;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		probe_err("fail to get domain for dnc\n");
		goto err_data;
	}
	system->domain = domain;

	mems_node = of_get_child_by_name(dev->of_node, "samsung,npurmem-address");
	if (!mems_node) {
		ret = 0;	/* not an error */
		probe_err("null npurmem-address node\n");
		goto err_data;
	}

	for_each_child_of_node(mems_node, mem_node) {
		rmt = &((system->rmem_area)[rmem_cnt]);
		rmt->area_info = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);

		rmt->name = kbasename(mem_node->full_name);
		ret = of_property_read_u32(mem_node,
				"iova", (u32 *) &rmt->area_info->daddr);
		if (ret) {
			probe_err("'iova' is mandatory but not defined (%d)\n", ret);
			goto err_data;
		}

		phdl_cnt = of_count_phandle_with_args(mem_node,
					"memory-region", NULL);
		if (phdl_cnt > 1) {
			probe_err("only one phandle required. "
					"phdl_cnt(%d)\n", phdl_cnt);
			ret = -EINVAL;
			goto err_data;
		}

		if (phdl_cnt == 1) {	/* reserved mem case */
			phdl_node = of_parse_phandle(mem_node,
						"memory-region", 0);
			if (!phdl_node) {
				ret = -EINVAL;
				probe_err("fail to get memory-region in name(%s)\n", rmt->name);
				goto err_data;
			}

			rsvd_mem = of_reserved_mem_lookup(phdl_node);
			if (!rsvd_mem) {
				ret = -EINVAL;
				probe_err("fail to look-up rsvd mem(%s)\n", rmt->name);
				goto err_data;
			}
			rmt->rmem = rsvd_mem;
		}

		ret = of_property_read_u32(mem_node,
				"size", &size);
		if (ret) {
			probe_err("'size' is mandatory but not defined (%d)\n", ret);
			goto err_data;
		} else {
			if (size > rmt->rmem->size) {
				ret = -EINVAL;
				probe_err("rmt->size(%x) > rsvd_size(%llx)\n", size, rmt->rmem->size);
				goto err_data;
			}
		}
		rmt->area_info->size = size;

		/* iommu map */
		ret = iommu_map(system->domain, rmt->area_info->daddr, rmt->rmem->base,
										rmt->area_info->size, 0);
		if (ret) {
			probe_err("fail to map iova for rmem(%s) ret(%d)\n",
				rmt->name, ret);
			goto err_data;
		}

		ret = npu_rsvd_map(system, rmt);
		if (ret) {
			probe_err("fail to map kvmap, rmem(%s), ret(%d)", rmt->name, ret);
			goto err_data;
		}

		rmem_cnt++;
	}

	/* set NULL for last */
	(system->mem_area)[mi].heapname = NULL;
	(system->mem_area)[mi].name = NULL;
	(system->rmem_area)[rmem_cnt].heapname = NULL;
	(system->rmem_area)[rmem_cnt].name = NULL;
	(system->io_area)[si].heapname = NULL;
	(system->io_area)[si].name = NULL;

	ret = of_property_read_u32(dev->of_node,
			"samsung,npusys-corenum", &core_num);
	if (ret)
		core_num = NPU_SYSTEM_DEFAULT_CORENUM;

	set_max_npu_core(system, core_num);

	probe_info("complete in init_iomem_area\n");
err_data:
	devm_kfree(dev, iomem_data);
err_exit:
	return ret;
}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
int dsp_system_load_binary(struct npu_system *system)
{
	int ret = 0, i;
	struct device *dev;
	struct npu_mem_data *md;

	BUG_ON(!system);

	dev = &system->pdev->dev;

	/* load binary only for heap memory area */
	for (md = (system->mem_area), i = 0; md[i].name; i++) {
		/* if heapname is filename,
		 * this means we need load the file on this memory area
		 */
		if ((md[i].heapname)[strlen(md[i].heapname)-4] == '.') {
			const struct firmware	*fw_blob;

			ret = request_firmware_direct(&fw_blob, md[i].heapname, dev);
			if (ret < 0) {
				npu_err("%s : error in loading %s on %s\n",
						md[i].name, md[i].heapname, md[i].name);
				goto err_data;
			}
			if (fw_blob->size > (md[i].area_info)->size) {
				npu_err("%s : not enough memory for %s (%d/%d)\n",
						md[i].name, md[i].heapname,
						(int)fw_blob->size, (int)(md[i].area_info)->size);
				release_firmware(fw_blob);
				ret = -ENOMEM;
				goto err_data;
			}
			memcpy((void *)(u64)((md[i].area_info)->vaddr), fw_blob->data, fw_blob->size);
			release_firmware(fw_blob);
			npu_info("%s : %s loaded on %s\n",
					md[i].name, md[i].heapname, md[i].name);
		}
	}
err_data:
	return ret;
}
#endif

static int npu_clk_init(struct npu_system *system)
{
	/* TODO : need core driver */
	return 0;
}

static const char *npu_check_fw_arch(struct npu_system *system,
				struct npu_memory_buffer *fwmem)
{
	if (!strncmp(FW_64_SYMBOL, fwmem->vaddr + FW_SYMBOL_OFFSET, FW_64_SYMBOL_S)) {
		npu_info("FW is 64 bit, cmd map %s\n", FW_64_BIT);
		return FW_64_BIT;
	}

	npu_info("FW is 32 bit, cmd map : %s\n", FW_32_BIT);
	return FW_32_BIT;
}

static int npu_cpu_on(struct npu_system *system)
{
	int ret = 0;
	struct npu_iomem_area *tcusram;
	struct npu_memory_buffer *fwmem;

	BUG_ON(!system);

	npu_dbg("start\n");

	tcusram = npu_get_io_area(system, "tcusram");
	fwmem = npu_get_mem_area(system, "fwmem");

#if IS_ENABLED(CONFIG_NPU_USE_MBR)
	npu_info("use Master Boot Record\n");
	npu_dbg("ask to jump to  0x%x\n", (u32)fwmem->daddr);
	ret = dsp_npu_release(true, fwmem->daddr);
	if (ret) {
		npu_err("Failed to release CPU_SS : %d\n", ret);
		goto err_exit;
	}
#endif
#if IS_ENABLED(CONFIG_NPU_USE_JUMPCODE)
	if (tcusram) {
		memset_io(tcusram->vaddr, 0x0, tcusram->size);
		/* load SRAM binary */
		memcpy_toio(tcusram->vaddr, npu_jump_code, npu_jump_code_len);
	}
#endif
	ret = npu_cmd_map(system, npu_check_fw_arch(system, fwmem));
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for cpu_on\n", ret);
		goto err_exit;
	}

	npu_info("release CA32\n");
	npu_dbg("complete\n");
	return 0;
err_exit:
	npu_info("error(%d) in npu_cpu_on\n", ret);
	return ret;
}

static int npu_cpu_off(struct npu_system *system)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_NPU_USE_MBR)
	ret = dsp_npu_release(false, 0);
	if (ret) {
		npu_err("Failed to release CPU_SS : %d\n", ret);
		goto err_exit;
	}
#endif
	BUG_ON(!system);

	npu_dbg("start\n");
	ret = npu_cmd_map(system, "cpuoff");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for cpu_off\n", ret);
		goto err_exit;
	}

	npu_dbg("complete\n");
	return 0;
err_exit:
	npu_info("error(%d)\n", ret);
	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
int npu_pwr_on(struct npu_system *system)
{
	int ret = 0;

	BUG_ON(!system);

	npu_dbg("start\n");

	ret = npu_cmd_map(system, "pwron");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for pwr_on\n", ret);
		goto err_exit;
	}

	npu_dbg("complete\n");
	return 0;
err_exit:
	npu_info("error(%d)\n", ret);
	return ret;

}
#endif

static int npu_system_soc_probe(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;

	BUG_ON(!system);
	probe_info("system soc probe: ioremap areas\n");

	ret = npu_init_iomem_area(system);
	if (ret) {
		probe_err("fail(%d) in init iomem area\n", ret);
		goto p_err;
	}

	ret = npu_init_cmd_list(system);
	if (ret) {
		probe_err("fail(%d) in cmd list register\n", ret);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_NPU_STM)
	ret = npu_stm_probe(system);
	if (ret)
		probe_err("npu_stm_probe error : %d\n", ret);
#endif

p_err:
	return ret;
}

#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION) && !IS_ENABLED(CONFIG_SOC_S5E8835)
int npu_system_prepare_cam_on_off(struct notifier_block *noti_data, unsigned long on, void *data)
{
	bool is_on = (bool) on;
	struct npu_device *device;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_info *d;

	device = container_of(noti_data, struct npu_device, noti_data);
	info = device->sched;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return -1;
	}

	if (is_on) {
		npu_info("cam on\n");
		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			if (!strcmp("NPU0", d->name) || !strcmp("NPU1", d->name) || !strcmp("DSP", d->name)) {
				npu_scheduler_set_freq(d, &d->qos_req_max_cam_noti, NPU_SHARED_MIF_PLL_CLK);
			}
		}
		mutex_unlock(&info->exec_lock);
	} else {
		npu_info("cam off\n");
		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
			if (!strcmp("NPU0", d->name) || !strcmp("NPU1", d->name) || !strcmp("DSP", d->name)) {
				npu_scheduler_set_freq(d, &d->qos_req_max_cam_noti, d->max_freq);
			}
		}
		mutex_unlock(&info->exec_lock);
	}

	return 0;
}

extern int register_cam_pwr_notifier(struct notifier_block *nb);
extern int unregister_cam_pwr_notifier(struct notifier_block *nb);

int __register_cam_pwr_notifier(struct notifier_block *nb)
{
	return register_cam_pwr_notifier(nb);
}

int __unregister_cam_pwr_notifier(struct notifier_block *nb)
{
	return unregister_cam_pwr_notifier(nb);
}
#endif

int npu_system_probe(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	void *addr1, *addr2, *addr3 = NULL;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!pdev);

	dev = &pdev->dev;
	device = container_of(system, struct npu_device, system);
	system->pdev = pdev;	/* TODO: Reference counting ? */

	ret = npu_platform_get_irq(system);
	if (ret < 0)
		goto p_err;

	ret = npu_memory_probe(&system->memory, dev);
	if (ret) {
		probe_err("fail(%d) in npu_memory_probe\n", ret);
		goto p_err;
	}

	/* Invoke platform specific probe routine */
	ret = npu_system_soc_probe(system, pdev);
	if (ret) {
		probe_err("fail(%d) in npu_system_soc_probe\n", ret);
		goto p_err;
	}

	addr1 = (void *)npu_get_io_area(system, "sfrmbox0")->vaddr;
	addr2 = (void *)npu_get_io_area(system, "sfrmbox1")->vaddr;
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	addr3 = (void *)npu_get_io_area(system, "sfrmbox2")->vaddr;
#endif
	ret = npu_interface_probe(dev, addr1, addr2, addr3);
	if (ret) {
		probe_err("fail(%d) in npu_interface_probe\n", ret);
		goto p_err;
	}

	ret = npu_binary_init(&system->binary,
		dev,
		NPU_FW_PATH1,
		NPU_FW_PATH2,
		NPU_FW_NAME);
	if (ret) {
		probe_err("fail(%d) in npu_binary_init\n", ret);
		goto p_err;
	}

	ret = npu_util_memdump_probe(system);
	if (ret) {
		probe_err("fail(%d) in npu_util_memdump_probe\n", ret);
		goto p_err;
	}

	ret = npu_iommu_dma_enable_best_fit_algo(dev);
	if (ret) {
		probe_err("fail(%d) npu_iommu_dma_enable_best_fit_algo\n", ret);
		goto p_err;
	}

	ret = npu_scheduler_probe(device);
	if (ret) {
		probe_err("npu_scheduler_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

	ret = npu_qos_probe(system);
	if (ret) {
		probe_err("npu_qos_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

	ret = npu_imgloader_probe(&system->binary, dev);
	if (ret) {
		probe_err("npu_imgloader_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	memset(&system->imb_alloc_data, 0, sizeof(struct imb_alloc_info));
	memset(&system->imb_size_ctl, 0, sizeof(struct imb_size_control));
	mutex_init(&system->imb_alloc_lock);
	init_waitqueue_head(&system->imb_size_ctl.waitq);
	system->chunk_imb = npu_get_io_area(system, "CHUNK_IMB");
#endif

#if IS_ENABLED(CONFIG_PM_SLEEP)
	/* initialize the npu wake lock */
	npu_wake_lock_init(dev, &system->ws,
				NPU_WAKE_LOCK_SUSPEND, "npu_run_wlock");
#endif
	init_waitqueue_head(&sysPwr.wq);
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	init_waitqueue_head(&system->dsp_wq);
#endif

	sysPwr.system_result.result_code = NPU_SYSTEM_JUST_STARTED;

	system->layer_start = NPU_SET_DEFAULT_LAYER;
	system->layer_end = NPU_SET_DEFAULT_LAYER;

	system->fw_cold_boot = true;

#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION) && !IS_ENABLED(CONFIG_SOC_S5E8835)
	device->noti_data.notifier_call = npu_system_prepare_cam_on_off;
	npu_info("register_cam_pwr_notifier called\n");
	__register_cam_pwr_notifier(&device->noti_data);
#endif
	goto p_exit;
p_qos_err:
p_err:
p_exit:
	return ret;
}

static int npu_system_soc_release(struct npu_system *system, struct platform_device *pdev)
{
	int ret;

	ret = npu_stm_release(system);
	if (ret)
		npu_err("npu_stm_release error : %d\n", ret);
	return 0;
}

/* TODO: Implement throughly */
int npu_system_release(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!pdev);

	dev = &pdev->dev;
	device = container_of(system, struct npu_device, system);

#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION) && !IS_ENABLED(CONFIG_SOC_S5E8835)
	npu_info("unregister_cam_pwr_notifier called\n");
	__unregister_cam_pwr_notifier(&device->noti_data);
#endif

#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_destroy(system->ws);
#endif
	ret = npu_scheduler_release(device);
	if (ret)
		probe_err("fail(%d) in npu_scheduler_release\n", ret);

	/* Invoke platform specific release routine */
	ret = npu_system_soc_release(system, pdev);
	if (ret)
		probe_err("fail(%d) in npu_system_soc_release\n", ret);

	npu_imgloader_desc_release(system);

	return ret;
}

int npu_system_open(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);
	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);
	system->binary.dev = dev;

	ret = npu_memory_open(&system->memory);
	if (ret) {
		npu_err("fail(%d) in npu_memory_open\n", ret);
		goto p_err;
	}

	ret = npu_util_memdump_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_util_memdump_open\n", ret);
		goto p_err;
	}

	ret = npu_scheduler_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_open\n", ret);
		goto p_err;
	}
	// to check available max freq for current NPU dvfs governor, need to finish scheduler open
	// then we can set boost as available
	npu_scheduler_boost_on(device->sched);

	ret = npu_qos_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_qos_open\n", ret);
		goto p_err;
	}

	/* Clear resume steps */
	system->resume_steps = 0;
#if IS_ENABLED(CONFIG_NPU_STM)
	system->fw_load_success = 0;
#endif
p_err:
	return ret;
}

int npu_system_close(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device;

	BUG_ON(!system);
	device = container_of(system, struct npu_device, system);

	ret = npu_qos_close(system);
	if (ret)
		npu_err("fail(%d) in npu_qos_close\n", ret);

	ret = npu_scheduler_close(device);
	if (ret)
		npu_err("fail(%d) in npu_scheduler_close\n", ret);

#if IS_ENABLED(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS)
	ret = npu_util_memdump_close(system);
	if (ret)
		npu_err("fail(%d) in npu_util_memdump_close\n", ret);
#endif
	ret = npu_memory_close(&system->memory);
	if (ret)
		npu_err("fail(%d) in npu_memory_close\n", ret);

	npu_llc_close(device->sched);

	return ret;
}

static inline void print_iomem_area(const char *pr_name, const struct npu_iomem_area *mem_area)
{
	if (mem_area->vaddr)
		npu_dbg("(%13s) Phy(0x%08x)-(0x%08llx) Virt(%pK) Size(%llu)\n",
			 pr_name, mem_area->paddr, mem_area->paddr + mem_area->size,
			 mem_area->vaddr, mem_area->size);
	else
		npu_dbg("(%13s) Reserved Phy(0x%08x)-(0x%08llx) Size(%llu)\n",
			 pr_name, mem_area->paddr, mem_area->paddr + mem_area->size,
			 mem_area->size);
}

static void print_all_iomem_area(const struct npu_system *system)
{
	int i;

	npu_dbg("start in IOMEM mapping\n");
	for (i = 0; i < NPU_MAX_IO_DATA && system->io_area[i].name != NULL; i++) {
		if (system->io_area[i].area_info)
			print_iomem_area(system->io_area[i].name, system->io_area[i].area_info);
	}
	npu_dbg("end in IOMEM mapping\n");
}

static int npu_system_soc_resume(struct npu_system *system, u32 mode)
{
	int ret = 0;

	BUG_ON(!system);

	/* Clear resume steps */
	system->resume_soc_steps = 0;

	print_all_iomem_area(system);

	npu_clk_init(system);

	ret = npu_cpu_on(system);
	if (ret) {
		npu_err("fail(%d) in npu_cpu_on\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps);
	set_bit(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps);

	return ret;
p_err:
	npu_err("Failure detected[%d].\n", ret);
	return ret;
}

int npu_system_soc_suspend(struct npu_system *system)
{
	int ret = 0;

	BUG_ON(!system);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps, "SOC completed", ;);
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps, "Turn NPU cpu off", {
		ret = npu_cpu_off(system);
		if (ret)
			npu_err("fail(%d) in npu_cpu_off\n", ret);
	});

	if (system->resume_soc_steps != 0)
		npu_warn("Missing clean-up steps [%lu] found.\n", system->resume_soc_steps);

	/* Function itself never be failed, even thought there was some error */
	return ret;
}

int npu_system_resume(struct npu_system *system, u32 mode)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;
	struct npu_memory_buffer *fwmbox;
	struct npu_memory_buffer *fwmem;
	struct npu_binary		vector_binary;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	npu_info("fw_cold_boot(%d)\n", system->fw_cold_boot);
	dev = &system->pdev->dev;
	vector_binary.dev = dev;

	device = container_of(system, struct npu_device, system);
	fwmbox = npu_get_mem_area(system, "fwmbox");
	system->mbox_hdr = (volatile struct mailbox_hdr *)(fwmbox->vaddr);

	npu_info("mbox_hdr physical : %llx\n", fwmbox->paddr);

	/* Clear resume steps */
	system->resume_steps = 0;

#if IS_ENABLED(CONFIG_PM_SLEEP)
	/* prevent the system to suspend */
	if (!npu_wake_lock_active(system->ws)) {
		npu_wake_lock(system->ws);
		npu_info("wake_lock, now(%d)\n", npu_wake_lock_active(system->ws));
	}
	set_bit(NPU_SYS_RESUME_SETUP_WAKELOCK, &system->resume_steps);
#endif

	if (system->fw_cold_boot) {
		ret = npu_system_alloc_fw_dram_log_buf(system);
		if (ret) {
			npu_err("fail(%d) in npu_system_alloc_fw_dram_log_buf\n", ret);
			goto p_err;
		}
	}
	set_bit(NPU_SYS_RESUME_INIT_FWBUF, &system->resume_steps);

	if (system->fw_cold_boot) {
		npu_dbg("reset FW mailbox memory : paddr 0x%08llx, vaddr 0x%p, daddr 0x%08llx, size %lu\n",
				fwmbox->paddr, fwmbox->vaddr, fwmbox->daddr, fwmbox->size);

		memset(fwmbox->vaddr, 0, fwmbox->size);

		ret = npu_firmware_load(system, 0);
		if (ret) {
			npu_err("fail(%d) in npu_firmware_load\n", ret);
			goto p_err;
		}
	}
	set_bit(NPU_SYS_RESUME_FW_LOAD, &system->resume_steps);
	set_bit(NPU_SYS_RESUME_FW_VERIFY, &system->resume_steps);
	set_bit(NPU_SYS_RESUME_CLK_PREPARE, &system->resume_steps);

	if (system->fw_cold_boot) {
		/* Clear mailbox area and setup some initialization variables */
		memset((void *)system->mbox_hdr, 0, sizeof(*system->mbox_hdr));
	}
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	/* Save hardware info */
	system->dsp_system_flag = 0x0;
#endif

	/* Invoke platform specific resume routine */
	if (system->fw_cold_boot) {
		fwmem = npu_get_mem_area(system, "fwmem");

		if (likely(fwmem && fwmem->vaddr)) {
			npu_fw_slave_load(&vector_binary, fwmem->vaddr + 0xf000);
			print_ufw_signature(fwmem);
		}
	}

	ret = npu_system_soc_resume(system, mode);
	if (ret) {
		npu_err("fail(%d) in npu_system_soc_resume\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC, &system->resume_steps);

	ret = npu_interface_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_interface_open\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_OPEN_INTERFACE, &system->resume_steps);

	set_bit(NPU_SYS_RESUME_COMPLETED, &system->resume_steps);
#if IS_ENABLED(CONFIG_NPU_STM)
	system->fw_load_success = NPU_FW_LOAD_SUCCESS;
#endif
	return ret;
p_err:
	npu_err("Failure detected[%d]. Set emergency recovery flag.\n", ret);
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
	ret = 0;//emergency case will be cared by suspend func
	return ret;
}

int npu_system_suspend(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

	npu_info("fw_cold_boot(%d)\n", system->fw_cold_boot);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_COMPLETED, &system->resume_steps, NULL, ;);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_OPEN_INTERFACE, &system->resume_steps, "Close interface", {
		ret = npu_interface_close(system);
		if (ret)
			npu_err("fail(%d) in npu_interface_close\n", ret);
	});

	if (system->fw_cold_boot)
		BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_FW_LOAD, &system->resume_steps, "FW load", {
			npu_imgloader_shutdown(system);
		});

	/* Invoke platform specific suspend routine */
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC, &system->resume_steps, "SoC suspend", {
		ret = npu_system_soc_suspend(system);
		if (ret)
			npu_err("fail(%d) in npu_system_soc_suspend\n", ret);
	});

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_CLK_PREPARE, &system->resume_steps, "Unprepare clk", ;);
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_FW_VERIFY, &system->resume_steps, "FW VERIFY suspend", {
		/* Do not need anything */
	});

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_INIT_FWBUF, &system->resume_steps, "Free DRAM fw log buf", {
		ret = npu_system_free_fw_dram_log_buf();
		if (ret)
			npu_err("fail(%d) in npu_cpu_off\n", ret);
	});

#if IS_ENABLED(CONFIG_PM_SLEEP)
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SETUP_WAKELOCK, &system->resume_steps, "Unlock wake lock", {
		if (npu_wake_lock_active(system->ws)) {
			npu_wake_unlock(system->ws);
			npu_info("wake_unlock, now(%d)\n", npu_wake_lock_active(system->ws));
		}
	});
#endif

	if (system->resume_steps != 0)
		npu_warn("Missing clean-up steps [%lu] found.\n", system->resume_steps);

	/* Function itself never be failed, even thought there was some error */
	return 0;
}

int npu_system_start(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

#if IS_ENABLED(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS)
	ret = npu_util_memdump_start(system);
	if (ret) {
		npu_err("fail(%d) in npu_util_memdump_start\n", ret);
		goto p_err;
	}
#endif

	ret = npu_scheduler_start(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_start\n", ret);
		goto p_err;
	}

p_err:

	return ret;
}

int npu_system_stop(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	BUG_ON(!system);
	BUG_ON(!system->pdev);

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

#if IS_ENABLED(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS)
	ret = npu_util_memdump_stop(system);
	if (ret) {
		npu_err("fail(%d) in npu_util_memdump_stop\n", ret);
		goto p_err;
	}
#endif

	ret = npu_scheduler_stop(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_stop\n", ret);
		goto p_err;
	}

p_err:

	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
int npu_system_save_result(struct npu_session *session, struct nw_result nw_result)
{
	int ret = 0;

	sysPwr.system_result.result_code = nw_result.result_code;
	wake_up(&sysPwr.wq);
	return ret;
}
#endif

static int npu_firmware_load(struct npu_system *system, int mode)
{
	int ret = 0;
#if defined(CLEAR_SRAM_ON_FIRMWARE_LOADING)
	u32 v;
#endif
	struct npu_memory_buffer *fwmem;

	BUG_ON(!system);

	fwmem = npu_get_mem_area(system, "fwmem");

#ifdef CLEAR_SRAM_ON_FIRMWARE_LOADING
#ifdef CLEAR_ON_SECOND_LOAD_ONLY

	BUG_ON(!system->fwmemory)

	npu_dbg("start\n");
	if (fwmem->vaddr) {
		v = readl(fwmem->vaddr + fwmem->size - sizeof(u32));
		npu_dbg("firmware load: Check current signature value : 0x%08x (%s)\n",
			v, (v == 0)?"First load":"Second load");
	}
#else
	v = 1;
#endif
	if (v != 0) {
		if (fwmem->vaddr) {
			npu_dbg("firmware load : clear working memory at %p(0x%llx), Len(%llu)\n",
				fwmem->vaddr, fwmem->daddr, (long long unsigned int)fwmem->size);
			/* Using memset here causes unaligned access fault.
			Refer: https://patchwork.kernel.org/patch/6362401/ */
			memset(fwmem->vaddr, 0, fwmem->size);
		}
	}
#else
	if (fwmem->vaddr) {
		npu_dbg("firmware load: clear firmware signature at %pK(u64)\n",
			fwmem->vaddr + fwmem->size - sizeof(u64));
		writel(0, fwmem->vaddr + fwmem->size - sizeof(u64));
	}
#endif
	if (fwmem->vaddr) {
		npu_dbg("firmware load: read and locate firmware to %pK\n", fwmem->vaddr);
		ret = npu_firmware_file_read_signature(&system->binary,
					fwmem->vaddr, fwmem->size, mode);
		if (ret) {
			npu_err("error(%d) in npu_firmware_file_read_signature\n", ret);
			goto err_exit;
		}
		npu_dbg("checking firmware head MAGIC(0x%08x)\n", *(u32 *)fwmem->vaddr);
	}

	npu_dbg("complete\n");
	return ret;
err_exit:

	npu_err("error(%d)\n", ret);
	return ret;
}

/*
 * functionality helps to get logs during fw-bootup stage
 * where fw-report logs will be present after the mailbox
 * downward initialization in the fw
 */
void fw_print_log2dram(struct npu_system *system, u32 len)
{
	struct npu_memory_buffer *fw_log;
	char *start = 0, *end = 0;
	u32 pos = 0, loc = 0;
	int i = 0;

	fw_log = npu_get_mem_area(system, "fwlog");

	if (fw_log == NULL) {
		pr_err("fw_log pointer is NULL\n");
		goto error;
	}

	pr_info("FW_LOG vaddr: 0x%p, daddr: 0x%x size: 0x%x\n",
			fw_log->vaddr, (unsigned int)fw_log->daddr,
			(unsigned int)fw_log->size);
	if (!fw_log->vaddr) {
		pr_err("fw_log vaddr is null\n");
		goto error;
	}

	start = (char *)fw_log->vaddr;
	end = (char *)((u64)fw_log->vaddr + (u64)fw_log->size);
	pos = system->mbox_hdr->log_dram;

	if (pos == 0) {
		/*
		 * log2dram will be executed only if there is a positive value
		 * from pos; pos == 0 is a corner case where the whole 2MB of
		 * log2dram is filled then log2dram will not be executed further
		 */
		goto error;
	} else {
		if (pos > fw_log->size || len > fw_log->size) {
			pr_err("fw_log pos(%d) or len(%d) is invalid!\n", pos, len);
			goto error;
		}

		if (len > BUF_SIZE)
			len = BUF_SIZE;

		memset(buf, 0, len);
		memset(buf2, 0, BUF2_SIZE);


		pr_info("pos value is %d\n", pos);
		pr_info("(pos-len): %d", (pos - len));
		if (pos < len) {
			pr_info("start address of log2dram(circular case): 0x%x, dest address:0x%x\n",
				(unsigned int)((u64)end - (u64)(len - pos)),
				(unsigned int)((u64)start + (u64)len));
			memcpy(buf, (char *)((u64)end - (u64)(len - pos)), (len - pos));
			memcpy(&buf[(len - pos)], start, pos);
		} else {
			pr_info("start address of log2dram: 0x%x, dest address:0x%x\n",
				(unsigned int)((u64)start + (u64)(pos - len)),
				(unsigned int)((u64)start + (u64)(pos - len) + (u64)len));
			memcpy(buf, (char *)((u64)start + (u64)(pos - len)), len);
		}
		pr_err("---------FW LOG2DRAM STARTS---------");
		for (i = 0; i < len; i++) {
			buf2[loc] = buf[i];
			loc++;
			if (buf[i] == '\n' || loc == (BUF2_SIZE-1)) {
				buf2[loc] = '\0';
				pr_err("%s", buf2);
				loc = 0;
			}
		}

		if (loc) {
			buf2[loc] = '\0';
			pr_err("%s", buf2);
		}
		pr_err("---------FW LOG2DRAM ENDS-----------");
	}
error:
	return;
}
