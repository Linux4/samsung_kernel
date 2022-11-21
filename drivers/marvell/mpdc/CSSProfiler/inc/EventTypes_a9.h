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

#ifndef __PX_EVENT_TYPES_A9_H__
#define __PX_EVENT_TYPES_A9_H__


/**
 * OS Timer
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_OS_TIMER.
 */
#define A9_OS_TIMER_DEFAULT_EVENT_TYPE 0xFFFF

/**
 * Core clock tick
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_CCNT.
 */
#define A9_PMU_CCNT_CORE_CLOCK_TICK 0xFF
    
/**
 * Core clock tick (64 clock base)
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_CCNT.
 */
#define A9_PMU_CCNT_CORE_CLOCK_TICK_64 0xFE
    
/**
 * Instruction architecturally executed, condition check pass, Software increment:
 *	The counter increments on writes to the PMSWINC register.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_SOFTWARE_INC  0x0

/**
 * Level 1 instruction cache refill:
 *	The counter counts instruction memory accesses that cause a TLB refill of at least the Level 1 instruction TLB. 
 *	This includes each instruction memory access that causes an access to a level of memory system due to a translation table walk or an access to another level of TLB caching.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_INST_FETECH_CAUSE_CACHE_REFILL  0x1

/**
 * Level 1 instruction TLB refill:
 *	The counter counts each instruction memory access that causes an access to a level of the memory system due to a translation table walk 
 *	or an access to another level of TLB caching. CP15 TLB maintenance operations do not count as events.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_INST_FETECH_CAUSE_TLB_REFILL    0x2

/**
 * Level 1 data cache refill:
 *	The counter counts each memory-read operation or memory-write operation that causes a refill of at least the Level 1 data or unified cache. Accesses that do not cause a new cache refill, but are satisfied from refilling data of a previous miss, are not counted. 
 *	Each access to a cache line that causes a new linefill is counted, including the multiple accesses of load or store multiples. Accesses to a cache line that generate a memory access but not a new linefill, such as Write-through writes that hit in the cache, are not counted.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_DATA_OPERATION_CAUSE_CACHE_REFILL 0x3

/**
 * Level 1 data cache access:
 *	The counter counts each memory-read operation or memory-write operation that causes a cache access to at least the Level 1 data or unified cache.
 *	Each access to a cache line is counted including the multiple accesses of instructions such as LDM or STM. Each access to other Level 1 data or unified memory structures, for example refill buffers, write buffers and write-back buffers, is also counted.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_DATA_OPERATION_CAUSE_CACHE_ACCESS 0x4

/**
 * Level 1 data TLB refill:
 *	The counter counts each memory-read or memory-write operation that causes a TLB refill of at least the Level 1 data or unified TLB. Each read or write that causes a refill, in the form of a translation table walk or an access to another level of TLB caching is counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_DATA_OPERATION_CAUSE_TLB_REFILL 0x5

/**
 * Instruction architecturally executed, condition check pass, Load:
 *	The counter increments for every executed instruction that explicitly reads data, including SWP.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_DATA_READ 0x6

/**
 * Instruction architecturally executed, condition check pass, Store:
 *	The counter increments for every executed instruction that explicitly writes data, including SWP.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_DATA_WRITE 0x7

/**
 * Instruction architecturally executed:
 *	The counter increments for every architecturally executed instruction.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_INST_EXECUTED   0x8

/**
 * Exception taken:
 *	The counter increments for each exception taken.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_EXCEPTION_TAKEN 0x9

/**
 * Instruction architecturally executed, condition check pass, Exception return:
 *	The counter increments for each executed exception return instruction.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_EXCEPTION_RETURN_EXECUTED 0xa

/**
 * Instruction architecturally executed, condition check pass, Write to CONTEXTIDR:
 *	The counter increments for every write to the CONTEXTIDR.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_INST_WRITE_CONTEXT_ID_REG_EXECUTED 0xb

/**
 * Instruction architecturally executed, condition check pass, Software change of the PC:
 *	The counter increments for every software change of the PC.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_SOFTWARE_CHANGE_OF_PC_EXECUTED 0xc

/**
 * Instruction architecturally executed - Immediate branch:
 *	The counter counts all immediate branch instructions that are architecturally executed.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_IMMEDIATE_BRANCH_EXECUTED 0xd

/**
 * Instruction architecturally executed, condition check pass, Procedure return:
 *	The counter counts the procedure return instruction.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_PROCEDURE_RETURN_EXECUTED 0xe

/**
 * Instruction architecturally executed, condition check pass, Unaligned load or store:
 *	The counter counts each instruction that accesses an unaligned address.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_UNALIGNED_ACCESS_EXECUTED 0xf


/**
 * Mispredicted or not predicted branch speculatively executed:
 *	The counter counts for each correction to the predicted program flow that occurs because of a misprediction from, or no prediction from, the branch prediction resources and that relates to instructions that the branch prediction resources are capable of predicting.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_MISPREDICTED_OR_NOT_PREDICTED 0x10

/**
 * Cycle:
 *	The counter increments on every cycle.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_CYCLE_COUNT 0x11

/**
 * Predictable branch speculatively executed:
 *	The counter counts every branch or other change in the program flow that the branch prediction resources are capable of predicting.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_COULD_HAVE_BEEN_PREDICTED 0x12

/**
 * Data memory access:
 *	The counter counts memory-read or memory-write operations that the processor made.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_DATA_MEMORY_ACCESS 0x13

/**
 * Level 1 instruction cache access:
 *	The counter counts instruction memory accesses that access at least Level 1 instruction or unified cache. Each access to other Level 1 instruction memory structures such as refill buffers, is also counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_LEVEL_1_INSTRUCTION_CACHE_ACCESS 0x14

/**
 * Level 1 data cache write-back:
 *	The counter counts every write-back of data from the Level 1 data or unified cache. The counter counts each write-back that causes data to be written from the Level 1 cache to outside of the Level 1 cache.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_LEVEL_1_DATA_CACHE_WRITE_BACK 0x15

/**
 * Level 2 data cache access:
 *	The counter counts memory-read or memory-write operations that access at least the Level 2 data or unified cache. 
 *	Each access to a cache line is counted including refills of and write-backs from the Level 1 caches.
 *	Each access to other Level 2 data or unified memory structures such as refill buffers, write buffers and write-back buffers, is also counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_LEVEL_2_DATA_CACHE_ACCESS 0x16


/**
 * Level 2 data cache refill:
 *	The counter counts memory-read or write operations that access at least the Level 2 data or unified cache and cause a refill of a Level 1 cache or of the Level 2 data or unified cache.
 *	Each read from or write to the cache that causes a refill from outside the cache is counted. Accesses that do not cause a new cache refill, but are satisfied from refilling data of a previous miss are not counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_LEVEL_2_DATA_CACHE_REFILL 0x17

/**
 * Level 2 data cache write-back:
 *	The counter counts every write-back of data from the Level 2 data or unified cache that the processor made. 
 *	Each write-back that causes data to be written from the Level 2 cache to outside the Level 1 and Level 2 caches is counted.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_LEVEL_2_DATA_CACHE_WRITE_BACK 0x18

/**
 * Bus Access:
 *	The counter counts memory-read or memory-write operations that access outside of the boundary of the processor and its closely-coupled caches. 
 *	Where this boundary lies with respect to any implemented caches is IMPLEMENTATION DEFINED. 
 *	It must count accesses beyond the cache furthest from the processor for which accesses can be counted.
 *	The definition of a bus access is IMPLEMENTATION DEFINED, but physically is a single beat rather than a burst. 
 *	That is, for each bus cycle for which the bus is active.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_BUS_ACCESS 0x19

/**
 * Local memory error:
 *	The counter counts every occurrence of a memory error signaled by a memory closely coupled to this processor. 
 *	The definition of local memories is IMPLEMENTATION DEFINED, but typically includes caches, tightly-coupled memories, and TLB arrays.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_LOCAL_MEMORY_ERROR 0x1a

/**
 * Instruction speculatively executed:
 *	The counter counts instructions that are speculatively executed by the processor. This includes instructions that are subsequently not architecturally executed.
 *	As a result, this event counts a larger number of instructions than the number of instructions architecturally executed. The definition of speculatively executed is IMPLEMENTATION DEFINED.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_INSTRUCTION_SPECULATIVELY_EXECUTED 0x1b

/**
 * Instruction architecturally executed, condition check pass, Write to translation table base:
 *	The counter counts every write to a translation table base register, TTBR0 or TTBR1.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_WRITE_TO_TTBR0_OR_TTBR1 0x1c

/**
 * Bus cycle:
 *	The register is incremented on every cycle of the external memory interface of the processor.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
#define A9_PMU_BRANCH_BUS_CYCLE 0x1d

/**
 * Java bytecode execute:
 *	Counts the number of Java bytecodes being decoded, including speculative ones.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_JAVA_BYTECODE_EXECUTE 0x40

/**
 * Software Java bytecode executed:
 *	Counts the number of software java bytecodes being decoded, including speculative ones.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_SOFTWARE_JAVA_BYTECODE_EXECUTED 0x41

/**
 * Jazelle backward branches executed:
 *	Counts the number of Jazelle taken branches being executed. This includes the branches that are flushed because of a previous load/store which aborts late.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_JAZELLE_BACKWARD_BRANCHES_EXECUTED 0x42

/**
 * Coherent linefill miss:
 *	Counts the number of coherent linefill requests performed by the Cortex-A9 processor which also miss in all the other Cortex-A9 processors, 
 *	meaning that the request is sent to the external memory.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_COHERENT_LINEFILL_MISS 0x50

/**
 * Coherent linefill hit:
 *	Counts the number of coherent linefill requests performed by the Cortex-A9 processor which hit in another Cortex-A9 processor,
 *	meaning that the linefill data is fetched directly from the relevant Cortex-A9 cache.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_COHERENT_LINEFILL_HIT 0x51

/**
 * Instruction cache dependent stall cycles:
 *	Counts the number of cycles where the processor is ready to accept new instructions,
 *	but does not receive any because of the instruction side not being able to provide any 
 *	and the instruction cache is currently performing at least one linefill.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_INSTRUCTION_CACHE_DEPENDENT_STALL 0x60

/**
 * Data cache dependent stall cycles:
 *	Counts the number of cycles where the core has some instructions that it cannot issue to any pipeline,
 *	and the Load Store unit has at least one pending linefill request, and no pending TLB requests.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_DATA_CACHE_DEPENDENT_STALL 0x61

/**
 * Main TLB miss stall cycles:
 *	Counts the number of cycles where the processor is stalled waiting for the completion of translation table walks from the main TLB. 
 *	The processor stalls can be because of the instruction side not being able to provide the instructions,
 *	or to the data side not being able to provide the necessary data, because of them waiting for the main TLB translation table walk to complete.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_MAIN_TLB_MISS_STALL 0x62

/**
 * STREX passed:
 *	Counts the number of STREX instructions architecturally executed and passed.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_STREX_PASSED 0x63

/**
 * STREX failed:
 *	Counts the number of STREX instructions architecturally executed and failed.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_STREX_FAILED 0x64

/**
 * Data eviction:
 *	Counts the number of eviction requests because of a linefill in the data cache.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_DATA_EVICTION 0x65

/**
 * Issue does not dispatch any instruction:
 *	Counts the number of cycles where the issue stage does not dispatch any instruction because it is empty or cannot dispatch any instructions.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_ISSUE_NOT_DISPATCH_INSTRUCTION 0x66

/**
 * Issue is empty:
 *	Counts the number of cycles where the issue stage is empty.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_ISSUE_IS_EMPTY 0x67

/**
 * Instructions coming out of the core renaming stage:
 *	Counts the number of instructions going through the Register Renaming stage. This number is an approximate number of the total number of instructions speculatively executed,
 *	and even more approximate of the total number of instructions architecturally executed. The approximation depends mainly on the branch misprediction rate.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_instructions through Register Renaming stage 0x68

/**
 * Predictable function returns:
 *	Counts the number of procedure returns whose condition codes do not fail, excluding all returns from exception. 
 *	This count includes procedure returns which are flushed because of a previous load/store which aborts late.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_Predictable function returns 0x6e

/**
 * Main execution unit instructions:
 *	Counts the number of instructions being executed in the main execution pipeline of the processor, the multiply pipeline and arithmetic logic unit pipeline. The counted instructions are still speculative.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_MAIN_EXECUTION_UNIT_INSTRUCTIONS 0x70

/**
 * Second execution unit instructions:
 *	Counts the number of instructions being executed in the processor second execution pipeline (ALU). The counted instructions are still speculative.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_SECOND_EXECUTION_UNIT_INSTRUCTIONS 0x71

/**
 * Load/Store Instructions:
 *	Counts the number of instructions being executed in the Load/Store unit. The counted instructions are still speculative.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_LOAD_STORE_INSTRUCTIONS 0x72

/**
 * Floating-point instructions:
 *	Counts the number of Floating-point instructions going through the Register Rename stage. Instructions are still speculative in this stage.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_FLOATING_POINT_INSTRUCTIONS 0x73

/**
 * NEON instructions:
 *	Counts the number of NEON instructions going through the Register Rename stage. Instructions are still speculative in this stage.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_NEON_INSTRUCTIONS 0x74

/**
 * Processor stalls because of PLDs:
 *	Counts the number of cycles where the processor is stalled because PLD slots are all full.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PROCESSOR_STALLS_DUE_TO_PLDS 0x80

/**
 * Processor stalled because of a write to memory:
 *	Counts the number of cycles when the processor is stalled and the data side is stalled too because it is full and executing writes to the external memory.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PROCESSOR_STALLED_DUE_TO_WRITE_MEMORY 0x81

/**
 * Processor stalled because of instruction side main TLB miss:
 *	Counts the number of stall cycles because of main TLB misses on requests issued by the instruction side.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PROCESSOR_STALLED_DUE_TO_ITLB_MISS 0x82

/**
 * Processor stalled because of data side main TLB miss:
 *	Counts the number of stall cycles because of main TLB misses on requests issued by the data side.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PROCESSOR_STALLED_DUE_TO_DTLB_MISS 0x83
/**
 * Processor stalled because of instruction micro TLB miss:
 *	Counts the number of stall cycles because of micro TLB misses on the instruction side. 
 *	This event does not include main TLB miss stall cycles that are already counted in the corresponding main TLB event.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PROCESSOR_STALLED_DUE_TO_MITLB_MISS 0x84
/**
 * Processor stalled because of data micro TLB miss:
 *	Counts the number of stall cycles because of micro TLB misses on the data side. This event does not include main TLB miss stall cycles that are already counted in the corresponding main TLB event.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PROCESSOR_STALLED_DUE_TO_MDTLB_MISS 0x85
/**
 * Processor stalled because of DMB:
 *	Counts the number of stall cycles because of the execution of a DMB memory barrier. This includes all DMB instructions being executed, even speculatively.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PROCESSOR_DUE_TO_DMB 0x86
/**
 * Integer clock enabled:
 *	Counts the number of cycles during which the integer core clock is enabled.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_INTEGER_CLOCK_ENABLED 0x8a
/**
 * Data Engine clock enabled:
 *	Counts the number of cycles during which the Data Engine clock is enabled.
 * Supported profilers: counter monitor.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_DATA_ENGINE_CLOCK_ENABLED 0x8b

/**
 * ISB instructions:
 *	Counts the number of ISB instructions architecturally executed.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_ISB_INSTRUCTIONS 0x90

/**
 * DSB instructions:
 *	Counts the number of DSB instructions architecturally executed.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_DSB_INSTRUCTIONS 0x91

/**
 * DMB instructions:
 *	Counts the number of DMB instructions speculatively executed.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_DMB_INSTRUCTIONS 0x92

/**
 * External interrupts:
 *	Counts the number of external interrupts executed by the processor.
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_EXTERNAL_INTERRUPTS 0x93

/**
 * PLE cache line request completed
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PLE_CACHE_REQUEST_COMPLETED 0xa0

/**
 * PLE cache line request skipped
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PLE_CACHE_REQUEST_SKIPPED 0xa1

/**
 * PLE FIFO flush
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PLE_FIFO_FLUSH 0xa2

/**
 * PLE request completed
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PLE_REQUEST_COMPLETED 0xa3

/**
 * PLE FIFO overflow:
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PLE_FIFO_OVERFLOW 0xa4

/**
 * PLE request programmed
 * Supported profilers: counter monitor, hot spot, call stack.
 * Available counter ids: COUNTER_A9_PMU_PMN0 = 2, COUNTER_A9_PMU_PMN1 = 3, COUNTER_A9_PMU_PMN2 = 4, COUNTER_A9_PMU_PMN3 = 5, COUNTER_A9_PMU_PMN4 = 6, COUNTER_A9_PMU_PMN5 = 7.
 */
 #define A9_PMU_GENERAL_PLE_REQUEST_PROGRAMMED 0xa5

