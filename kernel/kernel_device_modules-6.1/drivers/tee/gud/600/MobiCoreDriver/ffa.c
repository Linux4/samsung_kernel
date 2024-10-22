// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021-2022 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/scatterlist.h>
#include <linux/interrupt.h>

#include "mci/mcifc.h"

#include "platform.h"	/* MC_SMC_FASTCALL */
#include "main.h"
#include "ffa.h"
#include "mmu.h"
#include "mmu_internal.h"
#include "public/mc_user.h"

#ifdef MC_FFA_FASTCALL
#include <linux/arm_ffa.h>
#include "public/trustonic_ffa_api.h"

#define KINIBI_FFA_TAG_SHARED		0
#define FFA_MEMORY_RECLAIM_KEEP_MEMORY  0
#define FFA_MEMORY_NO_TIME_SLICE	0

static struct {
	struct ffa_device *dev;
	const struct ffa_ops *ops;
	struct ffa_device_id device_id;
	irq_handler_t global_irq_handler;
} l_ffa_ctx = {
	.dev = NULL,
	.ops = NULL,
	.device_id = {
		UUID_INIT(0x8bf0ccc3, 0x42a3, 0x5271,
			  0x86, 0x0d, 0x66, 0x58, 0x04, 0x69, 0x47, 0x9f) },
};

int ffa_module_probe(void)
{
	if (!l_ffa_ctx.ops)
		return -ENXIO;

	return 0;
}

static int ffa_probe(struct ffa_device *ffa_dev)
{
	int ret = 0;
	const struct ffa_ops *ffa_ops;

	mc_dev_info("init FFA protocol");

	ffa_ops = ffa_dev->ops;
	if (IS_ERR_OR_NULL(ffa_ops)) {
		ret = PTR_ERR(ffa_ops);
		mc_dev_err(ret, "failed to obtain FFA ops");
		return ret;
	}

	l_ffa_ctx.dev = ffa_dev;
	l_ffa_ctx.ops = ffa_ops;

	l_ffa_ctx.ops->msg_ops->mode_32bit_set(ffa_dev);

	return 0;
}

static void ffa_remove(struct ffa_device *ffa_dev)
{
	(void)ffa_dev;
}

#ifdef MC_FFA_NOTIFICATION
static void ffa_notif_callback(ffa_partition_id_t partition_id,
			       ffa_notification_id_t notification_id,
			       void *dev_data)
{
	if (l_ffa_ctx.global_irq_handler)
		l_ffa_ctx.global_irq_handler(notification_id, dev_data);
}
#endif

static inline int ffa_shm_register(struct page **pages, size_t num_pages,
				   struct tee_mmu *mmu, u64 tag)
{
	struct ffa_mem_region_attributes mem_attr = {
		.receiver = l_ffa_ctx.dev->vm_id,
		.attrs = FFA_MEM_RW,
	};
	struct ffa_mem_ops_args args = {
		.use_txbuf = true,
		.attrs = &mem_attr,
		.nattrs = 1,
		.tag = tag,
		.flags = FFA_MEMORY_RECLAIM_KEEP_MEMORY
			 | FFA_MEMORY_NO_TIME_SLICE,
	};
	struct sg_table sgt;
	int rc;
	bool lend = mmu->flags & MC_IO_FFA_LEND;

	rc = sg_alloc_table_from_pages(&sgt, pages, num_pages, 0,
				       num_pages * PAGE_SIZE, GFP_KERNEL);
	if (rc)
		return rc;

	args.sg = sgt.sgl;
	if (lend)
		rc = l_ffa_ctx.ops->mem_ops->memory_lend(&args);
	else
		rc = l_ffa_ctx.ops->mem_ops->memory_share(&args);

	sg_free_table(&sgt);
	if (rc)
		return rc;

	mmu->handle = args.g_handle;

	mc_dev_devel("FFA buffer %s: handle %llx, tag %llx\n",
		     lend ? "lent" : "shared", mmu->handle, tag);

	return 0;
}

