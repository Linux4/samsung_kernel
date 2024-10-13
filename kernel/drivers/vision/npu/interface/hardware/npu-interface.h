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

#ifndef _NPU_INTERFACE_H_
#define _NPU_INTERFACE_H_

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#include "ncp_header.h"
#include "npu-log.h"
#include "npu-util-llq.h"
#include "mailbox_ipc.h"
#include "mailbox_msg.h"
#include "npu-if-protodrv-mbox2.h"
#include "npu-protodrv.h"
#include "npu-system.h"

#define TRY_CNT	100
#define QSIZE	100
#define BUFSIZE		1024
#define FW_LOGSIZE	(1024 * 256)
#define TRUE		1
#define FALSE		0
#define LENGTHOFEVIDENCE 128

#define IOR8(port)            readb((const volatile void *)&port)
#define IOR16(port)           readw((const volatile void *)&port)
#define IOR32(port)           readl((const volatile void *)&port)
#define IOR64(port)           readq((const volatile void *)&port)

#define	MAILBOX_CLEAR_CHECK_TIMEOUT	10000

/* We need interrupts listed in dts in below order only
 * high priority response irq
 * normal priority response irq
 * imb request irq
 * report irq
 * fence irq
 * afm irq1
 * afm irq2
 */

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
#define MAILBOX_IRQ_CNT		5
#else // IS_ENABLED(CONFIG_SOC_S5E8845)
#define MAILBOX_IRQ_CNT		4
#endif
#define AFM_IRQ_INDEX			MAILBOX_IRQ_CNT

struct dsp_dhcp;

/* Interface object to mailbox_ipc */
struct npu_interface {
	/*mailbox header address */
	volatile struct mailbox_hdr *mbox_hdr;
	/*mailbox sfr address */
	volatile struct mailbox_sfr *sfr;
	volatile struct mailbox_sfr *sfr2;
	volatile struct mailbox_sfr *sfr3;
	/* gnpu sfr address */
	struct npu_iomem_area *gnpu0;
	struct npu_iomem_area *gnpu1;

	/*mailbox undelay. it could be removed later. */
	void *addr;
	int (*msgid_get_type)(int);
	protodrv_notifier rslt_notifier;
	struct mutex lock;
	struct npu_system *system;
	struct dsp_dhcp *dhcp;
};

int nw_req_manager(int msgid, struct npu_nw *nw);
int fr_req_manager(int msgid, struct npu_frame *frame);
int nw_rslt_manager(int *ret_msgid, struct npu_nw *nw);
int fr_rslt_manager(int *ret_msgid, struct npu_frame *frame, enum channel_flag c);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
int fw_msg_available(void);
#endif
int nw_rslt_available(void);
int fr_rslt_available(enum channel_flag c);
int register_msgid_get_type(int (*msgid_get_type_func)(int));
int register_rslt_notifier(protodrv_notifier);
void fw_rprt_manager(void);
int mbx_rslt_fault_listener(void);
int npu_check_unposted_mbox(int nCtrl);
void fw_rprt_gather(void);
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
int dsp_interface_send_irq(int status);
#endif
#ifdef CONFIG_NPU_KUNIT_TEST
#define dbg_print_ncp_header(x) do {} while(0)
#else
void dbg_print_ncp_header(struct ncp_header *nhdr);
#endif
#endif
