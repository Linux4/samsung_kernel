/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "npu-profile-v2.h"
#include "npu-interface.h"
#include "npu-util-llq.h"
#include "npu-vertex.h"
#include "npu-util-memdump.h"
#include "dsp-dhcp.h"
#include "npu-dvfs.h"
#include "npu-precision.h"
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC) || IS_ENABLED(CONFIG_DSP_USE_VS4L)
#include "npu-util-regs.h"
#include "dsp-common-type.h"
#endif
#include "npu-hw-device.h"

#define LINE_TO_SGMT(sgmt_len, ptr)	((ptr) & ((sgmt_len) - 1))
static struct workqueue_struct *wq;
static struct work_struct work_report;

static void __rprt_manager(struct work_struct *w);
int npu_kpi_response_read(void);
extern struct npu_proto_drv *protodr;

static struct npu_interface interface = {
	.mbox_hdr = NULL,
	.sfr = NULL,
	.sfr2 = NULL,
	.sfr3 = NULL,
	.gnpu0 = NULL,
	.gnpu1 = NULL,
	.addr = NULL,
};

void dbg_dump_mbox(void);

#define npu_mailbox_wait_for_clr(grp_num, bit)	{				\
	while (timeout && (interface.sfr->grp[grp_num].ms & bit)) {		\
		timeout--;							\
		if (!(timeout % 100))						\
			npu_warn("Mailbox %d delayed for %d\n", cmdType,	\
				MAILBOX_CLEAR_CHECK_TIMEOUT - timeout);		\
	}									\
										\
	if (!timeout) {								\
		npu_err("Mailbox %d delayed for %d times, cancelled\n",		\
				cmdType, MAILBOX_CLEAR_CHECK_TIMEOUT);		\
		dbg_dump_mbox();						\
		ret = -EWOULDBLOCK;						\
		return ret;							\
	}									\
}

static int __send_interrupt(u32 cmdType, struct command *cmd, u32 type)
{
	int ret = 0;
	u32 val;
	u32 timeout;
	u32 mbox_or_bit;

	timeout = MAILBOX_CLEAR_CHECK_TIMEOUT;
	switch (cmdType) {
	case COMMAND_LOAD:
	case COMMAND_UNLOAD:
	case COMMAND_PROFILE_CTL:
	case COMMAND_PURGE:
	case COMMAND_PWR_CTL:
	case COMMAND_MODE:
	case COMMAND_FW_TEST:
	case COMMAND_CORE_CTL:
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		/* fallthrough */
	case COMMAND_IMB_SIZE:
#endif
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		interface.sfr->grp[MAILBOX_GROUP_LPRIORITY].g = 0x10000;
		val = interface.sfr->grp[MAILBOX_GROUP_LPRIORITY].g;
#else
		interface.sfr->grp[MAILBOX_SWI_H2F].s = 0x1;
		val = interface.sfr->grp[MAILBOX_SWI_H2F].s;
#endif

		break;
	case COMMAND_PROCESS:
	case COMMAND_PROCESS_CANCEL:
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		if (type == NPU_MBOX_REQUEST_MEDIUM || type == NPU_MBOX_REQUEST_LOW)
			mbox_or_bit = MAILBOX_GROUP_MPRIORITY;
		else
			mbox_or_bit = MAILBOX_GROUP_HPRIORITY;

		interface.sfr->grp[mbox_or_bit].g = 0xFFFFFFFF;
		val = interface.sfr->grp[mbox_or_bit].g;
#else
		if (type == NPU_MBOX_REQUEST_MEDIUM || type == NPU_MBOX_REQUEST_LOW)
			mbox_or_bit = 0x2;
		else
			mbox_or_bit = 0x4;

		interface.sfr->grp[MAILBOX_SWI_H2F].s = mbox_or_bit;
		val = interface.sfr->grp[MAILBOX_SWI_H2F].s;
#endif

		break;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	case COMMAND_IMB_RSP:
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		interface.sfr->grp[MAILBOX_GROUP_FW_RESP].g = 0xFFFFFFFF;
		val = interface.sfr->grp[MAILBOX_GROUP_FW_RESP].g;
#else
		interface.sfr->grp[MAILBOX_SWI_H2F].s = 0x8;
		val = interface.sfr->grp[MAILBOX_SWI_H2F].s;
#endif
		break;
#endif
	default:
		break;
	}

	return ret;
}