static int ffa_share_continuous_buffer(const void *buf,
				       size_t n_cont_pages,
				       u64 *ffa_handle)
{
	struct page	**pages = NULL;  /* Same as below, conveniently typed */
	unsigned long	pages_page = 0;	/* Page to contain the page pointers */
	struct page	*page = NULL;
	struct tee_mmu mmu;
	int		ret = 0;
	int		i = 0;

	memset(&mmu, 0, sizeof(mmu));

	/* Get a page to store page pointers */
	pages_page = get_zeroed_page(GFP_KERNEL);
	if (!pages_page) {
		ret = -ENOMEM;
		goto end;
	}
	pages = (struct page **)pages_page;

	page = virt_to_page(buf);
	for (i = 0; i < n_cont_pages; i++)
		pages[i] = page++;

	ret = ffa_shm_register(pages, n_cont_pages, &mmu,
			       KINIBI_FFA_TAG_SHARED);
	if (ret)
		goto end;

	if (ffa_handle)
		*ffa_handle = mmu.handle;

end:
	if (pages_page)
		free_page(pages_page);

	return ret;
}

#ifdef MC_FFA_NOTIFICATION
int ffa_register_notif_handler(irq_handler_t handler)
{
	int id = 0;

	/* Setup global notification */
	id = l_ffa_ctx.ops->request_notification(l_ffa_ctx.dev, false,
						  ffa_notif_callback, NULL);
	if (id < 0) {
		mc_dev_err(ret, "requesting FFA notification");
		return id;
	}

	l_ffa_ctx.global_irq_handler = handler;
	mc_dev_info("FFA notification handler registered on notification id %x",
		    id);

	return id;
}
#endif

int ffa_share_mci_buffer(const void *mci, size_t n_cont_pages, u64 *ffa_handle)
{
	return ffa_share_continuous_buffer(mci, n_cont_pages, ffa_handle);
}

int ffa_share_trace_buffer(phys_addr_t buffer_phys, u32 size, u64 *ffa_handle)
{
	size_t n_cont_pages = roundup(size / PAGE_SIZE, 1);
	const void *virt_addr = phys_to_virt(buffer_phys);

	return ffa_share_continuous_buffer(virt_addr, n_cont_pages, ffa_handle);
}

int ffa_register_buffer(struct page **pages, struct tee_mmu *mmu, u64 tag)
{
	return ffa_shm_register(pages, mmu->nr_pages, mmu, tag);
}

int ffa_reclaim_buffer(struct tee_mmu *mmu)
{
	return l_ffa_ctx.ops->mem_ops->memory_reclaim(mmu->handle,
					     FFA_MEMORY_RECLAIM_KEEP_MEMORY);
}

/* Wrapper around FFA_MSG_SEND_DIRECT_REQ */
inline int ffa_fastcall(union fc_common *fc)
{
	int ret;
	struct ffa_send_direct_data data = {
		.data0 = fc->in.cmd,
		.data1 = fc->in.param[0],
		.data2 = fc->in.param[1],
		.data3 = fc->in.param[2],
	};

	ret = l_ffa_ctx.ops->msg_ops->sync_send_receive(l_ffa_ctx.dev,
					       &data);
	if (ret)
		return ret;

	/* Copy out */
	fc->out.resp = data.data0;
	fc->out.ret = data.data1;
	fc->out.param[0] = data.data2;
	fc->out.param[1] = data.data3;

	return 0;
}

static struct ffa_driver kinibi_ffa_driver = {
	.name = "kinibi",
	.probe = ffa_probe,
	.remove = ffa_remove,
	.id_table = &l_ffa_ctx.device_id,
};

int ffa_register_module(void)
{
	return ffa_register(&kinibi_ffa_driver);
}

void ffa_unregister_module(void)
{
	ffa_unregister(&kinibi_ffa_driver);
}

// FFA trampoline API

int tt_ffa_memory_lend(struct ffa_mem_ops_args *args)
{
	if (!l_ffa_ctx.dev)
		return -ENODEV;

	if (!args)
		return -EINVAL;

	if (args->nattrs == 0)
		return -EINVAL;

	if (!args->attrs)
		return -EINVAL;

	// Kinibi uses the TX FFA buffer to transmit memory descriptors
	args->use_txbuf = true;
	// Only overwrite the first attribute vm id with Kinibi's one
	args->attrs[0].receiver = l_ffa_ctx.dev->vm_id;

	return l_ffa_ctx.ops->mem_ops->memory_lend(args);
}
EXPORT_SYMBOL(tt_ffa_memory_lend);

int tt_ffa_memory_reclaim(u64 handle, u64 tag)
{
	if (!l_ffa_ctx.dev)
		return -ENODEV;

	return l_ffa_ctx.ops->mem_ops->memory_reclaim(handle, tag);
}
EXPORT_SYMBOL(tt_ffa_memory_reclaim);

#endif /* MC_FFA_FASTCALL */
