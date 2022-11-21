/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_hrvc_worker.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains hw related functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifdef CONFIG_EXYNOS_MFC_HRVC

#include <linux/vmalloc.h>
#include <linux/smc.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <linux/firmware.h>
#include <linux/exynos_ion.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

#include "s5p_mfc_hrvc_worker.h"

#define __4KB__ 0x001000
#define __1MB__ 0x100000

#define SMC_DRM_FW_COPY		0x82000500
#define HRVC_COMMAND_INIT	0x8200F810
#define HRVC_COMMAND_CMD	0x8200F811

#define COPY_SUCCESS		1

typedef unsigned int (*fw_entry_fn)(unsigned int mode, unsigned long int shared_addr,
	unsigned long int sfr_addr);

#define MFC_LIBFW_NAME	"mfc_fw.bin"
#define MFC_LDFW_NAME	"mfc_ldfw.bin"

static struct kthread_worker hrvc_worker;
static struct kthread_work work_normal_cmd;
static struct kthread_work work_secure_cmd;
static struct kthread_work work_normal_int;
static struct kthread_work work_secure_int;
struct task_struct *g_th_id;
struct vm_struct vm_mfc_fw;
EXPORT_SYMBOL(vm_mfc_fw);

struct mfc_memory_attr {
	pgprot_t state;
	int numpages;
	ulong vaddr;
};

struct mfc_memory_change_data {
	pgprot_t set_mask;
	pgprot_t clear_mask;
};

extern struct s5p_mfc_dev *g_mfc_dev[MFC_DEV_NUM_MAX];

static int mfc_memory_page_range(pte_t *ptep, pgtable_t token, unsigned long addr,
					void *data)
{
	struct mfc_memory_change_data *cdata = data;
	pte_t pte = *ptep;

	pte = clear_pte_bit(pte, cdata->clear_mask);
	pte = set_pte_bit(pte, cdata->set_mask);

	set_pte(ptep, pte);

	return 0;
}

static int mfc_memory_attr_change(ulong addr, int numpages,
		pgprot_t set_mask, pgprot_t clear_mask)
{
	ulong start = addr;
	ulong size = PAGE_SIZE * numpages;
	ulong end = start + size;
	int ret;
	struct mfc_memory_change_data data;

	if (!IS_ALIGNED(addr, PAGE_SIZE)) {
		start &= PAGE_MASK;
		end = start + size;
		WARN_ON_ONCE(1);
	}

	data.set_mask = set_mask;
	data.clear_mask = clear_mask;

	ret = apply_to_page_range(&init_mm, start, size, mfc_memory_page_range,
					&data);
	flush_tlb_kernel_range(start, end);

	return ret;
}

int mfc_memory_attr_nxrw(struct mfc_memory_attr *attribute)
{
	int ret;

	if (attribute->state != PTE_RDONLY) {
		mfc_err("memory attribute state is wrong\n");
		return -EINVAL;
	}

	ret = mfc_memory_attr_change(attribute->vaddr,
				attribute->numpages,
				__pgprot(PTE_PXN), __pgprot(0));
	if (ret) {
		mfc_err("memory attribute change fail [PTE_PX -> PTE_PXN]\n");
		return ret;
	}

	ret = mfc_memory_attr_change(attribute->vaddr,
				attribute->numpages,
				__pgprot(PTE_WRITE),
				__pgprot(PTE_RDONLY));
	if (ret) {
		mfc_err("memory attribute change fail [PTE_RDONLY -> PTE_WRITE]\n");
		goto memory_excutable;
	}

	attribute->state = PTE_WRITE;

	return 0;

memory_excutable:
	ret = mfc_memory_attr_change(attribute->vaddr,
			attribute->numpages,
			__pgprot(0),
			__pgprot(PTE_PXN));

	return ret;
}

