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

#ifndef __PX_EVENT_TYPES_PJ4_H__
#define __PX_EVENT_TYPES_PJ4_H__


/**
 * OS Timer event.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ4_OS_TIMER = 0.
 */
#define PJ4_OS_TIMER_DEFAULT_EVENT_TYPE 0xFFFF

/**
 * CCNT event.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_CCNT = 1. 
 */
#define PJ4_PMU_CCNT_CORE_CLOCK_TICK 0xFF
    
/**
 * CCNT 64 cycle event.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_CCNT = 1. 
 */
#define PJ4_PMU_CCNT_CORE_CLOCK_TICK_64 0xFE
    
/* Software increment */
// #define PJ4_PMU_SOFTWARE_INC  0x0

/**
 * L1 I-Cache miss:
 *	Each instruction fetch from normal cacheable memory that causes a refill from outside of the cache is counted. 
 *	Accesses that do not cause a new cache refill but are satisfied from refilling data of a previous miss are not counted. 
 *	Where instruction fetches consist of multiple instructions, these accesses count as single events. CP15 cache maintenance operations do not count as events. 
 *	This counter increments for speculative instruction fetches and for fetches of instructions that reach execution.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_INST_FETECH_CAUSE_CACHE_REFILL  0x1

/**
 * I-TLB miss:
 *	Instruction fetch that causes a TLB refill at the lowest level of TLB.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_INST_FETECH_CAUSE_TLB_REFILL    0x2

/**
 * L1 D-Cache miss:
 *	Data read or write operation that causes a refill at the lowest level of data or unified cache.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_DATA_OPERATION_CAUSE_CACHE_REFILL 0x3

/**
 * L1 D-Cache access:
 *	Data read or write operation that causes a cache access at the lowest level of data or unified cache
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_DATA_OPERATION_CAUSE_CACHE_ACCESS 0x4

/**
 * D-TLB miss:
 *	Data read or write operation that causes a TLB refill at the lowest level of TLB.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_DATA_OPERATION_CAUSE_TLB_REFILL 0x5

/**
 * Data read executed:
 *	Data read architecturally executed.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_DATA_READ 0x6

/**
 * Data write executed:
 *	Data write architecturally executed.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_DATA_WRITE 0x7

/**
 * Instruction executed:
 *	Instruction architecturally executed.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_INST_EXECUTED   0x8

/**
 * Exception taken:
 *	This counts for each exception taken.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_EXCEPTION_TAKEN 0x9

/**
 * Exception return:
 *	Exception return architecturally executed.
 *	This includes:
 *	RFE <addressing_mode> <Rn>{!}
 *	MOVS PC (and other similar data processing instructions.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_EXCEPTION_RETURN_EXECUTED 0xa

/**
 * Writes context ID register:
 *	Instruction that writes to the Context ID Register architecturally executed.
 *  This counter only increments for instructions that are unconditional or that pass their condition codes.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_INST_WRITE_CONTEXT_ID_REG_EXECUTED 0xb

/**
 * Software change of PC:
 *	Software change of PC, except by an exception, architecturally executed.
 *	This counter only increments for instructions that are unconditional or that pass their condition codes.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_SOFTWARE_CHANGE_OF_PC_EXECUTED 0xc

/**
 * Branch executed:
 *	Immediate branch architecturally executed, taken or not taken.
 *	This includes B{L}, BLX, CB{N}Z, HB{L}, and HBLP.
 *	This counter counts for all immediate branch instructions that are architecturally executed, including conditional instructions that fail their condition codes.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_IMMEDIATE_BRANCH_EXECUTED 0xd

/**
 * Procedure return executed:
 *	Procedure return, other than exception returns, architecturally executed. 
 *	This includes:
 *	BX R14
 *	MOV PC, LR
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_PROCEDURE_RETURN_EXECUTED 0xe

/**
 * Unaligned access:
 *	This counts each instruction that is an access to an unaligned address. 
 *	This counter only increments for instructions that are unconditional or that pass their condition codes.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
 */
#define PJ4_PMU_UNALIGNED_ACCESS_EXECUTED 0xf


