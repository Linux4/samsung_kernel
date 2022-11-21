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
	\brief Cell-Guide common return codes
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

	This file define the infrastructure and common error codes for cell-guide projects
 */
#ifndef CG_RETURN_CODES_H
#define CG_RETURN_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CgTypes.h"						/**< CellGuide Types */

#define CG_RETURN_CODE_SUB_RANGE         (0x400)
#define RANGE_MASK ~(CG_RETURN_CODE_SUB_RANGE-1)

#define ECgCommonErrorBase               (0)
#define ECgPrivateErrorBase              (-1 * CG_RETURN_CODE_SUB_RANGE)
#define ECgPrivateMsgBase                (+1 * CG_RETURN_CODE_SUB_RANGE)


/* System-wide positives */

/** OK status */
#define ECgOk                            (0)
#define ECgProgressNotification          ( ECgCommonErrorBase + 1 )

/* System-wide errors */
#define ECgBadArgument                   	( ECgCommonErrorBase - 1 )
#define ECgNotSupported                  	( ECgCommonErrorBase - 2 )
#define ECgGeneralFailure                	( ECgCommonErrorBase - 3 )
#define ECgInitFailure                   	( ECgCommonErrorBase - 4 )
#define ECgErrMemory                     	( ECgCommonErrorBase - 5 )
#define ECgErrGeneral                    	( ECgCommonErrorBase - 6 )
#define ECgErrNotInitialized             	( ECgCommonErrorBase - 7 )
#define ECgOutOfBounds                   	( ECgCommonErrorBase - 8 )
#define ECgDataNotFound                  	( ECgCommonErrorBase - 9 )
#define ECgPermissionDenied              	( ECgCommonErrorBase - 10 )
#define ECgNullPointer                   	( ECgCommonErrorBase - 11 )
#define ECgFileOpen                      	( ECgCommonErrorBase - 12 )
#define ECgFileRead                      	( ECgCommonErrorBase - 13 )
#define ECgFileChecksum                  	( ECgCommonErrorBase - 14 )
#define ECgTimeout                       	( ECgCommonErrorBase - 15 )
#define ECgTimeNotValid                  	( ECgCommonErrorBase - 16 )
#define ECgEndOfFile                     	( ECgCommonErrorBase - 17 )
#define ECgInvalidCharacter              	( ECgCommonErrorBase - 18 )
#define ECgFatalError                    	( ECgCommonErrorBase - 19 )
#define ECgBadConfiguration              	( ECgCommonErrorBase - 20 )
#define ECgReadOnlyProperty              	( ECgCommonErrorBase - 21 )
#define ECgExitModeFlag      	         	( ECgCommonErrorBase - 22 )
#define ECgFatalInternalError  	         	( ECgCommonErrorBase - 23 )


#define ECgPropertiesMissingLevel        	( ECgCommonErrorBase - 24 )
#define ECgPropertiesMissingGetMethod    	( ECgCommonErrorBase - 25 )

#define ECgNotAllowed				     	( ECgCommonErrorBase - 26 )

#define ECgDataNotValid 				 	( ECgCommonErrorBase - 27 )
#define ECgDataNotEqual 				 	( ECgCommonErrorBase - 28 )

#define	ECgDataNotAvailable		         	( ECgCommonErrorBase - 30 )
#define	ECgFileNotFound			         	( ECgCommonErrorBase - 31 )
#define	ECgPathNotFound			         	( ECgCommonErrorBase - 32 )
#define ECgNoMoreFiles				     	( ECgCommonErrorBase - 33 )
#define ECgNoMoreSlots					 	( ECgCommonErrorBase - 34 )
#define ECgInvalidData					 	( ECgCommonErrorBase - 35 )
#define ECgPathExists                    	( ECgCommonErrorBase - 36 )
#define ECgNotEnoughData                 	( ECgCommonErrorBase - 37 )
#define ECgBusy                          	( ECgCommonErrorBase - 38 )
#define ECgAnyErrCode                    	( ECgCommonErrorBase - 39 )