int mfc_memory_attr_rox(struct mfc_memory_attr *attribute)
{
	int ret;

	if (attribute->state != PTE_WRITE) {
		mfc_err("memory attribute state wrong\n");
		return -EINVAL;
	}

	ret = mfc_memory_attr_change(attribute->vaddr,
			attribute->numpages,
			__pgprot(PTE_RDONLY),
			__pgprot(PTE_WRITE));
	if (ret) {
		mfc_err("memory attribute change fail [PTE_WRITE -> PTE_RDONLY]\n");
		return ret;
	}

	ret = mfc_memory_attr_change(attribute->vaddr,
			attribute->numpages,
			__pgprot(0),
			__pgprot(PTE_PXN));
	if (ret) {
		mfc_err("memory attribute change fail [PTE_PXN -> PTE_PX]\n");
		return ret;
	}

	attribute->state = PTE_RDONLY;

	return 0;
}

int hrvc_load_libfw(void)
{
	int ret = 0;
	struct firmware *fw_blob;
	struct s5p_mfc_dev *dev = g_mfc_dev[0];
	struct mfc_memory_attr va_mfc_memory_attr = {
		PTE_RDONLY, PFN_UP(LIB_CODE_SIZE), (ulong)vm_mfc_fw.addr};

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	ret = mfc_memory_attr_nxrw(&va_mfc_memory_attr);
	if (ret) {
		mfc_err("failed to change into NX memory attribute (%d)\n", ret);
		return ret;
	}

	ret = request_firmware((const struct firmware **)&fw_blob,
			MFC_LIBFW_NAME, dev->v4l2_dev.dev);

	if (ret != 0) {
		mfc_err("LIB Firmware is not present in the /lib/firmware directory"\
			" nor compiled in kernel.\n");
		return -EINVAL;
	}
	mfc_debug(2, "request_firmware() is succeeded. fw_blob->size = %zu\n",
			fw_blob->size);

	if (fw_blob->size > PAGE_ALIGN(__1MB__)) {
		mfc_err_dev("MFC firmware lib. is too big to be loaded.\n");
		release_firmware(fw_blob);
		return -ENOMEM;
	}

	mfc_debug(2, "FW_VA=0x%lx, fw_blob->data = 0x%lx\n",
			(unsigned long int)vm_mfc_fw.addr,
			(unsigned long int)fw_blob->data);

	dev->fw_size = fw_blob->size;
	memcpy(vm_mfc_fw.addr, fw_blob->data, fw_blob->size);

	release_firmware(fw_blob);

	ret = mfc_memory_attr_rox(&va_mfc_memory_attr);
	if (ret) {
		mfc_err("failed to change into EX memory attribute (%d)\n", ret);
		return ret;
	}

	mfc_debug_leave();

	return ret;
}

void hrvc_libfw_on(void)
{
	struct s5p_mfc_dev *dev = g_mfc_dev[0];

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mfc_debug(2, "mfc_dev->vptr_regs_base = 0x%lx, mfc_dev->sfr_base = 0x%lx\n",
			(unsigned long int)dev->vptr_regs_base, (unsigned long int)dev->sfr_base);

	((fw_entry_fn)vm_mfc_fw.addr)((unsigned int)HRVC_COMMAND_INIT,
		(unsigned long int)dev->vptr_regs_base, (unsigned long int)dev->sfr_base);

	mfc_debug_leave();
}

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
int hrvc_load_ldfw(void)
{
	static int ret = 0;
	struct firmware *fw_blob;
	struct s5p_mfc_dev *dev = g_mfc_dev[0];
	char *virt_base, *virt_end;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (dev->drm_fw_status > 0) {
		mfc_info_dev("secure fw was loaded\n");
		return 0;
	}

	ret = request_firmware((const struct firmware **)&fw_blob,
		MFC_LDFW_NAME, dev->v4l2_dev.dev);

	if (ret != 0) {
		mfc_err("LD Firmware is not present in the /lib/firmware directory"\
			" nor compiled in kernel.\n");
		return -EINVAL;
	}

	mfc_debug(2, "request_firmware() is succeeded. fw_blob->size = %zu\n",
			fw_blob->size);

	if (fw_blob->size > PAGE_ALIGN(__1MB__)) {
		mfc_err_dev("MFC LDFW lib. is too big to be loaded.\n");
		release_firmware(fw_blob);
		return -ENOMEM;
	}

	/*
	 * vm_mfc_fw.addr is temporarily used for storing the ld firmware
	 * before the firmware is copied to secure world.
	 * Therefore, this operation should be done before the lib firmware is loaded.
	 * Otherwise the lib firmware is overwritten by the ld firmware.
	 */
	virt_base = (char *)vm_mfc_fw.addr;
	virt_end = virt_base + fw_blob->size;
	dev->fw_size = fw_blob->size;

	memcpy(virt_base, fw_blob->data, fw_blob->size);

	mfc_debug(2, "FW_VA=0x%lx, fw_blob->data = 0x%lx\n",
			(unsigned long int)virt_base,
			(unsigned long int)fw_blob->data);


	__dma_flush_range((void *)virt_base, (void *)virt_end);
	dev->drm_fw_status = exynos_smc(SMC_DRM_FW_COPY, vm_mfc_fw.phys_addr, fw_blob->size, 0);
	if (dev->drm_fw_status != COPY_SUCCESS) {
		mfc_err("FAIL: exynos_smc() : ret = 0x%08x virt 0x%lx phy 0x%lx 0x%x\n",
				ret, (unsigned long)virt_base,
				(unsigned long)vm_mfc_fw.phys_addr, (int)fw_blob->size);
		ret = -EPERM;
	}

	release_firmware(fw_blob);

	mfc_debug_leave();

	return ret;
}