/* D-Cache Read Hit */
// #define A9_PMU_DCACHE_READ_HIT 0x40

/* D-Cache Read Miss */
// #define A9_PMU_DCACHE_READ_MISS 0x41

/* D-Cache Write Hit */
// #define A9_PMU_DCACHE_WRITE_HIT 0x42

/* D-Cache Write Miss */
/// #define A9_PMU_DCACHE_WRITE_MISS 0x43

/* MMU Bus Request */
///#define A9_PMU_MMU_BUS_REQUEST 0x44

/* I-Cache Bus Request */
///#define A9_PMU_ICACHE_BUS_REQUEST 0x45

/* WB write latency */
///#define A9_PMU_WB_WRITE_LATENCY 0x46

/* Hold LDM/STM */
///#define A9_PMU_HOLD_LDM_STM 0x47

/* No Dual cflag */
///#define A9_PMU_NO_DUAL_CFLAG 0x48

/* No Dual Register Plus */
///#define A9_PMU_NO_DUAL_REGISTER_PLUS 0x49

/* LDST ROB0 on Hold */
///#define A9_PMU_LDST_ROB0_ON_HOLD 0x4a

/* LDST ROB1 on Hold */
///#define A9_PMU_LDST_ROB1_ON_HOLD 0x4b

/* Data Write Access Count */
///#define A9_PMU_DATA_WRITE_ACCESS_COUNT 0x4c

