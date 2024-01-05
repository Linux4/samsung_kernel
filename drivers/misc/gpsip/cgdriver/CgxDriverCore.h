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
	\brief CellGuide CGX5900 driver core prototypes (non-OS depended)
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

*/

#ifndef CGX_DRIVR_CORE_H
#define CGX_DRIVR_CORE_H


#include "CgReturnCodes.h"
#include "CgxDriverApi.h"

#define CG_CG_CORE_REG_BASE_ADDRESS	(0)

/**
	Stop driver.
	The function is called as a result of GPSenseEngine CGX_IOCTL_STOP request.
	Stop the driver current operations, destroy all memory structures and release system resources.
	this function is the counterpart of CgxDriverConstruct.

	\note Generic function : Not required to be ported

    \param[in]  pDriver			Global driver structure pointer
	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)
*/
TCgReturnCode CgxDriverDestroy(
	void *pDriver);



/**
	Setup driver structure

    \param[in]  pDriver			Global driver structure pointer
    \param[in]  pState			Driver state structure pointer

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverConstruct(
	void *pDriver,
	TCgxDriverState *pState);






/**
	Execute request

    \param[in]  pDriver			Global driver structure pointer
    \param[in]  pState			Driver state structure pointer
    \param[in]  aRequest
    \param[in]  pControl
    \param[in]  pResults

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverExecute(
	void *pDriver,
	TCgxDriverState *pState,
	U32 aRequest,
	TCgDriverControl *pControl,
	TCgDriverStatus *pResults);

/**
	Execute request, specific to IP/Core

	\param[in]  pDriver			Global driver structure pointer
	\param[in]  pState			Driver state structure pointer
	\param[in]  aRequest
	\param[in]  pControl
	\param[in]  pResults

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverExecuteSpecific(
							   void *pDriver,
							   TCgxDriverState *pState,
							   U32 aRequest,
							   TCgDriverControl *pControl,
							   TCgDriverStatus *pResults);


/**
	Check if request canceled

    \param[in]  pState			Driver state structure pointer

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverIsReadCanceled(TCgxDriverState *pState);



/**
	Handle GPS interrupt

    \param[in]  pDriver			Global driver structure pointer
    \param[in]  pState			Driver state structure pointer

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverGpsInterruptHandler(
	void *pDriver,
	TCgxDriverState *pState);



/**
	Handle DMA interrupt

    \param[in]  pDriver			Global driver structure pointer
    \param[in]  pState			Driver state structure pointer

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if data ready is 0 (low active)

*/
TCgReturnCode CgxDriverDataReadyInterruptHandler(
	void *pDriver,
	TCgxDriverState *pState);

TCgReturnCode CgxDriverHardwareReset(unsigned long aResetLevel);

/**
	Move chip to power down mode

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if chip finished power-down

*/
TCgReturnCode CgxDriverPowerDown(void);

/**
	Move chip to power up mode

	\return System wide return code  (see CgReturnCodes.h)
	\retval ECgOk if chip finished power-down

*/
TCgReturnCode CgxDriverPowerUp(void);

/**
Cut power from the CellGuide device

\return System wide return code  (see CgReturnCodes.h)
\retval ECgOk if chip finished power-down

*/
TCgReturnCode CgxDriverPowerOn(void);

/**
Connect power to the CellGuide device

\return System wide return code  (see CgReturnCodes.h)
\retval ECgOk if chip finished power-down

*/
TCgReturnCode CgxDriverPowerOff(void);

/**
Move RF chip to power down mode

\return System wide return code  (see CgReturnCodes.h)
\retval ECgOk if RF chip finished power-down

*/
TCgReturnCode CgxDriverRFPowerDown(void);

/**
Move RF chip to power down mode

\return System wide return code  (see CgReturnCodes.h)
\retval ECgOk if RF chip finished power-up

*/
TCgReturnCode CgxDriverRFPowerUp(void);

/**
	Set output of CGsnap's GPO aGpioPin, to aVal

	\return System wide return code  (see CgReturnCodes.h)
*/
TCgReturnCode CgCGCoreGpioSet(U32 aGpioPin, U32 aVal);


#if 0
//gaole add
void CgCoreReleaseRes(void);
void CgGpsReset(void);
#endif



#ifndef MIN
	#define MIN(__a__, __b__) 					((__a__) < (__b__) ? (__a__) : (__b__))
#endif

//#define CGCORE_ACCESS_VIA_SPI
extern bool flag_power_up;

#ifdef CGCORE_ACCESS_VIA_SPI
extern  int gps_spi_write_bytes( unsigned int  len, unsigned int  addr,unsigned int data);
    #define CGCORE_WRITE_REG(addr,val)        \
            gps_spi_write_bytes(1,addr,val)


extern int gps_spi_read_bytes( unsigned int len,unsigned int addr,unsigned int *data);
    #define CGCORE_READ_REG(addr,val)        \
        gps_spi_read_bytes(1,addr,val)

#else
     #define CGCORE_WRITE_REG(addr,val)          CgxCpuWriteMemory((U32)CG_DRIVER_CGCORE_BASE_VA, (U32)addr, val);
     #define CGCORE_READ_REG(addr,val)          CgxCpuReadMemory((U32)CG_DRIVER_CGCORE_BASE_VA, (U32)addr, val);
#endif



#endif