int hrvc_ldfw_on(void)
{
	unsigned int ret = 0;
	unsigned long int addr;
	struct s5p_mfc_dev *dev = g_mfc_dev[0];

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	addr = (unsigned long int)dev->pptr_regs_base;
	__dma_flush_range(dev->vptr_regs_base, dev->vptr_regs_base + __4KB__);

	mfc_debug(2, "mfc_dev->pptr_regs_base = 0x%lx, mfc_dev->mfc_mem->start = 0x%x\n",
			(unsigned long int)dev->pptr_regs_base, (unsigned int)dev->mfc_mem->start);

	ret = exynos_smc(HRVC_COMMAND_INIT, (unsigned int)(addr >> 32),
		(unsigned int)(addr & 0xFFFFFFFF), (unsigned int)dev->mfc_mem->start);
	if (ret != DRMDRV_OK) {
		mfc_err("FAIL: exynos_smc() : ret = 0x%08x\n", ret);
		return -EINVAL;
	}

	mfc_debug_leave();

	return ret;
}
#endif

static void worker_normal_cmd_func(struct kthread_work *work)
{
	unsigned int ret;
	unsigned int reason;
	unsigned int result;
	struct s5p_mfc_dev *dev = g_mfc_dev[0];
	struct s5p_mfc_ctx *ctx;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mfc_debug_enter();

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	mfc_debug(2, "[c:%d] Before calling mfc_fw_lib_entry(HRVC_COMMAND_CMD)\n",
			dev->curr_ctx);
	dev->cmd_status = 1;
	((fw_entry_fn)vm_mfc_fw.addr)((unsigned int)HRVC_COMMAND_CMD,
		(unsigned long int)0, (unsigned long int)0);

	result = MFC_READL(S5P_FIMV_RISC2HOST_CMD);
	mfc_debug(2, "[c:%d] After calling mfc_fw_lib_entry(HRVC_COMMAND_CMD) result = %u\n",
			dev->curr_ctx, result);

	if (result != S5P_FIMV_R2H_CMD_RET_HW_START) {
		reason = s5p_mfc_get_int_reason();
		ret = s5p_mfc_get_int_err();
		mfc_debug(2, "[c:%d] Int reason: %d (err: %d)\n", dev->curr_ctx, reason, ret);

		mfc_debug(2, "[c:%d] Before calling s5p_mfc_irq()\n", dev->curr_ctx);
		s5p_mfc_irq((int)0, (void *)dev);
		mfc_debug(2, "[c:%d] After calling s5p_mfc_irq()\n", dev->curr_ctx);
	}
	dev->cmd_status = 0;

	mfc_debug_leave();
}