/* Data Read Access Count */
///#define A9_PMU_DATA_READ_ACCESS_COUNT 0x4d

/* A2 Stall */
///#define A9_PMU_A2_STALL 0x4e

/* L2 Cache Write Hit */
///#define A9_PMU_L2_CACHE_WRITE_HIT 0x4f

/* L2 Cache Write Miss */
//#define A9_PMU_L2_CACHE_WRITE_MISS 0x50

/* L2 Cache Read Count */
//#define A9_PMU_L2_CACHE_READ_COUNT 0x51

/* I-Cache Read Miss */
//#define A9_PMU_ICACHE_READ_MISS 0x60

/* ITLB Miss */
//#define A9_PMU_ITLB_MISS 0x61

/* Single Issue */
//#define A9_PMU_SINGLE_ISSUE 0x62

/* Branch Retired */
//#define A9_PMU_BRANCH_RETIRED 0x63

/* ROB Full */
//#define A9_PMU_ROB_FULL 0x64

/* MMU Read Beat */
//#define A9_MMU_READ_BEAT 0x65

/* WB Write Beat */
//#define A9_PMU_WB_WRITE_BEAT 0x66

/* Dual Issue */
//#define A9_PMU_DUAL_ISSUE 0x67

/* No Dual raw */
//#define A9_PMU_NO_DUAL_RAW 0x68

