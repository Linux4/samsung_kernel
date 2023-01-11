/*
 * arch/arm/common/mcpm_head.S -- kernel entry point for multi-cluster PM
 *
 * Created by:  Nicolas Pitre, March 2012
 * Copyright:   (C) 2012-2013  Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Refer to Documentation/arm/cluster-pm-race-avoidance.txt
 * for details of the synchronisation algorithms used here.
 */

#include <linux/linkage.h>
#include <asm/mcpm.h>

#include "vlock.h"

.if MCPM_SYNC_CLUSTER_CPUS
.error "cpus must be the first member of struct mcpm_sync_struct"
.endif

	.macro	pr_dbg	string
	.endm

	.macro	mem_dbg	tmp
	adr	x0, mcpm_members
	ldr	x1, [x0, #S_X4]
	add	x1, x0, x1
	mov	x0, \tmp
	str	x0, [x1, x4, lsl #3]
	.endm
	.align

ENTRY(mcpm_entry_point)
	mrs	x0, mpidr_el1			// MPIDR
	ubfx	x9, x0, #0, #8			// x9 = cpu
	ubfx	x10, x0, #8, #8			// x10 = cluster
	cmp	x10, #0xf			// single cluster
	b.ne	1f
	mov	x10, #0
1:	mov	x3, #MAX_CPUS_PER_CLUSTER
	madd	x4, x3, x10, x9			// x4 = canonical CPU index
	cmp	x4, #(MAX_CPUS_PER_CLUSTER * MAX_NR_CLUSTERS)
	b.lo	2f

2:	pr_dbg	"kernel mcpm_entry_point\n"

	/*
	 * MMU is off so we need to get to various variables in a
	 * position independent way.
	 */
	mem_dbg	#0x5a5a

	adr	x5, mcpm_members
	ldp	x6, x7, [x5]
	ldp	x8, x11, [x5, #S_X2]
	add	x6, x5, x6			// r6 = mcpm_entry_vectors
	ldr	x7, [x5, x7]			// r7 = mcpm_power_up_setup_phys
	add	x8, x5, x8			// r8 = mcpm_sync
	add	x11, x5, x11			// r11 = first_man_locks

	mov	x0, #MCPM_SYNC_CLUSTER_SIZE
	madd	x8, x0, x10, x8			// r8 = sync cluster base

	/*
	 * Signal that this CPU is coming UP:
	 */
	mov	w0, #CPU_COMING_UP
	mov	x5, #MCPM_SYNC_CPU_SIZE
	madd	x5, x9, x5, x8			// r5 = sync cpu address
	strb	w0, [x5]

	/*
	 * At this point, the cluster cannot unexpectedly enter the GOING_DOWN
	 * state, because there is at least one active CPU (this CPU).
	 */
	mov	x0, #VLOCK_SIZE
	madd	x11, x0, x10, x11		// r11 = cluster first man lock
	mov	x0, x11
	mov	x1, x9				// cpu
	bl	vlock_trylock			// implies DMB

	cmp	x0, #0				// failed to get the lock?
	b.ne	mcpm_setup_wait			// wait for cluster setup

	ldrb	w0, [x8, #MCPM_SYNC_CLUSTER_CLUSTER]
	cmp	w0, #CLUSTER_UP			// cluster already up?
	b.ne	mcpm_setup			// if not, set up the cluster

	/*
	 * Otherwise, release the first man lock and skip setup:
	 */
	mov	x0, x11
	bl	vlock_unlock
	b	mcpm_setup_complete

mcpm_setup:
	/*
	 * Control dependency implies strb not observable before previous ldrb.
	 *
	 * Signal that the cluster is being brought up:
	 */
	mov	w0, #INBOUND_COMING_UP
	strb	w0, [x8, #MCPM_SYNC_CLUSTER_INBOUND]
	dmb	ish

	/*
	 * Any CPU trying to take the cluster into CLUSTER_GOING_DOWN from this
	 * point onwards will observe INBOUND_COMING_UP and abort.
	 *
	 * Wait for any previously-pending cluster teardown operations to abort
	 * or complete:
	 */
mcpm_teardown_wait:
	ldrb	w0, [x8, #MCPM_SYNC_CLUSTER_CLUSTER]
	cmp	w0, #CLUSTER_GOING_DOWN
	b.ne	first_man_setup
	wfe
	b	mcpm_teardown_wait

first_man_setup:
	dmb	ish

	/*
	 * If the outbound gave up before teardown started, skip cluster setup:
	 */
	cmp	x0, #CLUSTER_UP
	b.eq	mcpm_setup_leave

	cmp	x7, #0
	mov	x0, #1
	b.eq	skip_power_up_setup
	br	x7

skip_power_up_setup:
	dmb	ish

	mov	w0, CLUSTER_UP
	strb	w0, [x8, #MCPM_SYNC_CLUSTER_CLUSTER]
	dmb	ish

mcpm_setup_leave:
	/*
	 * Leave the cluster setuip critical section:
	 */
	mov	w0, #INBOUND_NOT_COMING_UP
	strb	w0, [x8, #MCPM_SYNC_CLUSTER_INBOUND]
	dsb	sy
	sev

	mov	x0, x11
	bl	vlock_unlock	// implies DMB
	b	mcpm_setup_complete

	/*
	 * In the contended case, non-first men wait here for cluster setup
	 * to complete:
	 */
mcpm_setup_wait:
	ldrb	w0, [x8, #MCPM_SYNC_CLUSTER_CLUSTER]
	cmp	w0, #CLUSTER_UP
	b.eq	mcpm_setup_wait_out
	wfe
	b	mcpm_setup_wait

mcpm_setup_wait_out:
	dmb	ish

mcpm_setup_complete:
	/*
	 * If a platform-specific CPU setup hook is needed, it is
	 * called from here.
	 */
	cmp	x7, #0
	mov	x0, #0		// first (CPU) affinity level
	b.eq	3f
	blr	x7		// Call power_up_setup if defined
3:
	dmb	ish

	/*
	 * Mark the CPU as up:
	 */
	mov	w0, #CPU_UP
	strb	w0, [x5]

	/*
	 * Observability order of CPU_UP and opening of the gate does not
	 * matter.
	 */
mcpm_entry_gated:
	ldr	x5, [x6, x4, lsl #3]		// r5 = CPU entry vector

	cmp	x5, #0
	b.ne	4f
	wfe
	b	mcpm_entry_gated
4:
	dmb	ish
	pr_dbg  "released\n"
	mem_dbg	#0x5c5c

	br	x5

	.align	3

mcpm_members:
	.quad	mcpm_entry_vectors - .
	.quad	mcpm_power_up_setup_phys - mcpm_members
	.quad	mcpm_sync - mcpm_members
	.quad	first_man_locks - mcpm_members
	.quad	mcpm_dbg - mcpm_members

ENDPROC(mcpm_entry_point)

	.bss

	.align	__CACHE_WRITEBACK_ORDER
	.type	first_man_locks, #object
first_man_locks:
	.space	VLOCK_SIZE * MAX_NR_CLUSTERS
	.align	__CACHE_WRITEBACK_ORDER

	.type	mcpm_entry_vectors, #object
ENTRY(mcpm_entry_vectors)
	.space	8 * MAX_NR_CLUSTERS * MAX_CPUS_PER_CLUSTER

	.type	mcpm_power_up_setup_phys, #object
ENTRY(mcpm_power_up_setup_phys)
	.space	8

	.type	mcpm_dbg, #object
ENTRY(mcpm_dbg)
	.space	8 * MAX_NR_CLUSTERS * MAX_CPUS_PER_CLUSTER