static void worker_secure_cmd_func(struct kthread_work *work)
{
	unsigned int ret;
	unsigned int reason;
	unsigned int result;
	struct s5p_mfc_dev *dev = g_mfc_dev[0];
	struct s5p_mfc_ctx *ctx;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mfc_debug_enter();

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	__dma_flush_range(dev->vptr_regs_base, dev->vptr_regs_base + __4KB__);
	mfc_debug(2, "[c:%d] Before calling exynos_smc(HRVC_COMMAND_CMD)\n", dev->curr_ctx);

	ret = exynos_smc(HRVC_COMMAND_CMD, 0, 0, 0);
	if (ret != DRMDRV_OK) {
		mfc_err("FAIL: exynos_smc() : ret = 0x%08x\n", ret);
		return;
	}

	result = MFC_READL(S5P_FIMV_RISC2HOST_CMD);
	mfc_debug(2, "[c:%d] After calling exynos_smc(HRVC_COMMAND_CMD) result = %u\n",
			dev->curr_ctx, result);

	if (result != S5P_FIMV_R2H_CMD_RET_HW_START) {
		reason = s5p_mfc_get_int_reason();
		ret = s5p_mfc_get_int_err();
		mfc_debug(2, "[c:%d] Int reason: %d (err: %d)\n", dev->curr_ctx, reason, ret);

		mfc_debug(2, "[c:%d] Before calling s5p_mfc_irq()\n", dev->curr_ctx);
		s5p_mfc_irq((int)0, (void *)dev);
		mfc_debug(2, "[c:%d] After calling s5p_mfc_irq()\n", dev->curr_ctx);
	}

	mfc_debug_leave();
}

static void worker_normal_int_func(struct kthread_work *work)
{
	unsigned int ret;
	unsigned int reason;
	unsigned int result;
	struct s5p_mfc_dev *dev = g_mfc_dev[0];
	struct s5p_mfc_ctx *ctx;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mfc_debug_enter();

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	MFC_WRITEL(S5P_FIMV_CH_HW_INT, S5P_FIMV_HOST2RISC_CMD);

	mfc_debug(2, "[c:%d] Before calling mfc_fw_lib_entry(HRVC_COMMAND_CMD)\n", dev->curr_ctx);
	((fw_entry_fn)vm_mfc_fw.addr)((unsigned int)HRVC_COMMAND_CMD,
		(unsigned long int)0, (unsigned long int)0);

	result = MFC_READL(S5P_FIMV_RISC2HOST_CMD);
	mfc_debug(2, "[c:%d] After calling mfc_fw_lib_entry(HRVC_COMMAND_CMD) result = %u\n",
			dev->curr_ctx, result);

	if (result != S5P_FIMV_R2H_CMD_RET_HW_START) {
		reason = s5p_mfc_get_int_reason();
		ret = s5p_mfc_get_int_err();
		mfc_debug(2, "[c:%d] Int reason: %d (err: %d)\n", dev->curr_ctx, reason, ret);

		mfc_debug(2, "[c:%d] Before calling s5p_mfc_irq()\n", dev->curr_ctx);
		s5p_mfc_irq((int)0, (void *)dev);
		mfc_debug(2, "[c:%d] After calling s5p_mfc_irq()\n", dev->curr_ctx);
	}

	mfc_debug_leave();
}

static void worker_secure_int_func(struct kthread_work *work)
{
	unsigned int ret;
	unsigned int reason;
	unsigned int result;
	struct s5p_mfc_dev *dev = g_mfc_dev[0];
	struct s5p_mfc_ctx *ctx;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mfc_debug_enter();

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	MFC_WRITEL(S5P_FIMV_CH_HW_INT, S5P_FIMV_HOST2RISC_CMD);

	__dma_flush_range(dev->vptr_regs_base, dev->vptr_regs_base + __4KB__);
	mfc_debug(2, "[c:%d] Before calling exynos_smc(HRVC_COMMAND_CMD)\n", dev->curr_ctx);

	ret = exynos_smc(HRVC_COMMAND_CMD, 0, 0, 0);
	if (ret != DRMDRV_OK) {
		mfc_err("FAIL: exynos_smc() : ret = 0x%08x\n", ret);
		return;
	}

	result = MFC_READL(S5P_FIMV_RISC2HOST_CMD);
	mfc_debug(2, "[c:%d] After calling exynos_smc(HRVC_COMMAND_CMD) result = %u\n",
			dev->curr_ctx, result);

	if (result != S5P_FIMV_R2H_CMD_RET_HW_START) {
		reason = s5p_mfc_get_int_reason();
		ret = s5p_mfc_get_int_err();
		mfc_debug(2, "[c:%d] Int reason: %d (err: %d)\n", dev->curr_ctx, reason, ret);

		mfc_debug(2, "[c:%d] Before calling s5p_mfc_irq()\n", dev->curr_ctx);
		s5p_mfc_irq((int)0, (void *)dev);
		mfc_debug(2, "[c:%d] After calling s5p_mfc_irq()\n", dev->curr_ctx);
	}

	mfc_debug_leave();
}

