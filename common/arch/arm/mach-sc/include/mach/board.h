/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_BOARD_H
#define __ASM_ARCH_BOARD_H

#ifdef	CONFIG_MACH_SP8825EA
#include "__board-sp8825ea.h"
#endif

#ifdef	CONFIG_MACH_SPX35EA
#include "__board-sp8830ea.h"
#endif

#ifdef	CONFIG_MACH_SPX35EB
#include "__board-sp8830eb.h"
#endif

#ifdef	CONFIG_MACH_SP8835EB
#include "__board-sp8835eb.h"
#endif

#ifdef	CONFIG_MACH_SPX35EC
#include "__board-sp8830ec.h"
#endif

#ifdef	CONFIG_MACH_SP8830GA
#include "__board-sp8830ga.h"
#endif

#ifdef	CONFIG_MACH_SP7730GA
#include "__board-sp7730ga.h"
#endif

#ifdef	CONFIG_MACH_SP8830SSW
#include "__board-sp8830ssw.h"
#endif

#ifdef	CONFIG_MACH_SP7730EC
#include "__board-sp7730ec.h"
#endif

#ifdef	CONFIG_MACH_SC9620OPENPHONE
#include "__board-sc9620openphone.h"
#endif

#ifdef  CONFIG_MACH_SC9620FPGA
#include "__board-sc9620fpga.h"
#endif

#ifdef	CONFIG_MACH_SP7730ECTRISIM
#include "__board-sp7730ectrisim.h"
#endif

#ifdef	CONFIG_MACH_SP5735EA
#include "__board-sp5735ea.h"
#endif

#if defined (CONFIG_MACH_SPX35FPGA) || defined (CONFIG_MACH_SPX15FPGA)
#include "__board-sp8830fpga.h"
#endif

#ifdef	CONFIG_MACH_SP7735EC
#include "__board-sp7735ec.h"
#endif

#ifdef	CONFIG_MACH_SP7715EA
#include "__board-sp7715ea.h"
#endif

#ifdef	CONFIG_MACH_SP7715EAOPENPHONE
#include "__board-sp7715eaopenphone.h"
#endif

#ifdef	CONFIG_MACH_SP6815GA
#include "__board-sp6815ga.h"
#endif
#ifdef	CONFIG_MACH_SP7715GA
#include "__board-sp7715ga.h"
#endif

#ifdef	CONFIG_MACH_SP8815GA
#include "__board-sp8815ga.h"
#endif

#ifdef  CONFIG_MACH_POCKET2
#include "__board-pocket23g.h"
#endif
#include <asm/sizes.h>

#ifdef  CONFIG_MACH_CORSICA_VE
#include "__board-corsicave3g.h"
#endif

#ifdef  CONFIG_MACH_YOUNG2
#include "__board-young2.h"
#endif

#ifdef  CONFIG_MACH_VIVALTO
#include "__board-vivalto3g.h"
#endif

#ifdef  CONFIG_MACH_HIGGS
#include "__board-higgs2g.h"
#endif

#ifdef CONFIG_SPRD_IOMMU
    #define SPRD_H264_DECODE_SIZE	(0 * SZ_1M)
#else
    #define SPRD_H264_DECODE_SIZE	(25 * SZ_1M)
#endif

#ifdef CONFIG_ION
	#ifdef CONFIG_ARCH_SCX35
		#ifdef CONFIG_SPRD_IOMMU
			#define SPRD_ION_MM_SIZE   (0 * SZ_1M)
		#else
			#ifndef SPRD_ION_MM_SIZE       /* should be defined in "__board-sp**.h" already */
				#define SPRD_ION_MM_SIZE   (76 * SZ_1M)
			#endif
		#endif

		#ifndef SPRD_ION_OVERLAY_SIZE	/* should be defined in "__board-sp**.h" already */
			#define SPRD_ION_OVERLAY_SIZE	(12 * SZ_1M)
		#endif
	#else
		#ifndef SPRD_ION_MM_SIZE
			#define SPRD_ION_MM_SIZE	(CONFIG_SPRD_ION_MM_SIZE * SZ_1M)
		#endif

		#ifndef SPRD_ION_OVERLAY_SIZE
			#define SPRD_ION_OVERLAY_SIZE	(CONFIG_SPRD_ION_OVERLAY_SIZE * SZ_1M)
		#endif
	#endif
#else /* !ION */
	#define SPRD_ION_MM_SIZE           (0 * SZ_1M)
	#define SPRD_ION_OVERLAY_SIZE   (0 * SZ_1M)
#endif

#ifdef CONFIG_CMA
       #define CMA_MARGIN              (0 * SZ_1M)
       #define CMA_ALIGNMENT           (4 * SZ_1M)
       #define SPRD_ION_MEM_RAW_SIZE    (SPRD_ION_MM_SIZE + SPRD_ION_OVERLAY_SIZE + CMA_MARGIN)
	/* ALIGN UP */
       #define SPRD_ION_MEM_SIZE        ((SPRD_ION_MEM_RAW_SIZE + (CMA_ALIGNMENT - 1)) & (~(CMA_ALIGNMENT - 1)))
#else
       #define SPRD_ION_MEM_SIZE        (SPRD_ION_MM_SIZE + SPRD_ION_OVERLAY_SIZE)
#endif

#if defined(CONFIG_MACH_SP7715GA) || defined(CONFIG_MACH_SP7715GATRISIM) || defined(CONFIG_MACH_SP8815GA) || defined(CONFIG_MACH_SP8815GAOPENPHONE) || defined(CONFIG_MACH_SP6815GA)/* Nand 4+2 */
#define SPRD_ION_MEM_BASE	\
	((CONFIG_PHYS_OFFSET & (~(SZ_256M - 1))) + SZ_256M - SPRD_ION_MEM_SIZE)
#else
#define SPRD_ION_MEM_BASE	\
	((CONFIG_PHYS_OFFSET & (~(SZ_512M - 1))) + SZ_512M - SPRD_ION_MEM_SIZE)
#endif

#define SPRD_ION_MM_BASE	(SPRD_ION_MEM_BASE)
#define SPRD_ION_OVERLAY_BASE	(SPRD_ION_MEM_BASE + SPRD_ION_MM_SIZE)

#ifdef CONFIG_PSTORE_RAM
#define SPRD_RAM_CONSOLE_SIZE	0x20000
#define SPRD_RAM_CONSOLE_START	(SPRD_ION_MEM_BASE - SPRD_RAM_CONSOLE_SIZE)
#else
#define SPRD_RAM_CONSOLE_START	(SPRD_ION_MEM_BASE)
#endif

#ifdef CONFIG_FB_LCD_RESERVE_MEM
#define SPRD_FB_MEM_SIZE	SZ_8M
#define SPRD_FB_MEM_BASE	(SPRD_RAM_CONSOLE_START - SPRD_FB_MEM_SIZE)
#endif

#define SPRD_SYSDUMP_MAGIC	(SPRD_ION_MEM_BASE + SPRD_ION_MEM_SIZE - SZ_1M)

struct sysdump_mem {
	unsigned long paddr;
	unsigned long vaddr;
	unsigned long soff;
	size_t size;
	int type;
};

enum sysdump_type {
	SYSDUMP_RAM,
	SYSDUMP_MODEM,
	SYSDUMP_IOMEM,
};

extern int sprd_dump_mem_num;
extern struct sysdump_mem sprd_dump_mem[];

#endif