#define ECgNetworkNotInitialized		 	( ECgCommonErrorBase - 40 )
#define ECgNetworkSubsysFailed			 	( ECgCommonErrorBase - 41 )
#define ECgNetworkPortInUse				 	( ECgCommonErrorBase - 42 )
#define ECgNetworkInvalidAddress		 	( ECgCommonErrorBase - 43 )
#define ECgNetworkPortAlreadyInUse		 	( ECgCommonErrorBase - 44 )
#define ECgNetworkTooManyConnections	 	( ECgCommonErrorBase - 45 )
#define ECgInvalidFileName				 	( ECgCommonErrorBase - 46 )
#define ECgDeviceOpenFail				 	( ECgCommonErrorBase - 47 )
#define ECgDataOverrun				     	( ECgCommonErrorBase - 48 )
#define ECgNetworkInvalidPort			 	( ECgCommonErrorBase - 49 )
#define ECgNetworkSocketNotConnected	 	( ECgCommonErrorBase - 50 )

#define ECgResume						 	( ECgCommonErrorBase - 61 )
#define ECgPowerOff							( ECgCommonErrorBase - 62 )

#define ECgGreater						 	( ECgCommonErrorBase - 100 )
#define ECgSmaller						 	( ECgCommonErrorBase - 101 )

#define ECgTerminate					 	( ECgCommonErrorBase - 102 )
#define ECgNotOwner						 	( ECgCommonErrorBase - 103 )
#define ECgCanceled						 	( ECgCommonErrorBase - 104 )

#define ECgIoPending					 	( ECgCommonErrorBase - 105 )
#define ECgSingularMatrix                	( ECgCommonErrorBase - 106 )

#define ECgIntegerOverFlow				 	( ECgCommonErrorBase - 107 )
#define EcgInSufficientWorkPadSize		 	( ECgCommonErrorBase - 108 )
#define ECgSatelliteOverflow             	( ECgCommonErrorBase - 109 )
#define ECgLastItem						 	( ECgCommonErrorBase - 110 )


typedef S32 TCgReturnCode;


//#if (CG_USE_MAX_LOG_LEVEL >= CGLOG_FATAL)
//#ifdef CG_LOG_H	// CG_LOG_H_USED indicates CgLog.h is included in the compilation
#ifndef CGXDRIVER_EXPORTS
	#ifdef CGTEST
		#define CG_STORE_ERROR_LINE
	#elif defined DEBUG
		#define CG_STORE_ERROR_LINE
	#elif defined _DEBUG
		#define CG_STORE_ERROR_LINE
	#endif
#endif

#ifdef CG_STORE_ERROR_LINE
	#undef CG_STORE_ERROR_LINE
#endif

//#endif
//#endif

BOOL cgRetCode_store_error_line(const char * aFileName, long lineNumber, TCgReturnCode rc);
BOOL cglog_wrn_file_and_line(const char * aFileName, long aLineNumber, long rc);

#ifdef CG_STORE_ERROR_LINE
	#define OK(rc) (rc >= ECgOk ? TRUE : cgRetCode_store_error_line(__FILE__,__LINE__,rc))
	#define FAIL(rc) (rc < ECgOk ? !cgRetCode_store_error_line(__FILE__,__LINE__,rc) : FALSE)
	//#define OK(rc) (rc >= ECgOk ? TRUE : cglog_wrn_file_and_line(__FILE__,__LINE__,rc))
	//#define FAIL(rc) (rc < ECgOk ? !cglog_wrn_file_and_line(__FILE__,__LINE__,rc) : FALSE)
#else
	#define OK(rc) (rc >= ECgOk)
	#define FAIL(rc) (rc < ECgOk)
#endif

#define SELECT_FIRST_ERROR2(rc1,rc2)			(rc1 == ECgOk) ? rc2 : rc1
#define SELECT_FIRST_ERROR3(rc1,rc2,rc3)		SELECT_FIRST_ERROR2((SELECT_FIRST_ERROR2(rc1,rc2)),rc3)
#define SELECT_FIRST_ERROR4(rc1,rc2,rc3,rc4)	SELECT_FIRST_ERROR2((SELECT_FIRST_ERROR3(rc1,rc2,rc3)),rc4)
#define SELECT_FIRST_ERROR5(rc1,rc2,rc3,rc4,rc5)SELECT_FIRST_ERROR2((SELECT_FIRST_ERROR4(rc1,rc2,rc3,rc4)),rc5)

// 0 indicates no error. index starts at 1.
#define INDEX_FIRST_ERROR2(rc1,rc2)			((rc1 == ECgOk) ? ((rc2 == ECgOk) ? 0 : 2) : 1)
#define INDEX_FIRST_ERROR3(rc1,rc2,rc3)		((rc3 == ECgOk) ? INDEX_FIRST_ERROR2(rc1,rc2) : 3)

#ifdef __cplusplus
}
#endif

#endif // CG_RETURN_CODES_H
