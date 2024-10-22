/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 TRUSTONIC LIMITED
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

#ifndef SCHED_H
#define SCHED_H

#define MC_STATE_FLAG_TEE_HALT_MASK BIT(0)

/*
 * Initialisation values flags
 */
/* Set if IRQ is present */
#define MC_IV_FLAG_IRQ			BIT(0)
#define MC_IV_FLAG_FFA_NOTIFICATION	BIT(1)

struct init_values {
	u32	flags;
	u32	irq;
};

/** TEE status flags */
struct tee_sched {
	/** Initialisation values */
	struct init_values	init_values;
	/** Secure-world sleep timeout in milliseconds */
	s32			timeout_ms;
	/** Swd requested nb of TEE workers */
	u16			required_workers;
	/** TEE flags */
	u8			tee_flags;
	/** Reserved for future use */
	u8			RFU_padding;
};

#endif /* SCHED_H */