static irqreturn_t mailbox_high_prio_response(int irq, void *data)
{
	struct npu_device *device;
	struct dsp_dhcp *dhcp;

	device = container_of(interface.system, struct npu_device, system);
	dhcp = device->system.dhcp;

	/* high reponse */
	npu_profile_record_start(PROFILE_DD_TOP, 0, ktime_to_us(ktime_get_boottime()), 8, 0);
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	npu_write_hw_reg(npu_get_io_area(interface.system, "sfrmbox1"), 0x0, 0x0, 0xFFFFFFFF, 0);
#else
	{
		u32 val;
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		val = interface.sfr2->grp[MAILBOX_GROUP_HRESPONSE].ms;
		if (val)
			interface.sfr2->grp[MAILBOX_GROUP_HRESPONSE].c = val;
#else
		val = interface.sfr2->grp[MAILBOX_SWI_F2H].ms & 1;
		if (val)
			interface.sfr2->grp[MAILBOX_SWI_F2H].c = val;
#endif
	}
#endif
	if (device->sched->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE) {
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		if (dsp_dhcp_read_reg_idx(dhcp, DSP_DHCP_PDCL_RESULT) == -ERR_TIMEOUT) {
			npu_err("watchdog reset triggered by NPU driver\n");
			npu_util_dump_handle_nrespone(interface.system);
		}
#endif
		complete(&device->my_completion);
	} else if (device->sched->mode == NPU_PERF_MODE_NPU_BOOST_BLOCKING) {
		npu_kpi_response_read();
	} else {
		atomic_inc(&protodr->high_prio_frame_count);
		if (interface.rslt_notifier != NULL)
			interface.rslt_notifier(NULL);
	}

	return IRQ_HANDLED;
}
static irqreturn_t mailbox_normal_prio_response(int irq, void *data)
{
	/* normal reponse */
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	npu_write_hw_reg(npu_get_io_area(interface.system, "sfrmbox1"), 0x1000, 0x0, 0xFFFFFFFF, 0);
#else
	{
		u32 val;
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		val = interface.sfr2->grp[MAILBOX_GROUP_NRESPONSE].ms;
		if (val)
			interface.sfr2->grp[MAILBOX_GROUP_NRESPONSE].c = val;
#else
		val = interface.sfr2->grp[MAILBOX_SWI_F2H].ms & 2;
		if (val)
			interface.sfr2->grp[MAILBOX_SWI_F2H].c = val;
#endif
	}
#endif
	atomic_inc(&protodr->normal_prio_frame_count);
	if (interface.rslt_notifier != NULL)
		interface.rslt_notifier(NULL);
	return IRQ_HANDLED;
}
static irqreturn_t mailbox_imb_req(int irq, void *data)
{
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	/* fw message */
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	npu_write_hw_reg(npu_get_io_area(interface.system, "sfrmbox1"), 0x2000, 0x0, 0xFFFFFFFF, 0);
#else
	{
		u32 val;
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		val = interface.sfr2->grp[MAILBOX_GROUP_FW_MSG].ms;
		if (val)
			interface.sfr2->grp[MAILBOX_GROUP_FW_MSG].c = val;
#else
		val = interface.sfr2->grp[MAILBOX_SWI_F2H].ms & 4;
		if (val)
			interface.sfr2->grp[MAILBOX_SWI_F2H].c = val;
#endif
	}
#endif
	if (interface.rslt_notifier != NULL)
		interface.rslt_notifier(NULL);
#else
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev < 2)) {
		unsigned long flags;
		volatile u32 interrupt, hw_intr;
		struct npu_device *device;
		struct dsp_dhcp *dhcp;

		spin_lock_irqsave(&(interface.system->token_wq_lock), flags);

		device = container_of(interface.system, struct npu_device, system);
		dhcp = device->system.dhcp;

		hw_intr = npu_read_hw_reg(npu_get_io_area(interface.system, "sfrmbox1"), 0x2000, 0xFFFFFFFF, 0);
		interrupt = dsp_dhcp_read_reg_idx(dhcp, DSP_DHCP_TOKEN_INTERRUPT);
		if (interrupt || hw_intr) {
			dsp_dhcp_write_reg_idx(dhcp, DSP_DHCP_TOKEN_INTERRUPT, 0x0);
			npu_write_hw_reg(npu_get_io_area(interface.system, "sfrmbox1"), 0x2000, 0x0, 0xFFFFFFFF, 0);
			npu_dvfs_update_token_status(interface.system);
			interface.system->token_clear_cnt++;
			interface.system->token_intr_cnt++;
		}

		spin_unlock_irqrestore(&(interface.system->token_wq_lock), flags);
	}
#endif
#endif
	return IRQ_HANDLED;
}
static irqreturn_t mailbox_report(int irq, void *data)
{
	/* report */
#if IS_ENABLED(CONFIG_SOC_S5E9945)
	npu_write_hw_reg(npu_get_io_area(interface.system, "sfrmbox1"), 0x3000, 0x0, 0xFFFFFFFF, 0);
#else
	{
		u32 val;
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		val = interface.sfr2->grp[MAILBOX_GROUP_REPORT].ms;
		if (val)
			interface.sfr2->grp[MAILBOX_GROUP_REPORT].c = val;
#else
		val = interface.sfr2->grp[MAILBOX_SWI_F2H].ms & 8;
		if (val)
			interface.sfr2->grp[MAILBOX_SWI_F2H].c = val;
#endif
	}
#endif
	if (wq)
		queue_work(wq, &work_report);
	return IRQ_HANDLED;
}

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
static irqreturn_t mailbox_fence(int irq, void *data)
{
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	/* fence */
	u32 val;
	int i = 0;
	u32 out_fence, gnpu0_info, gnpu1_info;
	struct npu_device *device;
	unsigned long flags;

	val = interface.sfr3->grp[MAILBOX_SWI_F2H].ms & 1;
	if (val)
		interface.sfr3->grp[MAILBOX_SWI_F2H].c = val;

	device = container_of(interface.system, struct npu_device, system);

	gnpu0_info = npu_read_hw_reg(interface.gnpu0, 0x20B0, 0xFFFFFFFF, 0);
	gnpu1_info = npu_read_hw_reg(interface.gnpu1, 0x20B0, 0xFFFFFFFF, 0);
	out_fence = gnpu0_info | gnpu1_info;

	/* Check the bit and call the fence connected to the session where the operation is finished. */
	local_irq_save(flags);
	for (i = 0; i < NPU_MAX_SESSION; i++) {
		if (out_fence & (1 << i) && device->sessionmgr.session[i]) {
			device->sessionmgr.session[i]->out_fence_exe_value++;
			npu_sync_timeline_signal(device->sessionmgr.session[i]->out_fence, device->sessionmgr.session[i]->out_fence_exe_value);
		}
	}
	local_irq_restore(flags);
#endif

	return IRQ_HANDLED;
}
#endif

static irqreturn_t (*mailbox_isr_list[])(int, void *) = {
	mailbox_high_prio_response,
	mailbox_normal_prio_response,
	mailbox_imb_req,
	mailbox_report,
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	mailbox_fence,
#endif
};

void dbg_dump_mbox(void)
{
	int i, rptr;
	u32 sgmt_len;
	char *base;

	volatile struct mailbox_ctrl *ctrl;

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		ctrl = &(interface.mbox_hdr->f2hctrl[i]);
		sgmt_len = ctrl->sgmt_len;
		base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);
		rptr = 0;
		npu_dump("H2F_MBOX - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
	}

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		ctrl = &(interface.mbox_hdr->h2fctrl[i]);
		sgmt_len = ctrl->sgmt_len;
		base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);
		rptr = 0;
		npu_dump("F2H_MBOX - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
	}
}

