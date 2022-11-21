/*
** (C) Copyright 2009 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd. 
** under the terms of the GNU General Public License Version 2, June 1991 (the "License"). 
** You may use, redistribute and/or modify this File in accordance with the terms and 
** conditions of the License, a copy of which is available along with the File in the 
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place, 
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES 
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.  
** The License provides additional details about this warranty disclaimer.
*/

/* (C) Copyright 2009 Marvell International Ltd. All Rights Reserved */

#ifndef __PX_EVENT_TYPES_A7_H__
#define __PX_EVENT_TYPES_A7_H__

/* Refer DDI0464D_cortex_a7_mpcore_r0p3_trm.pdf. Ignored: 0x1a, 0x1b, 0x1c, 0xc7, 0xc8, which not happened */

/**
 * OS Timer
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A7_OS_TIMER.
 */
#define A7_OS_TIMER_DEFAULT_EVENT_TYPE 0xFFFF

/**
 * Core clock tick
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_CCNT.
 */
#define A7_PMU_CCNT_CORE_CLOCK_TICK 0xFF
    
/**
 * Core clock tick (64 clock base)
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_CCNT.
 */
#define A7_PMU_CCNT_CORE_CLOCK_TICK_64 0xFE
    
/**
 * Instruction architecturally executed, condition check pass, Software increment:
 *	The counter increments on writes to the PMSWINC register.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_SOFTWARE_INC  0x0

/**
 * Level 1 instruction cache refill:
 *	The counter counts instruction memory accesses that cause a TLB refill of at least the Level 1 instruction TLB. 
 *	This includes each instruction memory access that causes an access to a level of memory system due to a translation table walk or an access to another level of TLB caching.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_INST_FETECH_CAUSE_CACHE_REFILL  0x1

/**
 * Level 1 instruction TLB refill:
 *	The counter counts each instruction memory access that causes an access to a level of the memory system due to a translation table walk 
 *	or an access to another level of TLB caching. CP15 TLB maintenance operations do not count as events.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_INST_FETECH_CAUSE_TLB_REFILL    0x2

/**
 * Level 1 data cache refill:
 *	The counter counts each memory-read operation or memory-write operation that causes a refill of at least the Level 1 data or unified cache. Accesses that do not cause a new cache refill, but are satisfied from refilling data of a previous miss, are not counted. 
 *	Each access to a cache line that causes a new linefill is counted, including the multiple accesses of load or store multiples. Accesses to a cache line that generate a memory access but not a new linefill, such as Write-through writes that hit in the cache, are not counted.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_DATA_OPERATION_CAUSE_CACHE_REFILL 0x3

/**
 * Level 1 data cache access:
 *	The counter counts each memory-read operation or memory-write operation that causes a cache access to at least the Level 1 data or unified cache.
 *	Each access to a cache line is counted including the multiple accesses of instructions such as LDM or STM. Each access to other Level 1 data or unified memory structures, for example refill buffers, write buffers and write-back buffers, is also counted.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_DATA_OPERATION_CAUSE_CACHE_ACCESS 0x4

/**
 * Level 1 data TLB refill:
 *	The counter counts each memory-read or memory-write operation that causes a TLB refill of at least the Level 1 data or unified TLB. Each read or write that causes a refill, in the form of a translation table walk or an access to another level of TLB caching is counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_DATA_OPERATION_CAUSE_TLB_REFILL 0x5

/**
 * Instruction architecturally executed, condition check pass, Load:
 *	The counter increments for every executed instruction that explicitly reads data, including SWP.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_DATA_READ 0x6

/**
 * Instruction architecturally executed, condition check pass, Store:
 *	The counter increments for every executed instruction that explicitly writes data, including SWP.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_DATA_WRITE 0x7

/**
 * Instruction architecturally executed:
 *	The counter increments for every architecturally executed instruction.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_INST_EXECUTED   0x8

/**
 * Exception taken:
 *	The counter increments for each exception taken.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_EXCEPTION_TAKEN 0x9

/**
 * Instruction architecturally executed, condition check pass, Exception return:
 *	The counter increments for each executed exception return instruction.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_EXCEPTION_RETURN_EXECUTED 0xa

/**
 * Instruction architecturally executed, condition check pass, Write to CONTEXTIDR:
 *	The counter increments for every write to the CONTEXTIDR.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_INST_WRITE_CONTEXT_ID_REG_EXECUTED 0xb

/**
 * Instruction architecturally executed, condition check pass, Software change of the PC:
 *	The counter increments for every software change of the PC.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_SOFTWARE_CHANGE_OF_PC_EXECUTED 0xc

/**
 * Instruction architecturally executed - Immediate branch:
 *	The counter counts all immediate branch instructions that are architecturally executed.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_IMMEDIATE_BRANCH_EXECUTED 0xd

/**
 * Instruction architecturally executed, condition check pass, Procedure return:
 *	The counter counts the procedure return instruction.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_PROCEDURE_RETURN_EXECUTED 0xe

/**
 * Instruction architecturally executed, condition check pass, Unaligned load or store:
 *	The counter counts each instruction that accesses an unaligned address.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_UNALIGNED_ACCESS_EXECUTED 0xf


/**
 * Mispredicted or not predicted branch speculatively executed:
 *	The counter counts for each correction to the predicted program flow that occurs because of a misprediction from, or no prediction from, the branch prediction resources and that relates to instructions that the branch prediction resources are capable of predicting.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_MISPREDICTED_OR_NOT_PREDICTED 0x10

/**
 * Cycle:
 *	The counter increments on every cycle.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_CYCLE_COUNT 0x11

/**
 * Predictable branch speculatively executed:
 *	The counter counts every branch or other change in the program flow that the branch prediction resources are capable of predicting.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_COULD_HAVE_BEEN_PREDICTED 0x12

/**
 * Data memory access:
 *	The counter counts memory-read or memory-write operations that the processor made.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_DATA_MEMORY_ACCESS 0x13

/**
 * Level 1 instruction cache access:
 *	The counter counts instruction memory accesses that access at least Level 1 instruction or unified cache. Each access to other Level 1 instruction memory structures such as refill buffers, is also counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_LEVEL_1_INSTRUCTION_CACHE_ACCESS 0x14

/**
 * Level 1 data cache write-back:
 *	The counter counts every write-back of data from the Level 1 data or unified cache. The counter counts each write-back that causes data to be written from the Level 1 cache to outside of the Level 1 cache.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_LEVEL_1_DATA_CACHE_WRITE_BACK 0x15

/**
 * Level 2 data cache access:
 *	The counter counts memory-read or memory-write operations that access at least the Level 2 data or unified cache. 
 *	Each access to a cache line is counted including refills of and write-backs from the Level 1 caches.
 *	Each access to other Level 2 data or unified memory structures such as refill buffers, write buffers and write-back buffers, is also counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_LEVEL_2_DATA_CACHE_ACCESS 0x16


/**
 * Level 2 data cache refill:
 *	The counter counts memory-read or write operations that access at least the Level 2 data or unified cache and cause a refill of a Level 1 cache or of the Level 2 data or unified cache.
 *	Each read from or write to the cache that causes a refill from outside the cache is counted. Accesses that do not cause a new cache refill, but are satisfied from refilling data of a previous miss are not counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_LEVEL_2_DATA_CACHE_REFILL 0x17

/**
 * Level 2 data cache write-back:
 *	The counter counts every write-back of data from the Level 2 data or unified cache that the processor made. 
 *	Each write-back that causes data to be written from the Level 2 cache to outside the Level 1 and Level 2 caches is counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_LEVEL_2_DATA_CACHE_WRITE_BACK 0x18

/**
 * Bus Access:
 *	The counter counts memory-read or memory-write operations that access outside of the boundary of the processor and its closely-coupled caches. 
 *	Where this boundary lies with respect to any implemented caches is IMPLEMENTATION DEFINED. 
 *	It must count accesses beyond the cache furthest from the processor for which accesses can be counted.
 *	The definition of a bus access is IMPLEMENTATION DEFINED, but physically is a single beat rather than a burst. 
 *	That is, for each bus cycle for which the bus is active.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_BUS_ACCESS 0x19

/**
 * Bus cycle:
 *	The register is incremented on every cycle of the external memory interface of the processor.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
#define A7_PMU_BRANCH_BUS_CYCLE 0x1d

/**
 * Bus access, read.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_BUS_ACCESS_READ 0x60

/**
 * Bus access, write.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_BUS_ACCESS_WRITE 0x61

/**
 * IRQ exception taken.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_IRQ_EXCEPTION_TAKEN 0x86

/**
 * FIQ exception taken.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_FIQ_EXCEPTION_TAKEN 0x87

/**
 * External memory request.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_EXTERNAL_MEM_REQ 0xc0

/**
 * Non-cacheable external memory request.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_NON_CACHEABLE_EXTERNAL_MEM_REQ 0xc1

/**
 * Linefill because of prefetch.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_LINEFILL_BECAUSE_OF_PREFETCH 0xc2

/**
 * Prefetch linefill dropped.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_PREFETCH_LINEFILL_DROPPED 0xc3

/**
 * Entering read allocate mode.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_ENTERING_READ_ALLOCATE_MODE 0xc4

/**
 * Read allocate mode.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_READ_ALLOCATE_MODE 0xc5

/**
 * Data Write operation that stalls the pipeline because the store buffer is full.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_DATA_WRITE_STALLS_BECAUSE_STORE_BUFFER_FULL 0xc9

/**
 * Data snooped from other processor. This event counts memory-read operations that read data from
 * another processor within the local Cortex-A7 cluster, rather than accessing the L2 cache or issuing an
 * external read. It increments on each transaction, rather than on each beat of data.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A7_PMU_PMN0 = 2, COUNTER_A7_PMU_PMN1 = 3, COUNTER_A7_PMU_PMN2 = 4, COUNTER_A7_PMU_PMN3 = 5.
 */
 #define A7_PMU_DATA_SNOOPED_FROM_OTHER_PROCESSOR 0xca

#endif /* __PX_EVENT_TYPES_A7_H__ */

