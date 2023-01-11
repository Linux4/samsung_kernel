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
 * Filename     : osa_os_inc.h
 * Author       : Dafu Lv
 * Date Created : 04/06/08
 * Description  : specifiec os files of osa for linux
 *
 */

#ifndef _OSA_OS_INC_H_
#define _OSA_OS_INC_H_

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))

#include <stdarg.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <linux/atomic.h>
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <linux/uaccess.h>

#define OSA_EXPORT_SYMBOL       EXPORT_SYMBOL
#define INFINITE                (0xFFFFFFFF)

typedef uint32_t bool;

#define true                    (1)
#define false                   (0)

typedef signed long long_t;
typedef unsigned long ulong_t;

#else

#include <stdarg.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/nvram.h>

#include <asm/cacheflush.h>
#include <linux/io.h>
#include <linux/mmu_context.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/sizes.h>
#include <linux/atomic.h>
#include <asm/pgtable.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/signal.h>
#include <asm/io.h>
#include <asm/memory.h>

#define OSA_EXPORT_SYMBOL       EXPORT_SYMBOL
#define INFINITE                (0xFFFFFFFF)

#define true                    (1)
#define false                   (0)

typedef signed long long_t;
typedef unsigned long ulong_t;

#endif

#endif /* _OSA_OS_INC_H_ */