/**
* Branch mispredicted or not predicted:
*	This counts for every pipeline flush because of a misprediction from the program flow prediction resources.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_BRANCH_MISPREDICTED_OR_NOT_PREDICTED 0x10

/**
* Cycle count:
*	Counts for every clock cycle.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_CYCLE_COUNT 0x11

/**
* Predictable branches:
*	Branches or other change in the program flow that could have been predicted by the branch prediction resources of the processor.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_BRANCH_COULD_HAVE_BEEN_PREDICTED 0x12

/**
* L1 D-Cache read hit:
*	Counts the number of Data Cache read hits. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_READ_HIT 0x40

/**
* L1 D-Cache read miss:
*	Counts the number of Data Cache read misses. This includes data cache accesses that are non-cacheable. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_READ_MISS 0x41

/**
* L1 D-Cache write hit:
*	Counts the number of Data Cache write hits. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_WRITE_HIT 0x42

/**
* L1 D-Cache write miss:
*	Counts the number Data Cache write misses. This includes data cache accesses that are non-cacheable and/or non-bufferable. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_WRITE_MISS 0x43

/**
* MMU bus request:
*	Counts the number of cycles of requests to the MMU bus. This request can come from multiple masters. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_MMU_BUS_REQUEST 0x44

/**
* L1 I-Cache bus request:
*	Counts the number of cycles the Instruction Cache requests the bus until the data returns. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_ICACHE_BUS_REQUEST 0x45

/**
* Write buffer write latency:
*	Counts the number of cycles the Write Buffer requests the bus. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WB_WRITE_LATENCY 0x46

/** Hold LDM/STM:
*	Counts the number of cycles the pipeline is held because of a load/store multiple instruction. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_HOLD_LDM_STM 0x47

/**
* No dual cflag:
*	Counts the number of cycles the processor cannot dual issue because of a Carry flag dependency. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_NO_DUAL_CFLAG 0x48

/**
* No dual register plus:
*	Counts the number of cycles the processor cannot dual issue because the register file does not have enough read ports and at least one other reason. Counts cycles
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_NO_DUAL_REGISTER_PLUS 0x49

/**
* Load/store ROB0 on hold:
*	Counts the number of cycles a load or store instruction waits to retire from ROB0. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_LDST_ROB0_ON_HOLD 0x4a

/**
* Load/store ROB1 on hold:
*	Counts the number of cycles a load or store instruction waits to retire from ROB0=1. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_LDST_ROB1_ON_HOLD 0x4b

/**
* Data write access:
*	Counts the number of any Data write access. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DATA_WRITE_ACCESS_COUNT 0x4c

/**
* Data read access:
*	Counts the number of any Data read access. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DATA_READ_ACCESS_COUNT 0x4d

/**
* A2 stage stall:
*	Counts the number of cycles ALU A2 is stalled. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_A2_STALL 0x4e

/**
* L2-Cache write hit:
*	Counts the number of write accesses to addresses already in the L2C. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L2_CACHE_WRITE_HIT 0x4f

/**
* L2-Cache write miss:
*	Counts the number of write accesses to addresses not in the L2 Cache. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L2_CACHE_WRITE_MISS 0x50

/**
* L2-Cache read count:
*	Counts the number of 64-bit read transactions (read ready) sent from the bus. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L2_CACHE_READ_COUNT 0x51

/**
* L1 I-Cache read miss:
*	Counts the number of Instruction Cache read misses. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_ICACHE_READ_MISS 0x60

/**
* I-TLB main MMU miss:
*	Counts the number of instruction TLB misses. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_ITLB_MISS 0x61

/**
* Single issue:
*	Counts the number of cycles the processor single issues. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_SINGLE_ISSUE 0x62

/**
* Branch retired:
*	Counts the number of times one branch retires. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_BRANCH_RETIRED 0x63

/**
* ROB full:
*	Counts the number of cycles the Re-order Buffer (ROB) is full. Counts cycle.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_ROB_FULL 0x64

/**
* MMU read beat:
*	Counts the number of times the bus returns RDY to the MMU. 
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_MMU_READ_BEAT 0x65

/**
* Write buffer write beat:
*	Counts the number of times the bus returns ready to the Write Buffer. Counts events. 
*	This is useful to determine the average write latency of the write buffer (WB write latency/WB write count).
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WB_WRITE_BEAT 0x66

/**
* Dual issue:
*	Counts the number of cycles the processor dual issues. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DUAL_ISSUE 0x67

/**
* No dual raw:
*	Counts the number of cycles the processor cannot dual issue because of a Read after Write hazard. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_NO_DUAL_RAW 0x68

/**
* Hold issue stage:
*	Counts the number of cycles the issue is held. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_HOLD_IS 0x69

/**
* L2-Cache latency:
*	Counts the total number of cycles that L2C line fill requests to the bus. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L2_CACHE_LATENCY 0x6a

/**
* L1 D-Cache access, including non-cacheable requests:
*	Counts the number of times the Data cache is accessed. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_ACCESS 0x70

/**
* D-TLB main MMU miss:
*	Counts the number of data TLB misses. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DTLB_MISS 0x71

/**
* Branch mispredicted:
*	Counts the number of mispredicted branches. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_BRANCH_PREDICTION_MISS 0x72

/**
* A1 stage stall:
*	Counts the number of cycles ALU A1 is stalled. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_A1_STALL 0x74

/**
* L1 D-Cache read latency:
*	Counts the number of cycles the Data cache requests the bus for a read. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_READ_LATENCY 0x75

/**
* L1 D-Cache write latency:
*	Counts the number of cycles the Data cache requests the bus for a write. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_WRITE_LATENCY 0x76

/**
* No dual register file:
*	Counts the number of cycles the processor cannot dual issue because the register file doesn't have enough read ports. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_NO_DUAL_REGISTER_FILE 0x77

/**
* BIU simultaneous access
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_BIU_SIMULTANEOUS_ACCESS 0x78

/**
* L2-Cache read hit:
*	Counts the number of L2 Cache cache-to-bus external read requests. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L2_CACHE_READ_HIT 0x79

/**
* L2-Cache read miss:
*	Counts the number of L2 Cache read accesses that resulted in an external read request. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L2_CACHE_READ_MISS 0x7a


/**
* L2-Cache eviction:
*	Counts the number of evictions (Castouts) of a line from the L2 cache. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L2_CACHE_EVICTION 0x7b

/**
* TLB miss:
*	Counts the number of instruction and data TLB misses. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_TLB_MISS 0x80

/**
* Branches taken:
*	Counts the number of taken branches. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_BRANCH_TAKEN 0x81

/**
* Write buffer full
*	Counts the number of cycles WB is full. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WB_FULL 0x82

/**
* L1 D-Cache read beat:
*	Counts the number of times the bus returns ready to the Data cache during read requests. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_READ_BEAT 0x83

/**
* L1 D-Cache write beat:
*	Counts the number of times the bus returns ready to the Data cache during write requests. Counts events.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_DCACHE_WRITE_BEAT 0x84

/**
* No dual hardward:
*	Counts the number of cycles the processor cannot dual issue because of hardware conflict. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_NO_DUAL_HW 0x85

/**
* No dual multiple:
*	Counts the number of cycles the processor cannot dual issue because of multiple reasons. Counts cycles.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_NO_DUAL_MULTIPLE 0x86

/**
* BIU any access:
*	Counts the number of cycles the BIU is accessed by any unit.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_BIU_ANY_ACCESS 0x87

/**
* Main TLB miss caused by I-Cache:
*	Counts the number of Instruction fetch operations that causes a Main TLB walk.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_MAIN_TLB_REFILL_BY_ICACHE 0x88

/**
* Main TLB miss caused by D-Cache:
*	Counts the number of Data read or write operations that causes a Main TLB walk.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_MAIN_TLB_REFILL_BY_DCACHE 0x89

/**
* L1 I-Cache read beat:
*	Counts the number of times the bus returns RDY to the instruction cache, 
*	useful to determine the cache' average read latency (also known as "read miss" or "read count").
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_ICACHE_READ_BEAT 0x8a

/**
* Predicted branch count:
*	Counts the number of times a branch is predicted successfully.
* Supported processors: ARMADA 610, ARMADA 510.
* Not suppoted processors: PXA 955.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_PREDICTED_BRANCH_COUNT 0x8b

/* Counts any event from external input source PMUEXTIN[0] */
//#define PJ4_PMU_COUNT_ANY_EVENT_FROM_EXTERNAL_INPUT_SOURCE_PMUEXTIN0 0x90