void hrvc_worker_queue(int mode, int is_drm)
{
	mfc_debug_enter();

	mfc_debug(2, "%s : %s\n", (HRVC_CMD == mode) ? "HRVC_CMD" : "HRVC_INT",
					is_drm ? "secure" : "normal");
	switch (mode) {
	case HRVC_CMD:
		if (is_drm)
			queue_kthread_work(&hrvc_worker, &work_secure_cmd);
		else
			queue_kthread_work(&hrvc_worker, &work_normal_cmd);
		break;
	case HRVC_INT:
		if (is_drm)
			queue_kthread_work(&hrvc_worker, &work_secure_int);
		else
			queue_kthread_work(&hrvc_worker, &work_normal_int);
		break;
	}

	mfc_debug_leave();
}

#define MAX_TRIAL_KTHREAD_CREATION 5

int hrvc_worker_initialize(void)
{
	int ret = 0;
	int i;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	mfc_debug_enter();

	if (g_th_id == NULL) {
		init_kthread_worker(&hrvc_worker);

		for (i = 0; i < MAX_TRIAL_KTHREAD_CREATION; i++) {
			g_th_id = (struct task_struct *)kthread_run(kthread_worker_fn,
				&hrvc_worker, "hrvc_worker");
			if (!IS_ERR(g_th_id))
				break;
			mfc_err("kthread_run returns error\n");
		}

		if (IS_ERR(g_th_id)) {
			mfc_err("Fail to create kthread for HRVC\n");
			mfc_debug_leave();
			return -1;
		}

		init_kthread_work(&work_normal_cmd, worker_normal_cmd_func);
		init_kthread_work(&work_secure_cmd, worker_secure_cmd_func);
		init_kthread_work(&work_normal_int, worker_normal_int_func);
		init_kthread_work(&work_secure_int, worker_secure_int_func);
		ret = sched_setscheduler_nocheck(g_th_id, SCHED_FIFO, &param);
		if (ret)
			mfc_err("sched_setscheduler_nocheck is fail(%d)", ret);
	}

	mfc_debug_leave();
	return ret;
}

void hrvc_worker_finalize(void)
{
	mfc_debug_enter();

	if (g_th_id) {
		flush_kthread_worker(&hrvc_worker);
		kthread_stop(g_th_id);
		g_th_id = NULL;
	}

	mfc_debug_leave();
}

static int __init mfc_reserved_mem_setup(struct reserved_mem *rmem)
{
	const __be32 *fixed_upper_addr, *fixed_lower_addr;
	unsigned long fixed_va = 0;
	unsigned int temp;
	int len = 0;

	fixed_upper_addr = of_get_flat_dt_prop(rmem->fdt_node, "fixed_upper_base", &len);
	fixed_lower_addr = of_get_flat_dt_prop(rmem->fdt_node, "fixed_lower_base", &len);
	if (!fixed_upper_addr || !fixed_lower_addr) {
		mfc_err("fixed addr is NULL\n");
		return 1;
	}

	temp = be32_to_cpu(*fixed_upper_addr);
	fixed_va = (unsigned long)temp << 32;

	temp = be32_to_cpu(*fixed_lower_addr);
	fixed_va |= (unsigned long)temp;

	vm_mfc_fw.addr = (void*)fixed_va;
	vm_mfc_fw.phys_addr = rmem->base;
	vm_mfc_fw.size = rmem->size + PAGE_SIZE;
	vm_area_add_early(&vm_mfc_fw);
	pr_info("Fixed region for MFC FW: %#lx:%#lx\n",
			(unsigned long)vm_mfc_fw.addr,
			(unsigned long)vm_mfc_fw.size);
	return 0;
}
RESERVEDMEM_OF_DECLARE(vnfw, "exynos7570-mfc,vnfw", mfc_reserved_mem_setup);

#endif
