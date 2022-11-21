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
	\brief CellGuide global types
	\attention This file should not be modified.
		If you think something is wrong here, please contact CellGuide
*/
#ifndef _CG_TYPES_H
#define _CG_TYPES_H

#ifdef	__cplusplus
extern "C" {
#endif /*__cplusplus*/

/** Unsigned 8 bit data */
typedef unsigned char   U8;
/** Unsigned 16 bit data */
typedef unsigned short  U16;
/** Unsigned 32 bit data*/
typedef unsigned long    U32;
/** Signed 8 bit data */
typedef signed char    S8;
/** Signed 16 bit data */
typedef signed short   S16;
/** Signed 32 bit data */
typedef signed long    S32;
/** 8-bit text data */
typedef char     	T8;
/** 16-bit text data */
typedef U16     T16;

#define CG_MAX_S32		(0x7FFFFFFF)
#define CG_MAX_S16		(0x7FFF)

#if !(defined (__BORLANDC__)) && !(defined (_MSC_VER)) && !(defined(TCC7801))
/** Boolean, TRUE/FALSE */
	#define BOOL U8
#endif /* !(defined (__BORLANDC__) && defined (_MSC_VER)) */

#if !(defined (__BORLANDC__)) && !(defined (_MSC_VER)) && !(defined (APPROACH5C))
    /**  void */
    typedef void    VOID;
#endif /* !(defined (__BORLANDC__) && defined (_MSC_VER)) */


#ifdef __BORLANDC__
	typedef int  	BOOL;
#endif

#ifdef _MSC_VER
	typedef int BOOL;
#endif


#ifdef TCC7801
	typedef int BOOL;
#endif


#if defined (_MSC_VER)
	//#define U64_ZERO 0
	//	typedef unsigned __int64 U64_32; /* unsigned 64 bit data */  // TODO Noam - made problems in hardware files...
	#define U64_ZERO {0,0}
	typedef struct {
		U32 low;
		U32 high;
	} U64_32;
	typedef __int64 S64; /* signed 64 bit data */
	typedef __int64 SINT64; /* signed 64 bit data */
	typedef unsigned __int64 U64; /* signed 64 bit data */
#elif defined (__LINUX__)
	#define U64_ZERO {0,0}
	typedef struct {
		U32 low;
		U32 high;
	} U64_32;
	typedef signed long long int S64; /* signed 64 bit data */
	typedef signed long long int SINT64; /* signed 64 bit data */
	typedef unsigned long long int U64; /* signed 64 bit data */
#elif /*defined (__BORLANDC__) || */defined(EN_64BIT_INTEGER)
	#define U64_ZERO 0
	typedef unsigned long long U64; /* unsigned 64 bit data */
	typedef long long S64; /* signed 64 bit data */
	typedef long long SINT64; /* signed 64 bit data */
#else
	// All that are not WCE not Linux (ADS1_2 for instance)
	#define U64_ZERO {0,0}
/** a structure for 64-bit integer	*/
	typedef struct {
		U32 low;
		U32 high;
	} U64_32;
	typedef unsigned long long U64; /* unsigned 64 bit data */
/** a structure for 64-bit signed integer	*/
	typedef struct {
		S32 low;
		S32 high;
	} S64;
	typedef long long SINT64; /* signed 64 bit data */
#endif

/** Single-precision floating point */
typedef float	FLOAT;
/** Double-precision floating point */
typedef double	DOUBLE;

typedef S16 DATA;
typedef S32 LDATA;

#ifndef NULL
	#define NULL 0
#endif

#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#if (defined(__TARGET_ARCH_4T) ||defined (__CW32__) || defined (__GCC32__) || defined (__UCSO__)  || defined (__LINUX__) ||\
		defined (__SYMBIAN__) ||defined (__NUCLEUS__) || defined (__BORLANDC__) || defined (_MSC_VER) || defined(MP_211) || defined(APPROACH5C)) ||\
		defined (__ARMEL__ )
#ifndef LITTLE_ENDIAN
	#define LITTLE_ENDIAN
#endif
#else
	#define  BIG_ENDIAN
#endif /*  */

/** file types for ADM (Assistance Data Manager)*/
typedef enum
{
    ECgAgpsBinaryFile  = 0,
    ECgAgpsRawBinaryFile,
    ECgAgpsAsciiFile,
    ECgAgpsAlmanacSemFile,
    ECgAgpsAlmanacYumaFile
} TCgFileType;


/* This type will be added if it is not exists in our target compiler */
typedef enum {
    cgFALSE = 0 ,
    cgTRUE = 0x7FFF
} cgBool ;

typedef U16 USHORT;
typedef U8 UCHAR;
typedef U32 ULONG;

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// TEMPORARY CODE - NOAM
//@@
/*************************************************************************************************
 Type: TCgAgpsIonosphere
 Description: Ionosphere data
*************************************************************************************************/

//@@
/*************************************************************************************************
 Type: TCgAgpsPosition
 Description: Geographical position
*************************************************************************************************/
// TODO NG 22/6/2010 should be replaced with TCgCoordinatesLLA (from CgCoordinates.h)
typedef struct SCgAgpsPosition
{
	DOUBLE latitude;	/** geographical latitude 	*/
	DOUBLE longitude;	/** geographical longitude 	*/
	DOUBLE altitude;	/** geographical altitude 	*/
} TCgAgpsPosition;

#define ALTITUDE_NOT_VALID (0xFFFF)
typedef struct
{
	S16 altitude;	/** height (from database) [meters] */
	U16 err;		/** Height expected error [meters]	*/
}TCgHeightData;

#ifdef __GCC32__
	#define __STRUCT_PACKED__	__attribute__ ((packed))
#else
	#define __STRUCT_PACKED__
#endif


typedef enum {
	ECG_BYTE_ORDER_1234 = 0x0,
	ECG_BYTE_ORDER_2143 = 0x1,
	ECG_BYTE_ORDER_3412 = 0x2,
	ECG_BYTE_ORDER_4321 = 0x3
} TCgByteOrder;

typedef enum {
	ECG_SWAP_NONE,
	ECG_SWAP_BYTES,
	ECG_SWAP_WORDS,
	ECG_SWAP_BYTES_AND_WORDS
}TCgByteSwapType;

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#ifdef	__cplusplus
}
#endif /*__cplusplus*/

#endif /*_CG_AGPS_GENERAL_TYPES_H*/
