// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <asm/compiler.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/sm_err.h>
#include <linux/trusty/trusty.h>
#include <linux/trusty/trusty_shm.h>
#include <linux/soc/mediatek/mtk-ise-mbox.h>
#include <linux/soc/mediatek/mtk_ise.h>
#include <linux/soc/mediatek/mtk_ise_lpm.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

struct trusty_state;

struct trusty_work {
	struct trusty_state *ts;
	struct work_struct work;
};

struct trusty_state {
	struct mutex smc_lock;
	struct atomic_notifier_head notifier;
	struct atomic_notifier_head ise_notifier;
	struct completion cpu_idle_completion;
	char *version_str;
	u32 api_version;
	struct device *dev;
	struct workqueue_struct *nop_wq;
	struct trusty_work __percpu *nop_works;
	struct list_head nop_queue;
	spinlock_t nop_lock; /* protects nop_queue */
};

struct trusty_state *tstate;
static struct workqueue_struct *notif_call_wq;
static struct work_struct notif_call_work;
static struct workqueue_struct *ise_notif_call_wq;
static struct work_struct ise_notif_call_work;
static u32 real_drv;
static phys_addr_t mcia_paddr;
static size_t mcia_size;

enum ise_type {
	ISE_TYPE_SHM = 0,
	ISE_TYPE_MCIA,
	ISE_TYPE_NUM,
};

#if IS_ENABLED(CONFIG_ARM64)
#define SMC_ARG0		"x0"
#define SMC_ARG1		"x1"
#define SMC_ARG2		"x2"
#define SMC_ARG3		"x3"
#define SMC_ARCH_EXTENSION	""
#define SMC_REGISTERS_TRASHED	"x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", \
				"x12", "x13", "x14", "x15", "x16", "x17"
#else
#define SMC_ARG0		"r0"
#define SMC_ARG1		"r1"
#define SMC_ARG2		"r2"
#define SMC_ARG3		"r3"
#define SMC_ARCH_EXTENSION	".arch_extension sec\n"
#define SMC_REGISTERS_TRASHED	"ip"
#endif

static inline ulong smc(ulong r0, ulong r1, ulong r2, ulong r3)
{
	mailbox_request_t request = {0};
	mailbox_payload_t payload = {0};
	mailbox_reply_t reply = {0};

	if (mtk_ise_awake_lock(ISE_REE)) {
		pr_info("%s: ise power on failed\n", __func__);
		goto out;
	}

	payload.size = 4;
	payload.fields[0] = r0;
	payload.fields[1] = r1;
	payload.fields[2] = r2;
	payload.fields[3] = r3;

	reply = ise_mailbox_request(&request, &payload, REQUEST_TRUSTY,
		TRUSTY_SR_ID_SMC, TRUSTY_SR_VER_SMC);
	if (reply.status.error != MAILBOX_SUCCESS)
		pr_info("%s: request mailbox failed 0x%x\n", __func__, reply.status.error);

	if (mtk_ise_awake_unlock(ISE_REE))
		pr_info("%s: ise power off failed\n", __func__);

out:
	return reply.payload.fields[0];
}

s32 ise_fast_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	BUG_ON(!s);
	BUG_ON(!SMC_IS_FASTCALL(smcnr));
	BUG_ON(SMC_IS_SMC64(smcnr));

	return smc(smcnr, a0, a1, a2);
}
EXPORT_SYMBOL(ise_fast_call32);

#if IS_ENABLED(CONFIG_64BIT)
s64 trusty_fast_call64(struct device *dev, u64 smcnr, u64 a0, u64 a1, u64 a2)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	BUG_ON(!s);
	BUG_ON(!SMC_IS_FASTCALL(smcnr));
	BUG_ON(!SMC_IS_SMC64(smcnr));

	return smc(smcnr, a0, a1, a2);
}
#endif

#if !IS_ENABLED(CONFIG_TRUSTY)
static ulong trusty_std_call_inner(struct device *dev, ulong smcnr,
				   ulong a0, ulong a1, ulong a2)
{
	ulong ret;
	int retry = 5;

	dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx)\n",
		__func__, smcnr, a0, a1, a2);
	while (true) {
		ret = smc(smcnr, a0, a1, a2);
		while ((s32)ret == SM_ERR_FIQ_INTERRUPTED)
			ret = smc(SMC_SC_RESTART_FIQ, 0, 0, 0);
		if ((int)ret != SM_ERR_BUSY || !retry)
			break;

		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy, retry\n",
			__func__, smcnr, a0, a1, a2);
		retry--;
	}

	return ret;
}