void dbg_print_error(void)
{
	u32 nSize = 0;
	u32 wptr, rptr, sgmt_len;
	char buf[BUFSIZE - 1] = "";
	char *base;

	volatile struct mailbox_ctrl *ctrl;

	if (interface.mbox_hdr == NULL)
		return;
	mutex_lock(&interface.lock);

	ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT]);
	wptr = ctrl->wptr;
	rptr = ctrl->rptr;
	sgmt_len = ctrl->sgmt_len;
	base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);

	if (wptr < rptr) { //When wptr circled a time,
		while (rptr != 0) {
			if ((sgmt_len - rptr) >= BUFSIZE) {
				nSize = BUFSIZE - 1;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr += nSize;
			} else {
				nSize = sgmt_len - rptr;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr = 0;
			}
			pr_err("%s\n", buf);
			buf[0] = '\0';
		}
	}
	while (wptr != rptr) {
		if ((wptr - rptr) > BUFSIZE - 1) {//need more memcpy from io.
			nSize = BUFSIZE - 1;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr += nSize;
		} else {
			nSize = wptr - rptr;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr = wptr;
		}
		pr_err("%s\n", buf);
		buf[0] = '\0';
	}
	mutex_unlock(&interface.lock);
}

#ifndef CONFIG_NPU_KUNIT_TEST
void dbg_print_ncp_header(struct ncp_header *nhdr)
{
	npu_info("============= ncp_header =============\n");
	npu_info("mabic_number1: 0X%X\n", nhdr->magic_number1);
	npu_info("hdr_version: 0X%X\n", nhdr->hdr_version);
	npu_info("hdr_size: 0X%X\n", nhdr->hdr_size);
#if (NCP_VERSION >= 25)
	npu_info("model name: %s\n", nhdr->model_name);
	npu_info("periodicity: 0X%X\n", nhdr->periodicity);
	npu_info("unique_id: 0X%X\n", nhdr->unique_id);
#else
	npu_info("net_id: 0X%X\n", nhdr->net_id);
	npu_info("unique_id: 0X%X\n", nhdr->unique_id);
	npu_info("period: 0X%X\n", nhdr->period);
#endif
	npu_info("priority: 0X%X\n", nhdr->priority);
	npu_info("flags: 0X%X\n", nhdr->flags);
	npu_info("workload: 0X%X\n", nhdr->workload);

	npu_info("addr_vector_offset: 0X%X\n", nhdr->address_vector_offset);
	npu_info("addr_vector_cnt: 0X%X\n", nhdr->address_vector_cnt);
	npu_info("magic_number2: 0X%X\n", nhdr->magic_number2);
}
#endif

void dbg_print_interface(void)
{
	npu_info("=============  interface mbox hdr =============\n");
	npu_info("mbox_hdr: 0x%pK\n", interface.mbox_hdr);
	npu_info("mbox_hdr.max_slot: %d\n", interface.mbox_hdr->max_slot);
	npu_info("mbox_hdr.version: %d\n", interface.mbox_hdr->version);
	npu_info("mbox_hdr.signature2: 0x%x\n", interface.mbox_hdr->signature2);
	npu_info("mbox_hdr.signature1: 0x%x\n", interface.mbox_hdr->signature1);
	npu_info("sfr: 0x%pK\n", interface.sfr);
	npu_info("addr: 0x%pK\n", NPU_MBOX_BASE(interface.mbox_hdr));
}

u32 npu_get_hw_info(void)
{
	union npu_hw_info hw_info;

	memset(&hw_info, 0, sizeof(hw_info));

	hw_info.fields.product_id = (exynos_soc_info.product_id & EXYNOS_SOC_MASK) << 4;

	/* DCache disable before EVT 1.1 */
	if ((exynos_soc_info.main_rev >= 2)
	   || (exynos_soc_info.main_rev >= 1 && exynos_soc_info.sub_rev >= 1))
		hw_info.fields.dcache_en = 1;
	else
		hw_info.fields.dcache_en = 0;

	npu_info("HW Info = %08x\n", hw_info.value);

	return hw_info.value;
}
static int check_interruptable(u32 cmdType, u32 type){
	int ret = 0;
	u32 timeout;
	u32 mbox_or_bit;

	timeout = MAILBOX_CLEAR_CHECK_TIMEOUT;
	switch (cmdType) {
	case COMMAND_LOAD:
	case COMMAND_UNLOAD:
	case COMMAND_PROFILE_CTL:
	case COMMAND_PURGE:
	case COMMAND_PWR_CTL:
	case COMMAND_MODE:
	case COMMAND_FW_TEST:
	case COMMAND_CORE_CTL:
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		/* fallthrough */
	case COMMAND_IMB_SIZE:
#endif
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		npu_mailbox_wait_for_clr(MAILBOX_GROUP_LPRIORITY, 0xFFFFFFFF);
#else
		npu_mailbox_wait_for_clr(MAILBOX_SWI_H2F, 0x1);
#endif

		break;
	case COMMAND_PROCESS:
	case COMMAND_PROCESS_CANCEL:
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		if (type == NPU_MBOX_REQUEST_MEDIUM || type == NPU_MBOX_REQUEST_LOW)
			mbox_or_bit = MAILBOX_GROUP_MPRIORITY;
		else
			mbox_or_bit = MAILBOX_GROUP_HPRIORITY;

		npu_mailbox_wait_for_clr(mbox_or_bit, 0xFFFFFFFF);
#else
		if (type == NPU_MBOX_REQUEST_MEDIUM || type == NPU_MBOX_REQUEST_LOW)
			mbox_or_bit = 0x2;
		else
			mbox_or_bit = 0x4;

		npu_mailbox_wait_for_clr(MAILBOX_SWI_H2F, mbox_or_bit);
#endif

		break;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	case COMMAND_IMB_RSP:
#if IS_ENABLED(CONFIG_NPU_USE_MAILBOX_GROUP)
		npu_mailbox_wait_for_clr(MAILBOX_GROUP_FW_RESP, 0xFFFFFFFF);
#else
		npu_mailbox_wait_for_clr(MAILBOX_SWI_H2F, 0x8);
#endif
		break;
#endif
	default:
		break;
	}

	return ret;
}
static int npu_set_cmd(struct message *msg, struct command *cmd, u32 cmdType)
{
	int ret = 0;
	volatile struct mailbox_ctrl *ctrl;
	ctrl = &interface.mbox_hdr->h2fctrl[cmdType];

	ret = check_interruptable(msg->command, cmdType);
	if (ret)
		goto I_ERR;

	ret = mbx_ipc_put(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, msg, cmd);
	if (ret)
		goto I_ERR;

	ret = __send_interrupt(msg->command, cmd, cmdType);
	if (ret)
		goto I_ERR;
	return 0;
I_ERR:
	switch (ret) {
	case -ERESOURCE:
		npu_warn("No space left on mailbox : ret = %d\n", ret);
		break;
	default:
		npu_err("mbx_ipc_put err with %d\n", ret);
		break;
	}
	return ret;
}