/* Hold IS */
//#define A9_PMU_HOLD_IS 0x69

/* L2 Cache Latency */
//#define A9_PMU_L2_CACHE_LATENCY 0x6a

/* D-Cache Access */
//#define A9_PMU_DCACHE_ACCESS 0x70

/* DTLB Miss */
//#define A9_PMU_DTLB_MISS 0x71

/* Branch Prediction Miss */
//#define A9_PMU_BRANCH_PREDICTION_MISS 0x72

/* A1 Stall */
//#define A9_PMU_A1_STALL 0x74

/* D-Cache Read Latency */
//#define A9_PMU_DCACHE_READ_LATENCY 0x75

/* D-Cache Write Latency */
//#define A9_PMU_DCACHE_WRITE_LATENCY 0x76

/* No Dual Register File */
//#define A9_PMU_NO_DUAL_REGISTER_FILE 0x77

/* BIU Simultaneous Access */
//#define A9_PMU_BIU_SIMULTANEOUS_ACCESS 0x78

/* L2 Cache Read Hit */
//#define A9_PMU_L2_CACHE_READ_HIT 0x79

/* L2 Cache Read Miss */
//#define A9_PMU_L2_CACHE_READ_MISS 0x7a


/* L2 Cache Eviction */
//#define A9_PMU_L2_CACHE_EVICTION 0x7b

