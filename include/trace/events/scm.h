/* Copyright (c) 2016, 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM scm

#if !defined(_TRACE_SCM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCM_H
#include <linux/types.h>
#include <soc/qcom/scm.h>
#include <linux/tracepoint.h>
#include <soc/qcom/secure_buffer.h>

TRACE_EVENT(scm_call_start,

	TP_PROTO(u64 x0, struct scm_desc *p),

	TP_ARGS(x0, p),

	TP_STRUCT__entry(
		__field(u64, x0)
		__field(u32, arginfo)
		__array(u64, args, MAX_SCM_ARGS)
		__field(u64, x5)
	),

	TP_fast_assign(
		__entry->x0		= x0;
		__entry->arginfo	= p->arginfo;
		memcpy(__entry->args, p->args, sizeof(__entry->args));
		__entry->x5		= p->x5;
	),

	TP_printk("func id=%#llx (args: %#x, %#llx, %#llx, %#llx, %#llx)",
		__entry->x0, __entry->arginfo, __entry->args[0],
		__entry->args[1], __entry->args[2], __entry->x5)
);


TRACE_EVENT(scm_call_end,

	TP_PROTO(struct scm_desc *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__array(u64, ret, MAX_SCM_RETS)
	),

	TP_fast_assign(
		memcpy(__entry->ret, p->ret, sizeof(__entry->ret));
	),

	TP_printk("ret: %#llx, %#llx, %#llx",
		__entry->ret[0], __entry->ret[1], __entry->ret[2])
);

TRACE_EVENT(hyp_assign_table,

	TP_PROTO(void *buf),

	TP_ARGS(buf),

	TP_STRUCT__entry(
		__field(u64, buf)
	),

	TP_fast_assign(
		__entry->buf = (unsigned long) buf;
	),

	TP_printk("Caller 2: %pS", __entry->buf)
);

#define MAX_TRACE_VM			0x3
#define GET_DEST_INFO(dst_vm, dst_perm, vm_info, dest_nelems, i)	\
	i = 0;								\
	do {								\
		dst_vm[i] = vm_info[i].vm;				\
		dst_perm[i] = vm_info[i].perm;				\
		i++;							\
	} while (i < dest_nelems && i < MAX_TRACE_VM)

TRACE_EVENT(phys_assign,

	TP_PROTO(struct mem_prot_info *info, u32 src_vm,
			struct dest_vm_and_perm_info *dest_vm,
			int dest_nelems, int ret),

	TP_ARGS(info, src_vm, dest_vm, dest_nelems, ret),

	TP_STRUCT__entry(
		__field(u64, addr)
		__field(u64, size)
		__field(u32, src_vm)
		__array(int, dst_vm, MAX_TRACE_VM)
		__array(int, dst_perm, MAX_TRACE_VM)
		__field(int, dest_nelems)
		__field(int, ret)
		__field(int, idx)
	),

	TP_fast_assign(
		__entry->addr	= info->addr;
		__entry->size	= info->size;
		__entry->ret	= ret;
		__entry->src_vm = src_vm;
		__entry->dest_nelems = dest_nelems;
		memset(__entry->dst_vm, 0, sizeof(__entry->dst_vm));
		memset(__entry->dst_perm, 0, sizeof(__entry->dst_perm));
		GET_DEST_INFO(__entry->dst_vm, __entry->dst_perm, dest_vm,
				dest_nelems, __entry->idx);
	),

	TP_printk("phys_addr = %#llx, size = %#llx, ret = %d, srcVM = %u, dstVMs = %d %d %d, dstVM perms = %d %d %d, dst size = %d",
		__entry->addr, __entry->size, __entry->ret, __entry->src_vm,
		__entry->dst_vm[0], __entry->dst_vm[1], __entry->dst_vm[2],
		__entry->dst_perm[0], __entry->dst_perm[1],
		__entry->dst_perm[2], __entry->dest_nelems)
);
#endif /* _TRACE_SCM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
