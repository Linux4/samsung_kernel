/******************************************************************************
 *                                LEGAL NOTICE                                *
 *                                                                            *
 *  USE OF THIS SOFTWARE (including any copy or compiled version thereof) AND *
 *  DOCUMENTATION IS SUBJECT TO THE SOFTWARE LICENSE AND RESTRICTIONS AND THE *
 *  WARRANTY DISLCAIMER SET FORTH IN LEGAL_NOTICE.TXT FILE. IF YOU DO NOT     *
 *  FULLY ACCEPT THE TERMS, YOU MAY NOT INSTALL OR OTHERWISE USE THE SOFTWARE *
 *  OR DOCUMENTATION.                                                         *
 *  NOTWITHSTANDING ANYTHING TO THE CONTRARY IN THIS NOTICE, INSTALLING OR    *
 *  OTHERISE USING THE SOFTWARE OR DOCUMENTATION INDICATES YOUR ACCEPTANCE OF *
 *  THE LICENSE TERMS AS STATED.                                              *
 *                                                                            *
 ******************************************************************************/
/* Version: 1.8.9\3686 */
/* Build  : 13 */
/* Date   : 12/08/2012 */
/**
	\file
	\brief Os depended services requires by CgCpu
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

*/

#ifndef CG_CPU_OS_H
#define CG_CPU_OS_H

#ifdef __cplusplus
    extern "C"
    {
#endif

// ===========================================================================

#include "CgReturnCodes.h"


// ===========================================================================



/**
    \ingroup cg_cpu_os
	Map physical address space to virtual one

    \param[out]		pObject		Pointer to virtual address for physical memory region
    \param[in]		aBaseAddr	Base address (physical)
    \param[in]		aSize		Number of bytes

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk Success
*/
TCgReturnCode CgCpuVirtualAllocate(volatile void **pObject, unsigned long aBaseAddr, unsigned long aSize);


/**
    \ingroup cg_cpu_os
	Un-map physical address space to virtual one

    \param			pObject		Pointer to virtual address for physical memory region

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk Success
*/
TCgReturnCode CgCpuVirtualFree(volatile void **pObject);

/**
    \ingroup cg_cpu_os
	Allocate physical memory

    \param[out]		pObject		Pointer to virtual address of allocated memory
    \param[in]		pAddress	Address of allocated memory (physical)
    \param[in]		aSize		Number of bytes

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk Success
*/
TCgReturnCode CgCpuPhysicalAllocate(void **pObject, U32 *pAddress, U32 aSize);

/**
    \ingroup cg_cpu_os
	Free physical memory

    \param[out]		pObject		Pointer to virtual address of allocated memory
    \param[out]		pAddress	physical address
    \param[in]		aSize		size in bytes

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk Success
*/
TCgReturnCode CgCpuPhysicalFree(void **pObject, U32 *pAddress, unsigned long aSize);


/**
    \ingroup cg_cpu_os
	Enter Kernel mode

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk Success
*/
TCgReturnCode CgCpuKernelModeEnter(void);

/**
    \ingroup cg_cpu_os
	Exit Kernel mode

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk Success
*/
TCgReturnCode CgCpuKernelModeExit(void);

/**
    \ingroup cg_cpu_os
	Cache sync

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk Success
*/
//TCgReturnCode strinsCgCpuCacheSync(void);
TCgReturnCode  CgCpuCacheSync(void);

/**
	Read value from host memory location
	Used to read CGsnap memory mapped registers and other memories, as required.

    \param[in]  aBaseAddr		Base address (Virtual Memory)
    \param[in]  aOffset			Offset from base address
    \param[out] apValue			Read value

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxCpuReadMemory(U32 aBaseAddr, U32 aOffset, U32 *apValue);

/**
	Write value to host memory location
	Used to write CGsnap memory mapped registers and other memories, as required.

    \param[in]  aBaseAddr		Base address (Virtual Memory)
    \param[in]  aOffset			Offset from base address
    \param[in]	aValue			Value to write

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxCpuWriteMemory(U32 aBaseAddr, U32 aOffset, U32 aValue);


/**
	Sleep for the specified time.

    \param[in]  milliSeconds	time in miliseconds

	\return System wide return code  (see CgReturnCodes.h)
*/
TCgReturnCode CgxCpuSleep(U32 milliSeconds);


/**
	Return the CPU tick counter (mostly in mSec)
	\note tick counter units are different for each CPU

	\return System wide return code  (see CgReturnCodes.h)
*/
TCgReturnCode CgxCpuTick(U32 * apTickMSec);


// ===========================================================================

#ifdef __cplusplus
    }
#endif

#endif