/* TLB Miss */
//#define A9_PMU_TLB_MISS 0x80

/* Branches Taken */
//#define A9_PMU_BRANCH_TAKEN 0x81

/* WB Full */
//#define A9_PMU_WB_FULL 0x82

/* D-Cache Read Beat */
//#define A9_PMU_DCACHE_READ_BEAT 0x83

/* D-Cache Write Beat */
//#define A9_PMU_DCACHE_WRITE_BEAT 0x84

/* No Dual HW */
//#define A9_PMU_NO_DUAL_HW 0x85

/* No Dual Multiple */
//#define A9_PMU_NO_DUAL_MULTIPLE 0x86

/* BIU Any Access */
//#define A9_PMU_BIU_ANY_ACCESS 0x87

/* Main TLB refill caused by I-Cache */
//#define A9_PMU_MAIN_TLB_REFILL_BY_ICACHE 0x88

/* Main TLB refill caused by D-Cache */
//#define A9_PMU_MAIN_TLB_REFILL_BY_DCACHE 0x89

/* I-Cache read beat */
//#define A9_PMU_ICACHE_READ_BEAT 0x8a

/* Counts any event from external input source PMUEXTIN[0] */
//#define A9_PMU_COUNT_ANY_EVENT_FROM_EXTERNAL_INPUT_SOURCE_PMUEXTIN0 0x90

