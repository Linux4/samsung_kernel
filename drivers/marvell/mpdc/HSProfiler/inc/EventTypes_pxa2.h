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

#ifndef __PX_EVENT_TYPES_PXA2_H__
#define __PX_EVENT_TYPES_PXA2_H__

/**
  Events for Counter Monitor
*/
typedef enum _PXA2_Event_Type_e
{
	INVALIDATE			= 0xFFFFFFFFu,
	
	/**
	 * L1 I-Cache miss:
	 *	L1 Instruction cache miss requires fetch from external memory.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L1_INSTRUCTION_MISS	= 0x0u,
	
	/**
	 * L1 I-Cache miss:
	 *	L1 Instruction cache cannot deliver an instruction. This could indicate an instruction cache or TLB miss. 
	 *	This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L1_INSTRUCTION_NOT_DELIVER = 0x01,	
	
	/**
	 * Data dependency stall:
	 *	Stall due to a data dependency. This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_STALL_DATA_DEPENDENCY = 0x02,		
	
	/**
	 * I-TLB miss:
	 *	Instruction TLB miss.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_INSTRUCTION_TLB_MISS = 0x03,
	
	/**
	 * D-TLB miss:
	 *	Data TLB miss.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_DATA_TLB_MISS = 0x04,

	 /**
	 * B and BL instruction retired:
	 *	Branch instruction retired, branch may or may not have changed program flow. 
	 *	(Counts only B and BL instructions, in both ARM and Thumb mode)
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_BRANCH_RETIRED = 0x05,
	
	/**
	 * Branch mispredicted:
	 *	Branch mispredicted. (Counts only B and BL instructions, in both ARM and Thumb mode)
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_BRANCH_MISPREDICTED = 0x06,
	
	/**
	 * Instruction executed:
	 *	Instruction retired. This event steps in a count of the number of executed instructions.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_INSTRUCTION_RETIRED = 0x07,			

	/**
	 * L1 D-Cache buffer full stall (cycle):
	 *	L1 Data cache buffer full stall. This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L1_DATA_STALL = 0x08,

	/**
	 * L1 D-Cache buffer full stall (sequence):
	 *	L1 Data cache buffer full stall. This event occurs once for each contiguous sequence of this type of stall.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L1_DATA_STALL_C = 0x09,

    /**
	 * L1 D-Cache access:
	 *	L1 Data cache access, not including Cache Operations.
	 *	All data accesses are treated as cacheable accesses and are counted here even if the cache is not enabled.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L1_DATA_ACCESS = 0x0A,

    /**
	 * L1 D-Cache miss:
	 *	L1 Data cache miss, not including Cache Operations. 
	 *	All data accesses are treated as cacheable accesses and are counted as misses if the data cache is not enabled.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L1_DATA_MISS = 0x0B,

    /**
	 * L1 D-Cache write back:
	 *	L1 Data cache write-back. This event occurs once for each line (32 bytes) that is written back from the cache.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L1_DATA_WRITE_BACK = 0x0C,

    /**
	 * Software change of PC:
	 *	Software changed the PC ('b', 'bx', 'bl', 'blx', 'and', 'eor', 'sub', 'rsb', 'add', 'adc', 'sbc', 'rsc', 'orr', 'mov',	'bic', 'mvn', 'ldm Rn, {..., pc}', 'ldr pc, [...]', pop {..., pc} will be counted).
	 *	The count does not increment when an exception occurs and the PC changes to the exception address (e.g., IRQ, FIQ, SWI, ...).
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SOFTWARE_CHANGED_PC = 0x0D,

    /**
	 * Branch retired:
	 *	Branch instruction retired. Branch may or may not have changed program flow. (Count ALL branch instructions, indirect as well as direct)
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_BRANCH_RETIRED_ALL = 0x0E,

    /**
	 * Hold issue stage:
	 *	Instruction issue cycle of retired instruction. This event is a count of the number of core cycles each instruction requires to issue.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_INSTRUCTION_CYCLE_RETIRED = 0x0F,

    /**
	 * Coprocessor stalled pipeline:
	 *	Coprocessor stalled the instruction pipeline. This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_COPROCESSOR_STALL_PIPE_NEW = 0x1d,

	/**
	 * All changes to the PC
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_ALL_CHANGED_PC = 0x18,

	/**
	 * Pipeline flush:
	 *	Pipeline flush due to branch mispredict or exception.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_PIPE_FLUSH_BRANCH = 0x19,

    /**
	 * Instruction issue failed:
	 *	The core could not issue an instruction due to a backend stall. This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_BACKEND_STALL = 0x1A,				

    /**
	 * Multiplier in use:
	 *	This event will occur every cycle in which the multiplier is active.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MULTIPLIER = 0x1B,

	/**
	 * Multiplier stalled pipeline:
	 *	>Multiplier stalled the instruction pipeline due to a resource stall. This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MULTIPLIER_STALL_PIPE = 0x1C,

    /**
	 * D-Cache stalled pipeline:
	 *	Data Cache stalled the instruction pipeline. This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_DATA_CACHE_STALL_PIPE = 0x1E,

    /**
	 * Snoop request stalled pipeline:
	 *	Snoop request stalled the instruction pipeline. This event will occur every cycle in which the condition is present.
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SNOOP_REQUEST_STALL_PIPE = 0x1F,

    /**
	 * L2-Cache request:
	 *	Unified L2 cache request, not including cache operations. This event includes table walks, data and instruction requests.
	 * Supported processors: PXA 3xx, PXA 93x.
	 * Not supported processors: PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_REQUEST = 0x20,

	/**
	 * L2-Cache data request:
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_DATA_REQUEST = 0x21,

	/**
	 * L2-Cache instruction request
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_INSTRUCTION_REQUEST = 0x22,

	/**
	 * L2-Cache miss
	 * Supported processors: PXA 3xx, PXA 93x.
	 * Not supported processors: PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_MISS=0x23,

	/**
	 * L2-Cache data miss
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_DATA_MISS = 0x24,

	/**
	 * L2-Cache instruction fetch miss
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_INSTRUTION_FETCH_MISS = 0x25,

	/* Unified L2 cache data read miss. */
	/**
	 * L2-Cache read miss
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_DATA_READ_MISS = 0x26,

	/**
	 * L2-Cache data read request
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_DATA_READ_REQUEST = 0x27,

	/**
	 * L2-Cache write miss
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_DATA_WRITE_MISS = 0x28,

	/**
	 * L2-Cache data write request
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_DATA_WRITE_REQUEST = 0x29,

	/**
	 * L2-Cache line writeback
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_LINE_WRITE_BACK = 0x2A,

	/**
	 *L2-Cache snoop or snoop confirm access
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_SNOOP_OR_SNOOP_CONFIRM_ACCESS = 0x2B,

	/**
	 * L2-Cache snoop miss
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_SNOOP_MISS = 0x2C,

	/**
	 * L2-Cache active
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_ACTIVE = 0x2D,

	/**
	 * L2-Cache push access
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_PUSH_ACCESS = 0x2E,

	/**
	 * L2-Cache access
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950.
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_ACCESS = 0x2F,

	/**
	 * Address bus transaction
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_ADDRESS_BUS = 0x40,

	/**
	 * Self initiated address bus transaction
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SELF_INITIATED_ADDRRES = 0x41,

    /**
	 * Bus grant delay of self initiated address bus transaction
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_BUS_GRANT_DELAY_ADDRESS = 0x42,

	/**
	 * Bus clock
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_BUS_CLOCK = 0x43,				

	/**
	 * Bus initiated data bus transaction
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SELF_INITIATED_DATA = 0x44,

	/**
	 * Bus grant delay of self initiated data bus transaction:
	 *	Bus grant delay of self initiated data bus transaction.
	 *	This event will occur every bus cycle in which the condition is present. 
	 *	(Bus cycle count from request to grant)
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_BUS_GRANT_DELAY_DATA = 0x45,

	/**
	 * Self initiated data bus transaction
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SELF_INITIATED_DATA_COUNT = 0x47,

	/**
	 * Data bus transaction
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_DATA_BUS_TRANSACTION = 0x48,

	/**
	 * Retired bus transaction
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_RETIRED_BUS_TRANSACTION = 0x49,

	/**
	 * L2-Cache line allocation
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_LINE_ALLOCATION = 0x50,

	/**
	 * L2-Cache line update
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_CACHE_LINE_UPDATE = 0x51,

	/**
	 * L2-Cache recirculated operation
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_RECIRCULATED_OPERATION = 0x52,

	/* Unified L2 snoop request */
	/**
	 * L2-Cache snoop request
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_SNOOP_REQUEST = 0x53,

	/**
	 * L2-Cache snoop confirm
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_SNOOP_CONFIRM = 0x54,

	/* Unified L2 push request */
	/**
	 * L2-Cache push request
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_PUSH_REQUEST = 0x55,

	/**
	 * L2-Cache push update
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_PUSH_UPDATE = 0x56,

	/**
	 * L2-Cache push allocation
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_PUSH_ALLOCATION = 0x57,

	/**
	 * L2-Cache special operation
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_SPECIAL_OPERATION = 0x58,

	/**
	 * L2-Cache snoop hit on clean cache line
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_SNOOP_HIT_ON_CLEAN_CACHE_LINE = 0x59,

	/**
	 * L2-Cache snoop hit on dirty cache line
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_L2_SNOOP_HIT_ON_DIRTY_CACHE_LINE = 0x5A,

	/**
	 * Address transaction retry
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_ADDRESS_TRANSACTION_RETRY = 0x60,

	/**
	 * Snoop transaction retry
	 * Supported processors: PXA 93x.
	 * Not supported processors: PXA 3xx, PXA 940, PXA 950
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SNOOP_TRANSACTION_RETRY = 0x61,

	PXA2_PMU_EVENT_ASSP_0=0x80,
	PXA2_PMU_EVENT_ASSP_1,
	PXA2_PMU_EVENT_ASSP_2,
	PXA2_PMU_EVENT_ASSP_3,
	PXA2_PMU_EVENT_ASSP_4,
	PXA2_PMU_EVENT_ASSP_5,
	PXA2_PMU_EVENT_ASSP_6,
	PXA2_PMU_EVENT_ASSP_7,

	PXA2_PMU_PMU_EVNET_POWER_SAVING=0xFF,
	PXA2_PMU_MONAHANS_EVENT_MASK=0x10000000,

	/**
	 * New instruction fetch performing
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_INSTRUCTION_FETCH=PXA2_PMU_MONAHANS_EVENT_MASK,  
	
	/**
	 * New data fetch performing
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_DATA_FETCH,				
	
	/**
	 * Core read request count
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_READ,					
	
	/* LCD read request cout*/
	//PXA2_PMU_MONAHANS_EVENT_LCD_READ,					
	
	/* DMA read request count*/
	//PXA2_PMU_MONAHANS_EVENT_DMA_READ,					
	
	/* Camera interface read request cout*/
	//PXA2_PMU_MONAHANS_EVENT_CAMERA_READ,
	
	/* USB 2.0 read request count*/
	//PXA2_PMU_MONAHANS_EVENT_USB20_READ,
	
	/* 2D graphic read request count*/
	//PXA2_PMU_MONAHANS_EVENT_2D_READ,
	
	/* USB1.1 host read reqeust count*/
	//PXA2_PMU_MONAHANS_EVENT_USB11_READ,
	
	/**
	 * System Bus 1 utilization
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_UNITIZATION=PXA2_PMU_MONAHANS_EVENT_MASK|9,
	
	/**
	 * System Bus 2 utilization
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_UNITIZATION,
	
	/**
	 * Dynamic memory queue occupied
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_DMC_NOT_EMPTY=PXA2_PMU_MONAHANS_EVENT_MASK|14,			
	
	/**
	 * Dynamic memory queue occupied by more than 1 request:
	 *	Number of cycles when the dynamic memory controller queue has 2 or more requests.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_DMC_2,

	/**
	 * Dynamic memory queue occupied by more than 2 requests:
	 *	Number of cycles when the dynamic memory controller queue has 3 or more reuests.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_DMC_3,						

	/**
	 * Dynamic memory queue occupied by more than 3 requests:
	 *	Number of cycles when the dynamic memory controller queue is full.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_DMC_FULL,					

	/**
	 * Static memory queue occupied:
	 *	Number of cycles when the static memory controler queue is not empty.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_SMC_NOT_EMPTY,

	/**
	 * Static memory queue occupied by more than 1 request:
	 *	Number of cycles when the static memory controller queue has 2 or more requests.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_SMC_2,

	/**
	 * Static memory queue occupied by more than 2 requests:
	 *	Number of cycles when the static memory controller queue has 3 or more requests.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_SMC_3,

	/**
	 * Static memory queue occupied by more than 3 requests:
	 *	Number of cycles when the static memory controller queue is full.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_SMC_FULL,					

	/**
	 * Internal SRAM memory queue occupied:
	 *	Number of cycles when the internal memory controller queue is not empty.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SRAM_MEM_QUEUE_OCCUPIED_0 = PXA2_PMU_MONAHANS_EVENT_MASK|26,
	
	/**
	 * Internal SRAM memory queue occupied by more than 1 request:
	 *	Number of cycles when the internal memory controller queue has two or more requests.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SRAM_MEM_QUEUE_OCCUPIED_1,
	
	/**
	 * Internal SRAM memory queue occupied by more than 2 requests:
	 *	Number of cycles when the internal memory controller queue 3 or more requests.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SRAM_MEM_QUEUE_OCCUPIED_2,
	
	/**
	 * Internal SRAM memory queue occupied by more than 3 requests:
	 *	Number of cycles when the internal memory controller queue is full.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_SRAM_MEM_QUEUE_OCCUPIED_3,
	
	/**
	 * External memory controller bus is occupied
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_EXTERNAL_MEM_CONTROLLER_BUS_OCCUPIED,
	
	/**
	 * External data flash bus is occupied
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_EXTERNAL_DATA_FLUSH_BUS_OCCUPIED,

	/**
	 * Core write access count
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_WRITE=PXA2_PMU_MONAHANS_EVENT_MASK|36,
	
	/* DMA write request count*/
	// PXA2_PMU_MONAHANS_EVENT_DMA_WRITE,
	
	/* Camera interface write request cout*/
	// PXA2_PMU_MONAHANS_EVENT_CAMERA_WRITE,
	
	/* USB 2.0 write request count*/
	// PXA2_PMU_MONAHANS_EVENT_USB20_WRITE,
	
	/* 2D graphic write request count*/
	// PXA2_PMU_MONAHANS_EVENT_2D_WRITE,
	
	/* USB1.1 host write request count*/
	// PXA2_PMU_MONAHANS_EVENT_USB11_WRITE,

	/**
	 * System bus 1 bus request:
	 *	Length of time that at least one bus request is asserted on System Bus 1.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_REQUEST=PXA2_PMU_MONAHANS_EVENT_MASK|42,
	
	/**
	 * System bus 2 bus request:
	 *	Length of time that at least one bus request is asserted on System Bus 2.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_REQUEST,

	/**
	 * System bus 1 bus retries
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_RETRIES,
	
	/**
	 * System bus 2 bus retries
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_RETRIES,

	/**
	 * Temperature level 1:
	 *	Temperature level 1 (time the PXA930 processor has spent in temperature range 1). 
	 *	The PSR register shows the current temperature range of operation (TSS field)
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_TEMPERATURE_1,
	
	/**
	 * Temperature level 2:
	 *	Temperature level 2 (time the PXA930 processor has spent in temperature range 2). 
	 *	The PSR register shows the current temperature range of operation (TSS field)
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_TEMPERATURE_2,
	
	/**
	 * Temperature level 3:
	 *	Temperature level 3 (time the PXA930 processor has spent in temperature range 3).
	 *	The PSR register shows the current temperature range of operation (TSS field)
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_TEMPERATURE_3,

	/**
	 * Temperature level 4:
	 *	Temperature level 4 (time the PXA930 processor has spent in temperature range 4). 
	 *	The PSR register shows the current temperature range of operation (TSS field)
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_TEMPERATURE_4,

	/**
	 * Core read/write latency measurement 1:
	 *	Amount of time when core has more than 1 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_LATENCY_1,				

	/**
	 * Core read/write latency measurement 2:
	 *	Amount of time when core has more than 2 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_LATENCY_2,
	
	/**
	 * Core read/write latency measurement 3:
	 *	Amount of time when core has more than 3 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_LATENCY_3,
	
	/**
	 * Core read/write latency measurement 4:
	 *	Amount of time when core has more than 4 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_CORE_LATENCY_4,

	/**
	 * System bus 1 to internal memory read/write latency measurement:
	 *	Amount of time when System Bus 1 to internal memory has more than 1 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_IM_1,
	
	/**
	 * System bus 1 to internal memory read/write latency measurement 2:
	 *	Amount of time when System Bus 1 to internal memory has more than 2 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_IM_2,

	/**
	 * System bus 1 to internal memory read/write latency measurement 3:
	 *	Amount of time when System Bus 1 to internal memory has more than 3 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_IM_3,

	/**
	 * System bus 1 to internal memory read/write latency measurement 4:
	 *	Amount of time when System Bus 1 to internal memory has more than 4 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_IM_4,

	/**
	 * System bus 1 to dynamic/static memory read/write latency measurement:
	 *	Amount of time when System Bus 1 to dynamic/static memory has more than 1 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_MEM_1,
	
	/**
	 * System bus 1 to dynamic/static memory read/write latency measurement 2:
	 *	Amount of time when System Bus 1 to dynamic/static memory has more than 2 read/write requests outstanding
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_MEM_2,
	
	/**
	 * System bus 1 to dynamic/static memory read/write latency measurement 3:
	 *	Amount of time when System Bus 1 to dynamic/static memory has more than 3 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_MEM_3,

	/**
	 * System bus 1 to dynamic/static memory read/write latency measurement 4:
	 *	Amount of time when System Bus 1 to dynamic/static memory has more than 4 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX1_MEM_4,

	/**
	 * System bus 2 to dynamic/static memory read/write latency measurement 1:
	 *	Amount of time when System Bus 2 to dynamic/static memory has more than 1 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_IM_1,
	
	/**
	 * System bus 2 to dynamic/static memory read/write latency measurement 2:
	 *	Amount of time when System Bus 2 to dynamic/static memory has more than 2 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_IM_2,

	/**
	 * System bus 2 to dynamic/static memory read/write latency measurement 3:
	 *	Amount of time when System Bus 2 to dynamic/static memory has more than 3 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_IM_3,

	/**
	 * System bus 2 to dynamic/static memory read/write latency measurement 4:
	 *	Amount of time when System Bus 2 to dynamic/static memory has more than 4 read/write requests outstanding.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_IM_4,

	/**
	 * System bus 2 to internal memory read/write latency measurement:
	 *	Amount of time when System Bus 2 to internal memory has more than 1 read/write requests outstanding
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_MEM_1,
	
	/**
	 * System bus 2 to internal memory read/write latency measurement 2:
	 *	Amount of time when System Bus 2 to internal memory has more than 2 read/write requests outstanding
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_MEM_2,
	
	/**
	 * System bus 2 to internal memory read/write latency measurement 3:
	 *	Amount of time when System Bus 2 to internal memory has more than 3 read/write requests outstanding
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_MEM_3,

	/**
	 * System bus 2 to internal memory read/write latency measurement 4:
	 *	Amount of time when System Bus 2 to internal memory has more than 4 read/write requests outstanding
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_PX2_MEM_4,

	/**
	 * B2I Deblock FIFO full
	 * Supported processors: PXA 940, PXA 950.
	 * Not supported processors:  PXA 3xx, PXA 93x.
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_B2I_DEBLOCK_FIFO_FULL=PXA2_PMU_MONAHANS_EVENT_MASK|71,

	/**
	 * B2I Deblock FIFO empty
	 * Supported processors: PXA 940, PXA 950.
	 * Not supported processors:  PXA 3xx, PXA 93x.
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_B2I_DEBLOCK_FIFO_EMPTY,

	/**
	 * B2I Decode FIFO full
	 * Supported processors: PXA 940, PXA 950.
	 * Not supported processors:  PXA 3xx, PXA 93x.
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_B2I_DECODE_FIFO_FULL,

	/**
	 * B2I Decode FIFO empty
	 * Supported processors: PXA 940, PXA 950.
	 * Not supported processors:  PXA 3xx, PXA 93x.
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_MONAHANS_EVENT_B2I_DECODE_FIFO_EMPTY,
	
	/**
	 * Core Clock:
	 *	Increments once for every core clock.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_CCNT_CORE_CLOCK_TICK = 0xFF,
	
	/**
	 * Core clock tick (64 clock base):
	 *	Increments once for every 64 core clock.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor, hot spot, call stack.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_PMU_CCNT_CORE_CLOCK_TICK_64 = 0xFE,
	
	/**
	 * Timer:
	 *	OS Timer event.
	 * Supported processors: PXA 3xx, PXA 93x, PXA 940, PXA 950.
	 * Not supported processors: 
	 * Supported profilers: counter monitor.
	 * Available counter ids: COUNTER_PXA2_PMU_PMN0 = 2, COUNTER_PXA2_PMU_PMN1 = 3, COUNTER_PXA2_PMU_PMN2 = 4, COUNTER_PXA2_PMU_PMN3 = 5.
	 */
	PXA2_OS_TIMER_DEFAULT_EVENT_TYPE = 0xFFFF

} PXA2_Event_Type;

#endif /* __PX_EVENT_TYPES_PXA2_H__ */