static int npu_interface_probe(struct device *dev, void *regs1, void *regs2, void *regs3)
{
	int ret = 0;

	BUG_ON(!dev);
	if (!regs1 || !regs2) {
		probe_err("fail\n");
		ret = -EINVAL;
		return ret;
	}

	interface.sfr = (volatile struct mailbox_sfr *)regs1;
	interface.sfr2 = (volatile struct mailbox_sfr *)regs2;
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	interface.sfr3 = (volatile struct mailbox_sfr *)regs3;
#endif

	mutex_init(&interface.lock);
	wq = alloc_workqueue("my work", WQ_FREEZABLE|WQ_HIGHPRI, 1);
	INIT_WORK(&work_report, __rprt_manager);

	return ret;
}

static int npu_interface_open(struct npu_system *system)
{
	int i, ret = 0, irq_num;
	const char *buf;
	struct npu_device *device;
	struct device *dev = &system->pdev->dev;
	struct cpumask cpu_mask;

	BUG_ON(!system);
	device = container_of(system, struct npu_device, system);
	interface.mbox_hdr = system->mbox_hdr;
	irq_num = MAILBOX_IRQ_CNT;
	interface.system = &device->system;
	interface.dhcp = device->system.dhcp;
	interface.mbox_hdr->hw_info = npu_get_hw_info();
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	interface.gnpu0 = npu_get_io_area(interface.system, "sfrgnpu0");
	interface.gnpu1 = npu_get_io_area(interface.system, "sfrgnpu1");
	if (device->sched->mode != NPU_PERF_MODE_NPU_BOOST_BLOCKING)
		interface.sfr3->grp[MAILBOX_SWI_F2H].e = 0x0;
	else
		interface.sfr3->grp[MAILBOX_SWI_F2H].e = 0x1;
#endif

	if ((sizeof(mailbox_isr_list)/sizeof(mailbox_isr_list[0])) != MAILBOX_IRQ_CNT)
		goto err_exit;

	for (i = 0; i < irq_num; i++) {
		ret = devm_request_irq(dev, system->irq[i], mailbox_isr_list[i],
				IRQF_TRIGGER_HIGH, "exynos-npu", NULL);
		if (ret) {
			npu_err("fail(%d) in devm_request_irq(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	ret = of_property_read_string(dev->of_node, "samsung,npuinter-isr-cpu-affinity", &buf);
	if (ret) {
		npu_info("set the CPU affinity of ISR to 5\n");
		cpumask_set_cpu(5, &cpu_mask);
	}	else {
		npu_info("set the CPU affinity of ISR to %s\n", buf);
		cpulist_parse(buf, &cpu_mask);
	}
	system->default_affinity = cpu_mask.bits[0];

	for (i = 0; i < irq_num; i++) {
		ret = irq_set_affinity_hint(system->irq[i], &cpu_mask);
		if (ret) {
			npu_err("fail(%d) in irq_set_affinity_hint(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	wq = alloc_workqueue("rprt_manager", __WQ_LEGACY | __WQ_ORDERED, 0);
	if (!wq) {
		npu_err("err in alloc_worqueue.\n");
		goto err_exit;
	}
	ret = mailbox_init(interface.mbox_hdr, system);
	if (ret) {
		npu_err("error(%d) in npu_mailbox_init\n", ret);
		dbg_print_interface();
		goto err_workqueue;
	}
	return ret;

err_workqueue:
	destroy_workqueue(wq);
err_probe_irq:
	for (i = 0; i < irq_num; i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}
err_exit:
	interface.addr = NULL;
	interface.mbox_hdr = NULL;
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
	npu_err("EMERGENCY_RECOVERY is triggered.\n");
	return ret;
}

static int npu_interface_close(struct npu_system *system)
{
	int i, wptr, rptr;
	struct device *dev;
	int irq_num;
	struct npu_device *device;

	if (!system) {
		npu_err("fail\n");
		return -EINVAL;
	}

	if (likely(interface.mbox_hdr))
		mailbox_deinit(interface.mbox_hdr, system);

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);
	irq_num = MAILBOX_IRQ_CNT;

	for (i = 0; i < irq_num; i++)
		irq_set_affinity_hint(system->irq[i], NULL);

	for (i = 0; i < irq_num; i++)
		devm_free_irq(dev, system->irq[i], NULL);

	queue_work(wq, &work_report);
	if ((wq) && (interface.mbox_hdr)) {
		if (work_pending(&work_report)) {
			cancel_work_sync(&work_report);
			wptr = interface.mbox_hdr->f2hctrl[1].wptr;
			rptr = interface.mbox_hdr->f2hctrl[1].rptr;
			npu_dbg("work was canceled due to interface close, rptr/wptr : %d/%d\n", wptr, rptr);
		}
		flush_workqueue(wq);
		destroy_workqueue(wq);
		wq = NULL;
	}

#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
if (device->sched->mode != NPU_PERF_MODE_NPU_BOOST_BLOCKING)
	interface.sfr3->grp[MAILBOX_SWI_F2H].e = 0x0;
else
	interface.sfr3->grp[MAILBOX_SWI_F2H].e = 0x1;
#endif

	interface.addr = NULL;
	interface.mbox_hdr = NULL;
	return 0;
}

int register_rslt_notifier(protodrv_notifier func)
{
	interface.rslt_notifier = func;
	return 0;
}

int register_msgid_get_type(int (*msgid_get_type_func)(int))
{
	interface.msgid_get_type = msgid_get_type_func;
	return 0;
}

static u32 config_npu_load_payload(struct npu_nw *nw)
{
	struct npu_session *session = nw->session;
	struct cmd_load_payload *payload = session->ncp_payload->vaddr;

	payload[COMMAND_LOAD_USER_NCP].addr = nw->ncp_addr.daddr;
	payload[COMMAND_LOAD_USER_NCP].size = nw->ncp_addr.size;
	payload[COMMAND_LOAD_USER_NCP].id = COMMAND_LOAD_USER_NCP;

	payload[COMMAND_LOAD_HDR_COPY].addr = session->ncp_hdr_buf->daddr;
	payload[COMMAND_LOAD_HDR_COPY].size = session->ncp_hdr_buf->size;
	payload[COMMAND_LOAD_HDR_COPY].id = COMMAND_LOAD_HDR_COPY;

	npu_session_ion_sync_for_device(session->ncp_payload, DMA_TO_DEVICE);

	return session->ncp_payload->daddr;
}

int nw_req_manager(int msgid, struct npu_nw *nw)
{
	int ret = 0;
	struct command cmd = {};
	struct message msg = {};

	switch (nw->cmd) {
	case NPU_NW_CMD_BASE:
		npu_info("abnormal command type\n");
		break;
	case NPU_NW_CMD_LOAD:
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		/* TODO: refactoring msgid check, NPU (0 ~ 31), DSP(32 ~ 63)*/
		if (msgid < 32) {
			/* NPU */
#endif
			cmd.c.load.oid = nw->uid;
			cmd.c.load.tid = nw->bound_id;
			cmd.c.load.priority = nw->priority;
			/*
			Currently, FW responds to high priority or normal priority response mailbox to the PROCESS cmd request based on NCP
			priority.Till a formal discussion is made regarding how FW responds based on NCP priority, and frame priority, this
			is a temporary hack to jump up	the NCP priority in case of KPI mode inferences. Without this change, even if we do
			KPI inferences, even though we send the request on high priority mailbox, we get response on normal priority
			response mailbox
			*/
			if (is_kpi_mode_enabled(false))
				cmd.c.load.priority = 200;	/*	KPI_FRAME_PRIORITY = 200	*/

			cmd.c.load.deadline = nw->deadline;
			cmd.c.load.flags = nw->preference;

			cmd.c.load.n_param = 2;
			cmd.length = sizeof(struct cmd_load_payload) * cmd.c.load.n_param;
			cmd.payload = config_npu_load_payload(nw);
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		} else {
			/* DSP */
			struct dsp_common_graph_info_v4 *load_msg =
				(struct dsp_common_graph_info_v4 *)(nw->ncp_addr.vaddr);
			int num_param = load_msg->n_tsgd + load_msg->n_param;

			cmd.c.load.oid = (nw->uid << 24) | (nw->session->unified_id << 16) | (load_msg->global_id & 0xFFFF);
			cmd.c.load.tid = load_msg->n_tsgd;
			cmd.c.load.n_param = load_msg->n_param;
			cmd.c.load.n_kernel = load_msg->n_kernel;

			//dl_unique_id
			cmd.c.load.deadline = nw->session->dl_unique_id;

			/* only need param list */
			cmd.length = (u32)sizeof(struct dsp_common_param_v4) * num_param;
			cmd.payload = nw->ncp_addr.daddr + sizeof(struct dsp_common_graph_info_v4);
		}
#endif

		npu_info("Loading data 0x%08x (%d)\n", cmd.payload, cmd.length);
		msg.command = COMMAND_LOAD;
		msg.length = (u32)sizeof(struct command);
		break;
	case NPU_NW_CMD_UNLOAD:
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		/* TODO: refactoring msgid check, NPU (0 ~ 31), DSP(32 ~ 63)*/
		if (msgid < 32) {
			/* NPU */
#endif
			cmd.c.unload.oid = nw->uid;//NPU_MAILBOX_DEFAULT_OID;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		} else {
			/* DSP */
			cmd.c.unload.oid = (nw->uid << 24) | (nw->session->unified_id << 16) | (nw->session->global_id & 0xFFFF);
		}
#endif
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_UNLOAD;
		msg.length = (u32)sizeof(struct command);
		break;
	case NPU_NW_CMD_MODE:
		cmd.c.mode.mode = nw->param0;
		cmd.c.mode.llc_size = nw->param1;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_MODE;
		msg.length = (u32)sizeof(struct command);
		break;
	case NPU_NW_CMD_STREAMOFF:
		cmd.c.purge.oid = nw->uid;
		cmd.payload = 0;
		msg.command = COMMAND_PURGE;
		msg.length = (u32)sizeof(struct command);
		break;
	case NPU_NW_CMD_POWER_CTL:
		cmd.c.pwr_ctl.magic = 0xdeadbeef;
		cmd.payload = 0;
		msg.command = COMMAND_PWR_CTL;
		msg.length = (u32)sizeof(struct command);
		break;
	case NPU_NW_CMD_FW_TC_EXECUTE:
		cmd.c.fw_test.param = nw->param0;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_FW_TEST;
		msg.length = (u32)sizeof(struct command);
		break;
	case NPU_NW_CMD_CORE_CTL:
		// cmd.c.core_ctl.magic = nw->param0;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_CORE_CTL;
		msg.length = (u32)sizeof(struct command);
		break;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	case NPU_NW_CMD_IMB_SIZE:
		cmd.c.imb.imb_size = nw->param0;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_IMB_SIZE;
		msg.length = (u32)sizeof(struct command);
		break;
#endif
	case NPU_NW_CMD_END:
		break;
	default:
		npu_err("invalid CMD ID: (%db)", nw->cmd);
		ret = FALSE;
		goto nw_req_err;
	}
	msg.mid = msgid;

	ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_LOW);
	if (ret)
		goto nw_req_err;

	mbx_ipc_print_dbg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->h2fctrl[0]);
	return TRUE;
nw_req_err:
	return ret;
}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
static int get_size_of_execution_info(struct npu_frame *frame)
{
	struct nq_buffer *buffers;
	struct dsp_common_execute_info_v4 *exe_info;
	int num_param;
	int size_of_exe_info;

	buffers = get_buffer_of_execution_info(frame);
	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return -1;
	}

	exe_info = buffers->vaddr;
	if (!exe_info) {
		npu_err("failed to get vaddr of execution info for dsp\n");
		return -2;
	}

	num_param = exe_info->n_update_param;
	size_of_exe_info = sizeof(struct dsp_common_param_v4) * num_param;

	return size_of_exe_info;
}

static dma_addr_t get_iova_of_execution_info(struct npu_frame *frame)
{
	struct nq_buffer *buffers = get_buffer_of_execution_info(frame);

	if (!buffers) {
		npu_err("failed to get buffer of execution info for dsp\n");
		return 0;
	}

	return buffers->daddr + sizeof(struct dsp_common_execute_info_v4);
}

int __update_iova_of_exe_message_of_dsp(struct npu_session *session, void *msg_kaddr);
#endif

int fr_req_manager(int msgid, struct npu_frame *frame)
{
	int offset = 0;
	int ret = FALSE;
	struct command cmd = {};
	struct message msg = {};
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	struct dsp_common_execute_info_v4 *exe_info;
	struct npu_session *session = frame->session;
	struct nq_buffer *buffers = get_buffer_of_execution_info(frame);

	WARN_ON(!buffers);
#endif
	npu_profile_record_end(PROFILE_DD_SESSION, frame->input->index, ktime_to_us(ktime_get_boottime()), 2, frame->uid);
	npu_profile_record_start(PROFILE_DD_FW_IF, frame->input->index, ktime_to_us(ktime_get_boottime()), 2, frame->uid);
	switch (frame->cmd) {
	case NPU_FRAME_CMD_BASE:
		break;
	case NPU_FRAME_CMD_Q:
		cmd.c.process.fid = frame->frame_id;
		cmd.c.process.priority = frame->priority;
		if (frame->input->containers->count > 1)
			cmd.c.process.flags = (frame->input->index << 28) | CMD_PROCESS_FLAG_BATCH;
		else 
			cmd.c.process.flags = (frame->input->index << 28);

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		if (session->hids & NPU_HWDEV_ID_DSP) {
			exe_info = get_execution_info_for_dsp(frame);
			cmd.c.process.oid = (frame->uid << 24) | (frame->session->unified_id << 16) | (exe_info->global_id & 0xFFFF);
			cmd.length = get_size_of_execution_info(frame);
			cmd.payload = get_iova_of_execution_info(frame);
			if (buffers)
				dma_buf_end_cpu_access(buffers->dma_buf, DMA_TO_DEVICE);
		} else {
			cmd.c.process.oid = frame->uid;
			cmd.length = (frame->input->containers->count << 16) | frame->mbox_process_dat.address_vector_cnt;
			cmd.payload = frame->mbox_process_dat.address_vector_start_daddr;
		}
#else
		cmd.c.process.oid = frame->uid;
		cmd.length = frame->mbox_process_dat.address_vector_cnt;
		cmd.payload = frame->mbox_process_dat.address_vector_start_daddr;
#endif

		msg.command = COMMAND_PROCESS;
		msg.length = (u32)sizeof(struct command);
		npu_dbg("oid %d, len %d, payload 0x%x, cmd %d\n",
				cmd.c.process.oid, cmd.length, cmd.payload, msg.command);
		break;

	case NPU_FRAME_CMD_Q_CANCEL:
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		if (session->hids & NPU_HWDEV_ID_DSP) {
			exe_info = get_execution_info_for_dsp(frame);
			cmd.c.process.oid = (frame->uid << 24) | (frame->session->unified_id << 16) | (exe_info->global_id & 0xFFFF);
		} else {
			cmd.c.process.oid = frame->uid;
		}
#else
		cmd.c.process.oid = frame->uid;
#endif

		cmd.c.process.fid = frame->frame_id;

		msg.command = COMMAND_PROCESS_CANCEL;
		msg.length = (u32)sizeof(struct command);
		npu_dbg("oid %d, cmd %d\n", cmd.c.process.oid, msg.command);


		break;
	case NPU_FRAME_CMD_END:
		break;
	}
	msg.mid = msgid;
	if (frame->priority < NPU_PRIORITY_PREMPT_MIN) {
		ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_MEDIUM);
		offset = 1;
	} else {
		ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_HIGH);
		offset = 2;
	}

	if (ret)
		goto fr_req_err;

	mbx_ipc_print_dbg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->h2fctrl[offset]);
	return TRUE;
