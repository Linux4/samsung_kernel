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

#ifndef __PX_EVENT_TYPES_PJ1_H__
#define __PX_EVENT_TYPES_PJ1_H__

//* Supported profilers:  counter monitor, hot spot, and call stack.

/**
 * OS Timer event
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_OS_TIMER = 0
 */
#define PJ1_OS_TIMER_DEFAULT_EVENT_TYPE 0xFFFF

/**
 * A1 stage stall:
 *	Counts the number of cycles ALU A1 is stalled.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_A1_STALL             0xe00000a

/**
 * BIU any access:
 *	Counts the number of cycles the BIU is accessed by any unit.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_BIU_ANY_ACCESS       0xe000010

/**
 * BIU simultaneous access:
 *	Counts the number of cycles the bus is requested by more than one master.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_BIU_SIMULTANEOUS_ACCESS	0xe000010

/**
 * Branch mispredicted:
 *	Counts the number of times branch prediction causes the wrong branch to be prefetched.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_BRANCH_PREDICT_MISS           0xe000008

/**
 * Branch retired:
 *	Counts the number of times one branch retires.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_BRANCH_RETIED                 0xe000008

/**
 * Branches taken:
 *	Counts the number of times branch prediction correctly prefetches the required branch.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_BRANCH_TAKEN                  0xe000008

/**
 * Cycle count:
 *	Counts the number of clock cycles. Every clock cycle increments the counter.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1, COUNTER_PJ1_PMU_PMN1 = 2, COUNTER_PJ1_PMU_PMN2 = 3, COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_CYCLE_COUNT                   0xe000001

/**
 * Data read access
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_DATA_READ_ACCESS_COUNT_COUNTER_1        0xe000010

/**
 * Data read access
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_DATA_READ_ACCESS_COUNT_COUNTER_0         0xe000013

/**
 * Data write access
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_DATA_WRITE_ACCESS_COUNT_COUNTER_0        0xe000012

/**
 * Data write access:
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_DATA_WRITE_ACCESS_COUNT_COUNTER_3        0xe000016

/**
 * L1 D-Cache access:
 *	Counts the number of data-cache accesses (read hits / misses and write hits / misses).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_DCACHE_ACCESS                 0xe000003

/**
 * L1 D-Cache read beat:
 *	Counts the number of times the bus returns data to the data cache during read requests.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_DCACHE_READ_BEAT              0xe00000b

/**
 * L1 D-Cache read hit:
 *	Counts the number of data-cache read hits.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_DCACHE_READ_HIT               0xe000002

/**
 * L1 D-Cache read latency:
 *	Counts the number of cycles the data cache requests the bus for a read.
  * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_DCACHE_READ_LATENCY           0xe00000b

/**
 * L1 D-Cache read miss:
 *	Counts the number of data-cache read misses (including non-cacheable and non-bufferable cache accesses).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_DCACHE_READ_MISS_ON_COUNTER_0   0xe000003

/**
 * L1 D-Cache read miss:
 *	Counts the number of data-cache read misses (including non-cacheable and non-bufferable cache accesses).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_DCACHE_READ_MISS_ON_COUNTER_1   0xe000003

/**
 * L1 D-Cache read miss:
 *	Counts the number of data-cache read misses (including non-cacheable and non-bufferable cache accesses).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_DCACHE_READ_MISS_ON_COUNTER_3   0xe000002

/**
 * L1 D-Cache write beat:
 *	Counts the number of times the bus returns ready to the data cache during write requests.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_DCACHE_WRITE_BEAT               0xe00000c

/**
 * L1 D-Cache write hit:
 *	Counts the number of data-cache write hits.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_DCACHE_WRITE_HIT                0xe000004

/**
 * L1 D-Cache write latency:
 *	Counts the number of cycles the data cache requests the bus for a write.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_DCACHE_WRITE_LATENCY            0xe00000c

/**
 * L1 D-Cache write miss:
 *	Counts the number of data-cache write misses (including non-cacheable and non-bufferable misses).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_DCACHE_WRITE_MISS_ON_COUNTER_0  0xe000005

/**
 * L1 D-Cache write miss:
 *	Counts the number of data-cache write misses (including non-cacheable and non-bufferable misses).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_DCACHE_WRITE_MISS_ON_COUNTER_1  0xe000004

/**
 * L1 D-Cache write miss:
 *	Counts the number of data-cache write misses (including non-cacheable and non-bufferable misses).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_DCACHE_WRITE_MISS_ON_COUNTER_3  0xe000003

/**
 * D-TLB miss:
 *	Counts the number of TLB misses for data entries.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_DTLB_MISS                       0xe000004

/**
 * Hold issue stage:
 *	Count the number of cycles the instruction issue is stalled.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_HOLD_IS                         0xe00000f

/**
 * Hold LDM/STM:
 *	Counts the number of cycles the pipeline is blocked because of multiple load/store operations.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_HOLD_LDM_STM                    0xe00000d

/**
 * L1 I-Cache bus request:
 *	Counts the number of cycles the Instruction cache requests the bus until the data returns.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_ICACHE_BUS_REQUEST              0xe00000b

/**
 * L1 I-Cache read beat:
 *	Counts the number of times the bus returns RDY to the instruction cache, 
 *	useful to determine the cache’s average read latency (also known as read miss or read count).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_ICACHE_READ_BEAT                0xe00000b

/**
 * L1 I-Cache read miss:
 *	Counts the number of instruction-cache read misses.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_ICACHE_READ_MISS                0xe000002

/**
 * I-TLB miss:
 *	Counts the number of instruction TLB misses.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_ITLB_MISS                       0xe000005

/**
 * MMU bus request:
 *	Counts the number of cycles to complete a request via the MMU bus. 
 *	This request can derive from multiple masters.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_MMU_BUS_REQUEST                 0xe00000a

/**
 * MMU read beat:
 *	Counts the number of times the bus returns RDY to the MMU, useful when determining bus efficiency. 
 *	A user can use the signal that the MMU is requesting the bus and how long it takes on average for the data to return. (mmu_bus_req / mmu_read_count)
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_MMU_READ_BEAT                   0xe00000a

/**
 * Predicted branch count:
 *	Counts the number of times a branch is predicted successfully.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_PREDICTED_BRANCH_COUNT          0xe000019

/**
 * Instruction executed:
 *	Counts every time an instruction is retired.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_RETIRED_INSTRUTION              0xe000006

/**
 * ROB full:
 *	Counts the number of cycles the ROB is full.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2.
 */
