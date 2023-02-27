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
    \cond OS_LINUX
*/


/**
	\file
	\brief Linux implementation of OS depended CellGuide CPU API
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide



*/
#include <stdarg.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>

#include <linux/sched.h>
#include <linux/time.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/cacheflush.h>

#include "CgCpuOs.h"
#include "CgCpu.h"
#include "platform.h"






TCgReturnCode CgCpuIntrDone(U32 aInterruptCode)
{
	TCgReturnCode rc = ECgOk;
	//InterruptDone(aInterruptCode);
	return rc;
}



TCgReturnCode CgCpuVirtualAllocate(volatile void **pObject, unsigned long aBaseAddr, unsigned long aSize)
{
	ASSERT_NOT_NULL(pObject);

    *pObject = ioremap_nocache(aBaseAddr, aSize);

    return (*pObject != NULL) ? ECgOk : ECgErrMemory;
}


TCgReturnCode CgCpuVirtualFree(volatile void **pObject)
{
	ASSERT_NOT_NULL(pObject);

    iounmap((void *)*pObject);
    *pObject = NULL;

    return ECgOk;
}


TCgReturnCode CgCpuPhysicalAllocate(void **pObject, U32 *pAddress, U32 aSize)
{
	DBG_FUNC_NAME("CgCpuPhysicalAllocate")
	TCgReturnCode rc = ECgOk;
	ASSERT_NOT_NULL(pObject);
	//DBGMSG1("Trying to allocate %d bytes", aSize);

	*pObject = dma_alloc_writecombine(0, aSize + 32, (dma_addr_t *)pAddress, GFP_KERNEL);

	if(*pObject == NULL) {
		DBGMSG("Error! Failed allocating buffer");
		rc = ECgErrMemory;
		}
	else {
		DBGMSG2("buffer allocated: ptr=0x%08X, addr=0x%08X", *pObject, *pAddress);
//		memset((void *)*pObject, 0, aSize);
		}

	*pObject = (unsigned char *)((((unsigned long)*pObject + 32 - 1) / 32) * 32);
	*pAddress = ((*pAddress + 32 - 1) / 32) * 32;

	return rc;
}

TCgReturnCode CgCpuPhysicalFree(void **pObject, U32 *pAddress, unsigned long aSize)
{
	DBG_FUNC_NAME("CgCpuPhysicalFree")
	TCgReturnCode rc = ECgOk;
	ASSERT_NOT_NULL(pObject);
	DBGMSG1("pObject=0x%08X", *pObject);

	if (*pAddress != 0)
		dma_free_writecombine(0, aSize + 32, *pObject, *pAddress);

	*pObject = NULL;

	return rc;
}




TCgReturnCode CgxCpuReadMemory(U32 aBaseAddr, U32 aOffset, U32 *apValue)
{
    	DBG_FUNC_NAME("CgxCpuReadMemory")

         DBGMSG1("read addd ress %x",aBaseAddr);


    *apValue = sci_glb_read((aBaseAddr + aOffset),-1UL);
    return ECgOk;
}

TCgReturnCode CgxCpuWriteMemory(U32 aBaseAddr, U32 aOffset, U32 aValue)
{
    DBG_FUNC_NAME("CgxCpuWriteMemory")

    DBGMSG1("addd ress %x",aBaseAddr);

	sci_glb_write((aBaseAddr + aOffset), aValue, -1UL);

    return ECgOk;
}

TCgReturnCode CgCpuKernelModeEnter(void)
{
	return ECgOk;
}


TCgReturnCode CgCpuKernelModeExit(void)
{
	return ECgOk;
}


TCgReturnCode CgCpuCacheSync(void)
{
	//DBG_FUNC_NAME("CgCpuCacheSync")
#ifndef ACP_PORT
	flush_cache_all();
#else
    DBGMSG("enable ACP");
#endif

	return ECgOk;
}



#ifdef TRACE_ON
#define MAX_LINE_LENGTH  256


TCgReturnCode CgxCpuPrint(char *aText)
{
	printk(KERN_EMERG "%s", aText);

	return ECgOk;
}

TCgReturnCode CgxCpuMsg(const char *aFuncName, const char *aFileName, U32 aLineNum, char *aFormat, ...)
{
#if 0
  	va_list args;
    char buf[MAX_LINE_LENGTH]= {0};
	int n = 0;
	U32 curTickMSec = 0;
   	va_start(args, aFormat); // prepare the arguments
	n = vsnprintf(buf, MAX_LINE_LENGTH, aFormat, args);
   	va_end(args); // clean the stack

	// Get current tick
	CgxCpuTick(&curTickMSec);

	if (n > 0)
		printk(KERN_EMERG ">>> %d.%03d %s[%d]: %s\n", curTickMSec / 1000, curTickMSec % 1000, aFuncName, (int)aLineNum, buf);
	#endif
	return ECgOk;
}
#endif

TCgReturnCode CgxCpuSleep(U32 milliSeconds)
{
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout (milliSeconds);
	return ECgOk;
}
#ifndef CG_HAS_CPU_TICK
	TCgReturnCode CgxCpuTick(U32 * apTickMSec)
	{
		struct timespec t = current_kernel_time();
		*apTickMSec = t.tv_sec * 1000 + t.tv_nsec / 1000000;
		return ECgOk;
	}
#endif

/** \endcond */