fr_req_err:
	return ret;
}

int nw_rslt_manager(int *ret_msgid, struct npu_nw *nw)
{
	int ret, channel;
	struct message msg;
	struct command cmd;

	channel = MAILBOX_F2HCTRL_NRESPONSE;

	ret = mbx_ipc_peek_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)
		return FALSE;

	ret = interface.msgid_get_type(msg.mid);
	if (ret != PROTO_DRV_REQ_TYPE_NW)
		return FALSE;//report error content:

	ret = mbx_ipc_get_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)
		return FALSE;

	ret = mbx_ipc_get_cmd(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg, &cmd);
	if (ret) {
		npu_err("get command error\n");
		return FALSE;
	}

	if (msg.command == COMMAND_DONE) {
		npu_info("COMMAND_DONE for mid: (%d), val(0x%x)\n", msg.mid,
			 cmd.c.done.request_specific_value);
		nw->result_value = cmd.c.done.request_specific_value;
		nw->result_code = NPU_ERR_NO_ERROR;
	} else if (msg.command == COMMAND_NDONE) {
		npu_err("COMMAND_NDONE for mid: (%d) error(%u/0x%08x)\n"
			, msg.mid, cmd.c.ndone.error, cmd.c.ndone.error);
		nw->result_code = cmd.c.ndone.error;
	} else {
		npu_err("invalid msg.command: (%d)\n", msg.command);
		return FALSE;
	}

	fw_rprt_manager();
	*ret_msgid = msg.mid;

	return TRUE;
}