#define PJ1_PMU_EVENT_ROB_FULL                        0xe000009

/**
 * WMMX2 busy:
 *	Counts the number of SIMD cycles.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_SIMD_CYCLE_COUNT                0xe000018

/* c2*/
/**
 * WMMX2 instruction FIFO full:
 *	Counts the number of cycles the SIMD coprocessor instruction buffer is full.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_SIMD_INST_BUFFER_FULL           0xe000019

/**
 * WMMX2 hold issue stage:
 *	Counts the number of cycles the SIMD coprocessor holds in its Issue (IS) stage.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_SIMD_HOLD_IS                    0xe000018

/**
 * WMMX2 hold write back stage:
 *	Counts the number of cycles the SIMD coprocessor holds in its writeback stage.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_SIMD_HOLD_WRITEBACK_STAGE       0xe000018

/**
 * WMMX2 finish FIFO full:
 *	Counts the number of cycles the SIMD coprocessor retire FIFO is full.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_SIMD_RETIRE_FIFO_FULL           0xe000019

/**
 * WMMX2 store FIFO full:
 *	Counts the number of cycles the SIMD coprocessor store FIFO is full.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2.
 */
#define PJ1_PMU_EVENT_SIMD_STORE_FIFO_FULL            0xe000019

/**
 * WMMX2 instruction retired.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2.
 */
#define PJ1_PMU_EVENT_SIMD_RETIED_INST                0xe000018

/**
 * Single issue:
 *	Counts the number of cycles the processor single-issues.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2.
 */
#define PJ1_PMU_EVENT_SINGLE_ISSUE                    0xe000006

/**
 * TLB miss:
 *	Counts the number of instruction and data TLB misses.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4.
 */
#define PJ1_PMU_EVENT_TLB_MISS                        0xe000004

/**
 * Write buffer write latency:
 *	Counts the number of cycles the write-back buffer requests the bus until the data is written to the bus.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_WB_BUS_REQUEST                  0xe00000c

/**
 * Write buffer full:
 *	Counts the number of cycles WB is full.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN3 = 4
 */
#define PJ1_PMU_EVENT_WB_FULL                         0xe000009

/**
 * Write buffer write beat
 *	Counts the number times the bus returns RDY to the write buffer, 
 *	useful to determine the write buffer’s average write latency (WB Write Latency/WB Write Beat).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_WB_WRITE_BEAT_ON_COUNTER_1      0xe00000c

/**
 * Write buffer write beat:
 *	Counts the number times the bus returns RDY to the write buffer, 
 *	useful to determine the write buffer’s average write latency (WB Write Latency/WB Write Beat).
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3
 */
#define PJ1_PMU_EVENT_WB_WRITE_BEAT_ON_COUNTER_2      0xe000009

/**
 * L2-Cache write hit.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1, COUNTER_PJ1_PMU_PMN1 = 2.
 */
#define PJ1_PMU_EVENT_L2C_WRITE_HIT                   0xe00001d

/**
 * L2-Cache read hit:
 *	The number of read accesses served from the L2C.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3, COUNTER_PJ1_PMU_PMN3 = 4.
 */
#define PJ1_PMU_EVENT_L2C_READ_HIT                    0xe00001d

/**
 * L2-Cache write miss:
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1, COUNTER_PJ1_PMU_PMN1 =2.
 */
#define PJ1_PMU_EVENT_L2C_WRITE_MISS                  0xe00001e

/**
 * L2-Cache read miss:
 *	The number of L2C read accesses that resulted in an external read request.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN2 = 3, COUNTER_PJ1_PMU_PMN3 = 4.
 */
#define PJ1_PMU_EVENT_L2C_READ_MISS                   0xe00001e

/**
 * L2-Cache read count:
 *	The number of L2C cache-to-bus external read requests.
 * Supported profilers:  counter monitor, hot spot, and call stack.
 * Available counter ids: COUNTER_PJ1_PMU_PMN0 = 1
 */
#define PJ1_PMU_EVENT_L2C_READ_COUNT                  0xe00001f

/**
 * L2-Cache latency:
 *	The latency for the most recent L2C read from the external bus.
 * Supported profilers:  counter monitor.
 * Available counter ids: COUNTER_PJ1_PMU_PMN1 = 2
 */
#define PJ1_PMU_EVENT_L2C_LATENCY                     0xe00001f

#endif /* __PX_EVENT_TYPES_PJ1_PMU_H__ */
