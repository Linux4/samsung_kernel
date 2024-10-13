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
	\brief Driver platform depended API
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

*/

#ifndef _CGX_DRIVER_PLATFORM_H__
#define _CGX_DRIVER_PLATFORM_H__

#include "CgReturnCodes.h"
#include "CgxDriverApi.h"

/**
    Define the incoming data byte order
*/
extern const TCgByteOrder CGX_DRIVER_NATIVE_BYTE_ORDER;

/**
    Return the interrupt code for DMA interrupt
	\if OS_LINUX
		On Linux, request_irq uses this value to register the specified interrupt handler
	\endif
	\if OS_ANDROID
		On Android, request_irq uses this value to register the specified interrupt handler
	\endif
    \return interrupt code
	\retval 0	Don't use interrupt
*/
U32 CgxDriverDataReadyInterruptCode(void);



/**
	 0 means no IRQ code

    \return IRQ # of DMA
*/
U32 CgxDriverDataReadyIrqCode(void);



/**
    Return the interrupt code for CGsnap interrupt
	\if OS_LINUX
		On Linux, request_irq uses this value to register the specified interrupt handler
	\endif
	\if OS_ANDROID
		On Android, request_irq uses this value to register the specified interrupt handler
	\endif
    \return interrupt code
	\retval 0	Don't use interrupt
*/
U32 CgxDriverGpsInterruptCode(void);



/**
	 0 means no IRQ code

    \return IRQ # of GPS
*/
U32 CgxDriverGpsIrqCode(void);



/**
	Configure Data Ready (DMA) interrupt pin & directions
    this function is called once, on driver startup (\ref CgxDriverConstruct)

	\return System wide return code
	\retval ECgOk Success
*/
TCgReturnCode CgxDriverDmaInterruptPrepare(void);



/**
	Initial setup for CGsnap interrupt
    this function is called once, on driver startup (\ref CgxDriverConstruct)

	\return System wide return code
	\retval ECgOk Success
*/
TCgReturnCode CgxDriverGpsInterruptPrepare(void);




/**
    Return GPIO code for specific control line(for debug only)

    \param[in]		aPinNumber	GPIO code (CellGuide format)
	\param[out]		aGpioCode	Pointer to returned value

	\return System wide return code
	\retval ECgOk Success
*/
TCgReturnCode CgxDriverGpioCode(int aPinNumber, int *aGpioCode);


TCgReturnCode  CGxDriverGpsInterruptDone(U32 aIntCode);
TCgReturnCode  CGxDriverDataReadyInterruptDone(U32 aIntCode);



/**
    Enable/Disable TCXO power

    \param[in]		aEnable		1 = Enable TCXO, 0 =Disable TCXO

	\note If required, use platform.h to define specific device pins,
		  and keep the actual function generic.

	\return System wide return code
	\retval ECgOk Success
*/
TCgReturnCode CgxDriverTcxoEnable(U32 aEnable);

#endif /* _CGX_DRIVER_PLATFORM_H__ */
