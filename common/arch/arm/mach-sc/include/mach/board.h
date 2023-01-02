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

#ifdef	CONFIG_MACH_SP8830GEA
#include "__board-sp8830gea.h"
#endif

#ifdef	CONFIG_MACH_SP8730SEA
#include "__board-sp8730sea.h"
#endif

#ifdef	CONFIG_MACH_SP7730GGA
#include "__board-sp7730gga.h"
#endif

#ifdef	CONFIG_MACH_SP7731GGA_LC
#include "__board-sp7731gga_lc.h"
#endif

#ifdef	CONFIG_MACH_SP7730GGA_LC
#include "__board-sp7730gga_lc.h"
#endif

#ifdef	CONFIG_MACH_SP7730GGAOPENPHONE
#include "__board-sp7730ggaopenphone.h"
#endif

#ifdef	CONFIG_MACH_SP7731GEA
#include "__board-sp7731gea.h"
#endif

#ifdef	CONFIG_MACH_SP7731GEA_FWVGA
#include "__board-sp7731gea_fwvga.h"
#endif

#ifdef	CONFIG_MACH_SP7731GEAOPENPHONE
#include "__board-sp7731geaopenphone.h"
#endif

#ifdef	CONFIG_MACH_SP7731GEA_LC
#include "__board-sp7731gea_lc.h"
#endif

#ifdef	CONFIG_MACH_SP8830GA
#include "__board-sp8830ga.h"
#endif

#ifdef	CONFIG_MACH_SP7730GA
#include "__board-sp7730ga.h"
#endif

#ifdef	CONFIG_MACH_KANAS_W
#include "__board-kanas_w.h"
#endif

#ifdef	CONFIG_MACH_KANAS_TD
#include "__board-kanas_td.h"
#endif

#ifdef	CONFIG_MACH_SP7730EC
#include "__board-sp7730ec.h"
#endif

#ifdef	CONFIG_MACH_SC9620OPENPHONE
#include "__board-sc9620openphone.h"
#endif

#ifdef	CONFIG_MACH_SC9620OPENPHONE_ZT
#include "__board-sc9620openphone_zt.h"
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

#ifdef	CONFIG_MACH_SP5735C1EA
#include "__board-sp5735c1ea.h"
#endif

#if defined (CONFIG_MACH_SPX35FPGA) || defined (CONFIG_MACH_SPX15FPGA)
#include "__board-sp8830fpga.h"
#endif

#ifdef	CONFIG_MACH_SP7735EC
#include "__board-sp7735ec.h"
#endif

#if defined (CONFIG_MACH_SP7715EA) || defined(CONFIG_MACH_SPX15)
#include "__board-sp7715ea.h"
#endif

#ifdef	CONFIG_MACH_SP7715EATRISIM
#include "__board-sp7715eatrisim.h"
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

#ifdef	CONFIG_MACH_SP7715GATRISIM
#include "__board-sp7715gatrisim.h"
#endif

#ifdef	CONFIG_MACH_SP8815GA
#include "__board-sp8815ga.h"
#endif

#ifdef	CONFIG_MACH_SP8815GAOPENPHONE
#include "__board-sp8815gaopenphone.h"
#endif

#ifdef  CONFIG_MACH_POCKET2
#include "__board-pocket2.h"
#endif

#ifdef  CONFIG_MACH_CORSICA_VE
#include "__board-corsica_ve.h"
#endif

#ifdef CONFIG_MACH_VIVALTO
#include "__board-vivalto.h"
#endif

#ifdef  CONFIG_MACH_YOUNG2
#include "__board-young2.h"
#endif

#ifdef  CONFIG_MACH_HIGGS
#include "__board-higgs.h"
#endif

#ifdef	CONFIG_MACH_SHANGHAI_W
#include "__board-shanghai3g_native.h"
#endif

#ifdef  CONFIG_MACH_CORE3_W
#include "__board-core33g.h"
#endif