static ulong trusty_std_call_helper(struct device *dev, ulong smcnr,
				    ulong a0, ulong a1, ulong a2)
{
	ulong ret;
	int sleep_time = 1;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	while (true) {
		local_irq_disable();
		atomic_notifier_call_chain(&s->notifier, TRUSTY_CALL_PREPARE,
					   NULL);
		ret = trusty_std_call_inner(dev, smcnr, a0, a1, a2);
		atomic_notifier_call_chain(&s->notifier, TRUSTY_CALL_RETURNED,
					   NULL);
		local_irq_enable();

		if ((int)ret != SM_ERR_BUSY)
			break;

		if (sleep_time == 256)
			dev_warn(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy\n",
				 __func__, smcnr, a0, a1, a2);
		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) returned busy, wait %d ms\n",
			__func__, smcnr, a0, a1, a2, sleep_time);

		msleep(sleep_time);
		if (sleep_time < 1000)
			sleep_time <<= 1;

		dev_dbg(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) retry\n",
			__func__, smcnr, a0, a1, a2);
	}

	if (sleep_time > 256)
		dev_warn(dev, "%s(0x%lx 0x%lx 0x%lx 0x%lx) busy cleared\n",
			 __func__, smcnr, a0, a1, a2);

	return ret;
}

static void trusty_std_call_cpu_idle(struct trusty_state *s)
{
	int ret;

	ret = wait_for_completion_timeout(&s->cpu_idle_completion, HZ * 10);
	if (!ret) {
		pr_warn("%s: timed out waiting for cpu idle to clear, retry anyway\n",
			__func__);
	}
}
#endif

s32 ise_std_call32(struct device *dev, u32 smcnr, u32 a0, u32 a1, u32 a2)
{
	int ret;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	BUG_ON(SMC_IS_FASTCALL(smcnr));
	BUG_ON(SMC_IS_SMC64(smcnr));

#if IS_ENABLED(CONFIG_TRUSTY)
	ret = smc(smcnr, a0, a1, a2);
	atomic_notifier_call_chain(&s->notifier, TRUSTY_CALL_RETURNED, NULL);
	return ret;
#else
	if (smcnr != SMC_SC_NOP) {
		mutex_lock(&s->smc_lock);
		reinit_completion(&s->cpu_idle_completion);
	}

	dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) started\n",
		__func__, smcnr, a0, a1, a2);

	ret = trusty_std_call_helper(dev, smcnr, a0, a1, a2);
	while (ret == SM_ERR_INTERRUPTED || ret == SM_ERR_CPU_IDLE) {
		dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) interrupted\n",
			__func__, smcnr, a0, a1, a2);
		if (ret == SM_ERR_CPU_IDLE)
			trusty_std_call_cpu_idle(s);
		ret = trusty_std_call_helper(dev, SMC_SC_RESTART_LAST, 0, 0, 0);
	}
	dev_dbg(dev, "%s(0x%x 0x%x 0x%x 0x%x) returned 0x%x\n",
		__func__, smcnr, a0, a1, a2, ret);

	WARN_ONCE(ret == SM_ERR_PANIC, "trusty crashed");

	if (smcnr == SMC_SC_NOP)
		complete(&s->cpu_idle_completion);
	else
		mutex_unlock(&s->smc_lock);

	return ret;
#endif
}
EXPORT_SYMBOL(ise_std_call32);

int ise_call_notifier_register(struct device *dev, struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_register(&s->notifier, n);
}
EXPORT_SYMBOL(ise_call_notifier_register);

int ise_call_notifier_unregister(struct device *dev,
				    struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_unregister(&s->notifier, n);
}
EXPORT_SYMBOL(ise_call_notifier_unregister);

int ise_notifier_register(struct device *dev, struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_register(&s->ise_notifier, n);
}
EXPORT_SYMBOL(ise_notifier_register);

int ise_notifier_unregister(struct device *dev,
				    struct notifier_block *n)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return atomic_notifier_chain_unregister(&s->ise_notifier, n);
}
EXPORT_SYMBOL(ise_notifier_unregister);

static int trusty_remove_child(struct device *dev, void *data)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

ssize_t trusty_version_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return scnprintf(buf, PAGE_SIZE, "%s\n", s->version_str);
}

DEVICE_ATTR(trusty_version, S_IRUSR, trusty_version_show, NULL);

const char *ise_version_str_get(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return s->version_str;
}
EXPORT_SYMBOL(ise_version_str_get);

