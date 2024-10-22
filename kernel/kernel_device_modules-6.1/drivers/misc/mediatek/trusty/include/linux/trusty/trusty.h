/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __LINUX_TRUSTY_TRUSTY_H
#define __LINUX_TRUSTY_TRUSTY_H

#include <linux/kernel.h>
#include <linux/trusty/sm_err.h>
#include <linux/device.h>
#include <linux/pagemap.h>

#define VIRTIO_ID_TRUSTY_IPC	14 /* virtio trusty ipc */

#if IS_ENABLED(CONFIG_TRUSTY)
s32 ise_std_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2);
s32 ise_fast_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2);
#if IS_ENABLED(CONFIG_64BIT)
s64 trusty_fast_call64(struct device *dev, u64 smcnr, u64 a0, u64 a1, u64 a2);
#endif
#else
static inline s32 ise_std_call32(struct device *dev, u32 smcnr,
				    u32 a0, u32 a1, u32 a2)
{
	return SM_ERR_UNDEFINED_SMC;
}
static inline s32 ise_fast_call32(struct device *dev, u32 smcnr,
				     u32 a0, u32 a1, u32 a2)
{
	return SM_ERR_UNDEFINED_SMC;
}
#if IS_ENABLED(CONFIG_64BIT)
static inline s64 trusty_fast_call64(struct device *dev,
				     u64 smcnr, u64 a0, u64 a1, u64 a2)
{
	return SM_ERR_UNDEFINED_SMC;
}
#endif
#endif

struct notifier_block;
enum {
	TRUSTY_CALL_PREPARE,
	TRUSTY_CALL_RETURNED,
};
int ise_call_notifier_register(struct device *dev,
				  struct notifier_block *n);
int ise_call_notifier_unregister(struct device *dev,
				    struct notifier_block *n);
int ise_notifier_register(struct device *dev,
				  struct notifier_block *n);
int ise_notifier_unregister(struct device *dev,
				    struct notifier_block *n);
const char *ise_version_str_get(struct device *dev);
u32 ise_get_api_version(struct device *dev);

struct ns_mem_page_info {
	uint64_t attr;
};

int trusty_encode_page_info(struct ns_mem_page_info *inf,
			    struct page *page, pgprot_t pgprot);

int trusty_call32_mem_buf(struct device *dev, u32 smcnr,
			  struct page *page,  u32 size,
			  pgprot_t pgprot);

struct trusty_nop {
	struct list_head node;
	u32 args[3];
};

static inline void trusty_nop_init(struct trusty_nop *nop,
				   u32 arg0, u32 arg1, u32 arg2) {
	INIT_LIST_HEAD(&nop->node);
	nop->args[0] = arg0;
	nop->args[1] = arg1;
	nop->args[2] = arg2;
}

void ise_enqueue_nop(struct device *dev, struct trusty_nop *nop);
void ise_dequeue_nop(struct device *dev, struct trusty_nop *nop);

void trusty_notifier_call(void);
void ise_notifier_call(void);

u32 is_trusty_real_driver(void);

#endif
