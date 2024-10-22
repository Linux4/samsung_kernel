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

#ifndef MCI_HEADER_H
#define MCI_HEADER_H

/** MCI header */
struct mci_header {
	u32	version;
	u32	size;
	u32	nq_offset;
	u32	nq_size;
	u32	mcp_offset;
	u32	mcp_size;
	u32	iwp_offset;
	u32	iwp_size;
	u32	time_offset;
	u32	time_size;
};

#endif /* MCI_HEADER_H */
