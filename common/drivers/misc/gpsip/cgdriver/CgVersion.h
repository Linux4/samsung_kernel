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
	\file CgVersion.h
	\brief
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide

	Defines
*/
#ifndef CG_VERSION_H
#define CG_VERSION_H

#ifdef	__cplusplus
extern "C" {
#endif /*__cplusplus*/

#include "CgTypes.h"						/**< CellGuide Types */

#ifdef CGTEST
	#define CG_BUILD_MODE	"TEST"
#elif defined(DEBUG) || defined (_DEBUG)
	#define CG_BUILD_MODE	"DEBUG"
#else
	#define CG_BUILD_MODE	"RELEASE"
#endif


/**
 * VERSION type to pass VERSION types of information using one union
*/
typedef struct SCgVersion{
	const char *buildVersion;
	const char *buildOriginator;
	const char *buildName;
	const char *buildNumber;
	const char *buildDate;
	const char *buildTime;
	const char *buildMode;
} TCgVersion;



const char *CgVersionBuildNumber(const TCgVersion *apVersion);
const char *CgVersionBuildDate(const TCgVersion *apVersion);
const char *CgVersionBuildTime(const TCgVersion *apVersion);




/*@}*/

#ifdef	__cplusplus
}
#endif /*__cplusplus*/


#endif
