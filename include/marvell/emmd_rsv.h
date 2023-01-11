/*
 *  EMMD RESERVE PAGE APIs
 *
 *  Copyright (C) 2014 Marvell International Ltd.
 *  Qing Zhu <qzhu@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */
#ifndef _EMMD_RSV_H_
#define _EMMD_RSV_H_
/*
 * MRVL: the emmd reserve page share APIs to
 * below items, they are aligned with uboot's:
 * 1. emmd indicator;
 * 2. dump style: USB or SD;
 * 3. super kmalloc flags;
 * 4. ram tags info;
 * 5. held status registers in SQU;
 * 6. (eden)PMIC power up/down log;
 * 7. (eden)reset status registers;
 *
 * note: RDC for RAMDUMP is defined @0x8140400,size is 0x400, so we will
 * use 0x8140000~0x8140400 to save above info.
 */
#define CRASH_PAGE_SIZE_SKM	8
#define CRASH_PAGE_SIZE_RAMTAG	9
#define CRASH_PAGE_SIZE_HELDSTATUS	64
#define CRASH_PAGE_SIZE_PMIC	2
#define CRASH_PAGE_SIZE_RESET_STATUS	4

struct ram_tag_info {
	char name[24];
	unsigned long data;
#ifdef CONFIG_ARM
	unsigned int reserved;
#endif
};

struct emmd_page {
	unsigned int indicator;
	unsigned int dump_style;
	unsigned int p_rsvbuf[CRASH_PAGE_SIZE_SKM];
	struct ram_tag_info ram_tag[CRASH_PAGE_SIZE_RAMTAG];
	unsigned int held_status[CRASH_PAGE_SIZE_HELDSTATUS];
	unsigned int pmic_regs[CRASH_PAGE_SIZE_PMIC];
	unsigned int reset_status[CRASH_PAGE_SIZE_RESET_STATUS];
};
extern struct emmd_page *emmd_page;

#endif /* _EMMD_RSV_H_ */