int fr_rslt_manager(int *ret_msgid, struct npu_frame *frame, enum channel_flag c)
{

	int ret = FALSE, channel;
	struct message msg;
	struct command cmd;

	if (c == HIGH_CHANNEL)
		channel = MAILBOX_F2HCTRL_RESPONSE;
	else
		channel = MAILBOX_F2HCTRL_NRESPONSE;

	ret = mbx_ipc_peek_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)
		return FALSE;

	ret = interface.msgid_get_type(msg.mid);
	if (ret != PROTO_DRV_REQ_TYPE_FRAME) {
		return FALSE;
	}

	ret = mbx_ipc_get_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)// 0 : no msg, less than zero : Err
		return FALSE;

	ret = mbx_ipc_get_cmd(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg, &cmd);
	if (ret)
		return FALSE;

	if (interface.system->s2d_mode == NPU_S2D_MODE_ON) {
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		npu_info("S2D mode is enable, so into wdtrst\n");
		npu_util_dump_handle_nrespone(interface.system);
#endif
	}

	if (msg.command == COMMAND_DONE) {
		npu_dbg("COMMAND_DONE for mid: (%d)\n", msg.mid);
		if (cmd.c.done.request_specific_value > 0)
			frame->duration	= cmd.c.done.request_specific_value;

		frame->result_code = NPU_ERR_NO_ERROR;

		if (msg.mid >= 32) {
			frame->ncp_type	= 0xC0DECAFE;
			npu_info("dsp only model, precision skip. (msg.mid : %d, fid : %u)\n", msg.mid, cmd.c.done.fid);
		} else {
			if (cmd.c.done.fid == 1) {
				frame->ncp_type	= cmd.c.done.request_specific_value;
				npu_info("For precision - msg.mid(%d) is smaller than 32, ncp_type : %u\n", msg.mid, frame->ncp_type);
			}
		}
	} else if (msg.command == COMMAND_NDONE) {
		npu_err("COMMAND_NDONE for mid: (%d) error(%u/0x%08x)\n"
			, msg.mid, cmd.c.ndone.error, cmd.c.ndone.error);
		if (cmd.c.ndone.error == -ERR_TIMEOUT_RECOVERED)
			frame->result_code = NPU_ERR_NPU_HW_TIMEOUT_RECOVERED;
		else if (cmd.c.ndone.error == -ERR_TIMEOUT) {
			frame->result_code = NPU_ERR_NPU_HW_TIMEOUT_NOTRECOVERABLE;
			npu_err("watchdog reset triggered by NPU driver\n");
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
			npu_util_dump_handle_nrespone(interface.system);
#endif
		} else
			frame->result_code = cmd.c.ndone.error;
	} else {
		npu_err("invalid msg.command: (%d)\n", msg.command);
		return FALSE;
	}

	*ret_msgid = msg.mid;
	fw_rprt_manager();
	return TRUE;
}

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
int fw_msg_manager(struct fw_message *fw_msg)
{
	int ret = FALSE, channel;
	struct message msg;
	struct command cmd;
	channel = MAILBOX_F2HCTRL_FWMSG;
	ret = mbx_ipc_get_msg(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg);
	if (ret <= 0)// 0 : no msg, less than zero : Err
		return FALSE;
	ret = mbx_ipc_get_cmd(NPU_MBOX_BASE((void *)interface.mbox_hdr), &interface.mbox_hdr->f2hctrl[channel], &msg, &cmd);
	if (ret) {
		npu_err("get command error\n");
		return FALSE;
	}
	fw_msg->mid = msg.mid;
	fw_msg->command = msg.command;
	if (fw_msg->command == COMMAND_IMB_REQ) {
		fw_msg->msg.imb.imb_type = cmd.c.imb_req.imb_type;
		fw_msg->msg.imb.imb_lsize = cmd.c.imb_req.imb_lsize;
		fw_msg->msg.imb.imb_hsize = cmd.c.imb_req.imb_hsize;
	}
	return TRUE;
}