static void trusty_init_version(struct trusty_state *s, struct device *dev)
{
	int ret;
	int i;
	int version_str_len;

	ret = ise_fast_call32(dev, SMC_FC_GET_VERSION_STR, -1, 0, 0);
	if (ret <= 0)
		goto err_get_size;

	version_str_len = ret;

	s->version_str = kmalloc(version_str_len + 1, GFP_KERNEL);
	if (!s->version_str)
		goto err_get_size;
	for (i = 0; i < version_str_len; i++) {
		ret = ise_fast_call32(dev, SMC_FC_GET_VERSION_STR, i, 0, 0);
		if (ret < 0)
			goto err_get_char;
		s->version_str[i] = ret;
	}
	s->version_str[i] = '\0';

	dev_info(dev, "trusty version: %s\n", s->version_str);

	ret = device_create_file(dev, &dev_attr_trusty_version);
	if (ret)
		goto err_create_file;
	return;

err_create_file:
err_get_char:
	kfree(s->version_str);
	s->version_str = NULL;
err_get_size:
	dev_err(dev, "failed to get version: %d\n", ret);
}

u32 ise_get_api_version(struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return s->api_version;
}
EXPORT_SYMBOL(ise_get_api_version);

static int trusty_init_api_version(struct trusty_state *s, struct device *dev)
{
	u32 api_version;
	api_version = ise_fast_call32(dev, SMC_FC_API_VERSION,
					 TRUSTY_API_VERSION_CURRENT, 0, 0);
	if (api_version == SM_ERR_UNDEFINED_SMC)
		api_version = 0;

	if (api_version > TRUSTY_API_VERSION_CURRENT) {
		dev_err(dev, "unsupported api version %u > %u\n",
			api_version, TRUSTY_API_VERSION_CURRENT);
		return -EINVAL;
	}

	dev_info(dev, "selected api version: %u (requested %u)\n",
		 api_version, TRUSTY_API_VERSION_CURRENT);
	s->api_version = api_version;

	return 0;
}

static bool dequeue_nop(struct trusty_state *s, u32 *args)
{
	unsigned long flags;
	struct trusty_nop *nop = NULL;

	spin_lock_irqsave(&s->nop_lock, flags);
	if (!list_empty(&s->nop_queue)) {
		nop = list_first_entry(&s->nop_queue,
				       struct trusty_nop, node);
		list_del_init(&nop->node);
		args[0] = nop->args[0];
		args[1] = nop->args[1];
		args[2] = nop->args[2];
	} else {
		args[0] = 0;
		args[1] = 0;
		args[2] = 0;
	}
	spin_unlock_irqrestore(&s->nop_lock, flags);
	return nop;
}

static void locked_nop_work_func(struct work_struct *work)
{
	int ret;
	struct trusty_work *tw = container_of(work, struct trusty_work, work);
	struct trusty_state *s = tw->ts;

	dev_dbg(s->dev, "%s\n", __func__);

	ret = ise_std_call32(s->dev, SMC_SC_LOCKED_NOP, 0, 0, 0);
	if (ret != 0)
		dev_err(s->dev, "%s: SMC_SC_LOCKED_NOP failed %d",
			__func__, ret);

	dev_dbg(s->dev, "%s: done\n", __func__);
}

static void nop_work_func(struct work_struct *work)
{
	int ret;
	bool next;
	u32 args[3];
	struct trusty_work *tw = container_of(work, struct trusty_work, work);
	struct trusty_state *s = tw->ts;

	dev_dbg(s->dev, "%s:\n", __func__);

	dequeue_nop(s, args);
	do {
		dev_dbg(s->dev, "%s: %x %x %x\n",
			__func__, args[0], args[1], args[2]);

		ret = ise_std_call32(s->dev, SMC_SC_NOP,
					args[0], args[1], args[2]);

		next = dequeue_nop(s, args);

		if (ret == SM_ERR_NOP_INTERRUPTED)
			next = true;
		else if (ret != SM_ERR_NOP_DONE)
			dev_dbg(s->dev, "%s: SMC_SC_NOP ret %d",
				__func__, ret);
	} while (next);

	dev_dbg(s->dev, "%s: done\n", __func__);
}

void ise_enqueue_nop(struct device *dev, struct trusty_nop *nop)
{
	unsigned long flags;
	struct trusty_work *tw;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	preempt_disable();
	tw = this_cpu_ptr(s->nop_works);
	if (nop) {
		WARN_ON(s->api_version < TRUSTY_API_VERSION_SMP_NOP);

		spin_lock_irqsave(&s->nop_lock, flags);
		if (list_empty(&nop->node))
			list_add_tail(&nop->node, &s->nop_queue);
		spin_unlock_irqrestore(&s->nop_lock, flags);
	}
	queue_work(s->nop_wq, &tw->work);
	preempt_enable();
}
EXPORT_SYMBOL(ise_enqueue_nop);

