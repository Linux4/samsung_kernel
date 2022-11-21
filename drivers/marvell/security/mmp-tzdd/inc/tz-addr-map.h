/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */
#ifndef __TZ_ADDR_MAP_H__
#define __TZ_ADDR_MAP_H__

#define RA_PHYS_ADDR    (0x00000000)
#define RA_SIZE         (0x00600000)

#define RB_PHYS_ADDR    (0x00600000)
#define RB_SIZE         (0x00100000)

#define RC_PHYS_ADDR    (0x00700000)
#define RC_SIZE         (0x00100000)

#ifdef TEE_PERF_MEASURE
#define PERF_BUF_ADDR   (0x00800000)
#define PERF_BUF_SIZE   (0x00800000)
#endif

#endif /* __TZ_ADDR_MAP__ */