int fw_res_manager(struct fw_message *fw_msg)
{
	int ret = FALSE;

	struct message msg = {};
	struct command cmd = {};

	msg.command = COMMAND_IMB_RSP;
	//msg.data = fw_msg->chunk_size;
	msg.mid = fw_msg->mid;
	msg.length = (u32)sizeof(struct command);
	if (fw_msg->command == COMMAND_IMB_REQ) {
		cmd.c.imb_rsp.imb_type = fw_msg->msg.imb.imb_type;
		cmd.c.imb_rsp.imb_lsize = fw_msg->msg.imb.imb_lsize;
		cmd.c.imb_rsp.imb_hsize = fw_msg->msg.imb.imb_hsize;
	}
	cmd.length = 0;
	cmd.payload = 0;
	ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_FWRESPONSE);
	if (ret == 0)
		return TRUE;
	else
		return FALSE;
}
#endif
//Print log which was written with last 128 byte.
int npu_check_unposted_mbox(int nCtrl)
{
	int pos;
	int ret = TRUE;
	char *base;
	u32 nSize, wptr, rptr, sgmt_len;
	u32 *buf, *strOut;
	volatile struct mailbox_ctrl *ctrl;

	if (interface.mbox_hdr == NULL)
		return -1;

	strOut = kzalloc(LENGTHOFEVIDENCE, GFP_ATOMIC);
	if (!strOut) {
		ret = -ENOMEM;
		goto err_exit;
	}
	buf = kzalloc(LENGTHOFEVIDENCE, GFP_ATOMIC);
	if (!buf) {
		if (strOut)
			kfree(strOut);
		ret = -ENOMEM;
		goto err_exit;
	}

	if (!in_interrupt())
		mutex_lock(&interface.lock);

	switch (nCtrl) {
	case ECTRL_LOW:
		ctrl = &(interface.mbox_hdr->h2fctrl[MAILBOX_H2FCTRL_LPRIORITY]);
		npu_dump("[V] H2F_MBOX[LOW] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
	case ECTRL_MEDIUM:
		ctrl = &(interface.mbox_hdr->h2fctrl[MAILBOX_H2FCTRL_MPRIORITY]);
		npu_dump("[V] H2F_MBOX[MEDIUM] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
	case ECTRL_HIGH:
		ctrl = &(interface.mbox_hdr->h2fctrl[MAILBOX_H2FCTRL_HPRIORITY]);
		npu_dump("[V] H2F_MBOX[HIGH] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
	case ECTRL_ACK:
		ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_RESPONSE]);
		npu_dump("[V] F2H_MBOX[RESPONSE] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
	case ECTRL_NACK:
		ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_NRESPONSE]);
		npu_dump("[V] F2H_MBOX[NRESPONSE] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
	case ECTRL_REPORT:
		ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT]);
		npu_dump("[V] F2H_MBOX[REPORT] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		break;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	case ECTRL_FWMSG:
		ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_FWMSG]);
		npu_err("[V] F2H_MBOX[FWMSG] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
	case ECTRL_FWRESPONSE:
		ctrl = &(interface.mbox_hdr->h2fctrl[MAILBOX_H2FCTRL_FWRESPONSE]);
		npu_err("[V] H2F_MBOX[FWRESPONSE] - rptr/wptr : %d/%d\n", ctrl->rptr, ctrl->wptr);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl, NPU_LOG_ERR);
		break;