void ise_dequeue_nop(struct device *dev, struct trusty_nop *nop)
{
	unsigned long flags;
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	if (WARN_ON(!nop))
		return;

	spin_lock_irqsave(&s->nop_lock, flags);
	if (!list_empty(&nop->node))
		list_del_init(&nop->node);
	spin_unlock_irqrestore(&s->nop_lock, flags);
}
EXPORT_SYMBOL(ise_dequeue_nop);

static int trusty_init(struct device *dev)
{
	return ise_fast_call32(dev, SMC_FC_PA_INFO,
			(u32)gshm.paddr,
			(u32)(gshm.paddr >> 32),
			0);
}

static int trusty_mcia_init(struct device *dev)
{
	return ise_fast_call32(dev, SMC_FC_MCIA_PA_INFO,
			(u32)mcia_paddr,
			(u32)(mcia_paddr >> 32),
			(u32)mcia_size);
}

static void notif_call_work_func(struct work_struct *work)
{
	atomic_notifier_call_chain(&tstate->notifier, TRUSTY_CALL_RETURNED, NULL);
}

void trusty_notifier_call(void)
{
	queue_work(notif_call_wq, &notif_call_work);
}
EXPORT_SYMBOL(trusty_notifier_call);

static void ise_notif_call_work_func(struct work_struct *work)
{
	atomic_notifier_call_chain(&tstate->ise_notifier, TRUSTY_CALL_RETURNED, NULL);
}

void ise_notifier_call(void)
{
	queue_work(ise_notif_call_wq, &ise_notif_call_work);
}
EXPORT_SYMBOL(ise_notifier_call);

u32 is_trusty_real_driver(void)
{
	return real_drv;
}
EXPORT_SYMBOL(is_trusty_real_driver);

static int trusty_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int cpu;
	work_func_t work_func;
	struct trusty_state *s;
	struct device_node *node = pdev->dev.of_node;
	struct arm_smccc_res res;

	if (!node) {
		dev_err(&pdev->dev, "of_node required\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "mediatek,real-drv", &real_drv);
	if (ret || !real_drv) {
		dev_info(&pdev->dev, "ise trusty dummy driver\n");
		return 0;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		ret = -ENOMEM;
		goto err_allocate_state;
	}

	arm_smccc_smc(MTK_SIP_KERNEL_ISE_CONTROL,
		ISE_MODULE_TRUSTY, ISE_TYPE_SHM, 0, 0, 0, 0, 0, &res);
	if (res.a0) {
		dev_err(&pdev->dev, "Failed to get shm memory region: 0x%lx\n", res.a0);
		ret = -EINVAL;
		goto err_mem;
	}

	gshm.paddr = res.a1;
	gshm.mem_size = res.a2;
	/* PROT_NORMAL_NC, normal uncached memory */
	gshm.vaddr = ioremap_wc(gshm.paddr, gshm.mem_size);
	if (IS_ERR(gshm.vaddr)) {
		dev_err(&pdev->dev, "Failed to map shm memory region: 0x%lx\n", res.a1);
		ret = -EINVAL;
		goto err_mem;
	}
	gshm.ioremap_paddr = page_to_phys(virt_to_page(gshm.vaddr));
	dev_dbg(&pdev->dev, "pa 0x%llx, ioremap_pa 0x%llx, size 0x%zx, va 0x%lx\n",
		gshm.paddr, gshm.ioremap_paddr, gshm.mem_size, (uintptr_t)gshm.vaddr);

	arm_smccc_smc(MTK_SIP_KERNEL_ISE_CONTROL,
		ISE_MODULE_TRUSTY, ISE_TYPE_MCIA, 0, 0, 0, 0, 0, &res);
	if (res.a0) {
		dev_err(&pdev->dev, "Failed to get mcia memory region: 0x%lx\n", res.a0);
		ret = -EINVAL;
		goto err_mem;
	}
	mcia_paddr = res.a1;
	mcia_size = res.a2;

	ret = trusty_shm_init_pool(&pdev->dev);
	if (ret < 0)
		goto err_shm_init_pool;

	s->dev = &pdev->dev;
	spin_lock_init(&s->nop_lock);
	INIT_LIST_HEAD(&s->nop_queue);
	mutex_init(&s->smc_lock);
	ATOMIC_INIT_NOTIFIER_HEAD(&s->notifier);
	ATOMIC_INIT_NOTIFIER_HEAD(&s->ise_notifier);
	init_completion(&s->cpu_idle_completion);
	platform_set_drvdata(pdev, s);

	trusty_init_version(s, &pdev->dev);

	ret = trusty_init_api_version(s, &pdev->dev);
	if (ret < 0)
		goto err_api_version;

	ret = trusty_init(&pdev->dev);
	if (ret < 0)
		goto err_init;

	ret = trusty_shm_init(&pdev->dev);
	if (ret < 0)
		goto err_shm_init;

	ret = trusty_mcia_init(&pdev->dev);
	if (ret < 0)
		goto err_mcia_init;

	s->nop_wq = alloc_workqueue("trusty-nop-wq", WQ_CPU_INTENSIVE, 0);
	if (!s->nop_wq) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "Failed create trusty-nop-wq\n");
		goto err_create_nop_wq;
	}

	s->nop_works = alloc_percpu(struct trusty_work);
	if (!s->nop_works) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to allocate works\n");
		goto err_alloc_works;
	}

	if (s->api_version < TRUSTY_API_VERSION_SMP)
		work_func = locked_nop_work_func;
	else
		work_func = nop_work_func;

	for_each_possible_cpu(cpu) {
		struct trusty_work *tw = per_cpu_ptr(s->nop_works, cpu);

		tw->ts = s;
		INIT_WORK(&tw->work, work_func);
	}

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to add children: %d\n", ret);
		goto err_add_children;
	}

	tstate = s;

	notif_call_wq = alloc_workqueue("trusty-notif-call-wq", WQ_UNBOUND, 0);
	if (!notif_call_wq) {
		dev_err(&pdev->dev, "Failed to create trusty-notif-call-wq\n");
		goto err_create_trusty_wq;
	}
	INIT_WORK(&notif_call_work, notif_call_work_func);

	ise_notif_call_wq = alloc_workqueue("ise-notif-call-wq", WQ_UNBOUND, 0);
	if (!ise_notif_call_wq) {
		dev_err(&pdev->dev, "Failed to create ise-notif-call-wq\n");
		goto err_create_ise_wq;
	}
	INIT_WORK(&ise_notif_call_work, ise_notif_call_work_func);

	return 0;

