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

#ifndef CGX_DRIVR_CORE_COMMON_H
#define CGX_DRIVR_CORE_COMMON_H


#include "CgReturnCodes.h"
#include "CgxDriverApi.h"

// Access to the APB
#define CGCORE_REG_OFFSET_SAT_RAM		(0x100)
#define CGCORE_REG_OFFSET_SAT_RAM_LAST	(0x1FC)
#define CGCORE_REG_OFFSET_APB_COMMAND	(0x090)
#define CGCORE_REG_OFFSET_APB_DATA		(0x094)

// CG Core interrupt codes
#define CGCORE_INT_RTC					(0x00000001)	// RTC timer has reached its threshold Interrupt
#define CGCORE_INT_SNAP_START			(0x00000002)	// StartSnap occurred Interrupt
#define CGCORE_INT_SNAP_END				(0x00000004)	// Snap completed successfully Interrupt
#define CGCORE_INT_TCXO_EXPIRED			(0x00000008)	// TCXO timer has reached its threshold Interrupt
#define CGCORE_INT_OVERRUN				(0x00000010)	// A buffer overrun occurred interrupt. Hence the last snap is invalid.
#define CGCORE_INT_EOT					(0x00000020)	// End of BS tracking period Interrupt
#define CGCORE_INT_EOM					(0x00000040)	// End of drift meas measurement period Interrupt

#define CGCORE_REG_INT_RAW				(0x80)
#define CGCORE_REG_INT_SRC				(0x84)
#define CGCORE_REG_INT_EN				(0x88)
#define CGCORE_REG_INT_CLR				(0x8C)


/**
    Define the incoming data byte order 
*/
extern const TCgByteOrder CGX_DRIVER_NATIVE_BYTE_ORDER;

BOOL isBlockCanceled(TCgxDriverState * apState, U32 aBlockNumber);

#endif