#endif
	default:
		BUG_ON(1);
	}

	wptr = ctrl->wptr;
	sgmt_len = ctrl->sgmt_len;
	base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);

	pos = 0;
	if (wptr < LENGTHOFEVIDENCE) {
		rptr = sgmt_len - (LENGTHOFEVIDENCE - wptr);
		nSize = LENGTHOFEVIDENCE - wptr;
		npu_debug_memdump32_by_memcpy((u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr)), (u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr) + nSize));
		rptr = 0;
		pos = 0;
		nSize = wptr;
	} else {
		rptr = wptr - LENGTHOFEVIDENCE;
		nSize = LENGTHOFEVIDENCE;
	}
	if (wptr > 0)
		npu_debug_memdump32_by_memcpy((u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr)), (u32 *)(base + LINE_TO_SGMT(sgmt_len, rptr) + nSize));

	if (!in_interrupt())
		mutex_unlock(&interface.lock);

	if (buf)
		kfree(buf);
	if (strOut)
		kfree(strOut);
err_exit:
	return ret;
}

void fw_rprt_gather(void)
{
	char *base;
	size_t nSize;
	u32 wptr, rptr, sgmt_len;
	char buf[BUFSIZE - 1] = "\0";
	volatile struct mailbox_ctrl *ctrl;

	if (interface.mbox_hdr == NULL)
		return;
	mutex_lock(&interface.lock);

	ctrl = &(interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT]);
	wptr = ctrl->wptr;
	rptr = ctrl->rptr;
	sgmt_len = ctrl->sgmt_len;
	base = NPU_MAILBOX_GET_CTRL(NPU_MBOX_BASE((void *)interface.mbox_hdr), ctrl->sgmt_ofs);

	if (wptr < rptr) { //When wptr circled a time,
		while (rptr != 0) {
			if ((sgmt_len - rptr) >= BUFSIZE) {
				nSize = BUFSIZE - 1;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr += nSize;
			} else {
				nSize = sgmt_len - rptr;
				memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
				rptr = 0;
			}
			npu_fw_report_store(buf, nSize);
			buf[0] = '\0';
		}
	}

	while (wptr != rptr) {
		if ((wptr - rptr) > BUFSIZE - 1) {//need more memcpy from io.
			nSize = BUFSIZE - 1;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr += nSize;
		} else {
			nSize = wptr - rptr;
			memcpy_fromio(buf, base + LINE_TO_SGMT(sgmt_len, rptr), nSize);
			rptr = wptr;
		}
		npu_fw_report_store(buf, nSize);
		buf[0] = '\0';
	}
	interface.mbox_hdr->f2hctrl[MAILBOX_F2HCTRL_REPORT].rptr = wptr;
	mutex_unlock(&interface.lock);
}

static void __rprt_manager(struct work_struct *w)
{
	fw_rprt_gather();
}

void fw_rprt_manager(void)
{
	if (wq)
		queue_work(wq, &work_report);
	else//not opened, or already closed.
		return;
}

int mbx_rslt_fault_listener(void)
{
	int i;

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		dbg_print_ctrl(&interface.mbox_hdr->h2fctrl[i]);
		mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr),
					&interface.mbox_hdr->h2fctrl[i], NPU_LOG_ERR);
	}

	for (i = 0; i < MAX_MAILBOX / 2; i++) {
		dbg_print_ctrl(&interface.mbox_hdr->f2hctrl[i]);
		if (i != MAILBOX_F2HCTRL_REPORT)
			mbx_ipc_print(NPU_MBOX_BASE((void *)interface.mbox_hdr),
					&interface.mbox_hdr->f2hctrl[i], NPU_LOG_ERR);
	}

	return 0;
}

int nw_rslt_available(void)
{
	int ret, channel;
	struct message msg;

	channel = MAILBOX_F2HCTRL_NRESPONSE;
	ret = mbx_ipc_peek_msg(
		NPU_MBOX_BASE((void *)interface.mbox_hdr),
		&interface.mbox_hdr->f2hctrl[channel], &msg);
	return ret;
}
int fr_rslt_available(enum channel_flag c)
{
	int ret, channel;
	struct message msg;

	if (c == HIGH_CHANNEL)
		channel = MAILBOX_F2HCTRL_RESPONSE;
	else
		channel = MAILBOX_F2HCTRL_NRESPONSE;

	ret = mbx_ipc_peek_msg(
		NPU_MBOX_BASE((void *)interface.mbox_hdr),
		&interface.mbox_hdr->f2hctrl[channel], &msg);
	return ret;
}

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
int fw_msg_available(void)
{
	int ret;
	struct message msg;

	int channel = MAILBOX_F2HCTRL_FWMSG;
	ret = mbx_ipc_peek_msg(
		NPU_MBOX_BASE((void *)interface.mbox_hdr),
		&interface.mbox_hdr->f2hctrl[channel], &msg);
	return ret;
}
#endif


#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
static int get_npu_utilization(int n)
{
	return interface.system->mbox_hdr->stats.npu_utilization[n];
}
static int get_cpu_utilization(void)
{
	return interface.system->mbox_hdr->stats.cpu_utilization;
}
static int get_dsp_utilization(void)
{
	return interface.system->mbox_hdr->stats.dsp_utilization;
}
#endif

const struct npu_log_ops npu_log_ops = {
	.fw_rprt_manager = fw_rprt_manager,
	.fw_rprt_gather = fw_rprt_gather,
	.npu_check_unposted_mbox = npu_check_unposted_mbox,
};

const struct npu_if_protodrv_mbox_ops protodrv_mbox_ops = {
	.frame_result_available = fr_rslt_available,
	.frame_post_request = fr_req_manager,
	.frame_get_result = fr_rslt_manager,
	.nw_result_available = nw_rslt_available,
	.nw_post_request = nw_req_manager,
	.nw_get_result = nw_rslt_manager,
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
	.fw_msg_get = fw_msg_manager,
	.fw_res_put = fw_res_manager,
	.fwmsg_available = fw_msg_available,
#endif
	.register_notifier = register_rslt_notifier,
	.register_msgid_type_getter = register_msgid_get_type,
};

const struct npu_interface_ops n_npu_interface_ops = {
	.interface_probe = npu_interface_probe,
	.interface_open = npu_interface_open,
	.interface_close = npu_interface_close,
};

#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
const struct npu_utilization_ops n_utilization_ops = {
	.get_s_npu_utilization = get_npu_utilization,
	.get_s_cpu_utilization = get_cpu_utilization,
	.get_s_dsp_utilization = get_dsp_utilization,
};
#endif

/* Unit test */
#if IS_ENABLED(CONFIG_NPU_UNITTEST)
#define IDIOT_TESTCASE_IMPL "npu-interface.idiot"
#include "idiot-def.h"
#endif