err_create_ise_wq:
	destroy_workqueue(notif_call_wq);
err_create_trusty_wq:
err_add_children:
	for_each_possible_cpu(cpu) {
		struct trusty_work *tw = per_cpu_ptr(s->nop_works, cpu);

		flush_work(&tw->work);
	}
	free_percpu(s->nop_works);
err_alloc_works:
	destroy_workqueue(s->nop_wq);
err_create_nop_wq:
	trusty_shm_destroy_pool(&pdev->dev);
err_mcia_init:
err_shm_init:
err_init:
err_api_version:
	if (s->version_str) {
		device_remove_file(&pdev->dev, &dev_attr_trusty_version);
		kfree(s->version_str);
	}
	device_for_each_child(&pdev->dev, NULL, trusty_remove_child);
	mutex_destroy(&s->smc_lock);
err_shm_init_pool:
err_mem:
	kfree(s);
err_allocate_state:
	return ret;
}

static int trusty_remove(struct platform_device *pdev)
{
	/* do not wake up ise during device shutdown */
	//unsigned int cpu;
	struct trusty_state *s = platform_get_drvdata(pdev);

	device_for_each_child(&pdev->dev, NULL, trusty_remove_child);

	destroy_workqueue(ise_notif_call_wq);
	destroy_workqueue(notif_call_wq);

	/* do not wake up ise during device shutdown */
#if 0
	for_each_possible_cpu(cpu) {
		struct trusty_work *tw = per_cpu_ptr(s->nop_works, cpu);

		flush_work(&tw->work);
	}
#endif
	free_percpu(s->nop_works);
	destroy_workqueue(s->nop_wq);

	trusty_shm_destroy_pool(&pdev->dev);

	mutex_destroy(&s->smc_lock);
	if (s->version_str) {
		device_remove_file(&pdev->dev, &dev_attr_trusty_version);
		kfree(s->version_str);
	}
	kfree(s);
	return 0;
}

static const struct of_device_id trusty_of_match[] = {
	{ .compatible = "android,ise-trusty-smc-v1", },
	{},
};

static struct platform_driver trusty_driver = {
	.probe = trusty_probe,
	.remove = trusty_remove,
	.driver	= {
		.name = "ise-trusty",
		.owner = THIS_MODULE,
		.of_match_table = trusty_of_match,
	},
};

static int __init trusty_driver_init(void)
{
	return platform_driver_register(&trusty_driver);
}

static void __exit trusty_driver_exit(void)
{
	platform_driver_unregister(&trusty_driver);
}

subsys_initcall(trusty_driver_init);
module_exit(trusty_driver_exit);

MODULE_LICENSE("GPL v2");