#ifdef  CONFIG_MACH_GRANDNEOVE3G
#include "__board-grandneove3g.h"
#endif

#ifdef	CONFIG_MACH_TSHARKWSAMSUNG
#include "__board-tsharkwsamsung.h"
#endif

#ifdef	CONFIG_MACH_YOUNG23GDTV
#include "__board-young23gdtv.h"
#endif

#ifdef	CONFIG_MACH_VIVALTO5MVE3G
#include "__board-vivalto5mve3g.h"
#endif


#ifdef	CONFIG_MACH_GOYAVE3G
#include "__board-goyave3g.h"
#endif

#ifdef	CONFIG_MACH_GOYAVEWIFI
#include "__board-goyavewifi.h"
#endif

#ifdef	CONFIG_MACH_J13G
#include "__board-j13g.h"
#endif

#include <asm/sizes.h>

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
#ifdef CONFIG_SPRD_IOMMU
	#define CMA_MARGIN		(0 * SZ_1M)
	#define CMA_RESERVE		(0 * SZ_1M)
	#define CMA_THRESHOLD		(0 * SZ_1M)
#else
	#define CMA_MARGIN		(0 * SZ_1M)
	#define CMA_RESERVE		(25 * SZ_1M)
	#define CMA_THRESHOLD		(4 * SZ_1M)
#endif
	#define CMA_ALIGNMENT		(4 * SZ_1M)
	#define SPRD_ION_MEM_RAW_SIZE	(SPRD_ION_MM_SIZE + SPRD_ION_OVERLAY_SIZE + CMA_MARGIN - CMA_RESERVE)
	/* ALIGN UP */
	#define SPRD_ION_MEM_SIZE	(((SPRD_ION_MEM_RAW_SIZE + (CMA_ALIGNMENT - 1)) & (~(CMA_ALIGNMENT - 1))) + CMA_RESERVE)
#else
	#define SPRD_ION_MEM_SIZE	(SPRD_ION_MM_SIZE + SPRD_ION_OVERLAY_SIZE)
#endif

#define SPRD_ION_BASE_USE_VARIABLE
#ifndef SPRD_ION_BASE_USE_VARIABLE
#if defined(CONFIG_MACH_SP7715GA) || defined(CONFIG_MACH_SP7715GATRISIM) || defined(CONFIG_MACH_SP8815GA) || defined(CONFIG_MACH_SP8815GAOPENPHONE) || defined(CONFIG_MACH_SP6815GA) || defined(CONFIG_MACH_SP7731GEA_LC) || defined(CONFIG_MACH_SP7731GGA_LC) || defined(CONFIG_MACH_SP7730GGA_LC)/* Nand 4+2 */
#define SPRD_ION_MEM_BASE	\
	((CONFIG_PHYS_OFFSET & (~(SZ_256M - 1))) + SZ_256M - SPRD_ION_MEM_SIZE)
#else
#define SPRD_ION_MEM_BASE	\
	((CONFIG_PHYS_OFFSET & (~(SZ_512M - 1))) + SZ_512M - SPRD_ION_MEM_SIZE)
#endif
#else
extern phys_addr_t sprd_reserve_limit;
#define SPRD_ION_MEM_BASE (sprd_reserve_limit - SPRD_ION_MEM_SIZE)
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
//#define SPRD_FB_MEM_SIZE	SZ_8M
#define SPRD_FB_MEM_SIZE	(CONFIG_SPRD_FB_SIZE*SZ_1M)
#define SPRD_FB_MEM_BASE	(SPRD_RAM_CONSOLE_START - SPRD_FB_MEM_SIZE)
#endif

#ifdef CONFIG_SPRD_IQ
#define SPRD_IQ_SIZE SZ_128M
#endif

#define SPRD_SYSDUMP_MAGIC     (SPRD_ION_MEM_BASE + SPRD_ION_MEM_SIZE - SZ_1M)

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
