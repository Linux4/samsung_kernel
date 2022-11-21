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
	\brief CellGuide CGsnap driver OS depended prototypes
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

	\addtogroup CGX_DRIVER_OS_API
	\@{

*/

#ifndef CGX_DRIVR_OS_H
#define CGX_DRIVR_OS_H

#include "CgReturnCodes.h"






/**
	Get driver allocated buffer and convert it for application

    \param[in]  pSource			Map pointer passed from application to the driver space
    \param[in]  aLength			Length allocated (bytes)

	\return Mapped pointer
	\retval NULL	on error

*/
void *CgxDriverGetInternalBuff(void *pSource, U32 aLength);



/**
	Allocate a kernel buffer

    \param[in]  pDriver			Pointer to driver global structure
    \param[in]  aLength			Length to allocate (bytes)
    \param[out] apBuffer		allocated buffer address
    \param[in]  aProcessID		process id

	\return System wide return code
*/
TCgReturnCode CgxDriverAllocInternalBuff(void *pDriver, U32 aLength, void **apBuffer, U32 aProcessID);




/**
	Free a kernel buffer

    \param[in]  pDriver			Pointer to drivers global data
    \param[in]  aProcessId		process id

	\return System wide return code
*/
TCgReturnCode CgxDriverFreeInternalBuff(void *pDriver, U32 aProcessId);




/**
	Copy Memory from User Space

    \param[in]  to				Target address (Kernel Space)
    \param[in]  fromuser		Source address (User Space)
    \param[in]  n      			Length to copy


	\return Number of bytes NOT copied (0=success)

*/
unsigned long CgxCopyFromUser(
	void *to,
	const void *fromuser,
	unsigned long n );






/**
	Create transfer end signaling object.
	the object will be used for blocking the application until transfer is finished.

    \param[in]  pDriver			Global driver structure pointer

	\return System wide return code
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverTransferEndCreate(
	void *pDriver);


/**
	Destroy transfer end signaling object.

    \param[in]  pDriver			Global driver structure pointer

	\return System wide return code
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverTransferEndDestroy(
	void *pDriver);


/**
	Signal transfer end object

    \param[in]  pDriver			Global driver structure pointer

	\return System wide return code
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverTransferEndSignal(
	void *pDriver);


/**
	Wait for transfer end object
	\param  pDriver			Global driver structure pointer
	\param  aTimeoutMS		timeout for the wait, in mili seconds.
							if the object is not singled after this time, the function will
							return with ECgTimeout error code.
	\return System wide return code
	\retval ECgOk if data ready is 0 (low active)
*/
TCgReturnCode CgxDriverTransferEndWait(
	void *pDriver,
	U32 aTimeoutMS);




/**
	Start data ready interrupt handle

    \param[in]  pDriver			Global driver structure pointer

	\return System wide return code
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverDataReadyInterruptHandlerStart(
	void *pDriver);



/**
	Start general interrupt handler

    \param[in]  pDriver			Global driver structure pointer

	\return System wide return code
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverGpsInterruptHandlerStart(
	void *pDriver);


/**
	Initialize the driver.

	This function is called as a result of GPSenseEngine specific CGX_IOCTL_INIT request,
	and is used to initialize the driver and hardware for the first time.

	\param	pDriver			Global driver structure pointer

	\return System wide return code
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverInit(
	void *pDriver);




#endif
/**
 \@}
*/