/* Counts any event from external input source PMUEXTIN[1] */
//#define PJ4_PMU_COUNT_ANY_EVENT_FROM_EXTERNAL_INPUT_SOURCE_PMUEXTIN1 0x91


/* Counts any event from both external input source PMUEXTIN[0] and PMUEXTIN[1]*/
//#define PJ4_PMU_COUNT_ANY_EVENT_FROM_EXTERNAL_INPUT_SOURCE_PMUEXTIN0_PMUEXTIN1 0x92


/**
* WMMX2 store FIFO full:
*	Counts the number of cycles when the WMMX2 store FIFO is full.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WMMX2_STORE_FIFO_FULL 0xc0

/**
* WMMX2 finish FIFO full:
*	Counts the number of cycles when the WMMX2 finish FIFO is full.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WMMX2_FINISH_FIFO_FULL 0xc1

/**
* WMMX2 instruction FIFO full:
*	Counts the number of cycles when the WMMX2 instruction FIFO is full.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WMMX2_INST_FIFO_FULL 0xc2

/**
* WMMX2 instruction retired:
*	Counts the number of retired WMMX2 instructions.
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WMMX2_INST_RETIRED 0xc3

/**
* WMMX2 busy:
*	Counts the number of cycles when the WMMX2 is busy.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WMMX2_BUSY 0xc4

/**
* WMMX2 hold issue stage:
*	Counts the number of cycles when WMMX2 holds the issue stage.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WMMX2_HOLD_MI 0xc5

/**
* WMMX2 hold write back stage:
*	Counts the number cycles when WMMX2 holds the write back stage.
* Supported profilers:  counter monitor.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_WMMX2_HOLD_MW 0xc6

/**
* L0 I-Cache line fill
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L0IC_LINE_FILL 0xf0

/**
* L0 I-Cache hit prefetch buffer
* Supported profilers:  counter monitor, hot spot, and call stack.
* Available counter ids: COUNTER_PJ4_PMU_PMN0 = 2, COUNTER_PJ4_PMU_PMN1 = 3, COUNTER_PJ4_PMU_PMN2 = 4, COUNTER_PJ4_PMU_PMN3 = 5.
*/
#define PJ4_PMU_L0IC_HIT_PREFETECH_BUFFER 0xf1

#endif /* __PX_EVENT_TYPES_PJ4_H__ */