/* Counts any event from external input source PMUEXTIN[1] */
//#define A9_PMU_COUNT_ANY_EVENT_FROM_EXTERNAL_INPUT_SOURCE_PMUEXTIN1 0x91


/* Counts any event from both external input source PMUEXTIN[0] and PMUEXTIN[1]*/
//#define A9_PMU_COUNT_ANY_EVENT_FROM_EXTERNAL_INPUT_SOURCE_PMUEXTIN0_PMUEXTIN1 0x92


/* WMMX2 store FIFO full */
//#define A9_PMU_WMMX2_STORE_FIFO_FULL 0xc0

/* WMMX2 finish FIFO full */
//#define A9_PMU_WMMX2_FINISH_FIFO_FULL 0xc1

/* WMMX2 instruction FIFO full */
//#define A9_PMU_WMMX2_INST_FIFO_FULL 0xc2

/* WMMX2 instruction retired */
//#define A9_PMU_WMMX2_INST_RETIRED 0xc3

/* WMMX2 Busy */
//#define A9_PMU_WMMX2_BUSY 0xc4

/* WMMX2 Hold MI */
//#define A9_PMU_WMMX2_HOLD_MI 0xc5

/* WMMX2 Hold MW */
//#define A9_PMU_WMMX2_HOLD_MW 0xc6

/* L0IC line fill */
//#define A9_PMU_L0IC_LINE_FILL 0xf0

/* L0IC hit prefetch buffer */
//#define A9_PMU_L0IC_HIT_PREFETECH_BUFFER 0xf1

#endif /* __PX_EVENT_TYPES_A9_H__ */

